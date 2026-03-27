#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <chrono>
#include "../src/server/FixServer.hpp"
#include "../src/engine/EngineDispatcher.hpp"
#include "../src/ipc/MPSC_Queue.hpp"
#include "../src/ipc/OrderEvent.hpp"
#include "../src/ipc/FillEvent.hpp"
#include "../src/gateway/SessionRegistry.hpp"
#include "../src/fix/FixMessageBuilder.hpp"
#include "../src/client/FixClient.hpp"
#include "../src/net/EventLoop.hpp"
#include "../src/net/IoHandler.hpp"

static constexpr short PORT = 12399;

struct ServerFixture
{
    MPSC_Queue<ipc::OrderEvent, 4096> inputQ;
    MPSC_Queue<ipc::FillEvent, 4096>  outputQ;
    gateway::SessionRegistry          registry;
    fix::EngineDispatcher             dispatcher{inputQ, outputQ};
    net::EventLoop                    loop;
    fix::FixServer                    server{loop, PORT, inputQ, registry};
    std::atomic<bool>                 server_running{true};
    std::thread                       server_thread;
    std::atomic<bool>                 drain_running{true};
    std::atomic<uint64_t>             fills_received{0};
    std::thread                       drain_thread;

    ServerFixture()
    {
        server_thread = std::thread([this]() {
            net::Event events[64];
            while (server_running.load(std::memory_order_relaxed)) {
                int n = loop.wait(events, 64, /*timeout_ms=*/1);
                for (int i = 0; i < n; ++i) {
                    auto* handler = static_cast<net::IoHandler*>(events[i].ctx);
                    if (events[i].readable) handler->onReadable();
                    if (events[i].writable) handler->onWritable();
                    if (handler->wantsClose()) server.closeSession(events[i].fd);
                }
            }
        });
        drain_thread = std::thread([this]() {
            ipc::FillEvent fe;
            while (drain_running.load(std::memory_order_relaxed))
                if (outputQ.tryConsume(fe))
                    fills_received.fetch_add(1, std::memory_order_relaxed);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~ServerFixture()
    {
        server_running.store(false);
        drain_running.store(false);
        server_thread.join();
        drain_thread.join();
    }
};

// Scenario 1: limit order send rate (no matches)
static void BM_EndToEnd_LimitFlood(benchmark::State &state)
{
    ServerFixture srv;
    fix::FixClient client("127.0.0.1", PORT);
    std::string msg = fix::FixMessageBuilder::limit<LOB::Side::BUY>(1, 1,
                                                                    100, "AAPL");

    for (auto _ : state)
        client.send(msg);

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_LimitFlood)->Iterations(10000);

// Scenario 2: matched pairs — measures full round trip including fill delivery 
static void BM_EndToEnd_MatchedPairs(benchmark::State &state)
{
    ServerFixture srv;
    fix::FixClient c1("127.0.0.1", PORT);
    fix::FixClient c2("127.0.0.1", PORT);

    LOB::UID uid = 1;
    for (auto _ : state)
    {
        srv.fills_received.store(0, std::memory_order_relaxed);
        c1.send(fix::FixMessageBuilder::limit<LOB::Side::BUY>(uid++, 1,
                                                              100, "MSFT"));
        c2.send(fix::FixMessageBuilder::limit<LOB::Side::SELL>(uid++, 1,
                                                               100, "MSFT"));

        // wait for both fill events (aggressor + resting)
        while (srv.fills_received.load(std::memory_order_relaxed) < 2)
            ;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_EndToEnd_MatchedPairs)->Iterations(1000);

BENCHMARK_MAIN();