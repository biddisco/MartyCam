#include "capturethread.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
//
#include <wordexp.h>
//
#include <iomanip>
#include <iostream>
//
#include "opencv2/imgproc.hpp"
#include "opencv2/imgproc/imgproc_c.h"
#include "opencv2/videoio/videoio_c.h"

#include <hpx/future.hpp>
#include <hpx/include/async.hpp>
#include <utility>

#include "debug/logging.hpp"
#include "utility_widgets/camera_utils.h"

// ----------------------------------------------------------------------------
static auto cap_log = martycam::log::create("Capture");
//
typedef std::shared_ptr<ConcurrentCircularBuffer<cv::Mat>> ImageBuffer;

// Expand shell-style variables and ~ in a path (e.g. $HOME/wildlife -> /home/user/wildlife)
static std::string expandPath(std::string const& path)
{
  wordexp_t p;
  if (wordexp(path.c_str(), &p, WRDE_NOCMD) == 0)
  {
    std::string result(p.we_wordv[0]);
    wordfree(&p);
    return result;
  }
  return path;
}

//
// May 2012.
// FOSCAM FI8904W running firmware 11.25.2.44
// capture =
// cvCaptureFromFile("http://admin:1234@192.168.1.21/videostream.cgi?req_fps=30&.mjpg");
//

// Image deinterlacing function for DV camera
cv::Mat Deinterlace(cv::Mat& src)
{
  cv::Mat res = src;    // src.clone();
  uchar* linea;
  uchar* lineb;
  uchar* linec;

  for (int i = 1; i < res.size().height - 1; i += 2)
  {
    linea = (uchar*) res.data + ((i - 1) * res.step[0]);
    lineb = (uchar*) res.data + ((i) *res.step[0]);
    linec = (uchar*) res.data + ((i + 1) * res.step[0]);

    for (int j = 0; j < res.size().width * res.channels(); j++)
    {
      lineb[j] = (uchar) ((linea[j] + linec[j]) / 2);
    }
  }

  if (res.size().height > 1 && res.size().height % 2 == 0)
  {
    linea = (uchar*) res.data + ((res.size().height - 2) * res.step[0]);
    lineb = (uchar*) res.data + ((res.size().height - 1) * res.step[0]);
    memcpy(lineb, linea, res.size().width);
  }
  return res;
}

//----------------------------------------------------------------------------
CaptureThread::CaptureThread(ImageBuffer imageBuffer, cv::Size const& size, int rotation,
    std::string const& URL, hpx::execution::parallel_executor exec, int requestedFps)
  : imageBuffer(std::move(imageBuffer))
  , imageSize(size)
  , rotation(-360)
  , CameraURL(URL)
  , executor(std::move(exec))
  , requestedFps(requestedFps)
  , requestedSizeCorrect(false)
  , actualFps(0.0)
  , FrameCounter(0)
  , frameTimes(50)
  , captureTimes(15)
  , abort(false)
  , captureActive(false)
  , deInterlace(false)
  , MotionAVI_Writing(false)
  , capture()
  , rotatedImage()
  , rotatedSize(cv::Size(0, 0))
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  // initialize font and precompute text size
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  this->text_size =
      cv::getTextSize(timestring.toUtf8().constData(), CV_FONT_HERSHEY_PLAIN, 1.0, 1, NULL);
  //
  // Connect to camera and use its default resolution
  this->connectCamera(this->CameraURL);
  //
  this->setRotation(rotation);
}

//----------------------------------------------------------------------------
CaptureThread::~CaptureThread()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->closeAVI();
  // Release our stream capture object, not necessary with openCV 2
  this->capture.release();
}

//----------------------------------------------------------------------------
bool CaptureThread::connectCamera(std::string const& URL)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  bool wasActive = this->stopCapture();
  if (this->capture.isOpened()) { this->capture.release(); }

  capture = camera_utils::createVideoCapture(
      URL, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), this->requestedFps, this->imageSize);

  if (!this->capture.isOpened())
  {
    MARTY_LOG_ERROR(cap_log, "{:<20} Camera connection failed", "CaptureThread");
    return false;
  }

  if (wasActive) { return this->startCapture(); }
  return true;
}

//----------------------------------------------------------------------------
void CaptureThread::setRotation(int value)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (this->rotation == value) { return; }
  bool wasActive = this->stopCapture();
  this->rotation = value;
  if (this->rotation == 1 || this->rotation == 2)
  {
    cv::Size workingSize = cv::Size(this->imageSize.height, this->imageSize.width);
    this->rotatedImage = cv::Mat(workingSize, CV_8UC3);
  }
  if (wasActive) this->startCapture();
}
//----------------------------------------------------------------------------
void CaptureThread::run()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  // Clear the frameTimes circular buffer to ensure actualFps is computed
  // correctly from the first frame
  this->frameTimes.clear();

  QElapsedTimer actualFpsTime;
  actualFpsTime.start();

  QElapsedTimer requestedFpsTime;
  requestedFpsTime.start();

  QElapsedTimer captureWaitTime;
  captureWaitTime.start();

  while (!this->abort)
  {
    if (!captureActive)
    {
      MARTY_LOG_WARN(cap_log,
          "{:<20} CaptureThread::run() still running even though "
          "captureActive=false",
          "CaptureThread");
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));
      continue;
    }

    // Continuously grab from camera to drain backend buffers and keep the
    // stream current; publish to processing only at requested FPS.
    captureWaitTime.restart();
    bool const grabbed = this->capture.grab();
    updateCaptureTime(captureWaitTime.elapsed());
    int elapsedSinceLastOutput_ms = requestedFpsTime.elapsed();

    if (!grabbed)
    {
      this->setAbort(true);
      MARTY_LOG_ERROR(
          cap_log, "{:<20} Failed to grab camera image, aborting this->capture", "CaptureThread");
      continue;
    }

    int requestedFrameTime_ms = (this->requestedFps > 0) ? (1000 / this->requestedFps) : 1000;
    this->sleepTime_ms = requestedFrameTime_ms - elapsedSinceLastOutput_ms;
    if (this->sleepTime_ms < 0) { this->sleepTime_ms = 0; }

    if (elapsedSinceLastOutput_ms < requestedFrameTime_ms) { continue; }

    // Retrieve the most recently grabbed frame for publication.
    cv::Mat frame;
    if (!this->capture.retrieve(frame))
    {
      this->setAbort(true);
      MARTY_LOG_ERROR(cap_log, "{:<20} Failed to retrieve camera image, aborting this->capture",
          "CaptureThread");
      continue;
    }

    if (frame.empty())
    {
      this->setAbort(true);
      MARTY_LOG_ERROR(
          cap_log, "{:<20} Empty camera image, aborting this->capture", "CaptureThread");
      continue;
    }

    if (this->deInterlace)
    {
      // de-interlace image
      frame = Deinterlace(frame);
    }

    // rotate image if necessary, makes a copy which we can pass to queue
    this->rotateImage(frame, this->rotatedImage);

    // always write the frame out if saving movie or in the process of closing AVI
    if (this->MotionAVI_Writing || this->MotionAVI_Writer.isOpened())
    {
      // add date time stamp if enabled
      this->captionImage(this->rotatedImage);
      this->saveAVI(this->rotatedImage);
    }

    this->currentFrame = frame;

    // add to buffer if space is available,
    imageBuffer->send(this->rotatedImage);

    this->FrameCounter++;

    updateActualFps(actualFpsTime.elapsed());

    requestedFpsTime.restart();
  }

  // The run() task is exiting -> wake the threads waiting for that.
  this->stopWait.wakeAll();
}
//----------------------------------------------------------------------------
bool CaptureThread::startCapture()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (!captureActive)
  {
    if (this->imageSize.width > 0)
    {
      this->capture.set(CV_CAP_PROP_FRAME_WIDTH, this->imageSize.width);
      this->capture.set(CV_CAP_PROP_FRAME_HEIGHT, this->imageSize.height);
    }
    else
    {
      this->capture.set(CV_CAP_PROP_FRAME_WIDTH, 2048);
      this->capture.set(CV_CAP_PROP_FRAME_HEIGHT, 2048);
    }
    this->capture.set(CV_CAP_PROP_FPS, this->requestedFps);
    // Minimize kernel frame buffering so capture >> frame always returns the
    // most recent frame rather than a stale buffered one (reduces display lag).
    this->capture.set(cv::CAP_PROP_BUFFERSIZE, 1);
    std::ostringstream output;
    output << "CV_CAP_PROP_FRAME_WIDTH\t" << this->capture.get(CV_CAP_PROP_FRAME_WIDTH)
           << std::endl;
    output << "CV_CAP_PROP_FRAME_HEIGHT\t" << this->capture.get(CV_CAP_PROP_FRAME_HEIGHT)
           << std::endl;
    output << "CV_CAP_PROP_FPS\t" << this->capture.get(CV_CAP_PROP_FPS) << std::endl;
    output << "CV_CAP_PROP_FOURCC\t" << this->capture.get(CV_CAP_PROP_FOURCC) << std::endl;
    output << "CV_CAP_PROP_BRIGHTNESS\t" << this->capture.get(CV_CAP_PROP_BRIGHTNESS) << std::endl;
    output << "CV_CAP_PROP_CONTRAST\t" << this->capture.get(CV_CAP_PROP_CONTRAST) << std::endl;
    output << "CV_CAP_PROP_SATURATION\t" << this->capture.get(CV_CAP_PROP_SATURATION) << std::endl;
    output << "CV_CAP_PROP_HUE\t" << this->capture.get(CV_CAP_PROP_HUE) << std::endl;

    captureActive = true;
    abort = false;

    hpx::async(this->executor, &CaptureThread::run, this);

    this->CaptureStatus += output.str();

    return true;
  }
  return false;
}
//----------------------------------------------------------------------------
bool CaptureThread::stopCapture()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  bool wasActive = this->captureActive;
  if (wasActive)
  {
    this->stopLock.lock();
    captureActive = false;
    abort = true;
    this->stopWait.wait(&this->stopLock);
    this->stopLock.unlock();
  }
  return wasActive;
}
//----------------------------------------------------------------------------
void CaptureThread::updateTimeLapse()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  // always write the frame out if saving movie or in the process of closing AVI
  if (this->TimeLapseAVI_Writer.isOpened())
  {
    // add date time stamp if enabled
    this->saveTimeLapseAVI(this->currentFrame);
  }
}
//----------------------------------------------------------------------------
void CaptureThread::saveTimeLapseAVI(cv::Mat const& image)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (!this->TimeLapseAVI_Writing) { this->TimeLapseAVI_Writer.release(); }
  else if (this->TimeLapseAVI_Writer.isOpened()) { this->TimeLapseAVI_Writer.write(image); }
}
//----------------------------------------------------------------------------
void CaptureThread::startTimeLapse(double fps)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (this->AVI_Directory.empty() || this->TimeLapseAVI_Name.empty())
  {
    std::cout << "Cannot create time-lapse writer: directory or filename is empty" << std::endl;
    this->TimeLapseAVI_Writing = false;
    return;
  }

  QDir dir(QString::fromStdString(this->AVI_Directory));
  if (!dir.exists())
  {
    if (!dir.mkpath("."))
    {
      std::cout << "Cannot create time-lapse writer: failed to create directory "
                << this->AVI_Directory << std::endl;
      this->TimeLapseAVI_Writing = false;
      return;
    }
  }

  std::string path =
      expandPath(this->AVI_Directory) + "/" + this->TimeLapseAVI_Name + std::string(".mp4");
  cv::Size frameSize = this->getImageSize();
  if (frameSize.width <= 0 || frameSize.height <= 0)
  {
    std::cout << "Cannot create time-lapse writer: invalid frame size " << frameSize.width << "x"
              << frameSize.height << std::endl;
    this->TimeLapseAVI_Writing = false;
    return;
  }

  if (!this->TimeLapseAVI_Writer.isOpened())
  {
    this->TimeLapseAVI_Writer.open(path.c_str(), CV_FOURCC('a', 'v', 'c', '1'), fps, frameSize);
    if (!this->TimeLapseAVI_Writer.isOpened())
    {
      this->TimeLapseAVI_Writer.open(path.c_str(), CV_FOURCC('m', 'p', '4', 'v'), fps, frameSize);
    }
    //    emit(RecordingState(true));
  }

  if (!this->TimeLapseAVI_Writer.isOpened())
  {
    std::cout << "Failed to open Time Lapse video writer : " << path.c_str() << std::endl;
    this->TimeLapseAVI_Writing = false;
  }
  else { this->TimeLapseAVI_Writing = true; }
}
//----------------------------------------------------------------------------
void CaptureThread::stopTimeLapse()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->TimeLapseAVI_Writing = false;
  //    emit(RecordingState(true));
}
//----------------------------------------------------------------------------
void CaptureThread::setRequestedFps(int value)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->requestedFps = value;
  this->frameTimes.clear();
}
//----------------------------------------------------------------------------
void CaptureThread::updateActualFps(int time)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  frameTimes.push_back(time);
  if (frameTimes.size() > 1)
  {
    actualFps = frameTimes.size() / ((double) time - frameTimes.front()) * 1000.0;
    actualFps = (static_cast<int>(actualFps * 10)) / 10.0;
  }
  else { actualFps = 0; }
}
//----------------------------------------------------------------------------
void CaptureThread::updateCaptureTime(int time_ms)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  captureTimes.push_back(time_ms);

  captureTime_ms = static_cast<int>(
      std::accumulate(captureTimes.begin(), captureTimes.end(), 0) / captureTimes.size());
}
//----------------------------------------------------------------------------
void CaptureThread::saveAVI(cv::Mat const& image)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  // CV_FOURCC('a', 'v', 'c', '1') = H.264 (best for MP4)
  // CV_FOURCC('m', 'p', '4', 'v') = MPEG-4 Part 2 (fallback)
  // CV_FOURCC('M', 'J', 'P', 'G') = Motion JPEG (AVI only)
  // CV_FOURCC('X', 'V', 'I', 'D') = XviD (legacy)
  if (!this->MotionAVI_Writer.isOpened())
  {
    if (this->AVI_Directory.empty() || this->MotionAVI_Name.empty())
    {
      std::cout << "Cannot create video writer: directory or filename is empty" << std::endl;
      this->MotionAVI_Writing = false;
      return;
    }

    std::string expandedDir = expandPath(this->AVI_Directory);
    QDir dir(QString::fromStdString(expandedDir));
    if (!dir.exists())
    {
      if (!dir.mkpath("."))
      {
        std::cout << "Cannot create video writer: failed to create directory " << expandedDir
                  << std::endl;
        this->MotionAVI_Writing = false;
        return;
      }
    }

    std::string path = expandedDir + "/" + this->MotionAVI_Name + std::string(".mp4");
    double fps = this->getActualFps();
    if (fps <= 0.0) { fps = static_cast<double>(this->requestedFps); }
    if (fps <= 0.0) { fps = 15.0; }

    cv::Size frameSize = image.size();
    if (frameSize.width <= 0 || frameSize.height <= 0)
    {
      std::cout << "Cannot create video writer: invalid frame size " << frameSize.width << "x"
                << frameSize.height << std::endl;
      this->MotionAVI_Writing = false;
      return;
    }

    this->MotionAVI_Writer.open(path.c_str(), CV_FOURCC('a', 'v', 'c', '1'), fps, frameSize);
    if (!this->MotionAVI_Writer.isOpened())
    {
      this->MotionAVI_Writer.open(path.c_str(), CV_FOURCC('m', 'p', '4', 'v'), fps, frameSize);
    }
    // emit(RecordingState(true));
  }
  if (this->MotionAVI_Writer.isOpened())
  {
    this->MotionAVI_Writer.write(image);
    // if CloseAvi has been called, stop writing.
    if (!this->MotionAVI_Writing)
    {
      this->MotionAVI_Writer.release();
      // emit(RecordingState(false));
    }
  }
  else
  {
    std::cout << "Failed to create video writer" << std::endl;
    this->MotionAVI_Writing = false;
    return;
  }
}
//----------------------------------------------------------------------------
void CaptureThread::closeAVI() { this->MotionAVI_Writing = false; }
//----------------------------------------------------------------------------
void CaptureThread::setWriteMotionAVIDir(char const* dir) { this->AVI_Directory = dir; }
//----------------------------------------------------------------------------
void CaptureThread::setWriteMotionAVIName(char const* name) { this->MotionAVI_Name = name; }
//----------------------------------------------------------------------------
void CaptureThread::setWriteTimeLapseAVIName(char const* name) { this->TimeLapseAVI_Name = name; }
//----------------------------------------------------------------------------
void CaptureThread::rotateImage(cv::Mat const& source, cv::Mat& rotated)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  switch (this->rotation)
  {
  case 0: source.copyTo(rotated); break;
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
void CaptureThread::captionImage(cv::Mat& image)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  QString timestring = QDateTime::currentDateTime().toString("HELLO dd/MM/yyyy hh:mm:ss");
  cv::putText(image, timestring.toLatin1().data(),
      cvPoint(image.size().width - text_size.width - 4, text_size.height + 4),
      CV_FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 255, 0), 1);
}
//----------------------------------------------------------------------------
// If the requested resolution is available switches to it and returns true.
// Otherwise leaves the current resolution and returns false
bool CaptureThread::tryResolutionUpdate(cv::Size requestedResolution)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->capture.set(CV_CAP_PROP_FRAME_WIDTH, requestedResolution.width);
  this->capture.set(CV_CAP_PROP_FRAME_HEIGHT, requestedResolution.height);
  auto width = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_WIDTH));
  auto height = static_cast<int>(capture.get(CV_CAP_PROP_FRAME_HEIGHT));

  return width == requestedResolution.width && height == requestedResolution.height;
}
//----------------------------------------------------------------------------
