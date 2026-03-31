#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include <deque>
#include <memory>
#include <mutex>

template <typename T>
class TSQueue
{
 public:
  TSQueue() = default;
  TSQueue(const TSQueue<T>&) = delete;
  virtual ~TSQueue() { clear(); }

  const T& front()
  {
    std::scoped_lock lck(mutex_);
    return ts_queue_.front();
  }

  const T& back()
  {
    std::scoped_lock lck(mutex_);
    return ts_queue_.back();
  }

  void push_back(const T& item)
  {
    std::scoped_lock lck(mutex_);
    ts_queue_.emplace_back(std::move(item));
  }

  void push_front(const T& item)
  {
    std::scoped_lock lck(mutex_);
    ts_queue_.emplace_front(std::move(item));
  }

  bool empty()
  {
    std::scoped_lock lck(mutex_);
    return ts_queue_.empty();
  }

  size_t count()
  {
    std::scoped_lock lck(mutex_);
    return ts_queue_.size();
  }

  void clear()
  {
    std::scoped_lock lck(mutex_);
    ts_queue_.clear();
  }

  T pop_front()
  {
    std::scoped_lock lck(mutex_);
    auto             t = std::move(ts_queue_.front());
    ts_queue_.pop_front();
    return t;
  }

  T pop_back()
  {
    std::scoped_lock lck(mutex_);
    auto             t = std::move(ts_queue_.back());
    ts_queue_.pop_back();
    return t;
  }

 protected:
  std::mutex    mutex_;
  std::deque<T> ts_queue_;
};

#endif
