# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** scaled output via switch from standard (1b1b8d0); hardware test
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

Build and load; `git log -1 --oneline` must show 1b1b8d0 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
```

PS1 on composite, game running. For each size: set it, start ffplay,
wait 15 s, close (Ctrl+C):

```sh
sudo dmesg -C
v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

1. 640 × 480 (above), 2. 480 × 360 (`width=480,height=360`),
3. standard (`width=720,height=576`).

Expected: dmesg "switching output size: 0 -> 1" (or 2) about 0.3 s after
the first START, then "analog mode now 576i, output 1"; ffplay's
`Stream #0:0` line shows 640x480 / 480x360. If ffplay hangs, take the log
in a second terminal before Ctrl+C. Paste the ffplay stream lines and the
complete output of:

```sh
sudo dmesg | grep -vE "bulk-IN data|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|no analog family|pre-config raw_status|retrying detection"
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

## Maintainer comments

