#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
//
#include <opencv2/highgui/highgui.hpp>

class ImageBuffer;

class CaptureThread : public QThread {
Q_OBJECT;
public: 
   CaptureThread(ImageBuffer* buffer, cv::Size &size, int device, QString &URL);
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

  void setWriteAVI(bool write) { this->AVI_Writing = write; }
  bool getWriteAVI() { return this->AVI_Writing; }
  void setWriteAVIDir(const char *dir);
  void setWriteAVIName(const char *name);
  void saveAVI(const cv::Mat &image);
  void closeAVI();
  std::string getCaptureStatusString() { return this->CaptureStatus; }
  void setRotation(int value);
  //
  void  rotateImage(const cv::Mat &source, cv::Mat &rotated);
  void  captionImage(cv::Mat &image);

  cv::Size getImageSize() { return this->imageSize; }

signals:
  void RecordingState(bool);
  void NewImage();

private:
  void updateFPS(int time);
  //
  bool             abort; 
  QMutex           captureLock;
  QWaitCondition   captureWait;
  ImageBuffer     *imageBuffer;
  bool             captureActive;
  cv::Size         imageSize;
  cv::Size         rotatedSize;
  CvCapture       *capture;
  double           fps;
  QQueue<int>      frameTimes;
  int              deviceIndex;
  int              rotation;
  int              FrameCounter;
  //
  cv::VideoWriter  AVI_Writer;
  bool             AVI_Writing;
  std::string      AVI_Directory;
  std::string      AVI_Name;
  std::string      CaptureStatus;
  //
  cv::Size         text_size;
  cv::Mat          rotatedImage;
};

#endif
