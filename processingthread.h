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
class PSNRFilter;
class GraphUpdateFilter;
#include "MotionFilter.h"

class ProcessingThread : public QThread {
Q_OBJECT;
public: 
   ProcessingThread(ImageBuffer* buffer, cv::Size &size);
  ~ProcessingThread();
  //
  void CopySettings(ProcessingThread *thread);
  void DeleteTemporaryStorage();
  // 
  double getMotionPercent() { return this->motionFilter->motionPercent; }
  //
  void setRootFilter(Filter* filter) {   this->motionFilter->renderer = filter; }
  void setThreshold(int val) { this->motionFilter->threshold = val; }
  void setAveraging(double val) { this->motionFilter->average = val; }
  void setErodeIterations(int val) { this->motionFilter->erodeIterations = val; }
  void setDilateIterations(int val) { this->motionFilter->dilateIterations = val; }
  void setDisplayImage(int image) { this->motionFilter->displayImage = image; }
  void setBlendRatios(double ratio1, double ratio2) { 
    this->motionFilter->blendRatio = ratio1;
    this->motionFilter->noiseBlendRatio = ratio2; 
  }

  void run();
  void setAbort(bool a) { this->abort = a; }

  double getPSNR();
  cv::Scalar getMSSIM(const cv::Mat& i1, const cv::Mat& i2);

  //
  GraphUpdateFilter  *graphFilter;
  MotionFilter       *motionFilter;

signals:
  void NewData();

private:
  ImageBuffer        *imageBuffer;
  bool                abort;
  //
#ifndef Q_MOC_RUN
  boost::circular_buffer<cv::Mat> RecordBuffer;
#endif
};

#endif
