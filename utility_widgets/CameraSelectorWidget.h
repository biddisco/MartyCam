#ifndef CAMERA_SELECTOR_WIDGET_H
#define CAMERA_SELECTOR_WIDGET_H

#include <QWidget>
//
#include <optional>
#include <string>
#include <unordered_map>
//
class QAbstractButton;
class QButtonGroup;
class QComboBox;
class QVBoxLayout;
class QWidget;
class IPCameraForm;
//
#include "utility_widgets/camera_utils.h"

class CameraSelectorWidget : public QWidget
{
  Q_OBJECT

  public:
  explicit CameraSelectorWidget(QWidget* parent = nullptr);

  bool hasSelection() const;
  QString selectedCameraPath() const;
  camera_utils::cam_res selectedResolution() const;
  int selectedFps() const;
  int selectedFourCC() const;

  QButtonGroup* createCameraResolutionGroup(QWidget* parent, std::string const& cameraName,
      std::string const& cameraPath, std::optional<CameraConfig> const& cachedConfig,
      std::function<void(int, int, int)> const& onProbeStepProgress);

  public slots:
  void refreshCameraList();
  void saveCameraConfig();

  signals:
  // we do not emit a cv::Size as that is not a Qt type, instead we emit width and height separately
  void cameraConfigChanged(QString cameraPath, int width, int height, int fps, int fourcc);

  private slots:
  void onCameraComboBoxChanged(int index);

  private:
  void clearCameraWidgets();
  void emitConfigChanged();

  QVBoxLayout* m_mainLayout;
  QVBoxLayout* m_camerasContainerLayout;
  QComboBox* m_cameraComboBox;
  QWidget* m_resolutionButtonsContainer;
  std::unordered_map<std::string, QButtonGroup*> m_cameraButtonGroups;

  IPCameraForm* m_cameraForm;

  bool m_hasSelection;
  std::string m_selectedCameraPath;
  camera_utils::cam_res m_selectedResolution;
  int m_selectedFps;
  int m_selectedFourCC;
  bool m_isRefreshing;
  bool m_refreshPending;
};

#endif
