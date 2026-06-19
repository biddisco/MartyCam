#pragma once

#include <QImage>
#include <QPainter>
#include <QSemaphore>
#include <QWidget>
#include <opencv2/opencv.hpp>

// 1. Define the canvas that actually draws the image
class RenderCanvas : public QWidget
{
  Q_OBJECT
  public:
  RenderCanvas(QWidget* parent = nullptr)
    : QWidget(parent)
    , bufferImage(nullptr)
    , imageValid(1)
  {
    setAttribute(Qt::WA_OpaquePaintEvent, true);    // don't clear the area before the paintEvent
  }

  void setImage(QImage* img)
  {
    imageValid.acquire();
    bufferImage = img;
    imageValid.release();
    update();    // Triggers a repaint
  }

  protected:
  void paintEvent(QPaintEvent*) override
  {
    QPainter painter(this);    // Paints directly on itself

    if (bufferImage)
    {
      imageValid.acquire();
      // Draw the image scaled to fill the canvas safely
      painter.drawImage(rect(), *bufferImage);
      imageValid.release();
    }
    else { painter.fillRect(rect(), Qt::lightGray); }
  }

  private:
  QImage* bufferImage;
  QSemaphore imageValid;
};
