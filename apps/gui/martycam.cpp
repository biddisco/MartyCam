#include "martycam.h"
#include "martycam/widgets/renderwidget.h"
#include "settings.h"
//
#include <QDockWidget>
#include <QSettings>
#include <QSizePolicy>
#include <QToolBar>
#include <QVBoxLayout>
//
#include <QtWidgets/QMessageBox>
#include <hpx/future.hpp>
#include <hpx/include/async.hpp>

#include <fmt/format.h>
#include "martycam/core/GraphUpdateFilter.h"
#include "debug/logging.hpp"
#include "martycam/widgets/CameraSelectorWidget.h"
//
// ----------------------------------------------------------------------------
static auto marty_log = martycam::log::create("MartyCam");

//----------------------------------------------------------------------------
int const MartyCam::IMAGE_BUFF_CAPACITY = 5;

MartyCam::MartyCam(hpx::execution::parallel_executor const& defaultExec,
    hpx::execution::parallel_executor const& blockingExec)
  : captureThread(nullptr)
  , processingThread(nullptr)
  , defaultExecutor(defaultExec)
  , blockingExecutor(blockingExec)
  , QMainWindow(nullptr)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->ui.setupUi(this);
  //
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
  //
  this->renderWidget = std::make_shared<RenderWidget>(this);
  this->renderWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  ui.central_layout->insertWidget(0, this->renderWidget.get(), 1);
  ui.motionGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  ui.central_layout->setStretch(0, 1);
  ui.central_layout->setStretch(1, 0);
  //
  this->imageBuffer = ImageBuffer(new ConcurrentCircularBuffer<cv::Mat>(IMAGE_BUFF_CAPACITY));

  //
  connect(this->renderWidget.get(), SIGNAL(mouseDblClicked(QPoint const&)), this,
      SLOT(onMouseDoubleClickEvent(QPoint const&)));
  //

  //
  // create a dock widget to hold the settings
  //
  settingsDock = std::make_shared<QDockWidget>("Choose Processing Type", this);
  settingsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  settingsDock->setFeatures(QDockWidget::DockWidgetMovable);
  settingsDock->setObjectName("SettingsDock");
  //
  // create the settings widget itself
  //
  this->settingsWidget = std::make_shared<SettingsWidget>(this);
  this->settingsWidget->setRenderWidget(this->renderWidget.get());
  settingsDock->setWidget(this->settingsWidget.get());
  settingsDock->setMinimumWidth(300);
  addDockWidget(Qt::RightDockWidgetArea, settingsDock.get());
  //
  connect(this->settingsWidget.get(), SIGNAL(cameraConfigChanged(QString, int, int, int, int)),
      this, SLOT(onCameraConfigChanged(QString, int, int, int, int)));
  connect(
      this->settingsWidget.get(), SIGNAL(rotationChanged(int)), this, SLOT(onRotationChanged(int)));
  connect(this->ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  //
  // not all controls could fit on the settings dock, trackval + graphs are
  // separate.
  //
  connect(this->ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int)));
  //
  //
  this->loadSettings();
  this->settingsWidget->loadSettings();
  //
#if 0   
  std::string camerastring;
  this->cameraIndex = this->settingsWidget->getCameraIndex(camerastring);
  //
  bool captureLaunched = false;
  while (!captureLaunched && this->cameraIndex >= 0)
  {
    bool requestedSizeCorrect = false;
    int numeResolutionsToTry = this->settingsWidget->getNumOfResolutions();
    while (!requestedSizeCorrect && numeResolutionsToTry > 0)
    {
      // Loop over different resolutions to make sure the one supported by
      // webcam is chosen
      cv::Size res = this->settingsWidget->getSelectedResolution();
      this->createCaptureThread(res, this->cameraIndex, camerastring, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), blockingExecutor);
      requestedSizeCorrect = captureThread->isRequestedSizeCorrect();
      if (!requestedSizeCorrect)
      {
        this->deleteCaptureThread();
        this->settingsWidget->switchToNextResolution();
      }
      else { captureLaunched = true; }
      numeResolutionsToTry -= 1;
    }
    if (this->captureThread->getImageSize().width == 0)
    {
      this->cameraIndex -= 1;
      camerastring = "";
      this->deleteCaptureThread();
    }
  }
  if (this->captureThread->getImageSize().width > 0)
  {
    this->renderWidget->setCVSize(this->captureThread->getImageSize());
    this->createProcessingThread(
        nullptr, defaultExecutor, this->settingsWidget->getCurentProcessingType());
    this->initChart();
  }
  else
  {
    // abort if no camera devices connected
  }
#endif
  //
  restoreState(settings.value("mainWindowState").toByteArray());
}

//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->saveSettings();
  this->settingsWidget->saveSettings();
  this->deleteCaptureThread();
  this->deleteProcessingThread();
}

//----------------------------------------------------------------------------
void MartyCam::createCaptureThread(cv::Size size, std::string const& cameraUTL, int fps, int fourcc,
    hpx::execution::parallel_executor exec)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->captureThread = std::make_shared<CaptureThread>(imageBuffer, size,
      this->settingsWidget->getSelectedRotation(), cameraUTL, this->blockingExecutor, fps, fourcc);
  this->captureThread->startCapture();
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);

  // not needed for now, but leave it here for future use
  connect(this->captureThread.get(), SIGNAL(RecordingState(bool)), this,
      SLOT(onRecordingStateChanged(bool)));
}

//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->captureThread->stopCapture();
  this->imageBuffer->shutdown();
  this->settingsWidget->unsetCaptureThread();
  this->captureThread = nullptr;
}

//----------------------------------------------------------------------------
void MartyCam::createProcessingThread(ProcessingThread* oldThread,
    hpx::execution::parallel_executor exec, ProcessingType processingType)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  MotionFilterParams mfp = this->settingsWidget->getMotionFilterParams();
  FaceRecogFilterParams frfp = this->settingsWidget->getFaceRecogFilterParams();
  this->processingThread =
      std::make_shared<ProcessingThread>(imageBuffer, exec, processingType, mfp, frfp);
  if (oldThread) this->processingThread->CopySettings(oldThread);
  this->processingThread->setRootFilter(renderWidget.get());
  this->processingThread->startProcessing();

  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
  //
  connect(this->processingThread.get(), SIGNAL(NewData()), this, SLOT(updateGUI()),
      Qt::QueuedConnection);
}

//----------------------------------------------------------------------------
void MartyCam::deleteProcessingThread()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->processingThread->stopProcessing();
  this->settingsWidget->unsetProcessingThread();
  this->processingThread = nullptr;
}

//----------------------------------------------------------------------------
void MartyCam::onRotationChanged(int rotation)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->captureThread->setRotation(rotation);
  //
  if (rotation == 0 || rotation == 3)
  {
    this->renderWidget->setCVSize(this->captureThread->getImageSize());
  }
  if (rotation == 1 || rotation == 2)
  {
    this->renderWidget->setCVSize(this->captureThread->getRotatedSize());
  }
  this->clearGraphs();
}

//----------------------------------------------------------------------------
void MartyCam::updateGUI()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  if (!this->processingThread) return;

  std::string const status = fmt::format(
      "Grab FPS: {:05.2f} | Req FPS: {:03d} | Actual FPS: {:05.2f} | Frame Counter: {:05d} "
      "| Image Buffer Occupancy: {:04.1f}% "
      "| Capture FPS: {:05.2f} "
      "| Processing Time: {:03d}ms",
      this->captureThread->getGrabFps(), this->captureThread->getRequestedFps(),
      this->captureThread->getActualFps(), captureThread->GetFrameCounter(),
      100.0f * this->imageBuffer->size() / IMAGE_BUFF_CAPACITY,
      this->captureThread->getCaptureFps(), processingThread->getProcessingTime());
  statusBar()->showMessage(QString::fromStdString(status));

  //
  // as the data scrolls, we move the x-axis start and end (size)
  //
  this->processingThread->graphFilter->updateChart(this->ui.chart);
  this->ui.detect_value->setText(QString::fromStdString(
      fmt::format("{:04.2f}", this->processingThread->motionFilter->motionEstimate)));

  //
  // if an event was triggered, start recording
  //
  if (this->processingThread->motionFilter->eventLevel >= 100 &&
      this->ui.RecordingEnabled->isChecked())
  {
    this->settingsWidget->RecordMotionAVI(true);
  }

  QDateTime now = QDateTime::currentDateTime();
  QDateTime start = this->settingsWidget->TimeLapseStart();
  QDateTime stop = this->settingsWidget->TimeLapseEnd();
  int secs = static_cast<int>(this->settingsWidget->TimeLapseInterval() / 1000);
  QDateTime next = this->lastTimeLapse.addSecs(secs);

  if (!this->captureThread->TimeLapseAVI_Writing)
  {
    if (this->settingsWidget->TimeLapseEnabled())
    {
      if (now > start && now < stop)
      {
        this->settingsWidget->SetupAVIStrings();
        this->captureThread->startTimeLapse(this->settingsWidget->TimeLapseFPS());
        this->captureThread->updateTimeLapse();
        this->lastTimeLapse = now;
      }
    }
  }
  else if ((now > next && now < stop) && this->settingsWidget->TimeLapseEnabled())
  {
    this->captureThread->updateTimeLapse();
    this->lastTimeLapse = now;
  }
  else if (now > stop || !this->settingsWidget->TimeLapseEnabled())
  {
    this->captureThread->stopTimeLapse();
  }
}

//----------------------------------------------------------------------------
void MartyCam::clearGraphs()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->processingThread->graphFilter->clearChart();
}

//----------------------------------------------------------------------------
void MartyCam::onUserTrackChanged(int value)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  double percent = value;
  this->processingThread->motionFilter->triggerLevel = percent;

  double logval = percent > 1 ? (100.0 / 4.0) * log10(percent) : 0;
  this->ui.set_value->setText(QString::fromStdString(fmt::format("{:04.2f}", logval)));
  // threshold back from log to original
  double trigger = pow(10, value * (4.0 / 100.0)) / 100.0;
  this->ui.set_value->setText(QString::fromStdString(fmt::format("{:04.2f}", trigger)));
}

//----------------------------------------------------------------------------
void MartyCam::onRecordingStateChanged(bool state)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  if (state)
  {
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: green; }");
    this->EventRecordCounter++;
    QString evc = QString("Events : %1").arg(this->EventRecordCounter, 3);
    this->ui.eventCounter->setText(evc);
    //
    // streamRecorder->startRecording("/home/biddisco/wildlife/test.mp4");
  }
  else
  {
    // streamRecorder->stopRecording();
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: window; }");
  }
}

//----------------------------------------------------------------------------
void MartyCam::resetChart()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->ui.chart->clearCurves();
}

//----------------------------------------------------------------------------
void MartyCam::initChart()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->processingThread->graphFilter->initChart(this->ui.chart);
}

//----------------------------------------------------------------------------
void MartyCam::saveSettings()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.setValue("mainWindowGeometry", saveGeometry());
  settings.setValue("mainWindowState", saveState());
  //
  settings.beginGroup("Trigger");
  settings.setValue("trackval", this->ui.user_trackval->value());
  settings.endGroup();
}

//----------------------------------------------------------------------------
void MartyCam::loadSettings()
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  MARTY_LOG_DEBUG(marty_log, "{:<20} {}", "Loading settings", settingsFileName.toStdString());
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginGroup("Trigger");
  {
    QSignalBlocker const blocker(this->ui.user_trackval);
    this->ui.user_trackval->setValue(settings.value("trackval", 50).toInt());
  }
  settings.endGroup();
}

//----------------------------------------------------------------------------
void MartyCam::onMouseDoubleClickEvent(QPoint const&)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  this->renderWidget->showFullScreen();
}

//----------------------------------------------------------------------------
void MartyCam::showEvent(QShowEvent* event)
{
  // Always call the base class implementation first to ensure normal Qt handling
  QMainWindow::showEvent(event);

  // Check if this is the absolute first time the window is rendered
  if (!m_isFirstShow && this->settingsWidget != nullptr)
  {
    settingsWidget->createCameraSelector();
    // Flip the flag so this block never runs on subsequent shows
    m_isFirstShow = true;
  }
}

//----------------------------------------------------------------------------
void MartyCam::onCameraConfigChanged(QString cameraPath, int width, int height, int fps, int fourcc)
{
  MARTY_LOG_SCOPE(marty_log, "{} {}", (void*) (this), __func__);
  if (this->captureThread)
  {
    this->deleteCaptureThread();
    this->deleteProcessingThread();
  }
  //
  MARTY_LOG_INFO(marty_log, "{:<20} Resetting image buffer", "CameraConfigChanged");
  this->imageBuffer->reset();
  //
  MARTY_LOG_INFO(marty_log, "{:<20} Creating capture thread", "CameraConfigChanged");
  this->createCaptureThread(
      cv::Size(width, height), cameraPath.toStdString(), fps, fourcc, blockingExecutor);
  MARTY_LOG_INFO(marty_log, "{:<20} Updating renderwidget size", "CameraConfigChanged");
  this->renderWidget->setCVSize(this->captureThread->getImageSize());
  MARTY_LOG_INFO(marty_log, "{:<20} Creating processing thread", "CameraConfigChanged");
  this->createProcessingThread(
      nullptr, defaultExecutor, this->settingsWidget->getCurentProcessingType());
  //
  auto cleanup = [this]() {
    MARTY_LOG_INFO(marty_log, "{:<20} Clearing graphs", "CameraConfigChanged");
    this->clearGraphs();
    MARTY_LOG_INFO(marty_log, "{:<20} Initializing chart", "CameraConfigChanged");
    this->resetChart();
    this->initChart();
  };
  QTimer::singleShot(10, this, cleanup);
}
