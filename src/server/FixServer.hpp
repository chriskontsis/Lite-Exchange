#pragma once

#include <memory>
#include <unordered_map>

#include "../fix/FixSession.hpp"
#include "../gateway/SessionRegistry.hpp"
#include "../gateway/SymbolRegistry.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "ServerBase.hpp"
#include "net/EventLoop.hpp"
namespace fix
{
class FixServer : public ServerBase<FixServer>
{
 public:
  FixServer(net::EventLoop& loop, short port, MPSC_Queue<ipc::OrderEvent, 65536>& inputq,
            gateway::SessionRegistry& registry, gateway::SymbolRegistry& symbols)
      : ServerBase(loop, port), input_q_(inputq), registry_(registry), symbols_(symbols)
  {
  }

  void onNewConnection(int fd)
  {
    auto session = std::make_shared<FixSession>(fd, loop_, input_q_, registry_, symbols_);
    sessions_[fd] = session;
    session->start();
    loop_.add(fd, session.get(), net::Watch::Read);
  }

  void closeSession(int fd)
  {
    auto it = sessions_.find(fd);
    if (it == sessions_.end())
      return;
    registry_.removeSession((it->second->sessionId()));
    loop_.remove(fd);
    sessions_.erase(it);
  }

 private:
  MPSC_Queue<ipc::OrderEvent, 65536>&                  input_q_;
  gateway::SessionRegistry&                            registry_;
  gateway::SymbolRegistry&                             symbols_;
  std::unordered_map<int, std::shared_ptr<FixSession>> sessions_;
};
}  // namespace fix