#include "filter.h"
#include "MotionFilter.h"
#include "PSNRFilter.h"
//
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//
#include <QDateTime>
#include <QString>
//----------------------------------------------------------------------------
MotionFilter::MotionFilter()
{
  this->imageSize         = cv::Size(-1,-1);
  this->tempImage         = NULL;
  this->difference        = NULL;
  this->blendImage        = NULL;
  this->greyScaleImage    = NULL;
  this->thresholdImage    = NULL;
  this->movingAverage     = NULL;
  this->noiseImage        = NULL;
  //
  this->PSNR_Filter       = new PSNRFilter();
  this->renderer          = NULL;
  //
  this->motionPercent     = 0.0;
  this->threshold         = 2;
  this->average           = 0.01;
  this->erodeIterations   = 1;
  this->dilateIterations  = 0;
  this->displayImage      = 3;
  this->blendRatio        = 0.75;
  this->noiseBlendRatio   = 0.75;
}
//----------------------------------------------------------------------------
MotionFilter::~MotionFilter()
{
  delete this->PSNR_Filter;
  this->DeleteTemporaryStorage();
}
//----------------------------------------------------------------------------
void MotionFilter::DeleteTemporaryStorage()
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
void MotionFilter::process(const cv::Mat &image)
{
  if (this->lastFrame.empty() || image.size()!=this->lastFrame.size()) {

  }
  else {
    cv::Mat workingImage = image;
    cv::Size workingSize = image.size();
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
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      this->text_size = cv::getTextSize( timestring.toAscii().data(), CV_FONT_HERSHEY_PLAIN, 1.0, 1, NULL);
    }

    cv::Mat shownImage;
    if (1) {
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
        cv::erode(this->thresholdImage, this->thresholdImage, cv::Mat(), cv::Point(-1,-1), this->erodeIterations);
      }
      if (this->dilateIterations>0) {
        cv::dilate(this->thresholdImage, this->thresholdImage, cv::Mat(), cv::Point(-1,-1), this->dilateIterations);
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
    this->PSNR_Filter->process(image);
    //
    // Add time and data to image
    //
    QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
    cv::putText(shownImage, timestring.toAscii().data(), 
      cvPoint(shownImage.size().width - text_size.width - 4, text_size.height+4), 
      CV_FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(255, 255, 255, 0), 1);
    //
    // Pass final image to GUI
    //
    if (renderer) {
      renderer->process(shownImage);
    }
    //    std::cout << "Processed frame " << framenum++ << std::endl;
  }
  //
  this->lastFrame = image.clone();
}
//----------------------------------------------------------------------------
void MotionFilter::updateNoiseMap(const cv::Mat &image, double noiseblend) 
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
void MotionFilter::countPixels(const cv::Mat &image) 
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
