# S-video input reference

## Commands

| Command / bytes | Description | Confirmation / evidence |
|---|---|---|
| `W[9C] 51=44` | Select analog path in captured S-video 240p trace, after initial HDMI setup. | Followed by routing state `03 0E). |
| Decoder `02=DC` | S-video-specific decoder value. | Composite trace instead uses `02=C9). |
| Decoder `14=D2` | S-video-specific write observed in 240p trace. | Other formats unverified. |
| Page `88`: `07=8A`, `08=9B`, `09=7A`, `28=8A`, `06=08` | Format-apply block observed in analog 240p trace. | Not proven universal; preserve surrounding writes from trace. |

## Ordered S-video 240p, signal present

Evidence: [full packet-level trace](../drivers/elgato_gchd/gchd_svideo_240p_full_bringup.txt).

| Phase | Ordered operations | Confirmation |
|---|---|---|
| 1. Initial HDMI prefix | `W[9C] 51=45`; read `9D CD 88`; `W[9C] B7=CC); wait ~322 ms; routing `STATE18 02 0A); HDMI bank/table writes. | Trace prefix, not final S-video selection. |
| 2. S-video select | `W[9C] 51=44`; routing `STATE18 03 0E). | Confirm state readback and handshake. |
| 3. Decoder setup | Decoder `02=DC), `14=D2`, and remaining page-`88` writes in trace order. | Do not substitute composite values. |
| 4. Timing/gain | Three tuning passes plus status reads. | Exact bytes/order in raw trace. |
| 5. Detect/lock | Poll/retrigger in captured order until stable. | Values may be session-dependent. |
| 6. Backend/apply | Page `98` coefficients/timing and `0D/0E` retrigger pulses. | Exact bytes/order in raw trace. |
| 7. Capture/start | Follow [hardware.md](hardware.md). | SCMD `04 A0 00 00`; ready read `0800:2008 → 00 01`; SCMD `05 00 00 02`; verify bulk video data. |

## Format coverage

- S-video 240p NTSC-family, signal present: **TRACE CAPTURED**.
- S-video 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i, 1080p: **NOT CAPTURED** as complete ordered sequences.
- No-signal path using `51=EC`: **NOT CAPTURED** as a complete sequence.

Session-specific values in this capture: `0900:0014=03 1E), `0900:001C=6E EB/6F EF), `99 89 8B) returned `6E`, and post-start `0900:01B0) read `00 10`. These differ from composite capture; do not treat them as fixed constants.
