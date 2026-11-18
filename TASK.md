# TASK T-046 — Format reporting for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** compliance fixes confirmed; S_INPUT busy, no PAL-60 (fd8426b); hardware test
- **Type:** driver feature (V4L2 reporting)
- **Branch:** `feat/format-reporting` (from `dev` 0.5.1)
- **TODO items:** T-046, T-022, T-042 (stage 1)

## Why

Applications learn the source format from V4L2 before and while capturing.
Today:

- **T-046:** before the first detection the frame rate is the NTSC value;
  ffplay showed 60 fps for a PAL source on the first open. G_PARM reports
  the field rate as an integer (60/1, 50/1), while the encoded stream has
  29.97 (NTSC) or 25 (PAL) frames per second (ffprobe of every recording).
- **T-022:** field order: analog sources are always encoded interlaced
  (`top first` in ffprobe), also 240p/288p; G_FMT reports
  `V4L2_FIELD_INTERLACED`. To be checked against `V4L2_FIELD_INTERLACED_TB`
  / `_BT` for NTSC and PAL.
- **T-042:** the current analog mode (NTSC 480i/240p, PAL 576i/288p, PAL
  60) is visible only in dmesg.

## Implementation (plan)

1. Frame rate: G_PARM and ENUM_FRAMEINTERVALS report the encoded frame
   rate, 30000/1001 for the 60 Hz family and 25/1 for the 50 Hz family.
   Before detection: the family last detected on this input while the
   module is loaded (NTSC on first use).
2. Field order from the stream (`top first` for both families per
   ffprobe); G_FMT and the buffer `field` agree.
3. Analog standard: `VIDIOC_QUERYSTD`, `VIDIOC_G_STD` and the `std` field
   of ENUMINPUT for inputs 2 and 3 (`V4L2_STD_NTSC_M`, `V4L2_STD_PAL`,
   `V4L2_STD_PAL_60` for 60 Hz with PAL colour); ENUMINPUT `status`
   `V4L2_IN_ST_NO_SIGNAL` while no source is detected. 240p/288p versus
   480i/576i in the input status log and the source-change event.

## Hardware test (maintainer)

Build and load; `git log -1 --oneline` must show fd8426b or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/format-reporting && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
```

The report command used below (paste its output each time):

```sh
v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
```

1. Composite (`-i 2`), MPEG format, PS1 running: `ffplay -f mpegts
   /dev/video0`.
2. Second terminal while it plays: `v4l2-ctl -d /dev/video0 -i 3`.
   Expected: "VIDIOC_S_INPUT: failed: Device or resource busy"; ffplay
   keeps playing. Then the report command: PAL, 720×576, 25 fps.
3. Second terminal: `v4l2-compliance -d /dev/video0`. Expected: ffplay
   keeps playing; in dmesg no "input switch requested" lines with
   `streaming=1` from the S_INPUT test (the input menu control may still
   switch; TODO T-066). Paste the summary line.
4. Close ffplay; power the PS1 off and on while no capture runs; start
   ffplay: expected 576i after the wait; report: PAL.

dmesg after the test:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
```

## Findings
- 4a51a75 test (composite, N64 then PS1): before capture NTSC-M, 720×480,
  Interlaced Top-Bottom, 29.97 fps, detected standard none, as expected;
  HDMI: G_STD/QUERYSTD "No data available" as intended. After the PS1
  session: PAL, 25 fps, but 720×480: the size was only updated while no
  buffers were allocated, and ffplay allocates them before detection.
  77ee6ed: G_FMT of composite and S-video reports the current source size
  (also while capturing), else that of the last detected standard. The
  report while ffplay played (step 2) was not in the comments.
- 77ee6ed test: **format reporting works.** Second terminal while
  capturing: PS1 "Composite: ok", PAL detected, 720×576, 25 fps; N64
  NTSC-M detected, 720×480, 29.97 fps. Before and after capture: the last
  standard with "no signal". (I first overlooked these lines at the end
  of the comments.)
- v4l2-compliance 1.26.1 (77ee6ed): fixed in 4c326eb: bus_info without the
  `usb-` path, standards enumerable on HDMI/component, `readbuffers` 0.
  Remaining (older, TODO T-066): DV timings on HDMI/component without a
  signal, `DV_TIMINGS_CAP`, a menu control rejecting item 0,
  `DV_RX_POWER_PRESENT` missing.
- Maintainer question, PAL-60: Elgato lists PAL, NTSC and SECAM for this
  device and does not mention PAL-60. The device captures it anyway (the
  PS1 sends it while booting; it is encoded as a 60 Hz 480-line picture),
  so the driver reports it as `V4L2_STD_PAL_60` when the decoder reports
  60 Hz with PAL colour. SECAM is not reported: no SECAM source tested.
- 4c326eb test: v4l2-compliance without capture 8 failures, all on the
  T-066 list; QUERYCAP (bus_info `usb-0000:00:0c.0-2`), STD and G/S_PARM
  pass. During capture (second terminal) compliance switched through all
  inputs and each switch stopped and restarted the running capture;
  fd8426b: S_INPUT to another input returns EBUSY for a file handle that does
  not own the buffers.
- PAL-60 (maintainer: probably a driver timing issue; source wanted): no
  source found that documents a PAL PS1 sending 60 Hz at start-up. The
  decoder is fully locked during it (`FA = 6`, `CA = 66`, `E7 = 59` for
  3-4 s) and the Windows recording reads the same at the PS1 start
  (`input-switching/gchd_PAL_composite_svideo_small` 10.70 s: `FA=16
  CA=66 E7=59`; Windows starts NTSC 240p there). Maintainer decision
  followed: PAL-60 is not reported; 60 Hz sources report NTSC-M (fd8426b).

## Maintainer comments

