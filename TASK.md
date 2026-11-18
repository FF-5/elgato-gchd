# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** `dev` fails too → setup check (composite cable only)
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Control test: **`dev` fails with PAL composite exactly like this branch.**
`dev` is the code of the seventeenth test, in which PAL composite worked
(merged unchanged from `feat/pal` e79f660). Same code, different result:
the cause is outside the driver.

The decoder never saw a usable signal: `0x88:FA` low nibble `5` at first (a
value never seen before, in no test and no recording), then `D` (no sync).
Everything that went wrong with composite since then (stretched picture,
60 Hz family, no sync) started after the PS1 was connected to composite and
S-video at the same time.

## Acceptance criteria (this test)

1. With **only the composite cable** connected to the PS1 (S-video
   unplugged at the device and at the console), fresh plug-in, `dev`:
   picture?
2. Which cable(s) are used: one cable with both composite and S-video
   plugs, or two cables? Does the PS1 show a picture on a TV through the
   composite plug at the same time?

## Hardware test (maintainer)

```sh
git fetch origin && git checkout --detach origin/dev
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# PS1 on composite only; unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

If composite works this way, the same with `fix/pal-svideo`
(`git checkout fix/pal-svideo && git pull`), still composite only.
Paste the output into the maintainer comments below.

## Findings

- Control test: `dev` fails with PAL composite like this branch (`FA` low
  nibble `5`, then `D`).
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

