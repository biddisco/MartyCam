// [UserCameras]
// 1\CameraName=MartyCamGarden
// 1\URL=rtsp://gardencam:gardencam@192.168.1.34:554/stream1
// size=1

#include "martycam/widgets/CameraSelectorWidget.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCameraDevice>
#include <QComboBox>
#include <QDebug>
#include <QEventLoop>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QMediaDevices>
#include <QProgressDialog>
#include <QRadioButton>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include <opencv2/core.hpp>

#include <string>
#include <unordered_map>
#include <vector>

#include "martycam/core/camera_utils.h"
#include "debug/logging.hpp"

// ----------------------------------------------------------------------------
static auto cam_log = martycam::log::create("Camera");

std::vector<std::pair<std::string, std::string>> load_ip_camerasettings()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  //
  std::vector<std::pair<std::string, std::string>> cameras;
  int size = settings.beginReadArray("UserCameras");
  for (int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);
    std::pair<std::string, std::string> camera;
    camera.first = settings.value("CameraName").toString().toLatin1().data();
    camera.second = settings.value("URL").toString().toLatin1().data();
    MARTY_LOG_INFO(
        cam_log, "{:<20} Camera: {}={}", "ip_camerasettings", camera.first, camera.second);
    cameras.push_back(camera);
  }
  settings.endArray();
  return cameras;
}

// ----------------------------------------------------------------------------
std::unordered_map<std::string, CameraConfig> load_cached_camera_configs()
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);
  std::unordered_map<std::string, CameraConfig> configs;

  int size = settings.beginReadArray("CachedCameraConfigs");
  for (int i = 0; i < size; ++i)
  {
    settings.setArrayIndex(i);
    std::string cameraName = settings.value("CameraName").toString().toStdString();
    std::string json = settings.value("ConfigJson").toString().toStdString();
    if (!cameraName.empty() && !json.empty())
    {
      try
      {
        CameraConfig config = CameraConfig::from_json(json);
        configs[cameraName] = std::move(config);
        MARTY_LOG_INFO(cam_log, "{:<20} Loaded cached config for {}", "CameraCache", cameraName);
      }
      catch (...)
      {
        MARTY_LOG_WARN(
            cam_log, "{:<20} Failed to parse cached config for {}", "CameraCache", cameraName);
      }
    }
  }
  settings.endArray();
  return configs;
}

// ----------------------------------------------------------------------------
void save_cached_camera_configs(std::vector<CameraConfig> const& configs)
{
  QString settingsFileName = QCoreApplication::applicationDirPath() + "/MartyCam.ini";
  QSettings settings(settingsFileName, QSettings::IniFormat);

  settings.beginWriteArray("CachedCameraConfigs");
  for (size_t i = 0; i < configs.size(); ++i)
  {
    settings.setArrayIndex(static_cast<int>(i));
    settings.setValue("CameraName", QString::fromStdString(configs[i].camera_name));
    settings.setValue("ConfigJson", QString::fromStdString(configs[i].to_json()));
  }
  settings.endArray();
}

// --------------------------------------------------------------------
// Constructor for the CameraSelectorWidget, initializing the UI and connecting signals for dynamic camera detection.
// --------------------------------------------------------------------
CameraSelectorWidget::CameraSelectorWidget(QWidget* parent)
  : QWidget(parent)
  , m_mainLayout(new QVBoxLayout(this))
  , m_camerasContainerLayout(new QVBoxLayout())
  , m_cameraComboBox(nullptr)
  , m_resolutionButtonsContainer(nullptr)
  , m_hasSelection(false)
  , m_selectedCameraPath("")
  , m_selectedResolution(0, 0)
  , m_selectedFps(30)
  , m_selectedFourCC(0)
  , m_isRefreshing(false)
  , m_refreshPending(false)
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
  // setMinimumWidth(450);
  m_mainLayout->addLayout(m_camerasContainerLayout);

  // deleted by Qt on close of this widget, no need to manage lifetime
  QMediaDevices* mediaDevices = new QMediaDevices(this);
  // when a new camera is plugged in or unplugged, rescan camera
  connect(
      mediaDevices, &QMediaDevices::videoInputsChanged, this, [this]() { refreshCameraList(); },
      Qt::QueuedConnection);

  // Trigger a scan of camera once the event processing loop is up and running
  // (MartyCam will initialize threads when the first cameraConfigChanged signal arrives.)
  QTimer::singleShot(0, this, &CameraSelectorWidget::refreshCameraList);
}

// --------------------------------------------------------------------
bool CameraSelectorWidget::hasSelection() const { return m_hasSelection; }

// --------------------------------------------------------------------
QString CameraSelectorWidget::selectedCameraPath() const
{
  return QString::fromStdString(m_selectedCameraPath);
}

// --------------------------------------------------------------------
camera_utils::cam_res CameraSelectorWidget::selectedResolution() const
{
  return m_selectedResolution;
}

// --------------------------------------------------------------------
int CameraSelectorWidget::selectedFps() const { return m_selectedFps; }

// --------------------------------------------------------------------
int CameraSelectorWidget::selectedFourCC() const { return m_selectedFourCC; }

// --------------------------------------------------------------------
void CameraSelectorWidget::clearCameraWidgets()
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);

  // Clear button groups first (before deleting the container that owns the buttons)
  for (auto& pair : m_cameraButtonGroups)
  {
    // Reparent buttons to nullptr so they don't get double-deleted
    for (QAbstractButton* button : pair.second->buttons()) { button->setParent(nullptr); }
    delete pair.second;
  }
  m_cameraButtonGroups.clear();

  // Now delete the layout items (container widgets)
  QLayoutItem* item = nullptr;
  while ((item = m_camerasContainerLayout->takeAt(0)) != nullptr)
  {
    if (item->widget()) { delete item->widget(); }
    delete item;
  }

  m_cameraComboBox = nullptr;
  m_resolutionButtonsContainer = nullptr;
}

//----------------------------------------------------------------------------
// Create a group of radio buttons for selecting camera resolutions
// by probing the camera capabilities via OpenCV
QButtonGroup* CameraSelectorWidget::createCameraResolutionGroup(QWidget* parent,
    std::string const& cameraName, std::string const& cameraPath,
    std::optional<CameraConfig> const& cachedConfig,
    std::function<void(int, int, int)> const& onProbeStepProgress)
{
  // button group to hold settings for this camera, will be returned to caller
  QButtonGroup* buttonGroup = new QButtonGroup(parent);
  buttonGroup->setExclusive(true);

  CameraConfig camera_config;
  if (cachedConfig.has_value())
  {
    camera_config = cachedConfig.value();
    MARTY_LOG_INFO(
        cam_log, "{:<20} Using cached config for {}", "CameraSelectorWidget", cameraName);
  }
  else { camera_config = camera_utils::ProbeCameraConfig(cameraName, cameraPath); }

  QString configJson = QString::fromStdString(camera_config.to_json());
  int resolutionIndex = 0;
  for (auto const& camRes : camera_config.resolutions)
  {
    QString res = QString::fromStdString(std::to_string(camRes.resolution.width) + " x " +
        std::to_string(camRes.resolution.height) + " [" + fourCCToString(camRes.fourcc) + "]");
    //
    QString text = res + (camRes.fpsList.size() > 1 ? " (" : " @");
    for (size_t i = 0; i < camRes.fpsList.size(); ++i)
    {
      text += QString::number(camRes.fpsList[i]);
      if (i < camRes.fpsList.size() - 1) { text += ", "; }
    }
    text += camRes.fpsList.size() > 1 ? " fps)" : " fps";

    QRadioButton* radioButton = new QRadioButton(text);
    radioButton->setProperty("camera_config_json", configJson);
    radioButton->setProperty("resolution_index", resolutionIndex);
    MARTY_LOG_INFO(cam_log, "{:<20} Adding resolution button: {} @ {} FPS", "CameraSelectorWidget",
        res.toStdString(),
        camRes.fpsList.size() > 1 ? "multiple" : std::to_string(camRes.fpsList[0]));
    buttonGroup->addButton(radioButton);
    ++resolutionIndex;
  }

  if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

  return buttonGroup;
}

// --------------------------------------------------------------------
// Scan for new/changed cameras and populate lists/widgets
// --------------------------------------------------------------------
void CameraSelectorWidget::refreshCameraList()
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
  if (m_isRefreshing)
  {
    m_refreshPending = true;
    return;
  }

  m_isRefreshing = true;
  m_refreshPending = false;

  MARTY_LOG_INFO(cam_log, "{:<20} Hardware modification detected! Refreshing camera grid...",
      "CameraSelectorWidget");

  QProgressDialog progressDialog("Querying Cameras", QString(), 0, 1, this);
  progressDialog.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
  progressDialog.setWindowModality(Qt::WindowModal);
  progressDialog.setCancelButton(nullptr);
  progressDialog.setAutoClose(false);
  progressDialog.setAutoReset(false);
  progressDialog.resize(420, 120);

  QRect const screenGeometry = QApplication::primaryScreen()->geometry();
  QRect const dialogGeometry = progressDialog.frameGeometry();
  progressDialog.move((screenGeometry.width() - dialogGeometry.width()) / 2,
      (screenGeometry.height() - dialogGeometry.height()) / 2);
  progressDialog.show();
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  clearCameraWidgets();

  std::vector<std::pair<std::string, std::string>> all_cameras;
  QList<QCameraDevice> const qcameras = QMediaDevices::videoInputs();
  for (int i = 0; i < qcameras.size(); ++i)
  {
    QString const cameraName = qcameras[i].description();
    QString const cameraPath = qcameras[i].id();
    all_cameras.push_back(std::make_pair(cameraName.toStdString(), cameraPath.toStdString()));
  }
  MARTY_LOG_INFO(cam_log, "{:<20} Loaded {} cameras from QMediaDevices.", "CameraSelectorWidget",
      qcameras.size());

  std::vector<std::pair<std::string, std::string>> ip_cameras = load_ip_camerasettings();
  all_cameras.insert(all_cameras.end(), ip_cameras.begin(), ip_cameras.end());
  MARTY_LOG_INFO(cam_log, "{:<20} Loaded {} IP cameras from settings.", "CameraSelectorWidget",
      ip_cameras.size());

  if (all_cameras.empty())
  {
    m_camerasContainerLayout->addWidget(new QLabel("No active video devices detected.", this));
    m_hasSelection = false;
    progressDialog.hide();

    m_isRefreshing = false;
    if (m_refreshPending)
    {
      m_refreshPending = false;
      QTimer::singleShot(0, this, &CameraSelectorWidget::refreshCameraList);
    }
    return;
  }

  // Use estimated progress maximum based on up to 2 probe codecs (MJPEG + GREY) per camera.
  int totalGlobalProbeSteps = static_cast<int>(all_cameras.size()) *
      static_cast<int>(camera_utils::testResolutions.size()) * camera_utils::testFPS.size() * 2;
  if (totalGlobalProbeSteps <= 0) totalGlobalProbeSteps = 1;

  // init progress bar
  progressDialog.setRange(0, totalGlobalProbeSteps > 0 ? totalGlobalProbeSteps : 1);
  progressDialog.setValue(0);
  progressDialog.setLabelText(QString("Querying Cameras (%1 detected)").arg(all_cameras.size()));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_cameraComboBox = new QComboBox(this);
  m_resolutionButtonsContainer = new QWidget(this);
  QVBoxLayout* containerLayout = new QVBoxLayout(m_resolutionButtonsContainer);
  containerLayout->setContentsMargins(0, 0, 0, 0);

  bool hasUsableCamera = false;
  int currentGlobalProgressCounter = 0;
  std::vector<CameraConfig> probedConfigs;
  auto cachedConfigs = load_cached_camera_configs();

  for (int i = 0; i < all_cameras.size(); ++i)
  {
    QString const cameraName = QString::fromStdString(all_cameras[i].first);
    QString const cameraPath = QString::fromStdString(all_cameras[i].second);
    std::string const cameraPathStr = cameraPath.toStdString();
    MARTY_LOG_INFO(cam_log, "{:<20} Probing camera {}: {}", "CameraSelectorWidget",
        all_cameras[i].first, all_cameras[i].second);

    progressDialog.setLabelText(QString("Querying Cameras: %1").arg(cameraName));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    std::optional<CameraConfig> cachedConfig;
    auto cacheIt = cachedConfigs.find(cameraName.toStdString());
    if (cacheIt != cachedConfigs.end()) { cachedConfig = cacheIt->second; }

    QButtonGroup* resGroup =
        createCameraResolutionGroup(m_resolutionButtonsContainer, cameraName.toStdString(),
            cameraPathStr, cachedConfig, [&](int probeWidth, int probeHeight, int probeFps) {
              ++currentGlobalProgressCounter;
              progressDialog.setLabelText(
                  QString("Querying Cameras: %1/%2 - %3\nTesting %4x%5 @ %6 FPS")
                      .arg(i + 1)
                      .arg(all_cameras.size())
                      .arg(cameraName)
                      .arg(probeWidth)
                      .arg(probeHeight)
                      .arg(probeFps));
              if (currentGlobalProgressCounter > progressDialog.maximum())
              {
                progressDialog.setMaximum(currentGlobalProgressCounter);
              }
              progressDialog.setValue(currentGlobalProgressCounter);
              QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            });

    if (resGroup->buttons().isEmpty())
    {
      delete resGroup;
      continue;
    }

    // when a resolution button is clicked, emit the config changed signal with the selected camera path, resolution, fps, and fourcc
    connect(
        resGroup, &QButtonGroup::buttonClicked, this,
        [this, resGroup](QAbstractButton* button) {
          MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
          Q_UNUSED(button);
          emitConfigChanged();
        },
        Qt::QueuedConnection);

    m_cameraComboBox->addItem(cameraName, QString::fromStdString(cameraPathStr));
    m_cameraButtonGroups[cameraPathStr] = resGroup;
    hasUsableCamera = true;

    // Collect config for caching (from first button's property)
    if (!resGroup->buttons().isEmpty())
    {
      QString configJson = resGroup->buttons().first()->property("camera_config_json").toString();
      try
      {
        CameraConfig config = CameraConfig::from_json(configJson.toStdString());
        probedConfigs.push_back(std::move(config));
      }
      catch (...)
      {
      }
    }
  }

  save_cached_camera_configs(probedConfigs);

  if (hasUsableCamera)
  {
    m_camerasContainerLayout->addWidget(m_cameraComboBox);
    m_camerasContainerLayout->addWidget(m_resolutionButtonsContainer);
    m_camerasContainerLayout->addStretch();

    connect(m_cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &CameraSelectorWidget::onCameraComboBoxChanged, Qt::QueuedConnection);

    // Restore previous selection or default to first camera
    restoreSelection();
  }
  else
  {
    delete m_cameraComboBox;
    delete m_resolutionButtonsContainer;
    m_cameraComboBox = nullptr;
    m_resolutionButtonsContainer = nullptr;
    m_camerasContainerLayout->addWidget(
        new QLabel("No cameras with supported resolutions were detected.", this));
    m_hasSelection = false;
  }

  progressDialog.setValue(progressDialog.maximum());
  progressDialog.hide();

  m_isRefreshing = false;
  if (m_refreshPending)
  {
    m_refreshPending = false;
    QTimer::singleShot(0, this, &CameraSelectorWidget::refreshCameraList);
  }
}

// --------------------------------------------------------------------
void CameraSelectorWidget::saveCameraConfig()
{
  QSettings settings(
      QCoreApplication::applicationDirPath() + "/MartyCam.ini", QSettings::IniFormat);
  settings.beginWriteArray("UserCameras");
  int i = 0;
  for (auto const& camera : load_ip_camerasettings())
  {
    settings.setArrayIndex(i++);
    settings.setValue("CameraName", QString::fromStdString(camera.first));
    settings.setValue("URL", QString::fromStdString(camera.second));
  }
  settings.endArray();
}

// --------------------------------------------------------------------
void CameraSelectorWidget::saveSelection()
{
  QSettings settings(
      QCoreApplication::applicationDirPath() + "/MartyCam.ini", QSettings::IniFormat);
  settings.beginGroup("CameraSelection");
  settings.setValue("cameraPath", QString::fromStdString(m_selectedCameraPath));

  // Save the actual resolution radio-button index, not the combo-box camera index.
  int resolutionIndex = 0;
  if (m_cameraComboBox)
  {
    std::string cameraPath = m_cameraComboBox->currentData().toString().toStdString();
    auto it = m_cameraButtonGroups.find(cameraPath);
    if (it != m_cameraButtonGroups.end())
    {
      QAbstractButton* checkedButton = it->second->checkedButton();
      if (checkedButton)
      {
        resolutionIndex = checkedButton->property("resolution_index").toInt();
      }
    }
  }
  settings.setValue("resolutionIndex", resolutionIndex);
  settings.endGroup();
}

// --------------------------------------------------------------------
void CameraSelectorWidget::restoreSelection()
{
  if (!m_cameraComboBox || m_cameraButtonGroups.empty()) { return; }

  QSettings settings(
      QCoreApplication::applicationDirPath() + "/MartyCam.ini", QSettings::IniFormat);
  settings.beginGroup("CameraSelection");
  QString savedPath = settings.value("cameraPath").toString();
  int savedResolutionIndex = settings.value("resolutionIndex", 0).toInt();
  settings.endGroup();

  if (savedPath.isEmpty()) { return; }

  // Find the combo box index for the saved camera path
  int cameraIndex = -1;
  for (int i = 0; i < m_cameraComboBox->count(); ++i)
  {
    if (m_cameraComboBox->itemData(i).toString() == savedPath)
    {
      cameraIndex = i;
      break;
    }
  }

  if (cameraIndex < 0) { return; }

  // Block signals to prevent premature emission while restoring
  bool oldState = m_cameraComboBox->blockSignals(true);
  m_cameraComboBox->setCurrentIndex(cameraIndex);
  m_cameraComboBox->blockSignals(oldState);

  // Update the resolution buttons for this camera
  onCameraComboBoxChanged(cameraIndex);

  // Find the button group for this camera and check the saved resolution
  auto it = m_cameraButtonGroups.find(savedPath.toStdString());
  if (it != m_cameraButtonGroups.end())
  {
    QButtonGroup* buttonGroup = it->second;
    QAbstractButton* targetButton = nullptr;
    for (QAbstractButton* button : buttonGroup->buttons())
    {
      int resIdx = button->property("resolution_index").toInt();
      if (resIdx == savedResolutionIndex)
      {
        targetButton = button;
        break;
      }
    }
    if (targetButton)
    {
      qobject_cast<QRadioButton*>(targetButton)->setChecked(true);
    }
    else if (!buttonGroup->buttons().isEmpty())
    {
      // Fallback to the first resolution if the saved index no longer exists
      qobject_cast<QRadioButton*>(buttonGroup->buttons().first())->setChecked(true);
    }
  }

  // Now that the UI is fully consistent, emit the actual selection so the
  // capture thread is created with the correct camera + resolution.
  emitConfigChanged();
}

void CameraSelectorWidget::onCameraComboBoxChanged(int index)
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
  Q_UNUSED(index);

  if (!m_cameraComboBox || !m_resolutionButtonsContainer) { return; }

  std::string cameraPath = m_cameraComboBox->currentData().toString().toStdString();
  auto it = m_cameraButtonGroups.find(cameraPath);
  if (it == m_cameraButtonGroups.end()) { return; }

  QButtonGroup* buttonGroup = it->second;

  // Hide all buttons from all groups
  for (auto const& pair : m_cameraButtonGroups)
  {
    for (QAbstractButton* button : pair.second->buttons()) { button->hide(); }
  }

  // Clear previous buttons from container
  QVBoxLayout* containerLayout = qobject_cast<QVBoxLayout*>(m_resolutionButtonsContainer->layout());
  if (!containerLayout)
  {
    containerLayout = new QVBoxLayout(m_resolutionButtonsContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
  }

  QLayoutItem* item = nullptr;
  while ((item = containerLayout->takeAt(0)) != nullptr) { delete item; }

  // Add and show buttons from the selected camera's group
  for (QAbstractButton* button : buttonGroup->buttons())
  {
    containerLayout->addWidget(button);
    button->show();
  }
  containerLayout->addStretch();
}

// --------------------------------------------------------------------
void CameraSelectorWidget::emitConfigChanged()
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
  if (!m_cameraComboBox || !m_resolutionButtonsContainer)
  {
    m_hasSelection = false;
    return;
  }

  std::string cameraPath = m_cameraComboBox->currentData().toString().toStdString();
  auto it = m_cameraButtonGroups.find(cameraPath);
  if (it == m_cameraButtonGroups.end())
  {
    m_hasSelection = false;
    return;
  }

  QButtonGroup* buttonGroup = it->second;
  QAbstractButton* checkedButton = buttonGroup->checkedButton();

  if (!checkedButton && !buttonGroup->buttons().isEmpty())
  {
    checkedButton = buttonGroup->buttons().first();
    if (qobject_cast<QRadioButton*>(checkedButton))
    {
      qobject_cast<QRadioButton*>(checkedButton)->setChecked(true);
    }
  }

  if (!checkedButton)
  {
    m_hasSelection = false;
    return;
  }

  QString configJson = checkedButton->property("camera_config_json").toString();
  int resolutionIndex = checkedButton->property("resolution_index").toInt();

  CameraConfig config = CameraConfig::from_json(configJson.toStdString());
  if (resolutionIndex < 0 || resolutionIndex >= static_cast<int>(config.resolutions.size()))
  {
    m_hasSelection = false;
    return;
  }

  camera_resolution const& camRes = config.resolutions[resolutionIndex];
  m_selectedCameraPath = config.camera_path;
  m_selectedResolution = camRes.resolution;
  m_selectedFourCC = camRes.fourcc;
  m_selectedFps = camRes.fpsList.empty() ? 30 : camRes.fpsList[0];
  m_hasSelection = (m_selectedResolution.width > 0 && m_selectedResolution.height > 0);

  MARTY_LOG_INFO(cam_log, "{:<20} Selection changed: path={}, resolution={}x{}, fps={}, fourcc={}",
      "CameraSelectorWidget", m_selectedCameraPath, m_selectedResolution.width,
      m_selectedResolution.height, m_selectedFps, ::fourCCToString(m_selectedFourCC));
  if (!m_hasSelection) { return; }

  emit cameraConfigChanged(QString::fromStdString(m_selectedCameraPath), m_selectedResolution.width,
      m_selectedResolution.height, m_selectedFps, m_selectedFourCC);
}
