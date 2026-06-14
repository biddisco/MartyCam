#ifndef CAMERA_UTILS_H
#define CAMERA_UTILS_H

#include <opencv2/opencv.hpp>

#include <array>
#include <functional>
#include <string>
#include <vector>

#include <QVariant>

class QAbstractButton;
class QButtonGroup;
class QString;
class QWidget;

// Convenience function to convert FOURCC int to string for debugging
std::string fourCCToString(int fourcc);

// Resolution configuration data structure
struct ResolutionConfig
{
  int fourcc;
  QVariantList fpsList;
};

namespace camera_utils {

  using cam_res = cv::Size;

  // List of FPS to test when probing cameras
  inline std::array<int, 3> testFPS = {15, 30, 60};

  // Common resolutions to test when probing cameras
  inline std::array<cam_res, 9> const testResolutions = {
      //
      cam_res{320, 240},      // QVGA
      cam_res{640, 480},      // VGA
      cam_res{640, 360},      // IR camera friendly
      cam_res{800, 600},      // SVGA
      cam_res{1024, 768},     // XGA
      cam_res{1280, 720},     // HD
      cam_res{1920, 1080},    // 1080p
      cam_res{2560, 1440},    // QHD
      cam_res{3840, 2160}     // 4K UHD
  };

  // Get supported FOURCC codes for a camera device or URL
  std::vector<int> getSupportedFourCCs(std::string const& cameraPath);

  // Get OpenCV-compatible FOURCC codes to probe
  std::vector<int> getOpenCvProbeFourCCs(std::vector<int> const& hardwareCodecs);

  // Parse a resolution string like "1280 x 720" into a cam_res object
  cam_res parseResolution(QString const& resolutionText);

  // Get a preferred FPS value from a list of supported values
  int choosePreferredFps(QVariantList const& fpsValues);

  // Create a group of radio buttons for selecting camera resolutions
  // by probing the camera capabilities via OpenCV
  QButtonGroup* createCameraResolutionGroup(QWidget* parent, std::string const& cameraPath,
      std::function<void(int, int, int)> const& onProbeStepProgress = nullptr);

  void setupVideoCapture(cv::VideoCapture& cap, int fourcc, int fps, cv::Size resolution);

  cv::VideoCapture createVideoCapture(
      std::string const& cameraPath, int fourcc, int fps, cv::Size resolution);

}    // namespace camera_utils

#endif
