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


## Sequences

### 240p / 288p

#### NTSC-family 240p vs PAL-family 288p-equivalent

Rows are aligned by logical operation so the two columns can be compared directly. A dash means the corresponding step was not observed (or not required) in that family’s capture. Step numbers are sequential within each column and reflect the order seen in the respective traces.

Observed NTSC order is taken from the HDMI→composite 240p NTSC signal-present capture (`filtered_240p_composite.pcapng`). The previously listed `0x88:02=C9` does not appear in that switch window and is omitted from the ordered NTSC steps (it remains in the common-commands table).

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
| — | — | 9 | Decoder wake / status prime; status loop (`CA=66`, `E7=69`, `F1=7E`, `CB=6E`) |
| 9 | Tuning pass 1: `6F D4 D0 08 08 88` to `0x88:10–15` | 10 | Tuning pass 1: `6F D4 D0 08 08 88` |
| 10 | Tuning pass 2: `6F EA D0 08 08 88` | 11 | Tuning pass 2: `6F EA D0 08 08 88` |
| 11 | Tuning pass 3: `6F EA D0 1B 1B 88` | 12 | Tuning pass 3: `6F EA D0 1B 1B 88` |
| 12 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | 13 | Device `0x94`: `41 37`, `4A B7`, `4B B7` (commit `09 00 66 03` + handshake after each) |
| 13 | Backend `04=95`, `70=D0`, `0F=88` | 14 | Backend `04=95`, `70=D0`, `0F=88` |
| 14 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` | 15 | Coefficient table `0x98:90–A3`; then `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`, `04=8D`, `62=91` |
| 15 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; later `15 → 14` | 16 | Retrigger `0x98:0D=C8`, `0x98:0E=05 → 04`; poll status and allow settle |
| 16 | Pipeline setup; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02` | 17 | Configure SoC pipeline; queue bulk IN on `0x81` and `0x84`; SCMD `04 A0 00 00`; read `0800:2008 → 00 01`; SCMD `05 00 00 02`. Capture starts and frames were received. |

#### No-input / no-signal NTSC path (HDMI → composite, source appears later)

Source: `filtered_240p_noinput_composite.pcapng`. Capture starts at HDMI selector `51 45`, runs the HDMI front-end / keep-alive, then switches to composite **with no source attached**. After a long post-switch idle the source is recognized and the host proceeds to apply / coefficients / SCMD.

**Differences vs the signal-present ordered table above**

| Aspect | Signal-present (`filtered_240p_composite.pcapng`) | No-input then source-found (this trace) |
|---|---|---|
| Tuning vs decoder/selector order | Decoder / front-end / selector **then** the three tuning passes | Three tuning passes (`6F D4…` / `6F EA…` / `6F EA 1B…`) run **first**, then `FD` → `06=08` → `F8` → `03=28` → `B3=33` → page → `51=44` → routing |
| `0x88:02=C9` | Not observed in switch window | Still not observed |
| Post-switch idle | Continues into lock / backend | Enters tight **FA / F5** poll loop (~50 ms) — same as S-video no-input; **not** the HDMI `AB 4C` loop |
| Source-found trigger | N/A (signal already present) | Idle broken by appearance of `89 89 CA` (and following status); then apply block |
| Apply block (`07=8A`, `08=9B`, `09=7A`, `28=88`/`8A`) | Not present in the signal-present ordered table | Present (twice) after source recognition; same values as S-video apply block |
| Coefficient table + SCMD | After tuning / device-0x94 | After source-found apply + status; same NTSC table `90=01…A3=49`; SCMD `04 A0 00 00` then `05 00 00 02` (later `05 00 00 04` also seen) |

**Post-switch ordered sequence (no source → source recognized)**

| Step | Command / bytes | Notes |
|---:|---|---|
| 1 | Three tuning passes to `0x88:10–15`: `6F D4 D0 08 08 88` → `6F EA D0 08 08 88` → `6F EA D0 1B 1B 88` | **Order difference** — tuning precedes decoder setup on this path. |
| 2 | Read `89 89 FD`; write `0x88:06=08`; read `89 89 F8`; write `0x88:03=28` | Shared analog decoder. |
| 3 | Front-end `0x9C:B3=33`; page `CE`; `0x9C:27=33`; page `CC`; read `9D CD 6E` | Shared front-end. |
| 4 | Selector `0x9C:51=44`; routing `02 0E → 03 0E → 03 0E` | Switch complete. |
| 5 | Device `0x94`: `41 37`, `4A B7`, `4B B7` | Same as signal-present. |
| 6 | Status sample (`FA` / `F5` / `FD` / `FC` / `F3`); dual retrigger `0x98:0D=C8`, `0E=05→04` then `15→14` | Host advances without lock. |
| 7 | **No-input idle loop** (see below) | Long-running until source appears. |
| 8 | Status becomes `89 89 CA` (and following `E7` / `F1` / …) | **Observed source-recognition trigger** that exits the FA/F5 idle. |
| 9 | Apply block: `0x88:07=8A`, `08=9B`, `09=7A`, `28=88` (later pass uses `28=8A`); `06=08` | Same apply values documented on S-video. |
| 10 | Further status + second apply / retrigger path (`0E=15→14`) |  |
| 11 | Backend `04=95`, `70=D0`, `0F=88`; full coefficient table `90=01 … A3=49`; tail `72=88` … `62=91` | Identical NTSC table to signal-present. |
| 12 | Pipeline / SCMD `04 A0 00 00`; SCMD `05 00 00 02` (later `05 00 00 04` also present) | Stream start path. |

**Post-switch no-input idle loop**

After the initial switch and early retriggers the device sits in a tight poll loop (identical to the S-video no-input idle; **not** the HDMI `AB 4C` keep-alive):

```
89 89 FA 00
99 89 F5 00
```

(each with the normal HD-New mailbox handshake). Period is roughly 50 ms. Occasional other status helpers appear in the same window. Composite and S-video therefore share the same no-source idle after analog input selection.

**Source-recognition trigger (observed)**

The FA/F5 idle is broken when the host observes a change that includes `89 89 CA` (followed by further status reads). Immediately after that the apply block, second retrigger series, coefficient table and SCMD sequence run. The exact hardware status bit that the host is waiting for is still UNKNOWN; `CA` is the first clear mailbox marker that correlates with exit from the no-input idle in this capture.

#### Other formats

##### 480i / 576i

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | `0x88:28=88` seen in setup observations; full 480i sequence **NOT CAPTURED**. | 1 | PAL-family 576i sequence **NOT CAPTURED**. |

##### 480p / 576p

| NTSC step | NTSC-family / 60 Hz | PAL step | PAL-family / 50 Hz |
|---:|---|---:|---|
| 1 | 480p sequence **NOT CAPTURED**. | 1 | 576p sequence **NOT CAPTURED**. |
