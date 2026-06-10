#pragma once
#include <cstdint>
#include <type_traits>

namespace lx::proto
{
// ---------- Wire Types ----------
enum class MsgType : uint8_t
{
  LOGON = 0x01,
  NEW_ORDER = 0x02,
  CANCEL_ORDER = 0x03,
  ACK = 0x81,
  REJECT = 0x82,
  FILL = 0x83,
};

enum class Side : uint8_t
{
  BUY = 0,
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
  INVALID_PRICE = 1,
  INVALID_QTY = 2
};
// -------------------------------

// ----- Header (4 bytes, every frame statrs w/) -----
#pragma pack(push, 1)
struct Header
{
  uint16_t len;
  MsgType  type;
  uint8_t  flags;
};

// ------ Inbound Messages --------------
struct Logon
{
  Header   hdr;
  uint32_t session_id;
  uint64_t padding;
};

struct NewOrder
{
  Header      hdr;
  uint64_t    order_id;
  int64_t     price;
  uint32_t    qty;
  uint16_t    symbol_id;
  Side        side;
  TimeInForce tif;
  uint32_t    padding;
};

struct CancelOrder
{
  Header   hdr;
  uint64_t order_id;
  uint32_t padding;
};

// ----- Outbound messages -------
struct Ack
{
  Header   hdr;
  uint64_t order_id;
  uint32_t padding;
};

struct Reject
{
  Header       hdr;
  uint64_t     order_id;
  RejectReason reason;
  uint8_t      padding[3];
};

struct Fill
{
  Header   hdr;
  uint64_t aggressor_id;
  uint64_t resting_id;
  int64_t  price;
  uint32_t qty;
};
#pragma pack(pop)

// Compile time assert
static_assert(sizeof(Header) == 4);
static_assert(sizeof(Logon) == 16);
static_assert(sizeof(NewOrder) == 32);
static_assert(sizeof(CancelOrder) == 16);
static_assert(sizeof(Ack) == 16);
static_assert(sizeof(Reject) == 16);
static_assert(sizeof(Fill) == 32);

static_assert(std::is_trivially_copyable_v<NewOrder>);
static_assert(std::is_trivially_copyable_v<Fill>);

}  // namespace lx::proto