#ifndef MESSAGE_QUEUE_HPP
#define MESSAGE_QUEUE_HPP

#include <deque>
#include <memory>
#include <mutex>

template<typename T>
class TSQueue
{
public:
    TSQueue() = default;
    TsQueue(const TsQueue<T>&) = delete;
    virtual ~TSQueue() {clear();}

    const T& front()
    {
        std::scoped_lock lck(mutex_);
        return queue.front();
    }

    const T& back()
    {
        std::scoped_lock lck(mutex_);
        return queue.back();
    }

    void push_back(const T& item)
    {
        std::scoped_lock lck(mutex_);
        queue.emplace_back(std::move(item));
    }

    void push_front(const T& item)
    {
        std::scoped_lock lck(mutex_);
        queue.emplace_back(std::move(item));
    }

    bool empty()
    {
        std::scoped_lock lck(mutex_);
        return queue.empty();
    }

    size_t count()
    {
        std::scoped_lock lck(mutex_);
        return queue.size();
    }

    void clear()
    {
        std::scoped_lock lck(mutex_);
        queue.clear();
    }

    T pop_front()
    {
        std::scoped_lock lck(mutex_);
        auto t = std::move(queue.front());
        queue.pop_front();
        return t;
    }

    T pop_back()
    {
        std::scoped_lock lck(mutex_);
        auto t = std::move(queue.back());
        queue.pop_back();
        return t;
    }


protected:
    std::mutex mutex_;
    std::deque<T> queue;
};

#endif