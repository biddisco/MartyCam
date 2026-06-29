#ifndef STREAM_CAPTURE_THREAD_H
#define STREAM_CAPTURE_THREAD_H
//
#include <hpx/config.hpp>
//
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio.hpp>
//
#include <QMutex>
#include <QObject>
#include <QWaitCondition>
#include <atomic>
#include <mutex>
//
#include <boost/lockfree/spsc_queue.hpp>
#include <memory>
//
#include <hpx/future.hpp>
#include <hpx/include/parallel_executors.hpp>
//
#include <vector>
#include "martycam/core/ConcurrentCircularBuffer.h"
#include "martycam/core/fps_helper.h"
#include "martycam/core/stream_buffer_recorder.hpp"
#include "martycam/core/timelapse_ffmpeg_writer.h"

#define IMAGE_QUEUE_LEN 1024

typedef boost::circular_buffer<int> IntCircBuff;
typedef std::shared_ptr<ConcurrentCircularBuffer<cv::Mat>> ImageBuffer;
typedef std::shared_ptr<
    boost::lockfree::spsc_queue<cv::Mat, boost::lockfree::capacity<IMAGE_QUEUE_LEN>>>
    ImageQueue;

class StreamCaptureThread;
typedef std::shared_ptr<StreamCaptureThread> StreamCaptureThread_SP;

class StreamCaptureThread : public QObject
{
  Q_OBJECT;

  public:
  StreamCaptureThread(ImageBuffer imageBuffer, cv::Size const& size, int rotation,
      std::string const& URL, hpx::execution::parallel_executor exec, int requestedFps,
      int requestedFourCC = cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
  ~StreamCaptureThread();

  void run();
  //
  bool connectCamera(std::string const& URL);
  bool startCapture();
  bool stopCapture();
  //
  void setRequestedFps(int value);
  int getRequestedFps() { return requestedFps; }
  double getActualFps() { return actualFps.value(); }
  double getGrabFps() { return grabFps.value(); }
  double getCaptureFps() { return captureFps.value(); }
  bool isCapturing() { return captureActive; }
  bool isRequestedSizeCorrect() { return requestedSizeCorrect; }

  int GetFrameCounter() { return this->FrameCounter; }
  std::string getCaptureStatusString() { return this->CaptureStatus; }

  //
  // Motion film
  //
  void setWriteMotionAVI(bool write);
  bool getWriteMotionAVI() { return this->MotionAVI_Writing; }
  void setWriteMotionAVIName(char const* name);

  //
  // Time Lapse film
  //
  void addNextFrameToTimeLapse(bool write) { this->TimeLapseAVI_Writing = write; }
  void setWriteTimeLapseAVIName(char const* name);
  void setTimeLapseBitrateMBps(double value) { this->TimeLapseBitrateMBps = value; }
  std::chrono::system_clock::time_point getLastTimeLapseWriteTime()
  {
    return this->timeLapseFFmpegWriter->getLastWriteTime();
  }

  //
  // General
  //
  void setWriteMotionAVIDir(char const* dir);
  void setWriteTimeLapseAVIDir(char const* dir);
  void closeAVI();

  void saveTimeLapseAVI(cv::Mat image);
  void startTimeLapse(double fps);
  void stopTimeLapse();
  void updateTimeLapse();

  void setRotation(int value);
  //
  void rotateImage(cv::Mat const& source, cv::Mat& rotated);
  static void captionImage(cv::Mat& image);

  cv::Size getImageSize() { return this->imageSize; }
  cv::Size getRotatedSize() { return this->rotatedSize; }

  private:
  void setAbort(bool a) { this->abort = a; }

  signals:
  void RecordingState(bool);

  private:
  //
  QMutex stopLock;
  std::mutex aviLock;
  QWaitCondition stopWait;
  hpx::execution::parallel_executor executor;
  //
  std::atomic<bool> abort;
  bool finished;
  ImageBuffer imageBuffer;
  bool captureActive;
  bool deInterlace;
  cv::Size imageSize;
  bool requestedSizeCorrect;
  cv::Size rotatedSize;
  fps_helper actualFps;
  fps_helper grabFps;
  fps_helper captureFps;
  int requestedFps;
  int requestedFourCC;
  int rotation;
  int FrameCounter;
  int motion_video_FrameCounter;
  ImageBuffer aviBuffer;
  //
  //   cv::VideoWriter TimeLapseAVI_Writer;
  std::shared_ptr<TimeLapseFFmpegWriter> timeLapseFFmpegWriter;
  std::atomic<bool> MotionAVI_Writing;
  std::atomic<bool> aviWriterActive;
  std::string AVI_Directory;
  std::string MotionAVI_Name;
  std::string TimeLapse_Directory;
  std::string TimeLapseAVI_Name;
  double TimeLapseBitrateMBps = 4.0;
  std::string CaptureStatus;
  std::string CameraURL;
  //
  cv::Size text_size;
  cv::Mat currentFrame;
  cv::Mat rotatedImage;

  // Stream buffer recorder alternative to cv::VideoCapture
  std::unique_ptr<stream_buffer_recorder> streamRecorder;

  public:
  std::atomic<bool> TimeLapseAVI_Writing;
};

#endif
