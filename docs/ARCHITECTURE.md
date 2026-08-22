# Architecture notes

## 1. Runtime contracts

The runtime is split into layers with narrow contracts:

1. **UDP receiver** returns one datagram.
2. **MoldUDP64 parser** validates framing and turns one datagram into sequenced messages.
3. **Recovery engine** guarantees monotonic contiguous message release. Future messages are buffered; stale duplicates are discarded; a missing range produces a re-request action.
4. **ITCH decoder** validates protocol length/type and normalizes only messages that mutate the displayed book.
5. **Order book** accepts ordered, valid state transitions.
6. **Capture/replay** sits at the wire boundary so the same datagrams can be fed back through the same runtime path.

The order book should never need to know whether a message arrived live, through retransmission, or from replay.

## 2. Recovery state model

Conceptual states are synchronized, recovering, catch-up, and end-of-session, but the implementation encodes the state through `expected_`, the pending sequence map, and the session identity.

For expected sequence `103`, receiving `106..108` stores those messages without applying them. The engine requests `[103,105]`. If a response only returns `103..104`, the expected pointer advances and a new request is produced for the still-missing suffix. Once the lowest pending key equals `expected_`, contiguous pending messages drain in order.

## 3. Book memory model

Each order lives in a preallocated `Slot` containing:

- fixed-width order fields
- previous/next slot indices for FIFO order at one price level
- free-list linkage when unused

The order ID index maps `OrderId -> SlotIndex` with open addressing. No new `Order` allocation occurs on Add. Each price level stores aggregate quantity plus head/tail slot indices.

This removes the `std::list` node allocation from the reference design while retaining readable ordered `std::map` price levels.

## 4. Compiler pipeline

`itch50.efe` -> Lexer -> Parser/AST -> semantic validation -> generated C++ views.

Semantic checks currently include duplicate message IDs, duplicate names, duplicate fields, unknown types, invalid ASCII widths, and one-byte message-ID validation.

Generated views validate message type/size once at construction. Accessors use fixed byte offsets and the shared big-endian byte reader.

## 5. Benchmark methodology

The included benchmark is a first laboratory, not a publication-quality result. A serious report should additionally:

- pin the benchmark process to one physical core
- disable/take account of frequency scaling and turbo behavior
- warm caches separately from cold-cache experiments
- run repeated trials and confidence intervals
- measure decoder-only and end-to-end paths separately
- use `perf stat` / `perf record`
- compare allocations, cache misses, branch misses, instructions, cycles
- report compiler version, flags, CPU model, OS/kernel, and workload distribution

Never optimize from intuition alone; preserve the differential tests and state hash while changing one variable at a time.
