#include "GraphUpdateFilter.h"
//
boost::accumulators::accumulator_set<int, boost::accumulators::stats<boost::accumulators::tag::rolling_mean> > acc(boost::accumulators::tag::rolling_window::window_size = 50);
//----------------------------------------------------------------------------
GraphUpdateFilter::GraphUpdateFilter()
{
  frameNumber    = intBuffer(CIRCULAR_BUFF_SIZE);
  thresholdTime  = intBuffer(2);
  thresholdLevel = doubleBuffer(2);
  //
  movingAverage = new Plottable<int, double>( 0.0, 100.0, "Moving Average", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  motionLevel   = new Plottable<int, double>( 0.0, 100.0, "Motion",         &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  psnr          = new Plottable<int, double>( 0.0, 100.0, "PSNR",           &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  events        = new Plottable<int, int>   ( 0.0, 100.0, "EventLevel",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  threshold     = new Plottable<int, double>( 0.0, 100.0, "Threshold",      &thresholdTime, &thresholdLevel, 2);
  //
  fastDecay     = new Plottable<int, double>( 0.0, 100.0, "Fast Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  slowDecay     = new Plottable<int, double>( 0.0, 100.0, "Slow Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
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
  movingAverage = new Plottable<int, double>( 0.0, 100.0, "Moving Average", &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  motionLevel   = new Plottable<int, double>( 0.0, 100.0, "Motion",         &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  psnr          = new Plottable<int, double>( 0.0, 100.0, "PSNR",           &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  events        = new Plottable<int, int>   ( 0.0, 100.0, "EventLevel",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  threshold     = new Plottable<int, double>( 0.0, 100.0, "Threshold",      &thresholdTime, &thresholdLevel, 2);
  //
  fastDecay     = new Plottable<int, double>( 0.0, 100.0, "Fast Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
  slowDecay     = new Plottable<int, double>( 0.0, 100.0, "Slow Decay",     &frameNumber, NULL, CIRCULAR_BUFF_SIZE);
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::process(double PSNR, double motion, int framenumber, double userlevel)
{
  // scale up to 100*100 = 1E4 for log display
  double percent = 100.0*motion;
  double logval = percent>1 ? (100.0/4.0)*log10(percent) : 0;
  acc(logval);

//  this->ui.detect_value->setText(QString("%1").arg(this->processingThread->getMotionPercent(),4 , 'f', 2));
//  int eventvalue = this->eventDecision() ? 100 : 0;
  int eventvalue = 1;
  // x-axis is frame counter
  frameNumber.push_back(framenumber);

//  std::cout << " Inside UpdateStats " << counter << std::endl;
  //statusBar()->showMessage(QString("FPS : %1, Counter : %2, Buffer : %3").
  //  arg(this->captureThread->getFPS(), 5, 'f', 2).
  //  arg(captureThread->GetFrameCounter(), 5).
  //  arg(this->imageBuffer->size(), 5));

  // update y-values of graphs
  motionLevel->yContainer->push_back(logval);
  movingAverage->yContainer->push_back(boost::accumulators::rolling_mean(acc));
  psnr->yContainer->push_back(PSNR);
  events->yContainer->push_back(eventvalue);

  // The threshold line is just two points, update them each time step
  thresholdTime[0] = frameNumber.front();
  thresholdTime[1] = thresholdTime[0] + CIRCULAR_BUFF_SIZE;
  thresholdLevel[0] = userlevel;
  thresholdLevel[1] = userlevel;
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::initChart(Chart *chart)
{

  /*
    Plottable<int, double> fastDecay(QPen(QColor(Qt::cyan)),     0.0, 100.0, "Fast Decay", &frameNumber, NULL);
    Plottable<int, double> slowDecay(QPen(QColor(Qt::darkCyan)), 0.0, 100.0, "Slow Decay", &frameNumber, NULL);
  */

  Channel motion(motionLevel->minVal, motionLevel->maxVal, motionLevel->data, motionLevel->title.c_str(), QPen(QColor(Qt::green)));
  Channel mean(movingAverage->minVal, movingAverage->maxVal, movingAverage->data, movingAverage->title.c_str(), QPen(QColor(Qt::blue)));
  Channel PSNR(psnr->minVal, psnr->maxVal, psnr->data, psnr->title.c_str(), QPen(QColor(Qt::yellow)));
  Channel eventline(events->minVal, events->maxVal, events->data, events->title.c_str(), QPen(QColor(Qt::white)));
  Channel trigger(threshold->minVal, threshold->maxVal, threshold->data, threshold->title.c_str(), QPen(QColor(Qt::red)));

  motion.setShowScale(true); 
  motion.setShowLegend(true);

  mean.setShowLegend(false);
  mean.setShowScale(false);
      
  PSNR.setShowLegend(false);
  PSNR.setShowScale(false);

  trigger.setShowLegend(false);
  trigger.setShowScale(false);

  eventline.setShowLegend(false);
  eventline.setShowScale(false);

  chart->addChannel(motion);
  chart->addChannel(mean);
  chart->addChannel(PSNR);
  chart->addChannel(trigger);
  chart->addChannel(eventline);
  chart->setSize(CIRCULAR_BUFF_SIZE + GRAPH_EXTENSION);
  chart->setZoom(1.0);
}
//----------------------------------------------------------------------------
void GraphUpdateFilter::updateChart(Chart *chart)
{
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
  events->clear();
  //
  fastDecay->clear();
  slowDecay->clear();
  //
//  threshold.clear();
//  vint     thresholdTime(2);
//  vdouble  thresholdLevel(2);
}
//----------------------------------------------------------------------------
