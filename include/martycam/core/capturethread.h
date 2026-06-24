#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#ifdef USE_STREAM_CAPTURE_THREAD

# include "stream_capture_thread.h"

typedef StreamCaptureThread CaptureThread;
typedef std::shared_ptr<CaptureThread> CaptureThread_SP;

#else

//
# include <hpx/config.hpp>
//
# include <opencv2/core/core.hpp>
# include <opencv2/highgui/highgui.hpp>
# include <opencv2/videoio.hpp>
//
# include <QMutex>
# include <QObject>
# include <QWaitCondition>
# include <atomic>
# include <mutex>
//
# include <boost/lockfree/spsc_queue.hpp>
# include <memory>
//
# include <hpx/include/parallel_executors.hpp>
//
# include "ConcurrentCircularBuffer.h"
# include "fps_helper.h"
# define IMAGE_QUEUE_LEN 1024

typedef boost::circular_buffer<int> IntCircBuff;
typedef std::shared_ptr<ConcurrentCircularBuffer<cv::Mat>> ImageBuffer;
typedef std::shared_ptr<
    boost::lockfree::spsc_queue<cv::Mat, boost::lockfree::capacity<IMAGE_QUEUE_LEN>>>
    ImageQueue;

class CaptureThread;
typedef std::shared_ptr<CaptureThread> CaptureThread_SP;

class CaptureThread : public QObject
{
  Q_OBJECT;

  public:
  CaptureThread(ImageBuffer imageBuffer, cv::Size const& size, int rotation, std::string const& URL,
      hpx::execution::parallel_executor exec, int requestedFps,
      int requestedFourCC = cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
  ~CaptureThread();

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
  void setWriteMotionAVI(bool write) { this->MotionAVI_Writing = write; }
  bool getWriteMotionAVI() { return this->MotionAVI_Writing; }
  void setWriteMotionAVIName(char const* name);

  //
  // Time Lapse film
  //
  void addNextFrameToTimeLapse(bool write) { this->TimeLapseAVI_Writing = write; }
  void setWriteTimeLapseAVIName(char const* name);

  //
  // General
  //
  void setWriteMotionAVIDir(char const* dir);
  void saveAVI(cv::Mat const& image);
  void closeAVI();

  void saveTimeLapseAVI(cv::Mat const& image);
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
  bool tryResolutionUpdate(cv::Size requestedResolution);

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
  cv::VideoCapture capture;
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
  cv::VideoWriter MotionAVI_Writer;
  cv::VideoWriter TimeLapseAVI_Writer;
  std::atomic<bool> MotionAVI_Writing;
  std::atomic<bool> aviWriterActive;
  std::string AVI_Directory;
  std::string MotionAVI_Name;
  std::string TimeLapseAVI_Name;
  std::string CaptureStatus;
  std::string CameraURL;
  //
  cv::Size text_size;
  cv::Mat currentFrame;
  cv::Mat rotatedImage;

  public:
  std::atomic<bool> TimeLapseAVI_Writing;
};

#endif

#endif
