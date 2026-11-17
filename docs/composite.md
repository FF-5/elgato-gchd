# Composite input

Notation and status labels are defined in [README.md](README.md).
Initialization and capture start are described in [hardware.md](hardware.md).

## 1. Summary

| Source format | Status | Encoder programming |
|---|---|---|
| 480i (NTSC) | Observed | `v_format = 4`, `v_ip_format = 0` (interlaced), 720 × 480 |
| 240p (NTSC line timing, progressive) | Observed | Same as 480i: `v_format = 4`, `v_ip_format = 0`, 720 × 480 |
| 576i (PAL) | Driver | `v_format = 5`, `v_ip_format = 0`, 720 × 576 |
| 288p (PAL line timing, progressive) | Observed | Same as 576i: `v_format = 5`, `v_ip_format = 0`, 720 × 576 |

All analog formats are encoded with `v_in_source_type = 3`.

A 240p or 288p source is encoded as an interlaced stream (720 × 480 at 60
fields per second, or 720 × 576 at 50 fields per second): each field carries
one complete picture. The analog decoder
reports the format in register `0x88:CA` and is reconfigured when the source
switches between 480i and 240p (§4, §5).

## 2. Status registers

| Read | Meaning |
|---|---|
| `0x88:CA` | Lock and format: `FE` unlocked, `7E` 480i, 576i or 288p locked, `66` 240p locked, `EE`/`E6` transitional. |
| `0x88:FA` | Low nibble `6` = NTSC (60 Hz) family, `7` = PAL (50 Hz) family once a signal is present. Reads `1F`/`3E` immediately after the input is selected. Polled while no signal is present. |
| `0x88:E7` | `69` when stable; `09` right after a 240p change. |
| `0x88:F1` | `7E` when stable (NTSC). |
| `0x88:CB` | `6E` when stable; `44` right after a 240p change. |
| `0x88:FD`, `0x88:F8` | `6E`, `C9`. Unknown. |

## 3. 480i: lock and capture start (Observed)

1. Input prefix: `0x88:06 = 08`; read `0x88:F8`; `0x88:03 = 28`;
   `0x9C:B3 = 33`; bank `CE`, `0x9C:27 = 33`; bank `CC`; read `0x9C:6E`;
   `0x9C:51 = 44` (input selector); read `0x88:FA`.
2. Decoder tuning, three passes on `0x88:10`–`15`:
   `6F D4 D0 08 08 88`, then `6F EA D0 08 08 88`, then `6F EA D0 1B 1B 88`.
3. Read `0x88:FA` (`06`) and `0x88:CA` (`FE`); `0x88:07 = 8A`, `08 = 9B`,
   `09 = 7A`, `28 = 88`; read `0x88:FD`; `0x88:06 = 08`.
4. Read `0x88:E7` (`69`), `F1` (`7E`), `CB` (`6E`), `CA`.
5. Back end: `0x98:04 = 95`; read `0x98:8B`; `0x98:70 = D0`, `0F = 88`;
   `0x98:90`–`A3` =
   `01 BD 9A 3A E9 AB 17 88 84 8A 9C 8C 98 93 83 A9 8B E8 81 49`;
   read `0x98:89`; `0x98:72 = 88`, `C0 = 88`, `61 = 98`, `63 = 89`,
   `64 = 96`, `65 = 88`; read `0x98:FF`; `0x98:04 = 8D`, `62 = 91`.
6. Retrigger: read `0x98:FD`, `FC`, `F3`; `0x98:0D = C8`, `0E = 05`,
   `0E = 04`; then the same with `0E = 15`, `0E = 14`.
7. Capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

## 4. 480i → 240p while capturing (Observed)

No SCMD and no encoder writes are issued; the stream continues.

```
read 0x88:CA -> 7E (480i) ... read 0x88:CA -> 66 (240p)
0x88:07 = 8A   0x88:08 = 9B   0x88:09 = 7A   0x88:28 = 8A
read 0x88:FD   0x88:06 = 08
read 0x88:E7 (09)   read 0x88:F1 (7E)
0x88:0A = 8F
read 0x88:CB (44)   read 0x88:CA (EE)
read 0x98:FD/FC/F3   0x98:0D = C8   0x98:0E = 15   0x98:0E = 14
read 0x88:FA (16)  CA (66)  E7 (69)  F1 (69)
0x88:0A = 98
steady state: FA 06, CA 66, E7 69, F1 7E
```

## 5. 240p → 480i (Observed)

The encoder is stopped and restarted:

1. `0x98:65 = 88`; read `0x98:FF`; `0x98:04 = 8D`, `62 = 91`; retrigger
   `0E = 05`, `04`.
2. SCMD STATE_CHANGE NULL (`05 00 00 04`), then STOP (`05 00 00 01`).
3. Read `0x88:CA` (`7E`).
4. Back-end table and following block as in §3 step 5; both retriggers.
5. SCMD INIT mode `A0`; SCMD STATE_CHANGE START (`05 00 00 02`).

The two directions are not symmetric: 480i → 240p changes decoder registers
only, while 240p → 480i restarts the encoder.

## 6. Switching from HDMI to composite with a 240p source (Observed)

1. Read `0x88:FD`; `0x88:06 = 08`; read `0x88:F8`; `0x88:03 = 28`;
   `0x9C:B3 = 33`; bank `CE`, `0x9C:27 = 33`; bank `CC`; `0x9C:51 = 44`;
   `0900:0018` written `02 0E`, then `03 0E` twice.
2. Two no-signal polling cycles (`0x88:FA` reads `1F`, then `3E`;
   `0x98:F5` reads `62`).
3. Decoder tuning passes 1–3 (§3 step 2).
4. `0x94:41 = 37`, `0x94:4A = B7`, `0x94:4B = B7`.
5. Status reads; first retrigger `0x98:0D = C8`, `0E = 15`, `14`; status
   polling until `0x88:CA` settles from `E6` to `66`; read `0800:2008`
   (`0x01`).
6. Back end as in §3 step 5.
7. Retrigger `0E = 05`, `04`, later `15`, `14`; 64 reads of `0x98:F5`.
8. Encoder setup including `v_format = 4`, `v_ip_format = 0`,
   `v_in_source_type = 3`, 720 × 480; capture start.

## 7. Input selector values

`0x9C:51` (Observed, no source present):

| Previous input → new input | Value |
|---|---|
| HDMI → composite or S-video | `44` |
| component → composite; composite ↔ S-video | `EC` |
| any → component | `ED` |
| any → HDMI | `45` |

Composite and S-video are distinguished by `0x88:02` (`C9` / `DC`) and
`0x88:14 = D2`, not by the selector.

## 8. No-signal polling

Read `0x88:FA`, read `0x98:F5`, about every 50 ms. When a source is
connected, `0x88:CA` changes from `FE`, polling ends, and the sequence
continues with §3 step 3.

## 9. PAL (Observed)

For a 50 Hz source (`0x88:FA` low nibble `7`) the back-end step of §3 step 5
uses a different per-mode table; all other writes are identical:

`0x98:70 = D0`, `0F = 88`, `90 = D8`, `91 = BF`, `95 = 9D`, `96 = 1B`,
`97 = 88`, `A0 = 8A`, `A1 = D8`, `A2 = B2`, `A3 = 59`.

Status values with a 288p source: `0x88:CA = 7E`, `E7 = 79`, `F1 = 63`.

## 10. Decoder apply block

The writes `0x88:07 = 8A`, `08 = 9B`, `09 = 7A`, `28 = …` followed by a read
of `0x88:FD` and `0x88:06 = 08` occur when lock is acquired after the input
was selected without a source (Observed). With a progressive source the block
is written twice: first with `28 = 88`, then with `28 = 8A`, followed by
`0x88:0A = 98` (NTSC 240p) or `0x88:0A = 8F` (PAL 288p). When the source is
already locked at input selection, the block is not written.
