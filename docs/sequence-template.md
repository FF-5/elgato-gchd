# Source-format sequence template

Copy this template for each input/source-format combination, for example `composite-240p.md`.

## Metadata

- Input:
- Source format:
- Standard/family (NTSC/PAL/other):
- Refresh rate:
- Scan mode:
- Source generator and settings:
- Device generation / firmware:
- Trace filename:
- Date/session:
- Evidence status: Needed / Partial / Trace available / Validated

## Preconditions

- Device state:
- Required initialization:
- Current input and signal state:
- Expected initial status:

## 1. Input selection

For every transfer, record:
- Time/packet range:
- USB transfer type, endpoint, request type, request, value, index, length:
- Exact payload:
- Expected reply/acknowledgement:
- Retry/timeout behavior:
- Interpretation and confidence:

## 2. Signal detection and lock

- Poll/read sequence and order:
- Exact response values before lock:
- Exact response values after lock:
- Lock condition:
- Timeout/retry behavior:
- Negative/no-signal behavior:

## 3. Format-specific setup

List every write/read in order. Include register page/bank selection and any delays. Separate confirmed format-dependent values from values that merely differed in one session.

| Order | Transfer / command | Exact bytes | Expected response | Purpose | Evidence |
|---:|---|---|---|---|---|
| 1 | TBD | TBD | TBD | Unknown | TBD |

## 4. Shared capture-engine setup

- Link to [capture-engine.md](capture-engine.md) if identical.
- List all differences if not identical.
- Record SCMD receipt, completion and state confirmation separately.
- Confirm that valid bulk video data arrives.

## 5. Comparison

- Same input, nearest known format:
- Same format, nearest known input:
- Shared sequence candidates:
- Differences requiring further tests:

## 6. Evidence and confidence

- **Observed:** directly present in source or trace.
- **Repeated:** reproduced across independent traces.
- **Inferred:** plausible interpretation not yet proved.
- **Unknown:** meaning not established.
- **Session-dependent:** changes across sessions/devices; do not treat as a protocol constant.

## 7. Driver implementation notes

- Safe to implement now:
- Requires more evidence:
- Required error handling:
- Expected behavior with no signal:
