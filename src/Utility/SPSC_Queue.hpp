#include <algorithm>
#include <memory>
#include <vector>

// queue empty: readPos == writePos
// queue full: readPos == (writePos + 1) % qsize;
template<typename T>
class SPSC_Queue
{
public:
    SPSC_Queue(int capacity) : ringBuffer(capacity + 1), writePos(0), readPos(0)
    {
        assert(capacity > 0);
        assert(capacity + 1 > 0);
    }

    // Queue full : readPos == increment(writePos);
    bool tryPublish(const T& item)
    {
        const int w = writePos.load();
        const int next_w = increment(w);

        if(readPos.load(std::memory_order_acquire) == next_w) 
            return false;
        
        ringBuffer[w] = item;
        writePos.store(next_w, std::memory_order_release);
            
        return true;
    }

    void push(const T& val)
    {
        while(!tryPublish(val));
    }

    // Blocks until an item is available, then returns it.
    T pop()
    {
        T out;
        while(!tryConsume(out));
        return out;
    }

    // Queue Empty : readPos == writePos

    bool tryConsume(T& out)
    {
        const int w = writePos.load(std::memory_order_acquire);
        const int r = readPos.load();
        if(r == w)
            return false;

        out = std::move(ringBuffer[r]);
        readPos.store(increment(r), std::memory_order_release);
        return true;
    }

private:
    inline int increment(int pos) const
    {
        return (pos + 1) % static_cast<int>(ringBuffer.size());
    }

    alignas(64) std::atomic<int> readPos;
    alignas(64) std::atomic<int> writePos;
    std::vector<T> ringBuffer;
};