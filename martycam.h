#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include <QMainWindow>
#include <QTimer>
//
#include <opencv2/core/core.hpp>
//
#include "ui_martycam.h"
#include "capturethread.h"
#include "processingthread.h"

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
  void clearGraphs();

public slots:
  void updateGUI();
  void onResolutionSelected(cv::Size newSize);
  void onCameraIndexChanged(int index, QString URL);
  void onUserTrackChanged(int value);
  void onRecordingStateChanged(bool state);
  //
  void onMouseDoubleClickEvent(const QPoint&);

protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(int FPS, cv::Size &size, int camera, QString &cameraname);
  void deleteProcessingThread();
  ProcessingThread *createProcessingThread(cv::Size &size, ProcessingThread *oldThread);
  
  void initChart();
  bool eventDecision();

private:
  Ui::MartyCam ui;
  RenderWidget            *renderWidget;
  QDockWidget             *progressToolbar;
  QDockWidget             *settingsDock;
  SettingsWidget          *settingsWidget;
  cv::Size                 imageSize;
  int                      cameraIndex;
  CaptureThread           *captureThread;
  ProcessingThread        *processingThread;
  ImageBuffer             *imageBuffer;
  double                   UserDetectionThreshold;
  int                      EventRecordCounter;
  int                      insideMotionEvent;
};

#endif
