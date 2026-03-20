#pragma once
#include <atomic>
#include <cstddef>

template <typename T, size_t N>
class MPSC_Queue
{
    static_assert((N & (N-1)) == 0, "N must be a power of 2");

    public:
        MPSC_Queue() {
            for(size_t i = 0; i < N; ++i)
                slots_[i].seq.store(i, std::memory_order_relaxed);
        }

        bool tryPush(const T& val)
        {
            size_t seq = write_seq_.load(std::memory_order_relaxed);
            Slot* slot;
            for(;;)
            {
                slot = &slots_[seq & (N-1)];
                size_t slot_seq = slot->seq.load(std::memory_order_acquire);
                intptr_t diff = (intptr_t)slot_seq - (intptr_t)seq;
                if(diff == 0) {
                    if(write_seq_.compare_exchange_weak(seq, seq+1, std::memory_order_relaxed))
                        break;
                }
                else if(diff < 0) {
                    return false;
                }
                else {
                    seq = write_seq_.load(std::memory_order_relaxed);
                }
            }

            slot->data = val;
            slot->seq.store(seq+1, std::memory_order_release);
            return true;
        }

        bool tryConsume(T& out)
        {
            Slot* slot = &slots_[read_seq_ & (N-1)];
            size_t seq = slot->seq.load(std::memory_order_acquire);
            if((intptr_t)seq - (intptr_t)(read_seq_+1) < 0)
                return false;
            out = slot->data;
            slot->seq.store(read_seq_ + N, std::memory_order_release);
            ++read_seq_;
            return true;
        }

    private:
        struct alignas(64) Slot {
            std::atomic<size_t> seq;
            T data;
        };
        
        alignas(64) Slot slots_[N];
        alignas(64) std::atomic<size_t> write_seq_{0};
        alignas(64) size_t read_seq_{0};
};