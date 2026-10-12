# Hardware-wide protocol

## USB command primitives

| Operation | USB setup / payload | Meaning / expected result |
|---|---|---|
| HD-New register write `W[ADDR] rr=vv` | Vendor OUT `40 BC`, `wValue=0800`, `wIndex=00C0`, length 2, data `rr vv`; then vendor OUT `40 B9`, `wValue=0000`, `wIndex=0000`, length 4, data `09 00 ADDR 02`; wait for `HS` | Write register `rr` on address/page `ADDR` (`9C`, `88`, `98` observed). |
| HD-New register read `READ aa bb rr` | OUT `40 BC`, `wValue=0800`, `wIndex=00C0`, length 4, data `aa bb rr 00`; OUT `40 B9` data `09 00 66 03`; wait `HS`; OUT `40 B9` data `09 01 66 01`; wait `HSR` | Response observed as `00 00 VV 59`; `VV` is the register value. Examples: `9D CD rr` reads page `9C`, `89 89 rr` reads page `88`, `99 89 rr` reads page `98`. |
| Auxiliary command `CMD aa bb cc` | Same setup/commit as read, but only `09 00 66 03` + `HS`; no `09 01 66 01` fetch | Used for device `0x94) mailbox commands in trace. |
| Synchronization `HS` | INT IN endpoint `0x83` → `10 00 00`; IN `0800:0016` → `09 00`; IN `0900:001C`; IN `0800:23BE` length 2 → `00 00`; OUT `0900:0014` session value; read `0900:001C` again | Preserve handshake order. `0900:0014` and `001C` values vary by session/device. |
| Synchronization/read `HSR` | INT IN `0x83` → `10 00 00`; IN `0800:0016` → `09 00`; IN `0900:001C`; IN `0800:23BE` length 4 → `00 00 VV 59` | Read response path. Low bit 0 of `0900:001C` was polled until set in one trace; treat as observed readiness behavior. |
| Routing state `STATE18 b0 b1` | OUT `0900:0014` session value; OUT `0900:0018` data `b0 b1`; read back `0014` and `0018` | Observed state words: `02 0A` HDMI; `02 0E` component; `03 0E` composite/S-video. |
| SoC write `SOC addr value mask` | OUT `40 BC`, `wValue=0001`, `wIndex=addr`, length 8, big-endian 32-bit value + 32-bit mask | Capture-engine configuration. |
| SCMD | Vendor OUT request `0xB8`, data length 4 in captured HD-New trace | Captured start commands: `04 A0 00 00`, then `05 00 00 02`. Do not generalize length to other generations. |

## Input selectors observed

| Input | Register write | Notes |
|---|---|---|
| HDMI | `W[9C] 51=45` | Observed as initial HDMI mux selection in analog bring-up traces. |
| Component | `W[9C] 51=ED` | Legacy reference; successful per-format trace **NOT CAPTURED**. |
| Composite | `W[9C] 51=44` | Observed with signal present. No-signal switching reportedly used `51=EC`; `44` is not established as universal. |
| S-video | `W[9C] 51=44` | Observed in 240p trace; decoder/path setup differs from composite. |

## Shared capture start: captured analog 240p traces

| Order | Command / operation | Expected confirmation |
|---:|---|---|
| 1 | Apply the ordered SoC register burst from the input trace; queue bulk IN URBs on `0x81` and `0x84` | Queueing occurs before start and does not prove capture is active. |
| 2 | SCMD `04 A0 00 00` | Continue with ready/readback path. |
| 3 | Read `0800:2008` | Captured ready value: `00 01`. |
| 4 | SCMD `05 00 00 02` | Verify state/completion path and actual bulk video payload. |
| 5 | Bulk IN completions on `0x81` / `0x84` | Captured transfers were 65536 bytes; real video data is the practical success check. |

USB completion, interrupt handshake, state transition, decoder lock, and successful video capture are separate checks.

## Raw evidence

- [Composite 240p full bring-up trace](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt)
- [S-video 240p full bring-up trace](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt)
- [Legacy command reference](../COMMAND_REFERENCE.md)
