#include "settings.h"
#include <QDebug>
#include <QFileDialog>
#include "../3rdparty/include/videoInput.h"
#include "renderwidget.h"

//----------------------------------------------------------------------------
SettingsWidget::SettingsWidget(QWidget* parent) : QWidget(parent) 
{
  ui.setupUi(this);
  setMinimumWidth(150);
  //  
  connect(ui.res640Radio, SIGNAL(toggled(bool)), this, SLOT(on640ResToggled(bool)));
  connect(ui.res320Radio, SIGNAL(toggled(bool)), this, SLOT(on320ResToggled(bool)));
  connect(ui.threshold, SIGNAL(valueChanged(int)), this, SLOT(onThresholdChanged(int)));
  connect(ui.average, SIGNAL(valueChanged(int)), this, SLOT(onAverageChanged(int)));
  connect(ui.erode, SIGNAL(valueChanged(int)), this, SLOT(onErodeChanged(int)));
  connect(ui.dilate, SIGNAL(valueChanged(int)), this, SLOT(onDilateChanged(int)));
  connect(ui.browse, SIGNAL(clicked()), this, SLOT(onBrowseClicked()));
  connect(ui.WriteAVI, SIGNAL(toggled(bool)), this, SLOT(onWriteAVIToggled(bool)));
  connect(&this->clock, SIGNAL(timeout()), this, SLOT(onTimer()));
  connect(ui.blendRatio, SIGNAL(valueChanged(int)), this, SLOT(onBlendChanged(int)));  
  
  ImageButtonGroup.addButton(ui.cameraImage,0);
  ImageButtonGroup.addButton(ui.movingAverage,1);
  ImageButtonGroup.addButton(ui.differenceImage,2);
  ImageButtonGroup.addButton(ui.blendedImage,3);
  connect(&ImageButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onImageSelection(int)));

  RotateButtonGroup.addButton(ui.rotate0,0);
  RotateButtonGroup.addButton(ui.rotate90,1);
  RotateButtonGroup.addButton(ui.rotate90m,2);
  RotateButtonGroup.addButton(ui.rotate180,3);
  connect(&RotateButtonGroup, SIGNAL(buttonClicked(int)), this, SLOT(onRotateSelection(int)));

  //
	// create a videoInput object to enumerate devices
  //
	videoInput VI;
	int numDevices = VI.listDevices();	
  for (int i=0; i<numDevices; i++) {
    this->ui.cameraSelect->addItem(VI.getDeviceName(i));
  }
  connect(this->ui.cameraSelect, SIGNAL(currentIndexChanged(int)), this, SLOT(onCameraSelection(int)));
}
//----------------------------------------------------------------------------
void SettingsWidget::setThreads(CaptureThread *capthread, ProcessingThread *procthread)
{
  this->capturethread = capthread;
  this->processingthread = procthread;
}
//----------------------------------------------------------------------------
void SettingsWidget::on640ResToggled(bool on) {
  if(on) {
    emit(resolutionSelected(cvSize(640,480)));
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::on320ResToggled(bool on) {
  if(on) {
    emit(resolutionSelected(cvSize(320,240)));
  }
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
    this->capturethread->setWriteAVIDir(fileName.toStdString().c_str());
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onWriteAVIToggled(bool state) 
{
  this->RecordAVI(state);
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
    this->ui.WriteAVI->setChecked(false);
  }
}
//----------------------------------------------------------------------------
CvSize SettingsWidget::getSelectedResolution() 
{
  if (ui.res640Radio->isChecked()) {
    return cvSize(640,480);
  }
  return cvSize(320,240);
}
//----------------------------------------------------------------------------
void SettingsWidget::RecordAVI(bool state) 
{
  this->ui.WriteAVI->setChecked(state);
  //
  QString filePath = this->ui.avi_directory->text();
  QString fileName = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
  this->capturethread->setWriteAVIName(fileName.toStdString().c_str());
  this->capturethread->setWriteAVIDir(filePath.toStdString().c_str());
  //
  // add duration to end time even if already running
  QTime duration = this->ui.AVI_Duration->time();
  int secs = duration.second();
  int mins = duration.minute();
  int hrs  = duration.hour();
  this->AVI_EndTime = QTime::currentTime().addSecs(secs+mins*60+hrs*60*60).addMSecs(500);
  //
  if (!state) {    
    this->capturethread->setWriteAVI(state);
    this->ui.WriteAVI->setText("Write AVI");
    this->capturethread->closeAVI() ;
    this->clock.stop();
  }
  else if (!this->capturethread->getWriteAVI()) {
    this->capturethread->setWriteAVI(state);
    this->AVI_StartTime = QTime::currentTime();
    this->ui.WriteAVI->setText("Stop AVI");
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
void SettingsWidget::onRotateSelection(int btn)
{
  this->processingthread->setRotation(btn);
  if (btn==0 || btn==3) {
    this->renderWidget->setFixedSize(640, 480);
  }
  if (btn==1 || btn==2) {
    this->renderWidget->setFixedSize(480, 640);
  }
}
//----------------------------------------------------------------------------
void SettingsWidget::onBlendChanged(int value)
{
  this->processingthread->setBlendRatio(value/100.0);
}
//----------------------------------------------------------------------------
void SettingsWidget::onCameraSelection(int index)
{
  emit(CameraIndexChanged(index));
}
//----------------------------------------------------------------------------
