#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui_settings.h"
#include "capturethread.h"
#include "processingthread.h"
#include "MotionFilter.h"
#include <QDateTime>
#include <QTimer>
#include <QButtonGroup>

class RenderWidget;
class IPCameraForm;

class SettingsWidget;
typedef std::shared_ptr<SettingsWidget> SettingsWidget_SP;

template<class T>
class QMySignalBlocker
{
  T* const o;
public:
  explicit QMySignalBlocker( T * oo ) : o(oo) {}
  T* operator->()
  {
    if (o) o->blockSignals( true );
    return o;
  }
  ~QMySignalBlocker()
  {
    if (o) o->blockSignals(false);
  }
};

template<class T>
QMySignalBlocker<T> SilentCall(T* o)
{
  return QMySignalBlocker<T>(o);
}

class SettingsWidget : public QWidget {
Q_OBJECT;
public:
  SettingsWidget(QWidget* parent);

  cv::Size  getSelectedResolution();
  int getSelectedResolutionButton();
  int       getSelectedRotation();
  int       getRequestedFps();
  int       getNumOfResolutions();


  void      RecordMotionAVI(bool state);

  void setThreads(CaptureThread_SP capthread, ProcessingThread_SP procthread);
  void unsetCaptureThread();
  void unsetProcessingThread();
  void setRenderWidget(RenderWidget *rw) { this->renderWidget = rw; }
  void switchToNextResolution();
  void switchToPreviousResolution();

  QDateTime TimeLapseStart();
  QDateTime TimeLapseEnd();
  QTime     TimeLapseInterval() { return this->ui.interval->time(); }
  double    TimeLapseFPS() { return this->ui.timeLapseFPS->value(); }
  bool      TimeLapseEnabled() { return this->ui.timeLapseEnabled->isChecked(); }

  int getCameraIndex(std::string &text);
  ProcessingType getCurentProcessingType();
  MotionFilterParams getMotionFilterParams();
  FaceRecogFilterParams getFaceRecogFilterParams();

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
  //
  void onRequestedFpsChanged(int);
  //
  void onDecimationCoeffChanged(int);
  void onEyesRecogStateChanged(int);
  //
  void onSnapClicked();
  void onStartTimeLapseClicked();
  //
  void onTabChanged(int);
  //
  void onCameraSelection(int index);

  void loadSettings();
  void saveSettings();
  void setupCameraList();
  void SetupAVIStrings() ;

signals:
  void resolutionSelected(cv::Size);
  void rotationChanged(int);

  void CameraIndexChanged(int, QString);

protected:
  QString decimationCoeffToQString(int sliderVal);

  Ui::SettingsWidget  ui;
  CaptureThread_SP      capturethread;
  ProcessingThread_SP   processingthread;
  QTime               AVI_StartTime;
  QTime               AVI_EndTime;
  QTimer              clock;
  int                 SnapshotId;
  QButtonGroup        ImageButtonGroup;
  QButtonGroup        RotateButtonGroup;
  QButtonGroup        ResolutionButtonGroup;
  int                 numberOfResolutions;
  int                 currentResolutionButtonIndex;
  int                 previousResolutionButtonIndex;
  RenderWidget       *renderWidget;
  IPCameraForm       *cameraForm;
  int                 NumDevices;
  //
  bool                faceRecognitionAcitve;
  int                 requestedFps;
  bool                eyesRecognitionActive;

};

#endif
