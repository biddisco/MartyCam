#include <QDebug>
#include "filter.h"
#include "imagebuffer.h"
#include "processingthread.h"
//----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer* buffer) : 
  QThread(), imageBuffer(buffer), rootFilter(0), flipVertical(false)
{
  this->MotionDetecting  = true;
  this->difference = NULL;
  this->tempImage = NULL;
  this->currentImage = NULL;
  this->threshold = 10;
  this->average = 0.6;
	this->erodeBlockSize = 4;
  this->motionPercent = 0.0;
  this->displayImage = 3;

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
	while (true) {
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
        cvRunningAvg(currentImage, movingAverage, this->average, NULL);

        //Convert the scale of the moving average.
        cvConvertScale(movingAverage,tempImage, 1.0, 0.0);

        //Minus the current frame from the moving average.
        cvAbsDiff(currentImage, tempImage, difference);

        //Convert the image to grayscale.
        cvCvtColor(difference,thresholdImage,CV_RGB2GRAY);

        //Convert the image to black and white.
        cvThreshold(thresholdImage, thresholdImage, threshold, 255, CV_THRESH_BINARY);

        //Dilate and erode to get people blobs
        cvDilate(thresholdImage, thresholdImage, 0, this->erodeBlockSize);
        cvErode(thresholdImage, thresholdImage, 0, this->erodeBlockSize);

        //Convert the image to grayscale.
        cvCvtColor(thresholdImage, this->blendImage, CV_GRAY2RGB);
        this->countPixels(thresholdImage);

        /* dst = src1 * alpha + src2 * beta + gamma */
	      cvAddWeighted(currentImage, 0.75, this->blendImage, 0.25, 0.0, this->blendImage);
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
