#pragma once
#include <cstdint>
#include <cstring>

#include "lx/proto/codec.hpp"
#include "lx/proto/messages.hpp"

namespace lx::net
{
struct Session
{
  static constexpr uint32_t RECV_BUF_SIZE = 4096;
  int                       fd;
  uint32_t                  session_id;

  Session(int fd_, uint32_t sid) : fd(fd_), session_id(sid) {}

  void append(const std::byte* data, uint32_t len)
  {
    uint32_t space = RECV_BUF_SIZE - recv_len_;
    if (len > space)
      len = space;

    std::memcpy(recv_buf_ + recv_len_, data, len);
    recv_len_ += len;
  }

  template <typename Queue>
  uint32_t consume(Queue& q)
  {
    if (closed_)
      return 0;

    uint32_t consumed = 0;
    uint32_t count = 0;

    while (true)
    {
      proto::MsgType type{};
      uint16_t       frame_len{};
      auto result = proto::tryDecode(recv_buf_ + consumed, recv_len_ - consumed, type, frame_len);
      if (result == proto::DecodeResult::NEED_MORE)
        break;
      if (result == proto::DecodeResult::INVALID)
      {
        closed_ = true;
        break;
      }
      // Both order types normalise into one InboundMsg so they share a single
      // ordered queue — a cancel can never overtake the order it targets.
      // Backpressure: if the inbound queue is full, stop and leave this frame in
      // the recv buffer to retry on the next consume() — never silently drop a
      // decoded order (the client would get no ack and the order would vanish).
      if (type == proto::MsgType::NEW_ORDER && frame_len == sizeof(proto::NewOrder))
      {
        proto::InboundMsg msg{};
        std::memcpy(&msg.new_order, recv_buf_ + consumed, sizeof(proto::NewOrder));
        msg.hdr.session_id = session_id;  // server-authoritative: overwrite client's value
        if (!q.push(msg))
          break;
      }
      else if (type == proto::MsgType::CANCEL_ORDER && frame_len == sizeof(proto::CancelOrder))
      {
        proto::InboundMsg msg{};
        std::memcpy(&msg.cancel, recv_buf_ + consumed, sizeof(proto::CancelOrder));
        msg.hdr.session_id = session_id;
        if (!q.push(msg))
          break;
      }

      consumed += frame_len;
      ++count;
    }

    if (consumed > 0)
    {
      std::memmove(recv_buf_, recv_buf_ + consumed, recv_len_ - consumed);
      recv_len_ -= consumed;
    }

    return count;
  }

  bool closed() const { return closed_; }

 private:
  std::byte recv_buf_[RECV_BUF_SIZE]{};
  uint32_t  recv_len_ = 0;
  bool      closed_ = false;
};
}  // namespace lx::net