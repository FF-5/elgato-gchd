# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** output-2 START does not complete; test variants (c2574b9)
- **Type:** driver feature
- **Branch:** `feat/output-size` (from `dev` 0.5.2)
- **TODO items:** T-055 (stage 1)

## Why

The capture application offers three output sizes for composite and
S-video; the driver always encodes the source size (720 × 480 / 576).
Recorded (`docs/composite.md §10`, recordings in `traces/composite/`:
`gchd_288p_output_switch_small`, `gchd_288p_standard_640x480_output_small`,
`gchd_288p_mobile_bitrate_change_small`):

| Output | Encoder fields |
|---|---|
| Standard | source size, `v_ip_format = 0`, GOP 12, transport output 1 |
| Standard 640 × 480 | `v_ip_format = 2`, GOP 6, level 30, transport output 2 |
| Mobile 480 × 360 | `v_ip_format = 2`, GOP 6, VBR, level 22, transport output 2 |

## Implementation (plan)

1. Analysis of the three recordings: complete encoder field sets per size
   (NTSC and PAL where recorded), the restart sequence, and the bulk-IN
   data of transport output 2 (PIDs, endpoint).
2. V4L2: `S_FMT` with 640 × 480 or 480 × 360 on composite/S-video selects
   the output size (encoder restart while capturing, as recorded);
   720 × source lines selects the standard output. `ENUM_FRAMESIZES` lists
   the three. `G_FMT` reports the selected output size. Kept while the
   device stays connected, like the input and the pixel format.
3. Docs: `video-output.md`, `composite.md §10`.

## Hardware test (maintainer)

Build and load; `git log -1 --oneline` must show c2574b9 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
```

A failed output-2 START leaves the device unusable until it is unplugged,
so each run starts with a fresh plug-in. Run with `out2_test` = **4**,
**5**, **6**, **1**, **0** in this order and stop at the first that shows a
picture:

```sh
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then ONE of these lines, in this order
# for the successive runs:
sudo insmod ./elgato_gchd.ko out2_test=4
sudo insmod ./elgato_gchd.ko out2_test=5
sudo insmod ./elgato_gchd.ko out2_test=6
sudo insmod ./elgato_gchd.ko out2_test=1
sudo insmod ./elgato_gchd.ko out2_test=0
# then:
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

PS1 on composite, game running. Wait 15 s. Note whether a picture comes,
then Ctrl+C (if ffplay hangs, unplug). After each run:

```sh
sudo dmesg | grep -E "out2_test|switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|ep84|first data|stats" | head -40
```

Paste per run: N, picture yes/no, the ffplay `Stream #0:0` line (if any)
and the dmesg lines.

## Findings
- b5d4a79 test (PS1 composite): format and ENUM_FRAMESIZES correct
  (640×480 / 480×360, field None; source size 720×480 before the first
  detection in the session). With 640×480 and 480×360 ffplay never got a
  stream and could not be ended without unplugging; the standard size
  worked (720×576, 25 fps). (Correction: the dmesg was there; I overlooked
  it.) It shows START refused by the device with the output-2 block
  (`SCMD timeout ... readback=0x0400`, twice), then a completion wait that
  never ended (the hang). Windows enters output 2 only from a running
  capture (all recordings); 1b1b8d0 does the same and bounds the wait.
- 1b1b8d0 test: the standard START completed, the switch to 640×480 /
  480×360 ran as recorded, but that START never completed (state stays
  STOP, 0.65 s; Windows 0.38 s). The next standard START failed too, the
  third attempt hung; unplug needed. Windows sends nothing else between
  NULL and START. Remaining differences made selectable (c2574b9, `out2_test`
  bits): `v_vinpelclk` 1, output 1 kept on, output 2 enabled in the
  encoder setup after the firmware INIT (as in the initialization
  recording).

## Maintainer comments

