#ifndef PROCESSING_THREAD_H
#define PROCESSING_THREAD_H
//
#include <hpx/config.hpp>
#include <hpx/execution/execution.hpp>
#include <hpx/include/parallel_executors.hpp>
//
#include <QMutex>
#include <QWaitCondition>
//
#include <QtCore/QObject>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
//
#include <memory>
#include "martycam/core/ConcurrentCircularBuffer.h"
#include "martycam/core/fps_helper.h"
typedef std::shared_ptr<ConcurrentCircularBuffer<cv::Mat>> ImageBuffer;
class ProcessingThread;
typedef std::shared_ptr<ProcessingThread> ProcessingThread_SP;
//
class Filter;
class PSNRFilter;
class GraphUpdateFilter;
#include "martycam/core/FaceRecogFilter.hpp"
#include "martycam/core/MotionFilter.h"

enum class ProcessingType : int
{
  motionDetection = 0,
  faceRecognition = 1,
};

class ProcessingThread : public QObject
{
  Q_OBJECT;

  public:
  ProcessingThread(ImageBuffer buffer, hpx::execution::parallel_executor exec,
      ProcessingType processingType, MotionFilterParams mfp, FaceRecogFilterParams frfp);
  ~ProcessingThread();
  //
  void CopySettings(ProcessingThread* thread);
  void DeleteTemporaryStorage();
  //
  double getmotionEstimate() { return this->motionFilter->motionEstimate; }
  //
  void setRootFilter(Filter* filter)
  {
    this->motionFilter->renderer = filter;
    this->faceRecogFilter->setRenderer(filter);
  }
  void setThreshold(int val) { this->motionFilter->threshold = val; }
  void setAveraging(double val) { this->motionFilter->average = val; }
  void setErodeIterations(int val) { this->motionFilter->erodeIterations = val; }
  void setDilateIterations(int val) { this->motionFilter->dilateIterations = val; }
  void setDisplayImage(int image) { this->motionFilter->displayImage = image; }
  void setBlendRatios(double ratio1) { this->motionFilter->blendRatio = ratio1; }
  void setBlendRatios(double ratio1, double ratio2)
  {
    this->motionFilter->blendRatio = ratio1;
    this->motionFilter->noiseBlendRatio = ratio2;
  }
  //
  void setEyesRecogState(int val) { this->faceRecogFilter->setEyesRecogState((bool) val); }
  void setDecimationCoeff(int val) { this->faceRecogFilter->setDecimationCoeff(val); }
  int getProcessingTime() { return this->processingTime.value(); }
  void run();
  bool startProcessing();
  bool stopProcessing();

  void setMotionDetectionProcessing();
  void setFaceRecognitionProcessing();
  MotionFilter_SP motionFilter;
  FaceRecogFilter_SP faceRecogFilter;

  double getPSNR();
  cv::Scalar getMSSIM(cv::Mat const& i1, cv::Mat const& i2);

  //
  GraphUpdateFilter* graphFilter;

  signals:
  void NewData();

  private:
  //
  QMutex stopLock;
  QWaitCondition stopWait;
  bool processingActive;
  std::atomic<bool> abort;
  bool finished;
  hpx::execution::parallel_executor executor;
  //
  ImageBuffer imageBuffer;
  ProcessingType processingType;
  rolling_average processingTime;
};

#endif
