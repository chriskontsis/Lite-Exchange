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
    proto::InboundMsg msg{};
    if (!inbound_.pop(msg))
      return;

    // Dispatch on the type tag in the shared Header (common initial sequence).
    switch (msg.hdr.type)
    {
      case proto::MsgType::NEW_ORDER:
        handle_new(msg.new_order);
        break;
      case proto::MsgType::CANCEL_ORDER:
        handle_cancel(msg.cancel);
        break;
      default:
        break;  // unknown type: drop (gateway validates, but stay defensive)
    }
  }

  void run()
  {
    while (running_.load(std::memory_order_relaxed))
      tick();
  }

  void stop() { running_.store(false, std::memory_order_relaxed); }

  SpscQueue<proto::InboundMsg, QUEUE_DEPTH>&  inbound() { return inbound_; }
  SpscQueue<proto::OutboundMsg, QUEUE_DEPTH>& outbound() { return outbound_; }

  int64_t best_bid() const { return book_.best_bid_price(); }
  int64_t best_ask() const { return book_.best_ask_price(); }

 private:
  void handle_new(const proto::NewOrder& order)
  {
    proto::Fill fills[64];
    uint32_t    fill_count = 0;
    book::OrderHandle h = book_.add_order(order, fills, fill_count, 64);

    for (uint32_t i = 0; i < fill_count; ++i)
      emit_fill(fills[i]);

    // A resting order gets an Ack carrying the token the client uses to cancel;
    // a fully-filled or IOC-killed order has nothing to cancel, so no Ack.
    if (h.valid())
      emit_ack(order.order_id, h.to_token());
  }

  void handle_cancel(const proto::CancelOrder& cancel)
  {
    if (book_.cancel_by_token(cancel.order_token))
      emit_ack(0, cancel.order_token);
    else
      emit_reject(cancel.order_token);
  }

  void emit_fill(const proto::Fill& fill)
  {
    proto::OutboundMsg m{};
    m.fill = fill;
    push_out(m);
  }

  void emit_ack(uint64_t order_id, uint64_t token)
  {
    proto::OutboundMsg m{};
    m.ack.hdr = {sizeof(proto::Ack), proto::MsgType::ACK, 0};
    m.ack.order_id = order_id;
    m.ack.order_token = token;
    push_out(m);
  }

  void emit_reject(uint64_t token)
  {
    proto::OutboundMsg m{};
    m.hdr = {sizeof(proto::Reject), proto::MsgType::REJECT, 0};
    // Reject shares only the Header prefix here; token echo lives in ack path.
    (void)token;
    push_out(m);
  }

  void push_out(const proto::OutboundMsg& m)
  {
    while (!outbound_.push(m))
      ;
  }

  book::OrderBook<MAX_ORDERS, LADDER_SIZE>   book_;
  SpscQueue<proto::InboundMsg, QUEUE_DEPTH>  inbound_;
  SpscQueue<proto::OutboundMsg, QUEUE_DEPTH> outbound_;
  std::atomic<bool>                          running_{true};
};
}  // namespace lx::engine
