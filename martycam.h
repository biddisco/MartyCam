#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include "ui_martycam.h"
#include "capturethread.h"
#include "processingthread.h"
#include <QMainWindow>

class TrackController;
class RenderWidget;
class QToolBar;
class QDockWidget;
class SettingsWidget;
    
class MartyCam : public QMainWindow {
Q_OBJECT
public:
    MartyCam();
public slots:
  void onResolutionSelected(CaptureThread::FrameSize newSize);
  void onCameraIndexChanged(int index);
  void onUserTrackChanged(int value);
  void startTracking();
  void stopTracking();
  void updateStats();
  void onRecordingStateChanged(bool state);
  //
protected:
  void closeEvent(QCloseEvent*);
private:
  Ui::MartyCam ui;
  RenderWidget            *renderWidget;
  QTimer                  *updateTimer;
  QDockWidget             *progressToolbar;
  QDockWidget             *settingsDock;
  SettingsWidget          *settingsWidget;
  CaptureThread           *captureThread;
  ProcessingThread        *processingThread;
  ImageBuffer             *imageBuffer;
  double                   UserDetectionThreshold;
  int                      RecordingEvents;
};

#endif
