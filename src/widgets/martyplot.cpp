///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2026 John Biddiscombe
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#include "martycam/widgets/martyplot.h"

#include <QPen>
#include <QwtLegend>
#include <QwtPlotCurve>
#include <QwtPlotGrid>

//----------------------------------------------------------------------------
MartyCamPlot::MartyCamPlot(QWidget* parent)
  : QwtPlot(parent)
  , m_grid(nullptr)
{
  setAutoReplot(false);

  // Grid
  m_grid = new QwtPlotGrid();
  m_grid->setMajorPen(Qt::gray, 0, Qt::DotLine);
  m_grid->attach(this);

  // Legend
  QwtLegend* legend = new QwtLegend();
  insertLegend(legend, QwtPlot::RightLegend);

  // Axis labels
//   setAxisTitle(QwtAxis::XBottom, "Frame");
//   setAxisTitle(QwtAxis::YLeft, "Value");
  setAxisScale(QwtAxis::YLeft, -5.0, 105.0);

  setCanvasBackground(Qt::black);

  for (int i = 0; i < NumCurves; ++i) { m_curves[i] = nullptr; }
}

//----------------------------------------------------------------------------
MartyCamPlot::~MartyCamPlot()
{
  for (int i = 0; i < NumCurves; ++i) { delete m_curves[i]; }
}

//----------------------------------------------------------------------------
void MartyCamPlot::initCurves()
{
  struct CurveDef
  {
    char const* title;
    QColor color;
    bool showLegend;
  };

  static CurveDef const defs[NumCurves] = {
      {"Motion", Qt::green, true},
      {"Moving Average", Qt::blue, false},
      {"PSNR", Qt::yellow, false},
      {"NM", Qt::cyan, false},
      {"EventLevel", Qt::white, false},
      {"Threshold", Qt::red, false},
      {"Slow Decay", Qt::darkMagenta, false},
      {"Fast Decay", Qt::darkGreen, false},
  };

  for (int i = 0; i < NumCurves; ++i)
  {
    delete m_curves[i];
    m_curves[i] = new QwtPlotCurve(defs[i].title);
    m_curves[i]->setPen(QPen(defs[i].color));
    m_curves[i]->setRenderHint(QwtPlotItem::RenderAntialiased);
    m_curves[i]->setItemAttribute(QwtPlotItem::Legend, defs[i].showLegend);
    m_curves[i]->attach(this);
  }
}

//----------------------------------------------------------------------------
void MartyCamPlot::updateCurveData(int curveId, double const* xData, double const* yData, int count)
{
  if (curveId >= 0 && curveId < NumCurves && m_curves[curveId] != nullptr)
  {
    // Use setSamples (not setRawSamples) so QWT copies the data internally.
    // setRawSamples only stores the pointer, which breaks when multiple curves
    // share the same temporary buffer.
    m_curves[curveId]->setSamples(xData, yData, count);
  }
}

//----------------------------------------------------------------------------
void MartyCamPlot::updateThresholdLine(double x1, double y1, double x2, double y2)
{
  m_thresholdX[0] = x1;
  m_thresholdY[0] = y1;
  m_thresholdX[1] = x2;
  m_thresholdY[1] = y2;
  if (m_curves[Threshold] != nullptr)
  {
    m_curves[Threshold]->setRawSamples(m_thresholdX, m_thresholdY, 2);
  }
}

//----------------------------------------------------------------------------
void MartyCamPlot::clearCurves()
{
  for (int i = 0; i < NumCurves; ++i)
  {
    if (m_curves[i] != nullptr)
    {
      m_curves[i]->setRawSamples(
          static_cast<double const*>(nullptr), static_cast<double const*>(nullptr), 0);
    }
  }
  replot();
}

//----------------------------------------------------------------------------
void MartyCamPlot::setXAxisRange(double min, double max)
{
  setAxisScale(QwtAxis::XBottom, min, max);
}
