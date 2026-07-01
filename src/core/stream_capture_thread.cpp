#include "martycam/core/stream_capture_thread.h"

#include <QDateTime>
#include <algorithm>
#include <chrono>
#include <filesystem>
//
#include <hpx/threading/thread.hpp>
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
#include "martycam/core/camera_utils.h"

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

//----------------------------------------------------------------------------
StreamCaptureThread::StreamCaptureThread(ImageBuffer imageBuffer, cv::Size const& size,
    int rotation, std::string const& URL, hpx::execution::parallel_executor exec, int requestedFps,
    int requestedFourCC)
  : imageBuffer(std::move(imageBuffer))
  , imageSize(size)
  , rotation(-360)
  , CameraURL(URL)
  , executor(std::move(exec))
  , requestedFps(requestedFps)
  , requestedFourCC(requestedFourCC)
  , requestedSizeCorrect(false)
  , FrameCounter(0)
  , abort(false)
  , finished(false)
  , captureActive(false)
  , deInterlace(false)
  , MotionAVI_Writing(false)
  , aviWriterActive(false)
  , timeLapseFFmpegWriter(std::make_shared<TimeLapseFFmpegWriter>())
  , rotatedImage()
  , rotatedSize(cv::Size(0, 0))
  , streamRecorder(std::make_unique<stream_buffer_recorder>(
        URL, 10.0, executor))    // Create the stream recorder with a 10 second buffer
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
StreamCaptureThread::~StreamCaptureThread()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->closeAVI();
  if (this->streamRecorder) { this->streamRecorder->close(); }
}

//----------------------------------------------------------------------------
bool StreamCaptureThread::connectCamera(std::string const& URL)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  bool wasActive = this->stopCapture();
  if (this->streamRecorder) { this->streamRecorder->close(); }

  this->streamRecorder = std::make_unique<stream_buffer_recorder>(URL, 10.0, executor);
  if (!this->streamRecorder->open(
          this->imageSize.width, this->imageSize.height, fourCCToString(this->requestedFourCC)))
  {
    MARTY_LOG_ERROR(cap_log, "{:<20} Camera connection failed", "StreamCaptureThread");
    return false;
  }

  // Probe the first frame to determine actual resolution
  cv::Mat probeFrame;
  bool gotFrame = false;
  for (int attempt = 0; attempt < 50 && !gotFrame; ++attempt)
  {
    gotFrame = this->streamRecorder->readFrame(probeFrame);
    if (!gotFrame) { hpx::this_thread::yield(); }
  }

  if (gotFrame && !probeFrame.empty())
  {
    cv::Size actualSize = probeFrame.size();
    this->requestedSizeCorrect =
        (actualSize.width == this->imageSize.width && actualSize.height == this->imageSize.height);
    // Update imageSize to reflect actual stream resolution
    this->imageSize = actualSize;
    std::ostringstream output;
    output << "StreamCaptureThread::connectCamera() - " << URL << std::endl;
    output << "FrameWidth\t" << this->imageSize.width << std::endl;
    output << "FrameHeight\t" << this->imageSize.height << std::endl;
    this->CaptureStatus += output.str();
  }
  else
  {
    MARTY_LOG_ERROR(
        cap_log, "{:<20} Failed to read initial frame from stream", "StreamCaptureThread");
    return false;
  }

  if (wasActive) { return this->startCapture(); }
  return true;
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setRotation(int value)
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
void StreamCaptureThread::setWriteMotionAVI(bool write)
{
  if (!this->MotionAVI_Writing && write)
  {
    // Starting a new recording, reset the frame counter
    this->motion_video_FrameCounter = 0;
    MARTY_LOG_INFO(cap_log, "{:<20} Creating video writer for {}.mp4", "StreamCaptureThread",
        this->MotionAVI_Name);
    if (this->AVI_Directory.empty() || this->MotionAVI_Name.empty())
    {
      MARTY_LOG_ERROR(cap_log, "{:<20} Cannot create video writer: directory or filename is empty",
          "StreamCaptureThread");
      // return without setting MotionAVI_Writing
      return;
    }

    std::string expandedDir = expandPath(this->AVI_Directory);
    std::filesystem::path dirPath(expandedDir);
    if (!std::filesystem::exists(dirPath))
    {
      if (!std::filesystem::create_directories(dirPath))
      {
        MARTY_LOG_ERROR(cap_log, "{:<20} Cannot create video writer: failed to create directory {}",
            "StreamCaptureThread", expandedDir);
        // return without setting MotionAVI_Writing
        return;
      }
    }

    this->MotionAVI_Writing = true;
    streamRecorder->startRecording(
        expandedDir + "/" + this->MotionAVI_Name + std::string(".mp4"), this->grabFps.value());
    // emit(RecordingState(true));
  }
  else if (this->MotionAVI_Writing && !write)
  {
    // Stopping the recording
    this->MotionAVI_Writing = false;
    streamRecorder->stopRecording();
    // emit(RecordingState(false));
  }
}

//----------------------------------------------------------------------------
void StreamCaptureThread::run()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  // Start the fps helpers so they own their own timers
  this->actualFps.start();
  this->grabFps.start();
  this->captureFps.start();

  while (!this->abort)
  {
    if (!captureActive)
    {
      MARTY_LOG_WARN(cap_log,
          "{:<20} StreamCaptureThread::run() still running even though "
          "captureActive=false",
          "StreamCaptureThread");
      hpx::this_thread::yield();
      continue;
    }

    // Read frame from the stream buffer recorder
    cv::Mat frame;
    bool const grabbed = this->streamRecorder->readFrame(frame);
    if (!grabbed)
    {
      // No frame available yet - non-blocking, so sleep briefly and retry
      hpx::this_thread::yield();
      continue;
    }
    this->grabFps.tick();
    this->streamRecorder->setStreamFps(this->grabFps.value());

    if (frame.empty())
    {
      this->setAbort(true);
      MARTY_LOG_ERROR(
          cap_log, "{:<20} Empty camera image, aborting this->capture", "StreamCaptureThread");
      continue;
    }

    // Adaptive throttle: drop this frame if accepting it would push the
    // output rate above the requested FPS (allow 1 FPS tolerance to avoid
    // consistently running slightly under target due to jitter).
    if (this->requestedFps > 0 &&
        this->captureFps.would_exceed(this->requestedFps + 1.0 / captureFps.size()))
    {
      continue;
    }

    if (this->deInterlace)
    {
      // de-interlace image
      // Note: deinterlace not implemented for stream capture path
    }

    // rotate image if necessary, makes a copy which we can pass to queue
    this->rotateImage(frame, this->rotatedImage);

    this->currentFrame = frame;

    // add to buffer if space is available,
    imageBuffer->send(this->rotatedImage);

    this->FrameCounter++;

    this->actualFps.tick();
    this->captureFps.tick();
  }

  // The run() task is exiting -> notify the stopCapture/stopProcessing waiter.
  {
    QMutexLocker locker(&stopLock);
    captureActive = false;
    abort = false;
    finished = true;
  }
  this->stopWait.wakeAll();
}

//----------------------------------------------------------------------------
bool StreamCaptureThread::startCapture()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (!captureActive)
  {
    if (!this->streamRecorder)
    {
      MARTY_LOG_ERROR(cap_log, "{:<20} Stream recorder is null", "StreamCaptureThread");
      return false;
    }

    captureActive = true;
    abort = false;
    finished = false;

    hpx::async(this->executor, &StreamCaptureThread::run, this);

    return true;
  }
  return false;
}

//----------------------------------------------------------------------------
bool StreamCaptureThread::stopCapture()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  bool wasActive = this->captureActive;
  if (wasActive)
  {
    stopLock.lock();
    captureActive = false;
    abort = true;
    while (!finished) { stopWait.wait(&stopLock, 100); }
    finished = false;
    stopLock.unlock();
  }
  return wasActive;
}

//----------------------------------------------------------------------------
void StreamCaptureThread::updateTimeLapse()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (!this->TimeLapseAVI_Writing) return;
  // always write the frame out if saving movie or in the process of closing AVI
  this->saveTimeLapseAVI(this->currentFrame);
}

//----------------------------------------------------------------------------
void StreamCaptureThread::saveTimeLapseAVI(cv::Mat image)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  //
  hpx::async(this->executor, [this, image = std::move(image)]() mutable {
    std::lock_guard<std::mutex> asyncLock(this->aviLock);
    if (!this->TimeLapseAVI_Writing || !this->timeLapseFFmpegWriter ||
        !this->timeLapseFFmpegWriter->isOpen())
    {
      return;
    }

    MARTY_LOG_INFO(cap_log, "{:<20} Time-lapse frame writing: {}x{} at {}", "StreamCaptureThread",
        image.cols, image.rows, QTime::currentTime().toString("hh:mm:ss").toStdString());
    if (!this->timeLapseFFmpegWriter->write(image))
    {
      MARTY_LOG_ERROR(cap_log, "{:<20} FFmpeg timelapse write failed: {}", "StreamCaptureThread",
          this->timeLapseFFmpegWriter->lastError());
      this->TimeLapseAVI_Writing = false;
      this->timeLapseFFmpegWriter->close();
      return;
    }
  });
}

//----------------------------------------------------------------------------
void StreamCaptureThread::startTimeLapse(double fps)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }

  if (!this->timeLapseFFmpegWriter->validateSettings())
  {
    MARTY_LOG_ERROR(cap_log,
        "{:<20} Cannot create time-lapse writer: directory or filename is empty",
        "StreamCaptureThread");
    this->TimeLapseAVI_Writing = false;
    return;
  }

  std::string const path = this->timeLapseFFmpegWriter->buildOutputPath();
  cv::Size const frameSize = this->getImageSize();
  if (frameSize.width <= 0 || frameSize.height <= 0)
  {
    MARTY_LOG_ERROR(cap_log, "{:<20} Cannot create time-lapse writer: invalid frame size {}x{}",
        "StreamCaptureThread", frameSize.width, frameSize.height);
    this->TimeLapseAVI_Writing = false;
    return;
  }

  if (path.empty())
  {
    MARTY_LOG_ERROR(cap_log, "{:<20} Cannot create time-lapse writer: output path is empty",
        "StreamCaptureThread");
    this->TimeLapseAVI_Writing = false;
    return;
  }

  MARTY_LOG_INFO(cap_log, "{:<20} Opening time-lapse writer: path='{}', fps={:0.2f}, size={}x{}",
      "StreamCaptureThread", path, fps, frameSize.width, frameSize.height);

  if (!this->timeLapseFFmpegWriter->open(path, frameSize.width, frameSize.height, fps,
          this->timeLapseFFmpegWriter->getBitrateMBps()))
  {
    MARTY_LOG_ERROR(cap_log, "{:<20} Failed to open FFmpeg timelapse writer: {}",
        "StreamCaptureThread", this->timeLapseFFmpegWriter->lastError());
    this->TimeLapseAVI_Writing = false;
    return;
  }

  this->TimeLapseAVI_Writing = true;
  MARTY_LOG_INFO(cap_log, "{:<20} Time-lapse FFmpeg writer started", "StreamCaptureThread");
}

//----------------------------------------------------------------------------
void StreamCaptureThread::stopTimeLapse()
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  MARTY_LOG_INFO(cap_log, "{:<20} Stopping time-lapse writer", "StreamCaptureThread");
  this->TimeLapseAVI_Writing = false;
  if (this->timeLapseFFmpegWriter && this->timeLapseFFmpegWriter->isOpen())
  {
    this->timeLapseFFmpegWriter->close();
    MARTY_LOG_INFO(cap_log, "{:<20} Time-lapse FFmpeg writer released", "StreamCaptureThread");
  }
  //    emit(RecordingState(true));
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setRequestedFps(int value)
{
  MARTY_LOG_SCOPE(cap_log, "{} {}", (void*) (this), __func__);
  this->requestedFps = value;
  this->actualFps.clear();
  this->grabFps.clear();
  this->captureFps.clear();
}

//----------------------------------------------------------------------------
void StreamCaptureThread::closeAVI() { this->MotionAVI_Writing = false; }

//----------------------------------------------------------------------------
void StreamCaptureThread::setWriteMotionAVIDir(char const* dir) { this->AVI_Directory = dir; }

//----------------------------------------------------------------------------
void StreamCaptureThread::setWriteMotionAVIName(char const* name) { this->MotionAVI_Name = name; }

//----------------------------------------------------------------------------
void StreamCaptureThread::setWriteTimeLapseAVIDir(char const* dir)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }
  this->timeLapseFFmpegWriter->setOutputDirectory(dir ? expandPath(dir) : std::string());
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setWriteTimeLapseAVIName(char const* name)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }
  this->timeLapseFFmpegWriter->setOutputFileName(name ? std::string(name) : std::string());
}

//----------------------------------------------------------------------------
void StreamCaptureThread::applyTimeLapseConfig(
    std::string const& directory, double bitrateMBps, std::uint64_t frameIntervalMs, double fps)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }

  this->timeLapseFFmpegWriter->setOutputDirectory(expandPath(directory));
  this->timeLapseFFmpegWriter->setBitrateMBps(bitrateMBps);
  this->timeLapseFFmpegWriter->setFrameInterval(frameIntervalMs);
  this->timeLapseFFmpegWriter->setFPS(fps);
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setTimeLapseBitrateMBps(double value)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }
  this->timeLapseFFmpegWriter->setBitrateMBps(value);
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setTimeLapseFrameIntervalMs(std::uint64_t value)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }
  this->timeLapseFFmpegWriter->setFrameInterval(value);
}

//----------------------------------------------------------------------------
void StreamCaptureThread::setTimeLapseFPS(double value)
{
  if (!this->timeLapseFFmpegWriter)
  {
    this->timeLapseFFmpegWriter = std::make_shared<TimeLapseFFmpegWriter>();
  }
  this->timeLapseFFmpegWriter->setFPS(value);
}

//----------------------------------------------------------------------------
void StreamCaptureThread::rotateImage(cv::Mat const& source, cv::Mat& rotated)
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
void StreamCaptureThread::captionImage(cv::Mat& image)
{
  MARTY_LOG_SCOPE(cap_log, "{}", __func__);
  QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
  std::string text = timestring.toLatin1().data();

  // Scale the text so its width is a fixed fraction of the image width.
  double const targetFraction = 0.20;    // 20 % of image width
  double const minScale = 0.5;
  double const maxScale = 4.0;
  int baseline = 0;

  cv::Size baseSize = cv::getTextSize(text, CV_FONT_HERSHEY_PLAIN, 1.0, 1, &baseline);
  double scale = 1.0;
  if (baseSize.width > 0)
  {
    scale = (image.size().width * targetFraction) / baseSize.width;
    scale = std::max(minScale, std::min(scale, maxScale));
  }

  int thickness = std::max(1, static_cast<int>(scale));
  cv::Size textSize = cv::getTextSize(text, CV_FONT_HERSHEY_PLAIN, scale, thickness, &baseline);

  cv::Point origin(image.size().width - textSize.width - 4, textSize.height + 4);
  cv::putText(
      image, text, origin, CV_FONT_HERSHEY_PLAIN, scale, cv::Scalar(255, 255, 255, 0), thickness);
}
