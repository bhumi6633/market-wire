# Agent instructions

- Keep the order-book invariant: only contiguous sequenced market events reach it.
- Do not replace fixed-point prices with floating point in wire or persistent book state.
- Do not add per-order `new`/`delete` on the hot path.
- Protocol offsets belong in the `.efe` schema and generated views, not duplicated manually, except the intentionally simple reference decoder used for differential testing.
- Every optimization needs a correctness test and benchmark before/after.
- Do not claim production/HFT latency from CI or container benchmarks.
- Use official Nasdaq specifications when changing ITCH or MoldUDP64 layouts.
