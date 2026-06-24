#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include <hpx/config.hpp>
#include <hpx/execution/execution.hpp>

#include <QDateTime>
#include <QDebug>
#include <QMainWindow>
#include <QShowEvent>
//
#include <memory>
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
  void onRotationChanged(int rotation);
  void onUserTrackChanged(int value);
  void onRecordingStateChanged(bool state);
  //
  void onMouseDoubleClickEvent(QPoint const&);
  void onCameraConfigChanged(QString cameraPath, int width, int height, int fps, int fourcc);

  protected:
  void closeEvent(QCloseEvent*);
  void deleteCaptureThread();
  void createCaptureThread(cv::Size size, std::string const& cameraname, int fps, int fourcc,
      hpx::execution::parallel_executor exec);
  void deleteProcessingThread();
  void createProcessingThread(ProcessingThread* oldThread, hpx::execution::parallel_executor exec,
      ProcessingType processingType);

  void resetChart();
  void initChart();

  protected:
  // Override the showEvent method
  void showEvent(QShowEvent* event) override;

  private:
  private:
  public:
  static int const IMAGE_BUFF_CAPACITY;
  Ui::MartyCam ui;

  // The tracking flag, initialized to false
  bool m_isFirstShow = false;

  CaptureThread_SP captureThread;
  ProcessingThread_SP processingThread;

  RenderWidget_SP renderWidget;
  QDockWidget_SP settingsDock;
  SettingsWidget_SP settingsWidget;

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
