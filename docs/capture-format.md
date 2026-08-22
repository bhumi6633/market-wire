# EFC2 capture format

All integers are unsigned big-endian. A file begins with the four bytes `EFC2`, followed by records in processing order:

| Field | Width |
|---|---:|
| Monotonic timestamp in nanoseconds | 8 bytes |
| Datagram length | 4 bytes |
| Datagram payload | declared length |

Datagrams are bounded to 65,535 bytes. Readers reject bad magic, oversized declarations, truncated timestamps, lengths, or payloads. Replay rejects decreasing timestamps before calculating a delay, preventing unsigned underflow. Capture preserves entire MoldUDP64 datagrams, including their session and sequence metadata.

## Record layout

```text
offset  width  meaning
0       4      ASCII magic: EFC2

Repeated until EOF:
0       8      monotonic timestamp, nanoseconds, big-endian
8       4      datagram length N, big-endian
12      N      complete datagram bytes
```

EFC2 stores complete datagrams instead of decoded events. That choice makes replay travel through the same MoldUDP64 framing, recovery, ITCH decoding, and book logic as live traffic.

## Replay modes

| Mode | Timing behavior | Typical use |
|---|---|---|
| Maximum speed | No inter-record delay | Tests and throughput experiments |
| Realtime | Original timestamp deltas | Reproducing temporal behavior |
| Scaled | Timestamp deltas divided by a positive factor | Faster or slower investigation |
| Step | Wait for user input between records | Interactive debugging |

Timestamps are offsets from a monotonic capture epoch, not wall-clock exchange timestamps. They preserve pacing without embedding local calendar time.

## What the format does not detect

EFC2 checks structure and record lengths, but it does not include a checksum or cryptographic integrity field. If damaged data still looks like a valid record, the parser may not notice until MoldUDP64 framing or ITCH decoding. This is a known limitation of the research format.
