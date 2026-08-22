# Exchange Feed Engine

A C++20 research/learning implementation of a low-latency market-data stack built around **Nasdaq TotalView-ITCH 5.0** carried over **MoldUDP64**.

The project asks one engineering question:

> Can a schema-generated decoder remain maintainable while getting close to a handwritten binary decoder, without giving up deterministic correctness and recovery?

This repository is deliberately infrastructure, not a trading bot. It receives/decodes market-data events, restores ordered delivery after gaps, reconstructs the displayed order book, captures wire packets, replays them deterministically, and benchmarks generated vs. handwritten decoding.

## What is implemented

- **Protocol compiler (`efe_protocolc`)**
  - custom DSL
  - lexer/parser/semantic checks
  - build-time C++ code generation
  - fixed offsets and typed accessors
  - 23 current ITCH 5.0 message layouts in `schemas/itch50.efe`
- **Generated ITCH decoder**
  - big-endian integer decoding
  - six-byte timestamps
  - fixed-width ASCII views
  - Price(4) / Price(8) raw fixed-point fields
  - all schema messages are length/type validated
  - book-changing `A/F/E/C/X/D/U` messages normalize into market events
- **Differential reference decoder** for the book-changing messages
- **MoldUDP64**
  - downstream packet header
  - variable message blocks
  - heartbeat/end-of-session handling
  - request-packet encoding
- **Recovery engine**
  - sequence-gap detection
  - future-message buffering while recovery is outstanding
  - duplicate/stale suppression
  - contiguous release only
  - partial re-request support
- **Order-book reconstruction**
  - fixed-capacity order arena
  - fixed-width 8-byte symbols (no per-order `std::string`)
  - preallocated open-addressed order-ID index
  - O(1)-style order lookup
  - ordered bid/ask price levels
  - intrusive FIFO links inside arena slots for price-time priority
  - Add / Execute / Cancel / Delete / Replace
  - deterministic state hash
- **Capture / replay**
  - `EFC2` packet capture format
  - maximum-speed, realtime, scaled, and step replay modes in the library
- **Networking**
  - POSIX UDP multicast receiver
  - UDP unicast re-request sender
- **Performance lab**
  - generated-vs-handwritten decoder throughput
  - P50/P95/P99/P99.9 latency samples
- **Hardening**
  - unit/integration tests
  - optional ASan + UBSan builds
  - optional Clang/libFuzzer targets for ITCH and MoldUDP64 inputs
- **CI** via GitHub Actions

## Architecture

```text
                       UDP multicast
                            |
                            v
                  +-------------------+
                  | MoldUDP64 parser  |
                  +---------+---------+
                            |
                            v
                  +-------------------+
                  | Recovery engine   |<----- UDP unicast rerequest
                  | gap + reorder     |
                  +---------+---------+
                            |
                     contiguous stream
                            |
                            v
                  +-------------------+
                  | generated ITCH    |
                  | message views     |
                  +---------+---------+
                            |
                       normalized events
                            |
                            v
                  +-------------------+
                  | arena order book  |
                  +-------------------+

 protocol schema -> lexer -> parser/AST -> semantic checks -> C++ generator

 wire datagrams -> EFC2 capture -> deterministic replay -> same pipeline
```

The key invariant is that the order book never sees event `N+1` before event `N`.

## Build

Linux/macOS with a C++20 compiler and CMake 3.20+:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The build first compiles `efe_protocolc`, then runs it on `schemas/itch50.efe`, then compiles the generated header into the feed engine.

## Demo

```bash
./build/feed_engine demo
```

The demo intentionally delivers sequence 3 before sequence 2. The recovery engine buffers 3, reports a gap, then releases 2 and 3 in order when the missing message arrives.

## Benchmark

```bash
./build/efe_bench 500000
```

This compares the schema-generated book-message decoder with the deliberately straightforward handwritten reference decoder. Treat results as machine/build specific; do not copy benchmark numbers into a resume without measuring your own release build on controlled hardware.

For hardware-counter work on Linux:

```bash
perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
  ./build/efe_bench 1000000
```

## Sanitizers

```bash
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DEFE_ENABLE_SANITIZERS=ON \
  -DEFE_BUILD_BENCHMARKS=OFF
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Fuzzing

With Clang:

```bash
cmake -S . -B build-fuzz \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DEFE_ENABLE_FUZZING=ON \
  -DEFE_BUILD_BENCHMARKS=OFF
cmake --build build-fuzz -j
./build-fuzz/fuzz_moldudp -max_total_time=30
./build-fuzz/fuzz_itch -max_total_time=30
```

## Live UDP mode

```bash
./build/feed_engine listen \
  <multicast-group> <port> \
  <rerequest-host> <rerequest-port> \
  [capture.efc]
```

This is a networking implementation, **not an entitlement to Nasdaq data**. A real direct-feed connection requires the appropriate commercial access, network connectivity, configuration, and operational controls. Use synthetic/sample/historical traffic unless you legitimately have such access.

## Replay

Create a self-contained synthetic capture first if you want to exercise replay without external data:

```bash
./build/feed_engine make-sample-capture sample.efc
```

Then replay it:

```bash
# 0 means maximum speed
./build/feed_engine replay capture.efc 0

# 10x recorded timing
./build/feed_engine replay capture.efc 10
```

The final `state_hash` is intended as a compact deterministic-regression signal: identical ordered input should lead to identical displayed-book state.

## Protocol DSL

A message is described once:

```text
message AddOrder "A" {
    stock_locate: u16_be;
    tracking_number: u16_be;
    timestamp: u48_be;
    order_reference: u64_be;
    side: char;
    shares: u32_be;
    stock: ascii[8];
    price: price4;
}
```

At build time the compiler calculates field offsets and emits a specialized `AddOrderView`. There is no runtime schema lookup for these accessors.

Supported DSL field types:

`u8`, `u16_be`, `u32_be`, `u48_be`, `u64_be`, `char`, `ascii[N]`, `price4`, `price8`.

## Important design decisions

### Build-time abstraction
The schema/compiler contains protocol abstraction. The runtime sees concrete generated C++ classes with constant offsets.

### Correctness before optimization
A handwritten reference decoder exists specifically for differential testing. Optimizations should preserve event equality and deterministic state hashes.

### No per-order arena allocation
Order objects and FIFO links live in a fixed-capacity slot arena. The order-ID index is a preallocated open-addressed table. Price levels still use `std::map` in this research implementation; that is a deliberate remaining optimization surface rather than a fake claim of a completely allocation-free engine.

### Zero-copy means no whole-message materialization
Generated message views point at the received byte span. Numeric accessors still load bytes into registers, as they must. ASCII fields are returned as `std::string_view` and copied only when persistent book state needs the symbol.

### Recovery and state are separate
MoldUDP64/recovery produces a contiguous ordered stream. The ITCH decoder does not know about UDP, and the book does not know about networking.

## What this project does *not* claim

- It is not Nasdaq production software.
- It is not a matching engine or order-entry gateway.
- It does not use kernel bypass, DPDK, Solarflare/OpenOnload, FPGA, or RDMA.
- `std::map` price levels are intentionally not the final word in low-latency data structures.
- Wall-clock microbenchmarks are not a substitute for pinned-core, isolated-host, hardware-counter measurements.

Those boundaries are useful: they make the measured engineering claims defensible.

## Repository layout

```text
include/efe/          core interfaces
src/                  engine implementations + CLI
schemas/              ITCH DSL specification
tools/protocolc/      DSL lexer/parser/code generator
tests/                correctness + differential + replay tests
bench/                decoder benchmark
fuzz/                 libFuzzer entry points
docs/                 architecture and learning notes
.github/workflows/     CI
```

## Primary protocol references

- Nasdaq TotalView-ITCH 5.0 specification: https://classic.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
- Nasdaq MoldUDP64 protocol specification: https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf

The schema should be reviewed whenever Nasdaq publishes a specification revision.
