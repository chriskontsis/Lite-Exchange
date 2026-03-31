#pragma once

namespace net
{
struct IoHandler
{
  virtual void onReadable() = 0;
  virtual void onWritable() {}
  virtual bool wantsClose() const { return false; }
  virtual int fd() const { return -1; }
  virtual ~IoHandler() = default;
};
}  // namespace net