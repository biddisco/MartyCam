#ifndef FPS_HELPER_H
#define FPS_HELPER_H

#include <boost/circular_buffer.hpp>
#include <chrono>
#include <numeric>

// ---------------------------------------------------------------------------
// Helper that maintains a circular buffer of timestamps (in milliseconds) and
// computes an FPS estimate from the span covered by the stored samples.
// Owns an internal std::chrono timer so callers don't need to manage one.
// ---------------------------------------------------------------------------
class fps_helper
{
  public:
  explicit fps_helper(std::size_t capacity = 50)
    : times_(capacity)
    , current_value_(0.0)
  {
  }

  // Reset the timer and clear the buffer. Call once before the measurement loop.
  void start()
  {
    start_time_ = std::chrono::steady_clock::now();
    clear();
  }

  // Record the elapsed time since start(), push it into the buffer and
  // recompute the current FPS value. The timer is NOT restarted.
  void tick()
  {
    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count());
    update(elapsed_ms);
  }

  void clear()
  {
    times_.clear();
    current_value_ = 0.0;
  }

  double value() const { return current_value_; }
  std::size_t size() { return times_.size(); }

  // Compute what the FPS would be if we called tick() right now, without
  // actually pushing the sample into the buffer. Accounts for buffer capacity.
  double projected_value() const
  {
    if (times_.empty()) return 0.0;

    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count());

    if (elapsed_ms <= times_.front()) return 0.0;

    std::size_t projected_count = times_.size() + 1;
    int projected_front = times_.front();

    // If the buffer is at capacity, the oldest sample would be dropped.
    if (times_.full())
    {
      projected_count = times_.capacity();
      projected_front = times_[1];
      if (elapsed_ms <= projected_front) return 0.0;
    }

    if (projected_count <= 1) return 0.0;

    double projected =
        (projected_count - 1) / (static_cast<double>(elapsed_ms) - projected_front) * 1000.0;
    projected = (static_cast<int>(projected * 10)) / 10.0;
    return projected;
  }

  // Returns true if calling tick() now would produce a value strictly greater
  // than target_fps. Useful for adaptive frame-rate throttling.
  bool would_exceed(double target_fps) const { return projected_value() > target_fps; }

  private:
  void update(int time_ms)
  {
    times_.push_back(time_ms);
    if (times_.size() > 1)
    {
      current_value_ =
          (times_.size() - 1) / (static_cast<double>(time_ms) - times_.front()) * 1000.0;
      current_value_ = (static_cast<int>(current_value_ * 10)) / 10.0;
    }
    else { current_value_ = 0.0; }
  }

  boost::circular_buffer<int> times_;
  double current_value_;
  std::chrono::steady_clock::time_point start_time_;
};

// ---------------------------------------------------------------------------
// Helper that maintains a circular buffer of integer samples and computes a
// rolling average. Owns an internal std::chrono timer so callers don't need
// to manage one.
// ---------------------------------------------------------------------------
class rolling_average
{
  public:
  explicit rolling_average(std::size_t capacity = 50)
    : values_(capacity)
    , current_value_(0)
  {
  }

  // Start (or restart) the internal timer.
  void start() { start_time_ = std::chrono::steady_clock::now(); }

  // Alias for start() - makes intent explicit when measuring intervals.
  void restart() { start_time_ = std::chrono::steady_clock::now(); }

  // Push elapsed time since start/restart into the buffer, recompute the average.
  void tick()
  {
    auto now = std::chrono::steady_clock::now();
    int elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count());
    update(elapsed_ms);
  }

  // Manually push a value into the buffer.
  void update(int value)
  {
    values_.push_back(value);
    current_value_ =
        static_cast<int>(std::accumulate(values_.begin(), values_.end(), 0) / values_.size());
  }

  void clear()
  {
    values_.clear();
    current_value_ = 0;
  }

  int value() const { return current_value_; }

  private:
  boost::circular_buffer<int> values_;
  int current_value_;
  std::chrono::steady_clock::time_point start_time_;
};

#endif
