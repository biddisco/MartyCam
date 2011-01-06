#include <QDebug>
#include "filter.h"
#include "imagebuffer.h"
#include "processingthread.h"
//----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer* buffer) : 
  QThread(), imageBuffer(buffer), rootFilter(0), flipVertical(false)
{
  this->abort = false;
  this->MotionDetecting  = true;
  this->difference = NULL;
  this->tempImage = NULL;
  this->currentImage = NULL;
  this->threshold = 7;
  this->average = 0.4;
  this->erodeIterations = 2;
  this->dilateIterations = 4;

  this->motionPercent = 0.0;
  this->displayImage = 3;
  this->blendRatio   = 0.75;

  this->imageSize.width = 640;
  this->imageSize.height = 480;
  this->movingAverage  = cvCreateImage( imageSize, IPL_DEPTH_32F, 3);
  this->thresholdImage = cvCreateImage( imageSize, IPL_DEPTH_8U, 1);
  this->blendImage     = cvCreateImage( imageSize, IPL_DEPTH_8U, 3);
}
//----------------------------------------------------------------------------
ProcessingThread::~ProcessingThread()
{
  cvReleaseImage(&this->movingAverage);
  cvReleaseImage(&this->thresholdImage);
  cvReleaseImage(&this->blendImage);
  cvReleaseImage(&this->difference);
  cvReleaseImage(&this->tempImage);
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
  while (!this->abort) {
    this->currentImage = imageBuffer->getFrame();
    if (!this->difference) {
      this->difference = cvCloneImage( this->currentImage );
      this->tempImage = cvCloneImage(this->currentImage );
      cvConvertScale(this->currentImage, movingAverage, 1.0, 0.0);
    }
    if (this->currentImage) {
      if (flipVertical) {
        cvFlip(this->currentImage, 0, 0);
      }
      if (this->MotionDetecting) {
        cvRunningAvg(this->currentImage, this->movingAverage, this->average, NULL);

        // Convert the type of the moving average from float to char
        cvConvertScale(this->movingAverage, this->tempImage, 1.0, 0.0);

        // Subtract the current frame from the moving average.
        cvAbsDiff(this->currentImage, this->tempImage, this->difference);

        // Convert the image to grayscale.
        cvCvtColor(this->difference, this->thresholdImage, CV_RGB2GRAY);

        // Threshold the image to black/white off/on.
        cvThreshold(this->thresholdImage, this->thresholdImage, threshold, 255, CV_THRESH_BINARY);

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

        /* dst = src1 * alpha + src2 * beta + gamma */
        cvAddWeighted(currentImage, this->blendRatio, this->blendImage, 1.0-this->blendRatio, 0.0, this->blendImage);
      }
      
      if (rootFilter) {
        switch (this->displayImage) {
          case 0:
            rootFilter->processPoint(this->currentImage);
            break;
          case 1:
            rootFilter->processPoint(this->movingAverage);
            break;
          case 2:
            rootFilter->processPoint(this->thresholdImage);
            break;
          default:
          case 3:
            rootFilter->processPoint(this->blendImage);
            break;
        }
      }
      cvReleaseImage(&this->currentImage);
    }
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
