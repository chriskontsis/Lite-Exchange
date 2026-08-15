#include <gtest/gtest.h>

#include "lx/engine/shard.hpp"

using namespace lx;
using namespace lx::proto;

// A shard hosts a handful of instruments drawn from a much larger global id
// space — the real arrangement, where reference data assigns instruments to
// engines and ids are neither contiguous nor zero-based on any one shard.
static constexpr uint16_t BOOKS_PER_SHARD = 4;
static constexpr uint16_t SYMBOL_SPACE = 1024;

static constexpr engine::ShardConfig TEST_SHARD{.max_orders = 256,
                                                .ladder_size = 64,
                                                .queue_depth = 64,
                                                .max_symbols = BOOKS_PER_SHARD,
                                                .symbol_space = SYMBOL_SPACE};
using TestShard = engine::Shard<TEST_SHARD>;

static constexpr uint8_t SHARD_ID = 0;

static constexpr uint16_t OWNED_LOW = 3;
static constexpr uint16_t OWNED_MID = 512;
static constexpr uint16_t OWNED_HIGH = 1000;
static constexpr uint16_t UNOWNED = 4;                 // in the space, another shard's
static constexpr uint16_t ABOVE_SPACE = SYMBOL_SPACE;  // outside the space entirely

static constexpr int64_t  BASE_PRICE = 100;
static constexpr int64_t  TICK_SIZE = 1;
static constexpr int64_t  REST_PRICE = 105;
static constexpr int64_t  HIGHER_PRICE = 107;
static constexpr uint32_t QTY = 50;

static InboundMsg in_new(uint16_t symbol, uint64_t oid, int64_t price, uint32_t qty, Side side)
{
  InboundMsg m{};
  m.new_order.hdr = {sizeof(NewOrder), MsgType::NEW_ORDER, 0};
  m.new_order.symbol_id = symbol;
  m.new_order.order_id = oid;
  m.new_order.price = price;
  m.new_order.qty = qty;
  m.new_order.side = side;
  m.new_order.tif = TimeInForce::GTC;
  return m;
}

static InboundMsg in_cancel(uint64_t token)
{
  InboundMsg m{};
  m.cancel.hdr = {sizeof(CancelOrder), MsgType::CANCEL_ORDER, 0};
  m.cancel.order_token = token;
  return m;
}

static OutboundMsg pop_out(TestShard& shard)
{
  OutboundMsg m{};
  EXPECT_TRUE(shard.outbound().pop(m));
  return m;
}

static TestShard make_shard()
{
  return TestShard{BASE_PRICE, TICK_SIZE, SHARD_ID, {OWNED_LOW, OWNED_MID, OWNED_HIGH}};
}

// Sparse ids must still land in separate books.
TEST(ShardSymbolTable, OwnedSymbolsMatchIndependently)
{
  TestShard shard = make_shard();

  shard.inbound().push(in_new(OWNED_LOW, 1, REST_PRICE, QTY, Side::SELL));
  shard.tick();
  EXPECT_EQ(pop_out(shard).hdr.type, MsgType::ACK);

  // Crossing price and side, different instrument: must rest, not fill.
  shard.inbound().push(in_new(OWNED_HIGH, 2, REST_PRICE, QTY, Side::BUY));
  shard.tick();
  EXPECT_EQ(pop_out(shard).hdr.type, MsgType::ACK);

  EXPECT_EQ(shard.best_ask(OWNED_LOW), REST_PRICE);
  EXPECT_EQ(shard.best_bid(OWNED_HIGH), REST_PRICE);
  EXPECT_EQ(shard.best_bid(OWNED_MID), INT64_MIN);
}

TEST(ShardSymbolTable, BestPricesKeyedByGlobalSymbolId)
{
  TestShard shard = make_shard();

  shard.inbound().push(in_new(OWNED_MID, 1, REST_PRICE, QTY, Side::BUY));
  shard.tick();
  shard.inbound().push(in_new(OWNED_HIGH, 2, HIGHER_PRICE, QTY, Side::BUY));
  shard.tick();

  EXPECT_EQ(shard.best_bid(OWNED_MID), REST_PRICE);
  EXPECT_EQ(shard.best_bid(OWNED_HIGH), HIGHER_PRICE);
}

// An id this shard does not own belongs to a different shard: reject, never
// silently match it into some other instrument's book.
TEST(ShardSymbolTable, UnownedSymbolRejected)
{
  TestShard shard = make_shard();

  shard.inbound().push(in_new(UNOWNED, 1, REST_PRICE, QTY, Side::BUY));
  shard.tick();

  OutboundMsg m = pop_out(shard);
  EXPECT_EQ(m.hdr.type, MsgType::REJECT);
  EXPECT_EQ(m.reject.order_id, uint64_t{1});
  EXPECT_EQ(m.reject.reason, RejectReason::UNKNOWN_SYMBOL);
}

TEST(ShardSymbolTable, SymbolAboveSpaceRejected)
{
  TestShard shard = make_shard();

  shard.inbound().push(in_new(ABOVE_SPACE, 1, REST_PRICE, QTY, Side::BUY));
  shard.tick();

  OutboundMsg m = pop_out(shard);
  EXPECT_EQ(m.hdr.type, MsgType::REJECT);
  EXPECT_EQ(m.reject.reason, RejectReason::UNKNOWN_SYMBOL);
}

TEST(ShardSymbolTable, UnownedSymbolHasNoBook)
{
  TestShard shard = make_shard();

  EXPECT_EQ(shard.best_bid(UNOWNED), INT64_MIN);
  EXPECT_EQ(shard.best_ask(UNOWNED), INT64_MAX);
}

// The token carries no symbol; the shard resolves the book off the live order.
TEST(ShardSymbolTable, CancelResolvesSparseOwningBook)
{
  TestShard shard = make_shard();

  shard.inbound().push(in_new(OWNED_HIGH, 1, REST_PRICE, QTY, Side::BUY));
  shard.tick();
  OutboundMsg ack = pop_out(shard);
  ASSERT_EQ(ack.hdr.type, MsgType::ACK);

  shard.inbound().push(in_cancel(ack.ack.order_token));
  shard.tick();

  EXPECT_EQ(pop_out(shard).hdr.type, MsgType::ACK);
  EXPECT_EQ(shard.best_bid(OWNED_HIGH), INT64_MIN);
}

// The point of the table: ladders scale with the instruments a shard hosts, not
// with the id space it can be addressed by.
TEST(ShardSymbolTable, BookCountTracksOwnedSymbolsNotSymbolSpace)
{
  static constexpr engine::ShardConfig NARROW{.max_orders = 256,
                                              .ladder_size = 64,
                                              .queue_depth = 64,
                                              .max_symbols = 2,
                                              .symbol_space = SYMBOL_SPACE};
  static constexpr engine::ShardConfig WIDE{.max_orders = 256,
                                            .ladder_size = 64,
                                            .queue_depth = 64,
                                            .max_symbols = 8,
                                            .symbol_space = SYMBOL_SPACE};

  constexpr std::size_t narrow = sizeof(engine::Shard<NARROW>);
  constexpr std::size_t wide = sizeof(engine::Shard<WIDE>);
  static_assert(wide > narrow, "more hosted instruments must cost more books");

  constexpr std::size_t book_cost = (wide - narrow) / (WIDE.max_symbols - NARROW.max_symbols);
  static_assert(narrow < book_cost * 16,
                "a 1024-id space must not cost anywhere near 1024 ladders");
}
