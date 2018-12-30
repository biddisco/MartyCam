#include "martycam.h"
#include "renderwidget.h"
#include "settings.h"
//
#include <QToolBar>
#include <QDockWidget>
#include <QSettings>
//
#include <hpx/lcos/future.hpp>
#include <hpx/include/async.hpp>
#include <QtWidgets/QMessageBox>

#include "GraphUpdateFilter.h"
//
//----------------------------------------------------------------------------
const int MartyCam::IMAGE_BUFF_CAPACITY = 5;

MartyCam::MartyCam(const hpx::threads::executors::pool_executor& defaultExec,
                   const hpx::threads::executors::pool_executor& blockingExec)
        : captureThread(nullptr), processingThread(nullptr),
          defaultExecutor(defaultExec), blockingExecutor(blockingExec),
          QMainWindow(nullptr)
{
  this->ui.setupUi(this);
  //
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
  //
  this->renderWidget = std::make_shared<RenderWidget>(this);
  this->ui.gridLayout->addWidget(this->renderWidget.get(), 0, Qt::AlignHCenter || Qt::AlignTop);

  this->imageBuffer = ImageBuffer(new ConcurrentCircularBuffer<cv::Mat>(IMAGE_BUFF_CAPACITY));

  //
  connect(this->renderWidget.get(), SIGNAL(mouseDblClicked(const QPoint&)), this,
    SLOT (onMouseDoubleClickEvent(const QPoint&)));
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
  connect(this->settingsWidget.get(), SIGNAL(resolutionSelected(cv::Size)), this, SLOT(onResolutionSelected(cv::Size)));
  connect(this->settingsWidget.get(), SIGNAL(rotationChanged(int)), this, SLOT(onRotationChanged(int)));
  connect(this->settingsWidget.get(), SIGNAL(CameraIndexChanged(int,QString)), this, SLOT(onCameraIndexChanged(int,QString)));
  connect(this->ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  //
  // not all controls could fit on the settings dock, trackval + graphs are separate.
  //
  connect(this->ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int)));
  //
  //
  this->loadSettings();
  this->settingsWidget->loadSettings();
  //
  std::string camerastring;
  this->cameraIndex = this->settingsWidget->getCameraIndex(camerastring);
  //
  bool captureLaunched = false;
  while (!captureLaunched && this->cameraIndex >= 0) {
    bool requestedSizeCorrect = false;
    int numeResolutionsToTry = this->settingsWidget->getNumOfResolutions();
    while (!requestedSizeCorrect  && numeResolutionsToTry > 0) {
      // Loop over different resolutions to make sure the one supported by webcam is chosen
      cv::Size res = this->settingsWidget->getSelectedResolution();
      this->createCaptureThread(res, this->cameraIndex, camerastring, blockingExecutor);
      requestedSizeCorrect = captureThread->isRequestedSizeCorrect();
      if(!requestedSizeCorrect) {
        this->deleteCaptureThread();
        this->settingsWidget->switchToNextResolution();
      } else {
        captureLaunched = true;
      }
      numeResolutionsToTry-=1;
    }
    if (this->captureThread->getImageSize().width == 0) {
      this->cameraIndex -= 1;
      camerastring = "";
      this->deleteCaptureThread();
    }
  }
  if (this->captureThread->getImageSize().width > 0) {
    this->renderWidget->setCVSize(this->captureThread->getImageSize());
    this->createProcessingThread(nullptr, defaultExecutor, this->settingsWidget->getCurentProcessingType());
    this->initChart();
  }
  else {
    //abort if no camera devices connected
  }
  //
  restoreState(settings.value("mainWindowState").toByteArray());
}
//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*) {
  this->saveSettings();
  this->settingsWidget->saveSettings();
  this->deleteCaptureThread();
  this->deleteProcessingThread();
}
//----------------------------------------------------------------------------
void MartyCam::createCaptureThread(cv::Size &size, int camera, const std::string &cameraname,
                                   hpx::threads::executors::pool_executor exec)
{
  this->captureThread =
          std::make_shared<CaptureThread>(imageBuffer, size, this->settingsWidget->getSelectedRotation(),
                                            camera, cameraname, this->blockingExecutor,
                                            this->settingsWidget->getRequestedFps());
  this->captureThread->startCapture();
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
}
//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
  this->captureThread->stopCapture();
  this->imageBuffer->clear();
  this->settingsWidget->unsetCaptureThread();
  this->captureThread = nullptr;

  this->imageBuffer->send(cv::Mat());
}
//----------------------------------------------------------------------------
void MartyCam::createProcessingThread(ProcessingThread *oldThread,
                                      hpx::threads::executors::pool_executor exec,
                                      ProcessingType processingType)
{
  MotionFilterParams mfp = this->settingsWidget->getMotionFilterParams();
  FaceRecogFilterParams frfp = this->settingsWidget->getFaceRecogFilterParams();
    this->processingThread = std::make_shared<ProcessingThread>(imageBuffer, exec, processingType, mfp, frfp);
  if (oldThread) this->processingThread->CopySettings(oldThread);
  this->processingThread->setRootFilter(renderWidget.get());
  this->processingThread->startProcessing();

  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
  //
  connect(this->processingThread.get(), SIGNAL(NewData()), this, SLOT(updateGUI()),Qt::QueuedConnection);
}
//----------------------------------------------------------------------------
void MartyCam::deleteProcessingThread()
{
  this->processingThread->stopProcessing();
  this->settingsWidget->unsetProcessingThread();
  this->processingThread = nullptr;
}
//----------------------------------------------------------------------------
void MartyCam::onCameraIndexChanged(int index, QString URL)
{
  if (this->cameraIndex==index) {
    return;
  }
  //
  this->cameraIndex = index;
  this->captureThread->connectCamera(index, URL.toStdString());
  this->imageBuffer->clear();
  this->clearGraphs();
}
//----------------------------------------------------------------------------
void MartyCam::onResolutionSelected(cv::Size newSize)
{
  if(this->captureThread->setResolution(newSize))
    // Update GUI renderwidget size
    this->renderWidget->setCVSize(newSize);
  else{
    QMessageBox::warning(
            this,
            tr("Unsupported Resolution."),
            tr(qPrintable(QString("Requested Resolution %1 x %2 is unsupported by your webcam. \n")
                                  .arg(newSize.width).arg(newSize.height)))
            + QString("Keeping the current resolution."));
    this->settingsWidget->switchToPreviousResolution();
  }
  this->clearGraphs();
}
//----------------------------------------------------------------------------
void MartyCam::onRotationChanged(int rotation)
{
  this->captureThread->setRotation(rotation);
  //
  if (rotation==0 || rotation==3) {
    this->renderWidget->setCVSize(this->captureThread->getImageSize());
  }
  if (rotation==1 || rotation==2) {
    this->renderWidget->setCVSize(this->captureThread->getRotatedSize());
  }
  this->clearGraphs();
}
//----------------------------------------------------------------------------
void MartyCam::updateGUI() {
  //
  if (!this->processingThread) return;

  statusBar()->showMessage(QString("FPS : %1 | Frame Counter : %2 "
                                   "| Image Buffer Occupancy : %3\% "
                                   "| Sleep in CaptureThread: %4ms "
                                   "| Capture Time: %5ms "
                                   "| Processing Time: %6ms").
    arg(this->captureThread->getActualFps(), 5, 'f', 2).
    arg(captureThread->GetFrameCounter(), 5).
    arg(100 * (float)this->imageBuffer->size()/IMAGE_BUFF_CAPACITY, 4).
    arg(captureThread->getSleepTime()).
    arg(captureThread->getCaptureTime()).
    arg(processingThread->getProcessingTime()));

  //
  // as the data scrolls, we move the x-axis start and end (size)
  //
  this->processingThread->graphFilter->updateChart(this->ui.chart);
  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->motionFilter->motionEstimate,4 , 'f', 2));

  //
  // if an event was triggered, start recording
  //
  if (this->processingThread->motionFilter->eventLevel>=100 && this->ui.RecordingEnabled->isChecked()) {
    this->settingsWidget->RecordMotionAVI(true);
  }

  QDateTime now = QDateTime::currentDateTime();
  QDateTime start = this->settingsWidget->TimeLapseStart();
  QDateTime stop = this->settingsWidget->TimeLapseEnd();
  QTime interval = this->settingsWidget->TimeLapseInterval();
  int secs = (interval.hour()*60 + interval.minute())*60 + interval.second();
  QDateTime next = this->lastTimeLapse.addSecs(secs);

  if (!this->captureThread->TimeLapseAVI_Writing) {
    if (this->settingsWidget->TimeLapseEnabled()) {
      if (now>start && now<stop) {
        this->settingsWidget->SetupAVIStrings();
        this->captureThread->startTimeLapse(this->settingsWidget->TimeLapseFPS());
        this->captureThread->updateTimeLapse();
        this->lastTimeLapse = now;
      }
    }
  }
  else if ((now>next && now<stop) && this->settingsWidget->TimeLapseEnabled()) {
    this->captureThread->updateTimeLapse();
    this->lastTimeLapse = now;
  }
  else if (now>stop || !this->settingsWidget->TimeLapseEnabled()) {
    this->captureThread->stopTimeLapse();
  }
}
//----------------------------------------------------------------------------
void MartyCam::clearGraphs()
{
  this->processingThread->graphFilter->clearChart();
}
//----------------------------------------------------------------------------
void MartyCam::onUserTrackChanged(int value)
{
  double percent = value;
  this->processingThread->motionFilter->triggerLevel = percent;

  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  this->ui.set_value->setText(QString("%1").arg(logval,4 , 'f', 2));
  // threshold back from log to original
  double trigger = pow(10,value*(4.0/100.0))/100.0;
  this->ui.set_value->setText(QString("%1").arg(trigger, 4 , 'f', 2));
}
//----------------------------------------------------------------------------
void MartyCam::onRecordingStateChanged(bool state)
{
  if (state) {
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: green; }");
    this->EventRecordCounter++;
    QString evc = QString("Events : %1").arg(this->EventRecordCounter, 3);
    this->ui.eventCounter->setText(evc);
  }
  else {
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: window; }");
  }
}
//----------------------------------------------------------------------------
void MartyCam::resetChart()
{
  this->ui.chart->channels().clear();
}
//----------------------------------------------------------------------------
void MartyCam::initChart()
{
  this->processingThread->graphFilter->initChart(this->ui.chart);
}
//----------------------------------------------------------------------------
void MartyCam::saveSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.setValue("mainWindowGeometry", saveGeometry());
  settings.setValue("mainWindowState", saveState());
  //
  settings.beginGroup("Trigger");
  settings.setValue("trackval",this->ui.user_trackval->value());
  settings.endGroup();
}
//----------------------------------------------------------------------------
void MartyCam::loadSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginGroup("Trigger");
  {
    const QSignalBlocker blocker(this->ui.user_trackval);
    this->ui.user_trackval->setValue(settings.value("trackval",50).toInt());
  }
  settings.endGroup();
}
//----------------------------------------------------------------------------
void MartyCam::onMouseDoubleClickEvent(const QPoint&)
{
  this->renderWidget->showFullScreen();
}

