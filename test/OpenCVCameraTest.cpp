// Uncomment the following line if you are compiling this code in Visual Studio
// #include "stdafx.h"

#include <cstdlib>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>

using namespace cv;
using namespace std;

int main(int argc, char* argv[])
{
  // Headless mode can be forced with MARTYCAM_HEADLESS_TEST=1.
  // If no desktop session is detected, headless mode is enabled automatically.
  bool headlessMode = false;
  if (char const* env = std::getenv("MARTYCAM_HEADLESS_TEST"))
  {
    headlessMode = std::string(env) == "1";
  }
  else
  {
    bool const hasDisplay =
        std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr;
    headlessMode = !hasDisplay;
  }

  // Open the default video camera
  VideoCapture cap("/dev/video0");

  // if not success, exit program
  if (cap.isOpened() == false)
  {
    cout << "Cannot open the video camera" << endl;
    cin.get();    // wait for any key press
    return -1;
  }

  double dWidth = cap.get(CAP_PROP_FRAME_WIDTH);      // get the width of frames of the video
  double dHeight = cap.get(CAP_PROP_FRAME_HEIGHT);    // get the height of frames of the video

  cout << "Resolution of the video : " << dWidth << " x " << dHeight << endl;

  string window_name = "My Camera Feed";
  if (!headlessMode)
  {
    try
    {
      namedWindow(window_name);    // create a window called "My Camera Feed"
    }
    catch (cv::Exception const& ex)
    {
      cout << "Display backend is unavailable, switching to headless mode: " << ex.what() << endl;
      headlessMode = true;
    }
  }

  int framesRead = 0;
  int const maxHeadlessFrames = 100;

  while (true)
  {
    Mat frame;
    bool bSuccess = cap.read(frame);    // read a new frame from video

    // Breaking the while loop if the frames cannot be captured
    if (bSuccess == false)
    {
      cout << "Video camera is disconnected" << endl;
      cin.get();    // Wait for any key press
      break;
    }

    if (!headlessMode)
    {
      // show the frame in the created window
      imshow(window_name, frame);

      // wait for for 10 ms until any key is pressed.
      // If the 'Esc' key is pressed, break the while loop.
      // If the any other key is pressed, continue the loop
      // If any key is not pressed withing 10 ms, continue the loop
      if (waitKey(10) == 27)
      {
        cout << "Esc key is pressed by user. Stoppig the video" << endl;
        break;
      }
    }
    else
    {
      ++framesRead;
      if (framesRead >= maxHeadlessFrames)
      {
        cout << "Headless mode: captured " << framesRead << " frames successfully" << endl;
        break;
      }
    }
  }

  return 0;
}
