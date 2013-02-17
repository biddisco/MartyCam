#include "filter.h"

//----------------------------------------------------------------------------
Filter::Filter() : delegate(0) 
{

}
//----------------------------------------------------------------------------
void Filter::setDelegate(Filter *filter) 
{
  this->delegate = filter;
}
//----------------------------------------------------------------------------
void Filter::invokeDelegate(const IplImage* image) {
  if (this->delegate) {
    this->delegate->process(image);
  }
}
//----------------------------------------------------------------------------
