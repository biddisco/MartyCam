#include "martycam/widgets/rendercanvas.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>

// --------------------------------------------------------------------
RenderCanvas::RenderCanvas(QWidget* parent)
  : QWidget(parent)
  , bufferImage(nullptr)
  , imageValid(1)
  , zoom(1.0)
  , panOffset(0.0, 0.0)
  , isPanning(false)
{
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::OpenHandCursor);
}

// --------------------------------------------------------------------
RenderCanvas::~RenderCanvas()
{
  delete bufferImage;
}

// --------------------------------------------------------------------
void RenderCanvas::setImage(QImage* img)
{
  imageValid.acquire();
  QImage* old = bufferImage;
  bufferImage = img;
  imageValid.release();
  delete old;
  update();
}

// --------------------------------------------------------------------
void RenderCanvas::resetView()
{
  zoom = 1.0;
  panOffset = QPointF(0.0, 0.0);
  update();
}

// --------------------------------------------------------------------
// Calculate the QRectF where the image should be drawn in widget coords.
// This accounts for aspect-ratio fitting, zoom, and pan.
// --------------------------------------------------------------------
QRectF RenderCanvas::calculateImageRect() const
{
  if (!bufferImage || bufferImage->isNull())
    return QRectF();

  double const widgetW = static_cast<double>(width());
  double const widgetH = static_cast<double>(height());
  double const imgW = static_cast<double>(bufferImage->width());
  double const imgH = static_cast<double>(bufferImage->height());

  // Aspect-ratio fit scale
  double const fitScale = qMin(widgetW / imgW, widgetH / imgH);

  // Apply user zoom
  double const finalScale = fitScale * zoom;

  double const drawW = imgW * finalScale;
  double const drawH = imgH * finalScale;

  // Centre in widget, then apply pan
  double const x = (widgetW - drawW) * 0.5 + panOffset.x();
  double const y = (widgetH - drawH) * 0.5 + panOffset.y();

  return QRectF(x, y, drawW, drawH);
}

// --------------------------------------------------------------------
// Zoom in/out keeping the pixel under `widgetPos` stable.
// ------------------------------------------------------------------
void RenderCanvas::zoomAt(QPointF const& widgetPos, double zoomFactor)
{
  QRectF const oldRect = calculateImageRect();
  if (!oldRect.isValid()) return;

  // Normalised position within the image (0..1)
  double const nx = (widgetPos.x() - oldRect.x()) / oldRect.width();
  double const ny = (widgetPos.y() - oldRect.y()) / oldRect.height();

  // Apply new zoom (clamp)
  double const newZoom = qBound(0.1, zoom * zoomFactor, 20.0);
  zoom = newZoom;

  // Recalculate with the new zoom
  QRectF const newRect = calculateImageRect();
  if (!newRect.isValid()) return;

  // Adjust pan so the same image pixel stays under cursor
  double const newX = widgetPos.x() - nx * newRect.width();
  double const newY = widgetPos.y() - ny * newRect.height();

  // Convert back to pan offset (difference from centred position)
  double const widgetW = static_cast<double>(width());
  double const widgetH = static_cast<double>(height());
  double const imgW = static_cast<double>(bufferImage->width());
  double const imgH = static_cast<double>(bufferImage->height());
  double const fitScale = qMin(widgetW / imgW, widgetH / imgH);
  double const finalScale = fitScale * zoom;

  double const centredX = (widgetW - imgW * finalScale) * 0.5;
  double const centredY = (widgetH - imgH * finalScale) * 0.5;

  panOffset = QPointF(newX - centredX, newY - centredY);
  update();
}

// --------------------------------------------------------------------
void RenderCanvas::paintEvent(QPaintEvent*)
{
  QPainter painter(this);

  // Always clear to a neutral background so panning/zooming leaves no artefacts.
  painter.fillRect(rect(), Qt::black);

  if (bufferImage)
  {
    imageValid.acquire();
    QRectF const dest = calculateImageRect();
    if (dest.isValid())
    {
      // Smooth scaling when zoomed in
      painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom > 1.0);
      painter.drawImage(dest, *bufferImage);
    }
    imageValid.release();
  }
}

// --------------------------------------------------------------------
void RenderCanvas::wheelEvent(QWheelEvent* event)
{
  if (!bufferImage) { event->ignore(); return; }

  QPoint const delta = event->angleDelta();
  if (delta.y() == 0) { event->ignore(); return; }

  double const step = 1.15;
  double const factor = (delta.y() > 0) ? step : (1.0 / step);

  zoomAt(event->position(), factor);
  event->accept();
}

// --------------------------------------------------------------------
void RenderCanvas::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
  {
    isPanning = true;
    lastMousePos = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

// --------------------------------------------------------------------
void RenderCanvas::mouseMoveEvent(QMouseEvent* event)
{
  if (isPanning && (event->buttons() & Qt::LeftButton))
  {
    QPoint const delta = event->pos() - lastMousePos;
    panOffset += QPointF(delta.x(), delta.y());
    lastMousePos = event->pos();
    update();
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

// --------------------------------------------------------------------
void RenderCanvas::mouseReleaseEvent(QMouseEvent* event)
{
  if (event->button() == Qt::LeftButton && isPanning)
  {
    isPanning = false;
    setCursor(Qt::OpenHandCursor);
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

// --------------------------------------------------------------------
void RenderCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
  emit doubleClicked(event->pos());
  event->accept();
}

// --------------------------------------------------------------------
void RenderCanvas::keyPressEvent(QKeyEvent* event)
{
  if (event->key() == Qt::Key_Escape)
  {
    resetView();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}
