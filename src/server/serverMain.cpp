#include <atomic>
#include <iostream>
#include <thread>

#include "../engine/EngineDispatcher.hpp"
#include "../fix/FixMessageBuilder.hpp"
#include "../gateway/SessionRegistry.hpp"
#include "../ipc/FillEvent.hpp"
#include "../ipc/MPSC_Queue.hpp"
#include "../ipc/OrderEvent.hpp"
#include "../net/EventLoop.hpp"
#include "../net/IoHandler.hpp"
#include "FixServer.hpp"

int main()
{
  MPSC_Queue<ipc::OrderEvent, 65536> inputQ;
  MPSC_Queue<ipc::FillEvent, 65536>  outputQ;
  gateway::SessionRegistry           registry;

  fix::EngineDispatcher dispatcher(inputQ, outputQ);

  net::EventLoop loop;
  fix::FixServer server(loop, 12345, inputQ, registry);

  std::atomic<bool> running{true};
  std::thread       drainThread(
      [&]()
      {
        ipc::FillEvent fe;
        while (running.load(std::memory_order_relaxed))
        {
          if (outputQ.tryConsume(fe))
          {
            if (auto* session = registry.lookup(fe.session_id_))
              session->sendData(fix::FixMessageBuilder::executionReport(fe));
          }
        }
      });

  std::cout << "Exchange listening on port 12345\n";
  net::Event events[64];

  while (running.load(std::memory_order_relaxed))
  {
    int n = loop.wait(events, 64, /* timeout ms */ 1);
    for (int i = 0; i < n; ++i)
    {
      auto* handler = static_cast<net::IoHandler*>(events[i].ctx);
      if (events[i].readable)
        handler->onReadable();
      if (events[i].writable)
        handler->onWritable();
      if (handler->wantsClose())
        server.closeSession(events[i].fd);
    }
  }

  running.store(false, std::memory_order_relaxed);
  drainThread.join();
  return 0;
}