# S-video input protocol

## Scope

S-video mux selection, decoder-path programming, signal lock, format-specific configuration, and shared capture startup.

## Observed 240p bring-up

Trace: [`gchd_svideo_240p_full_bringup.txt`](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt).

The trace describes an initial HDMI selection followed by an analog selection write `0x9C:0x51 = 0x44` for the S-video bring-up. Preserve the sequence and surrounding routing writes; do not infer that a single register write is a complete input-switch command.

## Decoder-specific observations

The trace analysis identifies:
- Decoder register `0x02 = 0xDC` on the S-video path, contrasted with `0x02 = 0xC9` in the composite trace.
- An S-video-specific write `0x14 = 0xD2`.
- A format-apply block with writes on page `0x88`, including `0x07 = 0x8A`, `0x08 = 0x9B`, `0x09 = 0x7A`, `0x28 = 0x8A`, and `0x06 = 0x08`.

These are trace observations, not a universal table for all S-video formats. Re-check the complete trace before copying any block into a driver sequence.

## Lock and capture

Record decoder status polls and the values that changed when lock was obtained. Then distinguish decoder lock from SoC/capture-engine initialization and the final SCMD start state. Session-dependent values documented in the trace must remain tagged as such.

## Format coverage

See [source-format-matrix.md](source-format-matrix.md). A successful 240p trace is available; other source formats still require their own evidence.
