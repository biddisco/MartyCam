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

  void loadSettings();
  void saveSettings();

public slots:
  void updateStats();
  void onResolutionSelected(CvSize newSize);
  void onCameraIndexChanged(int index, QString URL);
  void onUserTrackChanged(int value);
  void onRecordingStateChanged(bool state);
  void onMotionDetectionChanged(int state);
  //
protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(int FPS, CvSize &size, int camera, QString &cameraname);
  void deleteProcessingThread();
  ProcessingThread *MartyCam::createProcessingThread(CvSize &size, ProcessingThread *oldThread);
  
  void initChart();
  bool eventDecision();

private:
  Ui::MartyCam ui;
  RenderWidget            *renderWidget;
  QDockWidget             *progressToolbar;
  QDockWidget             *settingsDock;
  SettingsWidget          *settingsWidget;
  CvSize                   imageSize;
  int                      cameraIndex;
  CaptureThread           *captureThread;
  ProcessingThread        *processingThread;
  ImageBuffer             *imageBuffer;
  double                   UserDetectionThreshold;
  int                      EventRecordCounter;
  int                      insideMotionEvent;
};

#endif
