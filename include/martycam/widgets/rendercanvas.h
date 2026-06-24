#pragma once

#include <QImage>
#include <QPainter>
#include <QSemaphore>
#include <QWidget>
#include <opencv2/opencv.hpp>

class RenderCanvas : public QWidget
{
   Q_OBJECT

   public:
   explicit RenderCanvas(QWidget* parent = nullptr);
   ~RenderCanvas() override;

  void setImage(QImage* img);

  void resetView();

  signals:
  void doubleClicked(QPoint const& pos);

  protected:
   void paintEvent(QPaintEvent*) override;
  void wheelEvent(QWheelEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

   private:
   QRectF calculateImageRect() const;
   void zoomAt(QPointF const& widgetPos, double zoomFactor);

   QImage* bufferImage;
   QSemaphore imageValid;

   // View transform state
   double zoom;
   QPointF panOffset;
   bool isPanning;
   QPoint lastMousePos;
};
