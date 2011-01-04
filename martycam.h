#ifndef HEAD_TRACKER_H
#define HEAD_TRACKER_H

#include "ui_martycam.h"
#include "capturethread.h"
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
	void onFlipVerticalChanged(bool flip);
	void onResolutionSelected(CaptureThread::FrameSize newSize);
  void onThresholdChanged(int value);
  void onAverageChanged(double value);
  void onErodeBlockChanged(int value);
  void onUserTrackChanged(int value);
	void startTracking();
	void stopTracking();
	void startRecording();
	void updateStats();
  void onRecordingStateChanged(bool state);
  //
protected:
	void closeEvent(QCloseEvent*);
private:
  Ui::MartyCam ui;
	TrackController *trackController;
	RenderWidget    *renderWidget;
	QTimer          *updateTimer;
  QDockWidget     *progressToolbar;
	QDockWidget     *settingsDock;
	SettingsWidget  *settingsWidget;
  double           UserDetectionThreshold;
};

#endif
