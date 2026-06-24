#ifndef GRAPHUPDATEFILTER_H
#define GRAPHUPDATEFILTER_H

// Base filter class
#include "martycam/core/PSNRFilter.h"
#include "martycam/core/filter.h"

// Boost Circular Buffer
#include <boost/circular_buffer.hpp>

//
#define CIRCULAR_BUFF_SIZE 500
#define GRAPH_EXTENSION 20
//
//----------------------------------------------------------------------------
// Forward declaration
class MartyCamPlot;
//----------------------------------------------------------------------------
template <typename T1, typename T2>
class Plottable
{
  public:
  typedef boost::circular_buffer<T1> xListType;
  typedef boost::circular_buffer<T2> yListType;

  public:
  Plottable(
      T2 Minval, T2 Maxval, char const* Title, xListType* xcont, yListType* ycont, int bufferlength)
    : minVal(Minval)
    , maxVal(Maxval)
    , title(Title)
  {
    xContainer = xcont ? xcont : new xListType(bufferlength);
    yContainer = ycont ? ycont : new yListType(bufferlength);
    deletex = (xcont == NULL);
    deletey = (ycont == NULL);
  }

  ~Plottable()
  {
    if (deletex) delete xContainer;
    if (deletey) delete yContainer;
  }

  void clear()
  {
    xContainer->clear();
    yContainer->clear();
  }
  //
  xListType* xContainer;
  yListType* yContainer;
  bool deletex, deletey;
  T2 minVal;
  T2 maxVal;
  std::string title;
};
//----------------------------------------------------------------------------
class GraphUpdateFilter
{
  public:
  ~GraphUpdateFilter();
  GraphUpdateFilter();
  //
  void process(double PSNR, double motion, double norm, double mean, double slow, double fast,
      int framenumber, double userlevel, double eventlLevel);
  //
  void initChart(MartyCamPlot* plot);
  void updateChart(MartyCamPlot* plot);
  void clearChart();
  //
  // moving average accumulator
  //

  //
  // containers for graph plot data, multiple channels
  //
  typedef boost::circular_buffer<double> doubleBuffer;
  //
  doubleBuffer frameNumber;
  doubleBuffer thresholdTime;
  doubleBuffer thresholdLevel;
  //
  Plottable<double, double>* movingAverage;
  Plottable<double, double>* motionLevel;
  Plottable<double, double>* psnr;
  Plottable<double, double>* normalized;
  Plottable<double, double>* events;
  Plottable<double, double>* threshold;
  //
  Plottable<double, double>* fastDecay;
  Plottable<double, double>* slowDecay;
  //
  // contiguous buffers for QWT (circular_buffer may wrap in memory)
  std::vector<double> xContiguous;
  std::vector<double> yContiguous;
  //
  protected:
};

#endif
