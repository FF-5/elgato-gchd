# TASK T-060 — guvcview, OBS and similar applications

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** analysis; maintainer decisions needed (see "Open questions")
- **Type:** new userspace component (service) next to the driver
- **Branch:** `feat/app-bridge` (from `dev` 0.5.8), created when the
  decisions are made
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

## Plan (draft)

1. Building blocks: `v4l2loopback` (DKMS package of the distributions,
   `exclusive_caps=1`, `card_label`), ffmpeg (or GStreamer) for decoding,
   deinterlacing (`yadif`/`bwdif`) and scaling, a PipeWire/PulseAudio
   pipe or null sink for the audio.
2. Prototype as a shell script with ffmpeg: driver TS node → decode →
   v4l2loopback + audio; measure latency and CPU load.
3. Start on demand: systemd user service or udev rule; detection of
   an application opening the loopback device (v4l2loopback events, or the
   service always running while the Elgato is plugged in).
4. Input selection: a small command, a desktop entry, or a control on the
   loopback device.
5. Installer integration and documentation (`docs/`).

## Open questions (maintainer)

1. Language of the service: shell + ffmpeg (simplest, depends on the
   ffmpeg command line), Python + GStreamer, or C with libav*. Proposal:
   start with shell + ffmpeg, change only if latency or the start/stop
   behaviour needs it.
2. Always running while the device is plugged in (simple, the device and
   the encoder run all the time), or only while an application reads the
   loopback device (more work, depends on v4l2loopback)? Proposal: on
   demand.
3. Output size of the loopback device: 1280×720 (fits the later HDMI
   modes, scaled up from SD) or the source size 720×480/576 (no
   scaling, but the size depends on the first source)? Proposal: 1280×720
   now, configurable.
4. Folder name in the repository (proposal: `service/`).

## Hardware test (maintainer)

Written with the implementation.

## Findings

- T-066 accepted: gchd-test.sh T-066 6/6 PASS on 978d3fc
  (`v4l2-compliance` 0 failures; DV timings on HDMI without a signal; DV
  timings list; power-present 0; PAL composite 720×576 capture; input
  change from another process refused). Merged as 0.5.8.

## Maintainer comments

