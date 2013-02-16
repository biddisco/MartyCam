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
//
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>
//
using namespace boost::accumulators;    
accumulator_set<int, stats<tag::rolling_mean> > acc(tag::rolling_window::window_size = 50);
//
typedef vector<int> vint;
typedef vector<double> vdouble;
typedef list <int>  lint;
typedef list <double> ldouble;
typedef list< pair<double,double> > lpair;
//
vint     frameNumber;
vdouble  motionLevel;
vdouble  movingAverage;
vint     thresholdTime;
vdouble  thresholdLevel;
vint     frameNumberEvent;
vdouble  eventLevel;
//
lpair  position;
vint   velocity;
lint   press2;
ldouble press3;

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
  this->EventRecordCounter      = 0;
  this->insideMotionEvent       = 0;
  this->imageSize               = cvSize(0,0);
  this->imageBuffer             = new ImageBuffer(2);
  this->cameraIndex             = 1;

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
  this->settingsWidget = new SettingsWidget(this);
  this->settingsWidget->setRenderWidget(this->renderWidget);
  settingsDock->setWidget(this->settingsWidget);
  settingsDock->setMinimumWidth(300);
  addDockWidget(Qt::RightDockWidgetArea, settingsDock);
  connect(this->settingsWidget, SIGNAL(resolutionSelected(CvSize)), this, SLOT(onResolutionSelected(CvSize)));
  connect(this->settingsWidget, SIGNAL(CameraIndexChanged(int,QString)), this, SLOT(onCameraIndexChanged(int,QString)));
  //
  connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
  connect(ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
  //
  QString camerastring = "Garden";
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
  this->settingsWidget->loadSettings();
  //
  this->initChart();
}
//----------------------------------------------------------------------------
void MartyCam::closeEvent(QCloseEvent*) {
  this->settingsWidget->saveSettings();
  this->deleteProcessingThread();
  this->deleteCaptureThread();
}
//----------------------------------------------------------------------------
void MartyCam::deleteCaptureThread()
{
//  updateTimer.stop();
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
  //
  statusBar()->showMessage(QString("FPS: ")+QString::number(this->captureThread->getFPS(), 'f', 1));
  // scale up to 100*100 = 1E4 for log display
  double percent = 100.0*this->processingThread->getMotionPercent();
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->getMotionPercent(),4 , 'f', 2));
  //
  int counter = captureThread->GetFrameCounter();
  frameNumber.push_back(counter);
  frameNumberEvent.push_back(counter);
  motionLevel.push_back(logval);
  acc(logval);
  movingAverage.push_back(rolling_mean(acc));
  //
  if (this->eventDecision()) {
    eventLevel.push_back(100);
    if (this->ui.RecordingEnabled->isChecked()) {
      this->settingsWidget->RecordAVI(true);
    }
  }
  else {
    eventLevel.push_back(0);
  }
  //
  if (frameNumber.size()>475) {
    frameNumber.erase(frameNumber.begin(),frameNumber.begin()+1); 
    motionLevel.erase(motionLevel.begin(),motionLevel.begin()+1); 
    movingAverage.erase(movingAverage.begin(),movingAverage.begin()+1); 
    if (eventLevel.size()>475 && eventLevel.back()==0) {
      frameNumberEvent.erase(frameNumberEvent.begin(),frameNumberEvent.end()-475); 
      eventLevel.erase(eventLevel.begin(),eventLevel.end()-475); 
    }
  }
  thresholdTime[0] = frameNumber.front();
  thresholdTime[1] = frameNumber.back();
  thresholdLevel[0] = this->UserDetectionThreshold;
  thresholdLevel[1] = this->UserDetectionThreshold;
  //
  this->ui.chart->setUpdatesEnabled(0);
  this->ui.chart->setSize(this->ui.chart->width()+50);
  this->ui.chart->setPosition(frameNumber.front());
  this->ui.chart->setUpdatesEnabled(1);
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
    this->EventRecordCounter++;
    QString evc = QString("Events : %1").arg(this->EventRecordCounter, 3);
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
  const double gmax = 80;

  QPen vPen;
  vPen.setColor(Qt::red);
  vPen.setWidthF(2.0);

  vPen.setColor(Qt::green);
  Channel motion(0,gmax,
    new DoubleDataContainer<vint,vdouble>(frameNumber,motionLevel), trUtf8("Motion Level"),vPen);

  vPen.setColor(Qt::blue);
  Channel mean(0,gmax,
    new DoubleDataContainer<vint,vdouble>(frameNumber,movingAverage), trUtf8("Moving Average"),vPen);

  vPen.setColor(Qt::white);
  Channel eventline(0,gmax,
    new DoubleDataContainer<vint,vdouble>(frameNumberEvent,eventLevel), trUtf8("Event"),vPen);

  thresholdTime.push_back(0);
  thresholdTime.push_back(500);
  thresholdLevel.push_back(this->UserDetectionThreshold);
  thresholdLevel.push_back(this->UserDetectionThreshold);
  vPen.setColor(Qt::red);
  Channel trigger(0,gmax,
    new DoubleDataContainer<vint,vdouble>(thresholdTime,thresholdLevel), trUtf8("Trigger"),vPen);

  motion.setShowScale(true); 
  motion.setShowLegend(true);
  motion.setMaximum(gmax);

  mean.setMaximum(gmax);
  mean.setShowLegend(false);
  mean.setShowScale(false);

  trigger.setMaximum(gmax);
  trigger.setShowLegend(false);
  trigger.setShowScale(false);

  eventline.setMaximum(gmax);
  eventline.setShowLegend(false);
  eventline.setShowScale(false);

  //pozycja.showScale = predkosc.showScale = false ;
  //chart->scaleGrid().showScale=false;
  //chart->scaleGrid().showGrid=false;
  ui.chart->addChannel(motion);
  ui.chart->addChannel(mean);
  ui.chart->addChannel(trigger);
  ui.chart->addChannel(eventline);
  ui.chart->setSize(100);
}
//----------------------------------------------------------------------------
bool MartyCam::eventDecision() {
  double percent = 100.0*this->processingThread->getMotionPercent();
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
//  if (logval>this->UserDetectionThreshold) {
 if (rolling_mean(acc)>this->UserDetectionThreshold) { 
    return true; 
  }
  return false;
}
//----------------------------------------------------------------------------
