#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include <hpx/config.hpp>
#include <hpx/execution/execution.hpp>

#include <QDateTime>
#include <QDebug>
#include <QMainWindow>
//
#include <opencv2/core/core.hpp>
//
#include "capturethread.h"
#include "processingthread.h"
#include "renderwidget.h"
#include "settings.h"
#include "ui_martycam.h"
//
#include <boost/make_shared.hpp>

class RenderWidget;
class QDockWidget;
class SettingsWidget;

typedef std::shared_ptr<QDockWidget> QDockWidget_SP;

class MartyCam : public QMainWindow
{
  Q_OBJECT
  public:
  MartyCam(hpx::execution::parallel_executor const&, hpx::execution::parallel_executor const&);

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
  void onMouseDoubleClickEvent(QPoint const&);

  protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(cv::Size& size, int camera, std::string const& cameraname,
      hpx::execution::parallel_executor exec);
  void deleteProcessingThread();
  void createProcessingThread(ProcessingThread* oldThread, hpx::execution::parallel_executor exec,
      ProcessingType processingType);

  void resetChart();
  void initChart();

  private:
  public:
  static int const IMAGE_BUFF_CAPACITY;
  Ui::MartyCam ui;

  CaptureThread_SP captureThread;
  ProcessingThread_SP processingThread;

  RenderWidget_SP renderWidget;
  QDockWidget_SP settingsDock;
  SettingsWidget_SP settingsWidget;
  int cameraIndex;

  ImageBuffer imageBuffer;

  hpx::execution::parallel_executor blockingExecutor;
  hpx::execution::parallel_executor defaultExecutor;

  QDockWidget* progressToolbar;
  cv::Size imageSize;
  double UserDetectionThreshold;
  int EventRecordCounter;
  int insideMotionEvent;
  QDateTime lastTimeLapse;
};

#endif
