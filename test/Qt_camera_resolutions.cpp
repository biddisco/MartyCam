#include <QApplication>
#include <QButtonGroup>
#include <QCameraDevice>
#include <QDebug>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMediaDevices>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>
//
#include <cstring>    // For memset
#include <fcntl.h>    // For open() and O_RDONLY
#include <functional>
#include <linux/videodev2.h>
#include <regex>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
//
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>

struct ResCheck
{
  int width;
  int height;
};

// Internal structure to group combined format properties
struct ResolutionConfig
{
  int fourcc;
  QVariantList fpsList;
};

// Extractor helper to parse absolute index from device ID string
int parseActualCameraIndex(QString const& qtDeviceId, int fallbackIndex)
{
  std::string idStr = qtDeviceId.toStdString();
  std::regex videoRegex("video(\\d+)");
  std::smatch match;

  if (std::regex_search(idStr, match, videoRegex)) { return std::stoi(match[1].str()); }
  return fallbackIndex;
}

// --------------------------------------------------------------------
// Instantly queries the Linux kernel for a camera's supported FOURCC codes
// --------------------------------------------------------------------
std::vector<int> getSupportedFourCCs(int cameraIndex)
{
  std::vector<int> supportedCodes;
  std::string devicePath = "/dev/video" + std::to_string(cameraIndex);

  int fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) { return supportedCodes; }

  struct v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmtdesc.index = 0;

  while (ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
  {
    supportedCodes.push_back(static_cast<int>(fmtdesc.pixelformat));
    // qDebug() << "Found supported FOURCC code:"
    //          << QString::fromStdString(std::string((char*) &fmtdesc.pixelformat, 4));
    fmtdesc.index++;
  }

  close(fd);
  return supportedCodes;
}

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

// --------------------------------------------------------------------
// Robust OpenCV resolution probing module
// --------------------------------------------------------------------
QButtonGroup* createCameraResolutionGroup(QWidget* parent, int targetSystemIndex,
    std::function<void(int, int, int)> const& onProbeStepProgress = nullptr)
{
  QButtonGroup* buttonGroup = new QButtonGroup(parent);
  buttonGroup->setExclusive(true);

  std::vector<int> hardwareCodecs = getSupportedFourCCs(targetSystemIndex);

  bool hasMJPEG = false;
  bool hasGREY = false;

  for (int codec : hardwareCodecs)
  {
    if (codec == V4L2_PIX_FMT_MJPEG) hasMJPEG = true;
    if (codec == V4L2_PIX_FMT_GREY) hasGREY = true;
  }

  if (!hasMJPEG && !hasGREY) { return buttonGroup; }

  std::vector<int> targetFourCCs;
  if (hasMJPEG) targetFourCCs.push_back(cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
  if (hasGREY) targetFourCCs.push_back(cv::VideoWriter::fourcc('G', 'R', 'E', 'Y'));

  std::vector<ResCheck> testResolutions = {{320, 240}, {640, 480}, {640, 360}, {800, 600},
      {1024, 768}, {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
  std::vector<int> testFPS = {15, 30, 60};

  // Track configurations unique to each resolution name string
  std::unordered_map<std::string, ResolutionConfig> resolutionMap;

  cv::VideoCapture cap(targetSystemIndex, cv::CAP_V4L2);
  if (!cap.isOpened()) { return buttonGroup; }

  for (int currentFourCC : targetFourCCs)
  {
    for (auto const& res : testResolutions)
    {
      for (int fps : testFPS)
      {
        // Enforce converted mode baseline off to support native single-channel parsing
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

          // Seed new structural configs inside the tracking map
          if (resolutionMap.find(resKey) == resolutionMap.end())
          {
            resolutionMap[resKey] = ResolutionConfig{currentFourCC, QVariantList()};
          }

          if (!resolutionMap[resKey].fpsList.contains(fps))
          {
            resolutionMap[resKey].fpsList.append(fps);
          }

          std::string codecName = fourCCToString(currentFourCC);
        //   qDebug() << "Camera supports resolution:" << QString::fromStdString(codecName) << " : "
        //            << QString::fromStdString(resKey) << "@" << fps << "FPS";
        }

        if (onProbeStepProgress) { onProbeStepProgress(res.width, res.height, fps); }
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
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
    radioButton->setProperty(
        "supported_fourcc", config.fourcc);    // FIX 1: Save target codec integer code
    radioButton->setProperty("camera_index", targetSystemIndex);
    buttonGroup->addButton(radioButton);
  }

  if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

  return buttonGroup;
}

// --------------------------------------------------------------------
// Reusable widget that can be embedded into a larger form.
// --------------------------------------------------------------------
class CameraSelectorWidget : public QWidget
{
  public:
  explicit CameraSelectorWidget(QWidget* parent = nullptr)
    : QWidget(parent)
  {
    setMinimumWidth(450);

    m_mainLayout = new QVBoxLayout(this);
    m_camerasContainerLayout = new QVBoxLayout();
    m_mainLayout->addLayout(m_camerasContainerLayout);

    refreshCameraList();

    connect(new QMediaDevices(this), &QMediaDevices::videoInputsChanged, this,
        &CameraSelectorWidget::refreshCameraList);
  }

  private:
  void refreshCameraList()
  {
    qDebug() << "Hardware modification detected! Refreshing camera grid...";

    // create a progress dialog to provide feedback during potentially long-running camera queries
    QProgressDialog progressDialog("Querying Cameras", QString(), 0, 1, this);
    progressDialog.setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    progressDialog.setWindowModality(Qt::WindowModal);
    progressDialog.setCancelButton(nullptr);
    progressDialog.setAutoClose(false);
    progressDialog.setAutoReset(false);
    progressDialog.resize(420, 120);

    // Center the progress dialog on the screen
    QRect const screenGeometry = QApplication::primaryScreen()->geometry();
    QRect const dialogGeometry = progressDialog.frameGeometry();
    progressDialog.move((screenGeometry.width() - dialogGeometry.width()) / 2,
        (screenGeometry.height() - dialogGeometry.height()) / 2);
    progressDialog.show();
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Clear out existing layouts
    QLayoutItem* item;
    while ((item = m_camerasContainerLayout->takeAt(0)) != nullptr)
    {
      if (item->widget()) { delete item->widget(); }
      delete item;
    }

    // Query cameras before building the UI to get an accurate count for
    // progress tracking and to avoid redundant queries during widget construction
    QList<QCameraDevice> const cameras = QMediaDevices::videoInputs();
    if (cameras.empty())
    {
      m_camerasContainerLayout->addWidget(new QLabel("No active video devices detected.", this));
      progressDialog.hide();
      return;
    }

    // resolutions we will check for - can be expanded to include more exotic formats if needed,
    // but these cover the most common use cases
    std::vector<ResCheck> const testResolutions = {
        {320, 240},                 // QVGA
        {640, 480},                 // VGA
        {640, 360},                 // IR cameras : non-standard aspect ratios
        {800, 600},                 // XGA
        {1024, 768},                // SVGA
        {1280, 720},                // HD
        {1920, 1080},               // Full HD
        {2560, 1440},               // 2K
        {3840, 2160}                // Ultra HD
    };
    int const totalFPSCount = 3;    // {15, 30, 60}

    // Probing each camera for supported resolutions and codecs can take a while, so we
    // quick check each camera first to get an accurate count of total steps for progress 
    // tracking before starting the full render loop
    int totalGlobalProbeSteps = 0;
    std::vector<int> targetSystemIndices;
    std::vector<int> codecsPerCamera;

    for (int i = 0; i < cameras.size(); ++i)
    {
      int systemIndex = parseActualCameraIndex(cameras[i].id(), i);
      targetSystemIndices.push_back(systemIndex);

      // currently we are only bothering with MJPEG and GREY since they are the most common V4L2 
      // formats that support multiple resolutions, but this can be expanded as needed
      std::vector<int> codecs = getSupportedFourCCs(systemIndex);
      int validCodecCount = 0;
      for (int c : codecs)
      {
        if (c == V4L2_PIX_FMT_MJPEG || c == V4L2_PIX_FMT_GREY) { validCodecCount++; }
      }
      codecsPerCamera.push_back(validCodecCount);
      totalGlobalProbeSteps +=
          (validCodecCount * static_cast<int>(testResolutions.size()) * totalFPSCount);
    }
    // set the progress bar limits
    progressDialog.setRange(0, totalGlobalProbeSteps > 0 ? totalGlobalProbeSteps : 1);
    progressDialog.setValue(0);
    progressDialog.setLabelText(QString("Querying Cameras (%1 detected)").arg(cameras.size()));
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Build a new widget for each camera and add it to the tab container
    QTabWidget* cameraTabs = new QTabWidget(this);
    bool hasUsableCamera = false;
    int currentGlobalProgressCounter = 0;

    // Rebuild Tab Container Layout Grid
    for (int i = 0; i < cameras.size(); ++i)
    {
      QString cameraName = cameras[i].description();
      int systemIndex = targetSystemIndices[i];

      QWidget* cameraTab = new QWidget(cameraTabs);
      QVBoxLayout* tabLayout = new QVBoxLayout(cameraTab);

      QLabel* deviceInfoLabel = new QLabel(QString("Device: %1\nNode: /dev/video%2\nID: %3")
                                               .arg(cameraName)
                                               .arg(systemIndex)
                                               .arg(cameras[i].id()),
          cameraTab);
      deviceInfoLabel->setWordWrap(true);
      tabLayout->addWidget(deviceInfoLabel);

      progressDialog.setLabelText(QString("Querying Cameras: %1").arg(cameraName));
      qDebug() << "Probing camera:" << cameraName << "(Node:" << cameras[i].id() << ")";
      QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

      // Trigger the probe using an incremental absolute progress hook reference
      QButtonGroup* resGroup = createCameraResolutionGroup(
          cameraTab, systemIndex, [&](int probeWidth, int probeHeight, int probeFps) {
            currentGlobalProgressCounter++;
            progressDialog.setLabelText(
                QString("Querying Cameras: %1/%2 - %3\nTesting %4x%5 @ %6 FPS")
                    .arg(i + 1)
                    .arg(cameras.size())
                    .arg(cameraName)
                    .arg(probeWidth)
                    .arg(probeHeight)
                    .arg(probeFps));
            progressDialog.setValue(currentGlobalProgressCounter);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
          });

      // If no valid codes exist on this sensor, skip rendering the tab frame
      if (resGroup->buttons().isEmpty())
      {
        delete cameraTab;
        continue;
      }

      for (QAbstractButton* button : resGroup->buttons()) { tabLayout->addWidget(button); }
      tabLayout->addStretch();

      connect(resGroup, &QButtonGroup::buttonClicked, this,
          &CameraSelectorWidget::onResolutionSelected);

      cameraTabs->addTab(cameraTab, cameraName);
      hasUsableCamera = true;
    }

    if (hasUsableCamera) { m_camerasContainerLayout->addWidget(cameraTabs); }
    else
    {
      delete cameraTabs;
      m_camerasContainerLayout->addWidget(
          new QLabel("No cameras with supported resolutions were detected.", this));
    }

    progressDialog.setValue(progressDialog.maximum());
    progressDialog.hide();
  }

  void onResolutionSelected(QAbstractButton* button)
  {
    if (!button) return;

    int realCamIdx = button->property("camera_index").toInt();
    int packedFourCC =
        button->property("supported_fourcc").toInt();    // Read stored user-data parameter format
    QString resText = button->text();
    QVariantList fpsList = button->property("supported_fps").toList();

    QStringList fpsStrings;
    for (QVariant const& fps : fpsList) fpsStrings << QString::number(fps.toInt());
    QString fpsDisplay = fpsStrings.join(", ");

    QString codecNameStr = QString::fromStdString(fourCCToString(packedFourCC));

    QMessageBox::information(this, "Device Stream Configured",
        QString("Target Node: /dev/video%1\nCodec Format: %2\nResolution: %3\nFramerates: %4 FPS")
            .arg(realCamIdx)
            .arg(codecNameStr)
            .arg(resText)
            .arg(fpsDisplay));
  }

  QVBoxLayout* m_mainLayout;
  QVBoxLayout* m_camerasContainerLayout;
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  QDialog dialog;
  dialog.setWindowTitle("Hot-Plug Camera Manager");

  QVBoxLayout* dialogLayout = new QVBoxLayout(&dialog);
  CameraSelectorWidget* selectorWidget = new CameraSelectorWidget(&dialog);
  dialogLayout->addWidget(selectorWidget);

  QHBoxLayout* actionLayout = new QHBoxLayout();
  QPushButton* closeButton = new QPushButton("Apply & Exit", &dialog);
  actionLayout->addStretch();
  actionLayout->addWidget(closeButton);
  dialogLayout->addLayout(actionLayout);

  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

  dialog.show();

  return app.exec();
}