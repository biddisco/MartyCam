#include "PSNRFilter.h"

//----------------------------------------------------------------------------
PSNRFilter::PSNRFilter()
{
  this->minClamp = 25.0;
  this->maxClamp = 45.0;
  this->PSNR     = 100.0;
}
//----------------------------------------------------------------------------
void PSNRFilter::process(const cv::Mat &image)
{
  if (this->lastFrame.empty() || image.size()!=this->lastFrame.size()) {
    this->PSNR = this->minClamp;
  }
  else {
    this->PSNR = this->getPSNR(image, this->lastFrame);
    if (this->PSNR<this->minClamp) this->PSNR=this->minClamp;
    if (this->PSNR>this->maxClamp) this->PSNR=this->maxClamp;
  }
  // rescale from 0-100 for convenient plotting with other numbers
  this->PSNR = 100.0 - 99.9*(this->PSNR-this->minClamp)/(this->maxClamp-this->minClamp);

  this->lastFrame = image.clone();
}
//----------------------------------------------------------------------------
double PSNRFilter::getPSNR(const cv::Mat& I1, const cv::Mat& I2)
{
  // need more than 8 bits for image multiply
  cv::Mat s1 = cv::Mat( I1.size(), CV_32FC3);
  cv::absdiff(I1, I2, s1);       // |I1 - I2|
  s1 = s1.mul(s1);           // |I1 - I2|^2

  cv::Scalar s = sum(s1);         // sum elements per channel

  double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels

  if( sse <= 1e-10) {
    // for small values return 50
    return this->maxClamp;
  }
  else {
    double  mse =sse /(double)(I1.channels() * I1.total());
    double psnr = 10.0*log10((255*255)/mse);
    return psnr;
  }
}
//----------------------------------------------------------------------------
