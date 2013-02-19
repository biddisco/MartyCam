#ifndef MOTIONFILTER_H
#define MOTIONFILTER_H

#include "opencv2/core/core_c.h"
#include "opencv2/core/core.hpp"

class PSNRFilter;
class Filter;

//
// Filter which computes some motion estimates
//
class MotionFilter {
public:
   MotionFilter();
  ~MotionFilter();
  //
  virtual void process(const cv::Mat &image);
  //
  void DeleteTemporaryStorage();
  void updateNoiseMap(const cv::Mat &image, double noiseblend);
  void countPixels(const cv::Mat &image);

  PSNRFilter  *PSNR_Filter;
  Filter      *renderer;
  //
  // variables
  //
  int          threshold;
  double       average;
  int          erodeIterations;
  int          dilateIterations;
  double       motionPercent;
  int          displayImage;
  double       blendRatio;
  double       noiseBlendRatio;

  //
  // Temporary images 
  //
  cv::Size  imageSize;
  cv::Mat   greyScaleImage;
  cv::Mat   thresholdImage;
  cv::Mat   blendImage;
  cv::Mat   movingAverage;
  cv::Mat   difference;
  cv::Mat   tempImage;
  cv::Mat   noiseImage;
  cv::Size  text_size;
  //
  //
  cv::Mat  lastFrame;

protected:
};

#endif
