#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui_settings.h"
#include "capturethread.h"
#include "processingthread.h"
#include <QDateTime>
#include <QTimer>
#include <QButtonGroup>

class RenderWidget;
class IPCameraForm;

class SettingsWidget : public QWidget {
Q_OBJECT;
public:
  SettingsWidget(QWidget* parent);
  cv::Size getSelectedResolution();
  void RecordMotionAVI(bool state);

  void setThreads(CaptureThread *capthread, ProcessingThread *procthread);
  void setRenderWidget(RenderWidget *rw) { this->renderWidget = rw; }
  
  QDateTime TimeLapseStart();
  QDateTime TimeLapseEnd();
  QTime TimeLapseInterval() { return this->ui.interval->time(); }
  double  TimeLapseFPS() { return this->ui.timeLapseFPS->value(); }
  bool  TimeLapseEnabled() { return this->ui.timeLapseEnabled->isChecked(); }


public slots:
  void onThresholdChanged(int value);
  void onAverageChanged(int value);
  void onErodeChanged(int value);
  void onDilateChanged(int value);  
  void onBrowseClicked();
  void onAddCameraClicked();
  void onWriteMotionAVIToggled(bool state);
  void onTimer();
  void onImageSelection(int btn);
  void onRotateSelection(int btn);
  void onResolutionSelection(int btn);
  void onBlendChanged(int value);
  void onCameraSelection(int index);
  //
  void onSnapClicked();
  void onStartTimeLapseClicked();

  void loadSettings();
  void saveSettings();
  void setupCameraList();
  void SetupAVIStrings() ;

signals:
  void resolutionSelected(cv::Size);
  void rotationChanged(int);

  void CameraIndexChanged(int, QString);

protected:
  Ui::SettingsWidget  ui;
  CaptureThread      *capturethread;
  ProcessingThread   *processingthread;
  QTime               AVI_StartTime;
  QTime               AVI_EndTime;
  QTimer              clock;
  int                 SnapshotId;
  QButtonGroup        ImageButtonGroup;
  QButtonGroup        RotateButtonGroup;
  QButtonGroup        ResolutionButtonGroup;
  RenderWidget       *renderWidget;
  IPCameraForm       *cameraForm;
  int                 NumDevices;
};

#endif
