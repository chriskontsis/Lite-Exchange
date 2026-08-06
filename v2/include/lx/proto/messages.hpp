#pragma once
#include <cstdint>
#include <type_traits>

namespace lx::proto
{
// ---------- Wire Types ----------
enum class MsgType : uint8_t
{
  LOGON        = 0x01,
  NEW_ORDER    = 0x02,
  CANCEL_ORDER = 0x03,
  ACK          = 0x81,
  REJECT       = 0x82,
  FILL         = 0x83,
};

enum class Side : uint8_t
{
  BUY  = 0,
  SELL = 1
};

enum class TimeInForce : uint8_t
{
  GTC = 0,
  IOC = 1
};

enum class RejectReason : uint8_t
{
  UNKNOWN_SYMBOL = 0,
  INVALID_PRICE  = 1,
  INVALID_QTY    = 2
};

// Fields are ordered so every multi-byte field sits at its natural alignment
// with no compiler padding — verified by static_assert below.
// No #pragma pack needed: the layout is designed to be self-aligned.

// ----- Header (4 bytes, every frame starts with this) -----
struct Header
{
  uint16_t len;
  MsgType  type;
  uint8_t  flags;
};

// ------ Inbound Messages --------------

struct Logon
{
  Header   hdr;        // offset 0, size 4
  uint32_t session_id; // offset 4, size 4
  uint64_t padding;    // offset 8, size 8
};

struct NewOrder
{
  Header      hdr;       // offset 0,  size 4
  uint16_t    symbol_id; // offset 4,  size 2
  Side        side;      // offset 6,  size 1
  TimeInForce tif;       // offset 7,  size 1
  uint64_t    order_id;  // offset 8,  size 8  (8-byte aligned ✓)
  int64_t     price;     // offset 16, size 8
  uint32_t    qty;       // offset 24, size 4
  uint32_t    padding;   // offset 28, size 4
};

struct CancelOrder
{
  Header   hdr;         // offset 0, size 4
  uint32_t padding;     // offset 4, size 4
  uint64_t order_token; // offset 8, size 8  (8-byte aligned ✓) — exchange-assigned
};

// ----- Outbound Messages --------------

struct Ack
{
  Header   hdr;         // offset 0,  size 4
  uint32_t padding;     // offset 4,  size 4
  uint64_t order_id;    // offset 8,  size 8  (8-byte aligned ✓) — client's order_id
  uint64_t order_token; // offset 16, size 8  — exchange-assigned handle for cancels
};

struct Reject
{
  Header       hdr;        // offset 0, size 4
  RejectReason reason;     // offset 4, size 1
  uint8_t      padding[3]; // offset 5, size 3
  uint64_t     order_id;   // offset 8, size 8  (8-byte aligned ✓)
};

struct Fill
{
  Header   hdr;          // offset 0,  size 4
  uint32_t qty;          // offset 4,  size 4
  uint64_t aggressor_id; // offset 8,  size 8  (8-byte aligned ✓)
  uint64_t resting_id;   // offset 16, size 8
  int64_t  price;        // offset 24, size 8
};

// ----- Tagged unions for the engine queues -----
// Every message begins with Header at offset 0, so `hdr.type` is readable
// through any union member (common-initial-sequence rule for standard-layout
// structs). Cancels share the inbound queue with new orders so a cancel can
// never overtake the order it targets — FIFO ordering is a correctness need.

union InboundMsg
{
  Header      hdr;
  NewOrder    new_order;
  CancelOrder cancel;
};

union OutboundMsg
{
  Header hdr;
  Ack    ack;
  Fill   fill;
};

// ---- Compile-time layout verification ----
static_assert(sizeof(Header)      == 4);
static_assert(sizeof(Logon)       == 16);
static_assert(sizeof(NewOrder)    == 32);
static_assert(sizeof(CancelOrder) == 16);
static_assert(sizeof(Ack)         == 24);
static_assert(sizeof(Reject)      == 16);
static_assert(sizeof(Fill)        == 32);

static_assert(std::is_trivially_copyable_v<NewOrder>);
static_assert(std::is_trivially_copyable_v<Fill>);

}  // namespace lx::proto
