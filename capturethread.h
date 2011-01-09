#ifndef CAPTURE_THREAD_H
#define CAPTURE_THREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include "opencv/highgui.h"

class ImageBuffer;

class CaptureThread : public QThread {
Q_OBJECT;
public: 
   CaptureThread(ImageBuffer* buffer, CvSize &size, int device);
  ~CaptureThread() ;

  void run();
  void setAbort(bool a) { this->abort = a; }
  //
  bool startCapture(int framerate);
  void stopCapture();
  void setDeviceIndex(int index);
  //
  double getFPS() { return fps; }
  bool isCapturing() { return captureActive; }

  void setWriteAVI(bool write) { this->AVI_Writing = write; }
  bool getWriteAVI() { return this->AVI_Writing; }
  void setWriteAVIDir(const char *dir);
  void setWriteAVIName(const char *name);
  void saveAVI(IplImage *image);
  void closeAVI();
  std::string GetCaptureStatusString() { return this->CaptureStatus; }

signals:
  void RecordingState(bool);

private:
  void updateFPS(int time);
  //
  bool            abort; 
  QMutex          captureLock;
  QWaitCondition  captureWait;
  ImageBuffer    *imageBuffer;
  bool            captureActive;
  CvSize          imageSize;
  CvCapture      *capture;
  double          fps;
  QQueue<int>     frameTimes;
  int             deviceIndex;
  //
  CvVideoWriter  *AVI_Writer;
  bool            AVI_Writing;
  std::string     AVI_Directory;
  std::string     AVI_Name;
  std::string     CaptureStatus;
};

#endif
