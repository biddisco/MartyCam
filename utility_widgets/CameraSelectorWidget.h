#ifndef CAMERA_SELECTOR_WIDGET_H
#define CAMERA_SELECTOR_WIDGET_H

#include <QWidget>

#include <opencv2/core.hpp>

#include <unordered_map>

class QAbstractButton;
class QButtonGroup;
class QComboBox;
class QVBoxLayout;
class QWidget;

std::string fourCCToString(int fourcc);

class CameraSelectorWidget : public QWidget
{
  Q_OBJECT

  public:
  explicit CameraSelectorWidget(QWidget* parent = nullptr);

  bool hasSelection() const;
  QString selectedCameraPath() const;
  cv::Size selectedResolution() const;
  int selectedFps() const;
  int selectedFourCC() const;

  public slots:
  void refreshCameraList();

  signals:
  void cameraConfigChanged(QString cameraPath, cv::Size resolution, int fps, int fourcc);

  private slots:
  void onResolutionSelected(QAbstractButton* button);
  void onCameraComboBoxChanged(int index);

  private:
  void clearCameraWidgets();
  void emitCurrentSelection();

  QVBoxLayout* m_mainLayout;
  QVBoxLayout* m_camerasContainerLayout;
  QComboBox* m_cameraComboBox;
  QWidget* m_resolutionButtonsContainer;
  std::unordered_map<std::string, QButtonGroup*> m_cameraButtonGroups;

  bool m_hasSelection;
  std::string m_selectedCameraPath;
  cv::Size m_selectedResolution;
  int m_selectedFps;
  int m_selectedFourCC;
  bool m_isRefreshing;
  bool m_refreshPending;
};

#endif
