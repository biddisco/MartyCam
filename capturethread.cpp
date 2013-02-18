#include <QDebug>
#include <QTime>
//
#include <iostream>
//
#include "capturethread.h"
#include "imagebuffer.h"

//
// May 2012.
// FOSCAM FI8904W running firmware 11.25.2.44
// capture = cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");
//
//----------------------------------------------------------------------------
CaptureThread::CaptureThread(ImageBuffer* buffer, cv::Size &size, int device, QString &URL) : QThread()
{
  this->abort             = false;
  this->captureActive     = false;
  this->fps               = 0.0 ;
  this->FrameCounter      = 0;
  this->deviceIndex       = -1;
  this->AVI_Writing       = false;
  this->capture           = NULL;
  this->imageBuffer       = buffer;
  this->deviceIndex       = device;
  this->rotation          = 0;
  this->rotatedImage      = NULL;
  // initialize font and precompute text size
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  this->text_size = cv::getTextSize( timestring.toAscii().data(), CV_FONT_HERSHEY_PLAIN, 1.0, 1, NULL);

  //
  // start capture device driver
  //
  if (URL=="NO_CAMERA") {
    capture = NULL;
  }
  else {
    if (URL.size()>0) {
      std::cout << "Attempting IP camera connection " << URL.toAscii().data() << std::endl;
      // using an IP camera, assume default string to access martycam
      // capture = cvCaptureFromFile("http://192.168.1.21/videostream.asf?user=admin&pwd=1234");
      // capture = cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");
      capture = cvCaptureFromFile(URL.toAscii().data());
    }
    else {
      capture = cvCaptureFromCAM(CV_CAP_DSHOW + this->deviceIndex );
    }
    if (!capture) {
      std::cout << "Camera connection failed" << std::endl;
      return;
    }
  }
  if (size.width>0 && size.height>0) {
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH,  size.width);
    cvSetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT, size.height);
  }
  int w = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_WIDTH);
  int h = cvGetCaptureProperty(capture, CV_CAP_PROP_FRAME_HEIGHT);
  this->imageSize         = cv::Size(w,h);
  this->rotatedSize       = cv::Size(h,w);
}
//----------------------------------------------------------------------------
CaptureThread::~CaptureThread() 
{  
  this->closeAVI();
  // free memory
  this->setRotation(0); 
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
    cv::Mat frame = cvQueryFrame(capture);
    if (frame.empty()) {
      this->setAbort(true);
      continue;
    }
    updateFPS(time.elapsed());
    // rotate image if necessary
    this->rotateImage(frame, this->rotatedImage);
    // add to queue if space is available, 
    if (!imageBuffer->isFull()) {
      imageBuffer->addFrame(this->rotatedImage);
      this->FrameCounter++;
    }
    // always write the frame out if saving movie or in the process of closing AVI
    if (this->AVI_Writing || this->AVI_Writer.isOpened()) {
      // add date time stamp if enabled
      this->captionImage(this->rotatedImage);
      this->saveAVI(this->rotatedImage);
    }
    emit(NewImage());
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
void CaptureThread::saveAVI(const cv::Mat &image) 
{
  //CV_FOURCC('M', 'J', 'P', 'G'),
  //CV_FOURCC('M', 'P', '4', '2') = MPEG-4.2 codec
  //CV_FOURCC('D', 'I', 'V', '3') = MPEG-4.3 codec
  //CV_FOURCC('D', 'I', 'V', 'X') = MPEG-4 codec
  if (!this->AVI_Writer.isOpened()) {
    std::string path = this->AVI_Directory + "/" + this->AVI_Name + std::string(".avi");
    this->AVI_Writer.open(
      path.c_str(),
      0,  
      this->getFPS(),
      image.size()
    );
    emit(RecordingState(true));
  }
  if (this->AVI_Writer.isOpened()) {
    this->AVI_Writer.write(image);
    // if CloseAvi has been called, stop writing.
    if (!this->AVI_Writing) {
      this->AVI_Writer.release();
      emit(RecordingState(false));
    }
  }
  else {
    std::cout << "Failed to create AVI writer" << std::endl;
    return;
  }
}
//----------------------------------------------------------------------------
void CaptureThread::closeAVI() 
{
  this->AVI_Writing = false;
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
  if (this->rotation==value) {
    return;
  }
  this->rotation = value; 
  this->rotatedImage.release();
  if (this->rotation==1 || this->rotation==2) {
    cv::Size workingSize = cv::Size(this->imageSize.height, this->imageSize.width);
    this->rotatedImage = cv::Mat( workingSize, CV_8UC3);
  }
}
//----------------------------------------------------------------------------
void CaptureThread::rotateImage(const cv::Mat &source, cv::Mat &rotated)
{
  switch (this->rotation) {
    case 0:
      rotated = source;
      break;
    case 1:
      cv::flip(source, rotated, 1);
      cv::transpose(rotated, rotated);
      break;
    case 2:
      cv::transpose(source, rotated);
      cv::flip(rotated, rotated, 1);
      break;
    case 3:
      cv::flip(source, rotated, -1);
      break;
  }
}
//----------------------------------------------------------------------------
void CaptureThread::captionImage(cv::Mat &image)
{
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  cv::putText(image, timestring.toAscii().data(), 
    cvPoint(image.size().width - text_size.width - 4, text_size.height+4), 
    CV_FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 255, 0), 1);
}
//----------------------------------------------------------------------------
