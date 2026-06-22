#include "stream_buffer_recorder.hpp"
#include <iostream>

stream_buffer_recorder::stream_buffer_recorder(
    std::string const& stream_url, double buffer_duration_seconds)
  : url(stream_url)
  , max_buffer_duration(buffer_duration_seconds)
{
}

stream_buffer_recorder::~stream_buffer_recorder() { close(); }

bool stream_buffer_recorder::open(int width, int height, std::string const& fourcc_str)
{
  bool is_url = (url.find("://") != std::string::npos);

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
  worker_thread = std::thread(&stream_buffer_recorder::captureLoop, this);
  return true;
}

bool stream_buffer_recorder::open_ip()
{
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

bool stream_buffer_recorder::open_usb(int width, int height, std::string const& fourcc_str)
{
  avdevice_register_all();

  AVInputFormat const* ifmt = av_find_input_format("v4l2");
  if (!ifmt) return false;

  AVDictionary* options = nullptr;

  // 1. Map OpenCV/MartyCam FourCC strings to FFmpeg V4L2 names
  if (!fourcc_str.empty())
  {
    std::string ffmpeg_v4l2_format = "";

    if (fourcc_str == "GREY" || fourcc_str == "Y800" || fourcc_str == "Y8  ")
    {
      ffmpeg_v4l2_format = "raw";    // V4L2 uses 'raw' driver configurations for uncompressed gray
    }
    else if (fourcc_str == "MJPEG" || fourcc_str == "MJPG") { ffmpeg_v4l2_format = "mjpeg"; }
    else if (fourcc_str == "YUYV" || fourcc_str == "YUY2") { ffmpeg_v4l2_format = "yuyv422"; }

    if (!ffmpeg_v4l2_format.empty())
    {
      av_dict_set(&options, "input_format", ffmpeg_v4l2_format.c_str(), 0);
    }
  }

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

void stream_buffer_recorder::close()
{
  is_running = false;
  if (worker_thread.joinable()) worker_thread.join();

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

bool stream_buffer_recorder::readFrame(cv::Mat& out_frame)
{
  std::lock_guard<std::mutex> lock(mtx);
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

bool stream_buffer_recorder::startRecording(std::string const& output_filename)
{
  std::lock_guard<std::mutex> lock(mtx);
  if (is_recording) return true;

  if (!initOutputMuxer(output_filename))
  {
    std::cerr << "Failed to initialize output muxer" << std::endl;
    return false;
  }

  // Flush the existing rolling packet buffer straight into the output file
  for (auto const& bpkt : packet_buffer)
  {
    AVPacket* pkt = bpkt.pkt;
    av_packet_rescale_ts(
        pkt, ifmt_ctx->streams[video_stream_idx]->time_base, ofmt_ctx->streams[0]->time_base);
    pkt->stream_index = 0;
    av_interleaved_write_frame(ofmt_ctx, pkt);
  }

  is_recording = true;
  return true;
}

void stream_buffer_recorder::stopRecording()
{
  std::lock_guard<std::mutex> lock(mtx);
  if (!is_recording) return;
  is_recording = false;
  closeOutputMuxer();
}

void stream_buffer_recorder::captureLoop()
{
  AVPacket* pkt = av_packet_alloc();
  AVFrame* av_frame = av_frame_alloc();
  AVFrame* bgr_frame = av_frame_alloc();

  AVStream* in_stream = ifmt_ctx->streams[video_stream_idx];
  double time_base_secs = av_q2d(in_stream->time_base);

  while (is_running)
  {
    if (av_read_frame(ifmt_ctx, pkt) < 0) break;

    if (pkt->stream_index == video_stream_idx)
    {
      std::lock_guard<std::mutex> lock(mtx);

      // 1. Maintain Rolling Buffer of raw packets
      AVPacket* cloned_pkt = av_packet_clone(pkt);
      double pts_time = (cloned_pkt->pts == AV_NOPTS_VALUE) ? 0 : cloned_pkt->pts * time_base_secs;
      packet_buffer.push_back({cloned_pkt, pts_time});

      while (!packet_buffer.empty() &&
          (pts_time - packet_buffer.front().timestamp_secs > max_buffer_duration))
      {
        av_packet_free(&packet_buffer.front().pkt);
        packet_buffer.pop_front();
      }

      // 2. Stream directly to file if actively recording
      if (is_recording && ofmt_ctx)
      {
        AVPacket* rec_pkt = av_packet_clone(pkt);
        av_packet_rescale_ts(rec_pkt, in_stream->time_base, ofmt_ctx->streams[0]->time_base);
        rec_pkt->stream_index = 0;
        av_interleaved_write_frame(ofmt_ctx, rec_pkt);
        av_packet_free(&rec_pkt);
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

bool stream_buffer_recorder::initOutputMuxer(std::string const& filename)
{
  avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, filename.c_str());
  if (!ofmt_ctx) return false;

  AVStream* out_stream = avformat_new_stream(ofmt_ctx, nullptr);
  avcodec_parameters_copy(out_stream->codecpar, ifmt_ctx->streams[video_stream_idx]->codecpar);
  out_stream->codecpar->codec_tag = 0;

  if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
  {
    if (avio_open(&ofmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0) return false;
  }

  if (avformat_write_header(ofmt_ctx, nullptr) < 0) return false;
  return true;
}

void stream_buffer_recorder::closeOutputMuxer()
{
  if (ofmt_ctx)
  {
    av_write_trailer(ofmt_ctx);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) { avio_closep(&ofmt_ctx->pb); }
    avformat_free_context(ofmt_ctx);
    ofmt_ctx = nullptr;
  }
}

void stream_buffer_recorder::clearBuffer()
{
  std::lock_guard<std::mutex> lock(mtx);
  while (!packet_buffer.empty())
  {
    av_packet_free(&packet_buffer.front().pkt);
    packet_buffer.pop_front();
  }
  decoded_frames.clear();
  new_frame_available = false;
}