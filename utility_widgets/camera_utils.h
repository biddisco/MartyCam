#ifndef CAMERA_UTILS_H
#define CAMERA_UTILS_H

#include <opencv2/opencv.hpp>
//
#include <array>
#include <functional>
#include <string>
#include <vector>
//
#include <nlohmann/json.hpp>

class QAbstractButton;
class QButtonGroup;
class QString;
class QWidget;

//----------------------------------------------------------------------------
struct camera_resolution
{
  cv::Size resolution;
  int fourcc;
  std::vector<int> fpsList;
};

//----------------------------------------------------------------------------
struct CameraConfig
{
  std::string camera_name;
  std::string camera_path;
  std::vector<camera_resolution> resolutions;
  //
  std::string to_json() const
  {
    nlohmann::json json;
    json["camera_name"] = camera_name;
    json["camera_path"] = camera_path;
    json["resolutions"] = nlohmann::json::array();

    for (auto const& res : resolutions)
    {
      json["resolutions"].push_back({{"width", res.resolution.width},
          {"height", res.resolution.height}, {"fourcc", res.fourcc}, {"fpsList", res.fpsList}});
    }

    return json.dump();
  }

  //----------------------------------------------------------------------------
  static CameraConfig from_json(std::string const& json)
  {
    CameraConfig config;
    auto j = nlohmann::json::parse(json);
    config.camera_name = j["camera_name"].get<std::string>();
    config.camera_path = j["camera_path"].get<std::string>();
    for (auto const& res : j["resolutions"])
    {
      camera_resolution camRes;
      camRes.resolution.width = res["width"].get<int>();
      camRes.resolution.height = res["height"].get<int>();
      camRes.fourcc = res["fourcc"].get<int>();
      camRes.fpsList = res["fpsList"].get<std::vector<int>>();
      config.resolutions.push_back(camRes);
    }
    return config;
  }
};

//----------------------------------------------------------------------------
// Convenience function to convert FOURCC int to string for debugging
std::string fourCCToString(int fourcc);

//----------------------------------------------------------------------------
namespace camera_utils {

  using cam_res = cv::Size;

  // List of FPS to test when probing cameras
  inline std::array<int, 5> testFPS = {15, 20, 25, 30, 60};

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

  CameraConfig ProbeCameraConfig(std::string const& cameraName, std::string const& cameraPath);

  void setupVideoCapture(cv::VideoCapture& cap, int fourcc, int fps, cv::Size resolution);

  cv::VideoCapture createVideoCapture(
      std::string const& cameraPath, int fourcc, int fps, cv::Size resolution);

}    // namespace camera_utils

#endif
