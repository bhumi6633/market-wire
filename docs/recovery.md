# Sequencing and recovery

`RecoveryEngine` owns the rule that downstream consumers receive only contiguous message sequences. `expected_` identifies the next releasable message; a sorted pending map holds future messages.

If the engine expects sequence 102 and receives 103, 104, and 105, it buffers them and requests 102. If 106 arrives while recovery is active, that message is buffered too. When 102 arrives, the engine releases everything from 102 through 106 in order. Messages below `expected_` are stale duplicates. A partial retransmission moves the expected sequence forward and triggers a new request for whatever is still missing.

Session changes and sequence arithmetic overflow are rejected. Recovery does not decode ITCH or mutate the book, so live and retransmitted packets follow the same contiguous output path.

The `pipeline_property` CTest case validates this boundary end to end: it shuffles 50,000 MoldUDP64 packets, injects duplicates, asserts every released sequence is the exact successor of the last, and compares the recovered generated-decoder/book hash with an ordered reference run.

## State transitions

| Input relative to `expected_` | Action |
|---|---|
| Below expected | Mark stale and do not deliver |
| Exactly expected | Deliver, advance, then drain any contiguous pending suffix |
| Above expected | Buffer and report the first missing range |
| Heartbeat | Update transport state without creating a book event |
| End of session | Surface session completion without bypassing ordering |

The pending container is keyed by sequence, so duplicate future messages do not create duplicate downstream delivery. A recovery request is always derived from the current first hole rather than from packet arrival order.

## Worked example

```text
expected = 102
receive 104, 105  -> buffer both; request [102, 103]
receive 102       -> deliver 102; request [103, 103]
receive 105       -> duplicate future input; no delivery
receive 103       -> deliver 103, then drain 104 and 105
expected = 106
```

The book never needs to distinguish multicast traffic from retransmitted traffic. Both become ordinary sequenced messages only after the recovery boundary accepts them.

## Failure behavior

Session changes are rejected because sequence numbers from different sessions cannot safely share one state machine. Sequence overflow is rejected rather than wrapped. Malformed MoldUDP64 packets fail during framing and never enter recovery.

Transport layouts and rerequest encoding should remain consistent with the [official Nasdaq MoldUDP64 specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/moldudp64.pdf).
