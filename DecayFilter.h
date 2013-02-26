#ifndef DECAYFILTER_H
#define DECAYFILTER_H
//
#include "streamulus.h"
//
#include <iostream>
//
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
class Mavg
{
public:

  Mavg(int decay_factor) : mFirst(true), mDecayFactor(decay_factor), mMavg(0)
  {
  }

  template<class Sig> 
  struct result 
  {
    typedef double type; 
  };

  double operator()(const TimeValue& tick) 
  {
    if (! mFirst)
    {
      double alpha = 1-1/exp(mDecayFactor*(tick.time-mPrevTime));
      mMavg += alpha*(tick.value - mMavg);
      mPrevTime = tick.time;
    }
    else
    {
      mMavg = tick.value;
      mPrevTime = tick.time;
      mFirst = false;
    }
    return mMavg;
  }

private:
  clock_t mPrevTime;    
  bool mFirst;
  int mDecayFactor;
  double mMavg;  
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

  streamulus::InputStream<TimeValue>::type ts;
  streamulus::Streamulus                   engine;
  streamulus::Subscription<double>::type   slow;
  streamulus::Subscription<double>::type   fast;

  // The moving averages are streamified from the function objects.
  Mavg mavg1;
  Mavg mavg10;

  bool deathCross;
  bool goldenCross;

protected:
};

#endif
