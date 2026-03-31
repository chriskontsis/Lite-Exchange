#include <algorithm>
#include <cassert>
#include <vector>

// queue empty: read_pos_ == write_pos_
// queue full:  read_pos_ == (write_pos_ + 1) % qsize
template <typename T>
class SPSC_Queue
{
 public:
  SPSC_Queue(int capacity) : ring_buffer_(capacity + 1), write_pos_(0), read_pos_(0)
  {
    assert(capacity > 0);
    assert(capacity + 1 > 0);
  }

  bool tryPublish(const T& item)
  {
    const int w = write_pos_.load();
    const int next_w = increment(w);

    if (read_pos_.load(std::memory_order_acquire) == next_w)
      return false;

    ring_buffer_[w] = item;
    write_pos_.store(next_w, std::memory_order_release);
    return true;
  }

  void push(const T& val)
  {
    while (!tryPublish(val))
      ;
  }

  T pop()
  {
    T out;
    while (!tryConsume(out))
      ;
    return out;
  }

  bool tryConsume(T& out)
  {
    const int w = write_pos_.load(std::memory_order_acquire);
    const int r = read_pos_.load();
    if (r == w)
      return false;

    out = std::move(ring_buffer_[r]);
    read_pos_.store(increment(r), std::memory_order_release);
    return true;
  }

 private:
  inline int increment(int pos) const { return (pos + 1) % static_cast<int>(ring_buffer_.size()); }

  alignas(64) std::atomic<int> read_pos_;
  alignas(64) std::atomic<int> write_pos_;
  std::vector<T> ring_buffer_;
};
