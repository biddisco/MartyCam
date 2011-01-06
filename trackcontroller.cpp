#include "trackcontroller.h"
#include "processingthread.h"
#include "imagebuffer.h"

#include <QDebug>
//----------------------------------------------------------------------------
TrackController::TrackController() : frameRate(15), frameSize(CaptureThread::Size320) {
  imageBuffer = new ImageBuffer(1);
  captureThread = new CaptureThread(imageBuffer);
  processingThread = new ProcessingThread(imageBuffer);
  processingThread->start();
  startTracking();
}
//----------------------------------------------------------------------------
TrackController::~TrackController() 
{
  captureThread->stopCapture();
  processingThread->setAbort(true);
  captureThread->setAbort(true);
  captureThread->wait();
  processingThread->wait();
  delete captureThread;
  delete processingThread;
}
//----------------------------------------------------------------------------
void TrackController::startTracking() {
  captureThread->startCapture(frameRate, frameSize);
  // qDebug() << "About to start the capture thread";
  captureThread->start(QThread::IdlePriority);
  // qDebug() << "Started the capture thread";
}
//----------------------------------------------------------------------------
bool TrackController::isTracking() {
  return captureThread->isCapturing();
}
//----------------------------------------------------------------------------
void TrackController::stopTracking() {
  captureThread->stopCapture();
}
//----------------------------------------------------------------------------
void TrackController::setRootFilter(Filter* filter) {
  processingThread->setRootFilter(filter);
}
//----------------------------------------------------------------------------
double TrackController::getFPS() {
  return captureThread->getFPS();
}
//----------------------------------------------------------------------------
