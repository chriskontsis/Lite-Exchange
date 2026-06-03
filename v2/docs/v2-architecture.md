# Lite-Exchange v2 — Architecture

> Living document. Updated incrementally as each phase lands.
> v1 (`src/`) is left untouched as a reference baseline (p50 57 µs / p99 64 µs).

---

## Goals

- Sub-microsecond order-book operations (insert, cancel, match).
- Sub-10 µs round-trip p99 end-to-end on this box (c5.2xlarge, colocated NIC).
- Zero heap allocation on the hot path after startup.
- Deterministic latency — no GC pauses, no dynamic dispatch, no lock contention on the
  matching core.

---

## Component Map

```
v2/
├── include/lx/
│   ├── util/
│   │   └── cache.hpp        CachePadded<T>, kCacheLine — false-sharing prevention
│   ├── queue/
│   │   ├── spsc.hpp         Single-producer single-consumer ring (gateway↔shard)
│   │   └── mpsc.hpp         Vyukov bounded MPSC (cross-shard signals only)
│   ├── proto/
│   │   ├── messages.hpp     Packed POD structs for every message type
│   │   └── codec.hpp        tryDecode() → {kOk, kNeedMore, kInvalid}
│   ├── book/
│   │   ├── order.hpp        POD Order — intrusive list pointers, int64_t price_ticks
│   │   ├── pool.hpp         Index-based free-list allocator, huge-page backed
│   │   ├── price_level.hpp  Intrusive FIFO list head + aggregate volume
│   │   └── order_book.hpp   Price ladder + matching logic, per symbol
│   ├── engine/
│   │   ├── shard.hpp        One matching thread + its books + ingress/egress SPSCs
│   │   └── router.hpp       hash(symbol_id) → shard index
│   └── net/
│       ├── uring.hpp        RAII io_uring wrapper (SQPOLL, multishot, reg buffers)
│       └── session.hpp      Per-connection framing buffer + session ID
├── apps/
│   ├── exchange/            Main binary — wires gateway + router + shards together
│   ├── lx-load/             Synthetic load generator (rate, symbols, duration)
│   ├── lx-admin/            Admin/monitoring CLI over Unix-domain socket
│   └── lx-hexdump/          Binary frame decoder (debug tool)
└── scripts/
    ├── ec2-bootstrap.sh     Host tuning (packages, isolcpus, hugepages)
    ├── perf-record.sh       perf record / perf c2c invocations
    └── proto_ref.py         Python reference encoder for differential testing
```

---

## End-to-End Data Flow

```
  lx-load / real clients
  (N TCP connections, binary frames, port 9001)
          │
          │  TCP / loopback
          ▼
  ┌───────────────────────────────────────────┐
  │            io_uring Gateway               │  ← CPU 0 (housekeeping core)
  │                                           │
  │  • SQPOLL: kernel polls SQ ring —         │
  │    userspace never syscalls in steady     │
  │    state                                  │
  │  • RECV_MULTISHOT: kernel auto-rearms     │
  │    receive on each connection             │
  │  • Registered buffers: no per-op          │
  │    get_user_pages() overhead              │
  │                                           │
  │  net/uring.hpp + net/session.hpp          │
  └──────────────┬──────────────┬────────────┘
                 │              ▲
          decode │              │ encode
        (proto/) │              │ (proto/)
                 ▼              │
  ┌──────────────────────────────────────────┐
  │           engine/router.hpp              │
  │                                          │
  │   hash(symbol_id) % num_shards           │
  │   → pick shard → push to ingress SPSC    │
  └──────┬──────────────────────────┬────────┘
         │                          │
   SPSC  │  (one queue per shard)   │  SPSC
  ingress│                          │ ingress
         │                          │
         ▼                          ▼
  ┌─────────────────┐      ┌─────────────────┐
  │    Shard 0      │      │    Shard 1      │
  │  CPU 2          │      │  CPU 3          │
  │  (isolated)     │      │  (isolated)     │
  │                 │      │                 │
  │ book/           │      │ book/           │
  │  order_book.hpp │      │  order_book.hpp │
  │  pool.hpp       │      │  pool.hpp       │
  │  (huge pages)   │      │  (huge pages)   │
  │                 │      │                 │
  │ engine/         │      │ engine/         │
  │  shard.hpp      │      │  shard.hpp      │
  └────────┬────────┘      └────────┬────────┘
           │ SPSC egress            │ SPSC egress
           │ (Fill/Ack/Reject)      │
           └──────────┬─────────────┘
                      ▼
             Gateway encodes + sends
             binary frames back to
             the originating session


  ┌──────────────────────────────────────────┐
  │          lx-admin (separate plane)       │
  │                                          │
  │  Unix-domain socket (not port 9001)      │
  │  Line protocol: book, stats, sessions,   │
  │  kill                                    │
  │  Never touches the hot path              │
  └──────────────────────────────────────────┘
```

---

## Threading Model

| Thread | Pinned to | Role |
|---|---|---|
| Gateway | CPU 0 | io_uring event loop — accept, recv, send; pushes decoded messages to shard ingress SPSCs; drains shard egress SPSCs and sends responses |
| Shard 0 | CPU 2 | Spins on ingress SPSC; runs matching engine; pushes Fill/Ack/Reject to egress SPSC |
| Shard 1 | CPU 3 | Same as Shard 0, different symbols |
| Admin | CPU 1 | Unix-domain socket listener; reads book snapshots / stats — never writes shared matching state |

SMT siblings (CPUs 4–7) of the isolated cores are also isolated but kept idle — a noisy SMT sibling shares L1/L2 and execution units, so parking anything there would steal from the matching thread.

**Why single-writer-per-shard?** Each shard's ingress SPSC has exactly one producer (the gateway thread) and one consumer (the shard thread). No CAS retry, no cache-line ping-pong on the queue slot — just a store-release / load-acquire pair. This is the core reason SPSC is chosen over MPSC for the hot path.

---

## Wire Protocol — Binary Frames

No text parsing. Every message is a fixed-size packed POD.

```
 0       1       2       3       4  ...  N
 ┌───────┬───────┬───────┬───────┬──────────────────┐
 │  len  │  len  │ type  │ flags │     body          │
 │ (u16) │       │ (u8)  │ (u8)  │  (len - 4 bytes)  │
 └───────┴───────┴───────┴───────┴──────────────────┘
  little-endian
```

Receiver: read 4-byte header → validate `len` is in `[4, kMaxFrameLen]` → wait for remaining `len - 4` bytes → `reinterpret_cast<const MsgType*>(buf)`. Zero copies, zero allocation, zero branches on the field values until the cast.

### Inbound message types

| type | name | size |
|---|---|---|
| 0x01 | Logon | 16 B |
| 0x02 | NewOrder | 32 B |
| 0x03 | CancelOrder | 16 B |
| 0x04 | ReplaceOrder | 32 B |

### Outbound message types

| type | name | size |
|---|---|---|
| 0x81 | Ack | 16 B |
| 0x82 | Reject | 16 B |
| 0x83 | Fill | 32 B |
| 0x84 | CancelAck | 16 B |

---

## Order Book — Price Ladder

```
 ask side  ┌─────────────────────────────────────────┐
           │ price_ladder[ask_base + N]   ← best ask  │  best_ask_idx tracks this
           │ price_ladder[ask_base + N-1]              │  (walks up on exhaustion)
           │  ...                                      │
           │ price_ladder[ask_base + 1]                │
           │ price_ladder[ask_base + 0]                │
           ├─────────────────────────────────────────┤
           │ price_ladder[bid_base + 0]                │
           │ price_ladder[bid_base + 1]                │
           │  ...                                      │
           │ price_ladder[bid_base + M-1]              │
           │ price_ladder[bid_base + M]   ← best bid  │  best_bid_idx tracks this
 bid side  └─────────────────────────────────────────┘

  index = (price_ticks - base_price) / tick_size   → O(1) lookup
  each slot = intrusive FIFO list of Order* (price-time priority)
  std::map fallback for prices outside the ladder range
```

**Why a flat array, not a `std::map`?** A tree lookup is O(log N) with pointer chasing through cache-cold nodes. A flat array lookup is O(1) — one integer arithmetic op, one array dereference. At 100K+ orders/sec the difference is measurable. For liquid symbols (small spread, stable price range) the array covers 100% of traffic. The map fallback handles rare far-out-of-range prices without bounding the array size.

**Why `int64_t` ticks, not `double`?** Floating-point comparison is not exact — `0.1 + 0.2 != 0.3` in IEEE 754. Two orders at "the same price" that differ by 1 ULP would never match. Integer ticks are exact. The array index formula also requires exact arithmetic. Prices never leave integer-tick space; the tick size is a configuration constant.

---

## Key Design Decisions

| Decision | Why |
|---|---|
| Binary protocol, fixed-size frames | Zero allocation, zero parsing, branch-predictable header validation |
| SPSC queues on the hot path | Single writer + single reader = no CAS, no contention |
| Sharded by symbol | Each shard is single-threaded — no locking inside a shard at all |
| Huge pages for pool/queues | 512× TLB coverage per entry — eliminates TLB misses on large hot allocations |
| `isolcpus` + `nohz_full` | No involuntary context switches, no periodic tick interrupts on matching cores |
| No virtual dispatch on hot path | vtable call = indirect branch = branch-predictor miss + icache miss; switch on message type is direct and predictable |
| HdrHistogram for latency | Accurate percentiles (p99.9, p99.99) with minimal memory; a flat array loses precision at the tails |

---

## Phase Checklist

- [x] Phase 0 — Environment & skeleton (bootstrap, CMakeLists, smoke test)
- [ ] Phase 1 — Cache-line primitives & queues (util/cache.hpp, queue/spsc.hpp, queue/mpsc.hpp)
- [ ] Phase 2 — Binary protocol (proto/messages.hpp, proto/codec.hpp)
- [ ] Phase 3 — Order pool & price ladder (book/order.hpp, pool.hpp, price_level.hpp)
- [ ] Phase 4 — Matching engine per shard (book/order_book.hpp)
- [ ] Phase 5 — io_uring gateway (net/uring.hpp, net/session.hpp)
- [ ] Phase 6 — Sharded engine router (engine/shard.hpp, engine/router.hpp)
- [ ] Phase 7 — Admin & load generator CLIs
- [ ] Phase 8 — Linux tuning & measurement (hugepages wired, perf c2c, bench gating)
