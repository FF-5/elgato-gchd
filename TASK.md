# TASK T-055 — Output sizes for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** small sizes give no picture; dmesg needed
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

Build and load; `git log -1 --oneline` must show b5d4a79 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/output-size && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
```

PS1 on composite, game running. Two terminals.

1. Terminal 1:

   ```sh
   sudo dmesg -C
   v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
   ffplay -f mpegts /dev/video0
   ```

2. Wait 20 s. **While ffplay still hangs**, terminal 2:

   ```sh
   sudo dmesg | grep -vE "bulk-IN data|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed" > ~/gchd-640.txt
   ps -o pid,stat,wchan:32,cmd -C ffplay >> ~/gchd-640.txt
   cat ~/gchd-640.txt
   ```

   Paste the whole output (`START poll` lines are kept on purpose).
3. Then Ctrl+C in terminal 1 (unplug only if it does not return), and
   append `sudo dmesg | tail -50`.

## Findings
- b5d4a79 test (PS1 composite): format and ENUM_FRAMESIZES correct
  (640×480 / 480×360, field None; source size 720×480 before the first
  detection in the session). With 640×480 and 480×360 ffplay never got a
  stream and could not be ended without unplugging; the standard size
  worked (720×576, 25 fps). The dmesg part of the comment was empty, so it
  is not known whether START completed or where data went; log requested.

## Maintainer comments

