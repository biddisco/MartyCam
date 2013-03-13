#include "DecayFilter.h"
#include <functional>

//----------------------------------------------------------------------------
// Remove consecutive repetitions from a stream. 
template<typename T>
class unique
{
public:

  unique() : mFirst(true) { }

  template<class Sig> 
  struct result 
  {
    typedef T type; 
  };

  boost::optional<T> operator()(const T& value) 
  {
    if (mFirst || (value != mPrev))
    {
      mFirst = false;
      return mPrev = value;
    }
    return boost::none;
  }

private:
  bool mFirst; 
  T mPrev;
};

//----------------------------------------------------------------------------
// Functor that prints whatever it gets. 
// Stremify<print> is a stream function that prints every
// element of a stream.
struct print 
{    
  template<class Sig> struct result;

  template<class This,typename T>
  struct result<This(T)>
  {
    typedef T type; 
  };

  template<typename T>
  typename result<print(T)>::type
    operator()(const T& value) const
  { 
    std::cout << value << std::endl;
    return value;
  }
};

//----------------------------------------------------------------------------
// Return whatever you got as a string. This is useful 
// for printing sub-expression results within a string
// (converting to string allows + with other strings).
struct as_string 
{    
  template<class Sig> struct result;

  template<class This,typename T>
  struct result<This(T)>
  {
    typedef std::string type; 
  };

  template<typename T>
  typename result<print(T)>::type
    operator()(const T& value) const
  { 
    std::stringstream ss;
    ss << value;
    return ss.str();
  }
};

//----------------------------------------------------------------------------
// Functor that lets us add a "this" pointer to the stream 
struct thisptr 
{    
  template<class Sig> struct result;

  template<class This,typename T>
  struct result<This(T)>
  {
    typedef T type; 
  };

  template<typename T>
  typename result<thisptr(T)>::type
    operator()(const T* ptr) const
  { 
    return ptr;
  }
};

//----------------------------------------------------------------------------
bool cross_alert::operator()(const bool is_golden_cross) 
{
  if (is_golden_cross) {
    this->filter->goldenCross = true;
    this->filter->deathCross  = false;
    std::cout << "Golden cross " << std::endl;
  }
  else {
    this->filter->goldenCross = false;
    this->filter->deathCross  = true;
    std::cout << "Death cross " << std::endl;
  }
  return is_golden_cross;
}
//----------------------------------------------------------------------------
double Mavg::operator()(const TimeValue& tick) 
{
  if (!mFirst)
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

//----------------------------------------------------------------------------
DecayFilter::DecayFilter() : 
  mavg1(0.01), mavg10(2), alert(this),
  ts(streamulus::NewInputStream<TimeValue>("TS", true )),
  slow(engine.Subscribe(streamulus::Streamify(streamulus_wrapper<Mavg>(mavg1))(ts))), 
  fast(engine.Subscribe(streamulus::Streamify(streamulus_wrapper<Mavg>(mavg10))(ts)))
{
  engine.Subscribe(streamulus::Streamify<print>(std::string("Slow Mavg = ") + streamulus::Streamify<as_string>(slow)));
  engine.Subscribe(streamulus::Streamify<print>(std::string("Fast Mavg = ") + streamulus::Streamify<as_string>(fast)));

//  streamulus::Streamify<unique<bool> > tttt(slow < fast);

//  engine.Subscribe(
//    streamulus::Streamify<cross_alert>(( streamulus::Streamify<unique<bool> >(slow < fast) ))
//  );

  // we'd never work out the correct type without the auto keyword
//  cross_alert crossing(this);
//  streamulus::Subscription<bool>::type cross_type
 //   (
 //   );

//  auto result = streamulus::Streamify(crossing()();

//  auto result = streamulus::Streamify(alert)(slow < fast);
  // + (streamulus::Streamify<unique<bool> >(slow < fast) );
  // decode as follows ... streamify->unique->cross->filter
//  result.child0.child0.setFilter(this);
  // The cross detection expression:
  auto result2 = streamulus::Streamify(alert)(streamulus::Streamify<unique<bool> >(slow < fast) );
  engine.Subscribe(result2);
}
//----------------------------------------------------------------------------
void DecayFilter::process(const TimeValue &tv)
{
  this->goldenCross = false;
  this->deathCross  = false;
  InputStreamPut(ts, tv);
}
