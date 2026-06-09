#include "CameraSelectorWidget.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QCameraDevice>
#include <QDebug>
#include <QEventLoop>
#include <QLabel>
#include <QLayoutItem>
#include <QList>
#include <QMediaDevices>
#include <QProgressDialog>
#include <QRadioButton>
#include <QScreen>
#include <QTabWidget>
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

  int parseActualCameraIndex(QString const& qtDeviceId, int fallbackIndex)
  {
    std::string idStr = qtDeviceId.toStdString();
    std::regex videoRegex("video(\\d+)");
    std::smatch match;

    if (std::regex_search(idStr, match, videoRegex)) { return std::stoi(match[1].str()); }
    return fallbackIndex;
  }

  std::vector<int> getSupportedFourCCs(int cameraIndex)
  {
    std::vector<int> supportedCodes;

#ifdef __linux__
    std::string devicePath = "/dev/video" + std::to_string(cameraIndex);

    int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
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
    Q_UNUSED(cameraIndex);
    supportedCodes.push_back(cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
#endif

    return supportedCodes;
  }

  bool isSupportedProbeCodec(int codec)
  {
#ifdef __linux__
    return codec == V4L2_PIX_FMT_MJPEG || codec == V4L2_PIX_FMT_GREY;
#else
    Q_UNUSED(codec);
    return true;
#endif
  }

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

  QButtonGroup* createCameraResolutionGroup(QWidget* parent, int targetSystemIndex,
      std::function<void(int, int, int)> const& onProbeStepProgress = nullptr)
  {
    QButtonGroup* buttonGroup = new QButtonGroup(parent);
    buttonGroup->setExclusive(true);

    std::vector<int> hardwareCodecs = getSupportedFourCCs(targetSystemIndex);
    std::vector<int> targetFourCCs = getOpenCvProbeFourCCs(hardwareCodecs);

    if (targetFourCCs.empty()) { return buttonGroup; }

    std::vector<ResCheck> testResolutions = {{320, 240}, {640, 480}, {640, 360}, {800, 600},
        {1024, 768}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
    std::vector<int> testFPS = {15, 30, 60};

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

      QRadioButton* radioButton = new QRadioButton(resName, parent);
      radioButton->setProperty("supported_fps", config.fpsList);
      radioButton->setProperty("supported_fourcc", config.fourcc);
      radioButton->setProperty("camera_index", targetSystemIndex);
      buttonGroup->addButton(radioButton);
    }

    if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

    return buttonGroup;
  }

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

CameraSelectorWidget::CameraSelectorWidget(QWidget* parent)
  : QWidget(parent)
  , m_mainLayout(new QVBoxLayout(this))
  , m_camerasContainerLayout(new QVBoxLayout())
  , m_cameraTabs(nullptr)
  , m_hasSelection(false)
  , m_selectedCameraIndex(-1)
  , m_selectedResolution(0, 0)
  , m_selectedFps(30)
  , m_selectedFourCC(0)
{
  setMinimumWidth(450);
  m_mainLayout->addLayout(m_camerasContainerLayout);

  // Defer first scan until the event loop is active, avoiding processEvents during construction.
  // MartyCam will initialize threads when the first cameraConfigChanged signal arrives.
  QTimer::singleShot(0, this, &CameraSelectorWidget::refreshCameraList);

  QMediaDevices* mediaDevices = new QMediaDevices(this);
  QTimer* refreshDebounceTimer = new QTimer(this);
  refreshDebounceTimer->setSingleShot(true);
  refreshDebounceTimer->setInterval(250);

  connect(mediaDevices, &QMediaDevices::videoInputsChanged, this,
      [this, refreshDebounceTimer]() { refreshDebounceTimer->start(); });
  connect(refreshDebounceTimer, &QTimer::timeout, this, &CameraSelectorWidget::refreshCameraList);
}

bool CameraSelectorWidget::hasSelection() const { return m_hasSelection; }

int CameraSelectorWidget::selectedCameraIndex() const { return m_selectedCameraIndex; }

cv::Size CameraSelectorWidget::selectedResolution() const { return m_selectedResolution; }

int CameraSelectorWidget::selectedFps() const { return m_selectedFps; }

int CameraSelectorWidget::selectedFourCC() const { return m_selectedFourCC; }

void CameraSelectorWidget::clearCameraWidgets()
{
  QLayoutItem* item = nullptr;
  while ((item = m_camerasContainerLayout->takeAt(0)) != nullptr)
  {
    if (item->widget()) { delete item->widget(); }
    delete item;
  }

  m_cameraTabs = nullptr;
}

void CameraSelectorWidget::refreshCameraList()
{
  if (m_isRefreshing)
  {
    m_refreshPending = true;
    return;
  }

  m_isRefreshing = true;
  m_refreshPending = false;

  qDebug() << "Hardware modification detected! Refreshing camera grid...";

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

  std::vector<ResCheck> const testResolutions = {{320, 240}, {640, 480}, {640, 360}, {800, 600},
      {1024, 768}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
  int const totalFPSCount = 3;

  // Use estimated maximum based on up to 2 probe codecs (MJPEG + GREY) per camera.
  int totalGlobalProbeSteps = static_cast<int>(cameras.size()) *
      static_cast<int>(testResolutions.size()) * totalFPSCount * 2;
  if (totalGlobalProbeSteps <= 0) totalGlobalProbeSteps = 1;

  progressDialog.setRange(0, totalGlobalProbeSteps > 0 ? totalGlobalProbeSteps : 1);
  progressDialog.setValue(0);
  progressDialog.setLabelText(QString("Querying Cameras (%1 detected)").arg(cameras.size()));
  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  m_cameraTabs = new QTabWidget(this);
  bool hasUsableCamera = false;
  int currentGlobalProgressCounter = 0;
  int uiPumpCounter = 0;

  for (int i = 0; i < cameras.size(); ++i)
  {
    QString const cameraName = cameras[i].description();
    int const systemIndex = parseActualCameraIndex(cameras[i].id(), i);

    QWidget* cameraTab = new QWidget(m_cameraTabs);
    QVBoxLayout* tabLayout = new QVBoxLayout(cameraTab);

    QLabel* deviceInfoLabel = new QLabel(QString("Device: %1\nNode: /dev/video%2\nID: %3")
                                             .arg(cameraName)
                                             .arg(systemIndex)
                                             .arg(cameras[i].id()),
        cameraTab);
    deviceInfoLabel->setWordWrap(true);
    tabLayout->addWidget(deviceInfoLabel);

    progressDialog.setLabelText(QString("Querying Cameras: %1").arg(cameraName));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    QButtonGroup* resGroup = createCameraResolutionGroup(
        cameraTab, systemIndex, [&](int probeWidth, int probeHeight, int probeFps) {
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
          ++uiPumpCounter;
          if ((uiPumpCounter % 8) == 0)
          {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
          }
        });

    if (resGroup->buttons().isEmpty())
    {
      delete cameraTab;
      continue;
    }

    for (QAbstractButton* button : resGroup->buttons()) { tabLayout->addWidget(button); }
    tabLayout->addStretch();

    connect(
        resGroup, &QButtonGroup::buttonClicked, this, &CameraSelectorWidget::onResolutionSelected);

    m_cameraTabs->addTab(cameraTab, cameraName);
    hasUsableCamera = true;
  }

  if (hasUsableCamera)
  {
    m_camerasContainerLayout->addWidget(m_cameraTabs);
    connect(m_cameraTabs, &QTabWidget::currentChanged, this,
        &CameraSelectorWidget::onCurrentTabChanged);
  }
  else
  {
    delete m_cameraTabs;
    m_cameraTabs = nullptr;
    m_camerasContainerLayout->addWidget(
        new QLabel("No cameras with supported resolutions were detected.", this));
    m_hasSelection = false;
  }

  progressDialog.setValue(progressDialog.maximum());
  progressDialog.hide();

  emitCurrentSelection();

  m_isRefreshing = false;
  if (m_refreshPending)
  {
    m_refreshPending = false;
    QTimer::singleShot(0, this, &CameraSelectorWidget::refreshCameraList);
  }
}

void CameraSelectorWidget::onResolutionSelected(QAbstractButton* button)
{
  Q_UNUSED(button);
  emitCurrentSelection();
}

void CameraSelectorWidget::onCurrentTabChanged(int index)
{
  Q_UNUSED(index);
  emitCurrentSelection();
}

void CameraSelectorWidget::emitCurrentSelection()
{
  if (!m_cameraTabs)
  {
    m_hasSelection = false;
    return;
  }

  QWidget* currentTab = m_cameraTabs->currentWidget();
  if (!currentTab)
  {
    m_hasSelection = false;
    return;
  }

  QList<QRadioButton*> buttons = currentTab->findChildren<QRadioButton*>();
  QRadioButton* checkedButton = nullptr;
  for (QRadioButton* button : buttons)
  {
    if (button->isChecked())
    {
      checkedButton = button;
      break;
    }
  }

  if (!checkedButton && !buttons.isEmpty())
  {
    checkedButton = buttons.first();
    checkedButton->setChecked(true);
  }

  if (!checkedButton)
  {
    m_hasSelection = false;
    return;
  }

  m_selectedResolution = parseResolution(checkedButton->text());
  m_selectedCameraIndex = checkedButton->property("camera_index").toInt();
  m_selectedFourCC = checkedButton->property("supported_fourcc").toInt();
  m_selectedFps = choosePreferredFps(checkedButton->property("supported_fps").toList());
  m_hasSelection = (m_selectedResolution.width > 0 && m_selectedResolution.height > 0);

  if (!m_hasSelection) { return; }

  emit cameraConfigChanged(
      m_selectedCameraIndex, m_selectedResolution, m_selectedFps, m_selectedFourCC);
}
