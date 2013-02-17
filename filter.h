#ifndef FILTER_H
#define FILTER_H

#include "opencv/cxcore.h"

//
// Base class for filter/processor objects
//
class Filter {
public:
  Filter();
  //
  virtual void setDelegate(Filter* list);
  virtual void process(const IplImage* image) = 0;

protected:
  Filter* delegate;
  void invokeDelegate(const IplImage* image);
};

#endif
