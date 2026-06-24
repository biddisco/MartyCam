#ifndef THREAD_SAFE_CIRCULAR_BUFFER_H
#define THREAD_SAFE_CIRCULAR_BUFFER_H

#include <boost/circular_buffer.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

// Thread safe circular buffer
template <typename T>
class ConcurrentCircularBuffer : private boost::noncopyable
{
  public:
  typedef boost::mutex::scoped_lock lock;
  ConcurrentCircularBuffer()
    : shutdown_requested(false)
  {
  }
  ConcurrentCircularBuffer(int n)
    : shutdown_requested(false)
  {
    cb.set_capacity(n);
  }

  void send(T imdata)
  {
    lock lk(monitor);
    cb.push_back(imdata);
    buffer_not_empty.notify_one();
  }

  T receive()
  {
    lock lk(monitor);
    while (cb.empty() && !shutdown_requested) { buffer_not_empty.wait(lk); }
    if (cb.empty()) { return T(); }
    T imdata = cb.front();
    cb.pop_front();
    return imdata;
  }

  // Return the newest frame and drop older queued frames to avoid lag.
  T receive_latest()
  {
    lock lk(monitor);
    while (cb.empty() && !shutdown_requested) { buffer_not_empty.wait(lk); }
    if (cb.empty()) { return T(); }
    T imdata = cb.back();
    cb.clear();
    return imdata;
  }

  void clear()
  {
    lock lk(monitor);
    cb.clear();
  }

  int size()
  {
    lock lk(monitor);
    return cb.size();
  }

  void set_capacity(int capacity)
  {
    lock lk(monitor);
    cb.set_capacity(capacity);
  }

  // Request shutdown and wake all threads waiting on the buffer.
  void shutdown()
  {
    lock lk(monitor);
    shutdown_requested = true;
    buffer_not_empty.notify_all();
  }

  // Reset the shutdown flag so the buffer can be reused.
  void reset()
  {
    lock lk(monitor);
    shutdown_requested = false;
    cb.clear();
  }

  private:
  boost::condition buffer_not_empty;
  boost::mutex monitor;
  boost::circular_buffer<T> cb;
  bool shutdown_requested;
};

#endif
