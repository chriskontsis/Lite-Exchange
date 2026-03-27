#pragma once

namespace net
{
struct IoHandler
{
  virtual void onReadable() = 0;
  virtual void onWritable() {}
  virtual bool wantsClose() const { return false; }
  virtual ~IoHandler() = default;
};
}  // namespace net