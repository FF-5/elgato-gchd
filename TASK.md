# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** prototype (`service/gchd-bridge`); first hardware test
- **Type:** new userspace component (service) next to the driver
- **Branch:** `feat/app-bridge` (from `dev` 0.5.8)
- **TODO items:** T-060 (stage 1b)

## Why

The driver delivers H.264 or an MPEG transport stream (H.264 + MP2 audio).
guvcview, OBS's V4L2 source, browsers and video-call programs expect a
V4L2 device with uncompressed frames (YUYV/NV12) and a separate audio
source. Decoding is not allowed in the kernel (RULES E2), so a small
userspace service turns the driver's stream into such a device.

## Goal (acceptance criteria, draft)

1. A second video device, named "Elgato Game Capture HD", offers
   uncompressed frames (YUYV and/or NV12) at a fixed size for the session,
   deinterlaced (480i/576i) or line-doubled (240p/288p).
2. An audio source of the same name in PipeWire/PulseAudio, in sync with
   the picture.
3. The service starts when an application opens the device and releases
   the capture device when no application uses it (or after a delay).
4. A source change (NTSC ↔ PAL, 480i ↔ 240p, console swap) does not change
   the format the applications see; the picture is scaled and the aspect
   ratio kept.
5. The input can be selected without `v4l2-ctl` (the driver's menu control
   was removed in T-066).
6. Works in guvcview and OBS (V4L2 source and audio capture) with PS1 (PAL)
   and N64 (NTSC) on composite; installed by `install-driver.sh`, removed by
   `--uninstall`.

## Decisions (maintainer, 2026-11-18)

1. Language: the fastest in the end; shell + ffmpeg for now, kept modular
   so parts can be replaced (ffmpeg does the decoding in C either way).
2. The capture device runs only while an application uses it (the device
   gets hot when it runs without need).
3. Output size: the size of the source. A v4l2loopback device keeps its
   format while an application has it open, so the size is taken when an
   application opens it; a source change while it is open (NTSC ↔ PAL) is
   scaled to that size until the application reopens. To be checked:
   whether OBS/guvcview can be made to reopen by themselves.
4. Folder: `service/`.

## Prototype (`service/gchd-bridge`)

- `gchd-bridge run`: finds the driver's device and the v4l2loopback
  device (by driver, label "Elgato Game Capture HD"), creates a null sink
  and an audio source "Elgato Game Capture HD" (`pactl`).
- Idle: a black YUYV picture of the current source size at 5 frames/s,
  so applications can list and open the device; the capture device stays
  off. The idle size follows the source (checked every 5 s).
- An application opens the loopback device (`fuser`, 2 checks 1 s apart):
  the black picture stops; ffmpeg reads the transport stream of the
  driver, deinterlaces (`bwdif`, one frame per field: 50/60 frames/s),
  scales to the opened size, writes YUYV to the loopback device and the
  audio to the null sink. `keep_format=1` on the loopback device keeps
  the format during the switch.
- Last application closed: ffmpeg stops, the driver shuts the device down,
  the black picture returns.
- `gchd-bridge input composite|svideo|component|hdmi`: stops the capture,
  selects the input, the capture restarts with the same size.
- Offline check: an interlaced 576i → 480i transport stream through the
  same filter chain gives continuous 720×576 YUYV frames.

Next steps after the test: systemd user service (start at login),
`install-driver.sh` integration (v4l2loopback options in `modprobe.d`,
service install), documentation (`docs/`), input selection from the
desktop.

## Hardware test (maintainer)

Needs `v4l2loopback-dkms`, guvcview and OBS. PS1 (PAL) on composite, the
N64 (NTSC) for the source change. The test loads v4l2loopback as
`/dev/video42`; if it is loaded already with other options it is
reloaded.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-060.sh > tests/T-060.sh
bash gchd-test.sh T-060 --branch feat/app-bridge
```

Paste the summary; on FAILs the log too (the bridge's own messages are
at the end of it).

## Findings

- T-066 accepted: gchd-test.sh T-066 6/6 PASS on 978d3fc
  (`v4l2-compliance` 0 failures; DV timings on HDMI without a signal; DV
  timings list; power-present 0; PAL composite 720×576 capture; input
  change from another process refused). Merged as 0.5.8.

## Maintainer comments

