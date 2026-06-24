#pragma once

#include <atomic>
#include <deque>
#include <queue>
#include <string>
//
#include <hpx/config.hpp>
//
#include <hpx/condition_variable.hpp>
#include <hpx/execution/execution.hpp>
#include <hpx/executors/parallel_executor.hpp>
#include <hpx/futures/future.hpp>
#include <hpx/include/parallel_executors.hpp>
//
#include <opencv2/opencv.hpp>
//
#include "debug/logging.hpp"
//
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>    // for usb type devices
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
}
// ----------------------------------------------------------------------------
static auto sbr_log = martycam::log::create("StreamBuf");

class stream_buffer_recorder
{
  public:
  stream_buffer_recorder(std::string const& stream_url, double buffer_duration_seconds,
      hpx::execution::parallel_executor exec);
  ~stream_buffer_recorder();

  bool open(int width = 0, int height = 0, std::string const& fourcc_str = "");
  void close();

  // Pull decoded frames for your existing OpenCV analysis loop
  bool readFrame(cv::Mat& out_frame);

  // Call these to dump the buffer and write live stream bytes to disk
  bool startRecording(std::string const& output_filename, double fps = 0.0);
  void stopRecording();

  // Inform the recorder of the measured stream frame rate so it can
  // synthesise correct timestamps and set up the output muxer properly.
  void setStreamFps(double fps) { stream_fps = fps; }

  private:
  void captureLoop();
  void clearBuffer();
  bool initOutputMuxer(std::string const& filename);
  void closeOutputMuxer();

  // Cleanly split initialization methods
  bool open_ip();
  bool open_usb(int width, int height, std::string const& fourcc_str);

  //
  void writerLoop();

  std::string url;
  bool is_url;
  double max_buffer_duration;

  using mutex_type = hpx::spinlock;

  // FFmpeg Ingestion Contexts
  AVFormatContext* ifmt_ctx = nullptr;
  AVCodecContext* decoder_ctx = nullptr;
  int video_stream_idx = -1;
  SwsContext* sws_ctx = nullptr;

  // FFmpeg Recording Contexts
  AVFormatContext* ofmt_ctx = nullptr;
  double stream_fps = 0.0;
  int64_t last_mux_dts = AV_NOPTS_VALUE;

  // Decoded Frame Buffer (for OpenCV consumption)
  std::deque<cv::Mat> decoded_frames;
  size_t const max_decoded_queue_size = 5;
  std::atomic<bool> new_frame_available{false};
  mutex_type decoded_frames_mtx;

  // Rolling Packet Buffer Structure
  struct BufferedPacket
  {
    AVPacket* pkt;
    double timestamp_secs;
  };
  std::deque<BufferedPacket> packet_buffer;

  // Threading and State
  std::atomic<bool> is_running{false};
  std::atomic<bool> is_recording{false};
  hpx::execution::parallel_executor executor;
  mutex_type packet_buffer_mtx;
  hpx::future<void> worker_future;
  hpx::future<void> writer_future;
  hpx::condition_variable writer_cv;
  std::queue<AVPacket*> packet_queue;
};