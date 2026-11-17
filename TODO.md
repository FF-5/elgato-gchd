# TODO — roadmap

**Development-only file** (never merged to `master`, RULES B2).

Goal: a DKMS/V4L2 driver (C, `dkms/`) that behaves like the Windows driver for
every input and mode the hardware supports. Our own USB recordings are the
reference (`traces` branch); `src/` is read-only. Each open item becomes one
`TASK.md` and one feature branch off `dev`.

Legend: **[HW]** maintainer's hardware, **[WIN]** new Windows recording,
**[OFF]** offline, **(S/M/L)** size. Test results: `logs/hardware-test-log.md`
on the `traces` branch.

## Current state

- Works on `dev` (0.4.0): composite NTSC 480i/240p and PAL 576i/288p,
  S-video NTSC 480i/240p, from a fresh start and with mode changes while
  capturing (NTSC ~1.5 s with encoder restart, PAL ~0.5 s decoder-only),
  source off/on, close and reopen. ffplay, H.264 stream. No audio. guvcview
  cannot display the stream. Sources used: N64 (NTSC, menu 480i / demo
  240p), PS1 (PAL, intro 576i / 288p).
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller (5–8× faster
  than EHCI). Performance to be judged on non-virtualised hardware.
- Bring-up order: composite and S-video (all resolutions), component, HDMI.

## Next

- [ ] **T-051 [WIN+HW] (M)** Component, all modes (480i, 480p, 576i, 576p,
      720p, 1080i; mode changes). The driver uses the C++ blocks; no
      recording of component with a signal yet.
- [ ] **T-053b [HW] (S)** PAL S-video: works since b17d75e (`0x98:F5`
      bit 7 retrigger). Open on `fix/pal-svideo`: composite after S-video
      without replugging (decoder kept `0x88:02 = DC`; fixed, test pending).
      288p capture start is still not recorded.
- [ ] **T-047 [HW] (S)** `hardware shutdown on last close failed: -5` when
      ffplay is closed after a session in which no source was detected.
- [ ] **T-005b [OFF] (S)** Code comments still name `configureDevice()`
      (`input.c`, `usb.c`, RULES E10); comments only, verify code-identical.
- [ ] **T-046 [OFF] (S)** The frame rate reported before the source is
      detected is the NTSC default: ffplay showed 60 fps for a PAL stream on
      the first open after loading the module (50 fps on the second).
- [ ] **T-044 [HW] (S)** Reopening right after closing failed once while the
      old ffplay process was still exiting; the stop on close takes ~1 s.
      Make a new open wait for the running stop.
- [ ] **T-045 [OFF] (S)** ffplay on close: "VIDIOC_QBUF: Bad file
      descriptor" (since the close-path change) and "Some buffers are still
      owned by the caller" (also before). Check buffer return on
      STREAMOFF/close.
- [ ] **T-042 [OFF] (S)** Report 240p/480i to userspace (the
      `V4L2_EVENT_SOURCE_CHANGE` on change is sent).
- [ ] **T-031 [HW] (S)** STREAMOFF → STREAMON without closing.

## Inputs and modes

- [ ] **T-050 [WIN+HW] (M)** HDMI with a signal, all modes, mode changes.
- [ ] **T-055 [OFF→HW] (M)** Encoder output size independent of the input
      (`docs/hdmi.md §4`: 1920×1080 … 480×360, per-size bitrate/level,
      transport output 1 or 2). Maintainer report: analog inputs offer only
      480×360 and 640×480 in the Windows application. Recorded for analog
      sources (`docs/composite.md §10`): 640×480 and 480×360 use
      `v_ip_format 2`, GOP 6 and transport output 2; HD sizes and cropping
      cause no control transfers (bulk OUT `0x02` not in the reduced
      recordings; check with the filter below); bitrate slider and
      "stretch" (`v_in_source_type 2`) are encoder fields.
- [ ] **T-054 [HW] (S)** Source loss and return.

## Data path and usability

- [ ] **T-020 [OFF→HW] (M)** MPEG-TS passthrough (`V4L2_PIX_FMT_MPEG`)
      instead of PES/H.264 extraction; robust TS resync.
- [ ] **T-023 [OFF→HW] (M)** Audio (MPEG-1 Layer II, PID `0x010F`): comes
      with T-020; PCM for an ALSA device only via the userspace bridge (T-060).
- [ ] **T-022 [OFF] (S)** `V4L2_FIELD_INTERLACED` for 480i/576i/240p.
- [ ] **T-060 (M)** Userspace bridge for guvcview/OBS: ffmpeg →
      v4l2loopback, `separatefields` for 240p, deinterlace for 480i.
- [ ] **T-061 (M)** V4L2 controls for bitrate, GOP, profile, audio bitrate.
- [ ] **T-062 (S)** Original-HD family (`0044/004e/0051`): needs a tester.
- [ ] **T-063 (S)** Versioning and `CHANGELOG.md` for `master` releases.

## Code and tooling

- [ ] **T-002 [OFF] (M)** File split ("Planned layout" in `dkms/README.md`),
      fold the `*_exact` wrappers; each move checked with
      `compare-mailbox-ops.py`.
- [ ] **T-003 [OFF] (S)** Make `vb2_fops` static.
- [ ] **T-004 [OFF] (S)** `0x1004` "stout_mode": the defaults write bit 0;
      check against the initialization recording.
- [ ] **T-010 [OFF] (M)** usbmon decoder on the `tools` branch, same output
      as the pcapng decoder, to diff driver traffic against Windows.
- [ ] **T-011 [OFF] (M)** Remaining recording analysis: initialization
      (`filtered_init_switch_to_component`), input switching, repeated
      `0x88:02/3C` and `0x94` writes after detection.
- [ ] **T-012 [OFF] (S)** debugfs raw bulk-IN dump behind a module parameter
      (reference: `tools/archive/debug-trace.c` on the `tools` branch).
- [ ] Branches to delete on GitHub (deletion is refused from the
      development session). Composite and S-video are done, so all of them
      can go; the recording and logs on them were copied to the `traces`
      branch, `debug`'s `trace.c` to `tools/archive/`:
      `fix/mode-change-restart`, `feat/svideo`, `feat/pal`,
      `async-bulk-in-queue`, `cherry3`, `debug`, `debug-saved-enable-state`,
      `diagnostic-input-baseline`, `state-machine`, `v4l2-dkms-driver`,
      `v4l2-h264-access-units`, `video-output-lazy-init`.

## Known differences between the driver and the recordings

Change one at a time (RULES D5), when the mode can be tested.

| Item | Driver | Recordings |
|---|---|---|
| `system_rate` | `1.075 × (bitrate + 320) + 256` (12693 at 720 wide) | `1.0728 × bitrate + 470` (12539 NTSC, 12531 PAL) |
| HDMI/component encoder and back end | C++ blocks (`65`/`64` pulses, `0900:0018 = 00 0A`, `v_vinpelclk = 0`, `time_scale_l = 0`, `num_units_in_tick = 0`, GOP 14, bitrate formula) | `05`/`15` pulses, `02 0A`, `v_vinpelclk = 1`, `time_scale_l = 60`, `num_units_in_tick = 1`, GOP 8, bitrate per output size |
| Initial analog detection | waits until `0x88:CA` reads `7E` or `66` | unknown (no recording of a 480i start) |
| Receive queue at restart | URBs queued after INIT A0, before START | queued before the first encoder write |
| Bulk IN `0x84` | required (`-ENODEV` if absent) | queued (8 URBs), never carries data |
| Repeated `0x88:02/3C`, `0x94` writes after detection | written | not written (T-011) |

## Evidence wanted (maintainer, opportunistic)

Reduce recordings with a filter that keeps bulk OUT (only the incoming
stream on `0x81`/`0x84` is large):
`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`

- Cropping on/off and an HD output size with this filter, to confirm that
  nothing is sent on bulk OUT `0x02`.

- Windows: composite 480i start, PAL 576i/288p, S-video, component and HDMI
  with a signal.
- Windows: composite 480i with the output set to 480×360 and 640×480
  (T-055; control traffic is enough).
