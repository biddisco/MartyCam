#include <QDebug>
#include <QTime>
//
#include <iostream>
//
#include <cv.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//
#include "filter.h"
#include "imagebuffer.h"
#include "processingthread.h"
//----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer* buffer, cv::Size &size) : QThread()
{
  this->imageBuffer       = buffer;
  this->rootFilter        = NULL; 
  this->abort             = false;
  this->MotionDetecting   = true;
  this->motionPercent     = 0.0;
  this->threshold         = 2;
  this->average           = 0.01;
  this->erodeIterations   = 1;
  this->dilateIterations  = 0;
  this->displayImage      = 3;
  this->blendRatio        = 0.75;
  this->noiseBlendRatio   = 0.75;
  this->rotation          = 0;
  //
  this->imageSize         = size;
  this->cameraImage       = NULL;
  this->tempImage         = NULL;
  this->difference        = NULL;
  this->blendImage        = NULL;
  this->greyScaleImage    = NULL;
  this->thresholdImage    = NULL;
  this->movingAverage     = NULL;
  this->noiseImage        = NULL;
}
//----------------------------------------------------------------------------
ProcessingThread::~ProcessingThread()
{
  this->DeleteTemporaryStorage();
}
//----------------------------------------------------------------------------
void ProcessingThread::DeleteTemporaryStorage()
{
  this->movingAverage.release();
  this->thresholdImage.release();
  this->greyScaleImage.release();
  this->blendImage.release();
  this->difference.release();
  this->tempImage.release();
  this->noiseImage.release();
  //
  this->noiseImage        = NULL;
  this->tempImage         = NULL;
  this->difference        = NULL;
  this->blendImage        = NULL;
  this->greyScaleImage    = NULL;
  this->thresholdImage    = NULL;
  this->movingAverage     = NULL;
}
//----------------------------------------------------------------------------
void ProcessingThread::CopySettings(ProcessingThread *thread)
{
  this->motionPercent     = thread->motionPercent;
  this->threshold         = thread->threshold;
  this->average           = thread->average;
  this->erodeIterations   = thread->erodeIterations;
  this->dilateIterations  = thread->dilateIterations;
  this->displayImage      = thread->displayImage;
  this->blendRatio        = thread->blendRatio;
  this->rotation          = thread->rotation;
}
//----------------------------------------------------------------------------
void ProcessingThread::setRotation(int value) { 
  if (this->rotation==value) {
    return;
  }
  this->rotation = value; 
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
  static int framenum = 0;
  while (!this->abort) {
    this->cameraImage = imageBuffer->getFrame();
    // if camera not working or disconnected, abort
    if (this->cameraImage.empty()) {
      msleep(100);
      continue;
    }
    cv::Mat workingImage = this->cameraImage;
    cv::Size workingSize = this->cameraImage.size();
    if (workingSize.width!=this->imageSize.width ||
      workingSize.height!=this->imageSize.height) 
    {
      this->DeleteTemporaryStorage();
      this->imageSize = workingSize;
    }
    //
    // allocate image arrays, only executed on first pass (or size change)
    //
    if (this->difference.empty()) {
      //
      this->movingAverage   = cv::Mat( workingSize, CV_32FC3);
      this->greyScaleImage  = cv::Mat( workingSize, CV_8UC1);
      this->thresholdImage  = cv::Mat( workingSize, CV_8UC1);
      this->blendImage      = cv::Mat( workingSize, CV_8UC3);
      this->noiseImage      = cv::Mat( workingSize, CV_32FC1);
      this->difference      = workingImage.clone();
      this->tempImage       = workingImage.clone();
      //
      workingImage.convertTo(this->movingAverage, this->movingAverage.type(), 1.0, 0.0);

      // initialize font and precompute text size
      cvInitFont(&this->font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      cv::getTextSize( timestring.toAscii().data(), CV_FONT_HERSHEY_PLAIN, 1.0, 1, NULL);
    }

    cv::Mat shownImage;
    if (this->MotionDetecting) {
      cv::accumulateWeighted(workingImage, this->movingAverage, this->average);

      // Convert the type of the moving average from float to char
      this->movingAverage.convertTo(this->tempImage, this->tempImage.type(), 1.0, 0.0);

      // Subtract the current frame from the moving average.
      cv::absdiff(workingImage, this->tempImage, this->difference);

      // Convert the image to grayscale.
      cv::cvtColor(this->difference, this->greyScaleImage, CV_RGB2GRAY);

      // Threshold the image to black/white off/on.
      cv::threshold(this->greyScaleImage, this->thresholdImage, threshold, 255, CV_THRESH_BINARY);

      // Add current changing pixels to our noise map
      this->updateNoiseMap(this->thresholdImage, this->noiseBlendRatio);

      // Erode and Dilate to denoise and produce blobs
      if (this->erodeIterations>0) {
        cv::erode(this->thresholdImage, this->thresholdImage, Mat(), Point(-1,-1), this->erodeIterations);
      }
      if (this->dilateIterations>0) {
        cv::dilate(this->thresholdImage, this->thresholdImage, Mat(), Point(-1,-1), this->dilateIterations);
      }

      // Convert the image to grayscale.
      cv::cvtColor(this->thresholdImage, this->blendImage, CV_GRAY2RGB);
      this->countPixels(this->thresholdImage);

      switch (this->displayImage) {
      case 0:
        shownImage = workingImage;
        break;
      case 1:
        shownImage = this->movingAverage;
        break;
      case 2:
        shownImage = this->thresholdImage;
        break;
      default:
      case 3:
        /* dst = src1 * alpha + src2 * beta + gamma */
        cv::addWeighted(workingImage, this->blendRatio, this->blendImage, 1.0-this->blendRatio, 0.0, this->blendImage);
        shownImage = this->blendImage;
        break;
      case 4:
        cv::bitwise_or(workingImage, this->blendImage, this->blendImage);
        shownImage = this->blendImage;
        break;
      case 5:
        shownImage = this->noiseImage;
        break;
      }
    }
    //
    // Add time and data to image
    //
    QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
    cv::putText(shownImage, timestring.toAscii().data(), 
      cvPoint(shownImage.size().width - text_size.width - 4, text_size.height+4), 
      CV_FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 255, 0));
    //
    // Pass final image to GUI
    //
    if (rootFilter) {
      rootFilter->process(shownImage);
    }
    //
    // Release our copy of original captured image
    //
    this->cameraImage.release();
    //    std::cout << "Processed frame " << framenum++ << std::endl;
  }
}
//----------------------------------------------------------------------------
void ProcessingThread::countPixels(const cv::Mat &image) 
{
  // Acquire image unfo
  int height    = image.size().height;
  int width     = image.size().width;
  int step      = image.step[0];
  int channels  = image.channels();
  uchar *data   = image.data;

  // Begin
  int white = 0;
  for (int i=0;i<height;i++) {
    for (int j=0;j<width;j++) {
      int nonwhite = 0;
      for (int k=0;k<channels;k++) {
        if (data[i*step+j*channels+k]==255) nonwhite = 1;
        else {
          nonwhite = 0;
          break;
        }
      }
      if (nonwhite == 1) white++;
    }
  }           

  this->motionPercent = 100.0*white/(height*width);
}
//----------------------------------------------------------------------------
void ProcessingThread::updateNoiseMap(const cv::Mat &image, double noiseblend) 
{
  static int cnt = 0;
  if (cnt==0) {
    cv::rectangle(
      this->noiseImage, 
      cvPoint(0,0), this->noiseImage.size(),
      cvScalar(0), CV_FILLED);
    cnt = 0;
  }
  double fps = 10.0;
  //  cvAcc( const CvArr* image, CvArr* sum,
  //                   const CvArr* mask CV_DEFAULT(NULL) );

  // multiply the image diffs to enhance them
//  cvConvertScale( image, image, -10.0, 255.0);

  // Add to running average of 'noisy' pixels
  cv::accumulateWeighted(image, this->noiseImage, noiseblend);
//  cvThreshold(this->noiseImage, this->noiseImage, threshold, 32, CV_THRESH_BINARY);

  // invert pixels using : (255 - x)
  //  cvConvertScale( image, image, -1.0, 128.0);
  //  cvAdd(image, this->noiseImage, this->noiseImage);
  /*
  */
  cnt++;
}
//----------------------------------------------------------------------------
double ProcessingThread::getPSNR(const cv::Mat& I1, const cv::Mat& I2)
{
  cv::Mat s1;
  absdiff(I1, I2, s1);       // |I1 - I2|
  s1.convertTo(s1, CV_32F);  // cannot make a square on 8 bits
  s1 = s1.mul(s1);           // |I1 - I2|^2

  cv::Scalar s = sum(s1);         // sum elements per channel

  double sse = s.val[0] + s.val[1] + s.val[2]; // sum channels

  if( sse <= 1e-10) // for small values return zero
    return 0;
  else
  {
    double  mse =sse /(double)(I1.channels() * I1.total());
    double psnr = 10.0*log10((255*255)/mse);
    return psnr;
  }
}
//----------------------------------------------------------------------------
Scalar ProcessingThread::getMSSIM(const cv::Mat& i1, const cv::Mat& i2)
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
  GaussianBlur(I1, mu1, Size(11, 11), 1.5);
  GaussianBlur(I2, mu2, Size(11, 11), 1.5);

  cv::Mat mu1_2   =   mu1.mul(mu1);
  cv::Mat mu2_2   =   mu2.mul(mu2);
  cv::Mat mu1_mu2 =   mu1.mul(mu2);

  cv::Mat sigma1_2, sigma2_2, sigma12;

  GaussianBlur(I1_2, sigma1_2, Size(11, 11), 1.5);
  sigma1_2 -= mu1_2;

  GaussianBlur(I2_2, sigma2_2, Size(11, 11), 1.5);
  sigma2_2 -= mu2_2;

  GaussianBlur(I1_I2, sigma12, Size(11, 11), 1.5);
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

  Scalar mssim = mean( ssim_map ); // mssim = average of ssim map
  return mssim;
}
