# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite after S-video still stretched; diagnosis (register dump, recording)
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (847b592): S-video works (the family check before START caught a
brief NTSC reading and started 576i). **Composite after S-video is still
stretched:** the decoder keeps reporting the 60 Hz family (NTSC) for the PAL
PS1, and CA flips between 66 and 7E every few seconds, although the driver
now writes the full recorded S-video → composite sequence. After a fresh
plug-in, composite PAL worked (seventeenth test).

So S-video leaves some other decoder state behind that the recorded switch
does not reset. The only recording of that switch was made without a
source, so it cannot show what Windows does with a signal. Two ways to find
it:

1. **Decoder register dump:** new module parameter `dec_dump=1` logs the
   decoder registers `0x88:00`–`7F` right before analog detection. A dump
   of a working composite start (fresh plug-in) and of a failing one (after
   S-video) shows which registers differ.
2. **Windows recording** of the PS1 on S-video, switched to composite while
   running.

## Acceptance criteria (this test)

1. Two dumps: composite after a fresh plug-in, and composite after S-video.
2. If possible, the Windows recording.

## Hardware test (maintainer)

PS1 connected to composite and S-video, as before.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko dec_dump=1
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite: picture OK?
# close, wait until ffplay has exited
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0      # S-video, wait for a 288p change
# close, wait
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0      # composite again: stretched?
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

The important lines are `dec 00:` … `dec 70:` (eight per session).

Windows recording (if possible): PS1 on S-video, capture running, then
select composite in the Windows software and wait for the picture. Reduce
with
`tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`
and upload to `traces/input-switching/`.

Paste the output into the maintainer comments below.

## Findings

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

