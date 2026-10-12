# Component input reference

## Known commands

| Order | Target / bytes | Description | Confirmation / status |
|---:|---|---|---|
| 1 | `0x9C:51=ED` (mailbox selector `51 ED` on port `0x4E`) | Component selector recorded in command inventory. | Complete successful packet-level trace not captured. |
| 2 | State word `0900:0018 = 02 0E` | Component route/state. | Expected state word where this path uses state-word operation; exact component trace not captured. |
| 3 | `0x88:06=86`, `0x88:03=2F`, `0x9C:B3=CC`, `0x9C:27=CC` | Digital/component-side prefix reported in switch analysis. | Observed in switch analysis; full ordering with all reads/handshakes needs a successful component capture. |
| 4 | Shared HDMI/component prefix: `0x9C:AB=4C`, `0x9C:1B=33`, `0x9C:B7=CE`, `0x9C:B8=DC`, `0x9C:B8=CC`, `0x9C:07=38`, `0x9C:07=C8` | Common path writes before selector in one reported sequence. | Preserve order when reproducing; register meanings unknown. |
| 5 | `24 0C`, `21 CD`, `91 C8`, `0E 8C`, `11 EC`, `54 FC`, `71 4C`, `04 CC` | Component-specific tail recorded in input-switch inventory. | Exact full transaction context and confirmation not captured. |

## Ordered component selection

1. Run common device initialization in [hardware.md](hardware.md).
2. Apply the digital/component path prefix in the exact order above, including surrounding bank/page writes and mailbox handshake; the complete packet-level order is **NOT CAPTURED**.
3. Write selector `0x9C:51=ED); then write/read routing state `02 0E) where applicable.
4. Apply component-specific tail writes; exact full order and required waits are **NOT CAPTURED**.
5. Component signal detection, per-format decoder configuration, final transcoder setup, and lock criteria: **NOT CAPTURED**.
6. Use the shared SCMD/capture confirmation sequence in [hardware.md](hardware.md) only after component-specific setup is validated.

## Format coverage

Complete component sequences for 240p, 288p, 480i, 576i, 480p, 576p, 720i, 720p, 1080i, and 1080p: **NOT CAPTURED**. Do not copy HDMI mode detection or composite decoder values without evidence.
