# Learning path through the codebase

Use this order if the goal is to understand the project rather than merely run it.

1. Start with `include/efe/types.hpp` for fixed-point prices, fixed-width symbols, and event state.
2. Move to `src/order_book.cpp` for price levels, price-time priority, intrusive FIFO links, the arena, and order lookup.
3. Open `schemas/itch50.efe` to see how market messages map to binary layouts.
4. Read `tools/protocolc/main.cpp` to follow the lexer, parser, semantic checks, and code generator.
5. Inspect `build/generated/efe/generated/itch50.hpp` to see what the build-time abstraction produces.
6. Compare the generated and handwritten paths in `src/itch_decoder.cpp`.
7. Read `src/moldudp64.cpp` for packet framing, message blocks, session markers, and byte order.
8. Follow gap detection and ordered release in `src/recovery_engine.cpp`.
9. Read `src/capture.cpp` and `src/replay.cpp` to understand reproducible input.
10. Review `bench/benchmark.cpp`, then read the methodology before interpreting its numbers.
11. Finish with `fuzz/` and a sanitizer build to see how malformed inputs are handled.

For every layer, ask three questions: what invariant does it own, what input can violate that invariant, and how is the failure surfaced without corrupting downstream state?

## Suggested hands-on path

1. Build and run `feed_engine demo`; predict the gap and final best bid before reading the output.
2. Generate `sample.efc`, replay it at maximum speed, and confirm repeated runs produce the same state hash.
3. Read `tests/test_transport.cpp` beside `src/recovery_engine.cpp` and trace how a future packet becomes releasable.
4. Change a copy of the schema, run `efe_protocolc`, and inspect the generated header. Do not edit production offsets outside the schema.
5. Read the randomized book model in `tests/test_order_book.cpp`; compare its simple state representation with the arena-backed implementation.
6. Run `efe_bench`, then read `docs/benchmarking.md` before interpreting the numbers.
7. Build with sanitizers and fuzzing to see how malformed inputs are kept outside trusted state.

## Commands

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/feed_engine demo
./build/feed_engine make-sample-capture sample.efc
./build/feed_engine replay sample.efc --speed max
./build/efe_bench
```

Use the official [ITCH 5.0](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf) and [MoldUDP64](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf) specifications whenever you compare this implementation with the wire protocols.
