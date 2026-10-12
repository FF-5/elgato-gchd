# Component input reference

## Commands

| Command / bytes | Description | Confirmation / evidence |
|---|---|---|
| `W[9C] 51=ED` | Component selector recorded in legacy command reference. | Successful full packet-level component trace: **NOT CAPTURED**. |
| `STATE18 02 0E` | Routing-state word associated with component. | Shared operation and readback in [hardware.md](hardware.md); component trace not captured. |
| `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `71 4C`, `04 CC` | Component-specific write group noted in legacy reference. | Preserve this order if implementing that recorded path; semantics and full context require trace verification. |

## Ordered configuration

1. Common USB/hardware setup: [hardware.md](hardware.md).
2. Select component: `W[9C] 51=ED`; expected routing state where used: `02 0E`.
3. Component front-end/decoder setup: **NOT CAPTURED** as a complete ordered sequence.
4. Signal detection and lock confirmation: **NOT CAPTURED**.
5. Device configuration before stream start: **NOT CAPTURED**.
6. Component-specific stream start and confirmation: **NOT CAPTURED**. Do not assume composite/S-video setup applies unchanged.

## Format coverage

Component × 240p, 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i, 1080p: **NOT CAPTURED** as complete ordered command sequences.
