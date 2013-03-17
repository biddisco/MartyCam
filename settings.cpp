#include "settings.h"
#include <QDebug>
#include <QFileDialog>
#include <QSettings>
#include <QClipboard.h>
//
#include "renderwidget.h"
#include "IPCameraForm.h"
//
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui_c.h>
//
// we need these to get access to videoInput
// Caution: including cpp file here as routines are not exported from openCV
#include "../../highgui/src/precomp.hpp"
#include "../../highgui/src/cap_dshow.cpp"

//----------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent) : QWidget(parent) 
{
  this->processingthread = NULL;
  this->capturethread    = NULL;
  this->SnapshotId       = 0;
  //
  ui.setupUi(this);
  setMinimumWidth(150);
  //
  connect(ui.threshold, SIGNAL(valueChanged(int)), this, SLOT(onThresholdChanged(int)));
  connect(ui.average, SIGNAL(valueChanged(int)), this, SLOT(onAverageChanged(int)));
  connect(ui.erode, SIGNAL(valueChanged(int)), this, SLOT(onErodeChanged(int)));
  connect(ui.dilate, SIGNAL(valueChanged(int)), this, SLOT(onDilateChanged(int)));
  connect(ui.browse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
  connect(ui.add_camera, SIGNAL(clicked()), this, SLOT(onAddCameraClicked()));
  
  connect(ui.WriteMotionAVI, SIGNAL(toggled(bool)), this, SLOT(onWriteMotionAVIToggled(bool)));
  connect(&this->clock, SIGNAL(timeout()), this, SLOT(onTimer()));
  connect(ui.blendRatio, SIGNAL(valueChanged(int)), this, SLOT(onBlendChanged(int)));  
  connect(ui.noiseBlend, SIGNAL(valueChanged(int)), this, SLOT(onBlendChanged(int)));  
  //
  connect(ui.snapButton, SIGNAL(clicked()), this, SLOT(onSnapClicked()));  
  connect(ui.startTimeLapse, SIGNAL(clicked()), this, SLOT(onStartTimeLapseClicked()));  

  
  ResolutionButtonGroup.addButton(ui.res720Radio,0);
  ResolutionButtonGroup.addButton(ui.res640Radio,1);
  ResolutionButtonGroup.addButton(ui.res320Radio,2);
  connect(&ResolutionButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onResolutionSelection(int)));

  ImageButtonGroup.addButton(ui.cameraImage,0);
  ImageButtonGroup.addButton(ui.movingAverage,1);
  ImageButtonGroup.addButton(ui.differenceImage,2);
  ImageButtonGroup.addButton(ui.blendedImage,3);
  ImageButtonGroup.addButton(ui.maskImage,4);
  ImageButtonGroup.addButton(ui.noiseImage,5);
  connect(&ImageButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onImageSelection(int)));

  RotateButtonGroup.addButton(ui.rotate0,0);
  RotateButtonGroup.addButton(ui.rotate90,1);
  RotateButtonGroup.addButton(ui.rotate90m,2);
  RotateButtonGroup.addButton(ui.rotate180,3);
  connect(&RotateButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onRotateSelection(int)));
  //
  this->cameraForm = new IPCameraForm(this);
  this->setupCameraList();
}
//----------------------------------------------------------------------------
void SettingsWidget::setThreads(CaptureThread *capthread, ProcessingThread *procthread)
{
  this->capturethread = capthread;
  this->processingthread = procthread;
}
//----------------------------------------------------------------------------
void SettingsWidget::onThresholdChanged(int value)
{
  this->ui.t_label->setText(QString("%1").arg(value, 2));
  this->processingthread->setThreshold(value);
}
//----------------------------------------------------------------------------
void SettingsWidget::onAverageChanged(int value)
{
  double newval = value/100.0;
  QString label = QString("%1").arg(newval,4 , 'f', 2);
  this->ui.a_label->setText(label);
  this->processingthread->setAveraging(value/100.0);
}
//----------------------------------------------------------------------------
void SettingsWidget::onErodeChanged(int value)
{
  this->ui.e_label->setText(QString("%1").arg(value, 2));
  this->processingthread->setErodeIterations(value);
}
//----------------------------------------------------------------------------
void SettingsWidget::onDilateChanged(int value)
{
  this->ui.d_label->setText(QString("%1").arg(value, 2));
  this->processingthread->setDilateIterations(value);
}
//----------------------------------------------------------------------------
void SettingsWidget::onBrowseClicked()
{
  QFileDialog dialog;
  dialog.setViewMode(QFileDialog::Detail);
  dialog.setFileMode(QFileDialog::DirectoryOnly);
  if(dialog.exec()) {
    QString fileName = dialog.selectedFiles().at(0);
    this->ui.avi_directory->setText(fileName);
    this->capturethread->setWriteMotionAVIDir(fileName.toStdString().c_str());
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onAddCameraClicked()
{
  this->cameraForm->seupModelView();
  if(this->cameraForm->exec()) {
    this->cameraForm->saveSettings();
    this->setupCameraList();
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::setupCameraList()
{
  disconnect(this->ui.cameraSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(onCameraSelection(int)));
  int index = this->ui.cameraSelect->currentIndex();
  this->ui.cameraSelect->blockSignals(true);
  this->ui.cameraSelect->clear();
  //
  // use videoInput object to enumerate devices
  this->NumDevices = videoInput::listDevices(true);
  for (int i=0; i<this->NumDevices; i++) {
    this->ui.cameraSelect->addItem(videoInput::getDeviceName(i));
  }
  //
  stringpairlist &cameras = this->cameraForm->getList();
  for (stringpairlist::iterator it=cameras.begin(); it!=cameras.end(); ++it) {
    this->ui.cameraSelect->addItem(it->first.c_str());
  }
  this->ui.cameraSelect->blockSignals(false);
  connect(this->ui.cameraSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(onCameraSelection(int)));
  if (index>=0 && index<this->ui.cameraSelect->count()) {
    this->ui.cameraSelect->setCurrentIndex(index);
  }
  else {
    this->ui.cameraSelect->setCurrentIndex(0);
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onWriteMotionAVIToggled(bool state) 
{
  this->RecordMotionAVI(state);
}
//----------------------------------------------------------------------------
void SettingsWidget::onTimer() 
{
  QTime now = QTime::currentTime().addMSecs(500);
  int el = this->AVI_StartTime.elapsed();
  int h  = el/3600000>0?el/3600000:0;
  int m  = el-h*3600000>0?(el-h*3600000)/60000:0;
  int s  = el-h*3600000-m*60000>0?(el-(h*3600000+m*60000))/1000:0;
  int ms = el-(h*3600000+m*60000+s*1000)>0?el-(h*3600000+m*60000+s*1000):el;
  if (ms>500) s+=1;
  this->ui.elapsed->setText(QTime(h,m,s,0).toString("hh:mm:ss"));
  //
  int re = now.secsTo(this->AVI_EndTime);
  if (re>0) {
    h = re/3600>0 ? re/3600 : 0;
    m = re-h*3600>0 ? (re-h*3600)/60 : 0;
    s = re-h*3600-m*60>0 ? (re-(h*3600+m*60)) : 0;
    this->ui.remaining->setText(QTime(h,m,s,0).toString("hh:mm:ss"));
  }
  else {
    this->ui.WriteMotionAVI->setChecked(false);
  }
}
//----------------------------------------------------------------------------
cv::Size SettingsWidget::getSelectedResolution() 
{
  if (ui.res720Radio->isChecked()) {
    return cv::Size(720,576);
  }  
  if (ui.res640Radio->isChecked()) {
    return cv::Size(640,480);
  }
  return cv::Size(320,240);
}
//----------------------------------------------------------------------------
void SettingsWidget::SetupAVIStrings() 
{
  QString filePath = this->ui.avi_directory->text();
  QString fileName = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
  this->capturethread->setWriteMotionAVIName(fileName.toAscii().constData());
  this->capturethread->setWriteMotionAVIDir(filePath.toAscii().constData());
  QString fileName2 = "TimeLapse" + QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
  this->capturethread->setWriteTimeLapseAVIName(fileName2.toAscii().constData());
}
//----------------------------------------------------------------------------
void SettingsWidget::RecordMotionAVI(bool state) 
{
  this->ui.WriteMotionAVI->setChecked(state);
  //
  QString filePath = this->ui.avi_directory->text();
  QString fileName = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
  this->capturethread->setWriteMotionAVIName(fileName.toAscii().constData());
  this->capturethread->setWriteMotionAVIDir(filePath.toAscii().constData());

  //
  // add duration to end time even if already running
  //
  QTime duration = this->ui.AVI_Duration->time();
  int secs = duration.second();
  int mins = duration.minute();
  int hrs  = duration.hour();
  this->AVI_EndTime = QTime::currentTime().addSecs(secs+mins*60+hrs*60*60).addMSecs(500);
  //
  if (!state) {    
    this->capturethread->setWriteMotionAVI(state);
    this->ui.WriteMotionAVI->setText("Write AVI");
    this->capturethread->closeAVI();
    this->clock.stop();
  }
  else if (!this->capturethread->getWriteMotionAVI()) {
    this->capturethread->setWriteMotionAVI(state);
    this->AVI_StartTime = QTime::currentTime();
    this->ui.WriteMotionAVI->setText("Stop AVI");
    this->ui.elapsed->setText("00:00:00");
    this->ui.remaining->setText(duration.toString("hh:mm:ss"));
    this->clock.setInterval(1000);
    this->clock.start();
    this->onTimer();
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onImageSelection(int btn)
{
  this->processingthread->setDisplayImage(btn);
}
//----------------------------------------------------------------------------
void SettingsWidget::onResolutionSelection(int btn)
{
  switch (btn) {
    case 0: 
      emit(resolutionSelected(cv::Size(720,576)));
      break;
    case 1: 
      emit(resolutionSelected(cv::Size(640,480)));
      break;
    case 2: 
      emit(resolutionSelected(cv::Size(320,240)));
      break;
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onRotateSelection(int btn)
{
  this->capturethread->setAbort(true);
  this->capturethread->wait();
  this->capturethread->setRotation(btn);
  this->capturethread->setAbort(false);
  this->capturethread->start();
  //
  if (btn==0 || btn==3) {
    this->renderWidget->setCVSize(this->capturethread->getImageSize());
  }
  if (btn==1 || btn==2) {
    this->renderWidget->setCVSize(this->capturethread->getRotatedSize());
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onBlendChanged(int value)
{
  this->processingthread->setBlendRatios(this->ui.blendRatio->value()/100.0, this->ui.noiseBlend->value()/100.0);
}
//----------------------------------------------------------------------------
void SettingsWidget::onCameraSelection(int index)
{
  QString val = this->ui.cameraSelect->currentText();
  // if index is a user supplied IP camera, get the URL from the map
  if (index>=this->NumDevices) {
    stringpairlist &cameras = this->cameraForm->getList();
    val = cameras[val.toAscii().data()].c_str();
  }
  else {
    val = "";
  }
  //
  emit(CameraIndexChanged(index, val));
}
//----------------------------------------------------------------------------
void SettingsWidget::saveSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginGroup("MotionDetection");
  settings.setValue("threshold",this->ui.threshold->value()); 
  settings.setValue("average",this->ui.average->value()); 
  settings.setValue("erode",this->ui.erode->value()); 
  settings.setValue("dilate",this->ui.dilate->value()); 
  settings.endGroup();

  settings.beginGroup("UserSettings");
  settings.setValue("resolution",this->ResolutionButtonGroup.checkedId());
  settings.setValue("cameraIndex",this->ui.cameraSelect->currentIndex()); 
  settings.setValue("aviDirectory",this->ui.avi_directory->text()); 
  settings.setValue("rotation",this->RotateButtonGroup.checkedId());
  settings.setValue("display",this->ImageButtonGroup.checkedId());
  settings.setValue("blendImage",this->ui.blendRatio->value()); 
  settings.setValue("blendNoise",this->ui.noiseBlend->value());
  settings.endGroup();

  settings.beginGroup("MotionAVI");
  settings.setValue("snapshot",this->SnapshotId);
  settings.setValue("aviDuration", this->ui.AVI_Duration->time());
  settings.endGroup();

  settings.beginGroup("TimeLapse");
  settings.setValue("interval", this->ui.interval->time());
  settings.setValue("duration", this->ui.duration->dateTime());
  settings.setValue("startDateTime", this->ui.startDateTime->dateTime());
  settings.endGroup();
}
//----------------------------------------------------------------------------
void SettingsWidget::loadSettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  settings.beginGroup("MotionDetection");
  this->ui.threshold->setValue(settings.value("threshold",3).toInt()); 
  this->ui.average->setValue(settings.value("average",10).toInt()); 
  this->ui.erode->setValue(settings.value("erode",1).toInt()); 
  this->ui.dilate->setValue(settings.value("dilate",1).toInt()); 
  settings.endGroup();
/*
  connect(ui.browse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
  connect(ui.WriteMotionAVI, SIGNAL(toggled(bool)), this, SLOT(onWriteMotionAVIToggled(bool)));
  connect(&this->clock, SIGNAL(timeout()), this, SLOT(onTimer()));
  connect(ui.blendRatio, SIGNAL(valueChanged(int)), this, SLOT(onBlendChanged(int)));  
*/
  settings.beginGroup("UserSettings");
  this->ImageButtonGroup.button(settings.value("resolution",0).toInt())->click();
  this->ui.cameraSelect->setCurrentIndex(settings.value("cameraIndex",0).toInt());
  this->ui.avi_directory->setText(settings.value("aviDirectory","C:\\Wildlife").toString());
  //
  this->RotateButtonGroup.button(settings.value("rotation",0).toInt())->click();
  this->ImageButtonGroup.button(settings.value("display",0).toInt())->click();
  this->ui.blendRatio->setValue(settings.value("blendImage",0.5).toInt()); 
  this->ui.noiseBlend->setValue(settings.value("blendNoise",0.5).toInt()); 
  settings.endGroup();

  settings.beginGroup("MotionAVI");
  this->SnapshotId = settings.value("snapshot",0).toInt();  
  this->ui.AVI_Duration->setTime(settings.value("aviDuration", QTime(0,0,10)).toTime());
  settings.endGroup();

  settings.beginGroup("TimeLapse");

  this->ui.startDateTime->setDateTime(settings.value("startDateTime", QDateTime(QDate::currentDate(), QTime::currentTime())).toDateTime());
  this->ui.interval->setTime(settings.value("interval", QTime(0,1,00)).toTime());
  this->ui.duration->setTime(settings.value("duration", QDateTime(QDate(0,0,1), QTime(0,1,0))).toTime());

  this->SnapshotId = settings.value("snapshot",0).toInt();  
  this->ui.AVI_Duration->setTime(settings.value("aviDuration", QTime(0,0,10)).toTime());
  settings.endGroup();
}
//----------------------------------------------------------------------------
void SettingsWidget::onSnapClicked()
{
  QPixmap p = QPixmap::grabWidget(this->renderWidget);
  QString filename = QString("%1/MartySnap-%2").arg(this->ui.avi_directory->text()).arg(SnapshotId, 3, 10, QChar('0')) + QString(".png");
  p.save(filename);
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setPixmap(p);
}
//----------------------------------------------------------------------------
void SettingsWidget::onStartTimeLapseClicked()
{
  QPixmap p = QPixmap::grabWidget(this->renderWidget);
  QString filename = QString("%1/MartySnap-%2").arg(this->ui.avi_directory->text()).arg(SnapshotId, 3, 10, QChar('0')) + QString(".png");
  p.save(filename);
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setPixmap(p);
}
//----------------------------------------------------------------------------
QDateTime SettingsWidget::TimeLapseStart()
{
  return this->ui.startDateTime->dateTime();
}
//----------------------------------------------------------------------------
QDateTime SettingsWidget::TimeLapseEnd()
{
  QDateTime finish = this->ui.duration->dateTime();
  int days = QDateTime().daysTo(finish);
  QDateTime result = this->TimeLapseStart().addDays(days);
  return result;
}

