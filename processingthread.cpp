#include <QDebug>
#include <QTime>
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
  this->threshold         = 7;
  this->average           = 0.4;
  this->erodeIterations   = 2;
  this->dilateIterations  = 4;
  this->displayImage      = 3;
  this->blendRatio        = 0.75;
  this->rotation          = 0;
  this->storageInvalid    = false;
  //
  this->imageSize         = size;
  this->cameraImage       = NULL;
  this->tempImage         = NULL;
  this->difference        = NULL;
  this->blendImage        = NULL;
  this->thresholdImage    = NULL;
  this->movingAverage     = NULL;
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
  cvReleaseImage(&this->blendImage);
  cvReleaseImage(&this->difference);
  cvReleaseImage(&this->tempImage);
  this->tempImage         = NULL;
  this->difference        = NULL;
  this->blendImage        = NULL;
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
  this->storageInvalid = true;
  this->rotation = value; 
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
  while (!this->abort) {
    this->cameraImage = imageBuffer->getFrame();
    // if camera not working or disconnected, abort
    if (!this->cameraImage) {
      msleep(100);
      continue;
    }
//    if (this->storageInvalid) {
//      this->DeleteTemporaryStorage();
//      this->storageInvalid = false;
//    }
    IplImage *workingImage = this->cameraImage;
    CvSize     workingSize = cvSize(this->cameraImage->width, this->cameraImage->height);
    if (workingSize.width!=this->imageSize.width ||
        workingSize.height!=this->imageSize.height) 
    {
      this->DeleteTemporaryStorage();
      this->imageSize = workingSize;
    }
    //
    // only executed on first pass 
    //
    if (!this->difference) {
      //
      this->movingAverage   = cvCreateImage( workingSize, IPL_DEPTH_32F, 3);
      this->thresholdImage  = cvCreateImage( workingSize, IPL_DEPTH_8U, 1);
      this->blendImage      = cvCreateImage( workingSize, IPL_DEPTH_8U, 3);
      this->difference      = cvCloneImage(workingImage);
      this->tempImage       = cvCloneImage(workingImage);
      cvConvertScale(workingImage, this->movingAverage, 1.0, 0.0);

      // initialize font and precompute text size
      cvInitFont(&this->font, CV_FONT_HERSHEY_PLAIN, 1.0, 1.0, 0, 1, CV_AA);
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      cvGetTextSize( timestring.toAscii(), &this->font, &this->text_size, NULL);
    }
    if (this->MotionDetecting) {
      cvRunningAvg(workingImage, this->movingAverage, this->average, NULL);

      // Convert the type of the moving average from float to char
      cvConvertScale(this->movingAverage, this->tempImage, 1.0, 0.0);

      // Subtract the current frame from the moving average.
      cvAbsDiff(workingImage, this->tempImage, this->difference);

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
      cvAddWeighted(workingImage, this->blendRatio, this->blendImage, 1.0-this->blendRatio, 0.0, this->blendImage);
    }
    
    if (rootFilter) {
      IplImage *shownImage;
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
          shownImage = this->blendImage;
          break;
      }
      QString timestring = QDateTime::currentDateTime().toString("dd/MM/yyyy hh:mm:ss");
      cvPutText(shownImage, timestring.toAscii(), 
        cvPoint(shownImage->width-text_size.width-4, text_size.height+4), 
        &this->font, cvScalar(255, 255, 255, 0));
      rootFilter->processPoint(shownImage);
    }
    cvReleaseImage(&this->cameraImage);
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
