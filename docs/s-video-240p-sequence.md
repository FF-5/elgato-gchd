# S-video / 240p — sequence reference

Evidence: [full ordered trace](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt), `filtered_240p_svideo.pcapng`. Phase labels are trace-author interpretations. Use the raw trace for all numbered operations, reads, polls, waits and exact payloads.

## 1. Input switch

| Order | Operation | Exact command/value | Expected response/state | Evidence |
|---:|---|---|---|---|
| 1 | Trace starting-state HDMI select (not S-video selection) | `W[9C] 51=45` | Handshake and subsequent routing setup | P1 |
| 2 | Set HDMI routing state | `STATE18 02 0A` | State readback | P3 |
| 3 | **Select S-video analog path** | `W[9C] 51=44` | Analog decoder wake/status sequence follows | P7 |
| 4 | Set analog routing state | `STATE18 03 0E` | `0900:0018` write/readback; `001C` values are session-dependent | P7 |

## 2. Device configuration before stream start

| Order | Stage | Exact sequence / check | Evidence |
|---:|---|---|---|
| 1 | Decoder wake/status prime | Preserve ordered commands, replies and handshakes | P8 |
| 2 | Timing/gain tuning | Exact ordered writes on page `0x88`; compare with composite | P9 |
| 3 | Auxiliary device programming | `0x94` mailbox commands and handshake behavior | P10 |
| 4 | Signal detection/retrigger | Exact status polls and lock transition | P11 |
| 5 | Lock verification | Exact readbacks and lock criteria | P13 |
| 6 | Back-end config | Page `0x98` coefficients/timing writes | P14 |
| 7 | Apply/retrigger | `0D/0E` pulses and settle interval | P15–P16 |
| 8 | Capture engine | Ordered SoC masked writes and bulk URB queueing | P17 |

## 3. Stream start

| Order | Command/check | Exact value | Expected response/state | Evidence |
|---:|---|---|---|---|
| 1 | SCMD INIT without encode firmware reload | `04 A0 00 00` | Follow trace's readiness/handshake checks | P18 |
| 2 | Device-ready read | `0800:2008 → 00 01` observed | Device reports ready | P18 |
| 3 | SCMD START | `05 00 00 02` | Confirm completion/state | P18 |
| 4 | Stream verification | Bulk IN on `0x81/0x84` | Verify valid video payload; trace slice lacks initial URB submissions | Post-start notes |

## 4. Differences from composite 240p

| Item | S-video | Composite |
|---|---|---|
| Input mux | `0x9C:0x51=44` | `0x9C:0x51=44` |
| Analog route | `03 0E` | `03 0E` |
| Decoder register `0x02` | `DC` | `C9` |
| S-video-specific write | `0x14=D2` | Not established as shared |
| Format apply page `0x88` | Includes `07=8A,08=9B,09=7A,28=8A,06=08` | Compare exact raw trace |
| Session values | `0900:0014=03 1E`; `001C=6E EB/6F EF` | `0014=D3 9E`; `001C=2E 6B/2F 6F` |
| Start | `04 A0 00 00` then `05 00 00 02` | Same observed sequence |

Session values are not constants. Exact-byte comparison requires comparing the complete raw traces, not only this summary.