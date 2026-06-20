#include "GraphUpdateFilter.h"
#include "martyplot.h"
//
//----------------------------------------------------------------------------
GraphUpdateFilter::GraphUpdateFilter()
{
  frameNumber = doubleBuffer(CIRCULAR_BUFF_SIZE);
  thresholdTime = doubleBuffer(2);
  thresholdLevel = doubleBuffer(2);
//
#define eps -0.000
  //
  motionLevel =
      new Plottable<double, double>(eps, 54.0, "Motion", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  psnr = new Plottable<double, double>(eps, 100.0, "PSNR", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  normalized =
      new Plottable<double, double>(eps, 100.0, "NM", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);

  movingAverage = new Plottable<double, double>(
      eps, 100.0, "Moving Average", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  events =
      new Plottable<double, double>(0, 100, "EventLevel", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  threshold =
      new Plottable<double, double>(eps, 100.0, "Threshold", &thresholdTime, &thresholdLevel, 2);
  //
  slowDecay = new Plottable<double, double>(
      eps, 100.0, "Slow Decay", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  fastDecay = new Plottable<double, double>(
      eps, 100.0, "Fast Decay", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  //
  thresholdTime.push_back(0);
  thresholdTime.push_back(CIRCULAR_BUFF_SIZE);
  //
  thresholdLevel.push_back(0.0);
  thresholdLevel.push_back(0.0);
  //
}
//----------------------------------------------------------------------------
GraphUpdateFilter::~GraphUpdateFilter()
{
  delete movingAverage;
  delete motionLevel;
  delete psnr;
  delete events;
  delete threshold;
  delete normalized;
  //
  delete fastDecay;
  delete slowDecay;
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::process(double PSNR, double motion, double norm, double mean, double slow,
    double fast, int framenumber, double userlevel, double eventLevel)
{
  // x-axis is frame counter
  frameNumber.push_back(static_cast<double>(framenumber));

  // update y-values of graphs
  movingAverage->yContainer->push_back(mean);
  motionLevel->yContainer->push_back(motion);
  psnr->yContainer->push_back(PSNR);
  events->yContainer->push_back(eventLevel);
  //
  normalized->yContainer->push_back(norm);
  //
  //
  fastDecay->yContainer->push_back(fast);
  slowDecay->yContainer->push_back(slow);

  // The threshold line is just two points, update them each time step
  thresholdTime[0] = frameNumber.front();
  thresholdTime[1] = thresholdTime[0] + CIRCULAR_BUFF_SIZE;
  thresholdLevel[0] = userlevel;
  thresholdLevel[1] = userlevel;
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::initChart(MartyCamPlot* plot) { plot->initCurves(); }
//----------------------------------------------------------------------------
// Helper: copy circular_buffer contents into a contiguous std::vector
// because QwtPlotCurve::setRawSamples requires contiguous arrays.
//----------------------------------------------------------------------------
static void linearizeBuffer(boost::circular_buffer<double> const& src, std::vector<double>& dst)
{
  dst.resize(src.size());
  std::copy(src.begin(), src.end(), dst.begin());
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::updateChart(MartyCamPlot* plot)
{
  if (frameNumber.empty()) { return; }

  // Linearize x-axis (frame numbers) into contiguous memory
  linearizeBuffer(frameNumber, xContiguous);
  double const* xData = xContiguous.data();
  int count = static_cast<int>(xContiguous.size());

  // Linearize each y-channel and update its curve
  linearizeBuffer(*motionLevel->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::MotionLevel, xData, yContiguous.data(), count);

  linearizeBuffer(*movingAverage->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::MovingAverage, xData, yContiguous.data(), count);

  linearizeBuffer(*psnr->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::PSNR, xData, yContiguous.data(), count);

  linearizeBuffer(*normalized->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::Normalized, xData, yContiguous.data(), count);

  linearizeBuffer(*events->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::Events, xData, yContiguous.data(), count);

  linearizeBuffer(*slowDecay->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::SlowDecay, xData, yContiguous.data(), count);

  linearizeBuffer(*fastDecay->yContainer, yContiguous);
  plot->updateCurveData(MartyCamPlot::FastDecay, xData, yContiguous.data(), count);

  plot->updateThresholdLine(
      thresholdTime[0], thresholdLevel[0], thresholdTime[1], thresholdLevel[1]);

  plot->replot();
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::clearChart()
{
  frameNumber.clear();
  //
  movingAverage->clear();
  motionLevel->clear();
  psnr->clear();
  normalized->clear();
  events->clear();
  //
  fastDecay->clear();
  slowDecay->clear();
  //
  ///  threshold->clear();
  //  vint     thresholdTime(2);
  //  vdouble  thresholdLevel(2);
}
//----------------------------------------------------------------------------
