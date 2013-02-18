#include "martycam.h"
#include "renderwidget.h"
#include "settings.h"
#include "imagebuffer.h"
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QSplitter>
#include <QSettings>
//
#include "chart.h"
#include "chart/datacontainers.h"
//
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>
#include <boost/circular_buffer.hpp>
//
using namespace boost::accumulators;    
accumulator_set<int, stats<tag::rolling_mean> > acc(tag::rolling_window::window_size = 50);
//
#define CIRCULAR_BUFFER_SIZE 500
//
template<typename T1, typename T2>
class Plottable {  
  public:
    typedef boost::circular_buffer<T1> xListType;
    typedef boost::circular_buffer<T2> yListType;
    typedef DoubleDataContainer<xListType, yListType> channelType;

  public:
    Plottable(QPen &Pen, T2 Minval, T2 Maxval, const char *Title, xListType *xcont, yListType *ycont) : pen(Pen), minVal(Minval), maxVal(Maxval), title(Title)
    {
      xContainer = xcont ? xcont : new xListType(CIRCULAR_BUFFER_SIZE);
      yContainer = ycont ? ycont : new yListType(CIRCULAR_BUFFER_SIZE);
      deletex = (xcont==NULL);
      deletey = (ycont==NULL);
      data = new channelType(*xContainer, *yContainer);
    }

    ~Plottable()
    {
      if (deletex) delete xContainer;
      if (deletey) delete yContainer;
      delete data;
    }

    //
    QPen         pen;
    xListType   *xContainer;
    yListType   *yContainer;
    bool         deletex, deletey;
    channelType *data;
    T2           minVal;
    T2           maxVal;
    std::string  title;

};

typedef boost::circular_buffer<int>    vint;
typedef boost::circular_buffer<double> vdouble;
//
vint     frameNumber(CIRCULAR_BUFFER_SIZE);
vint     thresholdTime(2);
vdouble  thresholdLevel(2);
//
Plottable<int, double> movingAverage(QPen(QColor(Qt::blue)), 0, 100.0, "Moving Average", &frameNumber, NULL);
Plottable<int, double> motionLevel(QPen(QColor(Qt::green)),  0, 100.0, "Motion",         &frameNumber, NULL);
Plottable<int, double> psnr(QPen(QColor(Qt::yellow)),     20.0,  40.0, "PSNR",           &frameNumber, NULL);
Plottable<int, int> events(QPen(QColor(Qt::white)),        0.0, 100.0, "EventLevel",     &frameNumber, NULL);
Plottable<int, double> threshold(QPen(QColor(Qt::red)),    0.0, 100.0, "Threshold",      &thresholdTime, &thresholdLevel);


//
//----------------------------------------------------------------------------
MartyCam::MartyCam() : QMainWindow(0) 
{
  ui.setupUi(this);
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
  // Main threads for capture and processing
  //
  this->UserDetectionThreshold  = 0.25;
  this->EventRecordCounter      = 0;
  this->insideMotionEvent       = 0;
  this->imageSize               = cv::Size(0,0);
  this->imageBuffer             = new ImageBuffer(2);
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
  connect(ui.user_trackval, SIGNAL(valueChanged(int)), this, SLOT(onUserTrackChanged(int))); 
  //
  connect(ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));
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
//  updateTimer.stop();
  captureThread->setAbort(true);
  captureThread->wait();
  delete captureThread;
}
//----------------------------------------------------------------------------
void MartyCam::createCaptureThread(int FPS, cv::Size &size, int camera, QString &cameraname)
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
  this->processingThread = NULL;
}
//----------------------------------------------------------------------------
ProcessingThread *MartyCam::createProcessingThread(cv::Size &size, ProcessingThread *oldThread)
{
  ProcessingThread *temp = new ProcessingThread(imageBuffer, this->imageSize);
  if (oldThread) temp->CopySettings(oldThread);
  temp->setRootFilter(renderWidget);
  this->settingsWidget->setThreads(this->captureThread, temp);
  return temp;
}
//----------------------------------------------------------------------------
void MartyCam::onCameraIndexChanged(int index, QString URL)
{
  this->deleteCaptureThread();
  this->cameraIndex = index;
  this->createCaptureThread(15, this->imageSize, this->cameraIndex, URL);
}
//----------------------------------------------------------------------------
void MartyCam::onResolutionSelected(cv::Size newSize) {
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
  if (!this->processingThread) return;
  //
  statusBar()->showMessage(QString("FPS: ")+QString::number(this->captureThread->getFPS(), 'f', 1));
  // scale up to 100*100 = 1E4 for log display
  double percent = 100.0*this->processingThread->getMotionPercent();
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  acc(logval);
  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->getMotionPercent(),4 , 'f', 2));
  int eventvalue = this->eventDecision() ? 100 : 0;

  // x-axis is frame counter
  double counter = captureThread->GetFrameCounter();
  frameNumber.push_back(static_cast<int>(counter));
  // update y-values of graphs
  motionLevel.yContainer->push_back(logval);
  movingAverage.yContainer->push_back(rolling_mean(acc));
  psnr.yContainer->push_back(this->processingThread->getPSNR());
  events.yContainer->push_back(eventvalue);
  //
  if (eventvalue==100 && this->ui.RecordingEnabled->isChecked()) {
    this->settingsWidget->RecordAVI(true);
  }
  // The threshold line is just two points, update them each time step
  thresholdTime[0] = frameNumber.front();
  thresholdTime[1] = thresholdTime[0] + this->ui.chart->width();
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
  thresholdTime.push_back(0);
  thresholdTime.push_back(CIRCULAR_BUFFER_SIZE);
  //
  thresholdLevel.push_back(this->UserDetectionThreshold);
  thresholdLevel.push_back(this->UserDetectionThreshold);
  //
  Channel motion(motionLevel.minVal, motionLevel.maxVal, motionLevel.data, motionLevel.title.c_str(), motionLevel.pen);
  Channel mean(movingAverage.minVal, movingAverage.maxVal, movingAverage.data, movingAverage.title.c_str(), movingAverage.pen);
  Channel PSNR(psnr.minVal, psnr.maxVal, psnr.data, psnr.title.c_str(), psnr.pen);
  Channel eventline(events.minVal, events.maxVal, events.data, events.title.c_str(), events.pen);
  Channel trigger(threshold.minVal, threshold.maxVal, threshold.data, threshold.title.c_str(), threshold.pen);

  motion.setShowScale(true); 
  motion.setShowLegend(true);

  mean.setShowLegend(false);
  mean.setShowScale(false);
      
  PSNR.setShowLegend(false);
  PSNR.setShowScale(false);

  trigger.setShowLegend(false);
  trigger.setShowScale(false);

  eventline.setShowLegend(false);
  eventline.setShowScale(false);

  ui.chart->addChannel(motion);
  ui.chart->addChannel(mean);
  ui.chart->addChannel(PSNR);
  ui.chart->addChannel(trigger);
  ui.chart->addChannel(eventline);
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
 