# Lite Exchange

A high-performance FIX protocol matching engine built in C++20. Designed with lock-free IPC, zero heap allocation on the matching hot path, and kernel-bypass I/O. Benchmarked on AWS EC2 with sub-100 µs round-trip latency.

---

## Performance

Benchmarked on **AWS c5.2xlarge** (8 vCPU, 3.6 GHz, Ubuntu 22.04) with CPU affinity, `-march=native`, and io_uring.

### End-to-End Latency (Order → Fill Round Trip)

| Metric          | Result    |
| --------------- | --------- |
| **p50**         | **57 µs** |
| **p99**         | **64 µs** |
| **p999**        | **93 µs** |
| **CV (jitter)** | **7.7%**  |

p50–p99 spread of just **7 µs**

### Tick-to-Trade Latency (Fill → Re-quote → Ack)

Models a market maker receiving a fill and immediately re-quoting. Measures the full chain: aggressor send → match → fill delivered → re-quote sent → ack received.

| Metric          | Result    |
| --------------- | --------- |
| **p50**         | **86 µs** |
| **p99**         | **91 µs** |
| **p999**        | **94 µs** |
| **CV (jitter)** | **2.5%**  |

p50–p999 spread of just **8 µs**

### Throughput

| Benchmark                       | Throughput    | Latency |
| ------------------------------- | ------------- | ------- |
| Limit order flood (no match)    | 330k orders/s | 4.7 µs  |
| Cancel pressure (65% cancels)   | 391k ops/s    | 3.0 µs  |
| Matched pairs (full round trip) | 26k pairs/s   | 41 µs   |
| Fill throughput (4 pairs)       | 65k fills/s   | 123 µs  |
| Deep book sweep (200 fills)     | —             | 1.24 ms |

### Matching Engine (Isolated, Single Thread)

| Operation            | Throughput        | Latency |
| -------------------- | ----------------- | ------- |
| Limit insert         | 3M inserts/s      | 326 ns  |
| Cancel               | **64M cancels/s** | 16 ns   |
| Market fill          | **47M fills/s**   | 21 ns   |
| Steady state (mixed) | **59M ops/s**     | 17 ns   |

---

## Architecture

### Data Flow

```
[FixClient A] ──┐
[FixClient B] ──┼──► [MPSC inputQ] ──► [EngineDispatcher] ──► [LimitOrderBook]
[FixClient C] ──┘      OrderEvent       (matching thread)            │ fill
                                                                      ▼
[FixClient A] ◄──┐   [MPSC outputQ] ◄──────────────────── FillEvent written
[FixClient B] ◄──┤       FillEvent
[FixClient C] ◄──┘   (drain thread → SessionRegistry → sendData)
```

### Threading Model

| Thread          | Role                                                     | Core |
| --------------- | -------------------------------------------------------- | ---- |
| IO thread       | Accept connections, parse FIX, push to inputQ, send acks | 2    |
| Matching thread | Spin on inputQ, route to per-symbol book, push fills     | 0    |
| Drain thread    | Spin on outputQ, route fills to sessions                 | 1    |

All three threads are pinned to dedicated cores via `pthread_setaffinity_np` — no scheduler migration, no cache eviction between loops.

### Key Design Decisions

**No heap allocation on the hot path.** Every `Order` comes from a pre-allocated pool of 65536 slots (`OrderPool`). `allocate()` and `deallocate()` are O(1) stack-pointer operations with zero `malloc` interaction.

**No virtual dispatch on the hot path.** `ServerBase<Derived>` uses CRTP — `onNewConnection()` resolves at compile time. No vtable lookup per connection.

**No `shared_ptr` on the matching path.** Raw `Order*` backed by the pool. Ownership is enforced by invariant: an order is in the pool ↔ in `UIDtoOrderMap` ↔ in `LimitTree`. The matching thread is the sole writer.

**Intrusive linked list at price levels.** `Order` carries `il_prev_`/`il_next_` pointers directly — no separate list node allocation per enqueue. Cancel is two pointer writes: O(1) with no scan.

**Flat price array replaces `std::map`.** Price levels are a `std::array<Limit, 16384>` indexed by `(price - base)`. Insert is an array store. An intrusive occupied-level list maintains O(1) `findBest` across sparse books.

**Integer product IDs.** `SymbolRegistry` resolves symbol strings to `uint16_t` once at parse time. All downstream code uses the integer directly. `EngineDispatcher::books_` is a direct-indexed `std::array`, not a hash map.

**io_uring replaces epoll.** `IORING_OP_POLL_ADD` (one-shot) replaces `epoll_ctl`/`epoll_wait`. Completions are collected from a user-space ring without entering the kernel on the fast path. Result: p99 dropped from **205 µs → 64 µs**.

**Order acknowledgment.** Every accepted order receives an immediate `35=8|39=0` ack from the IO thread before the order reaches the matching engine. Fills generate `35=8` execution reports routed to both sides by session ID.

**Gateway / Engine decoupling.** The engine (`EngineDispatcher`, `LimitOrderBook`) has zero knowledge of sessions, sockets, or FIX protocol. It sees only `OrderEvent` in and `FillEvent` out. Swapping `MPSC_Queue<T,N>` for a shared-memory-backed queue splits Gateway and Engine into separate OS processes with no engine code changes.

---

## FIX Protocol Support

```
35=D  New Order       40=1  Market    54=1  Buy     59=3  IOC
35=F  Cancel          40=2  Limit     54=2  Sell
35=8  Execution Report (ack or fill)
```

Tags: `11` ClOrdID/UID · `31` LastPx · `32` LastQty · `38` OrderQty · `44` Price · `55` Symbol

Order types supported: **Limit**, **Market**, **IOC** (Immediate-Or-Cancel)

Every accepted order receives an immediate `35=8|39=0` acknowledgment. Fills generate `35=8` execution reports routed to both the aggressor and resting party by session ID.

---

## Build

**Requirements:** CMake 3.20+, Clang, GTest, Google Benchmark, abseil.

```bash
# macOS
brew install googletest google-benchmark abseil

# Linux (EC2)
sudo apt-get install -y libgtest-dev libbenchmark-dev libabsl-dev clang liburing-dev
```

```bash
# Configure and build
cmake -B build .
cmake --build build/

# With io_uring (Linux only)
cmake -B build . -DUSE_IO_URING=ON
cmake --build build/
```

```bash
# Run all tests
ctest --test-dir build

# Start the exchange server (port 12345)
./build/exchange

# Run benchmarks
./build/bench_orderbook      # matching engine microbenchmarks
./build/bench_endtoend       # full network round-trip benchmarks
```

---

## Optimization Journey

Starting from a naive implementation using `std::map`, `std::list`, and `shared_ptr` throughout:

| Optimization                          | Key Metric      | Before          | After              |
| ------------------------------------- | --------------- | --------------- | ------------------ |
| Pool allocator + intrusive list       | Limit insert    | 617 ns          | 212 ns             |
| Flat price array (`std::map` → array) | Dense insert    | 801 ns          | 188 ns             |
| Occupied-level intrusive list         | Sparse cancel   | 3360 ns         | 554 ns             |
| `absl::flat_hash_map` + Release build | Steady state    | 204 ns/op       | **14 ns/op**       |
| Integer product ID (`SymbolId`)       | Routing         | hash map lookup | direct array index |
| CPU affinity (3 dedicated cores)      | MatchedPairs CV | 33.8%           | 25.4%              |
| `-march=native` (AVX2)                | DeepBookSweep   | 1.47 ms         | 0.94 ms            |
| io_uring (replaces epoll)             | **p99 latency** | **205 µs**      | **64 µs**          |

---

## Project Structure

```
src/
├── matching-engine/   LimitOrderBook, LimitTree, OrderPool, OrderStructures
├── engine/            EngineDispatcher (matching thread, per-symbol routing)
├── fix/               FixMessage (parser), FixMessageBuilder, FixSession
├── gateway/           SessionRegistry, SymbolRegistry
├── ipc/               MPSC_Queue, OrderEvent, FillEvent
├── net/               EventLoop (kqueue/epoll/io_uring), IoHandler, SocketUtils
├── server/            FixServer, ServerBase (CRTP)
├── client/            FixClient
└── utility/           ThreadAffinity

bench/                 BenchOrderBook, BenchEndToEnd, TrafficGenerator, BookSeeder
test/                  FIX, OrderBook, SPSC, Integration
docs/                  Optimization log, benchmark results
```

---

## Tech Stack

| Component    | Choice           |
| ------------ | ---------------- |
| Language     | C++20            |
| Build        | CMake            |
| Testing      | Google Test      |
| Benchmarking | Google Benchmark |
| Deployment   | AWS c5.2xlarge   |
