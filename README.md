# Lite-Exchange

A low-latency matching exchange in C++23. Binary wire protocol, lock-free SPSC rings, a flat-array order book with O(1) best-price lookup, and a matching engine sharded by instrument.

**470 ns order-to-fill in process. 8.3M orders/sec across two shards. 53 ns of match compute per order.**

Built test-first. Every structural decision was measured against an alternative rather than adopted on reputation, and the results are reproducible from the benchmarks in this repository.

## Performance

All figures measured on AWS EC2, 4 vCPU at 3.0 GHz (TSC-calibrated), Ubuntu with kernel 6.17. Cores 2–3 isolated via `isolcpus` + `nohz_full` + `rcu_nocbs`. 2 MB huge pages enabled. gcc 13, `-O3 -march=native`, LTO on, sanitizers off.

### Order-to-Fill Latency

Full round trip in process: order pushed onto the inbound ring, matched, fill returned on the outbound ring.

| Metric | Result |
| ------ | ------ |
| p50 | **470 ns** |
| p99 | 560 ns |
| p99.9 | 600 ns |

### Latency Breakdown

The same round trip sub-stamped with `rdtscp` at each hand-off, isolating transport cost from compute cost.

| Segment | p50 | Share |
| ------- | --- | ----- |
| Inbound ring transit | 237 ns | 49% |
| **Match compute** | **53 ns** | **11%** |
| Outbound ring transit | 183 ns | 38% |
| Total | 487 ns | — |

Matching is the cheapest part of the path. Cross-core ring transit dominates, which is the correct place to look for further latency work and the reason the engine is structured around rings rather than shared state.

### Shard Scaling

Aggregate matching throughput as instruments are split across shards, driving `ShardRouter` directly with no network layer in the path.

| Shards | Throughput | Speedup |
| ------ | ---------- | ------- |
| 1 | 4.66M orders/sec | 1.00× |
| 2 | **8.32M orders/sec** | **1.79×** |

Perfect linear scaling would be 9.31M. The 11% shortfall is one producer thread splitting writes across two rings rather than any contention between shards — the shards share no mutable state.

Only two shard counts are reported. The host has four vCPUs, and the producer, drainer, and two shards consume all of them. A four-shard configuration would place six threads on four cores and report scheduler contention rather than scaling, so no such figure is published.

### Gateway Throughput

Two reactor implementations over loopback TCP, measured on both axes.

| Reactor | Order-to-ack p50 | Throughput @ 128 connections |
| ------- | ---------------- | ---------------------------- |
| epoll | **16.0 µs** | 164k orders/sec |
| io_uring | 18.0 µs | 177k orders/sec |
| io_uring + SQPOLL | — | **290k orders/sec** |

io_uring is 2 µs *slower* than epoll on single-client latency and 77% faster on many-connection throughput. It is a scalability optimisation, not a latency one. Reporting only one of these axes would have produced a misleading conclusion in either direction.

### The System Bottleneck

At 53 ns of match compute, a single shard absorbs roughly 19M orders/sec. The fastest reactor measured delivers 0.29M.

| Component | Throughput | Relative |
| --------- | ---------- | -------- |
| Matching engine, 2 shards | 8.32M orders/sec | — |
| io_uring + SQPOLL gateway | 0.29M orders/sec | 29× slower |
| epoll gateway | 0.16M orders/sec | 51× slower |

The network layer bounds the system by one and a half orders of magnitude. Sharding the engine further cannot move a system limited here, and any end-to-end benchmark driven through sockets would report the reactor's rate for every shard count — which is precisely why the scaling benchmark bypasses it.

## Architecture

### Data Path

```
client TCP ─▶ Gateway ─▶ Session ─▶ SPSC ring ─▶ Shard ─▶ SPSC ring ─▶ Gateway ─▶ client
              (epoll /   (framing,              (OrderBook
               io_uring)  session id)            match)
```

### Design Principles

- **The gateway and the engine are decoupled by a message transport, not a function call.** The gateway writes into per-shard rings; shards consume from them. Neither holds a pointer to the other. This mirrors how production venues separate their edge from their matching core, and it is what makes process separation a later refactor rather than a rewrite.
- **Routing is unconditional.** A single shard is `N = 1`, never a special case. There is no `if (shards == 1)` fast path to rot.
- **SPSC is preserved end to end.** One producer per inbound ring, one consumer per outbound ring. A multi-threaded gateway would make per-shard inbound MPSC; that constraint is explicit rather than incidental.
- **No allocation on the hot path.** Orders live in a pre-faulted arena on a single huge page, indexed by slot rather than pointer.
- **Compile-time capacities.** Arena size, ladder depth, ring depth, and instrument count are template parameters, so every array is fixed-size with a known layout.

### Components

| Component | Responsibility |
| --------- | -------------- |
| `queue/spsc.hpp` | Single-producer/single-consumer ring. Cache-padded head and tail, acquire/release ordering, no locks. |
| `book/order_book.hpp` | Flat price-array book. O(1) `find_best` via occupancy-bitmap bit scan. Borrows its arena rather than owning it. |
| `book/level_bitmap.hpp` | Per-side occupancy bitmap over the price ladder; `next_up` / `next_down` compile to bit-scan instructions. |
| `book/pool.hpp` | Index-based order arena on one huge page, with a per-slot generation counter for ABA safety. |
| `book/price_level.hpp` | Intrusive FIFO queue per price level, threaded through arena indices. |
| `proto/messages.hpp` | POD wire structs and tagged unions. Every size and layout fixed by `static_assert`. |
| `proto/codec.hpp` | Frame decoding with partial-message handling. |
| `net/session.hpp` | TCP reassembly and framing. Stamps the server-authoritative session id on ingress. |
| `net/gateway.hpp` | epoll reactor. Owns the session→fd routing table and drains every shard's outbound ring. |
| `net/uring_gateway.hpp` | io_uring reactor with optional SQPOLL. |
| `engine/shard.hpp` | One matching shard. Hosts books per instrument over a single shared arena. |
| `engine/shard_router.hpp` | Routes new orders by symbol and cancels by the shard id packed into their token. |
| `engine/shard_set.hpp` | Owns N shards plus the router. The handle the gateway holds. |
| `util/huge_page.hpp` | 2 MB huge-page allocation with fallback. |
| `util/tsc.hpp` | `rdtscp` timestamping and TSC frequency calibration. |

Extended design notes are in [ARCHITECTURE.md](ARCHITECTURE.md).

## Engineering Decisions

### Occupancy Bitmap for Best-Price Lookup

`find_best` originally scanned the price ladder linearly, making it O(ladder depth) and the dominant cost in the profile. Replacing the scan with a per-side occupancy bitmap and a bit-scan instruction reduced order-to-fill p50 from **1475 ns to roughly 390 ns**.

This single algorithmic change outperformed every memory-layout optimisation attempted before it — combined. Cache-line alignment, struct packing, and huge pages together produced a fraction of the improvement that removing the linear scan did.

### Exchange-Assigned Cancel Tokens

Cancellation normally requires a client-order-id lookup structure on the hot path. This engine returns an opaque 64-bit token on every acknowledgement instead:

```
[63:56] shard | [55:24] generation | [23:0] slot
```

- **Slot** indexes directly into the order arena. No hash map, no tree, no lookup.
- **Generation** increments each time a slot is recycled, so a stale token is rejected rather than cancelling an unrelated order. This makes the scheme ABA-safe.
- **Shard** lets a cancel find its owning shard even though the message carries no symbol — the routing problem that symbol-based sharding otherwise creates.

The order book masks the shard bits, so it remains shard-agnostic.

### Shared Arena, Per-Instrument Books

Each shard hosts many instruments, and each instrument needs its own book. The choice was per-instrument arenas or one arena per shard.

- **Per-instrument arenas** would statically partition capacity, so one active instrument starves while others sit idle. More importantly, a slot index would only be unique within an instrument, forcing the symbol into the cancel token and consuming generation bits.
- **One shared arena per shard** keeps slot indices unique shard-wide, so the token format needed no change at all. `handle_cancel` validates slot and generation, then reads the owning instrument off the live order. Capacity is pooled, so a busy instrument can use the whole arena.

The ordering is load-bearing: the generation check must precede reading the instrument, because a stale token can name a recycled slot now holding an order in a different book.

### Instruments as Reference Data

A shard hosts a subset of instruments whose global ids are sparse and not zero-based. Two ways to map a wire symbol id to a local book:

- **Arithmetic** (`symbol / N`) requires the shard to know the shard count, coupling the matching engine to the routing topology, and assumes contiguous ids.
- **A lookup table** maps global id to a dense book slot, which is how venues actually assign instruments to engines.

The table costs 2 KB per shard and cut per-shard memory **3.4× at 1024 instruments** versus indexing books by global id, because a shard no longer allocates ladders for instruments it does not own. An id the shard does not host is rejected rather than silently matched into a neighbouring book.

### Benchmark Design

Two benchmarking failures were specifically engineered against:

- **Measuring the wrong component.** The shard-scaling benchmark drives the router directly rather than through the gateway. Since the reactor is 29–51× slower than the engine, a socket-driven benchmark would return the reactor's rate for every shard count and make sharding appear worthless.
- **Trusting an unvalidated load generator.** A producer-fed benchmark reports a false plateau when the producer is the slower side. The producer therefore counts every push that found the ring full. That rate stayed between 95.6% and 98.8%, proving the rings were saturated and the shards — not the load generator — were the limit.

## Repository Layout

```
include/lx/       header-only engine
  book/           order book, price levels, occupancy bitmap, arena
  engine/         shard, router, shard set
  net/            session framing, epoll and io_uring gateways
  proto/          wire messages and codec
  queue/          SPSC ring
  util/           huge pages, affinity, TSC, cache primitives
test/             GoogleTest suites, one per component
bench/            latency, breakdown, gateway, and shard-scaling benchmarks
scripts/          EC2 bootstrap
ARCHITECTURE.md   design notes
```

## Building

### Requirements

- A C++23 compiler (gcc 13+ or clang 17+)
- CMake 3.28+
- GoogleTest
- liburing
- hdr_histogram

### Build and Test

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release
```

15 test suites cover the ring, arena, price levels, occupancy bitmap, order book, wire protocol, session framing, both gateways, the shard, the router, the shard set, and instrument isolation.

### Build Types

- **Debug** — AddressSanitizer and UndefinedBehaviorSanitizer enabled, frame pointers retained. The default when no build type is specified.
- **Release** — `-O3 -march=native -fno-plt`, LTO, sanitizers off. Required for any benchmark figure.
- **RelWithDebInfo** — `-O2` with frame pointers and DWARF, for profiling.

## Benchmarks

### Environment

```sh
sudo sysctl vm.nr_hugepages=256
```

Huge-page allocation falls back to 4 KB pages **silently** when `mmap(MAP_HUGETLB)` fails, so this must be set before any measurement. Core isolation via `isolcpus` and `nohz_full` on the kernel command line is assumed by the pinning in each benchmark.

### Suite

```sh
build-release/bench/latency_bench                    # order-to-fill, in process
build-release/bench/latency_breakdown                # rdtscp sub-stamped segments
build-release/bench/gateway_latency                  # order-to-ack over epoll TCP
build-release/bench/gateway_latency_uring [sqpoll]   # same over io_uring
build-release/bench/gateway_throughput [epoll|uring|sqpoll]
build-release/bench/shard_scaling [orders]           # throughput vs shard count
```

### Methodology

- Latency benchmarks report percentiles from HdrHistogram, not means. A mean latency figure hides the tail that matters.
- Timing uses `rdtscp` with a calibrated TSC frequency rather than `clock_gettime`, avoiding a syscall inside the measured region.
- Every measured thread is pinned to an isolated core.
- `shard_scaling` reports a producer stall rate alongside throughput so the figure can be checked for load-generator saturation.
- Reported figures are medians across repeated runs, with run-to-run spread noted in the benchmark documentation.

## Project History

An earlier engine (v1) preceded this one: C++20, FIX protocol, `absl::flat_hash_map` order storage, roughly 57 µs round trip. It remains in the git history prior to the v2 restructure.

v2 is a ground-up rewrite driven by what the first version got wrong — a binary protocol instead of FIX parsing, index-based storage instead of hash maps, a flat price array instead of node-per-level allocation, and measurement at every step instead of at the end.
