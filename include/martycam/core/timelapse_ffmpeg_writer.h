#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <opencv2/core/core.hpp>
//
#include <hpx/modules/synchronization.hpp>
#include "debug/logging.hpp"
//
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

// ----------------------------------------------------------------------------
static auto cap_log = martycam::log::create("StreamCapture");

// ----------------------------------------------------------------------------
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
  std::uint64_t getFrameNumber() const { return this->frame_counter.load(); }

  bool validateSettings()
  {
    bool geom = this->framerate > 0.0 && this->bitrateMBps > 0.0;
    bool files = !this->filepath.empty() && !this->filename.empty();
    if (!std::filesystem::exists(filepath))
    {
      if (!std::filesystem::create_directories(filepath))
      {
        MARTY_LOG_ERROR(cap_log,
            "{:<20} Cannot create time-lapse writer: failed to create directory {}",
            "StreamCaptureThread", filepath);
        return false;
      }
    }
    return geom && files;
  }
  //
  void setOutputFileName(std::string const& filename) { this->filename = filename; }
  void setOutputDirectory(std::string const& directory) { this->filepath = directory; }
  void setBitrateMBps(double MBps) { this->bitrateMBps = MBps; }
  double getBitrateMBps() const { return this->bitrateMBps; }
  void setFPS(double fps) { this->framerate = fps; }
  double getFPS() const { return this->framerate; }
  void setFrameInterval(std::uint64_t ms) { this->frameIntervalMs = ms; }
  std::uint64_t getFrameInterval() const { return this->frameIntervalMs; }
  std::string buildOutputPath() const
  {
    if (this->filepath.empty() || this->filename.empty()) return std::string();
    return this->filepath + "/" + this->filename + ".mp4";
  }

  // ----------------------------------------------------------------------------
  private:
  // ----------------------------------------------------------------------------
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
  std::atomic<std::uint64_t> frame_counter{0};
  int64_t frame_index = 0;
  std::string last_error;
  std::chrono::system_clock::time_point lastWriteTime;
  std::string filepath;
  std::string filename;
  double framerate = 20.0;
  double bitrateMBps = 4.0;
  std::uint64_t frameIntervalMs = 1000;
};
