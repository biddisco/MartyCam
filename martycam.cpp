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

  renderWidget = new RenderWidget(this);
  this->centralWidget()->layout()->addWidget(renderWidget);
  QSplitter *splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(this->ui.motionGroup);
  splitter->addWidget(renderWidget);
  
  this->setCentralWidget(splitter);

  imageBuffer = new ImageBuffer(1);
  captureThread = new CaptureThread(imageBuffer, 0);
  processingThread = new ProcessingThread(imageBuffer);
  processingThread->setRootFilter(renderWidget);
  
  // create the settings dock widget
  settingsDock = new QDockWidget("Settings", this);
  settingsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
  settingsWidget = new SettingsWidget(this, this->captureThread, processingThread);
  settingsDock->setWidget(settingsWidget);
  settingsDock->setMinimumWidth(300);
  addDockWidget(Qt::RightDockWidgetArea, settingsDock);
  connect(settingsWidget, SIGNAL(resolutionSelected(CaptureThread::FrameSize)), this, SLOT(onResolutionSelected(CaptureThread::FrameSize)));
  connect(settingsWidget, SIGNAL(CameraIndexChanged(int)), this, SLOT(onCameraIndexChanged(int)));
  
  updateTimer = new QTimer(this);
  connect(updateTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
  updateTimer->start(1000);
  
  connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  connect(ui.actionStart, SIGNAL(triggered()), this, SLOT(startTracking()));
  connect(ui.actionStop, SIGNAL(triggered()), this, SLOT(stopTracking()));
  connect(ui.actionRecord, SIGNAL(triggered()), this, SLOT(startRecording()));
  connect(ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
  //
  this->UserDetectionThreshold = 0.25;
  this->RecordingEvents = 0;
  connect(this->captureThread, SIGNAL(RecordingState(bool)), this, SLOT(onRecordingStateChanged(bool))); 
  //
  processingThread->start();
  captureThread->startCapture(15, CaptureThread::Size640);
  captureThread->start(QThread::IdlePriority);
}
//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*) {
  updateTimer->stop();
  captureThread->stopCapture();
  processingThread->setAbort(true);
  captureThread->setAbort(true);
  captureThread->wait();
  processingThread->wait();
  delete captureThread;
  delete processingThread;
}
//----------------------------------------------------------------------------
void MartyCam::onCameraIndexChanged(int index)
{
  updateTimer->stop();
  captureThread->stopCapture();
  captureThread->setAbort(true);
  captureThread->wait();
  delete captureThread;
  captureThread = new CaptureThread(imageBuffer, index);
  captureThread->startCapture(15, CaptureThread::Size640);
  captureThread->start(QThread::IdlePriority);
  updateTimer->start();
}
//----------------------------------------------------------------------------
// resolution has been changed from the settings
void MartyCam::onResolutionSelected(CaptureThread::FrameSize newSize) {
/*
  if(trackController->isTracking()) {
    trackController->stopTracking();
    trackController->setFrameSize(newSize);
    trackController->startTracking();
  }
  else {
    trackController->setFrameSize(newSize);
  }
*/
}
//----------------------------------------------------------------------------
void MartyCam::startTracking() {
//  trackController->setFrameSize(settingsWidget->getSelectedResolution());
//  trackController->startTracking();
}
//----------------------------------------------------------------------------
void MartyCam::stopTracking() {
//  trackController->stopTracking();
  ui.actionStart->setEnabled(true);
  ui.actionStop->setEnabled(false);
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
