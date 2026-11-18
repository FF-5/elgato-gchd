# TASK T-047 — Close and reopen without errors

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** T-047, T-031 done; T-045 application-side; T-044 reboot gap (96382eb) test
- **Type:** driver fix
- **Branch:** `fix/close-lifecycle` (from `dev` 0.5.0)
- **TODO items:** T-047, T-044, T-031, T-045 (stage 1)

## Why

`dev` 0.5.0 captures composite and S-video in all NTSC and PAL modes, with
H.264 or MPEG-TS with audio. Closing is not clean in every case:

- **T-047:** `hardware shutdown on last close failed: -5; keeping initialized
  state` after a session in which no source was found (seen in four tests
  on `feat/ts-audio`). -5 (`-EIO`) comes from a state change that finds the
  device in an unexpected state; the log does not say which step.
- **T-044:** a new open while the previous close is still running failed
  once.
- **T-031:** STREAMOFF → STREAMON without closing is untested.
- **T-045:** ffplay on close (H.264 format): "VIDIOC_QBUF: Bad file
  descriptor", "Some buffers are still owned by the caller".

## Implementation

1. Diagnostic (this build): the shutdown logs each step it runs with the
   device state before and after, and the step that fails.
2. Fix from the diagnostic: the shutdown sequence for a device that never
   started capturing.
3. T-044, T-031, T-045 after that, one at a time.

## Hardware test (maintainer)

Build and load (once); `git log -1 --oneline` must show dc1035a or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout fix/close-lifecycle && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
```

Use a build of 96382eb or later (`git log -1 --oneline`), N64 on composite,
MPEG format.

**Reopen right after closing (T-044), three times in a row:**

```sh
timeout -s INT 15 ffplay -f mpegts /dev/video0; ffplay -f mpegts /dev/video0
```

Expected each time: the second ffplay shows the picture after about 3-4 s
(dmesg: `waiting ... ms after the previous shutdown`). Close it, wait a
few seconds, repeat.

After the test, the log **with** the detection lines:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
```

## Findings

- 9e098d3 test (composite, no console, two sessions closed with Ctrl+C):
  first close failed at RESET (state `0x11` IDLE before, `0x01` STOP
  after): the session never started capture; after the shutdown sequence
  RESET found the device in STOP. The second close, from STOP, succeeded
  (`0x01` → `0x12`). dc1035a: when RESET leaves the device in STOP, IDLE and
  RESET are sent once more (the sequence that worked from STOP).
- dc1035a test: both closes after sessions without a source now end with
  "device in STOP after RESET; IDLE and RESET again" and "hardware shutdown
  done (0x0011 → 0x0012)"; closes after capture `0x0001 → 0x0012`. T-047
  fixed. Session 2 (ffplay alone, after the first close, N64 meant to be
  on) found no source for 33 s; session 3 (after `v4l2-ctl -i 2` and
  `-v pixelformat=MPEG`) and session 4 (`-v pixelformat=MPEG` and ffplay)
  showed the N64. With input 2 already selected `-i 2` changes nothing, and
  the format is kept across closes, so both commands should have no
  effect; the detection lines of session 2 were not in the filtered log.
- dc1035a reproduction: after a close from a session without a source,
  `ffplay` alone (no `v4l2-ctl`) detected the N64 (240p, then 480i during
  setup) and captured; closes `0x11 → 0x12` and `0x01 → 0x12`. The earlier
  session without detection did not recur (not reproduced; possibly the
  N64 was not on yet). **T-047 done.** The wait line called the N64's
  unsettled standard (`E7 = E9`) "60 Hz signal with PAL colour"; message
  fixed in ce4174f.
- ce4174f test: **T-031 works** (stream-restart, 3 rounds of STREAMON /
  100 buffers / STREAMOFF on one handle; first buffer after 2.66 s, then
  0.13 s). **T-045 is application-side:** "Some buffers are still owned by
  the caller on close" and "ioctl(VIDIOC_QBUF): Bad file descriptor" are
  printed by ffmpeg's V4L2 input, which closes the file while it still
  holds buffers and returns them afterwards. **T-044:** an open 0.12 s
  after the previous shutdown booted but found no source for 39 s (no
  back-end retrigger at the start, unlike every other session); opens
  1.6 s and later after a shutdown worked. 96382eb: a boot waits until 1.5 s
  after the last shutdown. (My log filter hid the detection lines of that
  session; the new command keeps them.)

## Maintainer comments

