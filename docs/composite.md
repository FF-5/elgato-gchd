# Composite input reference

## Common commands and configuration

| Target / operation | Bytes written or expected read | Notes |
|---|---|---|
| `0x88:06` | Write `08` | Analog decoder mode. |
| `0x88:03` | Write `28` | Analog decoder setup. |
| `0x9C:B3` | Write `33` | Analog front-end setup; meaning partly unknown. |
| `0x9C:00`, `0x9C:27` | `CE`, `33`, `CC` | Bank/page setup; preserve order. |
| `0x9C:51` | Write `44` | Composite selector on the HDMI→composite path (signal-present and no-input). |
| `0x9C:51` | Write `EC` | Composite/S-video selector when switching from component (or between analog inputs) with no source. See “Selector 51 EC vs 51 44” below. |
| `0x88:02` | Write `C9` | Composite discriminator; S-video uses `DC`. |
| Mailbox read `89 89 FA` | Expect low nibble `6` (NTSC/60 Hz) or `7` (PAL/50 Hz) | Detects family, not progressive/interlaced format. In the HDMI→composite 240p NTSC switch the two reads right after the selector (before the tuning passes) returned `1F` and `3E`; low nibble `6` only appears from the `0x94` writes onward (`16`, `06`). |
| Mailbox read `89 89 FD` | `60` at NTSC step 1 (before the selector); `6E` is returned by the `99 89 FD` / `99 89 FC` form used later in setup | Status poll; meaning unknown. Helper prefixes differ (see [commands.md](commands.md)). |
| Mailbox read `89 89 F1` | NTSC 240p: `7E`. PAL branch: `63` (earlier note) | PAL status poll; exact semantics unknown. The earlier `63` note **conflicts** with `F1=7E` in the PAL status loop and is unresolved. |
| `0x88:10–15` | `6F D4 D0 08 08 88` | Decoder tuning pass 1. |
| `0x88:10–15` | `6F EA D0 08 08 88` | Decoder tuning pass 2. |
| `0x88:10–15` | `6F EA D0 1B 1B 88` | Decoder tuning pass 3. |
| `0x98:0D`, `0x98:0E` | `C8`, `15`, `14` | Backend retrigger pulse; preserve order. Seen twice in the NTSC 240p switch: once before backend programming (right after the `0x94` writes) and once after the `05 → 04` apply pulse. |
| `0x98:0D`, `0x98:0E` | `C8`, `05`, `04` | Apply pulse after backend programming. |
| `0x98` backend | See ordered sequence and coefficient table | Register meanings are not fully decoded. |



### Backend coefficient table

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


## No-signal idle / keep-alive

After the composite selector and front-end programming complete with no source present, the device repeats this two-command loop (approximately 50 ms per cycle). In the two idle cycles observed between the selector and the tuning passes of the HDMI→composite 240p switch the spacing was about 70–110 ms (low confidence, only two cycles). Each mailbox command uses the normal HD-New mailbox handshake.

| Order | Mailbox payload | Notes |
|---:|---|---|
| 1 | `89 89 FA 00` | Status / source-detect poll |
| 2 | `99 89 F5 00` | Sequence-read / poll helper |

The repeated `89 89 FA` read is the current source-detection candidate: when a source appears, the reply can change to `CA` and the idle loop is exited. See [hardware.md](hardware.md) for the comparison across all inputs.

### Selector `51 EC` vs `51 44` (no-source multi-switch evidence)



| Transition | Selector written | Notes |
|---|---|---|
| HDMI → component | `51 ED` | Component selector (unchanged). |
| component → composite | `51 EC` | No `02 C9` discriminator in this hop; decoder/front-end prefix (`06 08`, `03 28`, `B3 33`) still runs. |
| composite → S-video | `51 EC` | Preceded by `02 DC` + `14 D2` (S-video discriminator path). |
| S-video → composite | `51 EC` | Preceded by `02 C9` + `14 D2` (composite discriminator). |
| composite → component | `51 ED` | Back to component. |
| component → HDMI | `51 45` | HDMI selector. |

In earlier HDMI→composite and HDMI→S-video no-input observations the selector was consistently `51 44`. In the multi-switch trace every composite/S-video selection that follows a component (or another analog) selection uses `51 EC` instead. The choice is therefore path-dependent:

- **`51 44`** — observed on the direct HDMI → composite / HDMI → S-video path (both signal-present and no-input).
- **`51 EC`** — observed when the previous input was component or another analog input, when no source is present.

After `51 EC` the device still enters the shared composite/S-video no-input idle (`89 89 FA` / `99 89 F5`) and can later take the source-found path (`89 89 CA`) documented in the no-input difference tables. The EC vs 44 distinction does not by itself suppress source detection.

## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

Rows are aligned by logical operation so the two columns can be compared directly. A dash means the corresponding step was not observed (or not required) in that family’s sequence. Step numbers are sequential within each column and reflect the order seen in the respective sequences.

Observed NTSC order is taken from the HDMI→composite 240p NTSC signal-present sequence (`240p composite sequence`). The previously listed `0x88:02=C9` does not appear in that switch window and is omitted from the ordered NTSC steps (it remains in the common-commands table).

| NTSC step | NTSC-family 240p | PAL step | PAL-family 288p-equivalent |
|---:|---|---:|---|
| 1 | Read `89 89 FD` | 1 | Read `89 89 FD` |
| 2 | Decoder `0x88:06=08` | 2 | Decoder `0x88:06=08` |
| 3 | Read `89 89 F8` | 3 | Read `89 89 F8` |
| 4 | Decoder `0x88:03=28` | 4 | Decoder `0x88:03=28` |
| 5 | Front-end `0x9C:B3=33` | 5 | Front-end `0x9C:B3=33` |
| 6 | Page `CE`; `0x9C:27=33`; page `CC` (mailbox reads between writes) | 6 | Page `CE`; `0x9C:27=33`; page `CC` (mailbox reads between writes) |
| 7 | Selector `0x9C:51=44` | 7 | Selector `0x9C:51=44` |
| 8 | Routing `02 0E → 03 0E → 03 0E` | 8 | Routing `02 0E → 03 0E → 03 0E` |
| 8a | Two idle cycles `89 89 FA` / `99 89 F5` before tuning (`FA` reads `1F`, then `3E`; `F5` reads `62`) | — | — |
| — | Not run here; NTSC runs the same status values later (steps `12a`–`12c`) | 9 | Decoder wake / status prime; status loop (`CA=66`, `E7=69`, `F1=7E`, `CB=6E`) |
| 9 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 10 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 10 | Tuning pass 2: `6F EA D0 08 08 88` | 11 | Tuning pass 2: `6F EA D0 08 08 88` |
| 11 | Tuning pass 3: `6F EA D0 1B 1B 88` | 12 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7` (commit `09 00 66 03` + handshake after each) |
| 12a | Mailbox status reads `89 89 FA`, `CA`, `E7`, then `99 89 F5`, `FD`, `FC`, `F3`, `F5` (replies `16`, `E6`, `E9`, `F2`, `6E`, `6E`, `7E`, `F2`) | — | — |
| 12b | First retrigger pulse, before any backend write: `0x98:0D=C8`, `0x98:0E=15 → 14` | — | — |
| 12c | Status loop repeats (`FA`, `CA`, `E7`, `F5`) while `CA` settles `E6 → 66` and `F5` moves to `72`; `0800:2008` read → `00 01`; final block `FA=06`, `CA=66`, `E7=69`, `F1=7E`, `CB=6E`, `CA=66` | — | — |
| 13 | Backend `04=95`, `70=D0`, `0F=88` | 14 | Backend `04=95`, `70=D0`, `0F=88` |
| 14 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | 15 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| 15 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; later `15 → 14` (the pre-backend pulse is step `12b`). Then poll `99 89 F5` (64 reads: about 14 of `62`, about 46 of `72`, a few `F2`/`E2` around the pulses) plus a status block (`FA=06`, `CA=66`, `E7=69`, `F1=7E`) to let the decoder settle | 16 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; poll status and allow settle |
| 16 | Pipeline setup: 120 masked 8-byte transcoder writes (`0x152C`/`0x162C` carry `0x02D0` and `0x01E0`); queue 36 bulk IN URBs on `0x81` and 8 on `0x84` during the burst; SCMD `04 A0 00 00` (no interrupt or `0x14` receipt); read `0800:2008 → 00 01`; SCMD `05 00 00 02`; receipt via interrupt `10 00 00` + `0800:0014 → 05 00`; two completion rounds (`2008 → 00 01`, then `00 02`, each acknowledged); first MPEG-TS bulk completion about 0.5 s after START | 17 | Configure SoC pipeline; queue bulk IN on `0x81` and `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. Capture starts and frames were received. |

#### No-input then source-found: NTSC and PAL

The order column numbers rows only. Read the NTSC and PAL step references independently from top to bottom; blank entries mean that sequence has no corresponding step at that position.

| Order | NTSC step(s) | NTSC sequence difference | PAL step(s) | PAL sequence difference |
|---:|---:|---|---:|---|
| 1 | 9–11 | Tuning passes 1–3 run before decoder/front-end/selector setup. | 1–8 | Decoder, front-end, selector, and routing setup runs first. |
| 2 | 1–8 | Decoder `0x88:06=08`, decoder `0x88:03=28`, front-end `0x9C:B3=33`, selector `0x9C:51=44`, routing `02 0E → 03 0E`; follows tuning. | 9 | Status-prime step from the signal-present sequence is not repeated here. |
| 3 | 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7`. | 10–12 | Tuning passes occur after decoder/front-end/selector setup, unlike NTSC no-input. |
| 4 | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). | 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7`. |
| 5 | — | Source recognition: status changes to `89 89 CA`, followed by `E7` / `F1` / further status reads; exits the idle loop. | — | No-input idle loop: `89 89 FA` / `99 89 F5` (about 50 ms). |
| 6 | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08` (the `06=08` write appears twice). | — | Source recognition: status changes to `89 89 CA`; exits the idle loop. |
| 7 | 13–16 | Backend coefficients `0x98:90–A3` and tail writes; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`), after source recognition. | — | Apply block: `07=8A`, `08=9B`, `09=7A`, `28=88` / `8A`, `06=08`. |
| 8 | — | — | 14–17 | Backend coefficients `0x98:90–A3` and tail writes; SCMD `04 A0 00 00` → `05 00 00 02` (later `05 00 00 04`), after source recognition. |

### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | `0x88:28=88` seen in setup observations; full 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | 480p sequence **NOT CAPTURED**. | 1 | 576p sequence **NOT CAPTURED**. |
