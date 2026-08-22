# Learning path through the codebase

Use this order if the goal is to understand the project rather than merely run it.

1. `include/efe/types.hpp` — fixed-point prices, fixed-width symbols, event state.
2. `src/order_book.cpp` — price levels, price-time priority, intrusive FIFO, arena/free list, open-addressed ID lookup.
3. `schemas/itch50.efe` — map market semantics to binary wire layouts.
4. `tools/protocolc/main.cpp` — lexer, parser, AST-ish structures, semantic checks, code generation.
5. generated `build/generated/efe/generated/itch50.hpp` — what build-time abstraction becomes at runtime.
6. `src/itch_decoder.cpp` — normalization from protocol-specific messages to domain events; compare generated vs handwritten decoder.
7. `src/moldudp64.cpp` — framing, message blocks, heartbeat/end-of-session, network byte order.
8. `src/recovery_engine.cpp` — sequence gaps, reordering, re-request ranges, contiguous release invariant.
9. `src/capture.cpp` + `src/replay.cpp` — deterministic input reproduction.
10. `bench/benchmark.cpp` — throughput and tail-latency measurement; then use hardware counters.
11. `fuzz/` + sanitizer build — malformed-input and memory-safety hardening.

For every layer, ask three questions: what invariant does it own, what input can violate that invariant, and how is the failure surfaced without corrupting downstream state?
