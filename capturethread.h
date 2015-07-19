#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
//
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
//
#ifndef Q_MOC_RUN
 #include <boost/shared_ptr.hpp>
 #include "ConcurrentCircularBuffer.h"
 typedef boost::circular_buffer< int > FrameSpeedBuffer;
 typedef boost::shared_ptr< ConcurrentCircularBuffer<cv::Mat> > ImageBuffer;
#endif
//

class CaptureThread : public QThread {
Q_OBJECT;
public: 
   CaptureThread(ImageBuffer buffer, const cv::Size &size, int device, const std::string &URL);
  ~CaptureThread() ;

  void run();
  void setAbort(bool a) { this->abort = a; }
  //
  bool connectCamera(int index, const std::string &URL);
  bool startCapture();
  bool stopCapture();
  //
  void setResolution(const cv::Size &res);
  //
  double getFPS() { return fps; }
  bool isCapturing() { return captureActive; }
  
  int  GetFrameCounter() { return this->FrameCounter; }
  std::string getCaptureStatusString() { return this->CaptureStatus; }

  //
  // Motion film
  //
  void setWriteMotionAVI(bool write) { this->MotionAVI_Writing = write; }
  bool getWriteMotionAVI() { return this->MotionAVI_Writing; }
  void setWriteMotionAVIName(const char *name);

  //
  // Time Lapse film
  //
  void addNextFrameToTimeLapse(bool write) { this->MotionAVI_Writing = write; }
  void setWriteTimeLapseAVIName(const char *name);

  //
  // General
  //
  void setWriteMotionAVIDir(const char *dir);
  void saveAVI(const cv::Mat &image);
  void closeAVI();

  void saveTimeLapseAVI(const cv::Mat &image);
  void startTimeLapse(double fps); 
  void stopTimeLapse(); 
  void updateTimeLapse();

  void setRotation(int value);
  //
  void  rotateImage(const cv::Mat &source, cv::Mat &rotated);
  void  captionImage(cv::Mat &image);

  cv::Size getImageSize() { return this->imageSize; }
  cv::Size getRotatedSize() { return this->rotatedSize; }

signals:
  void RecordingState(bool);

public:
  void updateFPS(int time);
  //
  QMutex           captureLock, stopLock;
  QWaitCondition   captureWait, stopWait;
  //
  bool             abort; 
  ImageBuffer      imageBuffer;
  bool             captureActive;
  bool             deInterlace;
  int              requestedFPS; 
  cv::Size         imageSize;
  cv::Size         rotatedSize;
  cv::VideoCapture capture;
  double           fps;
  FrameSpeedBuffer frameTimes;
  int              deviceIndex;
  int              rotation;
  int              FrameCounter;
  ImageBuffer      aviBuffer;
  //
  cv::VideoWriter  MotionAVI_Writer;
  cv::VideoWriter  TimeLapseAVI_Writer;
  bool             MotionAVI_Writing;
  bool             TimeLapseAVI_Writing;
  std::string      AVI_Directory;
  std::string      MotionAVI_Name;
  std::string      TimeLapseAVI_Name;
  std::string      CaptureStatus;
  std::string      CameraURL;
  //
  cv::Size         text_size;
  cv::Mat          currentFrame;
  cv::Mat          rotatedImage;
};

#endif
