# 0014 Outputs choose an encoding; every encoder lives in core and framing options in io

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

Until milestone M4 every output carried the same NMEA 0183 sentences, differing only by
transport and filter. M4 adds Signal K delta messages, which are JSON documents built from
the vessel state rather than sentences, ViewSync packets for Google Earth, IEC 61162-450 TAG
blocks in front of sentences, and sentences typed in by the operator. How are these fitted
into the engine and the runner so that one profile can feed a chart plotter with NMEA 0183, a
Signal K server with deltas and Google Earth with ViewSync at the same time?

## Decision drivers

- ADR 0002: encoders belong to the Qt-free `core`; the runner in `io` only moves bytes.
- One run must serve several encodings at once, each on its own transport with its own
  filter and period, as the reference application could not.
- Signal K and ViewSync describe the state, not sentences; they must not depend on which
  NMEA sentences are enabled, and they must work during a track and a delta run alike.
- The WebSocket greeting already exists; Signal K needs it for the hello message.
- Custom sentences must be scheduled, filtered and recorded like registry sentences.
- No JSON library in `core` for two fixed-shape documents.

## Considered options

1. Make every encoder produce `EmittedSentence`s through the registry, with Signal K and
   ViewSync as pseudo-sentences.
2. Keep the registry for NMEA 0183 sentences (registry and custom), and add state encoders
   in `core` (`signalk::encode_delta`, `viewsync::encode_packet`) that the runner calls per
   output on that output's period. The output's `encoding` selects which; TAG blocks are a
   per-output framing option applied by the runner with `core`'s formatter.
3. Convert the emitted NMEA sentences into Signal K in `io` with a sentence-to-path parser.

## Decision outcome

Option 2.

- `OutputConfig::Encoding` gains `SignalK` and `ViewSync`. An NMEA 0183 output receives the
  sentences the simulation emits; a Signal K or ViewSync output ignores them and instead
  receives one message per its own `period_ms` built from the current state after every
  tick. The filter of a Signal K output matches path prefixes instead of registry ids.
- The state encoders live in `core::signalk` and `core::viewsync` and know nothing about
  transports. The Signal K encoder renders JSON by hand: the documents have a fixed shape,
  and a JSON library in `core` would be a dependency for two functions.
- The Signal K hello message is the WebSocket greeting of a Signal K output, set by the
  runner when the output opens.
- TAG blocks are formatted by `core::nmea0183::format_tag_block` and prepended by the runner
  to every line of an NMEA 0183 output that enables them, with the simulated clock as the
  `c:` time; recordings keep the plain sentences.
- Custom sentences are part of the `SentenceScheduler`: the profile lists them, the scheduler
  frames each with its checksum, gives it an id and a period, and emits it after the registry
  sentences, so filters, the console and the recorder see no difference.

Option 1 was rejected because a delta is not a sentence: it has no talker, no 82-byte limit
and its own filter semantics, and forcing it through the registry would leak into every
per-sentence table. Option 3 was rejected because it would tie the Signal K output to the
enabled NMEA sentences and lose precision through two conversions.

### Consequences

- A profile can mix encodings freely; the CLI selects the encoding of its command-line
  outputs with `--encoding`, and the settings dialog per output.
- Replay mode carries no state for Signal K beyond what the decoder rebuilds, so a Signal K
  output during a replay reflects the decoded sentences only.
- ViewSync needs a per-output packet counter, kept by the runner's channel.
- New state encoders (NMEA 2000 later) follow the same pattern: a function in `core`, an
  `Encoding` value, and a branch in the runner.

## More information

- [Signal K reference](../reference/signalk.md)
- [ViewSync reference](../reference/viewsync.md)
- [TAG blocks and custom sentences](../reference/nmea0183-sentences.md#tag-blocks)
- [ADR 0002 Layered libraries](0002-layered-libraries.md)
