#ifndef CAMERA_SELECTOR_WIDGET_H
#define CAMERA_SELECTOR_WIDGET_H

#include <QWidget>

#include <opencv2/core.hpp>

class QAbstractButton;
class QTabWidget;
class QVBoxLayout;

std::string fourCCToString(int fourcc);

class CameraSelectorWidget : public QWidget
{
  Q_OBJECT

  public:
  explicit CameraSelectorWidget(QWidget* parent = nullptr);

  bool hasSelection() const;
  int selectedCameraIndex() const;
  cv::Size selectedResolution() const;
  int selectedFps() const;
  int selectedFourCC() const;

  public slots:
  void refreshCameraList();

  signals:
  void cameraConfigChanged(int cameraIndex, cv::Size resolution, int fps, int fourcc);

  private slots:
  void onResolutionSelected(QAbstractButton* button);
  void onCurrentTabChanged(int index);

  private:
  void clearCameraWidgets();
  void emitCurrentSelection();

  QVBoxLayout* m_mainLayout;
  QVBoxLayout* m_camerasContainerLayout;
  QTabWidget* m_cameraTabs;

  bool m_hasSelection;
  int m_selectedCameraIndex;
  cv::Size m_selectedResolution;
  int m_selectedFps;
  int m_selectedFourCC;
  bool m_isRefreshing;
  bool m_refreshPending;
};

#endif
