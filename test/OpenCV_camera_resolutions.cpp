#include <iostream>
#include <opencv2/opencv.hpp>
#include <vector>

// A simple structure to hold width and height pairs
struct Resolution
{
  int width;
  int height;
};

using namespace cv;
using namespace std;

int main()
{
  // 1. Define a list of common resolutions to test
  std::vector<Resolution> commonResolutions = {
      {320, 240},      //
      {640, 480},      //
      {800, 600},      //
      {1024, 768},     //
      {1280, 720},     // 720p
      {1920, 1080},    // 1080p
      {2560, 1440},    // 2K
      {3840, 2160}     // 4K
  };

  // Open the default video camera
  //   VideoCapture cap("/dev/video0");
  VideoCapture cap(
      "/dev/video0", cv::CAP_V4L2);    // Use V4L2 backend for better resolution support on Linux

  //   // 2. Open the default camera (index 0)
  //     cv::VideoCapture cap(0);

  if (!cap.isOpened())
  {
    std::cerr << "Error: Could not open the camera." << std::endl;
    return -1;
  }

  std::cout << "Checking supported resolutions... Please wait.\n" << std::endl;
  std::cout << "Supported Resolutions Found:" << std::endl;
  std::cout << "----------------------------" << std::endl;

  // 3. Test each resolution
  for (auto const& res : commonResolutions)
  {
    for (int fps : {30, 60})
    {
      // Force a standard frame rate first to unlock the resolution
      cap.set(cv::CAP_PROP_FPS, fps);
      // Explicitly force MJPEG on every single iteration step
      cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
      //
      cap.set(cv::CAP_PROP_FRAME_WIDTH, res.width);
      cap.set(cv::CAP_PROP_FRAME_HEIGHT, res.height);

      int actualWidth = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
      int actualHeight = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));

      if (actualWidth == res.width && actualHeight == res.height)
      {
        std::cout << " - " << res.width << " x " << res.height << " @ " << fps << "fps"
                  << std::endl;
      }
    }
  }
  // 4. Release the camera and exit
  cap.release();
  std::cout << "----------------------------" << std::endl;
  std::cout << "Done." << std::endl;

  return 0;
}