#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "../../src/client/FixClient.hpp"
#include "../../src/engine/EngineDispatcher.hpp"
#include "../../src/fix/FixMessageBuilder.hpp"
#include "../../src/gateway/SessionRegistry.hpp"
#include "../../src/ipc/FillEvent.hpp"
#include "../../src/ipc/MPSC_Queue.hpp"
#include "../../src/ipc/OrderEvent.hpp"
#include "../../src/net/EventLoop.hpp"
#include "../../src/net/IoHandler.hpp"
#include "../../src/server/FixServer.hpp"

class MultiClientTest : public ::testing::Test
{
 protected:
  MPSC_Queue<ipc::OrderEvent, 65536> inputQ_;
  MPSC_Queue<ipc::FillEvent, 65536>  outputQ_;
  gateway::SessionRegistry           registry_;
  fix::EngineDispatcher              dispatcher_{inputQ_, outputQ_};
  net::EventLoop                     loop_;
  fix::FixServer                     server_{loop_, 12346, inputQ_, registry_};
  std::atomic<bool>                  serverRunning_{true};
  std::thread                        server_thread_;
  std::atomic<bool>                  drainRunning_{true};
  std::thread                        drain_thread_;

  void SetUp() override
  {
    server_thread_ = std::thread(
        [this]()
        {
          net::Event events[64];
          while (serverRunning_.load(std::memory_order_relaxed))
          {
            int n = loop_.wait(events, 64, /*timeout_ms=*/1);
            for (int i = 0; i < n; ++i)
            {
              auto* handler = static_cast<net::IoHandler*>(events[i].ctx);
              if (events[i].readable)
                handler->onReadable();
              if (events[i].writable)
                handler->onWritable();
              if (handler->wantsClose())
                server_.closeSession(events[i].fd);
            }
          }
        });
    drain_thread_ = std::thread(
        [this]()
        {
          ipc::FillEvent fe;
          while (drainRunning_.load(std::memory_order_relaxed))
            outputQ_.tryConsume(fe);
        });
  }

  void TearDown() override
  {
    serverRunning_.store(false, std::memory_order_relaxed);
    drainRunning_.store(false, std::memory_order_relaxed);
    server_thread_.join();
    drain_thread_.join();
  }
};

TEST_F(MultiClientTest, SingleClientConnects)
{
  fix::FixClient client("127.0.0.1", 12346);
  client.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 10, 100, "AAPL"));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(MultiClientTest, MultipleClientsConnect)
{
  fix::FixClient c1("127.0.0.1", 12346);
  fix::FixClient c2("127.0.0.1", 12346);
  fix::FixClient c3("127.0.0.1", 12346);

  c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 10, 100, "AAPL"));
  c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(2, 10, 100, "AAPL"));
  c3.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(3, 10, 100, "MSFT"));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(MultiClientTest, ClientBurst)
{
  fix::FixClient client("127.0.0.1", 12346);
  for (int i = 0; i <= 100; ++i)
    client.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(i, 1, 100, "AAPL"));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(MultiClientTest, MultipleClientBurst)
{
  fix::FixClient c1("127.0.0.1", 12346);
  fix::FixClient c2("127.0.0.1", 12346);

  std::thread t1(
      [&]()
      {
        for (int i = 1; i <= 100; ++i)
          c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(i, 1, 100, "AAPL"));
      });
  std::thread t2(
      [&]()
      {
        for (int i = 101; i <= 200; ++i)
          c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(i, 1, 100, "AAPL"));
      });

  t1.join();
  t2.join();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
}