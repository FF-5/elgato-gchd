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

- Works on `dev`: composite 480i and 240p from a fresh start (ffplay, H.264
  stream, clean decode). No audio. guvcview cannot display the stream.
- On `fix/mode-change-restart` (T-041): stop and restart work (close and
  reopen, stable 240p, 39f63f2). Open: mode switches while capturing. Source
  used: N64 *Star Wars Episode I: Racer* (switches 480i ↔ 240p by itself).
- ffplay prints "VIDIOC_QBUF: Bad file descriptor" on close (this branch
  only; "Some buffers are still owned by the caller" also on `dev`).
- Test setup: VirtualBox with the USB 3.0 (xHCI) controller is 5–8× faster
  than EHCI (firmware load 0.8 s instead of 6 s).
- Bring-up order: composite and S-video (all resolutions), component, HDMI.

## Next

- [ ] **T-041 [HW] (M)** Restart after STOP (prerequisite for mode changes,
      redetection and reopening). Then the 480i ↔ 240p change as recorded
      (`docs/composite.md §4`), implemented on `fix/mode-change-restart`.
- [ ] **T-043 [HW] (S)** Milestone: live 480i ↔ 240p switching keeps
      streaming; `-vf separatefields` gives a stable 240p picture.
- [ ] **T-042 [OFF] (S)** Report 240p/480i to userspace;
      `V4L2_EVENT_SOURCE_CHANGE` on change.
- [ ] **T-031 [HW] (S)** STREAMOFF → STREAMON without closing (close →
      reopen works since 39f63f2).

## Inputs and modes

- [ ] **T-052 [HW] (M)** S-video 480i and 240p (`0x88:02=DC`, `14=D2`).
- [ ] **T-053 [WIN+HW] (S)** PAL 576i/288p (composite, S-video); 288p
      detection (`CA = 7E`, apply block `28=8A`, `0A=8F`).
- [ ] **T-051 [WIN+HW] (M)** Component, all modes.
- [ ] **T-050 [WIN+HW] (M)** HDMI with a signal, all modes, mode changes.
- [ ] **T-055 [OFF→HW] (M)** Encoder output size independent of the input
      (`docs/hdmi.md §4`: 1920×1080 … 480×360, per-size bitrate/level,
      transport output 1 or 2). Maintainer report: analog inputs offer only
      480×360 and 640×480 in the Windows application; unconfirmed (the
      composite recording was encoded at 720×480).
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
- [ ] **T-012 [OFF] (S)** debugfs raw bulk-IN dump behind a module parameter.
- [ ] Remote branches to delete on GitHub (not possible from the session):
      `claude/inspiring-bardeen-xjao9v`, `fix/analog-start-per-traces`,
      `fix/buffer-timestamps`, `fix/composite-240p-start`,
      `fix/v4l2-format-negotiation`, `refactor/t001-names-and-macros`.
      Historical branches: once composite, S-video and component work on
      `dev`.

## Known differences between the driver and the recordings

Change one at a time (RULES D5), when the mode can be tested.

| Item | Driver | Recordings |
|---|---|---|
| `system_rate` | `1.075 × (bitrate + 320) + 256` (12693 at 720 wide) | `1.0728 × bitrate + 470` (12539 NTSC, 12531 PAL) |
| HDMI/component encoder and back end | C++ blocks (`65`/`64` pulses, `0900:0018 = 00 0A`, `v_vinpelclk = 0`, `time_scale_l = 0`, `num_units_in_tick = 0`, GOP 14, bitrate formula) | `05`/`15` pulses, `02 0A`, `v_vinpelclk = 1`, `time_scale_l = 60`, `num_units_in_tick = 1`, GOP 8, bitrate per output size |
| Initial analog detection | mode from one `0x88:CA` read | unknown (no recording of a 480i start) |
| Receive queue at restart | URBs queued after INIT A0, before START | queued before the first encoder write |
| Bulk IN `0x84` | required (`-ENODEV` if absent) | queued (8 URBs), never carries data |
| Repeated `0x88:02/3C`, `0x94` writes after detection | written | not written (T-011) |

## Evidence wanted (maintainer, opportunistic)

- Windows: composite 480i start, PAL 576i/288p, S-video, component and HDMI
  with a signal.
- Windows: composite 480i with the output set to 480×360 and 640×480
  (T-055; control traffic is enough).
