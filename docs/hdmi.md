# HDMI input protocol

## Scope

HDMI-specific input selection, signal/timing detection, mode setup, and the transition into shared capture-engine configuration.

## Known input selection

The legacy command reference records mailbox port `0x4E` command `51 45`, observed as a register-style write `0x9C:0x51 = 0x45`, as the HDMI selector. The selector occurs within a larger ordered setup sequence; do not isolate it from required preceding/following commands.

## Detection and timing

The legacy command reference records reads involving registers `0x66/0x65`, `0x64/0x63`, and `0x34`. It describes timing thresholds as heuristics and uses register `0x34` to infer an RGB flag. These interpretations are not equivalent to a complete, verified timing table.

When documenting a new HDMI source-format trace, record:
- Exact source format and signal generator/settings.
- Raw timing/status register values before and after lock.
- Color-space/pixel-format indicators and how they were interpreted.
- Every format-dependent write, in original order.
- Readback/status condition that indicates lock and any timeout.
- Whether the detected format agrees with the source generator.

## Format coverage

See [source-format-matrix.md](source-format-matrix.md). No HDMI format should be marked confirmed solely because a timing threshold appears plausible; retain the raw measured values and trace reference.

## Sequence sources

- [Legacy command reference](../COMMAND_REFERENCE.md), input-selection and HDMI-detection sections.
- `drivers/elgato_gchd/hdmi2composite_trace_analysis.txt` and other trace analysis files, where applicable.
- Source implementation under `src/`.

## Open questions

- Confirm the complete HDMI selection sequence and all required acknowledgements from source/trace.
- Validate each timing threshold against known generator modes, including PAL-family and interlaced modes.
- Determine which decoder/transcoder writes are mode-specific versus shared.
