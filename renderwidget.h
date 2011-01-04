#ifndef RENDER_WIDGET_H
#define RENDER_WIDGET_H

#include <QWidget>
#include "filter.h"

class HeadState;

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
private:
	void updatePixmap(const IplImage* frame);
  QImage *bufferImage;
  QImage *lastImage;
};

#endif
