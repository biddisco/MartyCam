#include <iostream>
//
#include "debug/logging.hpp"
#include "martycam/core/stream_buffer_recorder.hpp"
//
#include <hpx/async_local/async_fwd.hpp>
#include <hpx/executors/parallel_executor.hpp>
#include <hpx/include/async.hpp>
#include <hpx/include/parallel_executors.hpp>

//----------------------------------------------------------------------------
stream_buffer_recorder::stream_buffer_recorder(std::string const& stream_url,
    double buffer_duration_seconds, hpx::execution::parallel_executor exec)
  : url(stream_url)
  , max_buffer_duration(buffer_duration_seconds)
{
  this->executor = exec;
}

//----------------------------------------------------------------------------
stream_buffer_recorder::~stream_buffer_recorder() { close(); }

//----------------------------------------------------------------------------
int stream_buffer_recorder::ffmpegInterruptCallback(void* opaque)
{
  auto* self = static_cast<stream_buffer_recorder*>(opaque);
  if (!self) return 0;
  // Return non-zero to interrupt blocking FFmpeg I/O calls during shutdown.
  return self->interrupt_requested.load() ? 1 : 0;
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::open(int width, int height, std::string const& fourcc_str)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  this->interrupt_requested = false;
  is_url = (url.find("://") != std::string::npos);

  if (is_url)
  {
    if (!open_ip()) return false;
  }
  else
  {
    // Pass the target specifications down to the USB configurator
    if (!open_usb(width, height, fourcc_str)) return false;
  }

  // Common setup: Find the streams, bind the decoder context and spawn the thread
  for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++)
  {
    if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      video_stream_idx = i;
      break;
    }
  }
  if (video_stream_idx == -1) return false;

  AVCodec const* decoder =
      avcodec_find_decoder(ifmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
  decoder_ctx = avcodec_alloc_context3(decoder);
  avcodec_parameters_to_context(decoder_ctx, ifmt_ctx->streams[video_stream_idx]->codecpar);
  if (avcodec_open2(decoder_ctx, decoder, nullptr) < 0) return false;

  is_running = true;
  worker_future = hpx::async(this->executor, [this]() { captureLoop(); });
  return true;
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::open_ip()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  ifmt_ctx = avformat_alloc_context();
  if (!ifmt_ctx) return false;
  ifmt_ctx->interrupt_callback.callback = &stream_buffer_recorder::ffmpegInterruptCallback;
  ifmt_ctx->interrupt_callback.opaque = this;

  AVDictionary* options = nullptr;

  // Force TCP transport to prevent packet loss and blocky artifacts
  av_dict_set(&options, "rtsp_transport", "tcp", 0);

  // Keep your timeout setting
  av_dict_set(&options, "timeout", "2000000", 0);

  if (avformat_open_input(&ifmt_ctx, url.c_str(), nullptr, &options) < 0)
  {
    av_dict_free(&options);
    if (ifmt_ctx)
    {
      avformat_free_context(ifmt_ctx);
      ifmt_ctx = nullptr;
    }
    return false;
  }

  av_dict_free(&options);
  if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
  {
    avformat_close_input(&ifmt_ctx);
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::open_usb(int width, int height, std::string const& fourcc_str)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  avdevice_register_all();

  ifmt_ctx = avformat_alloc_context();
  if (!ifmt_ctx) return false;
  ifmt_ctx->interrupt_callback.callback = &stream_buffer_recorder::ffmpegInterruptCallback;
  ifmt_ctx->interrupt_callback.opaque = this;

  AVInputFormat const* ifmt = av_find_input_format("v4l2");
  if (!ifmt) return false;

  AVDictionary* options = nullptr;

  // 1. Map OpenCV/MartyCam FourCC strings to FFmpeg V4L2 names
  std::string ffmpeg_v4l2_format = "";
  if (!fourcc_str.empty())
  {
    if (fourcc_str == "GREY" || fourcc_str == "Y800" || fourcc_str == "Y8  ")
    {
      // Use an explicit grayscale pixel format for raw IR sensors.
      ffmpeg_v4l2_format = "gray";
    }
    else if (fourcc_str == "MJPEG" || fourcc_str == "MJPG") { ffmpeg_v4l2_format = "mjpeg"; }
    else if (fourcc_str == "YUYV" || fourcc_str == "YUY2") { ffmpeg_v4l2_format = "yuyv422"; }
  }

  // Default to MJPEG for USB cameras if no format specified — raw formats like YUYV
  // are not muxable into MP4 and produce empty recordings.
  if (ffmpeg_v4l2_format.empty()) { ffmpeg_v4l2_format = "mjpeg"; }
  av_dict_set(&options, "input_format", ffmpeg_v4l2_format.c_str(), 0);

  // 2. Set requested Resolution dynamically instead of hardcoded 1280x720
  if (width > 0 && height > 0)
  {
    std::string res_str = std::to_string(width) + "x" + std::to_string(height);
    av_dict_set(&options, "video_size", res_str.c_str(), 0);
  }

  if (avformat_open_input(&ifmt_ctx, url.c_str(), ifmt, &options) < 0)
  {
    av_dict_free(&options);
    if (ifmt_ctx)
    {
      avformat_free_context(ifmt_ctx);
      ifmt_ctx = nullptr;
    }
    return false;
  }

  av_dict_free(&options);
  if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0)
  {
    avformat_close_input(&ifmt_ctx);
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::close()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  // Request prompt interruption of blocking read calls before waiting on worker exit.
  this->interrupt_requested = true;
  is_running = false;
  MARTY_LOG_INFO(sbr_log, "Stopping stream capture worker");
  if (worker_future.valid()) { worker_future.get(); }
  MARTY_LOG_INFO(sbr_log, "Stream capture worker stopped");

  // make sure the writer thread has exited before closing the muxer
  stopRecording();
  clearBuffer();

  if (sws_ctx)
  {
    sws_freeContext(sws_ctx);
    sws_ctx = nullptr;
  }
  if (decoder_ctx) { avcodec_free_context(&decoder_ctx); }
  if (ifmt_ctx) { avformat_close_input(&ifmt_ctx); }
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::readFrame(cv::Mat& out_frame)
{
  std::lock_guard<mutex_type> lock(decoded_frames_mtx);
  if (decoded_frames.empty())
  {
    new_frame_available = false;
    return false;
  }

  out_frame = decoded_frames.back();
  decoded_frames.clear();
  new_frame_available = false;
  return true;
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::captureLoop()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  AVPacket* pkt = av_packet_alloc();
  AVFrame* av_frame = av_frame_alloc();

  AVStream* in_stream = ifmt_ctx->streams[video_stream_idx];
  double time_base_secs = av_q2d(in_stream->time_base);

  int64_t frame_count = 0;

  while (is_running)
  {
    MARTY_LOG_TRACE(sbr_log, "{} {}", (void*) (this), __func__);
    if (av_read_frame(ifmt_ctx, pkt) < 0) break;

    if (pkt->stream_index == video_stream_idx)
    {
      // V4L2 raw capture often delivers packets with no timestamps.
      // Synthesize monotonic PTS/DTS so the muxer can produce a valid file.
      if (pkt->pts == AV_NOPTS_VALUE)
      {
        MARTY_LOG_WARN(
            sbr_log, "Packet has no PTS. Synthesizing timestamps for packet {}.", frame_count);
        if (stream_fps > 0.0)
        {
          // Generate timestamps that match the measured frame rate in the
          // stream's native time_base so rescaling to the output works.
          AVRational const fps_q = av_d2q(stream_fps, 100000);
          pkt->pts = av_rescale_q(frame_count, av_inv_q(fps_q), in_stream->time_base);
          pkt->dts = pkt->pts;
          MARTY_LOG_DEBUG(sbr_log,
              "Synthesizing PTS/DTS for packet {}: PTS = {}, DTS = {}, time_base = {}/{}",
              frame_count, pkt->pts, pkt->dts, in_stream->time_base.num, in_stream->time_base.den);
        }
        else
        {
          pkt->pts = frame_count;
          pkt->dts = frame_count;
          MARTY_LOG_DEBUG(sbr_log,
              "FrameCount PTS/DTS for packet {}: PTS = {}, DTS = {}, time_base = {}/{}",
              frame_count, pkt->pts, pkt->dts, in_stream->time_base.num, in_stream->time_base.den);
        }
      }
      ++frame_count;

      // 1. Maintain Rolling Buffer of raw packets
      AVPacket* cloned_pkt = av_packet_clone(pkt);
      double pts_time = (cloned_pkt->pts == AV_NOPTS_VALUE) ? 0 : cloned_pkt->pts * time_base_secs;
      bool is_recording_ = is_recording.load();
      if (!is_recording_)
      {
        // add new packet to the rolling buffer and remove any packets that are too old
        // we do this under lock as the packet buffer is not thread safe and is shared with the writer thread
        std::lock_guard<mutex_type> lock(packet_buffer_mtx);
        packet_buffer.push_back({cloned_pkt, pts_time});

        // loop from frontend of the buffer and remove any packets that are older than the max_buffer_duration
        while (!packet_buffer.empty() &&
            (pts_time - packet_buffer.front().timestamp_secs > max_buffer_duration))
        {
          av_packet_free(&packet_buffer.front().pkt);
          packet_buffer.pop_front();
        }
      }

      // 2. Stream packets directly to file if actively recording
      else
      {
        if (cloned_pkt)
        {
          std::lock_guard<mutex_type> lock(packet_buffer_mtx);
          packet_queue.push(cloned_pkt);
        }
      }

      // 3. Decode frame natively for your OpenCV analysis pipeline
      if (avcodec_send_packet(decoder_ctx, pkt) == 0)
      {
        while (avcodec_receive_frame(decoder_ctx, av_frame) == 0)
        {
          if (!sws_ctx)
          {
            sws_ctx = sws_getContext(av_frame->width, av_frame->height,
                (AVPixelFormat) av_frame->format, av_frame->width, av_frame->height,
                AV_PIX_FMT_BGR24, SWS_BICUBIC, nullptr, nullptr, nullptr);
          }

          cv::Mat matFrame(av_frame->height, av_frame->width, CV_8UC3);
          uint8_t* dest[4] = {matFrame.data, nullptr, nullptr, nullptr};
          int dest_linesize[4] = {static_cast<int>(matFrame.step), 0, 0, 0};

          sws_scale(sws_ctx, av_frame->data, av_frame->linesize, 0, av_frame->height, dest,
              dest_linesize);

          {
            std::lock_guard<mutex_type> lock(decoded_frames_mtx);
            decoded_frames.push_back(matFrame);
            if (decoded_frames.size() > max_decoded_queue_size) { decoded_frames.pop_front(); }
            new_frame_available = true;
          }
        }
      }
    }
    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
  av_frame_free(&av_frame);
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::startRecording(std::string const& output_filename, double fps)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  if (is_recording) return true;

  if (fps > 0.0) { stream_fps = fps; }

  if (!initOutputMuxer(output_filename))
  {
    std::cerr << "Failed to initialize output muxer" << std::endl;
    return false;
  }

  // Drain the buffered history into the writer queue before starting live writes.
  {
    std::lock_guard<mutex_type> lock(packet_buffer_mtx);
    for (auto const& bpkt : packet_buffer) { packet_queue.push(bpkt.pkt); }
    packet_buffer.clear();
  }

  // 2. Spawn dedicated writing thread
  last_mux_dts = AV_NOPTS_VALUE;
  is_recording = true;
  writer_future = hpx::async(this->executor, [this]() { writerLoop(); });
  return true;
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::stopRecording()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  {
    if (!is_recording.load()) return;
    is_recording = false;
  }

  // Wake up and join the async writer thread to ensure all frames are written
  writer_cv.notify_all();
  if (writer_future.valid()) { writer_future.get(); }

  closeOutputMuxer();
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::initOutputMuxer(std::string const& filename)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, filename.c_str());
  if (!ofmt_ctx) return false;

  AVStream* in_stream = ifmt_ctx->streams[video_stream_idx];
  AVStream* out_stream = avformat_new_stream(ofmt_ctx, nullptr);
  avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
  out_stream->codecpar->codec_tag = 0;

  // Inform the encoder/muxer infrastructure that the MP4 container format
  // requires global headers (extradata) rather than repeating in-band headers.
  if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) { ofmt_ctx->flags |= AVFMT_GLOBALHEADER; }

  if (is_url)
  {
    // IP Camera: ALWAYS match the input stream's native timebase exactly
    // to preserve internal compressed bitstream syntax and hardware decoding compliance
    out_stream->time_base = in_stream->time_base;
    out_stream->avg_frame_rate = in_stream->avg_frame_rate;
  }
  else
  {
    // USB Camera: Fall back to your custom or measured FPS timing logic
    // since local raw driver streams do not possess a fixed network transport clock
    if (stream_fps > 0.0)
    {
      out_stream->time_base = av_d2q(1.0 / stream_fps, 100000);
      out_stream->avg_frame_rate = av_d2q(stream_fps, 100000);
    }
    else
    {
      out_stream->time_base = in_stream->time_base;
      out_stream->avg_frame_rate = in_stream->avg_frame_rate;
    }
  }

  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
  {
    if (avio_open(&ofmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) return false;
  }

  if (avformat_write_header(ofmt_ctx, nullptr) < 0) return false;
  return true;
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::closeOutputMuxer()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  if (ofmt_ctx)
  {
    av_write_trailer(ofmt_ctx);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) { avio_closep(&ofmt_ctx->pb); }
    avformat_free_context(ofmt_ctx);
    ofmt_ctx = nullptr;
  }
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::clearBuffer()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  std::lock_guard<mutex_type> lock(packet_buffer_mtx);
  while (!packet_buffer.empty())
  {
    av_packet_free(&packet_buffer.front().pkt);
    packet_buffer.pop_front();
  }
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::writerLoop()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  int64_t first_stream_dts = AV_NOPTS_VALUE;
  bool I_frame_found = false;
  AVStream* in_stream = ifmt_ctx->streams[video_stream_idx];
  while (true)
  {
    std::unique_lock<mutex_type> lock(packet_buffer_mtx);
    writer_cv.wait(lock, [this]() { return !is_recording; });

    auto write_lambda = [this, in_stream, &I_frame_found, &first_stream_dts](AVPacket* pkt) {
      // 1. Check for the absolute anchor point (I-Frame/Keyframe)
      if (pkt->flags & AV_PKT_FLAG_KEY) { I_frame_found = true; }

      // 2. Drop all packets until the first I-Frame has been established
      if (!I_frame_found)
      {
        av_packet_free(&pkt);
        return;
      }

      // 3. Establish the file's absolute zero-baseline clock using the first saved packet
      if (first_stream_dts == AV_NOPTS_VALUE) { first_stream_dts = pkt->dts; }

      // 4. Subtract the baseline to pull the network clock back to 00:00
      if (pkt->dts != AV_NOPTS_VALUE) pkt->dts -= first_stream_dts;
      if (pkt->pts != AV_NOPTS_VALUE) pkt->pts -= first_stream_dts;

      // 5. Convert the packet time units safely from Camera scale to MP4 scale
      av_packet_rescale_ts(pkt, in_stream->time_base, ofmt_ctx->streams[0]->time_base);
      pkt->stream_index = 0;

      // 6. Final container-level safeguard for overlapping or duplicate timestamps
      if (last_mux_dts != AV_NOPTS_VALUE && pkt->dts <= last_mux_dts)
      {
        int64_t drift = (last_mux_dts + 1) - pkt->dts;
        pkt->dts += drift;
        pkt->pts += drift;
      }
      last_mux_dts = pkt->dts;

      // 7. Commit cleanly to disk and clean up the pointer
      av_interleaved_write_frame(ofmt_ctx, pkt);
      av_packet_free(&pkt);
    };

    if (is_recording)
    {
      while (!packet_queue.empty())
      {
        AVPacket* ready_pkt = packet_queue.front();
        packet_queue.pop();
        write_lambda(ready_pkt);
      }
    }
    if (!is_recording)
    {
      while (!packet_queue.empty())
      {
        AVPacket* ready_pkt = packet_queue.front();
        packet_queue.pop();
        write_lambda(ready_pkt);
      }
    }

    if (!is_recording && packet_queue.empty())
    {
      MARTY_LOG_INFO(sbr_log,
          "Recording stopped and all buffered packets have been written. Exiting writer thread.");
      break;
    }
  }
}