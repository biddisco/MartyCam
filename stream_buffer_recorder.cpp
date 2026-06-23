#include <iostream>
//
#include "debug/logging.hpp"
#include "stream_buffer_recorder.hpp"
//
#include <hpx/async_local/async_fwd.hpp>
#include <hpx/executors/parallel_executor.hpp>
#include <hpx/include/async.hpp>
#include <hpx/include/parallel_executors.hpp>

// ----------------------------------------------------------------------------
static auto sbr_log = martycam::log::create("StreamBuf");

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
bool stream_buffer_recorder::open(int width, int height, std::string const& fourcc_str)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
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
  AVDictionary* options = nullptr;

  // Force TCP transport to prevent packet loss and blocky artifacts
  av_dict_set(&options, "rtsp_transport", "tcp", 0);

  // Keep your timeout setting
  av_dict_set(&options, "timeout", "2000000", 0);

  if (avformat_open_input(&ifmt_ctx, url.c_str(), nullptr, &options) < 0)
  {
    av_dict_free(&options);
    return false;
  }

  av_dict_free(&options);
  if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0) return false;
  return true;
}

//----------------------------------------------------------------------------
bool stream_buffer_recorder::open_usb(int width, int height, std::string const& fourcc_str)
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  avdevice_register_all();

  AVInputFormat const* ifmt = av_find_input_format("v4l2");
  if (!ifmt) return false;

  AVDictionary* options = nullptr;

  // 1. Map OpenCV/MartyCam FourCC strings to FFmpeg V4L2 names
  std::string ffmpeg_v4l2_format = "";
  if (!fourcc_str.empty())
  {
    if (fourcc_str == "GREY" || fourcc_str == "Y800" || fourcc_str == "Y8  ")
    {
      ffmpeg_v4l2_format = "raw";    // V4L2 uses 'raw' driver configurations for uncompressed gray
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
    return false;
  }

  av_dict_free(&options);
  if (avformat_find_stream_info(ifmt_ctx, nullptr) < 0) return false;
  return true;
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::close()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  // block until the capture thread has exited to ensure all packets are processed
  is_running = false;
  if (worker_future.valid()) { worker_future.get(); }

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
  // MARTY_LOG_SCOPE(sbr_log, "{}", "Taking lock in readFrame()");
  if (!new_frame_available.load()) return false;
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

  // 1. Drain history buffer straight into the sorting window initialization
  {
    std::lock_guard<mutex_type> lock(packet_buffer_mtx);
    std::vector<AVPacket*> historical_packets;
    historical_packets.reserve(packet_buffer.size());

    for (auto const& bpkt : packet_buffer)
    {
      AVPacket* cloned = av_packet_clone(bpkt.pkt);
      if (cloned) { historical_packets.push_back(cloned); }
    }

    // Sort the history
    std::sort(historical_packets.begin(), historical_packets.end(),
        [](AVPacket* a, AVPacket* b) { return a->dts < b->dts; });

    // Transfer history to the processing queue
    for (AVPacket* pkt : historical_packets) { live_input_queue.push_back(pkt); }
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
  // MARTY_LOG_DEBUG(sbr_log, "Taking lock in stopRecording()");
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
void stream_buffer_recorder::captureLoop()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  AVPacket* pkt = av_packet_alloc();
  AVFrame* av_frame = av_frame_alloc();
  AVFrame* bgr_frame = av_frame_alloc();

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
        if (stream_fps > 0.0)
        {
          // Generate timestamps that match the measured frame rate in the
          // stream's native time_base so rescaling to the output works.
          AVRational const fps_q = av_d2q(stream_fps, 100000);
          pkt->pts = av_rescale_q(frame_count, av_inv_q(fps_q), in_stream->time_base);
          pkt->dts = pkt->pts;
        }
        else
        {
          pkt->pts = frame_count;
          pkt->dts = frame_count;
        }
      }
      ++frame_count;

      // 1. Maintain Rolling Buffer of raw packets
      AVPacket* cloned_pkt = av_packet_clone(pkt);
      double pts_time = (cloned_pkt->pts == AV_NOPTS_VALUE) ? 0 : cloned_pkt->pts * time_base_secs;
      {
        // add new packet to the rolling buffer and remove any packets that are too old
        // we do this under lock as the packet buffer is not thread safe and is shared with the writer thread
        // MARTY_LOG_DEBUG(sbr_log,
        //     "Taking lock in captureLoop() for packet with PTS = {} and timestamp = {}",
        //     cloned_pkt->pts, pts_time);
        std::lock_guard<mutex_type> lock(packet_buffer_mtx);
        packet_buffer.push_back({cloned_pkt, pts_time});

        while (!packet_buffer.empty() &&
            (pts_time - packet_buffer.front().timestamp_secs > max_buffer_duration))
        {
          av_packet_free(&packet_buffer.front().pkt);
          packet_buffer.pop_front();
        }
      }

      // 2. Stream packets directly to file if actively recording
      if (is_recording)
      {
        AVPacket* rec_pkt = av_packet_clone(pkt);
        if (rec_pkt)
        {
          // MARTY_LOG_DEBUG(sbr_log,
          //     "Taking lock in captureLoop() for recording packet with DTS = {}", rec_pkt->dts);
          std::lock_guard<mutex_type> lock(packet_buffer_mtx);
          live_input_queue.push_back(rec_pkt);
          MARTY_LOG_DEBUG(sbr_log,
              "waking writer thread for new packet with DTS = {}. Queue size = {}", rec_pkt->dts,
              live_input_queue.size());
          writer_cv.notify_one();    // Wake up the sorting loop
          MARTY_LOG_DEBUG(sbr_log,
              "Added packet with DTS = {} to live_input_queue. Queue size = {}", rec_pkt->dts,
              live_input_queue.size());
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

          decoded_frames.push_back(matFrame);
          if (decoded_frames.size() > max_decoded_queue_size) { decoded_frames.pop_front(); }
          new_frame_available = true;
        }
      }
    }
    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);
  av_frame_free(&av_frame);
  av_frame_free(&bgr_frame);
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
  // MARTY_LOG_DEBUG(sbr_log, "Taking lock in clearBuffer()");
  std::lock_guard<mutex_type> lock(packet_buffer_mtx);
  while (!packet_buffer.empty())
  {
    av_packet_free(&packet_buffer.front().pkt);
    packet_buffer.pop_front();
  }

  // Clean up any stray pointers left in the live look-ahead queue
  for (AVPacket* pkt : live_reorder_queue) { av_packet_free(&pkt); }
  live_reorder_queue.clear();
}

//----------------------------------------------------------------------------
void stream_buffer_recorder::writerLoop()
{
  MARTY_LOG_SCOPE(sbr_log, "{} {}", (void*) (this), __func__);
  std::vector<AVPacket*> sorting_window;
  size_t const target_window_depth = 20;    // Your proposed 20 frame threshold

  while (true)
  {
    std::unique_lock<mutex_type> lock(packet_buffer_mtx);

    // Wait until new packets arrive or recording stops
    MARTY_LOG_INFO(sbr_log,
        "Writer thread waiting: live_input_queue size = {}, sorting_window size = {}, is_recording "
        "= {}",
        live_input_queue.size(), sorting_window.size(), is_recording.load());
    writer_cv.wait(
        lock, [this, &sorting_window]() { return !live_input_queue.empty() || !is_recording; });

    MARTY_LOG_INFO(sbr_log,
        "Writer thread woke up: live_input_queue size = {}, sorting_window size = {}, is_recording "
        "= {}",
        live_input_queue.size(), sorting_window.size(), is_recording.load());
    // Pull packets out of the ingestion queue into our sorting vector
    while (!live_input_queue.empty())
    {
      sorting_window.push_back(live_input_queue.front());
      live_input_queue.pop_front();
    }

    // Keep the active sliding window sorted by true decoding timestamp
    std::sort(sorting_window.begin(), sorting_window.end(),
        [](AVPacket* a, AVPacket* b) { return a->dts < b->dts; });

    // If recording is running, only pop off packets if we have reached our look-ahead threshold
    while (
        !sorting_window.empty() && (sorting_window.size() >= target_window_depth || !is_recording))
    {
      MARTY_LOG_INFO(sbr_log,
          "Writing packet with DTS = {} to output file. Sorting window size = {}",
          sorting_window.front()->dts, sorting_window.size());
      AVPacket* ready_pkt = sorting_window.front();
      sorting_window.erase(sorting_window.begin());

      // Rescale timestamps right at the moment of file muxing
      AVStream* in_stream = ifmt_ctx->streams[video_stream_idx];
      av_packet_rescale_ts(ready_pkt, in_stream->time_base, ofmt_ctx->streams[0]->time_base);
      ready_pkt->stream_index = 0;

      // Final safeguard for duplicate timestamps
      if (last_mux_dts != AV_NOPTS_VALUE && ready_pkt->dts <= last_mux_dts)
      {
        int64_t drift = (last_mux_dts + 1) - ready_pkt->dts;
        ready_pkt->dts += drift;
        ready_pkt->pts += drift;
      }
      last_mux_dts = ready_pkt->dts;

      // Commit cleanly to disk
      av_interleaved_write_frame(ofmt_ctx, ready_pkt);
      av_packet_free(&ready_pkt);
    }

    // Break out completely if recording was stopped and the remaining buffer is completely empty
    if (!is_recording && sorting_window.empty())
    {
      MARTY_LOG_INFO(sbr_log,
          "Recording stopped and all buffered packets have been written. Exiting writer thread.");
      break;
    }
  }
}