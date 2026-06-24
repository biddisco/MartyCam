#ifndef FACERECOGFILTER_H
#define FACERECOGFILTER_H

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/opencv.hpp>
//
#include "martycam/core/filter.h"
//
#include <memory>

class FaceRecogFilter;
typedef std::shared_ptr<FaceRecogFilter> FaceRecogFilter_SP;

struct FaceRecogFilterParams
{
  bool detectEyes;
  double decimationCoeff;
};
//
// Filter which performs face recognition
//
class FaceRecogFilter
{
  public:
  FaceRecogFilter();
  FaceRecogFilter(FaceRecogFilterParams frfp);
  ~FaceRecogFilter();
  //
  virtual void process(cv::Mat const& image);
  //
  void setDecimationCoeff(int val);
  void setEyesRecogState(bool state) { this->eyesRecogState = state; }
  void DeleteTemporaryStorage();
  void countPixels(cv::Mat const& image);

  void setRenderer(Filter* renderer) { this->renderer = renderer; }

  protected:
  Filter* renderer;
  //
  // input variables
  //
  cv::CascadeClassifier cascade;
  cv::CascadeClassifier nestedCascade;
  double scale;
  cv::Size imageSize;
  int frameCount;
  bool eyesRecogState;
  cv::Mat inputImage;
  cv::Mat outputImage;
  cv::Size text_size;
};

#endif
