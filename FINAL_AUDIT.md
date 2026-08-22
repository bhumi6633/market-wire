# Final engineering audit

## Verdict

**READY**

This verdict is limited to the repository's stated portfolio/research scope. It is not a claim of production HFT readiness, Nasdaq entitlement or connectivity, exchange certification, operational resilience, or kernel-bypass latency.

## Architecture and implemented subsystems

The runtime remains separated into POSIX UDP reception, MoldUDP64 framing, contiguous sequencing/gap recovery, generated ITCH views, normalized events, and an arena-backed multi-symbol order book. EFC2 capture records whole datagrams and deterministic replay feeds them through the same framing/recovery/decoding/book pipeline. `efe_protocolc` moves schema abstraction to build time and emits specialized fixed-offset C++ views.

Major implementation locations:

- `include/efe/` and `src/`: domain types, safe byte readers, arena/index book, ITCH decoding, MoldUDP64, recovery, capture/replay, metrics, and RAII networking.
- `schemas/itch50.efe`: 23 Nasdaq TotalView-ITCH-style layouts.
- `tools/protocolc/main.cpp`: lexer, parser/AST, semantic checks, and deterministic generator.
- `tests/`: eight discoverable CTest cases, including a deterministic 100,000-transition book model and a 50,000-message end-to-end fault-injection property test.
- `fuzz/`: sanitizer-backed ITCH and MoldUDP64 libFuzzer targets plus checked-in seeds.
- `bench/benchmark.cpp`: warm, repeated, alternating-order mixed decoder benchmark.
- `.github/workflows/ci.yml`: Debug/Release/Werror tests, ASan/UBSan, and Clang fuzz compilation/smoke runs.
- `docs/`: architecture, compiler, recovery, EFC2 format, benchmarking, and learning notes.

## Correctness and hardening fixes

- State hashing now includes symbols, side, ordered levels, aggregate quantity, order IDs, remaining quantities, and FIFO order.
- Replace mutates a slot transactionally and preserves the old order when validation or preparation fails, including at full arena capacity.
- Add rollback removes unused map/index/arena state on overflow or exceptions.
- The open-addressed index now reuses a tombstone after a full probe. The 100,000-transition test discovered the prior long-run exhaustion failure at deterministic transition 68,750.
- Capture records are bounded to 65,535 bytes and oversized/truncated records are rejected before unbounded allocation.
- Replay rejects decreasing timestamps before delay subtraction and checks scaled-delay overflow.
- MoldUDP64 and recovery reject sequence-range exhaustion instead of wrapping.
- UDP construction closes candidate descriptors on every setup exception. A wake pipe provides controlled interruption of blocked receive calls.
- CLI exposes max-speed, scaled, realtime, and step replay plus `--help`.
- Warning categories were fixed and all targets build under the full warning set with `-Werror`.

## Exact validation commands

```bash
cmake -S . -B /tmp/efe-final-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/efe-final-debug --parallel 8
ctest --test-dir /tmp/efe-final-debug --output-on-failure

cmake -S . -B /tmp/efe-final-werror -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-Werror'
cmake --build /tmp/efe-final-werror --parallel 8
ctest --test-dir /tmp/efe-final-werror --output-on-failure

cmake -S . -B /tmp/efe-final-release -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/efe-final-release --parallel 8
ctest --test-dir /tmp/efe-final-release --output-on-failure

cmake -S . -B /tmp/efe-final-sanitizers -DCMAKE_BUILD_TYPE=Debug \
  -DEFE_ENABLE_SANITIZERS=ON -DEFE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/efe-final-sanitizers --parallel 8
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir /tmp/efe-final-sanitizers --output-on-failure

cmake -S . -B /tmp/efe-final-fuzz -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/bin/clang++ \
  -DEFE_ENABLE_FUZZING=ON -DEFE_BUILD_BENCHMARKS=OFF
cmake --build /tmp/efe-final-fuzz --parallel 8
ASAN_OPTIONS=detect_leaks=0 /tmp/efe-final-fuzz/fuzz_itch \
  /tmp/efe-final-itch-corpus -max_total_time=10 -print_final_stats=1
ASAN_OPTIONS=detect_leaks=0 /tmp/efe-final-fuzz/fuzz_moldudp \
  /tmp/efe-final-mold-corpus -max_total_time=10 -print_final_stats=1

/tmp/efe-final-release/efe_bench 500000
/tmp/efe-final-release/feed_engine --help
/tmp/efe-final-release/feed_engine demo
/tmp/efe-final-release/feed_engine make-sample-capture /tmp/efe-final-sample.efc
/tmp/efe-final-release/feed_engine replay /tmp/efe-final-sample.efc --speed max
git diff --check
```

## Build and test results

- Debug: configured and built all targets successfully with AppleClang 16.0.0.
- Strict warnings: the configured `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wsign-conversion` set built cleanly with `-Werror`.
- Release: built `libefe_core.a`, `efe_protocolc`, `feed_engine`, `efe_bench`, and all test executables.
- CTest: 8 registered tests; 7 passed, 0 failed, 1 environment skip. The skipped network test reported `bind: Operation not permitted` because this managed workspace forbids local socket binding. It is configured to execute normally on GitHub's Linux runner.
- Randomized book validation: 100,000 mixed Add/Execute/Cancel/Delete/Replace transitions completed against the reference model with seed `0x0EFE5EED`; live counts, lookup, quantities, sides, prices, best bid/ask, aggregate levels, FIFO, capacity reuse, and symbol isolation were checked.
- End-to-end fault injection: 50,000 binary A/D messages were wrapped in MoldUDP64, deterministically shuffled with seed `0xC01716A9`, duplicated periodically, and ingested through recovery, generated decoding, and the arena book. Exactly sequences 1–50,000 were released contiguously and the final hash matched ordered processing.
- Protocol generation: all 23 repository layouts generated and compiled. Positive/negative semantic cases passed, and two generated outputs had identical SHA-256 hashes.
- Differential decoding: A/F/E/C/X/D/U generated/reference results agreed; every schema size and representative accessor compiled and ran; all truncated prefixes were rejected.
- Recovery/MoldUDP64: in-order, buffered future messages, partial recovery, catch-up, stale messages, second gaps, session changes, heartbeat, end-session, malformed lengths, truncations, request encoding, and sequence overflow passed.
- Replay determinism: the automated pipeline test replayed the same capture three times with identical hashes. CLI replay was also repeated three times with final hash `0xb6cac6573c4522b7` each time.

## Sanitizers and fuzzing

The complete ASan/UBSan test suite passed with no findings. Apple ASan does not support leak detection on this platform, so `detect_leaks=0` was required locally; CI enables leak checking on Ubuntu.

Final bounded fuzz runs, with the core library and targets instrumented by ASan/UBSan:

- ITCH: 1,482,035 executions in 11 seconds, no crash or sanitizer finding.
- MoldUDP64: 237,194 executions in 11 seconds, no crash or sanitizer finding.

The checked-in seeds cover a structurally valid 36-byte Add message and malformed/truncated ITCH and MoldUDP64 prefixes. A preliminary Mold fuzzer assertion incorrectly treated end-session count `0xFFFF` as a normal message-block count; that harness invariant was corrected and the input was rerun successfully.

## Benchmark methodology and observed values

The Release benchmark uses the same repeating A/E/X/D/U workload for generated and reference decoding, performs two warmups, runs seven trials, alternates implementation order, times 256-message batches, and consumes event-derived results through a nonzero sink. It reports no per-message nanosecond percentiles.

Observed locally on arm64 Darwin 24.0.0 with AppleClang 16.0.0, 500,000 messages:

```text
generated median throughput=129.74 M msg/s; median 256-message batch=1.92 us
reference median throughput=274.15 M msg/s; median 256-message batch=0.92 us
sink=215600000
```

These are local wall-clock research measurements only. They are not production or HFT latency claims.

## README claims verified

- C++20 CMake Debug, Release, sanitizer, and Clang fuzz configurations exist.
- Twenty-three schema-generated views build deterministically.
- Generated and handwritten book-message decoders exist and are differentially tested.
- Fixed-point prices, fixed-width symbols, arena slots, intrusive FIFO, and preallocated ID lookup are implemented.
- MoldUDP64 framing/request encoding and contiguous recovery are tested.
- EFC2 maximum-speed, scaled, realtime, and step replay are library- and CLI-tested.
- State hashes cover behaviorally relevant displayed-book state including FIFO.
- GitHub Actions matches documented build/test/fuzz workflows.
- Project limitations explicitly reject real-connectivity and production-HFT claims.

## Remaining limitations

- Price levels use `std::map` and therefore allocate; the project claims only no per-order arena allocation.
- The UDP implementation is POSIX-only. Real exchange access, entitlements, redundant lines, operational monitoring, and production controls are external.
- Local socket integration could not execute inside this managed sandbox; CI is the supported unrestricted local-network check.
- macOS leak sanitizer is unavailable; Linux CI supplies leak checking.
- Fuzzing is bounded smoke coverage, not proof of exhaustive input safety. The corpus should grow with future regression inputs.
- Benchmarks are not core-pinned or frequency-controlled and do not include hardware counters.

## Reproducibility

Build products were created only below `/tmp`; build directories and common artifacts are ignored. CMake regenerates protocol views from the checked-in schema and compiler. No external generated source is required. The tracked-file check is performed after adding this report so a fresh clone contains the complete source, tests, corpora, CI, and documentation.
