#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include "ui_martycam.h"
#include "capturethread.h"
#include "processingthread.h"
#include <QMainWindow>
#include <QTimer>

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
  void updateStats();
  void onResolutionSelected(CvSize newSize);
  void onCameraIndexChanged(int index);
  void onUserTrackChanged(int value);
  void onRecordingStateChanged(bool state);
  //
protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(int FPS, CvSize &size, int camera);
  void deleteProcessingThread();
  void createProcessingThread(CvSize &size);

private:
  Ui::MartyCam ui;
  RenderWidget            *renderWidget;
  QTimer                   updateTimer;
  QDockWidget             *progressToolbar;
  QDockWidget             *settingsDock;
  SettingsWidget          *settingsWidget;
  CvSize                   imageSize;
  int                      cameraIndex;
  CaptureThread           *captureThread;
  ProcessingThread        *processingThread;
  ImageBuffer             *imageBuffer;
  double                   UserDetectionThreshold;
  int                      RecordingEvents;
};

#endif
