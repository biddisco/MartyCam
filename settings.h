#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui_settings.h"
#include "capturethread.h"
#include "processingthread.h"
#include <QDateTime>
#include <QTimer>
#include <QButtonGroup>

class SettingsWidget : public QWidget {
Q_OBJECT;
public:
  SettingsWidget(QWidget* parent, CaptureThread *capthread, ProcessingThread *procthread);
  CaptureThread::FrameSize getSelectedResolution();
  void RecordAVI(bool state);

public slots:
  void on640ResToggled(bool on);
  void on320ResToggled(bool on);
  void onVerticalFlipStateChanged(int state);
  void onThresholdChanged(int value);
  void onAverageChanged(int value);
  void onErodeChanged(int value);
  void onDilateChanged(int value);  
  void onBrowseClicked();
  void onWriteAVIToggled(bool state);
  void onTimer();
  void onImageSelection(int btn);
  void onBlendChanged(int value);

signals:
  void resolutionSelected(CaptureThread::FrameSize);

private:
  Ui::SettingsWidget  ui;
  CaptureThread      *capturethread;
  ProcessingThread   *processingthread;
  QTime               AVI_StartTime;
  QTime               AVI_EndTime;
  QTimer              clock;
  QButtonGroup        ImageButtonGroup;
};

#endif
