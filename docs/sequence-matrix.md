# Input × source-format sequence matrix

Status is for the complete **input switch → signal lock → device configuration → stream start** sequence.

| Source format | HDMI | Component | Composite | S-video |
|---|---|---|---|---|
| 240p NTSC-family | NOT CAPTURED | NOT CAPTURED | [TRACE](composite-240p-sequence.md) | [TRACE](s-video-240p-sequence.md) |
| 288p PAL-family | NOT CAPTURED | NOT CAPTURED | NOT CAPTURED | NOT CAPTURED |
| 480i NTSC-family | PARTIAL: timing heuristics | NOT CAPTURED | PARTIAL: detector analysis only | NOT CAPTURED |
| 576i PAL-family | NOT CAPTURED | NOT CAPTURED | PARTIAL: NTSC/PAL detector only | NOT CAPTURED |
| 480p | NOT CAPTURED | NOT CAPTURED | SUPPORT UNKNOWN | SUPPORT UNKNOWN |
| 576p | NOT CAPTURED | NOT CAPTURED | SUPPORT UNKNOWN | SUPPORT UNKNOWN |
| 720i | SUPPORT UNKNOWN | SUPPORT UNKNOWN | SUPPORT UNKNOWN | SUPPORT UNKNOWN |
| 720p | PARTIAL: timing heuristics | NOT CAPTURED | SUPPORT UNKNOWN | SUPPORT UNKNOWN |
| 1080i | PARTIAL: timing heuristics | NOT CAPTURED | SUPPORT UNKNOWN | SUPPORT UNKNOWN |
| 1080p | PARTIAL: timing heuristics | NOT CAPTURED | SUPPORT UNKNOWN | SUPPORT UNKNOWN |

## Required tables per cell

| Stage | Required record |
|---|---|
| Input switch | Exact ordered commands, register/page, USB transfer, handshake, expected readback |
| Signal lock | Poll/read sequence, exact lock condition, timeout |
| Pre-stream configuration | Every decoder/mailbox/transcoder write in order |
| Stream start | SCMD payload, completion/status polling, expected state |
| Stream proof | Bulk endpoint and evidence of valid video data |

## Known trace entry points

| Input/format | Switch | Configuration/start |
|---|---|---|
| Composite 240p | Raw trace P7 | Raw trace P8–P18 |
| S-video 240p | Raw trace P7 | Raw trace P8–P18 |

The raw trace begins by selecting HDMI; P1 is setup state, not the target analog input switch. The analog switch is P7. Full numbered command ordering and all polls/waits are in the linked raw traces.