#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <opencv2/core/core.hpp>
//
#include <hpx/modules/synchronization.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

class TimeLapseFFmpegWriter
{
  public:
  TimeLapseFFmpegWriter() = default;
  ~TimeLapseFFmpegWriter();

  bool open(std::string const& path, int width, int height, double fps, double bitrateMBps);
  bool write(cv::Mat const& image);
  void close();

  bool isOpen() const { return this->opened; }
  std::string const& lastError() const { return this->last_error; }
  std::chrono::system_clock::time_point getLastWriteTime() const { return this->lastWriteTime; }

  private:
  void setError(std::string const& msg) { this->last_error = msg; }
  bool encodeFrame(AVFrame* frame);

  using mutex_type = hpx::spinlock;
  mutex_type writer_lock;
  //
  AVFormatContext* format_ctx = nullptr;
  AVCodecContext* codec_ctx = nullptr;
  AVStream* stream = nullptr;
  SwsContext* sws_ctx = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* packet = nullptr;

  std::atomic<bool> opened{false};
  int width = 0;
  int height = 0;
  int64_t frame_index = 0;
  std::string last_error;
  std::chrono::system_clock::time_point lastWriteTime;
};
