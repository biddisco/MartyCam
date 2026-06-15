#ifndef RENDER_WIDGET_H
#define RENDER_WIDGET_H

#include <QSemaphore>
#include <QWidget>
//
#include <opencv2/core/core.hpp>
//
#include <memory>
#include "filter.h"

class RenderWidget;
typedef std::shared_ptr<RenderWidget> RenderWidget_SP;

class RenderWidget
  : public QWidget
  , public Filter
{
  Q_OBJECT;

  public:
  RenderWidget(QWidget* parent);
  void process(cv::Mat const& image) override; 

  virtual void mouseDoubleClickEvent(QMouseEvent* event) override;

  // provide a convenience function for cv::Size
  void setCVSize(cv::Size const& size);
  void resizeEvent(QResizeEvent* event) override;

  public slots:
  // void onFrameSizeChanged(int width, int height);
  void UpdateTrigger(bool, int);

  signals:
  // void frameSizeChanged(int width, int height);
  void update_signal(bool, int);
  void mouseDblClicked(QPoint const&);

  protected:
  void updatePixmap(cv::Mat const& frame);

  private:
  class RenderCanvas* canvas;
  cv::Size idealSize;
};

#endif
