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

    uint32_t session_id = msg.hdr.session_id;  // stamped by the gateway
    switch (msg.hdr.type)
    {
      case proto::MsgType::NEW_ORDER:
        handle_new(msg.new_order, session_id);
        break;
      case proto::MsgType::CANCEL_ORDER:
        handle_cancel(msg.cancel, session_id);
        break;
      default:
        break;  // unknown type: drop
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
  void handle_new(const proto::NewOrder& order, uint32_t session_id)
  {
    proto::Fill fills[64];
    uint32_t    fill_count = 0;
    book::OrderHandle h = book_.add_order(order, fills, fill_count, 64);

    for (uint32_t i = 0; i < fill_count; ++i)
      emit_fill(fills[i], session_id);

    if (h.valid())  // resting order: Ack carries the cancel token
      emit_ack(order.order_id, h.to_token(), session_id);
  }

  void handle_cancel(const proto::CancelOrder& cancel, uint32_t session_id)
  {
    if (book_.cancel_by_token(cancel.order_token))
      emit_ack(0, cancel.order_token, session_id);
    else
      emit_reject(cancel.order_token, session_id);
  }

  void emit_fill(const proto::Fill& fill, uint32_t aggressor_session)
  {
    // The book addressed the fill to the passive owner; send it verbatim.
    proto::OutboundMsg passive{};
    passive.fill = fill;
    push_out(passive);

    // Aggressor's copy, unless it's the same session (self-trade: one report).
    if (aggressor_session != fill.hdr.session_id)
    {
      proto::OutboundMsg agg{};
      agg.fill = fill;
      agg.hdr.session_id = aggressor_session;
      push_out(agg);
    }
  }

  void emit_ack(uint64_t order_id, uint64_t token, uint32_t session_id)
  {
    proto::OutboundMsg m{};
    m.ack.hdr = {sizeof(proto::Ack), proto::MsgType::ACK, 0};
    m.ack.hdr.session_id = session_id;
    m.ack.order_id = order_id;
    m.ack.order_token = token;
    push_out(m);
  }

  void emit_reject(uint64_t token, uint32_t session_id)
  {
    proto::OutboundMsg m{};
    m.hdr = {sizeof(proto::Reject), proto::MsgType::REJECT, 0};
    m.hdr.session_id = session_id;
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
