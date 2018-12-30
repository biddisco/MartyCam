#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include <hpx/config.hpp>
#include <hpx/parallel/execution.hpp>

#include <QMainWindow>
#include <QDateTime>
#include <QDebug>
//
#include <opencv2/core/core.hpp>
//
#include "ui_martycam.h"
#include "capturethread.h"
#include "renderwidget.h"
#include "processingthread.h"
#include "settings.h"
//
#include <boost/make_shared.hpp>

class RenderWidget;
class QDockWidget;
class SettingsWidget;

typedef std::shared_ptr<QDockWidget> QDockWidget_SP;

class MartyCam : public QMainWindow {
Q_OBJECT
public:
  MartyCam(const hpx::threads::executors::pool_executor&, const hpx::threads::executors::pool_executor&);

  void loadSettings();
  void saveSettings();
  void clearGraphs();

public slots:
  void updateGUI();
  void onResolutionSelected(cv::Size newSize);
  void onRotationChanged(int rotation);
  void onCameraIndexChanged(int index, QString URL);
  void onUserTrackChanged(int value);
  void onRecordingStateChanged(bool state);
  //
  void onMouseDoubleClickEvent(const QPoint&);

protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(cv::Size &size, int camera, const std::string &cameraname,
                           hpx::threads::executors::pool_executor exec);
  void deleteProcessingThread();
  void createProcessingThread(ProcessingThread *oldThread, hpx::threads::executors::pool_executor exec,
                              ProcessingType processingType);

  void resetChart();
  void initChart();

private:
public:
  static const int IMAGE_BUFF_CAPACITY;
  Ui::MartyCam ui;

  CaptureThread_SP captureThread;
  ProcessingThread_SP processingThread;

  RenderWidget_SP          renderWidget;
  QDockWidget_SP           settingsDock;
  SettingsWidget_SP        settingsWidget;
  int                      cameraIndex;

  ImageBuffer              imageBuffer;

  hpx::threads::executors::pool_executor blockingExecutor;
  hpx::threads::executors::pool_executor defaultExecutor;

  QDockWidget             *progressToolbar;
  cv::Size                 imageSize;
  double                   UserDetectionThreshold;
  int                      EventRecordCounter;
  int                      insideMotionEvent;
  QDateTime                lastTimeLapse;

};

#endif
