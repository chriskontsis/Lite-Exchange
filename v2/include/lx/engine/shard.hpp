#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <utility>

#include "lx/book/order_book.hpp"
#include "lx/proto/messages.hpp"
#include "lx/queue/spsc.hpp"

namespace lx::engine
{
// Compile-time capacities for a Shard, passed as a single class-type NTTP so
// call sites name each knob instead of listing bare numbers positionally.
struct ShardConfig
{
  uint32_t max_orders;   // shard-wide order arena (one huge-page pool)
  uint32_t ladder_size;  // price levels per symbol book
  uint32_t queue_depth;  // inbound/outbound SPSC ring slots
  uint16_t max_symbols = 1;
};

// One matching shard: owns max_symbols books over a single shared order arena,
// so a pool slot is unique shard-wide and a cancel token needs no symbol field.
template <ShardConfig CFG>
class Shard
{
 public:
  static constexpr uint32_t MAX_FILLS_PER_ORDER = 64;

  Shard(int64_t base_price, int64_t tick_size, uint8_t shard_id = 0)
      : books_(make_books(base_price, tick_size, std::make_index_sequence<CFG.max_symbols>{})),
        shard_id_(shard_id)
  {
  }

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

  SpscQueue<proto::InboundMsg, CFG.queue_depth>& inbound() { return inbound_; }
  SpscQueue<proto::OutboundMsg, CFG.queue_depth>& outbound() { return outbound_; }

  int64_t best_bid(uint16_t symbol = 0) const { return books_[symbol].best_bid_price(); }
  int64_t best_ask(uint16_t symbol = 0) const { return books_[symbol].best_ask_price(); }

 private:
  using Book = book::OrderBook<CFG.max_orders, CFG.ladder_size>;

  // Every book borrows pool_/order_session_, so both must already be built —
  // they are declared ahead of books_ below.
  template <std::size_t... I>
  std::array<Book, CFG.max_symbols> make_books(int64_t base_price, int64_t tick_size,
                                               std::index_sequence<I...>)
  {
    return {((void)I, Book{base_price, tick_size, pool_, order_session_})...};
  }

  void handle_new(const proto::NewOrder& order, uint32_t session_id)
  {
    if (order.symbol_id >= CFG.max_symbols)
    {
      emit_reject(order.order_id, proto::RejectReason::UNKNOWN_SYMBOL, session_id);
      return;
    }

    proto::Fill       fills[MAX_FILLS_PER_ORDER];
    uint32_t          fill_count = 0;
    book::OrderHandle h =
        books_[order.symbol_id].add_order(order, fills, fill_count, MAX_FILLS_PER_ORDER);

    for (uint32_t i = 0; i < fill_count; ++i)
      emit_fill(fills[i], session_id);

    if (h.valid())  // resting order: Ack carries the cancel token
      emit_ack(order.order_id, h.to_token(shard_id_), session_id);
  }

  void handle_cancel(const proto::CancelOrder& cancel, uint32_t session_id)
  {
    book::OrderHandle h = book::OrderHandle::from_token(cancel.order_token);

    // Validate slot+gen BEFORE reading the slot's symbol: a stale token may
    // point at a recycled order now belonging to a different book.
    if (h.slot >= CFG.max_orders || pool_.gen(h.slot) != h.gen)
    {
      emit_reject(0, proto::RejectReason::UNKNOWN_ORDER, session_id);
      return;
    }

    uint16_t symbol = pool_[h.slot].symbol;
    if (symbol >= CFG.max_symbols || !books_[symbol].cancel_order(h))
    {
      emit_reject(0, proto::RejectReason::UNKNOWN_ORDER, session_id);
      return;
    }
    emit_ack(0, cancel.order_token, session_id);
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

  void emit_reject(uint64_t order_id, proto::RejectReason reason, uint32_t session_id)
  {
    proto::OutboundMsg m{};
    m.reject.hdr = {sizeof(proto::Reject), proto::MsgType::REJECT, 0};
    m.reject.hdr.session_id = session_id;
    m.reject.order_id = order_id;
    m.reject.reason = reason;
    push_out(m);
  }

  void push_out(const proto::OutboundMsg& m)
  {
    while (!outbound_.push(m))
      ;
  }

  book::Pool<book::Order, CFG.max_orders>        pool_;
  uint32_t                                       order_session_[CFG.max_orders]{};
  std::array<Book, CFG.max_symbols>              books_;
  SpscQueue<proto::InboundMsg, CFG.queue_depth>  inbound_;
  SpscQueue<proto::OutboundMsg, CFG.queue_depth> outbound_;
  std::atomic<bool>                              running_{true};
  uint8_t                                        shard_id_;
};
}  // namespace lx::engine
