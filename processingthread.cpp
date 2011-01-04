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
  this->temp = NULL;
  this->colourImage = NULL;
  this->threshold = 10;
  this->average = 0.6;
	this->erodeBlockSize = 4;
  this->motionPercent = 0.0;

  this->imageSize.width = 640;
  this->imageSize.height = 480;
  this->movingAverage = cvCreateImage( imageSize, IPL_DEPTH_32F, 3);
  this->greyImage     = cvCreateImage( imageSize, IPL_DEPTH_8U, 1);
  this->blendImage    = cvCreateImage( imageSize, IPL_DEPTH_8U, 3);
}
//----------------------------------------------------------------------------
ProcessingThread::~ProcessingThread()
{
  cvReleaseImage(&this->greyImage);
  cvReleaseImage(&this->movingAverage);
  cvReleaseImage(&this->blendImage);
  cvReleaseImage(&this->difference);
  cvReleaseImage(&this->temp);
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
	while (true) {
		this->colourImage = imageBuffer->getFrame();
    if (!this->difference) {
      this->difference = cvCloneImage( this->colourImage );
      this->temp = cvCloneImage(this->colourImage );
      cvConvertScale(this->colourImage, movingAverage, 1.0, 0.0);
    }
		if (this->colourImage) {
			if (flipVertical) {
				cvFlip(this->colourImage, 0, 0);
			}
      if (this->MotionDetecting) {
        cvRunningAvg(colourImage, movingAverage, this->average, NULL);

        //Convert the scale of the moving average.
        cvConvertScale(movingAverage,temp, 1.0, 0.0);

        //Minus the current frame from the moving average.
        cvAbsDiff(colourImage,temp,difference);

        //Convert the image to grayscale.
        cvCvtColor(difference,greyImage,CV_RGB2GRAY);

        //Convert the image to black and white.
        cvThreshold(greyImage, greyImage, threshold, 255, CV_THRESH_BINARY);

        //Dilate and erode to get people blobs
        cvDilate(greyImage, greyImage, 0, this->erodeBlockSize);
        cvErode(greyImage, greyImage, 0, this->erodeBlockSize);

        //Convert the image to grayscale.
        cvCvtColor(greyImage, this->blendImage, CV_GRAY2RGB);
        this->countPixels(greyImage);

        /* dst = src1 * alpha + src2 * beta + gamma */
	      cvAddWeighted(colourImage, 0.75, this->blendImage, 0.25, 0.0, colourImage);
      }
			
			if(rootFilter) {
			//	// qDebug() << "about to notify the root listener of the frame";
				rootFilter->processPoint(colourImage);
			//	// qDebug() << "done notification";
			}
      cvReleaseImage(&this->colourImage);
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
