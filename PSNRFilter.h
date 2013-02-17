#ifndef PSNRFILTER_H
#define PSNRFILTER_H

#include "opencv2/core/core_c.h"
#include "opencv2/core/core.hpp"

//
// Base class for filter/processor objects
//
class PSNRFilter {
public:
  PSNRFilter();
  //
  virtual void process(const cv::Mat &image);
  double getPSNR(const cv::Mat& I1, const cv::Mat& I2);
  //
  cv::Mat lastFrame;
  double   PSNR;

protected:
};

#endif
