#include "martycam.h"
#include "renderwidget.h"
#include "settings.h"
#include "imagebuffer.h"
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
//
#include "chart.h"
#include "chart/datacontainers.h"


typedef vector<int> vint;
typedef list <int>  lint;
typedef list <double> ldouble;
typedef list< pair<double,double> > lpair;

lpair position;
vint  velocity;
vint  times;
vint  motionLevel;
lint  press2;
ldouble press3;

static int current_time = 0;

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
  this->imageSize               = cvSize(0,0);
  this->cameraIndex             = 0;
  this->imageBuffer             = new ImageBuffer(2);

  // Layout of - chart
  QWidget * widget = this->ui.motionGroup;
  Chart *chart = this->ui.chart; // new Chart();
/*
QLayout * layout3 = new QVBoxLayout();
  layout3->addWidget(chart);
  QLayout * wert = new QHBoxLayout(); 
  wert->addWidget(this->ui.chartPosition); 
  wert->addWidget(this->ui.zoomBox); 
  layout3->addItem(wert);
  layout3->addWidget(this->ui.sizeSlider);
//  layout3->addItem(this->ui.gridLayout);
  widget->setLayout(layout3);
*/

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
  connect(settingsWidget, SIGNAL(CameraIndexChanged(int,QString)), this, SLOT(onCameraIndexChanged(int,QString)));
  //
  connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  connect(ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
//  connect(&updateTimer, SIGNAL(timeout()), this, SLOT(updateStats()));
  //
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, QString());
  this->imageSize = this->captureThread->getImageSize();
  this->renderWidget->setFixedSize(this->imageSize.width, this->imageSize.height);
  this->processingThread = this->createProcessingThread(this->imageSize, NULL);
  this->processingThread->start();
  //
  this->updateTimer.start(50);
  connect(this->captureThread, SIGNAL(RecordingState(bool)), 
    this, SLOT(onRecordingStateChanged(bool)),Qt::QueuedConnection); 

  connect(this->captureThread, SIGNAL(NewImage()), 
    this, SLOT(updateStats()),Qt::QueuedConnection); 

  this->initChart();

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
void MartyCam::createCaptureThread(int FPS, CvSize &size, int camera, QString &cameraname)
{
  captureThread = new CaptureThread(imageBuffer, size, camera, cameraname);
  captureThread->startCapture(FPS);
  captureThread->start(QThread::IdlePriority);
  this->settingsWidget->setThreads(this->captureThread, this->processingThread);
  updateTimer.start();
//  std::string capStatus = captureThread->getCaptureStatusString();
//  this->ui.outputWindow->setText(capStatus.c_str());

  connect(this->captureThread, SIGNAL(RecordingState(bool)), 
    this, SLOT(onRecordingStateChanged(bool)),Qt::QueuedConnection); 

  connect(this->captureThread, SIGNAL(NewImage()), 
    this, SLOT(updateStats()),Qt::QueuedConnection); 

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
void MartyCam::onCameraIndexChanged(int index, QString val)
{
  this->deleteCaptureThread();
  this->cameraIndex = index;
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, val);
}
//----------------------------------------------------------------------------
void MartyCam::onResolutionSelected(CvSize newSize) {
  this->imageSize = newSize;
  ProcessingThread *temp = this->createProcessingThread(this->imageSize, this->processingThread);
  this->deleteProcessingThread();
  this->deleteCaptureThread();
  this->imageBuffer->clear();
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, QString());
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
  times.push_back(current_time++);
  motionLevel.push_back(logval);
  if (times.size()>475) {
    times.erase(times.begin(),times.begin()+50); 
    motionLevel.erase(motionLevel.begin(),motionLevel.begin()+50); 
  }
  this->ui.chart->setPosition(times.front());
  this->ui.chart->setSize(500);
  this->ui.chart->update();

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
void MartyCam::onMotionDetectionChanged(int state)
{
  this->processingThread->setMotionDetecting(state);
}
//----------------------------------------------------------------------------
void MartyCam::initChart()
{
  QPen vPen;
  vPen.setColor(Qt::red);
  vPen.setWidthF(2.0);
  vPen.setColor(Qt::green);
  Channel motion(0,90,new DoubleDataContainer<vint,vint>(times,motionLevel), trUtf8("Motion Level"),vPen);

  motion.setShowScale(true); 
  motion.setShowLegend(true);
  motion.setShowLegend(true);
  motion.setMaximum(75.0);
  //pozycja.showScale = predkosc.showScale = false ;
  //chart->scaleGrid().showScale=false;
  //chart->scaleGrid().showGrid=false;
  ui.chart->addChannel(motion);
  ui.chart->setSize(100);
}