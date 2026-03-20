#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <boost/asio.hpp>
#include "../../src/server/FixServer.hpp"
#include "../../src/engine/EngineDispatcher.hpp"
#include "../../src/client/FixClient.hpp"
#include "../../src/fix/FixMessageBuilder.hpp"
#include "../../src/ipc/MPSC_Queue.hpp"
#include "../../src/ipc/OrderEvent.hpp"
#include "../../src/ipc/FillEvent.hpp"
#include "../../src/gateway/SessionRegistry.hpp"

class MultiClientTest : public ::testing::Test
{
protected:
    MPSC_Queue<ipc::OrderEvent, 4096> inputQ_;
    MPSC_Queue<ipc::FillEvent, 4096> outputQ_;
    gateway::SessionRegistry registry_;
    fix::EngineDispatcher dispatcher_{inputQ_, outputQ_};
    boost::asio::io_context io_context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_{boost::asio::make_work_guard(io_context_)};
    fix::FixServer server_{io_context_, 12346, inputQ_, registry_};
    std::thread server_thread_;
    std::atomic<bool> drainRunning_{true};
    std::thread drain_thread_;

    void SetUp() override
    {
        server_thread_ = std::thread([this]()
                                     { io_context_.run(); });
        drain_thread_ = std::thread([this]()
                                    {
              ipc::FillEvent fe;
              while (drainRunning_.load(std::memory_order_relaxed))
                  outputQ_.tryConsume(fe); });
    }

    void TearDown() override
    {
        drainRunning_.store(false, std::memory_order_relaxed);
        drain_thread_.join();
        work_guard_.reset();
        io_context_.stop();
        if (server_thread_.joinable())
            server_thread_.join();
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

    std::thread t1([&]()
                   {
          for (int i = 1; i <= 100; ++i)
              c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(i, 1, 100, "AAPL")); });
    std::thread t2([&]()
                   {
          for (int i = 101; i <= 200; ++i)
              c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(i, 1, 100, "AAPL")); });

    t1.join();
    t2.join();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}