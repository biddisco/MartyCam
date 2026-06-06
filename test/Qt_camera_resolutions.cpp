#include <QApplication>
#include <QButtonGroup>
#include <QDebug>
#include <QDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QVariant>
#include <unordered_map>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// --- The Resolution Struct and Function from before ---
struct ResCheck
{
  int width;
  int height;
};

QButtonGroup* createCameraResolutionGroup(QWidget* parent, int cameraIndex = 0)
{
  QButtonGroup* buttonGroup = new QButtonGroup(parent);
  buttonGroup->setExclusive(true);

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

  cv::VideoCapture cap(cameraIndex, cv::CAP_V4L2);
  if (!cap.isOpened()) { return buttonGroup; }

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
        // Simple duplication safeguard
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
    buttonGroup->addButton(radioButton);
  }

  if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

  return buttonGroup;
}

// --- Main Application Loop ---
int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  // 1. Create a top-level Dialog Window
  QDialog dialog;
  dialog.setWindowTitle("Camera Configuration");
  dialog.setMinimumWidth(300);

  // 2. Setup the Layouts
  QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

  // 3. Instantiate our OpenCV-powered radio button group
  QButtonGroup* resGroup = createCameraResolutionGroup(&dialog, 0);

  // 4. Extract the physical buttons from the group and add them to the visual layout
  if (resGroup->buttons().isEmpty())
  {
    // Fallback message if no camera modes were found
    mainLayout->addWidget(new QPushButton("No supported camera modes found.", &dialog));
  }
  else
  {
    for (QAbstractButton* button : resGroup->buttons()) { mainLayout->addWidget(button); }
  }

  // 5. Create a Close Button at the bottom
  QHBoxLayout* buttonLayout = new QHBoxLayout();
  QPushButton* closeButton = new QPushButton("Close", &dialog);
  buttonLayout->addStretch();
  buttonLayout->addWidget(closeButton);
  mainLayout->addLayout(buttonLayout);

  // 6. Define the Click Callback (Using a modern C++ lambda for simplicity)
  QObject::connect(resGroup, &QButtonGroup::buttonClicked, [](QAbstractButton* button) {
    if (!button) return;

    QString resText = button->text();
    QVariantList fpsList = button->property("supported_fps").toList();

    // Convert the FPS list into a nice printable string (e.g., "[30, 60]")
    QStringList fpsStrings;
    for (QVariant const& fps : fpsList) { fpsStrings << QString::number(fps.toInt()); }
    QString fpsDisplay = fpsStrings.join(", ");

    // Print to standard terminal debug output
    qDebug() << "Selected:" << resText << "with matching FPS options:" << fpsDisplay;

    // Also pop up a quick non-blocking status message to show it works visually
    QMessageBox msgBox;
    msgBox.setText("Selected Mode: " + resText + "\nSupported FPS: [" + fpsDisplay + "]");
    msgBox.exec();
  });

  // 7. Wire up the close button to dismiss the dialog window
  QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

  // Run the UI
  dialog.show();
  return app.exec();
}