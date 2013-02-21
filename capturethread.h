#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
//
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
//
#ifndef Q_MOC_RUN
 #include <boost/shared_ptr.hpp>
 #include "ConcurrentCircularBuffer.h"
 typedef boost::shared_ptr< ConcurrentCircularBuffer<cv::Mat> > ImageBuffer;
#endif
//

class CaptureThread : public QThread {
Q_OBJECT;
public: 
   CaptureThread(ImageBuffer buffer, cv::Size &size, int device, QString &URL);
  ~CaptureThread() ;

  void run();
  void setAbort(bool a) { this->abort = a; }
  //
  bool startCapture(int framerate);
  void stopCapture();
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

  void setRotation(int value);
  //
  void  rotateImage(const cv::Mat &source, cv::Mat &rotated);
  void  captionImage(cv::Mat &image);

  cv::Size getImageSize() { return this->imageSize; }

signals:
  void RecordingState(bool);

private:
  void updateFPS(int time);
  //
  bool             abort; 
  QMutex           captureLock;
  QWaitCondition   captureWait;
  ImageBuffer      imageBuffer;
  bool             captureActive;
  cv::Size         imageSize;
  cv::Size         rotatedSize;
  cv::VideoCapture capture;
  double           fps;
  QQueue<int>      frameTimes;
  int              deviceIndex;
  int              rotation;
  int              FrameCounter;
  //
  cv::VideoWriter  MotionAVI_Writer;
  cv::VideoWriter  TimeLapseAVI_Writer;
  bool             MotionAVI_Writing;
  std::string      AVI_Directory;
  std::string      MotionAVI_Name;
  std::string      TimeLapseAVI_Name;
  std::string      CaptureStatus;
  //
  cv::Size         text_size;
  cv::Mat          rotatedImage;
};

#endif
