#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include "../src/ipc/MPSC_Queue.hpp"
#include "../src/ipc/OrderEvent.hpp"

// Single producer throughput
static void BM_MPSC_1Producer(benchmark::State &state)
{
    MPSC_Queue<ipc::OrderEvent, 4096> q;
    ipc::OrderEvent ev;
    for (auto _ : state)
    {
        while (!q.tryPush(ev))
            ;
        benchmark::DoNotOptimize(q.tryConsume(ev));
    }
}
BENCHMARK(BM_MPSC_1Producer);

// Multi producer throughput — N producers, 1 consumer
// Each iteration: all producers push one item, consumer drains all
static void BM_MPSC_MultiProducer(benchmark::State &state)
{
    const int numProducers = state.range(0);
    MPSC_Queue<ipc::OrderEvent, 4096> q;

    for (auto _ : state)
    {
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> producers;

        for (int p = 0; p < numProducers; ++p)
        {
            producers.emplace_back([&]()
                                   {
                  ready.fetch_add(1);
                  while (!go.load(std::memory_order_acquire));
                  ipc::OrderEvent ev;
                  while (!q.tryPush(ev)); });
        }

        while (ready.load() < numProducers)
            ;
        go.store(true, std::memory_order_release);

        ipc::OrderEvent ev;
        int consumed = 0;
        while (consumed < numProducers)
            if (q.tryConsume(ev))
                ++consumed;

        for (auto &t : producers)
            t.join();
    }
}
BENCHMARK(BM_MPSC_MultiProducer)->Arg(2)->Arg(4)->Arg(8);

BENCHMARK_MAIN();
