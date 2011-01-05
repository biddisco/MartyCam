#ifndef RENDER_WIDGET_H
#define RENDER_WIDGET_H

#include <QWidget>
#include <QSemaphore>
#include "filter.h"

class RenderWidget : public QWidget, public Filter {
Q_OBJECT;
public:
  RenderWidget(QWidget* parent);
  void processPoint(const IplImage* image);

public slots:
  void onFrameSizeChanged(int width, int height);
signals:
  void frameSizeChanged(int width, int height);

protected:
  void paintEvent(QPaintEvent*);
  void updatePixmap(const IplImage* frame);

private:
  QImage         *bufferImage;
  QSemaphore      imageValid;
};

#endif
