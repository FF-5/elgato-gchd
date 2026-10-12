# Composite input protocol

## Scope

Composite mux selection, decoder setup, NTSC/PAL detection, format/scan-mode handling, and handoff to shared capture configuration.

## Input selection

Selection must be documented in context with the surrounding setup and its confirmation. The recent 240p bring-up analysis reports the analog selection write `0x9C:0x51 = 0x44`; earlier no-signal observations reportedly differed. Treat `0x44` as observed for the signal-present sequence, not yet as a universal selector for every device state. Compare with the current driver implementation before changing behavior.

## Detection: NTSC/PAL is not 240p/480i

The existing userspace sequence sends mailbox command `89 89 FA` and examines the low nibble:
- `0x06` → NTSC / 60 Hz family
- `0x07` → PAL / 50 Hz family

The current code initializes scan mode to interlaced. This command alone is not evidence of a 240p-versus-480i detector. See [composite-240p-480i-trace.md](composite-240p-480i-trace.md) for the existing trace analysis.

## Observed 240p bring-up

Trace: [`gchd_composite_240p_full_bringup.txt`](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt).

The matching S-video trace is [`gchd_svideo_240p_full_bringup.txt`](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt).

The current analysis identifies a common 240p bring-up structure with input-specific decoder writes. Record these as separate stages:
1. Initial selection/routing.
2. Composite decoder path setup.
3. Signal detection and lock polling.
4. Format-apply register writes and readback.
5. Shared SoC/capture configuration.
6. SCMD initialization and capture start.

## Sequence-specific cautions

- Preserve the exact packet order and USB transfer context from the trace.
- A response value observed in one session may be session-dependent.
- A transport stream reporting 720×480 does not establish that the input signal is 480i; the capture engine's output format may differ from source timing.
- Do not add a hard-coded 240p signature until a stable, repeatable signal/scan-mode indicator is established.

## Format coverage

See [source-format-matrix.md](source-format-matrix.md). The available traces provide evidence for composite 240p bring-up and separate composite 240p/480i investigation, but do not yet establish a complete per-format table.
