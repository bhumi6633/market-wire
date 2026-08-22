# Benchmark methodology

`efe_bench` compares generated and handwritten/reference decoders using an identical repeating A/E/X/D/U stream. It performs two warmups, seven measured trials, and alternates implementation order to reduce run-order bias.

Throughput is timed over the complete stream. Timing samples cover batches of 256 messages so measurements exceed clock resolution; the program reports median batch microseconds, not fabricated per-message nanosecond percentiles. A consumed event-derived sink and compiler fences prevent removal of decoding work. Compiler, build mode, OS, and machine architecture are printed.

Results remain local wall-clock microbenchmarks. Serious performance work should additionally pin a physical core, control frequency scaling, characterize cache state, collect hardware counters, and report repeated trials on an isolated host. These measurements do not establish production or HFT latency.
