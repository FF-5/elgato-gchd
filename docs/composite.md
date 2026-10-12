# Composite input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:06` | Write `08` | Analog decoder mode. |
| `0x88:03` | Write `28` | Analog decoder setup. |
| `0x9C:B3` | Write `33` | Analog front-end setup; meaning partly unknown. |
| `0x9C:00`, `0x9C:27` | `CE`, `33`, `CC` | Bank/page setup; preserve order. |
| `0x9C:51` | Write `44` | Composite selector in the working signal-present path. |
| `0x9C:51` | Write `EC` | Also seen during no-source switching; context-specific alternative. |
| `0x88:02` | Write `C9` | Composite discriminator; S-video uses `DC`. |
| Mailbox read `89 89 FA` | Expect low nibble `6` (NTSC/60 Hz) or `7` (PAL/50 Hz) | Detects family, not progressive/interlaced format. |
| Mailbox read `89 89 FD` | Working NTSC-family path: `6E` | Status poll; meaning unknown. |
| Mailbox read `89 89 F1` | PAL branch expects `63` | PAL status poll; exact semantics unknown. |
| `0x88:10–15` | `6F D4 D0 08 08 88` | Decoder tuning pass 1. |
| `0x88:10–15` | `6F EA D0 08 08 88` | Decoder tuning pass 2. |
| `0x88:10–15` | `6F EA D0 1B 1B 88` | Decoder tuning pass 3. |
| `0x98:0D`, `0x98:0E` | `C8`, `15`, `14` | Backend retrigger pulse; preserve order. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04` | Apply pulse after backend programming. |
| `0x98` backend | See ordered sequence and coefficient table | Register meanings are not fully decoded. |



## Backend coefficient table

| Target | Value | Target | Value |
|---|---:|---|---:|
| `0x98:90` | `01` | `0x98:9A` | `9C` |
| `0x98:91` | `BD` | `0x98:9B` | `8C` |
| `0x98:92` | `9A` | `0x98:9C` | `98` |
| `0x98:93` | `3A` | `0x98:9D` | `93` |
| `0x98:94` | `E9` | `0x98:9E` | `83` |
| `0x98:95` | `AB` | `0x98:9F` | `A9` |
| `0x98:96` | `17` | `0x98:A0` | `8B` |
| `0x98:97` | `88` | `0x98:A1` | `E8` |
| `0x98:98` | `84` | `0x98:A2` | `81` |
| `0x98:99` | `8A` | `0x98:A3` | `49` |


## Sequences

### 240p / 288p

#### NTSC-family 240p — working full bring-up

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Start from HDMI selector `0x9C:51=45`; switch to composite `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` | Full working bring-up in the DKMS driver trace. |
| 2 | Decoder `0x88:02=C9`; `0x88:06=08`; `0x88:03=28`; front end `0x9C:B3=33`, page `CE`, `0x9C:27=33`, page `CC` | Preserve bank/page and handshake order. |
| 3 | Three tuning passes to `0x88:10–15`: `6F D4 D0 08 08 88`; `6F EA D0 08 08 88`; `6F EA D0 1B 1B 88` | Three distinct passes. |
| 4 | Auxiliary device `0x94`: `41 37`, `4A B7`, `4B B7`; backend `04=95`, `70=D0`, `0F=88`, coefficient table above, then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | Ordered auxiliary/backend configuration. |
| 5 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`, later `15 → 14`; pipeline setup; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | Working capture reaches START. |


#### PAL-family 288p-equivalent — working, frames received

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` | Decoder setup; observed replies `FD=60`, `F8=C9`. |
| 2 | Write `0x9C:B3=33`; read `9D CD 3F`; write `0x9C:00=CE`; write `0x9C:27=33`; read `9D CD 3F`; write `0x9C:00=CC`; read `9D CD 6E` | Banked analog front-end setup. |
| 3 | Write `0x9C:51=44` | Select composite input. |
| 4 | Write routing word `0900:0018 = 02 0E`, then `03 0E`, then `03 0E` again | Preserve the intermediate state and repeated final write. |
| 5 | Read `89 89 FA`; poll `F5`; read `89 89 FA`; poll `F5` | Decoder wake/status prime. |
| 6 | Pass 1: `0x88:10–15 = 6F D4 D0 08 08 88` | First tuning pass. |
| 7 | Pass 2: `0x88:10–15 = 6F EA D0 08 08 88` | Second tuning pass. |
| 8 | Pass 3: `0x88:10–15 = 6F EA D0 1B 1B 88` | Third tuning pass; do not merge passes. |
| 9 | Device `0x94`: `41 37`, `4A B7`, `4B B7`; commit `09 00 66 03` + handshake after each | Auxiliary configuration. Other captures may use `4A BB` / `4B BB`. |
| 10 | Read `89 89 FA`, `89 89 CA`, `89 89 E7`; poll `F5`; read `99 89 FD`, `99 89 FC`, `99 89 F3` | Status loop while lock settles. |
| 11 | Confirm reads `CA=66`, `E7=69`, `F1=7E`, `CB=6E` | Stable values associated with the working capture; retain as observations, not universal constants. |
| 12 | Backend: `0x98:04=95`, `70=D0`, `0F=88`; write coefficient table `90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | Ordered backend configuration. |
| 13 | Write `0x98:0D=C8`, `0x98:0E=05`, `0x98:0E=04`; poll status and allow settle | Apply backend settings. |
| 14 | Configure SoC pipeline; queue bulk IN on `0x81` and `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | Capture starts; video frames were received after START. |


### 480i / 576i

#### NTSC-family / 60 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | `0x88:28=88` is seen in setup observations; full 480i sequence **NOT CAPTURED**. | Observed mode-related value only; not a complete sequence. |

#### PAL-family / 50 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | PAL-family 576i sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

### 480p / 576p

#### NTSC-family / 60 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | 480p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |

#### PAL-family / 50 Hz

| Step | Command / bytes written or read | Notes |
|---:|---|---|
| 1 | 576p sequence **NOT CAPTURED**. | Complete ordered bring-up **NOT CAPTURED**. |
