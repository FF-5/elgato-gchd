# Composite input

Notation and confidence markers: [README.md](README.md). Common init and
capture start: [hardware.md](hardware.md).

## Summary

| Mode | C++ starting point | Windows trace (reference) | Encoder programming |
|---|---|---|---|
| 480i (NTSC) | **Yes**, worked in 2016 | Yes (`trace_analysis.txt`) | `v_format 4`, `v_ip_format 0` (interlaced) |
| 576i (PAL) | **Yes** | **No PAL trace exists in the repository** | `v_format 5`, `v_ip_format 0` |
| 240p (NTSC-family console signal) | No; detects once at start-up, so it would see 480i and encode as 480i; no mode-change handling | Yes (`trace_analysis.txt`, `gchd_composite_240p_full_bringup.txt`) | **Same as 480i**: `v_format 4`, `v_ip_format 0` (**WIN**) |
| 288p (PAL-family) | No | No (see §6 note) | Not captured; expected to mirror 576i |
| 480p / 576p over composite | Not a valid composite signal | — | — |

**Most important fact for 240p:** the Windows driver does **not** program the
encoder as progressive for 240p. The encoder runs exactly as for 480i
(720×480, field-coded H.264, 60 fields/s). A 240p source simply produces two
fields that carry the same 240-line picture, and the decoder register `0x88:28`
is toggled (`88` ↔ `8A`). Programming 240p as 480p (`v_format 8`,
`v_ip_format 2`) is what the DKMS `state-machine` branch does, and it is not
what Windows does.

## 1. C++ starting point (480i / 576i)

`configureComposite()` runs after common init step 24 ([hardware.md §5](hardware.md#5-common-initialization-c-configuredevice-verified-against-win)):

1. `94 41 37`, `94 4A AF`, `94 4B AF` (device `0x94` via `0x33`).
2. Family detection: read `89 89 FA`, low nibble `6` → NTSC 60 Hz, `7` → PAL
   50 Hz, anything else → error (unless the user forced a mode). The scan mode
   is always **interlaced**.
3. On a detection error the C++ runs an "anti-lockup" snippet on device `0x9C`
   before rethrowing (bank `CC`, `AB=4C`, bank `CE`, `1B=33`, bank `CC`,
   `B7=CE`, `B8=DC`, `B8=CC`).
4. NTSC only: `0x88:07=8A`, `08=9B`, `09=7A`, `28=88`; read `89 89 FD`;
   `0x88:06=08`.
5. Read `89 89 E7`. PAL only: read `89 89 F1`; `0x98:04=95`.
6. `configureSetupSubblock()` (device `0x98` `70`, `0F`, `90–A3` per mode);
   `configureCommonBlockB1(false)`.
7. Read `99 89 F5`, `FD`, `F5`, `FC`, `F3`; `0x98:0C=89`, `0D=C8`; read
   `99 89 F5` → `s`; `0x98:0E=(s&10)|65`, then `(s&10)|64`.
8. `configureCommonBlockA`, `configureSetupSubblock`, `configureCommonBlockB1(false)`,
   `configureCommonBlockB2`, `configureCommonBlockB3`, `configureCommonBlockC`,
   `configureCommonBlockB3`.
9. `transcoderFinalConfigure`, `transcoderSetup`, then capture start
   ([hardware.md §6](hardware.md#6-capture-start)).

Colour space defaults to YUV for composite. `v_in_source_type` is `0` (NTSC)
or `3` (PAL) unless the 16:9 stretch option is set.

## 2. Status registers

| Read | Meaning (WIN unless stated) |
|---|---|
| `89 89 CA` | **Lock/format:** `FE` unlocked, `7E` 480i locked, `66` 240p locked, `EE`/`E6` transient. Bits 4:3 = `00` for 240p. |
| `89 89 FA` | Low nibble `6` NTSC, `7` PAL once locked (**C++** detection). Reads `1F`/`3E` immediately after the selector, before tuning. Polled in the no-signal loop. |
| `89 89 E7` | `69` steady; `09` right after a 240p change. |
| `89 89 F1` | `7E` steady (NTSC); C++ expects `63` on PAL. |
| `89 89 CB` | `6E` steady; `44` right after a 240p change. |
| `89 89 FD`, `F8` | `6E`, `C9`/`CC`. Meaning UNKNOWN. |

## 3. Windows: 480i lock and start (`trace_analysis.txt`, t = 8.8–10.4 s)

1. Prefix: `0x88:06=08`; read `89 89 F8`; `0x88:03=28`; `0x9C:B3=33`;
   bank `CE`, `0x9C:27=33`; bank `CC`; read `9D CD 6E`; **`0x9C:51=44`**;
   read `89 89 FA` (`3F`).
2. Decoder tuning, three passes on `0x88:10–15`: `6F D4 D0 08 08 88`, then
   `6F EA D0 08 08 88`, then `6F EA D0 1B 1B 88`.
3. Read `FA` (`06`) and `CA` (`FE`); `0x88:07=8A`, `08=9B`, `09=7A`,
   **`28=88`**; read `FD`; `0x88:06=08`.
4. Read `E7` (`69`), `F1` (`7E`), `CB` (`6E`), `CA`.
5. Back end: `0x98:04=95`; read `99 89 8B`; `70=D0`, `0F=88`;
   `90–A3 = 01 BD 9A 3A E9 AB 17 88 84 8A 9C 8C 98 93 83 A9 8B E8 81 49`;
   read `99 89 89`; `72=88`, `C0=88`, `61=98`, `63=89`, `64=96`, `65=88`;
   read `99 89 FF`; `04=8D`, `62=91`.
6. Retrigger: read `99 89 FD/FC/F3`; `0x98:0D=C8`, `0E=05`, `0E=04`; repeat
   with `0E=15`, `0E=14`.
7. Transcoder burst and URB queueing, `SCMD 04 A0 00 00`, `SCMD 05 00 00 02`.

Mapping to C++: step 5 = `configureSetupSubblock` + `configureCommonBlockB1(false)`;
step 6 = the `0x98:0D/0E` retrigger. **DISCREPANCIES** with C++:

- The C++ NTSC-interlaced setup subblock writes only `90=68, 91=BF, 95=AB, 96=17, 97=88, A0=8C, A1=F8, A2=82, A3=59`. Windows writes all of `90–A3` with different values in `90`, `91`, `A0–A3`.
- `0x98:C0`: C++ `89`, Windows `88`.
- `0x98:0E` base: C++ `65/64`, Windows `05/04` and `15/14`.
- Tuning `0x88:10`: C++ `88`, Windows `6F`.
- Selector `0x9C:51`: C++ `CC`, Windows `44`.

The Windows trace is the reference: the DKMS driver follows the Windows values.
The C++ values are kept here because they produced 480i video in 2016; if a
Windows value misbehaves, test the C++ value as a fallback, one register at a
time, with a before/after capture.

## 4. Windows: 480i → 240p while capturing (`trace_analysis.txt`, t = 25.0–28.5 s)

**No SCMD, no transcoder writes; the stream keeps running.**

```
READ 89 89 CA -> 7E  (480i)              ... later READ CA -> 66 (240p)
WRITE 0x88: 07=8A 08=9B 09=7A 28=8A      # 28: 88 -> 8A
READ 89 89 FD ; WRITE 0x88:06=08
READ E7 (09) ; READ F1 (7E)
WRITE 0x88:0A=8F
READ CB (44) ; READ CA (EE)
READ 99 89 FD/FC/F3 ; WRITE 0x98:0D=C8 0E=15 0E=14
READ FA (16) CA (66) E7 (69) F1 (69)
WRITE 0x88:0A=98
(steady: FA 06, CA 66, E7 69, F1 7E)
```

## 5. Windows: 240p → 480i (`trace_analysis.txt`, t = 56.4–60.5 s)

**Full restart:** back-end tail (`0x98:65=88`; read `FF`; `04=8D`, `62=91`;
retrigger `05/04`); `SCMD 05 00 00 04` (NULL); `SCMD 05 00 00 01` (STOP); read
`CA` (`7E`); back-end table and B1 tail exactly as in §3 step 5; both
retriggers; `SCMD 04 A0 00 00`; `SCMD 05 00 00 02`.

The asymmetry is notable. 480i → 240p is register-only, while 240p → 480i
stops and restarts the encoder. The reason is not known. The DKMS driver must
reproduce both directions as recorded rather than "simplifying" them.

## 6. Windows: HDMI → composite with a 240p source (`gchd_composite_240p_full_bringup.txt`)

| Step | NTSC 240p (WIN) | PAL 288p-equivalent (**UNVERIFIED**, see note) |
|---:|---|---|
| 1 | Read `89 89 FD`; `0x88:06=08`; read `F8`; `0x88:03=28`; `0x9C:B3=33`; bank `CE`, `27=33`, bank `CC`; **`0x9C:51=44`**; routing `0900:0018 = 03 0E` | Same |
| 2 | Two idle cycles (`FA` = `1F`, `3E`; `F5` = `62`) | Status prime (`CA=66`, `E7=69`, `F1=7E`, `CB=6E`) |
| 3 | Tuning passes 1–3 on `0x88:10–15` | Same |
| 4 | `94 41 37`, `94 4A B7`, `94 4B B7` | Same |
| 5 | Status block; first retrigger `0x98:0D=C8, 0E=15/14`; status loop until `CA` settles `E6 → 66`; read `2008 → 0001` | — |
| 6 | Back end `04=95`, `70=D0`, `0F=88`, `90–A3` table (§3), B1 tail | Back end `70=D0, 0F=88, 90=D8, 91=BF, 95=9D, 96=1B, 97=88, A0=8A, A1=D8, A2=B2, A3=59` (identical to the C++ PAL-interlaced setup subblock), B1 tail |
| 7 | Retrigger `05/04`, later `15/14`; 64 polls of `99 89 F5` | Retrigger, settle |
| 8 | About 120 transcoder writes including `v_format=4`, `v_ip_format=0`, `v_in_source_type=3`, `720×480`; URBs (36 × `0x81`, 8 × `0x84`); `SCMD 04 A0`; read `2008`; `SCMD 05 00 00 02`; two completion rounds | Same pattern; frames received |

**Note on the PAL column.** Earlier documentation described a "working PAL
288p-equivalent" sequence, attributing it to composite in one page and to
S-video in another. No PAL capture or decoded PAL trace exists anywhere in the
repository history, and the back-end values are byte-identical to the C++
PAL-interlaced `configureSetupSubblock`. Treat the column as a hypothesis until
a PAL trace is recorded.

## 7. Selector `0x9C:51` for analog inputs (WIN, no source)

| Transition | Value |
|---|---|
| HDMI → composite / S-video | `44` |
| component → composite, composite ↔ S-video | `EC` |
| any → component | `ED` |
| any → HDMI | `45` |

Composite vs S-video is chosen by `0x88:02` (`C9` / `DC`) together with
`0x88:14 = D2`, not by the selector.

## 8. No-signal loop

Read `89 89 FA`, read `99 89 F5`, about every 50 ms. When a source appears,
`89 89 CA` stops reading `FE`, and Windows leaves the loop and runs §3 from
step 3.

## Open questions

- Why Windows writes `v_in_source_type = 3` (C++: "PAL 4:3") for an NTSC 240p
  source. Check whether the 480i trace writes the same value.
- PAL-family 288p ↔ 576i transitions were never captured.
- Whether any of the C++/Windows discrepancies in §3 change image quality.
