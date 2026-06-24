#pragma once
#include <atomic>
#include <cstdint>

#include "lx/book/order_book.hpp"
#include "lx/proto/messages.hpp"
#include "lx/queue/spsc.hpp"

namespace lx::engine
{
template <uint32_t MAX_ORDERS, uint32_t LADDER_SIZE, uint32_t QUEUE_DEPTH>
class Shard
{
 public:
  Shard(int64_t base_price, int64_t tick_size) : book_(base_price, tick_size) {}
  void tick()
  {
    proto::NewOrder msg{};
    if (!inbound_.pop(msg))
      return;

    proto::Fill fills[64];
    uint32_t    fill_count = 0;
    book_.add_order(msg, fills, fill_count);

    for (uint32_t i = 0; i < fill_count; ++i)
      outbound_.push(fills[i]);
  }

  void run()
  {
    while (running_.load(std::memory_order_relaxed))
      tick();
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

  SpscQueue<proto::NewOrder, QUEUE_DEPTH>& inbound() { return inbound_; }
  SpscQueue<proto::Fill,     QUEUE_DEPTH>& outbound() { return outbound_; }

  int64_t best_bid() const { return book_.best_bid_price(); }
  int64_t best_ask() const { return book_.best_ask_price(); }

 private:
  book::OrderBook<MAX_ORDERS, LADDER_SIZE> book_;
  SpscQueue<proto::NewOrder, QUEUE_DEPTH>  inbound_;
  SpscQueue<proto::Fill,     QUEUE_DEPTH>  outbound_;
  std::atomic<bool>                        running_{true};
};
}  // namespace lx::engine
