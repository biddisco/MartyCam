#include "martycam.h"
#include "renderwidget.h"
#include "settings.h"
//
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
#include <QSettings>
//
//----------------------------------------------------------------------------
MartyCam::MartyCam() : QMainWindow(0)
{
  this->ui.setupUi(this);
  this->processingThread = NULL;
  this->captureThread = NULL;
  //
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
  //
  this->renderWidget = new RenderWidget(this);
  // this->centralWidget()->layout()->addWidget
  this->ui.gridLayout->addWidget(this->renderWidget, 0, Qt::AlignHCenter || Qt::AlignTop);
  //
  // Main threads for capture and processing
  //
  this->EventRecordCounter      = 0;
  this->insideMotionEvent       = 0;
  this->imageSize               = cv::Size(0,0);
  this->imageBuffer             = ImageBuffer(new ConcurrentCircularBuffer<cv::Mat>(5));
  this->cameraIndex             = 1;

  //
  // create a dock widget to hold the settings
  //
  settingsDock = new QDockWidget("Settings", this);
  settingsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  settingsDock->setObjectName("SettingsDock");
  //
  // create the settings widget itself
  //
  this->settingsWidget = new SettingsWidget(this);
  this->settingsWidget->setRenderWidget(this->renderWidget);
  settingsDock->setWidget(this->settingsWidget);
  settingsDock->setMinimumWidth(300);
  addDockWidget(Qt::RightDockWidgetArea, settingsDock);
  connect(this->settingsWidget, SIGNAL(resolutionSelected(cv::Size)), this, SLOT(onResolutionSelected(cv::Size)));
  connect(this->settingsWidget, SIGNAL(rotationChanged(int)), this, SLOT(onRotationChanged(int)));
  connect(this->settingsWidget, SIGNAL(CameraIndexChanged(int,QString)), this, SLOT(onCameraIndexChanged(int,QString)));
  //
  // not all controls could fit on the settings dock, trackval + graphs are separate.
  //
  //
  connect(this->ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  //
  this->loadSettings();
  this->settingsWidget->loadSettings();
  //
  std::string camerastring;
  int index = this->settingsWidget->getCameraIndex(camerastring);
  //
  while (this->imageSize.width==0 && this->cameraIndex>=0) {
    cv::Size res = this->settingsWidget->getSelectedResolution();
    this->createCaptureThread(15, res, this->cameraIndex, camerastring);
    this->imageSize = this->captureThread->getImageSize();
    if (this->imageSize.width==0) {
      this->cameraIndex -=1;
      camerastring = "";
      this->deleteCaptureThread();
    }
  }
  if (this->imageSize.width>0) {
    this->renderWidget->setCVSize(this->imageSize);
    this->processingThread = this->createProcessingThread(this->imageSize, NULL);
    this->processingThread->start();
    //
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
void MartyCam::createCaptureThread(int FPS, cv::Size &size, int camera, const std::string &cameraname)
{
  this->captureThread = new CaptureThread(imageBuffer, size, camera, cameraname);
  this->captureThread->setRotation(this->settingsWidget->getSelectedRotation());
  this->captureThread->start(QThread::IdlePriority);
  this->captureThread->startCapture();
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
}
//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
  this->captureThread->setAbort(true);
  this->captureThread->wait();
  this->imageBuffer->clear();
  delete captureThread;
  // we will push an empty image onto the image buffer to ensure that any waiting processing thread is freed
  this->imageBuffer->send(cv::Mat());
}
//----------------------------------------------------------------------------
ProcessingThread *MartyCam::createProcessingThread(cv::Size &size, ProcessingThread *oldThread)
{
  ProcessingThread *temp = new ProcessingThread(imageBuffer, this->imageSize);
  if (oldThread) temp->CopySettings(oldThread);
  temp->setRootFilter(renderWidget);
  this->settingsWidget->setThreads(this->captureThread, temp);
  //
  connect(temp, SIGNAL(NewData()),
    this, SLOT(updateGUI()),Qt::QueuedConnection);
  //
  return temp;
}
//----------------------------------------------------------------------------
void MartyCam::deleteProcessingThread()
{
  this->processingThread->setAbort(true);
  this->processingThread->wait();
  delete processingThread;
  this->processingThread = NULL;
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
  if (newSize==this->imageSize) {
    return;
  }
  this->imageSize = newSize;
  this->captureThread->setResolution(this->imageSize);
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

  statusBar()->showMessage(QString("FPS : %1, Counter : %2, Buffer : %3").
    arg(this->captureThread->getFPS(), 5, 'f', 2).
    arg(captureThread->GetFrameCounter(), 5).
    arg(this->imageBuffer->size(), 5));

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
}
//----------------------------------------------------------------------------
void MartyCam::onUserTrackChanged(int value)
{
  double percent = value;
  this->processingThread->motionFilter->triggerLevel = percent;
}
//----------------------------------------------------------------------------
void MartyCam::resetChart()
{
}
//----------------------------------------------------------------------------
void MartyCam::initChart()
{
}
//----------------------------------------------------------------------------
void MartyCam::saveSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.setValue("mainWindowGeometry", saveGeometry());
  settings.setValue("mainWindowState", saveState());
}
//----------------------------------------------------------------------------
void MartyCam::loadSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
}

