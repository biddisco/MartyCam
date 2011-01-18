#include <iostream>
#include "capturethread.h"
#include "imagebuffer.h"
#include <QDebug>
#include <QTime>
//----------------------------------------------------------------------------
CaptureThread::CaptureThread(ImageBuffer* buffer, CvSize &size, int device) : QThread()
{
  this->abort             = false;
  this->captureActive     = false;
  this->fps               = 0.0 ;
  this->deviceIndex       = -1;
  this->AVI_Writing       = false;
  this->AVI_Writer        = NULL; 
  this->capture           = NULL;
  this->imageBuffer       = buffer;
  this->deviceIndex       = device;
  this->rotation          = 0;
  this->rotatedImage      = NULL;
  // initialize font and precompute text size
  cvInitFont(&this->font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  cvGetTextSize( timestring.toAscii(), &this->font, &this->text_size, NULL);
  // start capture device driver
  capture = cvCaptureFromCAM(CV_CAP_DSHOW + this->deviceIndex );
  if (size.width>0 && size.height>0) {
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH,  size.width);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, size.height);
  }
  int w = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
  int h = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
  this->imageSize         = cvSize(w,h);
  this->rotatedSize       = cvSize(h,w);
}
//----------------------------------------------------------------------------
CaptureThread::~CaptureThread() 
{  
  this->closeAVI();
  // free memory
  if (this->rotatedImage) {
    cvReleaseData(this->rotatedImage);
  }
  // Release our stream capture object
  cvReleaseCapture(&capture);
}
//----------------------------------------------------------------------------
void CaptureThread::run() {  
  QTime time;
  time.start();
  while (!this->abort) {
    if (!captureActive) {
      captureLock.lock();
      fps = 0;
      frameTimes.clear();
      captureWait.wait(&captureLock);
      time.restart();
      updateFPS(time.elapsed());
      captureLock.unlock();
    }
    // get latest frame from webcam
    IplImage *frame = cvQueryFrame(capture);
    updateFPS(time.elapsed());
    // rotate image if necessary
    IplImage *workingImage = this->rotateImage(frame, this->rotatedImage);
    // add to queue if space is available, 
    if (!imageBuffer->isFull()) {
      imageBuffer->addFrame(workingImage);
    }
    // always write the frame out if saving movie
    if (this->AVI_Writing) {
      // add date time stamp if enabled
      this->captionImage(workingImage);
      this->saveAVI(workingImage);
    }
  }
}
//----------------------------------------------------------------------------
void CaptureThread::updateFPS(int time) {
  frameTimes.enqueue(time);
  if (frameTimes.size() > 30) {
    frameTimes.dequeue();
  }
  if (frameTimes.size() > 1) {
    fps = frameTimes.size()/((double)time-frameTimes.head())*1000.0;
    fps = (static_cast<int>(fps*10))/10.0;
  }
  else {
    fps = 0;
  }
}
//----------------------------------------------------------------------------
bool CaptureThread::startCapture(int framerate) {
  if (!captureActive) {
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH, this->imageSize.width);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, this->imageSize.height);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FPS, framerate);
    std::ostringstream output;
    output << "CV_CAP_PROP_FRAME_WIDTH\t"   << cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH) << std::endl;
    output << "CV_CAP_PROP_FRAME_HEIGHT\t"  << cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT) << std::endl;
    output << "CV_CAP_PROP_FPS\t"           << cvGetCaptureProperty(capture, CV_CAP_PROP_FPS) << std::endl;
    output << "CV_CAP_PROP_FOURCC\t"        << cvGetCaptureProperty(capture, CV_CAP_PROP_FOURCC) << std::endl;
    output << "CV_CAP_PROP_BRIGHTNESS\t"    << cvGetCaptureProperty(capture, CV_CAP_PROP_BRIGHTNESS) << std::endl;
    output << "CV_CAP_PROP_CONTRAST\t"      << cvGetCaptureProperty(capture, CV_CAP_PROP_CONTRAST) << std::endl;
    output << "CV_CAP_PROP_SATURATION\t"    << cvGetCaptureProperty(capture, CV_CAP_PROP_SATURATION) << std::endl;
    output << "CV_CAP_PROP_HUE\t"           << cvGetCaptureProperty(capture, CV_CAP_PROP_HUE) << std::endl;
    captureActive = true;
    captureWait.wakeAll();
    this->CaptureStatus += output.str();
    return true;
  }
  return false;
}
//----------------------------------------------------------------------------
void CaptureThread::stopCapture() {
  captureActive = false;
}
//----------------------------------------------------------------------------
void CaptureThread::saveAVI(IplImage *image) 
{
  //CV_FOURCC('M', 'J', 'P', 'G'),
  //CV_FOURCC('M', 'P', '4', '2') = MPEG-4.2 codec
  //CV_FOURCC('D', 'I', 'V', '3') = MPEG-4.3 codec
  //CV_FOURCC('D', 'I', 'V', 'X') = MPEG-4 codec
  if (!this->AVI_Writer) {
    std::string path = this->AVI_Directory + "/" + this->AVI_Name + std::string(".avi");
    this->AVI_Writer = cvCreateVideoWriter(
      path.c_str(),
      0,  
      this->getFPS(),
      cvSize(image->width, image->height)
    );
    emit(RecordingState(true));
  }
  cvWriteFrame(this->AVI_Writer, image);
}
//----------------------------------------------------------------------------
void CaptureThread::closeAVI() 
{
  this->AVI_Writing = false;
  if (this->AVI_Writer) {
    cvReleaseVideoWriter(&this->AVI_Writer);
    emit(RecordingState(false));
  }
  this->AVI_Writer = NULL;
}
//----------------------------------------------------------------------------
void CaptureThread::setWriteAVIDir(const char *dir)
{
  this->AVI_Directory = dir;
}
//----------------------------------------------------------------------------
void CaptureThread::setWriteAVIName(const char *name)
{
  this->AVI_Name = name;
}
//----------------------------------------------------------------------------
void CaptureThread::setRotation(int value) { 
  this->rotation = value; 
  if (this->rotatedImage) {
    cvReleaseImage(&this->rotatedImage);
  }
  if (this->rotation==1 || this->rotation==2) {
    CvSize workingSize = cvSize(this->imageSize.height, this->imageSize.width);
    this->rotatedImage = cvCreateImage( workingSize, IPL_DEPTH_8U, 3);
  }
}
//----------------------------------------------------------------------------
IplImage *CaptureThread::rotateImage(IplImage *sourceImage, IplImage *rotatedImage)
{
  switch (this->rotation) {
    case 0:
      break;
    case 1:
      cvFlip(sourceImage, 0, 1);
      cvTranspose(sourceImage, rotatedImage );
      return rotatedImage;
    case 2:
      cvTranspose(sourceImage, rotatedImage );
      cvFlip(rotatedImage, 0, 1);
      return rotatedImage;
    case 3:
      cvFlip(sourceImage, 0, -1);
      break;
  }
  return sourceImage;
}
//----------------------------------------------------------------------------
void CaptureThread::captionImage(IplImage *sourceImage)
{
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  cvPutText(sourceImage, timestring.toAscii(), 
    cvPoint(sourceImage->width-text_size.width-4, text_size.height+4), 
    &this->font, cvScalar(255, 255, 255, 0));
}
//----------------------------------------------------------------------------
