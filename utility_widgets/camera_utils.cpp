#include "camera_utils.h"

#include <QButtonGroup>
#include <QRadioButton>
#include <QString>
#include <QVariant>
#include <QWidget>

#include <opencv2/videoio.hpp>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef __linux__
# include <fcntl.h>
# include <linux/videodev2.h>
# include <sys/ioctl.h>
# include <unistd.h>
#endif

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
  int choosePreferredFps(QVariantList const& fpsValues)
  {
    if (fpsValues.isEmpty()) return 30;
    int best = fpsValues.first().toInt();
    for (QVariant const& fps : fpsValues)
    {
      int value = fps.toInt();
      if (value > best) best = value;
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

  //----------------------------------------------------------------------------
  // Create a group of radio buttons for selecting camera resolutions
  // by probing the camera capabilities via OpenCV
  QButtonGroup* createCameraResolutionGroup(QWidget* parent, std::string const& cameraPath,
      std::function<void(int, int, int)> const& onProbeStepProgress)
  {
    cv::VideoCapture cap = createVideoCapture(
        cameraPath, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 30, cv::Size(640, 480));

    QButtonGroup* buttonGroup = new QButtonGroup(parent);
    if (!cap.isOpened()) { return buttonGroup; }

    buttonGroup->setExclusive(true);
    std::vector<int> hardwareCodecs = getSupportedFourCCs(cameraPath);
    std::vector<int> targetFourCCs = getOpenCvProbeFourCCs(hardwareCodecs);

    if (targetFourCCs.empty()) { return buttonGroup; }

    std::unordered_map<std::string, ResolutionConfig> resolutionMap;

    for (int currentFourCC : targetFourCCs)
    {
      for (auto const& res : testResolutions)
      {
        for (int fps : testFPS)
        {
          setupVideoCapture(cap, currentFourCC, fps, res);

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
      QString res = QString::fromStdString(pair.first);
      ResolutionConfig const& config = pair.second;

      QString text = res + (config.fpsList.size() > 1 ? " (" : " @");
      for (int i = 0; i < config.fpsList.size(); ++i)
      {
        text += QString::number(config.fpsList[i].toInt());
        if (i < config.fpsList.size() - 1) { text += ", "; }
      }
      text += config.fpsList.size() > 1 ? " fps)" : " fps";

      QRadioButton* radioButton = new QRadioButton(text);
      radioButton->setProperty("supported_resolution", res);
      radioButton->setProperty("supported_fps", config.fpsList);
      radioButton->setProperty("supported_fourcc", config.fourcc);
      radioButton->setProperty("camera_path", QString::fromStdString(cameraPath));
      buttonGroup->addButton(radioButton);
    }

    if (!buttonGroup->buttons().isEmpty()) { buttonGroup->buttons().first()->setChecked(true); }

    return buttonGroup;
  }

}    // namespace camera_utils
