#ifndef SETTINGS_H
#define SETTINGS_H

#include <QButtonGroup>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
//
#include "martycam/core/MotionFilter.h"
#include "martycam/core/stream_capture_thread.h"

#include "martycam/core/processingthread.h"
#include "martycam/widgets/CameraSelectorWidget.h"
//
#include "ui_settings.h"

class RenderWidget;

class SettingsWidget;
typedef std::shared_ptr<SettingsWidget> SettingsWidget_SP;

template <class T>
class QMySignalBlocker
{
  T* const o;

  public:
  explicit QMySignalBlocker(T* oo)
    : o(oo)
  {
  }
  T* operator->()
  {
    if (o) o->blockSignals(true);
    return o;
  }
  ~QMySignalBlocker()
  {
    if (o) o->blockSignals(false);
  }
};

template <class T>
QMySignalBlocker<T> SilentCall(T* o)
{
  return QMySignalBlocker<T>(o);
}

class SettingsWidget : public QWidget
{
  Q_OBJECT;

  public:
  SettingsWidget(QWidget* parent);

  // Create the camera selector widget and add it to the settings widget layout
  void createCameraSelector();

  int getSelectedRotation();
  int getRequestedFps();
  int getNumOfResolutions();

  void RecordMotionAVI(bool state);

  void setThreads(StreamCaptureThread_SP capthread, ProcessingThread_SP procthread);
  void unsetCaptureThread();
  void unsetProcessingThread();
  void setRenderWidget(RenderWidget* rw) { this->renderWidget = rw; }
  void switchToNextResolution();
  void switchToPreviousResolution();

  QDateTime TimeLapseStart();
  QDateTime TimeLapseEnd();
  // The interval is just a datetime hh:mm:ss but we just get the msecs since o0:00:00
  qint64 TimeLapseInterval() { return this->ui.interval->time().msecsSinceStartOfDay(); }
  double TimeLapseFPS() { return this->ui.timeLapseFPS->value(); }
  double TimeLapseBitrateMBps() { return this->ui.timeLapseBitrateMBps->value(); }
  bool TimeLapseEnabled() { return this->ui.timeLapseEnabled->isChecked(); }
  void ShowTimeLapseState(bool active);
  void setTimeLapseRunningUi(bool running);
  //
  bool MotionProcessingEnabled() { return this->ui.motionProcessingEnabled->isChecked(); }
  bool FaceProcessingEnabled() { return this->ui.faceProcessingEnabled->isChecked(); }

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
  void onBlendChanged(int value);
  //
  void onRequestedFpsChanged(int);
  //
  void onDecimationCoeffChanged(int);
  void onEyesRecogStateChanged(int);
  //
  void onSnapClicked();
  void onStartTimeLapseClicked();
  void onTimeLapseEnabledToggled(bool enabled);
  //
  void onTabChanged(int);
  void onProcessingEnableToggled(bool);
  //

  void loadSettings();
  void saveSettings();
  void SetupAVIStrings();
  void syncTimeLapseSettingsToCaptureThread();
  void applyProcessingEnableState();

  signals:
  void cameraConfigChanged(QString cameraPath, int width, int height, int fps, int fourcc);
  void rotationChanged(int);

  protected:
  QString decimationCoeffToQString(int sliderVal);

  Ui::SettingsWidget ui;
  StreamCaptureThread_SP capturethread;
  ProcessingThread_SP processingthread;
  QElapsedTimer AVI_StartTime;
  QTime AVI_EndTime;
  QTimer clock;
  int SnapshotId;
  QButtonGroup ImageButtonGroup;
  QButtonGroup RotateButtonGroup;
  CameraSelectorWidget* cameraSelectorWidget;
  // QButtonGroup ResolutionButtonGroup;
  // int numberOfResolutions;
  // int currentResolutionButtonIndex;
  // int previousResolutionButtonIndex;
  RenderWidget* renderWidget;
  int NumDevices;
  //
  bool faceRecognitionAcitve;
  int requestedFps;
  bool eyesRecognitionActive;
};

#endif
