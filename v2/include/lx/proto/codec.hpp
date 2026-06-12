#pragma once
#include <cstddef>
#include <cstdint>

#include "lx/proto/messages.hpp"

namespace lx::proto
{
inline constexpr uint16_t MIN_FRAME_LEN = sizeof(Header);
inline constexpr uint16_t MAX_FRAME_LEN = 256;

enum class DecodeResult : uint8_t
{
  OK,
  NEED_MORE,
  INVALID
};

// Inspec avail bytes at buf, on OK, sets 'type' and 'frame_len'
// caller advances the buffer by frame_len and reinterpret_cast
inline DecodeResult tryDecode(const std::byte* buf, std::size_t avail, MsgType& type,
                              uint16_t& frame_len)
{
  if (avail < MIN_FRAME_LEN)
    return DecodeResult::NEED_MORE;

  const auto* hdr = reinterpret_cast<const Header*>(buf);
  if (hdr->len < MIN_FRAME_LEN || hdr->len > MAX_FRAME_LEN)
    return DecodeResult::INVALID;

  switch (hdr->type)
  {
    case MsgType::LOGON:
    case MsgType::NEW_ORDER:
    case MsgType::CANCEL_ORDER:
    case MsgType::ACK:
    case MsgType::REJECT:
    case MsgType::FILL:
      break;
    default:
      return DecodeResult::INVALID;
  }

  if (avail < hdr->len)
    return DecodeResult::NEED_MORE;

  type = hdr->type;
  frame_len = hdr->len;
  return DecodeResult::OK;
}
}  // namespace lx::proto