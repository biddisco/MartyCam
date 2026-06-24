///////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2026 John Biddiscombe
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <QFrame>
#include <QwtPlot>

class QwtPlotCurve;
class QwtPlotGrid;

//----------------------------------------------------------------------------
// MartyCamPlot - A QwtPlot subclass for displaying live signal data from
// the various motion/PSNR/decay filters.
//----------------------------------------------------------------------------
class MartyCamPlot : public QwtPlot
{
  Q_OBJECT

  public:
  // One curve per signal channel
  enum CurveId
  {
    MotionLevel = 0,
    MovingAverage,
    PSNR,
    Normalized,
    Events,
    Threshold,
    SlowDecay,
    FastDecay,
    NumCurves
  };

  explicit MartyCamPlot(QWidget* parent = nullptr);
  ~MartyCamPlot();

  void initCurves();
  void updateCurveData(int curveId, double const* xData, double const* yData, int count);
  void updateThresholdLine(double x1, double y1, double x2, double y2);
  void clearCurves();
  void setXAxisRange(double min, double max);

  protected:
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

  private:
  void createHoverLegend();

  QwtPlotCurve* m_curves[NumCurves];
  QwtPlotGrid* m_grid;

  // Hover legend overlay
  QFrame* m_legendOverlay;

  // Local buffers for threshold line (only 2 points)
  double m_thresholdX[2];
  double m_thresholdY[2];
};
