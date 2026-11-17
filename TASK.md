# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** S-video PAL works; input change S-video → composite fixed, waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (b17d75e): **PAL S-video works** with the PS1: 576i start, three
576i ↔ 288p changes while capturing. Two things remained:

1. **Composite after S-video gave no picture until the device was
   unplugged.** The analog decoder keeps its S-video setting
   (`0x88:02 = DC`) until it loses power. The S-video → composite step
   (`0x88:02 = C9`, `0x88:14 = D2`) depended on the "previous input", which
   is reset to HDMI on every open, so after close → `v4l2-ctl -i 2` → open
   it was skipped and the decoder never locked. The driver now remembers
   the decoder's setting for as long as the device is plugged in.
2. **At the start the decoder briefly reported the NTSC family** for the PAL
   source and the driver configured 240p; the check before START caught it
   but logged "480i". The check now also compares the family (NTSC/PAL) and
   logs the correct mode.

## Acceptance criteria

1. S-video (PS1): picture; 576i ↔ 288p changes.
2. Close ffplay, `v4l2-ctl -i 2`, ffplay: composite picture without
   unplugging. Then back to S-video (`-i 3`): picture.
3. Then this branch is merged and component (`feat/component`) follows.

## Hardware test (maintainer)

PS1 connected to both composite and S-video.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close ffplay, wait until it has exited, then:
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close, wait, then -i 3 and ffplay once more
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the output into the maintainer comments below.

## Findings

- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

