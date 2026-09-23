# 0013 AIS own-vessel messages encoded in core with an independent decoder in the tests

- Status: accepted
- Date: 2026-09-23

## Context and problem statement

Milestone M4 adds the own vessel as an AIS station: VDO sentences, and VDM sentences for
consumers that ignore VDO, carrying a class A position report and the static data report.
AIS messages are bit-packed and armored into six-bit ASCII (ITU-R M.1371 and IEC 61162-1),
which is a different kind of encoding from the comma-separated sentences the registry has
handled so far. Where does the bit packing live, how are multi-sentence messages framed, and
how is correctness assured when no third-party AIS encoder is linked?

## Decision drivers

- ADR 0002: encoders live in the Qt-free `core`.
- ADR 0008: every sentence needs a golden-file test and an independent cross-check; a
  hand-rolled bit packer checked only against itself proves nothing.
- The registry, the scheduler, the per-sentence settings and the output filters must work
  for AIS entries exactly as for the parametric sentences.
- Every sentence must stay within 82 bytes, so a 424-bit message must be split.
- The reference application sends VDO and VDM; users asked to disable them independently.

## Considered options

1. Link a third-party AIS library.
2. A small `core::ais` module: a `BitPacker`, the two message packers, six-bit armoring and
   fragment framing, with four registry entries (`VDO-POS`, `VDO-STATIC`, `VDM-POS`,
   `VDM-STATIC`) whose encoders call it. Correctness checked by a test-only bit reader plus
   the third-party `pyais` decoder in CI.
3. Encode AIS in `io` on top of Qt's byte arrays.

## Decision outcome

Option 2. The packer appends unsigned, two's-complement and six-bit text fields most
significant bit first; the armoring maps every six bits to one payload character and reports
the fill bits; framing splits the payload into sentences of at most 60 payload characters and
numbers them, with the sequential message id derived from the simulated clock so that the
fragments of one message share it and consecutive messages differ without any encoder state.
The AIS static data (MMSI, name, call sign, IMO number, ship type, dimensions, draught,
destination, navigational status, position report type) is a struct on the vessel state, so
every source carries it and the profile persists it like any other seed value.

Tests read every golden payload back with an independent bit reader written for the tests,
field by field, and CI decodes the stream and the fixture file with `pyais`, which is
developed independently of this project.

Option 1 was rejected because the two messages needed are small, existing C++ AIS libraries
are either decoders or carry licences and dependencies out of proportion to the need, and
the bit packer is reusable for AIS targets after 1.0. Option 3 was rejected by ADR 0002.

### Consequences

- The registry gains a group `AIS` and encapsulated (`!`) sentences; the decoder recognises
  them as such and leaves the state alone during a replay.
- The rate of turn, speed, position and course use the "not available" codes when the
  simulated receiver has no fix, as a real transponder would.
- Message types 1, 2 and 3 share one packer and differ only in the type field; the type is
  a profile option.
- Encoders must not keep state: the sequential message id is derived from the clock rather
  than counted.

## More information

- [AIS reference](../reference/ais.md)
- [ADR 0008 Testing strategy](0008-testing-strategy.md)
