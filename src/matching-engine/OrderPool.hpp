#pragma once
#include <cstdint>
#include "OrderStructures.hpp"

template <size_t N>
concept PowerOfTwo = (N > 0) && ((N & (N-1)) == 0);

namespace LOB {

    template <size_t Cap> requires PowerOfTwo<Cap>
    class OrderPool 
    {
        public:
            OrderPool() 
            {
                for(uint32_t i = 0; i < Cap; ++i)
                    free_[i] = i;
            }

            Order* allocate() 
            {
                if(top_ == 0) return nullptr;
                return reinterpret_cast<Order*>(storage_ + free_[--top_] * sizeof(Order));
            }

            void deallocate(Order* p)
            {
                uint32_t idx = static_cast<uint32_t>(
                        (reinterpret_cast<char*>(p) - storage_) / sizeof(Order));
                free_[top_++] = idx;
            }

        private:
            alignas(Order) char storage_[Cap * sizeof(Order)];
            uint32_t free_[Cap];
            uint32_t top_ { Cap };
    };

} // namespace LOB