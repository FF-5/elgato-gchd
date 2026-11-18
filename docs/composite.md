# Composite input

Notation and status labels are defined in [README.md](README.md).
Initialization and capture start are described in [hardware.md](hardware.md).

## 1. Summary

| Source format | Status | Encoder programming |
|---|---|---|
| 480i (NTSC) | Observed | `v_format = 4`, `v_ip_format = 0` (interlaced), 720 × 480 |
| 240p (NTSC line timing, progressive) | Observed | Same as 480i: `v_format = 4`, `v_ip_format = 0`, 720 × 480 |
| 576i (PAL) | Observed | `v_format = 5`, `v_ip_format = 0`, 720 × 576 |
| 288p (PAL line timing, progressive) | Observed | Same as 576i: `v_format = 5`, `v_ip_format = 0`, 720 × 576 |

All analog formats are encoded with `v_in_source_type = 3`.

A 240p or 288p source is encoded as an interlaced stream (720 × 480 at 60
fields per second, or 720 × 576 at 50 fields per second): each field carries
one complete picture. The analog decoder
reports the format in register `0x88:CA`. A change between 480i and 240p
restarts the encoder (§4); a change between 576i and 288p reconfigures only
the decoder (§8).

## 2. Status registers

| Read | Meaning |
|---|---|
| `0x88:CA` | Lock and format, the same codes for both families: `FE` unlocked, `7E` interlaced (480i, 576i) locked, `66` progressive (240p, 288p) locked, `76` first value after a change to interlaced, `6E` first value after a change to progressive, `EE`/`E6` transitional. |
| `0x88:FA` | Low nibble `6` = NTSC (60 Hz) family, `7` = PAL (50 Hz) family once a signal is present. Reads `1F`/`3E` immediately after the input is selected. Polled while no signal is present. |
| `0x88:E7` | Decoder standard: `69` when stable (NTSC), `59` (60 Hz source at power-on of some consoles), `79` (PAL); `09` right after a 240p change; bit 7 set (`E9`, `D9`) while the decoder re-evaluates the standard, for about 0.4-0.9 s after an apply block and when the source changes family. |
| `0x88:F1` | `7E` when stable (NTSC); `63` (PAL). |
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
6. Retrigger and settle ([commands.md §3](commands.md)): retrigger (`05`);
   `0x98:F5` is read about every 4 ms and reads `62` until bit 7 is set
   (`F2`, after 40–700 ms); retrigger (`15`); `0x98:F5` reads `72`.
   INIT and START follow only after `0x98:F5` reads `72`.
7. Capture start ([hardware.md §6](hardware.md#6-capture-start-observed)).

## 4. 480i ↔ 240p while capturing (Observed)

Both directions use the same sequence; the encoder is stopped and restarted.
The lock status is polled together with `0x88:FA`, `E7` and `F1`.

1. Lock status change. 240p → 480i: `0x88:CA` reads `76`, then the change
   starts. 480i → 240p: `0x88:CA` first reads `6E`; one retrigger
   (`0x98:0D = C8`, `0E = 15`, `0E = 14`, preceded by reads of `0x98:FD`,
   `FC`, `F3`) is issued; the change starts when `0x88:CA` reads `66`.
2. Decoder: `0x88:07 = 8A`, `08 = 9B`, `09 = 7A`, `28 = 88` (to 480i) or
   `28 = 8A` (to 240p); read `0x88:FD`; `0x88:06 = 08`.
3. About 0.4 s later: read `0x88:E7` (`E9`).
4. Neutral back-end table: `0x98:04 = 95`; read `0x98:8B`; `0x98:70 = C8`,
   `90 = 88`, `91`–`A3 = 77`; read `0x98:6B`; `0x98:C0 = 80`, `A2 = 77`;
   read `0x98:58`; `0x98:C0 = 77`; read `0x98:89`; `0x98:72 = 88`,
   `C0 = 88`, `61 = 98`, `63 = 89`, `64 = 96`, `65 = 88`; read `0x98:FF`;
   `0x98:04 = 8D`, `62 = 91`; retrigger `0E = 05`, `04`.
5. SCMD STATE_CHANGE NULL (`05 00 00 04`). The state stays `0x02` and
   `0900:0074` reads `011A` for about 0.7 s, then the state reads `0x04` and
   `0900:0074` reads `011E`.
6. SCMD STATE_CHANGE STOP (`05 00 00 01`), completed after about 0.3 s.
7. Back-end table and following block as in §3 step 5, then retrigger and
   settle as in §3 step 6. The pulse values follow `0x98:F5` (`05`/`15` to
   480i, `15`/`15` to 240p in the recording).
8. Encoder setup (identical in both directions: `v_format = 4`,
   `v_ip_format = 0`, `v_in_source_type = 3`, 720 × 480), SCMD INIT mode
   `A0`, SCMD STATE_CHANGE START (`05 00 00 02`).

No `0x88:0A` write occurs in this sequence. Input routing and the decoder
tuning passes are not repeated.

## 5. Switching from HDMI to composite with a 240p source (Observed)

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

## 6. Input selector values

`0x9C:51` (Observed, no source present):

| Previous input → new input | Value |
|---|---|
| HDMI → composite or S-video | `44` |
| component → composite; composite ↔ S-video | `EC` |
| any → component | `ED` |
| any → HDMI | `45` |

Composite and S-video are distinguished by `0x88:02` (`C9` / `DC`) and
`0x88:14 = D2`, not by the selector.

The decoder keeps the S-video setting while the device has power. Selecting
composite after S-video (Observed): `0x88:02 = C9`, `14 = D2`, `3C = 6B`,
about 200 ms pause, `28 = 88`, read `FD`, `F8`, `03 = 28`, front-end and
routing writes as usual; then the first `0x98:F5` read shows bit 7 (`E2`)
and the back end is retriggered once. Lock follows about 0.3 s later. The
driver writes this sequence on the first composite start after loading,
since the decoder's state is not known then.

## 7. No-signal polling

Read `0x88:FA`, read `0x98:F5`, about every 50 ms. When `0x98:F5` has bit 7
set (`F2`, `E2`), the back end is retriggered (Observed: S-video and
composite, e.g. after the routing writes, after composite was selected
following S-video, and while a PAL console powers up after a signal loss;
when `F5` reads `62` no retrigger follows). When a source is connected, `0x88:CA` changes from `FE`, polling
ends, and the sequence continues with §3 step 3.

## 8. PAL (Observed)

### 8.1 Capture start with a 576i source

The sequence of §3 with these differences: no decoder apply block (step 3
has only the status reads), and the back-end step uses the PAL table:

`0x98:70 = D0`, `0F = 88`, `90 = D8`, `91 = BF`, `95 = 9D`, `96 = 1B`,
`97 = 88`, `A0 = 8A`, `A1 = D8`, `A2 = B2`, `A3 = 59`.

Status values with a locked 576i source: `0x88:FA = 07`/`17`,
`CA = 7E`, `E7 = 79`, `F1 = 63`.

### 8.2 576i ↔ 288p while capturing

No SCMD and no encoder writes are issued; the stream continues. The PAL
apply block uses different values from NTSC:

```
to 576i:  read 0x88:CA -> 76
          0x88:07 = 9A  08 = 90  09 = A8  28 = 88
          read 0x88:FD  0x88:06 = 08
          about 0.43 s later: retrigger (15)
          steady state: CA 7E
to 288p:  read 0x88:CA -> 6E
          retrigger (15)
          about 0.7 s later: read 0x88:CA -> 66
          0x88:07 = 9A  08 = 90  09 = A8  28 = 8A
          read 0x88:FD  0x88:06 = 08
          about 0.43 s later: read 0x88:E7, F1; retrigger (15)
          steady state: CA 66
```

A retrigger is also issued when `0x98:F5` shows bit 7 set (`E2`) while
capturing, followed by a second retrigger.

The encoder programming is the same for 576i and 288p (`v_format = 5`,
`v_ip_format = 0`, `v_in_source_type = 3`, 720 × 576).

### 8.3 60 Hz ↔ 50 Hz while capturing (Observed)

Some PAL consoles output a 60 Hz signal for a few seconds after power-on or
reset, then switch to 50 Hz. The change of family is handled with a full
encoder restart. It is started by `0x88:E7` with bit 7 set, or by
`0x88:FA` reporting the other family:

```
E7 bit 7:          neutral back-end table (§4 step 4), retrigger
other family read: apply block of that family (0x28 from CA, §9),
                   about 0.45 s later: read 0x88:E7, F1; retrigger
then:              NULL, STOP
                   read 0x88:FA, CA until locked (CA 7E or 66); while not
                   locked, the apply block of the family read every 0.5 s
                   read 0x88:E7, F1; 0x88:0A by E7 (§9)
                   back-end table of the family, retrigger, settle
                   read 0x88:FA, CA, E7, F1
                   encoder programming (§10), INIT A0, START
```

Total time from the trigger to START: about 3 s.

During this start-up phase the decoder reports `0x88:E7 = 59` (60 Hz with
PAL colour) or `E7` with bit 7 set. The driver waits up to 5 s for such a
signal to change to 50 Hz, both at the capture start and during a family
change, so that capture starts in the console's final mode. A source that
stays at 60 Hz is captured after that wait.

Applications that size their window from the first frames (for example
ffplay) show the picture squeezed or stretched after a family change while
capturing; restarting the application shows it correctly.

## 9. Decoder apply block

The writes `0x88:07 = 8A`, `08 = 9B`, `09 = 7A`, `28 = …` followed by a read
of `0x88:FD` and `0x88:06 = 08` occur when lock is acquired after the input
was selected without a source (Observed). With a progressive source the block
is written twice: first with `28 = 88`, then with `28 = 8A`, followed by
a write of `0x88:0A`. When the source is already locked at input selection,
the block is not written.

The block for the 50 Hz family is `0x88:07 = 9A`, `08 = 90`, `09 = A8`.
`0x88:28` follows bit 3 of `0x88:CA`: set (`7E`, `EE`, `FE`) → `88`, clear
(`66`, `E6`) → `8A`.

`0x88:0A` depends on the standard in `0x88:E7`, read once its bit 7 is
clear:

| `0x88:E7` | `0x88:0A` |
|---|---|
| `59` | `8F` |
| `69` | `98` |
| `79` | `85` |

## 10. Encoder settings for analog sources (Observed)

Settings of the capture application and the encoder fields they change,
recorded with a PAL 288p source (720 × 576 input). Each change stops and
restarts the encoder (§4 steps 5–8) unless noted.

| Setting | Encoder fields |
|---|---|
| Standard (default) | 720 × 576, `v_ip_format = 0`, `v_gop_size = 12`, `v_rate_mode = 0`, `v_max_cpb_delay = 3`, `v_bitrate = 11250`, max `10125`, average `5625`, min `2812`, `system_rate = 12531`, level 31, transport output 1 |
| Standard, lower bitrate | `v_rate_mode = 1`, `v_max_cpb_delay = 10`; for example `v_bitrate = 6000`, max `5400`, average `3000`, min `600`, `system_rate = 6898`; or `14040` / `12636` / `7020` / `3510`, `system_rate = 15524` |
| Standard, 640 × 480 | 640 × 480, `v_ip_format = 2`, `v_gop_size = 6`, level 30, transport output 2 |
| Mobile | 480 × 360, `v_ip_format = 2`, `v_gop_size = 6`, `v_rate_mode = 1`, `v_max_cpb_delay = 10`, level 22, transport output 2; bitrate from `4000` / `3600` / `2000` / `0` (`system_rate = 4753`) to `15000` / `13500` / `7500` / `3750` (`system_rate = 16554`) |
| Stretch standard-definition input | `v_in_source_type = 2` instead of `3` |
| Cropping (overscan) | no control transfers; bulk OUT (`0x02`) not recorded |
| HD output sizes (1280 × 720, 1920 × 1080) | no control transfers, the encoder stays at 720 × 576; bulk OUT (`0x02`) not recorded |

The same fields are written for the `_bl` variants. Transport output 1 and 2
are defined in [hdmi.md §4](hdmi.md#4-capture-with-a-1080p60-source-output-size-changes-observed).
