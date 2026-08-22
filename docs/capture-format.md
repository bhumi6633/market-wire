# EFC2 capture format

All integers are unsigned big-endian. A file begins with the four bytes `EFC2`, followed by records in processing order:

| Field | Width |
|---|---:|
| Monotonic timestamp in nanoseconds | 8 bytes |
| Datagram length | 4 bytes |
| Datagram payload | declared length |

Datagrams are bounded to 65,535 bytes. Readers reject bad magic, oversized declarations, truncated timestamps, lengths, or payloads. Replay rejects decreasing timestamps before calculating a delay, preventing unsigned underflow. Capture preserves entire MoldUDP64 datagrams, including their session and sequence metadata.
