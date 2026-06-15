// [UserCameras]
// 1\CameraName=MartyCamGarden
// 1\URL=rtsp://gardencam:gardencam@192.168.1.34:554/stream1
// size=1

#include "CameraSelectorWidget.h"

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

#include "camera_utils.h"
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
camera_utils::cam_res CameraSelectorWidget::selectedResolution() const { return m_selectedResolution; }

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

  std::vector<std::pair<std::string, std::string>> ip_cameras = load_ip_camerasettings();
  MARTY_LOG_INFO(cam_log, "{:<20} Loaded {} IP cameras from settings.", "CameraSelectorWidget",
      ip_cameras.size());
  std::vector<std::pair<std::string, std::string>> all_cameras = ip_cameras;
  QList<QCameraDevice> const qcameras = QMediaDevices::videoInputs();
  for (int i = 0; i < qcameras.size(); ++i)
  {
    QString const cameraName = qcameras[i].description();
    QString const cameraPath = qcameras[i].id();
    all_cameras.push_back(std::make_pair(cameraName.toStdString(), cameraPath.toStdString()));
  }
  MARTY_LOG_INFO(cam_log, "{:<20} Loaded {} cameras from QMediaDevices.", "CameraSelectorWidget",
      qcameras.size());

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

  for (int i = 0; i < all_cameras.size(); ++i)
  {
    QString const cameraName = QString::fromStdString(all_cameras[i].first);
    QString const cameraPath = QString::fromStdString(all_cameras[i].second);
    std::string const cameraPathStr = cameraPath.toStdString();
    MARTY_LOG_INFO(cam_log, "{:<20} Probing camera {}: {}", "CameraSelectorWidget",
        all_cameras[i].first, all_cameras[i].second);

    progressDialog.setLabelText(QString("Querying Cameras: %1").arg(cameraName));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QButtonGroup* resGroup = camera_utils::createCameraResolutionGroup(m_resolutionButtonsContainer,
        cameraPathStr, [&](int probeWidth, int probeHeight, int probeFps) {
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
  }

  if (hasUsableCamera)
  {
    m_camerasContainerLayout->addWidget(m_cameraComboBox);
    m_camerasContainerLayout->addWidget(m_resolutionButtonsContainer);
    m_camerasContainerLayout->addStretch();

    connect(m_cameraComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &CameraSelectorWidget::onCameraComboBoxChanged, Qt::QueuedConnection);

    // Trigger initial population of resolution buttons
    onCameraComboBoxChanged(0);
    emitConfigChanged();
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

  auto res = checkedButton->property("supported_resolution").toString();
  m_selectedResolution = camera_utils::parseResolution(res);
  m_selectedCameraPath = checkedButton->property("camera_path").toString().toStdString();
  m_selectedFourCC = checkedButton->property("supported_fourcc").toInt();
  auto fpslist = checkedButton->property("supported_fps").toList();
  m_selectedFps = fpslist[0].toInt();
  m_hasSelection = (m_selectedResolution.width > 0 && m_selectedResolution.height > 0);

  MARTY_LOG_INFO(cam_log, "{:<20} Selection changed: path={}, resolution={}x{}, fps={}, fourcc={}",
      "CameraSelectorWidget", m_selectedCameraPath, m_selectedResolution.width,
      m_selectedResolution.height, m_selectedFps, ::fourCCToString(m_selectedFourCC));
  if (!m_hasSelection) { return; }

  emit cameraConfigChanged(QString::fromStdString(m_selectedCameraPath), m_selectedResolution.width, m_selectedResolution.height,
      m_selectedFps, m_selectedFourCC);
}
