#include <QDebug>
#include <QTime>
//
#include <iostream>
//
#include <cv.h>
#include <opencv2/imgproc/imgproc.hpp>
//
#include "filter.h"
#include "imagebuffer.h"
#include "processingthread.h"
//----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer* buffer, CvSize &size) : QThread()
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
  cvReleaseImage(&this->movingAverage);
  cvReleaseImage(&this->thresholdImage);
  cvReleaseImage(&this->greyScaleImage);
  cvReleaseImage(&this->blendImage);
  cvReleaseImage(&this->difference);
  cvReleaseImage(&this->tempImage);
  cvReleaseImage(&this->noiseImage);
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
    if (!this->cameraImage) {
      msleep(100);
      continue;
    }
    IplImage *workingImage = this->cameraImage;
    CvSize     workingSize = cvSize(this->cameraImage->width, this->cameraImage->height);
    if (workingSize.width!=this->imageSize.width ||
      workingSize.height!=this->imageSize.height) 
    {
      this->DeleteTemporaryStorage();
      this->imageSize = workingSize;
    }
    //
    // allocate image arrays, only executed on first pass (or size change)
    //
    if (!this->difference) {
      //
      this->movingAverage   = cvCreateImage( workingSize, IPL_DEPTH_32F, 3);
      this->greyScaleImage  = cvCreateImage( workingSize, IPL_DEPTH_8U, 1);
      this->thresholdImage  = cvCreateImage( workingSize, IPL_DEPTH_8U, 1);
      this->blendImage      = cvCreateImage( workingSize, IPL_DEPTH_8U, 3);
      this->noiseImage      = cvCreateImage( workingSize, IPL_DEPTH_32F, 1);
      this->difference      = cvCloneImage( workingImage );
      this->tempImage       = cvCloneImage( workingImage );
      cvConvertScale(workingImage, this->movingAverage, 1.0, 0.0);

      // initialize font and precompute text size
      cvInitFont(&this->font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      cvGetTextSize( timestring.toAscii(), &this->font, &this->text_size, NULL);
    }

    IplImage *shownImage;
    if (this->MotionDetecting) {
      cvRunningAvg(workingImage, this->movingAverage, this->average, NULL);

      // Convert the type of the moving average from float to char
      cvConvertScale(this->movingAverage, this->tempImage, 1.0, 0.0);

      // Subtract the current frame from the moving average.
      cvAbsDiff(workingImage, this->tempImage, this->difference);

      // Convert the image to grayscale.
      cvCvtColor(this->difference, this->greyScaleImage, CV_RGB2GRAY);

      // Threshold the image to black/white off/on.
      cvThreshold(this->greyScaleImage, this->thresholdImage, threshold, 255, CV_THRESH_BINARY);

      // Add current changing pixels to our noise map
      this->updateNoiseMap(this->thresholdImage, this->noiseBlendRatio);

      // Erode and Dilate to denoise and produce blobs
      if (this->erodeIterations>0) {
        cvErode(this->thresholdImage, this->thresholdImage, 0, this->erodeIterations);
      }
      if (this->dilateIterations>0) {
        cvDilate(this->thresholdImage, this->thresholdImage, 0, this->dilateIterations);
      }

      // Convert the image to grayscale.
      cvCvtColor(this->thresholdImage, this->blendImage, CV_GRAY2RGB);
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
        cvAddWeighted(workingImage, this->blendRatio, this->blendImage, 1.0-this->blendRatio, 0.0, this->blendImage);
        shownImage = this->blendImage;
        break;
      case 4:
        cvOr(workingImage, this->blendImage, this->blendImage, NULL);
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
    cvPutText(shownImage, timestring.toAscii(), 
      cvPoint(shownImage->width-text_size.width-4, text_size.height+4), 
      &this->font, cvScalar(255, 255, 255, 0));
    //
    // Pass final image to GUI
    //
    if (rootFilter) {
      rootFilter->process(shownImage);
    }
    //
    // Release our copy of original captured image
    //
    cvReleaseImage(&this->cameraImage);
    //    std::cout << "Processed frame " << framenum++ << std::endl;
  }
}
//----------------------------------------------------------------------------
void ProcessingThread::countPixels(IplImage *image) 
{
  // Acquire image unfo
  int height    = image->height;
  int width     = image->width;
  int step      = image->widthStep;
  int channels  = image->nChannels;
  uchar *data = (uchar *)image->imageData;

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
void ProcessingThread::updateNoiseMap(IplImage *image, double noiseblend) 
{
  static int cnt = 0;
  if (cnt==0) {
    cvRectangle(
      this->noiseImage, 
      cvPoint(0,0), cvPoint(this->noiseImage->width, this->noiseImage->height),
      cvScalar(0), CV_FILLED);
    cnt = 0;
  }
  double fps = 10.0;
  //  cvAcc( const CvArr* image, CvArr* sum,
  //                   const CvArr* mask CV_DEFAULT(NULL) );

  // multiply the image diffs to enhance them
//  cvConvertScale( image, image, -10.0, 255.0);

  // Add to running average of 'noisy' pixels
  cvRunningAvg(image, this->noiseImage, noiseblend, NULL);
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
Scalar ProcessingThread::getMSSIM(const Mat& i1, const Mat& i2)
{
  const double C1 = 6.5025, C2 = 58.5225;
  /***************************** INITS **********************************/
  int d = CV_32F;

  Mat I1, I2;
  i1.convertTo(I1, d);           // cannot calculate on one byte large values
  i2.convertTo(I2, d);

  Mat I2_2   = I2.mul(I2);        // I2^2
  Mat I1_2   = I1.mul(I1);        // I1^2
  Mat I1_I2  = I1.mul(I2);        // I1 * I2

  /***********************PRELIMINARY COMPUTING ******************************/

  Mat mu1, mu2;   //
  GaussianBlur(I1, mu1, Size(11, 11), 1.5);
  GaussianBlur(I2, mu2, Size(11, 11), 1.5);

  Mat mu1_2   =   mu1.mul(mu1);
  Mat mu2_2   =   mu2.mul(mu2);
  Mat mu1_mu2 =   mu1.mul(mu2);

  Mat sigma1_2, sigma2_2, sigma12;

  GaussianBlur(I1_2, sigma1_2, Size(11, 11), 1.5);
  sigma1_2 -= mu1_2;

  GaussianBlur(I2_2, sigma2_2, Size(11, 11), 1.5);
  sigma2_2 -= mu2_2;

  GaussianBlur(I1_I2, sigma12, Size(11, 11), 1.5);
  sigma12 -= mu1_mu2;

  ///////////////////////////////// FORMULA ////////////////////////////////
  Mat t1, t2, t3;

  t1 = 2 * mu1_mu2 + C1;
  t2 = 2 * sigma12 + C2;
  t3 = t1.mul(t2);              // t3 = ((2*mu1_mu2 + C1).*(2*sigma12 + C2))

  t1 = mu1_2 + mu2_2 + C1;
  t2 = sigma1_2 + sigma2_2 + C2;
  t1 = t1.mul(t2);               // t1 =((mu1_2 + mu2_2 + C1).*(sigma1_2 + sigma2_2 + C2))

  Mat ssim_map;
  divide(t3, t1, ssim_map);      // ssim_map =  t3./t1;

  Scalar mssim = mean( ssim_map ); // mssim = average of ssim map
  return mssim;
}
