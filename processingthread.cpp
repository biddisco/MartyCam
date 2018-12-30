#include <QDebug>
#include <QTime>
//
#include <iostream>
//
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
//
#include "processingthread.h"
//
#include "filter.h"
#include "PSNRFilter.h"
#include "GraphUpdateFilter.h"
//
#include <hpx/lcos/future.hpp>
#include <hpx/include/async.hpp>
#include <utility> //----------------------------------------------------------------------------
ProcessingThread::ProcessingThread(ImageBuffer buffer,
                                   hpx::threads::executors::pool_executor exec,
                                   ProcessingType processingType,
                                   MotionFilterParams mfp,
                                   FaceRecogFilterParams frfp)
        : imageBuffer(std::move(buffer)),
          executor(std::move(exec)),
          processingActive(false),
          motionFilter(new MotionFilter(mfp)),
          faceRecogFilter(new FaceRecogFilter(frfp)),
          abort(false),
          processingType(processingType),
          processingTimes(15),
          processingTime_ms(0),
          QObject(nullptr)
{
  this->graphFilter       = new GraphUpdateFilter();
}
//----------------------------------------------------------------------------
ProcessingThread::~ProcessingThread()
{
  delete this->graphFilter;
}
//----------------------------------------------------------------------------
void ProcessingThread::CopySettings(ProcessingThread *thread)
{
  this->motionFilter->triggerLevel = thread->motionFilter->triggerLevel;
  this->motionFilter->threshold = thread->motionFilter->threshold;
  this->motionFilter->average =  thread->motionFilter->average;
  this->motionFilter->erodeIterations =  thread->motionFilter->erodeIterations;
  this->motionFilter->dilateIterations =  thread->motionFilter->dilateIterations;
  this->motionFilter->displayImage =  thread->motionFilter->displayImage;
  this->motionFilter->blendRatio =  thread->motionFilter->blendRatio;
  this->motionFilter->noiseBlendRatio =  thread->motionFilter->noiseBlendRatio;
}
//----------------------------------------------------------------------------
void ProcessingThread::setMotionDetectionProcessing(){
  this->processingType = ProcessingType::motionDetection;
}
void ProcessingThread::setFaceRecognitionProcessing(){
  this->processingType = ProcessingType::faceRecognition;
}
//----------------------------------------------------------------------------
void ProcessingThread::run() {
  int framenum = 0;
  QTime processingTime;
  processingTime.start();

  while (!this->abort) {
    // blocking : waits until next image is available if necessary
    cv::Mat cameraImage = imageBuffer->receive();
    // if camera not working or disconnected, abort
    if (cameraImage.empty()) {
      boost::this_thread::sleep(boost::posix_time::milliseconds(100));
      continue;
    }
    cv::Mat cameracopy = cameraImage.clone();

    processingTime.restart();
    switch(this->processingType){
        case ProcessingType::motionDetection :
          this->motionFilter->process(cameracopy);

    this->graphFilter->process(
      this->motionFilter->PSNR_Filter->PSNR,
      this->motionFilter->logMotion,
      this->motionFilter->normalizedMotion,
      this->motionFilter->rollingMean,
      this->motionFilter->decayFilter->mavg10.getLastResult(),
      this->motionFilter->decayFilter->mavg1.getLastResult(),
      framenum++,
      this->motionFilter->triggerLevel,
      this->motionFilter->eventLevel);

          break;
        case ProcessingType ::faceRecognition :
          this->faceRecogFilter->process(cameracopy);
          break;
    }
    updateProcessingTime(processingTime.elapsed());
    emit (NewData());
  }

  this->abort = false;
  this->stopWait.wakeAll();
}
//----------------------------------------------------------------------------
bool ProcessingThread::startProcessing()
{
  if (!processingActive) {
    processingActive = true;
    abort = false;

    hpx::async(this->executor, &ProcessingThread::run, this);

    return true;
  }
  return false;
}
//----------------------------------------------------------------------------
bool ProcessingThread::stopProcessing() {
  bool wasActive = this->processingActive;
  if (wasActive) {
    this->stopLock.lock();
    processingActive = false;
    abort = true;
    this->stopWait.wait(&this->stopLock);
    this->stopLock.unlock();
  }
  return wasActive;
}
//----------------------------------------------------------------------------
void ProcessingThread::updateProcessingTime(int time_ms) {
  processingTimes.push_back(time_ms);

  processingTime_ms = static_cast<int>(std::accumulate(processingTimes.begin(),
          processingTimes.end(), 0) / processingTimes.size());
}

//----------------------------------------------------------------------------
cv::Scalar ProcessingThread::getMSSIM(const cv::Mat& i1, const cv::Mat& i2)
{
  const double C1 = 6.5025, C2 = 58.5225;
  /***************************** INITS **********************************/
  int d = CV_32F;

  cv::Mat I1, I2;
  i1.convertTo(I1, d);           // cannot calculate on one byte large values
  i2.convertTo(I2, d);

  cv::Mat I2_2   = I2.mul(I2);        // I2^2
  cv::Mat I1_2   = I1.mul(I1);        // I1^2
  cv::Mat I1_I2  = I1.mul(I2);        // I1 * I2

  /***********************PRELIMINARY COMPUTING ******************************/

  cv::Mat mu1, mu2;   //
  GaussianBlur(I1, mu1, cv::Size(11, 11), 1.5);
  GaussianBlur(I2, mu2, cv::Size(11, 11), 1.5);

  cv::Mat mu1_2   =   mu1.mul(mu1);
  cv::Mat mu2_2   =   mu2.mul(mu2);
  cv::Mat mu1_mu2 =   mu1.mul(mu2);

  cv::Mat sigma1_2, sigma2_2, sigma12;

  GaussianBlur(I1_2, sigma1_2, cv::Size(11, 11), 1.5);
  sigma1_2 -= mu1_2;

  GaussianBlur(I2_2, sigma2_2, cv::Size(11, 11), 1.5);
  sigma2_2 -= mu2_2;

  GaussianBlur(I1_I2, sigma12, cv::Size(11, 11), 1.5);
  sigma12 -= mu1_mu2;

  ///////////////////////////////// FORMULA ////////////////////////////////
  cv::Mat t1, t2, t3;

  t1 = 2 * mu1_mu2 + C1;
  t2 = 2 * sigma12 + C2;
  t3 = t1.mul(t2);              // t3 = ((2*mu1_mu2 + C1).*(2*sigma12 + C2))

  t1 = mu1_2 + mu2_2 + C1;
  t2 = sigma1_2 + sigma2_2 + C2;
  t1 = t1.mul(t2);               // t1 =((mu1_2 + mu2_2 + C1).*(sigma1_2 + sigma2_2 + C2))

  cv::Mat ssim_map;
  divide(t3, t1, ssim_map);      // ssim_map =  t3./t1;

  cv::Scalar mssim = cv::mean( ssim_map ); // mssim = average of ssim map
  return mssim;
}
