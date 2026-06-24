#include "martycam/core/PSNRFilter.h"
#include <iostream>
//
#include <opencv2/imgproc/imgproc.hpp>

//----------------------------------------------------------------------------
PSNRFilter::PSNRFilter()
{
  this->minClamp = 25.0;
  this->maxClamp = 45.0;
  this->TotalNoise = 0.0;
  this->PSNR = 100.0;
}

//----------------------------------------------------------------------------
void PSNRFilter::process(cv::Mat const& image)
{
  if (this->lastFrame.empty() || image.size() != this->lastFrame.size())
  {
    this->PSNR = this->minClamp;
  }
  else
  {
    this->PSNR = this->getPSNR(image, this->lastFrame);
    if (this->PSNR < this->minClamp) this->PSNR = this->minClamp;
    if (this->PSNR > this->maxClamp) this->PSNR = this->maxClamp;
  }
  // rescale from 0-100 for convenient plotting with other numbers
  this->PSNR = 100.0 - 99.9 * (this->PSNR - this->minClamp) / (this->maxClamp - this->minClamp);

  this->lastFrame = image;
}

//----------------------------------------------------------------------------
double PSNRFilter::getPSNR(cv::Mat const& I1, cv::Mat const& I2)
{
  // need more than 8 bits for image multiply
  cv::Mat diff = cv::Mat(I1.size(), CV_32FC3);
  cv::absdiff(I1, I2, diff);    // |I1 - I2|
  diff = diff.mul(diff);        // |I1 - I2|^2
  //
  // sum elements per channel
  cv::Scalar s = sum(diff);
  //
  // sum channels
  this->TotalNoise = s.val[0] + s.val[1] + s.val[2];
  double mse = this->TotalNoise / (double) (I1.channels() * I1.total());
  //
  if (this->TotalNoise <= 1)
  {
    // for small values return 50
    return this->maxClamp;
  }
  else
  {
    double psnr = 10.0 * log10((255 * 255) / mse);
    return psnr;
  }
}

//----------------------------------------------------------------------------
