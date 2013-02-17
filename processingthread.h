#ifndef PROCESSING_THREAD_H
#define PROCESSING_THREAD_H

#include <QThread>

#ifndef Q_MOC_RUN
 #include <boost/circular_buffer.hpp>
#endif

//
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

class ImageBuffer;
class Filter;

using namespace cv;

class ProcessingThread : public QThread {
Q_OBJECT;
public: 
   ProcessingThread(ImageBuffer* buffer, CvSize &size);
  ~ProcessingThread();
  //
  void CopySettings(ProcessingThread *thread);
  void DeleteTemporaryStorage();
  // 
  void   countPixels(IplImage *image);
  void   updateNoiseMap(IplImage *image, double noiseblend);
  double getMotionPercent() { return this->motionPercent; }
  //
  void setRootFilter(Filter* filter) { rootFilter = filter; }
  void setMotionDetecting(bool md) { this->MotionDetecting = md; }
  void setThreshold(int val) { threshold = val; }
  void setAveraging(double val) { average = val; }
  void setErodeIterations(int val) { erodeIterations = val; }
  void setDilateIterations(int val) { dilateIterations = val; }
  void setDisplayImage(int image) { displayImage = image; }
  void setBlendRatios(double ratio1, double ratio2) { 
    this->blendRatio = ratio1;
    this->noiseBlendRatio = ratio2; 
  }
  void setRotation(int value);

  void run();
  void setAbort(bool a) { this->abort = a; }

  double getPSNR(const Mat& I1, const Mat& I2);
  Scalar getMSSIM( const Mat& i1, const Mat& i2);

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
  double       noiseBlendRatio;
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
