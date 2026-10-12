# Composite / 240p — sequence reference

Evidence: [full ordered trace](../drivers/elgato_gchd/gchd_composite_240p_full_bringup.txt), `filtered_240p_composite.pcapng`. Phase labels are trace-author interpretations. Use the raw trace for every numbered operation, read, poll, wait and exact payload.

## 1. Input switch

| Order | Operation | Exact command/value | Expected response/state | Evidence |
|---:|---|---|---|---|
| 1 | Trace starting-state HDMI select (not composite selection) | `W[9C] 51=45` | Handshake and subsequent routing setup | P1 |
| 2 | Set HDMI routing state | `STATE18 02 0A` | State readback | P3 |
| 3 | **Select composite analog path** | `W[9C] 51=44` | Analog decoder wake/status sequence follows | P7 |
| 4 | Set analog routing state | `STATE18 03 0E` | `0900:0018` write/readback; `001C` transition observed | P7 |

The observed signal-present composite mux value is `51=44`. Do not substitute the trace's initial `51=45` HDMI selection. No-signal switching requires separate evidence.

## 2. Device configuration before stream start

| Order | Stage | Exact sequence / check | Evidence |
|---:|---|---|---|
| 1 | Decoder wake/status prime | Use all ordered commands and replies as recorded; do not omit handshake steps | P8 |
| 2 | Timing/gain tuning | Three passes on page `0x88`; exact writes are numbered in raw trace | P9 |
| 3 | Auxiliary device programming | Three `0x94` mailbox commands; commit/handshake, no fetch | P10 |
| 4 | Signal detection/retrigger | Poll status; observed `CA: E6 → 66` | P11 |
| 5 | Lock verification | Observed `CA=66, E7=69, F1=7E, CB=6E` | P13 |
| 6 | Back-end config | Page `0x98` coefficients and timing writes | P14 |
| 7 | Apply/retrigger | `0D/0E` pulses and settle interval | P15–P16 |
| 8 | Capture engine | Ordered SoC masked writes and bulk URB queueing | P17 |

## 3. Stream start

| Order | Command/check | Exact value | Expected response/state | Evidence |
|---:|---|---|---|---|
| 1 | SCMD INIT without encode firmware reload | `04 A0 00 00` | Follow trace's readiness/handshake checks | P18 |
| 2 | Device-ready read | `0800:2008 → 00 01` observed in trace | Device reports ready | P18 |
| 3 | SCMD START | `05 00 00 02` | Confirm completion/state; don't treat USB submission as success | P18 |
| 4 | Stream verification | Bulk IN on `0x81/0x84` | Confirm valid video payload after start | Post-start trace |

## 4. Driver assertions

| Item | Expected |
|---|---|
| Composite mux | `0x9C:0x51 = 0x44` for this signal-present trace |
| Analog route state | `03 0E` |
| Decoder register `0x02` | `C9` in this composite trace |
| Source-mode detector | `89 89 FA` distinguishes NTSC/PAL family in userspace, not proven 240p vs 480i |
| Capture dimensions | 720×480 output does not prove 480i input |
| Session-dependent handshakes | Do not hardcode observed per-session values |

**Exact-byte rule:** this summary table intentionally links to the full numbered trace for the long register sequence. A driver comparison must compare the entire raw ordered sequence, not just these phase checkpoints.