# v2 Optimization Decisions — Interview Reference

Each entry follows the same structure:
- **What we did** — the concrete choice
- **Why** — the mechanism (CPU/kernel/compiler level)
- **Alternatives considered** — and why we didn't pick them
- **Interview question it answers**

Updated after every phase as new decisions land.

---

## Memory & Allocation

### Huge pages for the order pool and SPSC queues

**What we did:** back the order pool and queue storage with `mmap(MAP_HUGETLB)` (2 MiB pages)
instead of `new` / `malloc` (4 KiB pages).

**Why:** every memory access goes virtual → physical through the TLB. An L1 dTLB on this
Xeon holds ~64 entries. With 4 KiB pages, 64 entries covers only 256 KiB — a pool of 100K
orders (each ~64 B = ~6 MiB) causes constant TLB misses, each costing a 4-level page-table
walk (~10–20 ns, 4 dependent cache-miss loads). A 2 MiB huge page is one TLB entry covering
512× more memory — the entire working set fits in a handful of entries.

**Alternatives considered:**
- `new` / `malloc` — simple, but 4 KiB pages thrash the TLB under load.
- `madvise(MADV_HUGEPAGE)` (transparent huge pages, THP) — the kernel *may* coalesce pages
  into huge pages over time, but it's non-deterministic and can stall on a background
  `khugepaged` scan at the worst moment. `MAP_HUGETLB` is explicit and immediate.
- `jemalloc` with huge-page arenas — avoids manual `mmap`, but adds a dependency and still
  doesn't guarantee huge pages for every allocation.

**Interview question:** *"How do you reduce TLB pressure in a high-frequency system?"*

---

### Index-based order pool (uint32_t slot IDs, not pointers)

**What we did:** the pool hands out `uint32_t` slot indices; an `Order*` is reconstructed
as `base + idx * sizeof(Order)` when needed. Intrusive list links store `uint32_t` indices,
not raw pointers.

**Why:**
- **Size:** a pointer is 8 bytes; a slot index is 4 bytes. An `Order` with two intrusive
  list links saves 8 bytes — that's the difference between fitting 8 vs 10 orders per
  64-byte cache line. More orders per line = fewer cache misses per traversal.
- **Relocatability:** if we ever snapshot the pool to disk or shared memory, raw pointers
  are meaningless in another address space; indices are not.
- **No pointer arithmetic bugs:** offset from a known base is easier to sanitize (index
  out of bounds is detectable; a wild pointer is not).

**Alternatives considered:**
- Raw `Order*` pointers — simpler code, but 8-byte links, not relocatable.
- Tagged pointers (store metadata in low bits) — clever, but fragile and UB in C++.

**Interview question:** *"Why would you use index-based pools instead of pointer-based ones?"*

---

## Concurrency & Threading

### SPSC queues on the hot path (not MPSC, not mutex)

**What we did:** gateway→shard uses a single-producer / single-consumer ring per shard.

**Why:** a mutex takes a syscall on contention (~1 µs minimum). An MPSC (multi-producer)
queue needs a CAS (compare-and-swap) on the tail — under contention, CAS retries cause
the CPU to broadcast an MESI "invalidate" to every other core holding that cache line,
then wait for acknowledgement (~100–300 ns per retry, can stack). An SPSC queue needs only
one `store(release)` by the producer and one `load(acquire)` by the consumer — no CAS,
no cross-core invalidation on the hot slot.

**Alternatives considered:**
- `std::mutex` + `std::queue` — correct but ~1 µs per lock/unlock under contention.
- Vyukov MPSC — used for cross-shard signals (where multiple shards may signal one target)
  but kept off the gateway→shard hot path.
- `std::atomic` with `seq_cst` — correct, but `seq_cst` emits a full memory fence (`mfence`
  on x86), which is 2–5× more expensive than `acquire/release`.

**Interview question:** *"Why use SPSC instead of MPSC? What's the CPU-level cost of a CAS
under contention?"*

---

### Sharding by symbol (single-writer-per-shard)

**What we did:** hash `symbol_id % num_shards`; each shard owns its books exclusively and
is driven by a single thread.

**Why:** inside a shard, zero synchronisation is needed — no locks, no atomics, no memory
fences on the matching path itself. The only synchronisation boundary is the SPSC queue at
the shard's edge. Without sharding, all threads would contend on shared book state, and
even a single `std::mutex` on the order book costs ~50–200 ns of contention latency.

**Alternatives considered:**
- Single global matching thread — simple, but one thread saturates at ~5–10M orders/sec
  and cannot use multiple cores.
- Per-symbol fine-grained locks — many locks, complex deadlock avoidance, cache-line
  bouncing on each lock object.
- Lock-free order book (atomic best-bid/ask etc.) — extremely complex, hard to prove
  correct, and the lock-free primitives still generate MESI traffic.

**Interview question:** *"How do you scale a matching engine across cores without locks?"*

---

### CPU isolation (isolcpus + nohz_full + rcu_nocbs)

**What we did:** boot with `isolcpus=2,3,6,7 nohz_full=2,3,6,7 rcu_nocbs=2,3,6,7`;
pin shard threads to those cores via `sched_setaffinity`.

**Why (three separate mechanisms):**
- `isolcpus` — removes CPUs from the scheduler's load-balancing domain. Without it, the
  kernel can migrate any runnable task onto your matching core, causing an involuntary
  context switch (~1–5 µs) + cold L1/L2/TLB.
- `nohz_full` — stops the periodic scheduler tick (normally 250–1000 Hz) on isolated CPUs
  when exactly one task is runnable. Each tick is a timer interrupt that interrupts your
  thread, saves registers, runs kernel bookkeeping, restores — even if it decides not to
  preempt. Adds ~0.5–2 µs of jitter per tick.
- `rcu_nocbs` — offloads RCU callbacks (deferred memory frees etc.) to housekeeping CPUs.
  Without it, a `kfree` somewhere in the kernel might run an RCU callback on your core
  via softirq.

**Alternatives considered:**
- `SCHED_FIFO` / `SCHED_RR` real-time policy — raises priority but doesn't prevent
  interrupts or ticks; weaker than isolation.
- Disabling SMT (hyperthreading) — eliminates noisy-neighbour sharing of L1/L2 and
  execution units between SMT siblings, but halves logical core count. We keep SMT off
  on isolated physical cores by isolating both siblings.

**Interview question:** *"What's the difference between isolcpus, nohz_full, and
SCHED_FIFO? Which gives you the most deterministic latency and why?"*

---

## Data Structures

### Flat price-array order book (not a tree)

**What we did:** `Limit levels_[kLadderSize]` indexed by `(price_ticks - base) / tick`;
`best_bid_idx` / `best_ask_idx` tracked as integers.

**Why:** a `std::map` or red-black tree insert/lookup is O(log N) with pointer chasing
through cache-cold nodes — each pointer dereference is potentially a cache miss (~100 ns
on a cold L3 miss). For a liquid symbol the price spread is narrow; the array covers
100% of traffic with O(1) index arithmetic and no pointer chasing. The best-of-side is
one array lookup at a known index.

**Alternatives considered:**
- `std::map<Price, Limit>` — correct, O(log N), ~5–10 cache misses per insert.
- Skip list — O(log N) average, cache-friendlier than a tree, used in some real systems.
- `std::map` fallback *only* — we keep this for out-of-range prices, but it's not the
  primary path.

**Interview question:** *"Why is a flat array faster than a sorted map for an order book?
When would a tree be preferable?"*

---

### Integer tick prices (int64_t, not double)

**What we did:** all prices are `int64_t` ticks; the tick size is a config constant.
`price_dollars = price_ticks * tick_size`.

**Why:** IEEE 754 double has 53 bits of mantissa — `0.1 + 0.2 != 0.3`. Two orders at
"the same price" that differ by 1 ULP would never match, silently. Integer arithmetic
is exact, and the array index formula (`price_ticks - base`) *requires* exact arithmetic.
Integer compare (`a < b`) is also a single cycle vs floating-point compare which can
raise FP exceptions and requires FP pipeline resources.

**Alternatives considered:**
- `double` — common in naive implementations; silently wrong for price equality.
- Fixed-point with a scaling factor stored as a double — still floating-point under the
  hood.
- `__int128` — overkill; `int64_t` covers any real-world price × tick.

**Interview question:** *"Why should you never use floating point for prices in a
matching engine?"*

---

## I/O & Networking

### io_uring with SQPOLL + RECV_MULTISHOT + registered buffers

**What we did (Phase 5):** set up io_uring with `IORING_SETUP_SQPOLL` (kernel polls the
submission queue), `IORING_OP_RECV_MULTISHOT` (kernel auto-rearms receive), and
`io_uring_register_buffers` (pre-pinned DMA buffers).

**Why:**
- `SQPOLL`: in normal io_uring the userspace calls `io_uring_enter()` (a syscall) to
  notify the kernel of new SQEs. With SQPOLL, a kernel thread polls the SQ ring
  continuously — userspace just writes to shared memory. Zero syscalls in steady state.
  A syscall costs ~100–300 ns (mode switch, TLB flush on some kernels).
- `RECV_MULTISHOT`: normally after each received packet the op is consumed and userspace
  must re-submit. MULTISHOT means the kernel re-arms automatically — no re-submission
  latency, no extra SQE per message.
- Registered buffers: each `read`/`recv` normally calls `get_user_pages()` to pin the
  target buffer in memory. Pre-registering the buffers at startup means that pin happens
  once, not per-op.

**Alternatives considered:**
- `epoll` + blocking `recv` — ~2 syscalls per message (epoll_wait + recv); ~300–600 ns
  overhead vs near-zero for io_uring SQPOLL.
- `epoll` + `SO_BUSY_POLL` — spins the socket RX queue without sleeping, cuts latency but
  still requires syscalls; less composable than io_uring.
- `AF_XDP` / `ef_vi` / DPDK — bypass the kernel entirely, sub-microsecond, but requires
  dedicated NIC and root; out of scope for this project.

**Interview question:** *"Walk me through how io_uring SQPOLL eliminates syscalls on the
receive path."*

---

## Measurement

### rdtscp + CLOCK_MONOTONIC calibration for latency stamps

**What we did (Phase 8):** stamp entry/exit with `rdtscp` (read timestamp counter +
processor ID); calibrate TSC frequency against `CLOCK_MONOTONIC` at startup to convert
ticks → nanoseconds.

**Why:** `clock_gettime(CLOCK_MONOTONIC)` is a vDSO call — cheap (~20 ns), but still
reads a kernel-maintained variable and involves a memory barrier. `rdtscp` is a single
instruction (~7 cycles, ~2 ns) that also returns the CPU ID so you can detect cross-core
migrations mid-measurement. The TSC is invariant on modern Intel CPUs (constant rate
regardless of P-state), so calibration is stable.

**Alternatives considered:**
- `clock_gettime(CLOCK_MONOTONIC)` only — simple, ~20 ns per call, but adds ~20 ns to
  every measured interval.
- `rdtsc` (without the `p`) — doesn't serialize; an out-of-order CPU can execute it
  before the code you want to time; `rdtscp` serializes the instruction stream first.
- Hardware performance counters via `perf_event_open` — accurate but heavy setup;
  better for offline profiling than online latency stamps.

**Interview question:** *"Why use rdtscp instead of clock_gettime for latency
measurement? What does the 'p' in rdtscp do?"*

### HdrHistogram for percentile reporting

**What we did:** record latencies into an HdrHistogram; report p50/p99/p99.9/p99.99.

**Why:** a flat array of raw samples accurate to the nanosecond would need ~3.6 GB for a
1-hour run at 1M samples/sec. HdrHistogram uses a two-level bucketing scheme
(configurable precision × log-scale range) that covers nanoseconds to minutes in ~2 MB,
with configurable significant-digit precision. The tail (p99.9, p99.99) is where HFT
problems hide — a mean or even p99 can look fine while rare 1 ms spikes are occurring.

**Alternatives considered:**
- Sorted `std::vector` — exact, but O(N log N) sort and O(N) memory.
- Fixed-precision histogram (e.g. 1 ns buckets up to 1 ms) — small, but silently clips
  outliers above the range and gives no insight into the tail beyond 1 ms.
- `std::map<uint64_t, uint32_t>` frequency map — exact, but memory unbounded and
  cache-unfriendly.

**Interview question:** *"Why is the p99.9 latency more important than the mean in HFT?
How does HdrHistogram store it efficiently?"*
