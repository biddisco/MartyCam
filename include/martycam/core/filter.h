#ifndef FILTER_H
#define FILTER_H

#include "opencv2/core/core.hpp"
#include "opencv2/core/core_c.h"

//
// Base class for filter/processor objects
//
class Filter
{
  public:
  Filter();
  //
  virtual void setDelegate(Filter* list);
  virtual void process(cv::Mat const& image) = 0;

  protected:
  Filter* delegate;
  void invokeDelegate(cv::Mat const& image);
};

#endif
