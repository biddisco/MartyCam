#include "PSNRFilter.h"

//----------------------------------------------------------------------------
PSNRFilter::PSNRFilter()
{

}
//----------------------------------------------------------------------------
void PSNRFilter::process(const cv::Mat &image)
{
  if (this->lastFrame.empty() || image.size()!=this->lastFrame.size()) {
    this->PSNR = 0;
  }
  else {
    this->PSNR = this->getPSNR(image, this->lastFrame);
  }
  this->lastFrame = image;
}
//----------------------------------------------------------------------------
double PSNRFilter::getPSNR(const cv::Mat& I1, const cv::Mat& I2)
{
  cv::Mat s1;
  cv::absdiff(I1, I2, s1);       // |I1 - I2|
  s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
  s1 = s1.mul(s1);           // |I1 - I2|^2

  cv::Scalar s = sum(s1);         // sum elements per channel

  double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels

  if( sse <= 1e-10) {
    // for small values return zero
    return 0;
  }
  else {
    double  mse =sse /(double)(I1.channels() * I1.total());
    double psnr = 10.0*log10((255*255)/mse);
    return psnr;
  }
}
//----------------------------------------------------------------------------
