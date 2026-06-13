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
#include <QTimer>
#include <QVBoxLayout>
#include <QVariant>

#include <opencv2/videoio.hpp>

#include <functional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __linux__
# include <cstring>
# include <fcntl.h>
# include <linux/videodev2.h>
# include <sys/ioctl.h>
# include <unistd.h>
#endif

#include "debug/logging.hpp"

// ----------------------------------------------------------------------------
static auto cam_log = martycam::log::create("Camera");

// --------------------------------------------------------------------
// Convenience function to convert FOURCC int to string for debugging
// --------------------------------------------------------------------
std::string fourCCToString(int fourcc)
{
  std::string code;
  code += static_cast<char>(fourcc & 0xFF);            // 1st byte
  code += static_cast<char>((fourcc >> 8) & 0xFF);     // 2nd byte
  code += static_cast<char>((fourcc >> 16) & 0xFF);    // 3rd byte
  code += static_cast<char>((fourcc >> 24) & 0xFF);    // 4th byte
  return code;
}

namespace {
  struct ResCheck
  {
    int width;
    int height;
  };

  struct ResolutionConfig
  {
    int fourcc;
    QVariantList fpsList;
  };

  // List of FPS to test when probing cameras
  std::vector<int> testFPS = {15, 30, 60};

  // Common resolutions to test when probing cameras
  std::vector<ResCheck> const testResolutions = {    //
      {320, 240},                                    // QVGA
      {640, 480},                                    // VGA
      {640, 360},                                    // IR camera friendly
      {800, 600},                                    // SVGA
      {1024, 768},                                   // XGA
      {1280, 720},                                   // HD
      {1920, 1080},                                  // 1080p
      {2560, 1440},                                  // QHD
      {3840, 2160}};                                 // 4K UHD

  // --------------------------------------------------------------------
  // get the number from the end of a qt camera device string like "/dev/video0"
  // --------------------------------------------------------------------
  int parseActualCameraIndex(QString const& qtDeviceId, int fallbackIndex)
  {
    std::string idStr = qtDeviceId.toStdString();
    std::regex videoRegex("video(\\d+)");
    std::smatch match;

    if (std::regex_search(idStr, match, videoRegex)) { return std::stoi(match[1].str()); }
    return fallbackIndex;
  }

  // --------------------------------------------------------------------
  // probe the camera for supported FOURCC codes
  // --------------------------------------------------------------------
  std::vector<int> getSupportedFourCCs(std::string device_url)
  {
    std::vector<int> supportedCodes;

#ifdef __linux__

    int fd = open(device_url.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) { return supportedCodes; }

    v4l2_fmtdesc fmtdesc;
    std::memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    int enumGuard = 0;
    int lastPixelFormat = -1;
    while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
      int pixelFormat = static_cast<int>(fmtdesc.pixelformat);
      supportedCodes.push_back(pixelFormat);

      // Some buggy drivers can ignore fmtdesc.index and keep returning success forever.
      if (pixelFormat == lastPixelFormat && fmtdesc.index > 0) { break; }
      lastPixelFormat = pixelFormat;

      ++fmtdesc.index;
      ++enumGuard;
      if (enumGuard > 64) { break; }
    }

    close(fd);
#else
    Q_UNUSED(device_url);
    supportedCodes.push_back(cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
#endif

    return supportedCodes;
  }

  // --------------------------------------------------------------------
  // unused: check if camera mask in bitset
  // --------------------------------------------------------------------
  bool isSupportedProbeCodec(int codec)
  {
#ifdef __linux__
    return codec == V4L2_PIX_FMT_MJPEG || codec == V4L2_PIX_FMT_GREY;
#else
    Q_UNUSED(codec);
    return true;
#endif
  }

  // --------------------------------------------------------------------
  // Get the fourCC codes supported by OpenCV for a given resolution and camera index.
  // --------------------------------------------------------------------
  std::vector<int> getOpenCvProbeFourCCs(std::vector<int> const& hardwareCodecs)
  {
    std::vector<int> targetFourCCs;

#ifdef __linux__
    bool hasMJPEG = false;
    bool hasGREY = false;
    for (int codec : hardwareCodecs)
    {
      if (codec == V4L2_PIX_FMT_MJPEG) hasMJPEG = true;
      if (codec == V4L2_PIX_FMT_GREY) hasGREY = true;
    }

    if (hasMJPEG) targetFourCCs.push_back(cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    if (hasGREY) targetFourCCs.push_back(cv::VideoWriter::fourcc('G', 'R', 'E', 'Y'));
#else
    Q_UNUSED(hardwareCodecs);
    targetFourCCs.push_back(cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
#endif

    return targetFourCCs;
  }

  // --------------------------------------------------------------------
  // Get a preferred FPS value from a list of supported values, defaulting to 30 if available.
  // --------------------------------------------------------------------
  int choosePreferredFps(QVariantList const& fpsValues)
  {
    if (fpsValues.isEmpty()) return 30;

    if (fpsValues.contains(30)) return 30;

    int best = fpsValues.first().toInt();
    for (QVariant const& fps : fpsValues)
    {
      int value = fps.toInt();
      if (value > best) best = value;
    }
    return best;
  }

  // --------------------------------------------------------------------
  // Create a group of radio buttons for selecting camera resolutions, based on probing the camera capabilities.
  // --------------------------------------------------------------------
  QButtonGroup* createCameraResolutionGroup(QWidget* parent, int targetSystemIndex,
      std::string const& cameraPath,
      std::function<void(int, int, int)> const& onProbeStepProgress = nullptr)
  {
    QButtonGroup* buttonGroup = new QButtonGroup(parent);
    buttonGroup->setExclusive(true);

    std::vector<int> hardwareCodecs = getSupportedFourCCs(cameraPath);
    std::vector<int> targetFourCCs = getOpenCvProbeFourCCs(hardwareCodecs);

    if (targetFourCCs.empty()) { return buttonGroup; }

    std::unordered_map<std::string, ResolutionConfig> resolutionMap;

    cv::VideoCapture cap(targetSystemIndex, cv::CAP_V4L2);
    if (!cap.isOpened()) { return buttonGroup; }

    for (int currentFourCC : targetFourCCs)
    {
      for (auto const& res : testResolutions)
      {
        for (int fps : testFPS)
        {
          cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
          cap.set(cv::CAP_PROP_FOURCC, currentFourCC);
          cap.set(cv::CAP_PROP_FPS, fps);
          cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
          cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

          int actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
          int actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

          if (actualWidth == res.width && actualHeight == res.height)
          {
            std::string resKey = std::to_string(res.width) + " x " + std::to_string(res.height);

            if (resolutionMap.find(resKey) == resolutionMap.end())
            {
              resolutionMap[resKey] = ResolutionConfig{currentFourCC, QVariantList()};
            }

            if (resolutionMap[resKey].fourcc != currentFourCC)
            {
              // Keep fps values tied to the codec selected for this resolution key.
              continue;
            }

            if (!resolutionMap[resKey].fpsList.contains(fps))
            {
              resolutionMap[resKey].fpsList.append(fps);
            }
          }

          if (onProbeStepProgress) onProbeStepProgress(res.width, res.height, fps);
        }
      }
    }

    cap.release();

    for (auto const& pair : resolutionMap)
    {
      QString resName = QString::fromStdString(pair.first);
      ResolutionConfig const& config = pair.second;

      QRadioButton* radioButton = new QRadioButton(resName);
      radioButton->setProperty("supported_fps", config.fpsList);
      radioButton->setProperty("supported_fourcc", config.fourcc);
      radioButton->setProperty("camera_path", QString::fromStdString(cameraPath));
      buttonGroup->addButton(radioButton);
    }

    if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

    return buttonGroup;
  }

  // --------------------------------------------------------------------
  // Parse a resolution string like "1280 x 720" into a cv::Size object.
  // --------------------------------------------------------------------
  cv::Size parseResolution(QString const& resolutionText)
  {
    QStringList const parts = resolutionText.split("x", Qt::SkipEmptyParts);
    if (parts.size() != 2) { return cv::Size(0, 0); }

    bool widthOk = false;
    bool heightOk = false;
    int const width = parts[0].trimmed().toInt(&widthOk);
    int const height = parts[1].trimmed().toInt(&heightOk);

    if (!widthOk || !heightOk) { return cv::Size(0, 0); }
    return cv::Size(width, height);
  }
}    // namespace

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
cv::Size CameraSelectorWidget::selectedResolution() const { return m_selectedResolution; }

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

  QList<QCameraDevice> const cameras = QMediaDevices::videoInputs();
  if (cameras.empty())
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

  //
  int const totalFPSCount = 3;

  // Use estimated progress maximum based on up to 2 probe codecs (MJPEG + GREY) per camera.
  int totalGlobalProbeSteps = static_cast<int>(cameras.size()) *
      static_cast<int>(testResolutions.size()) * totalFPSCount * 2;
  if (totalGlobalProbeSteps <= 0) totalGlobalProbeSteps = 1;

  // init progress bar
  progressDialog.setRange(0, totalGlobalProbeSteps > 0 ? totalGlobalProbeSteps : 1);
  progressDialog.setValue(0);
  progressDialog.setLabelText(QString("Querying Cameras (%1 detected)").arg(cameras.size()));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_cameraComboBox = new QComboBox(this);
  m_resolutionButtonsContainer = new QWidget(this);
  QVBoxLayout* containerLayout = new QVBoxLayout(m_resolutionButtonsContainer);
  containerLayout->setContentsMargins(0, 0, 0, 0);

  bool hasUsableCamera = false;
  int currentGlobalProgressCounter = 0;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QString const cameraName = cameras[i].description();
    QString const cameraPath = cameras[i].id();
    std::string const cameraPathStr = cameraPath.toStdString();
    int const systemIndex = parseActualCameraIndex(cameraPath, i);

    progressDialog.setLabelText(QString("Querying Cameras: %1").arg(cameraName));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QButtonGroup* resGroup = createCameraResolutionGroup(m_resolutionButtonsContainer, systemIndex,
        cameraPathStr, [&](int probeWidth, int probeHeight, int probeFps) {
          ++currentGlobalProgressCounter;
          progressDialog.setLabelText(
              QString("Querying Cameras: %1/%2 - %3\nTesting %4x%5 @ %6 FPS")
                  .arg(i + 1)
                  .arg(cameras.size())
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

    connect(resGroup, &QButtonGroup::buttonClicked, this,
        &CameraSelectorWidget::onResolutionSelected, Qt::QueuedConnection);

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

void CameraSelectorWidget::onResolutionSelected(QAbstractButton* button)
{
  MARTY_LOG_SCOPE(cam_log, "{} {}", (void*) (this), __func__);
  Q_UNUSED(button);
  emitCurrentSelection();
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
void CameraSelectorWidget::emitCurrentSelection()
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

  m_selectedResolution = parseResolution(checkedButton->text());
  m_selectedCameraPath = checkedButton->property("camera_path").toString().toStdString();
  m_selectedFourCC = checkedButton->property("supported_fourcc").toInt();
  m_selectedFps = choosePreferredFps(checkedButton->property("supported_fps").toList());
  m_hasSelection = (m_selectedResolution.width > 0 && m_selectedResolution.height > 0);

  if (!m_hasSelection) { return; }

  emit cameraConfigChanged(QString::fromStdString(m_selectedCameraPath), m_selectedResolution,
      m_selectedFps, m_selectedFourCC);
}
