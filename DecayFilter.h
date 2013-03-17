#ifndef DECAYFILTER_H
#define DECAYFILTER_H
//
#include "streamulus.h"
#include <boost/ref.hpp>
//
#include <iostream>
//
class DecayFilter;

//----------------------------------------------------------------------------
// time-value pair
struct TimeValue
{
  TimeValue() : time(0) , value(0) { }

  TimeValue(clock_t t, double v) : time(t) , value(v) { }

  friend std::ostream& operator<<(std::ostream& os, const TimeValue& tv)
  {
    return os << "{" << tv.time << "," << tv.value << "}";
  }

  clock_t time;
  double value;
};

//----------------------------------------------------------------------------
// Exponentially decaying moving average
class Mavg : public boost::noncopyable
{
public:

  template <class Sig> struct result { typedef double type; };

  Mavg(double decay_factor) : mFirst(true), mDecayFactor(decay_factor), mMavg(0)
  {
    std::cout << "Mavg int " << std::endl;
  }

  double operator()(const TimeValue& tick); 

  inline double getLastResult() { return this->mMavg; }

private:
  clock_t mPrevTime;    
  bool    mFirst;
  double  mDecayFactor;
  double  mMavg;  
};

//----------------------------------------------------------------------------
// Print an alert when a cross comes. Value indicates 
// the type of the cross.
struct cross_alert
{
  template<class Sig> 
  struct result 
  {
    typedef bool type; 
  };

  cross_alert() : filter(NULL) {
    std::cout << "Null constructor " << std::endl;
  }; 
  cross_alert(DecayFilter *f) : filter(f) {
    std::cout << "Filter constructor, f=" << f << std::endl;
  }; 
  cross_alert(const cross_alert &that) : filter(that.filter) {
    std::cout << "Copy constructor, that.f=" << that.filter << std::endl;
  }; 
  
  void setFilter(DecayFilter *f) { this->filter = f; }

  bool operator()(const bool is_golden_cross);

  DecayFilter *filter;
};


//----------------------------------------------------------------------------
//
// class to hold our streamulus data
//
class DecayFilter {
public:
  DecayFilter();
  //
  virtual void process(const TimeValue &tv);
  //
  // The moving averages are streamified from the function objects.
  // thes must be declared before the subscription types to ensure correct intitialization order 
  
  cross_alert alert;
  //
  Mavg mavg1;
  Mavg mavg10;

  streamulus::InputStream<TimeValue>::type    ts;
  streamulus::Streamulus                      engine;
  streamulus::Subscription<double>::type      slow;
  streamulus::Subscription<double>::type      fast;

  bool deathCross;
  bool goldenCross;

protected:
};

#endif
