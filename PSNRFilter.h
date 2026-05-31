#ifndef PSNRFILTER_H
#define PSNRFILTER_H

#include "opencv2/core/core.hpp"
#include "opencv2/core/core_c.h"

//
// Base class for filter/processor objects
//
class PSNRFilter
{
  public:
  PSNRFilter();
  //
  virtual void process(cv::Mat const& image);
  double getPSNR(cv::Mat const& I1, cv::Mat const& I2);
  //
  cv::Mat lastFrame;
  double PSNR;
  double TotalNoise;

  double minClamp;
  double maxClamp;

  protected:
};

#endif
