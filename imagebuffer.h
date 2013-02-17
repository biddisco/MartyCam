#ifndef IMAGE_BUFFER_H
#define IMAGE_BUFFER_H

#include <QWaitCondition>
#include <QMutex>
#include <QQueue>
//
#include <opencv2/core/core.hpp>

/*
 * The image buffer buffers images between the capture thread and the processing thread and provides synchronization between the two threads.
 */

class ImageBuffer {
  public:
    ImageBuffer(int size);
    //
    void    addFrame(const cv::Mat &image);
    cv::Mat getFrame();
    //
    void clear();
    bool isFull();

  private:
    QWaitCondition bufferNotEmpty;
    QWaitCondition bufferNotFull;
    QMutex         mutex;
    int           bufferSize;
    /** Queue of IplImage objects to store the data. **/
    QQueue<cv::Mat> imageQueue;
};

#endif
