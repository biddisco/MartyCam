#include "DecayFilter.h"

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
// Print an alert when a cross comes. Value indicates 
// the type of the cross.
struct cross_alert
{
  template<class Sig> 
  struct result 
  {
    typedef bool type; 
  };

  cross_alert() : filter(NULL) {}; 
  void setFilter(DecayFilter *f) { this->filter = f; }

  bool operator()(const bool is_golden_cross) 
  {
    if (is_golden_cross) {
      this->filter->goldenCross = true;
      this->filter->deathCross  = false;
    }
    else {
      this->filter->goldenCross = false;
      this->filter->deathCross  = true;
    }
    return is_golden_cross;
  }
  
  DecayFilter *filter;
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
DecayFilter::DecayFilter() : 
  mavg1(1), mavg10(10), 
  ts(streamulus::NewInputStream<TimeValue>("TS", true /* verbose */)),
  slow(engine.Subscribe(streamulus::Streamify(mavg1)(ts))), 
  fast(engine.Subscribe(streamulus::Streamify(mavg10)(ts)))
{
  engine.Subscribe(streamulus::Streamify<print>(std::string("Slow Mavg = ") + streamulus::Streamify<as_string>(slow)));
  engine.Subscribe(streamulus::Streamify<print>(std::string("Fast Mavg = ") + streamulus::Streamify<as_string>(fast)));

  // we'd never work out the correct type without the auto keyword
  auto result = streamulus::Streamify<cross_alert>(streamulus::Streamify<unique<bool> >(slow < fast));
  // decode as follows ... streamify->unique->cross->filter
  result.child0.child0.setFilter(this);
  // The cross detection expression:
  engine.Subscribe(result);
}
//----------------------------------------------------------------------------
void DecayFilter::process(const TimeValue &tv)
{
  this->goldenCross = false;
  this->deathCross  = false;
  InputStreamPut(ts, tv);
}
