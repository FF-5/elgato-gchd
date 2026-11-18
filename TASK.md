# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite regression found (retrigger on all inputs); waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (13b1a5b, fresh plug-in): **composite PAL broken even on a fresh
start** (stretched, NTSC family detected, old frames flashing), although
it worked on `feat/pal`. The register dump was useless: the decoder's
configuration registers all read back as `6E`.

What changed for composite on this branch: b17d75e enabled the back-end
retrigger on `0x98:F5` bit 7 for **all** analog inputs while polling. The
recordings show it only for S-video; during composite polling Windows
always reads `F5 = 62` and never retriggers. The a675bc8 test, the first
one with that retrigger, already showed stretched composite. In addition,
847b592 made the first composite start after loading the module use the
S-video → composite sequence instead of the recorded normal start.

Now: the polling retrigger only on S-video; the S-video → composite
sequence only after S-video was used in the same session; a fresh
composite start is the same as on `feat/pal` again (plus a read of
`0x98:F5` and the family check before START). `dec_dump` removed.

## Acceptance criteria

1. Composite PAL after a fresh plug-in: correct proportions, `configuring
   576i`/`288p`.
2. S-video PAL: picture.
3. Composite after S-video (no replug): correct proportions.

## Hardware test (maintainer)

PS1 connected to composite and S-video.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite
# close, wait until ffplay has exited
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0      # S-video
# close, wait
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the output into the maintainer comments below.

## Findings

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

