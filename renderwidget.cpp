#include "renderwidget.h"
#include <QPainter>
#include <QDebug>
#include <QTime>
//----------------------------------------------------------------------------
IplImage* QImage2IplImage(QImage *qimg)
{
  IplImage *imgHeader = cvCreateImageHeader( cvSize(qimg->width(), qimg->width()), IPL_DEPTH_8U, 4);
  imgHeader->imageData = (char*) qimg->bits();

  uchar* newdata = (uchar*) malloc(sizeof(uchar) * qimg->byteCount());
  memcpy(newdata, qimg->bits(), qimg->byteCount());
  imgHeader->imageData = (char*) newdata;
  //cvClo
  return imgHeader;
}
//----------------------------------------------------------------------------
// this->movingAverage  = cvCreateImage( imageSize, IPL_DEPTH_32F, 3);
// this->thresholdImage = cvCreateImage( imageSize, IPL_DEPTH_8U, 1);
//----------------------------------------------------------------------------
template <typename T> 
QImage *IplImage2QImage(const IplImage *iplImg)
{
  int h = iplImg->height;
  int w = iplImg->width;
  int channels = iplImg->nChannels;
  QImage *qimg = new QImage(w, h, QImage::Format_ARGB32);
  const T *data = reinterpret_cast<const T *>(iplImg->imageData);
  //
  for (int y = 0; y < h; y++, data += iplImg->widthStep/sizeof(T)) {
    for (int x = 0; x < w; x++) {
      char r, g, b, a = 0;
      if (channels == 1) {
        r = data[x * channels];
        g = data[x * channels];
        b = data[x * channels];
      }
      else if (channels == 3 || channels == 4) {
        r = data[x * channels + 2];
        g = data[x * channels + 1];
        b = data[x * channels];
      }
      if (channels == 4) {
        a = data[x * channels + 3];
        qimg->setPixel(x, y, qRgba(r, g, b, a));
      }
      else {
        qimg->setPixel(x, y, qRgb(r, g, b));
      }
    }
  }
  return qimg;
}
//----------------------------------------------------------------------------
RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent), Filter() 
{
	setAttribute(Qt::WA_OpaquePaintEvent, true); // don't clear the area before the paintEvent
	setAttribute(Qt::WA_PaintOnScreen, true); // disable double buffering
	setFixedSize(640, 480);
	connect(this, SIGNAL(frameSizeChanged(int, int)), this, SLOT(onFrameSizeChanged(int, int)), Qt::QueuedConnection);
  this->bufferImage = NULL;
  this->lastImage = NULL;
  this->UpdatingImage = false;
}
//----------------------------------------------------------------------------
void RenderWidget::onFrameSizeChanged(int width, int height) {
	setFixedSize(width, height);
} 
//----------------------------------------------------------------------------
void RenderWidget::updatePixmap(const IplImage* frame) 
{
  QImage *temp = this->bufferImage;
  this->UpdatingImage = true;
  if (frame->depth == IPL_DEPTH_32F) {
    this->bufferImage = IplImage2QImage<float>(frame);
  } 
  else {
    this->bufferImage = IplImage2QImage<char>(frame);
  }
  this->UpdatingImage = false;
  delete temp;
}
//----------------------------------------------------------------------------
void RenderWidget::processPoint(const IplImage* image) {
	// copy the image to the local pixmap and update the display
	updatePixmap(image);
	update();
//	notifyListener(frame);
}
//----------------------------------------------------------------------------
void RenderWidget::paintEvent(QPaintEvent*) {
	QPainter painter(this);
	if (this->bufferImage && !this->UpdatingImage) {
		painter.drawImage(QPoint(0,0), *this->bufferImage);
	}
	else if (!this->UpdatingImage) {
		painter.setBrush(Qt::black);
		painter.drawRect(rect());
	}
}
//----------------------------------------------------------------------------
