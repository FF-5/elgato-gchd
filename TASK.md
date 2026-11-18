# TASK T-020 — MPEG-TS stream with audio

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** TS works; PS1 locks on composite and S-video; 60 Hz hold at PS1 start (33c23cf); hardware test
- **Type:** driver feature
- **Branch:** `feat/ts-audio` (from `dev`)
- **TODO items:** T-020, T-023 (stage 1)

## Why

Composite and S-video work for all NTSC and PAL modes (`dev` 0.4.1). The
driver still delivers only the H.264 video, one PES packet per buffer, and
drops the audio: the device multiplexes MPEG-1 Layer II audio (PID `0x010F`)
into its transport stream. Applications such as ffplay, ffmpeg and OBS can
use the complete transport stream directly, with sound. It is also the
input for the planned guvcview/OBS bridge (T-060).

## Implementation

1. A second pixel format on the same node: `V4L2_PIX_FMT_MPEG`, next to
   `V4L2_PIX_FMT_H264` (still the default). The selected format is kept
   while the device stays connected, like the input.
2. MPEG buffers carry 348 whole TS packets (65 424 bytes) as received from
   the device, without null packets (PID `0x1FFF`), starting with a PAT
   packet after every stream start or restart: video, audio (`0x010F`),
   PAT/PMT, PCR.
3. ffmpeg's V4L2 input does not know the TS format, so the standard way for
   encoder cards applies: select the format once with `v4l2-ctl`, then read
   the device like a file (`read()`), e.g. `ffplay -f mpegts /dev/video0`.

## Hardware test (maintainer)

Build and load (once). `git log -1 --oneline` must show 41cd5c1 or later,
and the build must be run in the same directory as `insmod`:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/ts-audio && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod ./elgato_gchd.ko
```

Composite runs:

```sh
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

S-video runs: the same with `-i 3`.

Note in your comment how you unplugged before `insmod`: cable pulled out
of the PC, or detached in the VirtualBox USB menu.

ffplay may keep running while you switch consoles or press reset.

1. Composite: start ffplay with the PS1 off, power it on. Expected: the
   ffplay window opens only once the PS1 is in 50 Hz, with the right
   aspect (`60 Hz signal with PAL colour ...; waiting up to 5000 ms` in
   dmesg, then `configuring 576i`).
2. Same session: press reset, wait; power-cycle, wait. Expected: one
   restart per reset (`decoder status changed ...`, the waiting line,
   `analog mode now 576i`). ffplay keeps its first window size, so check
   the aspect after closing and starting ffplay again while the game runs.
3. Same session: N64 instead of the PS1 (menu ↔ demo).
4. S-video: steps 1 to 3 again.

Expected: after each PS1 start or reset, `decoder status changed while
capturing (...)` within about 0.5 s of the switch to 50 Hz, then `analog
mode now 576i` (or 288p) and the picture returns about 3 s later. No such
line during a steady picture. N64 changes work as before (possibly via the
same line instead of `analog mode change detected`).

After each run:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
sudo dmesg -C
```

## Findings

- 38057b9 test: TS format works, video H.264 and MP2 audio 48 kHz stereo
  320 kbit/s, 20 s recording decodes. ffprobe showed "Program 1" with an
  empty video stream on PID `0x11` and the real video under "No Program":
  the driver's PMT (taken over from the C++) had wrong bytes. Replaced with
  the PSI tables Windows writes (`input-switching/filtered_init_switch_to_component`
  1.07-1.14 s). Decode errors only at the start and at the encoder restart
  of a 240p → 480i change.
- 1e86b93 test (S-video NTSC): video and audio inside "Program 1"; sound.
  Decode messages at the start (partial PES before the PAT) and at the
  encoder restarts. Now video and audio start with a complete PES packet
  after the PAT.
- 0d47f71 test: N64 composite and S-video, PS1 composite work in TS mode
  (PS1 S-video not in the pasted output). Decode messages at the same
  stream time in every run (dts 343080 ≈ 3.8 s, ≈ 5 s), right after
  ffplay/ffmpeg finish analysing the first ~5 MB: the driver buffer held
  only 32 entries (2 MB in TS mode, ~0.5 s in H.264 mode) and dropped
  buffers while the application set up. Raised to 512 entries (the 64 MB
  byte limit applies).
- 0d47f71 test, PS1 S-video (log pasted later): after the firmware start
  the decoder reported the 60 Hz family for ~2 s, 240p/480i was
  configured, wrong picture (sound fine); the second run caught it before
  START. Cause: the block after the encoder firmware start
  (`gchd_post_encoder_prefix`) set the decoder to composite (`0x88:02 =
  C9`) also for S-video. Now `DC` for S-video. In addition the family is
  checked while capturing: two polls with the other family → configure
  again.
- e025e9f test, PS1 S-video: the start still reads 60 Hz first (the first
  detection runs before the firmware start); corrected 0.6 s after START by
  the new check. But during steady 576i capture the decoder read 60 Hz for
  two polls once, and the driver reconfigured to 240p and back. The
  Windows PAL recordings never show `FA` with the 60 Hz family, so the
  check now needs 8 polls (2 s) and logs FA/CA/E7/F1 when it fires.
  `ring_dropped=0` throughout; the 20 s recording decodes without errors.
- Maintainer: in the first Windows recording with the PS1 the Windows
  application showed 720×480 at first and settled on 720×576 later, while
  the picture was correct throughout. In that recording
  (`composite/filtered_288p_pal_composite`) `0x88:FA` reads `1F` → `17` →
  `07` (50 Hz) before the encoder is programmed, and the encoder writes are
  PAL (`v_format = 5`, 720×576): the 720×480 was the application's label.
  If the driver still needs a restart at the start often, wait for a
  stable family reading before the first configuration.
- Unclean exit of a program: the kernel closes its file handles and the
  driver's close path (clean hardware shutdown) runs. A new open waits:
  the close holds the user lock through the shutdown, and open takes it.
- f12225e test (PS1 S-video, cable now on the PS1 output): the PS1
  locks. At power-on and after each reset the PS1 outputs 60 Hz (`FA = 06`,
  `CA = 66`, `E7 = 59`) for a few seconds, then 576i; the driver followed
  every change but needed 2 s (8 polls) plus a full redetection each time.
  ffplay reports 720×480 because the stream started at 240p. Windows does
  the same at PS1 start (`input-switching/gchd_PAL_composite_svideo_small`):
  it starts in NTSC, then restarts in PAL when `0x88:E7` gets bit 7 or
  `0x88:FA` shows the other family, with its own family-change sequence
  and a `0x88:0A` write by `E7`. Implemented in d578d60 (2 polls instead
  of 8; `docs/composite.md §8.3`, §9).

- 3dcf44a test, composite first on a fresh module (previous sessions on
  S-video), PS1 then N64 swapped in: no sync for 144 s (`0x88:FA =
  ED/FD`), shutdown on close -5. The decoder keeps its S-video setting
  while the device has power, also across module reloads (and a replug
  in the VirtualBox menu does not cut power); the driver assumed
  composite after loading and skipped the S-video → composite sequence.
  In the recording Windows also retriggers the back end once right after
  that sequence (`0x98:F5 = E2`) and composite locks 0.6 s later; the
  driver skipped that retrigger on composite. Both fixed in d2fe44f.
- Correction: the "3dcf44a composite" test above was run with input 3
  (S-video) while the cables were on composite; it says nothing about
  composite.
- 3dcf44a test on composite: N64 locked at once (480i, `ring_dropped=0`).
  PS1 in the second session (after the N64): no sync for 50 s (`0x88:FA =
  ED/FD`); shutdown on close -5. In the recording Windows retriggers the
  back end on composite too whenever `0x98:F5` has bit 7 set while it
  waits for a signal (PS1 powering up: 128.99 s, 129.34 s, lock at
  129.44 s). The driver did this on S-video only; now on both (7d08899).
  The no-signal log line now shows `0x98:F5`.
- 7d08899 test (composite and S-video; PS1 power-on, reset, power cycle,
  then N64; ffplay kept running): the PS1 locks on composite now (the
  back-end retrigger while polling ran: `0x98:F5 = F2, E2`). Each PS1
  start gave 240p first (`E7 = 59`, 60 Hz with PAL colour while the PS1
  boots), then the family change to 576i 1.5-3 s later; the driver
  followed every change. ffplay sized its window from the first 720×480
  frames and showed the later 576-line frames squeezed. One family change
  triggered during the reset blanking configured 240p and START timed out
  (-110): the source changed to 50 Hz during the setup. 33c23cf waits up
  to 5 s on such a 60 Hz signal (start and family change) and checks the
  source again before START after a restart.
---

## Maintainer comments

