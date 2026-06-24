#include "martycam/core/camera_utils.h"
#include "debug/logging.hpp"
//
#include <QString>
#include <QStringList>

#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#ifdef __linux__
# include <fcntl.h>
# include <linux/videodev2.h>
# include <sys/ioctl.h>
# include <unistd.h>
#endif

// ----------------------------------------------------------------------------
static auto camutil_log = martycam::log::create("Capture");

// Convenience function to convert FOURCC int to string for debugging
std::string fourCCToString(int fourcc)
{
  std::string code;
  code += static_cast<char>(fourcc & 0xFF);            // 1st byte
  code += static_cast<char>((fourcc >> 8) & 0xFF);     // 2nd byte
  code += static_cast<char>((fourcc >> 16) & 0xFF);    // 3rd byte
  code += static_cast<char>((fourcc >> 24) & 0xFF);    // 4th byte
  return code;
}

namespace camera_utils {

  //----------------------------------------------------------------------------
  // probe the camera for supported FOURCC codes
  std::vector<int> getSupportedFourCCs_device(std::string device_url)
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

  //----------------------------------------------------------------------------
  bool isSupportedProbeCodec(int codec)
  {
#ifdef __linux__
    return codec == V4L2_PIX_FMT_MJPEG || codec == V4L2_PIX_FMT_GREY;
#else
    Q_UNUSED(codec);
    return true;
#endif
  }
}    // namespace camera_utils

namespace camera_utils {

  //----------------------------------------------------------------------------
  // Get supported FOURCC codes for a camera device or URL
  std::vector<int> getSupportedFourCCs(std::string const& cameraPath)
  {
    bool is_url = (cameraPath.find("://") != std::string::npos);
    if (!is_url) { return getSupportedFourCCs_device(cameraPath); }
    else
    {
      // For IP cameras, we can only assume MJPEG support for now.
      return {cv::VideoWriter::fourcc('M', 'J', 'P', 'G')};
    }
  }

  //----------------------------------------------------------------------------
  // Get OpenCV-compatible FOURCC codes to probe
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

  //----------------------------------------------------------------------------
  // Parse a resolution string like "1280 x 720" into a cam_res object
  cam_res parseResolution(QString const& resolutionText)
  {
    QStringList const parts = resolutionText.split("x", Qt::SkipEmptyParts);
    if (parts.size() != 2) { return cam_res(0, 0); }

    bool widthOk = false;
    bool heightOk = false;
    int const width = parts[0].trimmed().toInt(&widthOk);
    int const height = parts[1].trimmed().toInt(&heightOk);

    if (!widthOk || !heightOk) { return cam_res(0, 0); }
    return cam_res(width, height);
  }

  //----------------------------------------------------------------------------
  // Get a preferred FPS value from a list of supported values
  int choosePreferredFps(std::vector<int> const& fpsValues)
  {
    if (fpsValues.empty()) return 15;
    int best = fpsValues.front();
    for (int const fps : fpsValues)
    {
      if (fps > best) best = fps;
    }
    return best;
  }

  //----------------------------------------------------------------------------
  void setupVideoCapture(cv::VideoCapture& cap, int fourcc, int fps, cv::Size resolution)
  {
    // cap.set(cv::CAP_PROP_CONVERT_RGB, 0);
    cap.set(cv::CAP_PROP_FOURCC, fourcc);
    cap.set(cv::CAP_PROP_FPS, fps);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, resolution.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, resolution.height);
  }

  //----------------------------------------------------------------------------
  cv::VideoCapture createVideoCapture(
      std::string const& cameraPath, int fourcc, int fps, cv::Size resolution)
  {
    // Define the timeout parameter (milliseconds)
    std::vector<int> params = {cv::CAP_PROP_OPEN_TIMEOUT_MSEC, 2000};
    //
    cv::VideoCapture cap;
    setupVideoCapture(cap, fourcc, fps, resolution);

    bool is_url = (cameraPath.find("://") != std::string::npos);
    if (!is_url)
    {
      int deviceIdx = std::stoi(cameraPath.substr(cameraPath.find_last_of("0123456789")));
      cap.open(deviceIdx, cv::CAP_V4L2);
    }
    else { cap.open(cameraPath, cv::CAP_ANY, params); }

    return cap;
  }

  CameraConfig ProbeCameraConfig(std::string const& cameraName, std::string const& cameraPath)
  {
    CameraConfig camera_config;
    camera_config.camera_name = cameraName;
    camera_config.camera_path = cameraPath;

    cv::VideoCapture cap = createVideoCapture(
        cameraPath, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, cv::Size(640, 480));

    if (!cap.isOpened()) { return camera_config; }

    std::vector<int> hardwareCodecs = getSupportedFourCCs(cameraPath);
    std::vector<int> targetFourCCs = getOpenCvProbeFourCCs(hardwareCodecs);

    for (int currentFourCC : targetFourCCs)
    {
      for (auto const& res : testResolutions)
      {
        for (int fps : testFPS)
        {
          setupVideoCapture(cap, currentFourCC, fps, res);
          int actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
          int actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
          int actualFPS = static_cast<int>(cap.get(cv::CAP_PROP_FPS));
          if ((actualWidth == res.width) && (actualHeight == res.height) && (actualFPS == fps))
          {
            camera_resolution camRes{res, currentFourCC, {fps}};
            camera_config.resolutions.push_back(std::move(camRes));
          }
        }
      }
    }
    cap.release();
    return camera_config;
  }

}    // namespace camera_utils
