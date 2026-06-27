#include "martycam/core/timelapse_ffmpeg_writer.h"

#include <cmath>

#include <opencv2/imgproc.hpp>

TimeLapseFFmpegWriter::~TimeLapseFFmpegWriter() { this->close(); }

bool TimeLapseFFmpegWriter::open(
    std::string const& path, int width_, int height_, double fps, double bitrateMBps)
{
  this->close();

  if (width_ <= 0 || height_ <= 0)
  {
    this->setError("invalid frame size");
    return false;
  }

  this->width = width_;
  this->height = height_;
  this->frame_index = 0;

  if (avformat_alloc_output_context2(&this->format_ctx, nullptr, nullptr, path.c_str()) < 0 ||
      !this->format_ctx)
  {
    this->setError("avformat_alloc_output_context2 failed");
    return false;
  }

  AVCodec const* codec = avcodec_find_encoder_by_name("libx264");
  if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
  if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
  if (!codec)
  {
    this->setError("no suitable video encoder found");
    this->close();
    return false;
  }

  this->stream = avformat_new_stream(this->format_ctx, nullptr);
  if (!this->stream)
  {
    this->setError("avformat_new_stream failed");
    this->close();
    return false;
  }

  this->codec_ctx = avcodec_alloc_context3(codec);
  if (!this->codec_ctx)
  {
    this->setError("avcodec_alloc_context3 failed");
    this->close();
    return false;
  }

  double const safe_fps = fps > 0.0 ? fps : 10.0;
  int const fps_int = static_cast<int>(std::round(safe_fps));
  this->codec_ctx->codec_id = codec->id;
  this->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
  this->codec_ctx->width = this->width;
  this->codec_ctx->height = this->height;
  this->codec_ctx->time_base = AVRational{1, fps_int > 0 ? fps_int : 10};
  this->codec_ctx->framerate = AVRational{fps_int > 0 ? fps_int : 10, 1};
  this->codec_ctx->sample_aspect_ratio = AVRational{1, 1};
  this->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  this->codec_ctx->gop_size = std::max(1, fps_int);
  this->codec_ctx->max_b_frames = 0;
  this->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;

  // Keep timelapse files compact and avoid large encoder queues that can
  // delay shutdown when flushing.
  double const safeMBps = bitrateMBps > 0.0 ? bitrateMBps : 4.0;
  int64_t const target_bitrate =
      std::max<int64_t>(1500000, static_cast<int64_t>(safeMBps * 8.0 * 1000.0 * 1000.0));
  this->codec_ctx->bit_rate = target_bitrate;
  this->codec_ctx->rc_max_rate = target_bitrate;
  this->codec_ctx->rc_min_rate = target_bitrate;
  this->codec_ctx->rc_buffer_size = target_bitrate;
  this->codec_ctx->bit_rate_tolerance = target_bitrate / 20;

  if (codec->id == AV_CODEC_ID_H264)
  {
    av_opt_set(this->codec_ctx->priv_data, "preset", "superfast", 0);
    av_opt_set(this->codec_ctx->priv_data, "tune", "zerolatency", 0);
    av_opt_set(this->codec_ctx->priv_data, "profile", "main", 0);
    av_opt_set(this->codec_ctx->priv_data, "rc-lookahead", "0", 0);
    av_opt_set(this->codec_ctx->priv_data, "threads", "1", 0);

    std::string const bitrate_k = std::to_string(target_bitrate / 1000) + "k";
    std::string const maxrate_k = std::to_string(target_bitrate / 1000) + "k";
    std::string const minrate_k = std::to_string(target_bitrate / 1000) + "k";
    std::string const bufsize_k = std::to_string(target_bitrate / 1000) + "k";
    av_opt_set(this->codec_ctx->priv_data, "b", bitrate_k.c_str(), 0);
    av_opt_set(this->codec_ctx->priv_data, "maxrate", maxrate_k.c_str(), 0);
    av_opt_set(this->codec_ctx->priv_data, "minrate", minrate_k.c_str(), 0);
    av_opt_set(this->codec_ctx->priv_data, "bufsize", bufsize_k.c_str(), 0);

    std::string const x264_params =
        "nal-hrd=cbr:force-cfr=1:scenecut=0:keyint=" + std::to_string(std::max(1, fps_int)) +
        ":min-keyint=" + std::to_string(std::max(1, fps_int));
    av_opt_set(this->codec_ctx->priv_data, "x264-params", x264_params.c_str(), 0);
  }

  if (this->format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
  {
    this->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  if (avcodec_open2(this->codec_ctx, codec, nullptr) < 0)
  {
    this->setError("avcodec_open2 failed");
    this->close();
    return false;
  }

  if (avcodec_parameters_from_context(this->stream->codecpar, this->codec_ctx) < 0)
  {
    this->setError("avcodec_parameters_from_context failed");
    this->close();
    return false;
  }
  this->stream->time_base = this->codec_ctx->time_base;
  this->stream->avg_frame_rate = this->codec_ctx->framerate;
  this->stream->sample_aspect_ratio = AVRational{1, 1};

  if (!(this->format_ctx->oformat->flags & AVFMT_NOFILE))
  {
    if (avio_open(&this->format_ctx->pb, path.c_str(), AVIO_FLAG_WRITE) < 0)
    {
      this->setError("avio_open failed");
      this->close();
      return false;
    }
  }

  if (avformat_write_header(this->format_ctx, nullptr) < 0)
  {
    this->setError("avformat_write_header failed");
    this->close();
    return false;
  }

  this->frame = av_frame_alloc();
  this->packet = av_packet_alloc();
  if (!this->frame || !this->packet)
  {
    this->setError("failed to allocate frame/packet");
    this->close();
    return false;
  }

  this->frame->format = this->codec_ctx->pix_fmt;
  this->frame->width = this->width;
  this->frame->height = this->height;
  this->frame->sample_aspect_ratio = AVRational{1, 1};
  if (av_frame_get_buffer(this->frame, 32) < 0)
  {
    this->setError("av_frame_get_buffer failed");
    this->close();
    return false;
  }

  this->sws_ctx = sws_getContext(this->width, this->height, AV_PIX_FMT_BGR24, this->width,
      this->height, this->codec_ctx->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (!this->sws_ctx)
  {
    this->setError("sws_getContext failed");
    this->close();
    return false;
  }

  this->opened = true;
  this->last_error.clear();
  return true;
}

bool TimeLapseFFmpegWriter::encodeFrame(AVFrame* frame_to_encode)
{
  if (avcodec_send_frame(this->codec_ctx, frame_to_encode) < 0)
  {
    this->setError("avcodec_send_frame failed");
    return false;
  }

  while (true)
  {
    int const ret = avcodec_receive_packet(this->codec_ctx, this->packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
    if (ret < 0)
    {
      this->setError("avcodec_receive_packet failed");
      return false;
    }

    av_packet_rescale_ts(this->packet, this->codec_ctx->time_base, this->stream->time_base);
    this->packet->stream_index = this->stream->index;

    if (av_interleaved_write_frame(this->format_ctx, this->packet) < 0)
    {
      av_packet_unref(this->packet);
      this->setError("av_interleaved_write_frame failed");
      return false;
    }
    av_packet_unref(this->packet);
  }

  return true;
}

bool TimeLapseFFmpegWriter::write(cv::Mat const& image)
{
  if (!this->opened)
  {
    this->setError("writer not open");
    return false;
  }

  cv::Mat bgr;
  if (image.type() == CV_8UC3)
    bgr = image;
  else if (image.type() == CV_8UC1) { cv::cvtColor(image, bgr, cv::COLOR_GRAY2BGR); }
  else if (image.type() == CV_8UC4) { cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR); }
  else
  {
    this->setError("unsupported input image format");
    return false;
  }

  if (bgr.cols != this->width || bgr.rows != this->height)
  {
    cv::resize(bgr, bgr, cv::Size(this->width, this->height));
  }

  if (av_frame_make_writable(this->frame) < 0)
  {
    this->setError("av_frame_make_writable failed");
    return false;
  }

  uint8_t const* src_slices[4] = {bgr.data, nullptr, nullptr, nullptr};
  int src_stride[4] = {static_cast<int>(bgr.step[0]), 0, 0, 0};
  sws_scale(this->sws_ctx, src_slices, src_stride, 0, this->height, this->frame->data,
      this->frame->linesize);

  this->frame->pts = this->frame_index++;
  return this->encodeFrame(this->frame);
}

void TimeLapseFFmpegWriter::close()
{
  if (this->codec_ctx && this->opened) { this->encodeFrame(nullptr); }

  if (this->format_ctx && this->opened) { av_write_trailer(this->format_ctx); }

  if (this->packet)
  {
    av_packet_free(&this->packet);
    this->packet = nullptr;
  }

  if (this->frame)
  {
    av_frame_free(&this->frame);
    this->frame = nullptr;
  }

  if (this->sws_ctx)
  {
    sws_freeContext(this->sws_ctx);
    this->sws_ctx = nullptr;
  }

  if (this->codec_ctx)
  {
    avcodec_free_context(&this->codec_ctx);
    this->codec_ctx = nullptr;
  }

  if (this->format_ctx)
  {
    if (!(this->format_ctx->oformat->flags & AVFMT_NOFILE) && this->format_ctx->pb)
    {
      avio_closep(&this->format_ctx->pb);
    }
    avformat_free_context(this->format_ctx);
    this->format_ctx = nullptr;
  }

  this->stream = nullptr;
  this->opened = false;
  this->width = 0;
  this->height = 0;
  this->frame_index = 0;
}
