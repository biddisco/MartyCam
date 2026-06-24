#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QTime>
//
#include <iostream>
//
#include <opencv2/imgproc/imgproc.hpp>

#include "debug/logging.hpp"
#include "martycam/widgets/rendercanvas.h"
#include "martycam/widgets/renderwidget.h"

// ----------------------------------------------------------------------------
static auto render_log = martycam::log::create("Renderer");

//----------------------------------------------------------------------------
/*
cv::Mat QImage2IplImage(QImage *qimg)
{
  IplImage *imgHeader = cvCreateImageHeader( cv::Size(qimg->width(), qimg->width()), IPL_DEPTH_8U, 4);
  imgHeader->imageData = (char*) qimg->bits();

  uchar* newdata = (uchar*) malloc(sizeof(uchar) * qimg->byteCount());
  memcpy(newdata, qimg->bits(), qimg->byteCount());
  imgHeader->imageData = (char*) newdata;
  //cvClo
  return imgHeader;
}
*/
//----------------------------------------------------------------------------
// this->movingAverage  = cvCreateImage( imageSize, IPL_DEPTH_32F, 3);
// this->thresholdImage = cvCreateImage( imageSize, IPL_DEPTH_8U, 1);
//----------------------------------------------------------------------------
template <typename T>
QImage* cvMat2QImage(cv::Mat const& mat)
{
  int h = mat.size().height;
  int w = mat.size().width;
  int channels = mat.channels();
  QImage* qimg = new QImage(w, h, QImage::Format_ARGB32);
  T const* data = reinterpret_cast<T const*>(mat.data);
  //
  for (int y = 0; y < h; y++, data += mat.step[0] / sizeof(T))
  {
    for (int x = 0; x < w; x++)
    {
      char r, g, b, a = 0;
      if (channels == 1)
      {
        r = g = b = data[x];
        qimg->setPixel(x, y, qRgb(r, g, b));
      }
      else if (channels == 3 || channels == 4)
      {
        r = data[x * channels + 2];
        g = data[x * channels + 1];
        b = data[x * channels + 0];
        if (channels == 4)
        {
          a = data[x * channels + 3];
          qimg->setPixel(x, y, qRgba(r, g, b, a));
        }
        else { qimg->setPixel(x, y, qRgb(r, g, b)); }
      }
    }
  }

  return qimg;
}

//----------------------------------------------------------------------------
RenderWidget::RenderWidget(QWidget* parent)
  : QWidget(parent)
  , Filter()
  , canvas(nullptr)
  , idealSize(640, 480)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);

  // 1. Tell parent layouts that this widget wants to expand freely
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  connect(this, SIGNAL(update_signal(bool, int)), this, SLOT(UpdateTrigger(bool, int)),
      Qt::QueuedConnection);

  // Full-bleed canvas; aspect-ratio fitting, zoom and pan are handled inside paintEvent.
  canvas = new RenderCanvas(this);
  canvas->setGeometry(0, 0, width(), height());

  // Proxy double-clicks from the inner canvas to the outer widget signal.
  connect(canvas, &RenderCanvas::doubleClicked, this, &RenderWidget::mouseDblClicked);
}

//----------------------------------------------------------------------------
void RenderWidget::resizeEvent(QResizeEvent* event)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);
  Q_UNUSED(event);

  if (canvas) { canvas->setGeometry(0, 0, width(), height()); }
}

//----------------------------------------------------------------------------
void RenderWidget::setCVSize(cv::Size const& size)
{
  this->idealSize = size;
  if (canvas) { canvas->resetView(); }
  // Update the canvas geometry immediately in case no resize event fires
  resizeEvent(nullptr);
  update();
}

//----------------------------------------------------------------------------
QSize RenderWidget::sizeHint() const { return QSize(idealSize.width, idealSize.height); }

//----------------------------------------------------------------------------
void RenderWidget::updatePixmap(cv::Mat const& frame)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);

  if (frame.empty() || !canvas) { return; }

  int bytes = frame.elemSize();
  int bytes_per_channel = bytes / frame.channels();

  QImage* temp = nullptr;
  if (bytes_per_channel == 1) { temp = cvMat2QImage<unsigned char>(frame); }
  else if (bytes_per_channel == 2) { temp = cvMat2QImage<short>(frame); }
  else if (bytes_per_channel == 4) { temp = cvMat2QImage<float>(frame); }
  else if (bytes_per_channel == 8) { temp = cvMat2QImage<double>(frame); }

  if (temp) { canvas->setImage(temp); }
}

//----------------------------------------------------------------------------
void RenderWidget::process(cv::Mat const& image)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);
  // copy the image to the local pixmap and update the display
  this->updatePixmap(image);
  emit(update_signal(true, 12));
}

//----------------------------------------------------------------------------
void RenderWidget::UpdateTrigger(bool, int)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);
  // force repaint to happen on the gui thread
  canvas->update();
}

//----------------------------------------------------------------------------
void RenderWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
  MARTY_LOG_SCOPE(render_log, "{} {}", (void*) (this), __func__);
  QPoint const p = event->pos();
  emit mouseDblClicked(p);
}
