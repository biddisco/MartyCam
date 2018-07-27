#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include <hpx/config.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/parallel/execution.hpp>

#include <QMainWindow>
#include <QTimer>
#include <QDateTime>
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
    MartyCam(hpx::threads::executors::pool_executor, hpx::threads::executors::pool_executor);

  void loadSettings();
  void saveSettings();
  void clearGraphs();

public slots:
  void updateGUI();
  void onResolutionSelected(cv::Size newSize);
  void onRotationChanged(int rotation);
  void onCameraIndexChanged(int index, QString URL);
  void onUserTrackChanged(int value);

protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(int FPS, cv::Size &size, int camera,
                           const std::string &cameraname, hpx::threads::executors::pool_executor exec);
  void deleteProcessingThread();
  ProcessingThread *createProcessingThread(cv::Size &size, ProcessingThread *oldThread);

  void resetChart();
  void initChart();

private:
  static const int IMAGE_BUFF_CAPACITY;
  Ui::MartyCam ui;
  RenderWidget            *renderWidget;
  QDockWidget             *progressToolbar;
  QDockWidget             *settingsDock;
  SettingsWidget          *settingsWidget;
  cv::Size                 imageSize;
  int                      cameraIndex;
  CaptureThread           *captureThread;
  ProcessingThread        *processingThread;
  ImageBuffer              imageBuffer;
  double                   UserDetectionThreshold;
  int                      EventRecordCounter;
  int                      insideMotionEvent;
  QDateTime                lastTimeLapse;
  hpx::threads::executors::pool_executor blockingExecutor;
  hpx::threads::executors::pool_executor defaultExecutor;
};

#endif
