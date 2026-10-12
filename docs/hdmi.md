# HDMI input reference

## Commands

| Command / bytes | Description | Confirmation / evidence |
|---|---|---|
| `W[9C] 51=45` | Select HDMI mux path. | Observed as initial mux selection in both analog bring-up traces. |
| `W[9C] 00=CE`, `01=AD`, `04=CD`, `06=C4`, `09=E4` | HDMI front-end programming; `00` selects page `CE`. | Exact order/intervening reads are in raw trace; meanings not all known. |
| `W[9C] 00=CC`, `54=EC`, `AC=4C`, `00=4C`, `00=CC`, `CE=4C`, `CF=CE`, `00=CC` | Further bank/register setup. | Preserve raw trace order. |
| `B0=E8`; writes to `B1`, `92–A9), `AD`; `B0=E9` | Parameter-table window load/commit. | Exact bytes/order in raw trace; not validated for all formats. |
| Reads involving `66/65`, `64/63`, `34` | Legacy signal/timing detection. | Threshold interpretations are heuristic, not a verified mode table. |

## Ordered configuration

1. Select HDMI: `W[9C] 51=45`.
2. Read `9D CD 88`; write `9C B7=CC`; wait ~330 ms in captured trace.
3. Set routing state `02 0A`.
4. Apply front-end bank writes and parameter-table load/commit in exact order shown in [raw composite bring-up trace](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt), phases P4–P6. This is the trace's HDMI prefix before switching to analog.
5. Complete HDMI source-format setup and lock confirmation: **NOT CAPTURED** as a complete sequence.
6. HDMI device configuration before stream start and HDMI-specific start confirmation: **NOT CAPTURED**. Shared captured analog start sequence is documented in [hardware.md](hardware.md).

## Format coverage

Complete HDMI sequences for 240p, 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i and 1080p: **NOT CAPTURED**.
