#include "filter.h"
#include "MotionFilter.h"
#include "PSNRFilter.h"
#include "DecayFilter.h"
//
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "opencv2/videoio/videoio_c.h"
#include "opencv2/imgproc/imgproc_c.h"
//
#include <QDateTime>
#include <QString>
//
boost::accumulators::accumulator_set<int, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> >
  acc(boost::accumulators::tag::rolling_window::window_size = 10);
//
//----------------------------------------------------------------------------
MotionFilter::MotionFilter()
{
  this->imageSize         = cv::Size(-1,-1);
  //
  this->PSNR_Filter       = new PSNRFilter();
  this->decayFilter       = new DecayFilter();
  this->renderer          = NULL;
  //
  this->triggerLevel      = 100.0;
  this->threshold         = 2;
  this->average           = 0.01;
  this->erodeIterations   = 1;
  this->dilateIterations  = 0;
  this->displayImage      = 3;
  this->blendRatio        = 0.75;
  this->noiseBlendRatio   = 0.75;
  //
  this->motionEstimate   = 0.0;
  this->logMotion        = 0.0;
  this->normalizedMotion = 0.0;
  this->rollingMean      = 0.0;
  this->eventLevel       = 0.0;
  //
  this->frameCount = 0;
}

MotionFilter::MotionFilter(MotionFilterParams mfp){
  this->imageSize         = cv::Size(-1,-1);
  //
  //
  this->PSNR_Filter       = new PSNRFilter();
  this->decayFilter       = new DecayFilter();
  this->renderer          = NULL;
  //
  this->triggerLevel      = 100.0;
  this->threshold         = mfp.threshold;
  this->average           = mfp.average;
  this->erodeIterations   = mfp.erodeIterations;
  this->dilateIterations  = mfp.dilateIterations;
  this->displayImage      = mfp.displayImage;
  this->blendRatio        = mfp.blendRatio;
  //
  this->motionEstimate   = 0.0;
  this->logMotion        = 0.0;
  this->normalizedMotion = 0.0;
  this->rollingMean      = 0.0;
  this->eventLevel       = 0.0;
  //
  this->frameCount = 0;
}
//----------------------------------------------------------------------------
MotionFilter::~MotionFilter()
{
  delete this->PSNR_Filter;
  delete this->decayFilter;
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
  this->floatImage.release();
  this->noiseImage.release();
}
//----------------------------------------------------------------------------
void MotionFilter::process(const cv::Mat &image)
{
  if (this->lastFrame.empty() || image.size()!=this->lastFrame.size()) {

  }
  else {
    cv::Mat currentFrame = image;
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
      this->floatImage      = cv::Mat( workingSize, CV_8UC3);
      this->difference      = cv::Mat( workingSize, CV_8UC3);
      //
      currentFrame.convertTo(this->movingAverage, this->movingAverage.type(), 1.0, 0.0);

      // initialize font and precompute text size
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      this->text_size = cv::getTextSize( timestring.toLatin1().data(), CV_FONT_HERSHEY_PLAIN, 1.0, 1, NULL);
    }

    cv::Mat shownImage;
    if (1) {
      // add last frame to weighted moving average
      // if average = 1 we compare this frame to last frame
      // if average = epsilon, we compare this frame to smoothed average of previous frames
      cv::accumulateWeighted(this->lastFrame, this->movingAverage, this->average);

      // Convert the current frame to float for diffing channels
      // we should leave as uchar, but his if for future use when we do more complex operations
      this->movingAverage.convertTo(this->floatImage, this->floatImage.type());

//      cv::Mat med2;
//      cv::medianBlur(this->floatImage, med2, 5);

      // get the difference of this frame from the moving average.
      cv::absdiff(currentFrame, this->floatImage, this->difference);

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
      cv::cvtColor(this->thresholdImage, this->blendImage, CV_GRAY2BGR);

      switch (this->displayImage) {
        case 0:
          shownImage = currentFrame;
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
          cv::addWeighted(currentFrame, this->blendRatio, this->blendImage, 1.0-this->blendRatio, 0.0, this->blendImage);
          shownImage = this->blendImage;
          break;
        case 4:
          cv::bitwise_or(currentFrame, this->blendImage, this->blendImage);
          shownImage = this->blendImage;
          break;
        case 5:
          shownImage = this->noiseImage;
          break;
      }
    }

    this->PSNR_Filter->process(image);

    double nonzero = cv::countNonZero(this->thresholdImage);
    double pixels = this->thresholdImage.total();
    // max 1, min 0
    this->motionEstimate = nonzero/pixels;
    // 10.0*log10(1  / 640*480) = -54dB : so max is 0, min -54
    // 10.0*log10(10 / 640*480) = -44dB :
    this->logMotion = this->motionEstimate>0 ? -10.0*log10(this->motionEstimate) : 54.0;
    // clamp values
    this->logMotion = (this->logMotion>54.0) ? 54.0 : this->logMotion;
    this->logMotion = 100.0*(54.0-this->logMotion)/54.0;
    //
    if (this->motionEstimate>0.0) {
      this->normalizedMotion = sqrt(this->PSNR_Filter->TotalNoise)/(nonzero);
    }
    this->normalizedMotion = (this->normalizedMotion>100.0) ? 100.0 : this->normalizedMotion;

    TimeValue tv(this->frameCount++, this->logMotion);
    this->decayFilter->process(tv);

    acc(this->normalizedMotion);
    this->rollingMean = boost::accumulators::rolling_mean(acc);
    //
    this->eventLevel = (rollingMean>this->triggerLevel) ? 100 : 0;

    //
    // Add time and data to image
    //
    QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
    cv::putText(shownImage, timestring.toLatin1().data(),
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
  this->lastFrame = image;
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

  this->motionEstimate = 100.0*white/(height*width);
}
