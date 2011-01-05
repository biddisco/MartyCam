#ifndef FILTER_H
#define FILTER_H

#include "opencv/cxcore.h"

/**
 * Base class for filters
 */
class Filter {
public:
  Filter();
  void setListener(Filter* list);
  virtual void processPoint(const IplImage* image) = 0;
protected:
  Filter* listener;
  void notifyListener(const IplImage* image);
};

#endif
