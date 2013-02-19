#ifndef GRAPHUPDATEFILTER_H
#define GRAPHUPDATEFILTER_H

// Base filter class
#include "filter.h"
#include "PSNRFilter.h"
// Boost Accumulators
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/weighted_median.hpp>
// Boost Circular Buffer
#include <boost/circular_buffer.hpp>
// Chart plotting declarations
#include "chart.h"
#include "chart/datacontainers.h"
//
#define  CIRCULAR_BUFF_SIZE 500
#define  GRAPH_EXTENSION    50
//
//----------------------------------------------------------------------------
//using namespace boost::accumulators;
//----------------------------------------------------------------------------
template<typename T1, typename T2>
class Plottable {  
  public:
    typedef boost::circular_buffer<T1> xListType;
    typedef boost::circular_buffer<T2> yListType;
    typedef DoubleDataContainer<xListType, yListType> channelType;

  public:
    Plottable(T2 Minval, T2 Maxval, const char *Title, xListType *xcont, yListType *ycont, int bufferlength) : minVal(Minval), maxVal(Maxval), title(Title)
    {
      xContainer = xcont ? xcont : new xListType(bufferlength);
      yContainer = ycont ? ycont : new yListType(bufferlength);
      deletex = (xcont==NULL);
      deletey = (ycont==NULL);
      data = new channelType(*xContainer, *yContainer);
    }

    ~Plottable()
    {
      if (deletex) delete xContainer;
      if (deletey) delete yContainer;
      delete data;
    }

    void clear() {
      xContainer->clear();
      yContainer->clear();
    }
    //
    xListType   *xContainer;
    yListType   *yContainer;
    bool         deletex, deletey;
    channelType *data;
    T2           minVal;
    T2           maxVal;
    std::string  title;

};
//----------------------------------------------------------------------------
class GraphUpdateFilter {
  public:
    ~GraphUpdateFilter();
     GraphUpdateFilter();
    //
    void process(double PSNR, double motion, int framenumber, double userlevel);
    //
    void initChart(Chart *chart);
    void updateChart(Chart *chart);
    void clearChart();
    //
    // moving average accumulator
    //

    //
    // containers for graph plot data, multiple channels
    //
    typedef boost::circular_buffer<int>    intBuffer;
    typedef boost::circular_buffer<double> doubleBuffer;
    //
    intBuffer     frameNumber;
    intBuffer     thresholdTime;
    doubleBuffer  thresholdLevel;
    //
    Plottable<int, double> *movingAverage;
    Plottable<int, double> *motionLevel;
    Plottable<int, double> *psnr;
    Plottable<int, int>    *events;
    Plottable<int, double> *threshold;
    //
    Plottable<int, double> *fastDecay;
    Plottable<int, double> *slowDecay;
    //
  protected:
};

#endif
