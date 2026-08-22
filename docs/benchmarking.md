# Benchmark methodology

`efe_bench` compares generated and handwritten/reference decoders using an identical repeating A/E/X/D/U stream. It performs two warmups, seven measured trials, and alternates implementation order to reduce run-order bias.

Throughput is timed over the complete stream. Timing samples cover batches of 256 messages so measurements exceed clock resolution; the program reports median batch microseconds, not fabricated per-message nanosecond percentiles. A consumed event-derived sink and compiler fences prevent removal of decoding work. Compiler, build mode, OS, and machine architecture are printed.

Results remain local wall-clock microbenchmarks. Serious performance work should additionally pin a physical core, control frequency scaling, characterize cache state, collect hardware counters, and report repeated trials on an isolated host. These measurements do not establish production or HFT latency.

## Workload

The stream repeats order-affecting `A/E/X/D/U` messages so decoding is consumed by real book transitions rather than timed in isolation. Both implementations receive identical bytes and produce an event-derived sink and final state hash. Any comparison is invalid if those outputs diverge.

## Reading the output

- **Messages per second** is aggregate throughput for this synthetic stream.
- **Microseconds per 256-message batch** keeps samples above clock resolution; it is not a per-message tail-latency percentile.
- **Median across trials** reduces sensitivity to a single interruption but does not eliminate OS noise.
- **Final state hash** is a correctness check, not a performance metric.

The reference decoder being faster or slower is a result, not a verdict. Generated code may improve maintainability without winning every microbenchmark, and optimization work should begin with profiles rather than assumptions.

## Reproducible comparison

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel
./build-release/efe_bench
```

Record the commit, compiler/version, flags, CPU, operating system, power mode, and full output. Compare changes on the same machine and keep correctness tests unchanged. CI and container numbers are useful regression signals, not evidence of production latency.
