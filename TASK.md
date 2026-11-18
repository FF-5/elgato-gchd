# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** same code fails today → hardware chain check with the N64
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (7d8ed87, PS1 PAL): composite no sync (`0x88:FA = ED/FD`) with
`composite_retrigger` off and on; S-video showed the stretched picture that
composite showed before, although S-video gave a correct 576i picture in
the earlier tests.

`dev`, the exact code that captured PAL composite correctly in the
seventeenth test, fails today as well. The same code giving different
results on different days points to the hardware chain (PS1 output,
the Elgato's analog breakout cable or connector, the device's analog input)
rather than the driver. A known-good NTSC source settles it: the N64 worked
on composite and S-video with `dev` many times.

## Acceptance criteria (this test)

1. N64 on composite, `dev`, fresh plug-in: picture (480i/240p)?
2. N64 on S-video, `dev`: picture?
3. If the N64 works: PS1 on composite again after reseating the Elgato's
   analog breakout connector (unplug it from the device and plug it in
   firmly) and power-cycling the PS1.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout --detach origin/dev
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# N64 on composite; unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; N64 on S-video:
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Then point 3 with the PS1, same commands. Please include the S-video part
of the log this time. Paste the outputs into the maintainer comments below.

## Findings

- 7d8ed87 test: composite no sync with and without the switch; S-video
  stretched (it showed correct 576i before).
- One input at a time is always used (PS1 has one output): not cabling.
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

