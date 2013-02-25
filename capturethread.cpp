#include <QDebug>
#include <QTime>
//
#include <iostream>
#include <iomanip>
//
#include "capturethread.h"
typedef boost::shared_ptr< ConcurrentCircularBuffer<cv::Mat> > ImageBuffer;

//
// May 2012.
// FOSCAM FI8904W running firmware 11.25.2.44
// capture = cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");
//
//----------------------------------------------------------------------------
CaptureThread::CaptureThread(ImageBuffer buffer, cv::Size &size, int device, QString &URL) : QThread(), frameTimes(50)
{
  this->abort             = false;
  this->captureActive     = false;
  this->fps               = 0.0 ;
  this->FrameCounter      = 0;
  this->deviceIndex       = -1;
  this->MotionAVI_Writing = false;
  this->TimeLapseAVI_Writing = false;
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
  capture.release();
  if (URL=="NO_CAMERA") {
    capture.release();
  }
  else {
    if (URL.size()>0) {
      std::cout << "Attempting IP camera connection " << URL.toAscii().data() << std::endl;
      // using an IP camera, assume default string to access martycam
      // capture = cvCaptureFromFile("http://192.168.1.21/videostream.asf?user=admin&pwd=1234");
      // capture = cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");
      capture.open(URL.toStdString());
    }
    else {
      capture.open(CV_CAP_DSHOW + this->deviceIndex );
    }
    if (!capture.isOpened()) {
      std::cout << "Camera connection failed" << std::endl;
      return;
    }
  }
  if (size.width>0 && size.height>0) {
    capture.set(CV_CAP_PROP_FRAME_WIDTH,  size.width);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, size.height);
  }
  int w = capture.get(CV_CAP_PROP_FRAME_WIDTH);
  int h = capture.get(CV_CAP_PROP_FRAME_HEIGHT);
  this->imageSize         = cv::Size(w,h);
  this->rotatedSize       = cv::Size(h,w);
}
//----------------------------------------------------------------------------
CaptureThread::~CaptureThread() 
{  
  this->closeAVI();
  // free memory
  this->setRotation(0); 
  // Release our stream capture object, not necessary with openCV 2
  capture.release();
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
    cv::Mat frame;
    capture >> frame;

    if (frame.empty()) {
      this->setAbort(true);
      std::cout << "Empty camera image, aborting capture " <<std::endl;
      continue;
    }

    // rotate image if necessary, makes a copy which we can pass to queue
    this->rotateImage(frame, this->rotatedImage);

    // always write the frame out if saving movie or in the process of closing AVI
    if (this->MotionAVI_Writing || this->MotionAVI_Writer.isOpened()) {
      // add date time stamp if enabled
      this->captionImage(this->rotatedImage);
      this->saveAVI(this->rotatedImage);
    }

    this->currentFrame = frame;

    // add to queue if space is available, 
    //
    imageBuffer->send(this->rotatedImage);
    this->FrameCounter++;
      //
//      std::cout << "Calling Emit " << std::endl;
     updateFPS(time.elapsed());
  }
}
//----------------------------------------------------------------------------
void CaptureThread::updateTimeLapse() 
{
  // always write the frame out if saving movie or in the process of closing AVI
  if (this->TimeLapseAVI_Writer.isOpened()) {
    // add date time stamp if enabled
    this->saveTimeLapseAVI(this->currentFrame);
  }
}
//----------------------------------------------------------------------------
void CaptureThread::updateFPS(int time) {
  frameTimes.push_back(time);
  if (frameTimes.size() > 1) {
    fps = frameTimes.size()/((double)time-frameTimes.front())*1000.0;
    fps = (static_cast<int>(fps*10))/10.0;
  }
  else {
    fps = 0;
  }
}
//----------------------------------------------------------------------------
bool CaptureThread::startCapture(int framerate) {
  if (!captureActive) {
    capture.set(CV_CAP_PROP_FRAME_WIDTH, this->imageSize.width);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, this->imageSize.height);
    capture.set(CV_CAP_PROP_FPS, framerate);
    std::ostringstream output;
    output << "CV_CAP_PROP_FRAME_WIDTH\t"   << capture.get(CV_CAP_PROP_FRAME_WIDTH) << std::endl;
    output << "CV_CAP_PROP_FRAME_HEIGHT\t"  << capture.get(CV_CAP_PROP_FRAME_HEIGHT) << std::endl;
    output << "CV_CAP_PROP_FPS\t"           << capture.get(CV_CAP_PROP_FPS) << std::endl;
    output << "CV_CAP_PROP_FOURCC\t"        << capture.get(CV_CAP_PROP_FOURCC) << std::endl;
    output << "CV_CAP_PROP_BRIGHTNESS\t"    << capture.get(CV_CAP_PROP_BRIGHTNESS) << std::endl;
    output << "CV_CAP_PROP_CONTRAST\t"      << capture.get(CV_CAP_PROP_CONTRAST) << std::endl;
    output << "CV_CAP_PROP_SATURATION\t"    << capture.get(CV_CAP_PROP_SATURATION) << std::endl;
    output << "CV_CAP_PROP_HUE\t"           << capture.get(CV_CAP_PROP_HUE) << std::endl;
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
void CaptureThread::saveTimeLapseAVI(const cv::Mat &image) 
{
  if (!this->TimeLapseAVI_Writing) {
    this->TimeLapseAVI_Writer.release();
  }
  else if (this->TimeLapseAVI_Writer.isOpened()) {
    this->TimeLapseAVI_Writer.write(image);
  }
}
//----------------------------------------------------------------------------
void CaptureThread::startTimeLapse(double fps) 
{
  std::string path = this->AVI_Directory + "/" + this->TimeLapseAVI_Name + std::string(".avi");
  if (!this->TimeLapseAVI_Writer.isOpened()) {
    this->TimeLapseAVI_Writer.open(
      path.c_str(),
      CV_FOURCC('X', 'V', 'I', 'D'),
      fps,
      this->getImageSize()
    );
//    emit(RecordingState(true));
  }

  if (!this->TimeLapseAVI_Writer.isOpened()) {
    std::cout << "Failed to open Time Lapse AVI writer : " << path.c_str() << std::endl;
    this->TimeLapseAVI_Writing = false;
  }
  else {
    this->TimeLapseAVI_Writing = true;
  }
}
//----------------------------------------------------------------------------
void CaptureThread::stopTimeLapse() 
{
  this->TimeLapseAVI_Writing = false;
//    emit(RecordingState(true));
}
//----------------------------------------------------------------------------
void CaptureThread::saveAVI(const cv::Mat &image) 
{
  //CV_FOURCC('M', 'J', 'P', 'G'),
  //CV_FOURCC('M', 'P', '4', '2') = MPEG-4.2 codec
  //CV_FOURCC('D', 'I', 'V', '3') = MPEG-4.3 codec
  //CV_FOURCC('D', 'I', 'V', 'X') = MPEG-4 codec
  //CV_FOURCC('X', 'V', 'I', 'D')  
  if (!this->MotionAVI_Writer.isOpened()) {
    std::string path = this->AVI_Directory + "/" + this->MotionAVI_Name + std::string(".avi");
    this->MotionAVI_Writer.open(
      path.c_str(),
      CV_FOURCC('X', 'V', 'I', 'D'),  
      this->getFPS(),
      image.size()
    );
    emit(RecordingState(true));
  }
  if (this->MotionAVI_Writer.isOpened()) {
    this->MotionAVI_Writer.write(image);
    // if CloseAvi has been called, stop writing.
    if (!this->MotionAVI_Writing) {
      this->MotionAVI_Writer.release();
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
  this->MotionAVI_Writing = false;
}
//----------------------------------------------------------------------------
void CaptureThread::setWriteMotionAVIDir(const char *dir)
{
  this->AVI_Directory = dir;
}
//----------------------------------------------------------------------------
void CaptureThread::setWriteMotionAVIName(const char *name)
{
  this->MotionAVI_Name = name;
}
//----------------------------------------------------------------------------
void CaptureThread::setWriteTimeLapseAVIName(const char *name)
{
  this->TimeLapseAVI_Name = name;
}
//----------------------------------------------------------------------------
void CaptureThread::setRotation(int value) { 
  if (this->rotation==value) {
    return;
  }
  this->rotation = value; 
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
      source.copyTo(rotated);
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
      source.copyTo(rotated);
      cv::flip(rotated, rotated, -1);
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
