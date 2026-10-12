# Input × source-format matrix

This matrix tracks the evidence status of each input/source-format combination. “Trace available” means a relevant trace exists; it does not automatically mean the full sequence has been validated or is implemented in the driver.

## Status definitions

- **Trace available** — relevant capture/analysis exists; sequence still needs review.
- **Partial evidence** — some related commands are known, but no complete verified bring-up.
- **Needed** — capture and document the sequence.
- **Verify support** — do not assume the hardware accepts this mode on this input.
- **Validated** — reserve for repeatable captures with successful lock and capture-data confirmation.

| Source format | HDMI | Component | Composite | S-video |
|---|---|---|---|---|
| 240p NTSC-family | Partial evidence | Needed | Trace available | Trace available |
| 288p PAL-family | Needed | Needed | Needed | Needed |
| 480i NTSC-family | Partial evidence; validate | Needed | Partial evidence | Needed |
| 576i PAL-family | Needed | Needed | Partial evidence for NTSC/PAL detector only | Needed |
| 480p | Needed | Needed | Verify support | Verify support |
| 576p | Needed | Needed | Verify support | Verify support |
| 720i | Verify support | Verify support | Verify support | Verify support |
| 720p | Partial evidence in legacy timing heuristics; validate | Needed | Verify support | Verify support |
| 1080i | Partial evidence in legacy timing heuristics; validate | Needed | Verify support | Verify support |
| 1080p | Partial evidence in legacy timing heuristics; validate | Needed | Verify support | Verify support |

## Required trace set for every cell

For each supported combination, capture:
1. Clean startup from a known device state.
2. Selection from another input, with signal present.
3. Selection with no signal, followed by signal insertion.
4. Stable lock and capture start.
5. Signal removal and re-acquisition.
6. A second independent run to identify session-dependent values.

Each trace record should include source generator/model, exact mode, refresh rate, scan mode, color space where relevant, device generation, firmware state, and trace filename.

## Important distinctions

- 240p and 480i can both produce a 720×480 capture stream; output dimensions alone are not a reliable source-mode detector.
- Composite NTSC/PAL detection is not by itself a 240p/480i detector.
- Keep PAL-family modes explicit: 576i is not a synonym for all PAL modes; 288p and 576p should be separate entries if tested.
- Keep 720i in the matrix as requested, but verify that the source/device combination supports it before treating it as a required implementation target.
