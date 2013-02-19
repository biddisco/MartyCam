#ifndef RENDER_WIDGET_H
#define RENDER_WIDGET_H

#include <QWidget>
#include <QSemaphore>
//
#include <opencv2/core/core.hpp>
//
#include "filter.h"

class RenderWidget : public QWidget, public Filter {
Q_OBJECT;
public:
  RenderWidget(QWidget* parent);
  void process(const cv::Mat &image);

  virtual void mouseDoubleClickEvent ( QMouseEvent * event );

public slots:
  void onFrameSizeChanged(int width, int height);
  void UpdateTrigger(bool, int);

signals:
  void frameSizeChanged(int width, int height);
  void update_signal(bool, int);
  void mouseDblClicked(const QPoint&);

protected:
  void paintEvent(QPaintEvent*);
  void updatePixmap(const cv::Mat &frame);

private:
  QImage         *bufferImage;
  QSemaphore      imageValid;
};

#endif
