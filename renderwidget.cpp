#include "renderwidget.h"
#include <QPainter>
#include <QDebug>
#include <QTime>
#include <iostream>
//
#include <opencv2/imgproc/imgproc.hpp>
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
RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent), Filter(), imageValid(1) 
{
  setAttribute(Qt::WA_OpaquePaintEvent, true); // don't clear the area before the paintEvent
  setAttribute(Qt::WA_PaintOnScreen, true); // disable double buffering
  setFixedSize(640, 480);
  connect(this, SIGNAL(frameSizeChanged(int, int)), this, SLOT(onFrameSizeChanged(int, int)));
  connect(this, SIGNAL(update_signal(bool, int)), this, SLOT(UpdateTrigger(bool, int)), Qt::QueuedConnection);
  //
  this->bufferImage = NULL;
}
//----------------------------------------------------------------------------
void RenderWidget::onFrameSizeChanged(int width, int height) {
  setFixedSize(width, height);
} 
//----------------------------------------------------------------------------
void RenderWidget::updatePixmap(const IplImage* frame) 
{
  QImage *temp = this->bufferImage;
  imageValid.acquire();
  if (frame->depth == IPL_DEPTH_32F) {
    this->bufferImage = IplImage2QImage<float>(frame);
  } 
  else {
    this->bufferImage = IplImage2QImage<char>(frame);
  }
  imageValid.release();
  delete temp;
}
//----------------------------------------------------------------------------
void RenderWidget::process(const IplImage* image) {
  // copy the image to the local pixmap and update the display
  this->updatePixmap(image);
  emit(update_signal(true, 12));
}
//----------------------------------------------------------------------------
void RenderWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  if (this->bufferImage) {
    imageValid.acquire();
    painter.drawImage(QPoint(0,0), *this->bufferImage);
    imageValid.release();
  }
  else {
    painter.setBrush(Qt::black);
    painter.drawRect(rect());
  }
}
//----------------------------------------------------------------------------
void RenderWidget::UpdateTrigger(bool, int) {
  this->repaint();
//  std::cout << "Rendered frame " << std::endl;
}