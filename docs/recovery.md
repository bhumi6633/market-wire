# Sequencing and recovery

`RecoveryEngine` owns the rule that downstream consumers receive only contiguous message sequences. `expected_` identifies the next releasable message; a sorted pending map holds future messages.

For expected sequence 102, receiving 103–105 buffers those messages and requests 102. Receiving 106 while recovery is active also buffers it. When 102 arrives, the engine releases 102–106 in order. Duplicates below `expected_` are marked stale. Partial retransmission advances the expected pointer and causes the still-missing suffix to be requested.

Session changes and sequence arithmetic overflow are rejected. Recovery does not decode ITCH or mutate the book, so live and retransmitted packets follow the same contiguous output path.

The `pipeline_property` CTest case validates this boundary end to end: it shuffles 50,000 MoldUDP64 packets, injects duplicates, asserts every released sequence is the exact successor of the last, and compares the recovered generated-decoder/book hash with an ordered reference run.
