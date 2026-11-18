# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite no signal; control test `dev` vs. this branch
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (5a189e2): composite showed **no signal at all** for 30 s
(`0x88:FA = ED`/`FD`, no sync), where the test before showed a stretched
picture. A fresh composite start on this branch now runs the same register
sequence as `dev`, where composite PAL worked (seventeenth test); the only
differences left are an extra read of `0x98:F5` and the family check
before START. The code does not explain this, so before changing anything
else, a control test with `dev` and the same setup, as for the restart
problem:

- `dev` composite works, this branch fails → the cause is on this branch.
- Both fail → the setup changed since the seventeenth test (cabling: the PS1
  now connected to composite and S-video at the same time, the cable, the
  VM), not the driver.

## Acceptance criteria (this test)

1. Composite with `dev`, fresh plug-in: picture or not.
2. Composite with this branch, fresh plug-in: picture or not.
3. Both with only the composite cable connected, if the first two fail.

## Hardware test (maintainer)

Test A — `dev`:

```sh
git fetch origin && git checkout --detach origin/dev
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Test B — this branch: `git checkout fix/pal-svideo && git pull`, then the
same commands (unplug and replug again before `insmod`).

Please note how the PS1 is cabled (composite only, or composite and
S-video at the same time). Paste both outputs, labelled A and B, into the
maintainer comments below.

## Findings

- 5a189e2 test: composite no sync at all (`FA = ED/FD`) on a fresh start.
- 13b1a5b test: composite broken also after a fresh plug-in; cause: the
  `F5` retrigger enabled for composite polling (b17d75e).
- 847b592 test: composite after S-video still detected as 60 Hz although
  the recorded S-video → composite sequence is written.
- a675bc8 test: composite stretched vertically (60 Hz family detected for
  the PAL source); decoder still set for S-video / progressive.
- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

