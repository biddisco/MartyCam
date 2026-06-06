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
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QVariant>
//
#include <opencv2/opencv.hpp>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

struct ResCheck
{
  int width;
  int height;
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
// Robust OpenCV resolution probing module
// --------------------------------------------------------------------
QButtonGroup* createCameraResolutionGroup(QWidget* parent, int targetSystemIndex)
{
  QButtonGroup* buttonGroup = new QButtonGroup(parent);
  buttonGroup->setExclusive(true);

  // Hardware verification check
  {
    cv::VideoCapture testCap;
    testCap.open(targetSystemIndex, cv::CAP_V4L2);
    if (!testCap.isOpened()) return buttonGroup;

    int fourcc = static_cast<int>(testCap.get(cv::CAP_PROP_FOURCC));
    double fps = testCap.get(cv::CAP_PROP_FPS);
    if (fourcc == 0 && fps <= 0.0)
    {
      testCap.release();
      return buttonGroup;
    }
    testCap.release();
  }

  std::vector<ResCheck> testResolutions = {
      {320, 240},      //
      {640, 480},      //
      {800, 600},      //
      {1024, 768},     //
      {1280, 720},     // 720p
      {1920, 1080},    // 1080p
      {2560, 1440},    // 2K
      {3840, 2160}     // 4K
  };
  std::vector<int> testFPS = {30, 60};
  std::unordered_map<std::string, QVariantList> resolutionMap;

  cv::VideoCapture cap(targetSystemIndex, cv::CAP_V4L2);
  if (!cap.isOpened()) return buttonGroup;

  for (auto const& res : testResolutions)
  {
    for (int fps : testFPS)
    {
      cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
      cap.set(cv::CAP_PROP_FPS, fps);
      cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
      cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

      int actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
      int actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

      if (actualWidth == res.width && actualHeight == res.height)
      {
        std::string resKey = std::to_string(res.width) + " x " + std::to_string(res.height);
        if (!resolutionMap[resKey].contains(fps)) { resolutionMap[resKey].append(fps); }
      }
    }
  }
  cap.release();

  for (auto const& pair : resolutionMap)
  {
    QString resName = QString::fromStdString(pair.first);
    QVariantList fpsList = pair.second;

    QRadioButton* radioButton = new QRadioButton(resName, parent);
    radioButton->setProperty("supported_fps", fpsList);
    radioButton->setProperty("camera_index", targetSystemIndex);
    buttonGroup->addButton(radioButton);
  }

  if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

  return buttonGroup;
}

// --------------------------------------------------------------------
// Custom Dialog Subclass to handle dynamic UI refreshes upon hardware changes
// --------------------------------------------------------------------
class CameraSelectorDialog : public QDialog
{
  Q_OBJECT

  public:
  explicit CameraSelectorDialog(QWidget* parent = nullptr)
    : QDialog(parent)
  {
    setWindowTitle("Hot-Plug Camera Manager");
    setMinimumWidth(450);

    // Core Window Structural Layout
    m_mainLayout = new QVBoxLayout(this);

    // This sub-layout holds our dynamic camera list boxes
    m_camerasContainerLayout = new QVBoxLayout();
    m_mainLayout->addLayout(m_camerasContainerLayout);

    // Persistent Close Action Button at the bottom
    QHBoxLayout* actionLayout = new QHBoxLayout();
    QPushButton* closeButton = new QPushButton("Apply & Exit", this);
    actionLayout->addStretch();
    actionLayout->addWidget(closeButton);
    m_mainLayout->addLayout(actionLayout);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    // Build initial UI state
    refreshCameraList();

    // HOT-PLUG HOOK: Listen to the OS hardware subsystem via Qt Multimedia.
    // Triggers instantly whenever a camera device is added or removed.
    // FIX: Instantiate a new QMediaDevices object or connect directly via a pointer to one
    connect(new QMediaDevices(this), &QMediaDevices::videoInputsChanged, this,
        &CameraSelectorDialog::refreshCameraList);
  }

  private slots:
  void refreshCameraList()
  {
    qDebug() << "Hardware modification detected! Refreshing camera grid...";

    // 1. Clear out existing UI elements inside the camera layout container
    QLayoutItem* item;
    while ((item = m_camerasContainerLayout->takeAt(0)) != nullptr)
    {
      if (item->widget())
      {
        delete item->widget();    // Cleans up the QGroupBox frames cleanly
      }
      delete item;
    }

    // 2. Query updated system hardware nodes
    QList<QCameraDevice> const cameras = QMediaDevices::videoInputs();

    if (cameras.empty())
    {
      m_camerasContainerLayout->addWidget(new QLabel("No active video devices detected.", this));
      return;
    }

    // 3. Rebuild the device interface grids
    for (int i = 0; i < cameras.size(); ++i)
    {
      QString cameraName = cameras[i].description();
      QString rawDeviceId = QString::fromUtf8(cameras[i].id());
      int systemIndex = parseActualCameraIndex(rawDeviceId, i);

      QGroupBox* cameraBox =
          new QGroupBox(QString("%1 (/dev/video%2)").arg(cameraName).arg(systemIndex), this);
      QVBoxLayout* boxLayout = new QVBoxLayout(cameraBox);

      QButtonGroup* resGroup = createCameraResolutionGroup(this, systemIndex);

      if (resGroup->buttons().isEmpty())
      {
        delete cameraBox;
        delete resGroup;
        continue;
      }

      for (QAbstractButton* button : resGroup->buttons()) { boxLayout->addWidget(button); }

      // Bind selection change notifications
      connect(resGroup, &QButtonGroup::buttonClicked, this,
          &CameraSelectorDialog::onResolutionSelected);
      m_camerasContainerLayout->addWidget(cameraBox);
    }
  }

  void onResolutionSelected(QAbstractButton* button)
  {
    if (!button) return;

    int realCamIdx = button->property("camera_index").toInt();
    QString resText = button->text();
    QVariantList fpsList = button->property("supported_fps").toList();

    QStringList fpsStrings;
    for (QVariant const& fps : fpsList) fpsStrings << QString::number(fps.toInt());
    QString fpsDisplay = fpsStrings.join(", ");

    QMessageBox::information(this, "Device Stream Configured",
        QString("Target Node: /dev/video%1\nResolution: %2\nFramerates: %3 FPS")
            .arg(realCamIdx)
            .arg(resText)
            .arg(fpsDisplay));
  }

  private:
  QVBoxLayout* m_mainLayout;
  QVBoxLayout* m_camerasContainerLayout;
};

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  CameraSelectorDialog dialog;
  dialog.show();

  return app.exec();
}

#include "Qt_camera_resolutions.moc"    // Required for standalone single-file Qt compilation builds