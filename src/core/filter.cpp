#include "martycam/core/filter.h"

//----------------------------------------------------------------------------
Filter::Filter()
  : delegate(0)
{
}

//----------------------------------------------------------------------------
void Filter::setDelegate(Filter* filter) { this->delegate = filter; }

//----------------------------------------------------------------------------
void Filter::invokeDelegate(cv::Mat const& image)
{
  if (this->delegate) { this->delegate->process(image); }
}

//----------------------------------------------------------------------------
