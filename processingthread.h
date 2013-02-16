#ifndef PROCESSING_THREAD_H
#define PROCESSING_THREAD_H

#include <QThread>

#ifndef Q_MOC_RUN
 #include <boost/circular_buffer.hpp>
#endif

#include "cv.h"
#include "highgui.h"
#include "opencv2/core/core.hpp"

class ImageBuffer;
class Filter;

class ProcessingThread : public QThread {
public: 
   ProcessingThread(ImageBuffer* buffer, CvSize &size);
  ~ProcessingThread();
  //
  void CopySettings(ProcessingThread *thread);
  void DeleteTemporaryStorage();
  // 
  void   countPixels(IplImage *image);
  void   updateNoiseMap(IplImage *image);
  double getMotionPercent() { return this->motionPercent; }
  //
  void setRootFilter(Filter* filter) { rootFilter = filter; }
  void setMotionDetecting(bool md) { this->MotionDetecting = md; }
  void setThreshold(int val) { threshold = val; }
  void setAveraging(double val) { average = val; }
  void setErodeIterations(int val) { erodeIterations = val; }
  void setDilateIterations(int val) { dilateIterations = val; }
  void setDisplayImage(int image) { displayImage = image; }
  void setBlendRatio(double ratio) { this->blendRatio = ratio; }
  void setRotation(int value);

  void run();
  void setAbort(bool a) { this->abort = a; }

  double getPSNR(const cv::Mat& I1, const cv::Mat& I2);

private:
  ImageBuffer *imageBuffer;
  Filter      *rootFilter;
  bool         MotionDetecting;
  int          threshold;
  double       average;
  int          erodeIterations;
  int          dilateIterations;
  double       motionPercent;
  int          displayImage;
  double       blendRatio;
  bool         abort;
  int          rotation;

  //Images to use in the program.
  CvSize     imageSize;
  IplImage  *cameraImage;
  IplImage  *greyScaleImage;
  IplImage  *thresholdImage;
  IplImage  *blendImage;
  IplImage  *movingAverage;
  IplImage  *difference;
  IplImage  *tempImage;
  IplImage  *noiseImage;
  CvFont     font;
  CvSize     text_size;
#ifndef Q_MOC_RUN
  boost::circular_buffer<IplImage*> RecordBuffer;
#endif
};

#endif
