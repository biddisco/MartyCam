#include "martycam.h"
#include "renderwidget.h"
#include "settings.h"
#include "imagebuffer.h"
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
//----------------------------------------------------------------------------
MartyCam::MartyCam() : QMainWindow(0) 
{
  ui.setupUi(this);
  //
  this->renderWidget = new RenderWidget(this);
  this->centralWidget()->layout()->addWidget(this->renderWidget);
  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(this->ui.motionGroup);
  splitter->addWidget(renderWidget);
  this->setCentralWidget(splitter);
  //
  // Main threads for capture and processing
  //
  this->UserDetectionThreshold  = 0.25;
  this->RecordingEvents         = 0;
  this->imageSize               = cvSize(640,480);
  this->cameraIndex             = 0;
  this->imageBuffer             = new ImageBuffer(1);
  //
  // create the settings dock widget
  //
  settingsDock = new QDockWidget("Settings", this);
  settingsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  settingsWidget = new SettingsWidget(this);
  settingsWidget->setRenderWidget(this->renderWidget);
  settingsDock->setWidget(settingsWidget);
  settingsDock->setMinimumWidth(300);
  addDockWidget(Qt::RightDockWidgetArea, settingsDock);
  connect(settingsWidget, SIGNAL(resolutionSelected(CvSize)), this, SLOT(onResolutionSelected(CvSize)));
  connect(settingsWidget, SIGNAL(CameraIndexChanged(int)), this, SLOT(onCameraIndexChanged(int)));
  //
  connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  connect(ui.actionStart, SIGNAL(triggered()), this, SLOT(startTracking()));
  connect(ui.actionStop, SIGNAL(triggered()), this, SLOT(stopTracking()));
  connect(ui.actionRecord, SIGNAL(triggered()), this, SLOT(startRecording()));
  connect(ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
  connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
  //
  this->createCaptureThread(15, this->imageSize, this->cameraIndex);
  this->imageSize = this->captureThread->getImageSize();
  this->processingThread = this->createProcessingThread(this->imageSize, NULL);
  this->processingThread->start();
  //
  this->updateTimer.start(1000);
  connect(this->captureThread, SIGNAL(RecordingState(bool)), this, SLOT(onRecordingStateChanged(bool))); 
}
//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*) {
  this->deleteProcessingThread();
  this->deleteCaptureThread();
}
//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
  updateTimer.stop();
  captureThread->setAbort(true);
  captureThread->wait();
  delete captureThread;
}
//----------------------------------------------------------------------------
void MartyCam::createCaptureThread(int FPS, CvSize &size, int camera)
{
  captureThread = new CaptureThread(imageBuffer, size, camera);
  captureThread->startCapture(FPS);
  captureThread->start(QThread::IdlePriority);
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
  updateTimer.start();
  std::string capStatus = captureThread->getCaptureStatusString();
  this->ui.outputWindow->setText(capStatus.c_str());
}
//----------------------------------------------------------------------------
void MartyCam::deleteProcessingThread()
{
  processingThread->setAbort(true);
  processingThread->wait();
  delete processingThread;
}
//----------------------------------------------------------------------------
ProcessingThread *MartyCam::createProcessingThread(CvSize &size, ProcessingThread *oldThread)
{
  ProcessingThread *temp = new ProcessingThread(imageBuffer, this->imageSize);
  if (oldThread) temp->CopySettings(oldThread);
  temp->setRootFilter(renderWidget);
  this->settingsWidget->setThreads(this->captureThread, temp);
  return temp;
}
//----------------------------------------------------------------------------
void MartyCam::onCameraIndexChanged(int index)
{
  this->deleteCaptureThread();
  this->cameraIndex = index;
  this->createCaptureThread(15, this->imageSize, this->cameraIndex);
}
//----------------------------------------------------------------------------
void MartyCam::onResolutionSelected(CvSize newSize) {
  this->imageSize = newSize;
  ProcessingThread *temp = this->createProcessingThread(this->imageSize, this->processingThread);
  this->deleteProcessingThread();
  this->deleteCaptureThread();
  this->imageBuffer->clear();
  this->createCaptureThread(15, this->imageSize, this->cameraIndex);
  this->processingThread = temp;
  this->processingThread->start(QThread::IdlePriority);
}
//----------------------------------------------------------------------------
void MartyCam::updateStats() {
  statusBar()->showMessage(QString("FPS: ")+QString::number(this->captureThread->getFPS(), 'f', 1));
  // scale up to 100*100 = 1E4 for log display
  double percent = 100.0*this->processingThread->getMotionPercent();
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  this->ui.progressBar->setValue(logval);
  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->getMotionPercent(),4 , 'f', 2));
  //
  if (this->ui.RecordingEnabled->isChecked()) {
    if (this->processingThread->getMotionPercent()>this->UserDetectionThreshold) {
      settingsWidget->RecordAVI(true);
    }
  }
}
//----------------------------------------------------------------------------
void MartyCam::onUserTrackChanged(int value)
{
  double percent = value;
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  this->ui.set_value->setText(QString("%1").arg(logval,4 , 'f', 2));
  // threshold back from log to original
  this->UserDetectionThreshold = pow(10,value*(4.0/100.0))/100.0;
  this->ui.set_value->setText(QString("%1").arg(this->UserDetectionThreshold,4 , 'f', 2));
}
//----------------------------------------------------------------------------
void MartyCam::onRecordingStateChanged(bool state)
{
  if (state) {
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: green; }");
    this->RecordingEvents++;
    QString evc = QString("Events : %1").arg(this->RecordingEvents, 3);
    this->ui.eventCounter->setText(evc);
  }
  else {
    this->ui.RecordingEnabled->setStyleSheet("QCheckBox { background-color: window; }");
  }
}
//----------------------------------------------------------------------------
