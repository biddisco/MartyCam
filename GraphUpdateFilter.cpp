#include "GraphUpdateFilter.h"
//
//----------------------------------------------------------------------------
GraphUpdateFilter::GraphUpdateFilter()
{
  frameNumber    = intBuffer(CIRCULAR_BUFF_SIZE);
  thresholdTime  = intBuffer(2);
  thresholdLevel = doubleBuffer(2);
  //
  #define eps -0.000
  //
  motionLevel   = new Plottable<int, double>( eps,  54.0, "Motion",         &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  psnr          = new Plottable<int, double>( eps, 100.0, "PSNR",           &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  normalized    = new Plottable<int, double>( eps, 100.0, "NM",             &frameNumber, NULL, CIRCULAR_BUFF_SIZE);

  movingAverage = new Plottable<int, double>( eps, 100.0, "Moving Average", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  events        = new Plottable<int, int>   (   0, 100,   "EventLevel",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  threshold     = new Plottable<int, double>( eps, 100.0, "Threshold",      &thresholdTime, &thresholdLevel, 2);
  //
  slowDecay     = new Plottable<int, double>( eps, 100.0, "Slow Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  fastDecay     = new Plottable<int, double>( eps, 100.0, "Fast Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
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
void GraphUpdateFilter::process(double PSNR, double motion, double norm, double mean, double slow, double fast, int framenumber, double userlevel, double eventLevel)
{
  // x-axis is frame counter
  frameNumber.push_back(framenumber);

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
void GraphUpdateFilter::initChart(Chart *chart)
{
  Channel motion(motionLevel->minVal, motionLevel->maxVal, motionLevel->data, motionLevel->title.c_str(), QPen(QColor(Qt::green)));
  Channel mean(movingAverage->minVal, movingAverage->maxVal, movingAverage->data, movingAverage->title.c_str(), QPen(QColor(Qt::blue)));
  Channel PSNR(psnr->minVal, psnr->maxVal, psnr->data, psnr->title.c_str(), QPen(QColor(Qt::yellow)));
  Channel norm(normalized->minVal, normalized->maxVal, normalized->data, normalized->title.c_str(), QPen(QColor(Qt::cyan)));

  Channel eventline(events->minVal, events->maxVal, events->data, events->title.c_str(), QPen(QColor(Qt::white)));
  Channel trigger(threshold->minVal, threshold->maxVal, threshold->data, threshold->title.c_str(), QPen(QColor(Qt::red)));

  Channel slow(slowDecay->minVal, slowDecay->maxVal, slowDecay->data, slowDecay->title.c_str(), QPen(QColor(Qt::darkMagenta)));
  Channel fast(fastDecay->minVal, fastDecay->maxVal, fastDecay->data, fastDecay->title.c_str(), QPen(QColor(Qt::darkGreen)));

  motion.setShowScale(true); 
  motion.setShowLegend(true);

  mean.setShowLegend(false);
  mean.setShowScale(false);
      
  PSNR.setShowLegend(false);
  PSNR.setShowScale(false);

  norm.setShowScale(false); 
  norm.setShowLegend(false);

  trigger.setShowLegend(false);
  trigger.setShowScale(false);

  eventline.setShowLegend(false);
  eventline.setShowScale(false);

  slow.setShowLegend(false);
  slow.setShowScale(false);

  fast.setShowLegend(false);
  fast.setShowScale(false);

  chart->addChannel(motion);
  chart->addChannel(mean);
  chart->addChannel(slow);
  chart->addChannel(fast);

  chart->addChannel(PSNR);
  chart->addChannel(norm);
  chart->addChannel(trigger);
  chart->addChannel(eventline);

  chart->setSize(CIRCULAR_BUFF_SIZE + GRAPH_EXTENSION);
  chart->setZoom(1.0);
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::updateChart(Chart *chart)
{
  if (frameNumber.empty()) {
    return;
  }
  chart->setUpdatesEnabled(0);
  chart->setPosition(frameNumber.front());
  chart->setSize(CIRCULAR_BUFF_SIZE + GRAPH_EXTENSION);
  chart->setZoom(1.0);
  chart->setUpdatesEnabled(1);
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
