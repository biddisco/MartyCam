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
  enum FrameSize { Size640, Size320 };
  //
   CaptureThread(ImageBuffer* buffer);
  ~CaptureThread() ;

  void run();
  bool startCapture(int framerate, FrameSize size);
  void stopCapture();
  double getFPS() { return fps; }
  bool isCapturing() { return captureActive; }

  void setWriteAVI(bool write) { this->AVI_Writing = write; }
  bool getWriteAVI() { return this->AVI_Writing; }
  void setWriteAVIDir(const char *dir);
  void setWriteAVIName(const char *name);
  void saveAVI(IplImage *image);
  void closeAVI();
  void setAbort(bool a) { this->abort = a; }

signals:
  void RecordingState(bool);

private:
  void updateFPS(int time);
  bool            abort; 
  QMutex          captureLock;
  QWaitCondition  captureWait;
  ImageBuffer    *imageBuffer;
  bool            captureActive;
  CvSize          imageSize;
  CvCapture      *capture;
  double          fps;
  QQueue<int>     frameTimes;
  //
  CvVideoWriter  *AVI_Writer;
  bool            AVI_Writing;
  std::string     AVI_Directory;
  std::string     AVI_Name;
};

#endif
