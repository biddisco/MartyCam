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
#include "GraphUpdateFilter.h"
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
  this->centralWidget()->layout()->addWidget(this->renderWidget);
  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(this->ui.motionGroup);
  splitter->addWidget(renderWidget);
  this->setCentralWidget(splitter);
  //
  connect(this->renderWidget, SIGNAL(mouseDblClicked(const QPoint&)), this,
    SLOT (onMouseDoubleClickEvent(const QPoint&)));
  //
  // Main threads for capture and processing
  //
  this->EventRecordCounter      = 0;
  this->insideMotionEvent       = 0;
  this->imageSize               = cv::Size(0,0);
  this->imageBuffer             = ImageBuffer(new ConcurrentCircularBuffer<cv::Mat>(100));
  this->cameraIndex             = 1;

  // Layout of - chart
  QWidget * widget = this->ui.motionGroup;
  Chart *chart = this->ui.chart; 

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
  connect(this->settingsWidget, SIGNAL(CameraIndexChanged(int,QString)), this, SLOT(onCameraIndexChanged(int,QString)));
  //
  // not all controls could fit on the settings dock, trackval + graphs are separate.
  //
  connect(this->ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
  //
  connect(this->ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  //
  QString camerastring = "NO_CAMERA";
  while (this->imageSize.width==0 && this->cameraIndex>=0) {
    this->createCaptureThread(15, this->imageSize, this->cameraIndex, camerastring);
    this->imageSize = this->captureThread->getImageSize();
    if (this->imageSize.width==0) {
      this->cameraIndex -=1;
      camerastring = "";
    }
  }
  if (this->imageSize.width>0) {
    this->renderWidget->setFixedSize(this->imageSize.width, this->imageSize.height);
    this->processingThread = this->createProcessingThread(this->imageSize, NULL);
    this->processingThread->start();
  }
  //
  this->loadSettings();
  this->settingsWidget->loadSettings();
  //
  this->initChart();
  // 
  restoreState(settings.value("mainWindowState").toByteArray());
}
//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*) {
  this->saveSettings();
  this->settingsWidget->saveSettings();
  this->deleteProcessingThread();
  this->deleteCaptureThread();
}
//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
  this->imageBuffer->clear();
  this->captureThread->setAbort(true);
  this->captureThread->wait();
  delete captureThread;
  this->imageBuffer->clear();
}
//----------------------------------------------------------------------------
void MartyCam::createCaptureThread(int FPS, cv::Size &size, int camera, QString &cameraname)
{
  this->captureThread = new CaptureThread(imageBuffer, size, camera, cameraname);
  this->captureThread->startCapture(FPS);
  this->captureThread->start(QThread::IdlePriority);
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);

  connect(this->captureThread, SIGNAL(RecordingState(bool)), 
    this, SLOT(onRecordingStateChanged(bool)),Qt::QueuedConnection); 
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
void MartyCam::onCameraIndexChanged(int index, QString URL)
{
  this->resetChart();
  ProcessingThread *temp = this->createProcessingThread(this->imageSize, this->processingThread);
  this->deleteProcessingThread();
  this->deleteCaptureThread();
  this->cameraIndex = index;
  this->processingThread = temp;
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, URL);
  //
  this->initChart();
  this->clearGraphs();
  this->processingThread->start(QThread::IdlePriority);
}
//----------------------------------------------------------------------------
void MartyCam::onResolutionSelected(cv::Size newSize) {
  this->imageSize = newSize;
  ProcessingThread *temp = this->createProcessingThread(this->imageSize, this->processingThread);
  this->deleteProcessingThread();
  this->deleteCaptureThread();
  this->processingThread = temp;
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, QString());
  //
  this->clearGraphs();
  this->processingThread->start(QThread::IdlePriority);
}
//----------------------------------------------------------------------------
void MartyCam::updateGUI() {
  //
  if (!this->processingThread) return;
  //
  //
  // as the data scrolls, we move the x-axis start and end (size)
  //
  this->processingThread->graphFilter->updateChart(this->ui.chart);
  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->motionFilter->motionPercent,4 , 'f', 2));

  //
  // if an event was triggered, start recording
  //
  if (this->processingThread->motionFilter->eventLevel>=100 && this->ui.RecordingEnabled->isChecked()) {
    this->settingsWidget->RecordMotionAVI(true);
  }

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
    if (now>start && now<stop) {
      this->settingsWidget->SetupAVIStrings();
      this->captureThread->startTimeLapse(this->settingsWidget->TimeLapseFPS());
      this->captureThread->updateTimeLapse(); 
      this->lastTimeLapse = now;
    }
  }
  else if (now>next && now<stop) {
    this->captureThread->updateTimeLapse(); 
    this->lastTimeLapse = now;
  }
  else if (now>stop) {
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
  this->ui.user_trackval->setValue(settings.value("trackval",50).toInt()); 
  settings.endGroup();
}
//----------------------------------------------------------------------------
void MartyCam::onMouseDoubleClickEvent(const QPoint&)
{
  this->renderWidget->showFullScreen();
}
