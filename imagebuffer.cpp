#include "imagebuffer.h"
#include <QDebug>
#include <QTime>
//----------------------------------------------------------------------------
ImageBuffer::ImageBuffer(int size) : bufferSize(size) 
{
}
//----------------------------------------------------------------------------
bool ImageBuffer::isFull()
{
  return (imageQueue.size() == bufferSize);
}
    int  size();
//----------------------------------------------------------------------------
int ImageBuffer::size()
{
  return imageQueue.size();
}
//----------------------------------------------------------------------------
/* Add a frame to the buffer.  
 * The data in the frame is copied to the internal image buffer.  
 * If there are no images in the buffer, the clients of the wait condition are woken to notify them of a new frame.
 * If the buffer is full, the function blocks until it there is space
 * @param image The image to add
 */
void ImageBuffer::addFrame(const cv::Mat &image) 
{
  if (image.empty()) {
    return;
  }
  // output << "Adding a frame";

  //
  // if the queue is full, wait until space is available
  //
  mutex.lock();
  if (this->isFull()) {
    bufferNotFull.wait(&mutex);
  }
  mutex.unlock();
  
  //
  // add a ref counted copy of the image to the queue
  //
  cv::Mat imagecopy = image;
  imageQueue.enqueue(imagecopy);

  //
  // wake anyone who's waiting for an image
  //
  mutex.lock();
  bufferNotEmpty.wakeAll();
  mutex.unlock();
}
//----------------------------------------------------------------------------
/* Return the oldest frame from the buffer.  
 * This is a blocking operation, so if there are no available images, 
 * this function will block until one is available or image acquisition is stopped.
 */
cv::Mat ImageBuffer::getFrame() 
{
  //
  // if the queue is empty, wait until an image is available
  //
  mutex.lock();
  if (imageQueue.isEmpty()) {
    bufferNotEmpty.wait(&mutex, 1000);
  }
  mutex.unlock();

  //
  // get the image and return it
  //
  cv::Mat frame;
  mutex.lock();
  if (!imageQueue.isEmpty()) {
    frame = imageQueue.dequeue();
  }
  bufferNotFull.wakeAll();
  mutex.unlock();
  return frame;
}
//----------------------------------------------------------------------------
/* Clear the buffer and release clients */
void ImageBuffer::clear() {
  imageQueue.clear();
  bufferNotEmpty.wakeAll();
  bufferNotFull.wakeAll();
}
