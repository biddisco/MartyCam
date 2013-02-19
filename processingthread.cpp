#include <QDebug>
#include <QTime>
//
#include <iostream>
//
#include <cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//
#include "processingthread.h"
//
#include "imagebuffer.h"
//
#include "filter.h"
#include "PSNRFilter.h"
#include "GraphUpdateFilter.h"
//
//----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer* buffer, cv::Size &size) : QThread()
{
  this->imageBuffer       = buffer;
  this->graphFilter       = new GraphUpdateFilter();
  this->motionFilter      = new MotionFilter();
  this->abort             = false;
}
//----------------------------------------------------------------------------
ProcessingThread::~ProcessingThread()
{
  delete this->graphFilter;
  delete this->motionFilter;
}
//----------------------------------------------------------------------------
void ProcessingThread::CopySettings(ProcessingThread *thread)
{
/*
  this->motionPercent     = thread->motionPercent;
  this->threshold         = thread->threshold;
  this->average           = thread->average;
  this->erodeIterations   = thread->erodeIterations;
  this->dilateIterations  = thread->dilateIterations;
  this->displayImage      = thread->displayImage;
  this->blendRatio        = thread->blendRatio;
  this->rotation          = thread->rotation;
*/
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
  int framenum = 0;
  while (!this->abort) {
    // blocking : waits until next image is available if necessary
    cv::Mat cameraImage = imageBuffer->getFrame();
    // if camera not working or disconnected, abort
    if (cameraImage.empty()) {
      msleep(100);
      continue;
    }
    //
    this->motionFilter->process(cameraImage);
    this->graphFilter->process(
      this->motionFilter->PSNR_Filter->PSNR, 
      this->motionFilter->motionPercent,
      framenum++,
      50.0);

    //
    // if an event was triggered, start recording
    //
//    if (eventvalue==100 && this->ui.RecordingEnabled->isChecked()) {
//      this->settingsWidget->RecordAVI(true);
//    }
     emit (NewData());
  }
}
//----------------------------------------------------------------------------
cv::Scalar ProcessingThread::getMSSIM(const cv::Mat& i1, const cv::Mat& i2)
{
  const double C1 = 6.5025, C2 = 58.5225;
  /***************************** INITS **********************************/
  int d = CV_32F;

  cv::Mat I1, I2;
  i1.convertTo(I1, d);           // cannot calculate on one byte large values
  i2.convertTo(I2, d);

  cv::Mat I2_2   = I2.mul(I2);        // I2^2
  cv::Mat I1_2   = I1.mul(I1);        // I1^2
  cv::Mat I1_I2  = I1.mul(I2);        // I1 * I2

  /***********************PRELIMINARY COMPUTING ******************************/

  cv::Mat mu1, mu2;   //
  GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
  GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

  cv::Mat mu1_2   =   mu1.mul(mu1);
  cv::Mat mu2_2   =   mu2.mul(mu2);
  cv::Mat mu1_mu2 =   mu1.mul(mu2);

  cv::Mat sigma1_2, sigma2_2, sigma12;

  GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
  sigma1_2 -= mu1_2;

  GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
  sigma2_2 -= mu2_2;

  GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
  sigma12 -= mu1_mu2;

  ///////////////////////////////// FORMULA ////////////////////////////////
  cv::Mat t1, t2, t3;

  t1 = 2 * mu1_mu2 + C1;
  t2 = 2 * sigma12 + C2;
  t3 = t1.mul(t2);              // t3 = ((2*mu1_mu2 + C1).*(2*sigma12 + C2))

  t1 = mu1_2 + mu2_2 + C1;
  t2 = sigma1_2 + sigma2_2 + C2;
  t1 = t1.mul(t2);               // t1 =((mu1_2 + mu2_2 + C1).*(sigma1_2 + sigma2_2 + C2))

  cv::Mat ssim_map;
  divide(t3, t1, ssim_map);      // ssim_map =  t3./t1;

  cv::Scalar mssim = cv::mean( ssim_map ); // mssim = average of ssim map
  return mssim;
}
