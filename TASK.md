# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** scaled outputs work (out2_test=5); final version 618a117; hardware test
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

Build and load; `git log -1 --oneline` must show 618a117 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
```

Fresh plug-in, then (no parameter any more):

```sh
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo insmod ./elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
```

PS1 on composite, game running. Each size: set it, ffplay 15 s, close.

1. `v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG`,
   `ffplay -f mpegts /dev/video0`
2. `width=480,height=360`
3. `width=720,height=576` (standard; check that it still works)
4. 640 × 480 again, then the N64 on composite (60 Hz source) with the
   same ffplay.
5. Optional: S-video with 480 × 360.

Expected: pictures in all; ffplay shows 640x480 / 480x360 progressive,
720x576 interlaced. Paste the ffplay `Stream #0:0` lines and:

```sh
sudo dmesg | grep -E "switching|START (transition|failed)|state-change|SCMD timeout|completed 10|analog mode now|first data|stats|shutdown" | head -60
```

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
- c2574b9 `out2_test` runs: **5 works** (640×480 progressive 25 fps,
  data on endpoint 0x84); 6 corrupt (both endpoints mixed, but START
  completed); 0 START never completes; 4 and 1 failed earlier, at the
  firmware start-up (TODO T-067). Decisive: output 2 enabled in the
  encoder setup after the firmware INIT, as Windows does. 618a117 makes this
  the default, `v_vinpelclk` 1 as recorded, output 2 read from 0x84.

## Maintainer comments

