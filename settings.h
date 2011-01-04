#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui_settings.h"
#include "capturethread.h"
#include <QDateTime>
#include <QTimer>

class SettingsWidget : public QWidget {
Q_OBJECT;
public:
  SettingsWidget(QWidget* parent, CaptureThread *capthread);
  CaptureThread::FrameSize getSelectedResolution();
  void RecordAVI(bool state);

public slots:
	void on640ResToggled(bool on);
	void on320ResToggled(bool on);
	void onVerticalFlipStateChanged(int state);
  void onThresholdChanged(int value);
  void onAverageChanged(int value);
  void onErodeBlockChanged(int value);
  void onBrowseClicked();
  void onWriteAVIToggled(bool state);
  void onTimer();

signals:
	void resolutionSelected(CaptureThread::FrameSize);
	void flipVerticalChanged(bool);
	void thresholdChanged(int);
  void averageChanged(double);
  void erodeBlockChanged(int);	

private:
	Ui::SettingsWidget ui;
	CaptureThread *capturethread;
  QTime   AVI_StartTime;
  QTime   AVI_EndTime;
  QTimer  clock;
};

#endif
