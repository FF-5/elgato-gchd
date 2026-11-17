# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

The PS1 does output S-video (the Windows recordings of PAL S-video were made
with it). With the driver, the decoder never locked (`0x88:FA = ED`/`FD`).

The Windows PAL S-video start (`traces/s-video/filtered_288p_pal_svideo`
4.09-4.29) shows the difference: right after the routing writes Windows
reads `0x98:F5 = F2`/`E2` (bit 7 set) and retriggers the back end; 0.17 s
later `0x88:FA = 17` (PAL locked). The driver read `0x98:F5` in its polling
but ignored the value, so this retrigger never happened. With the N64
(NTSC) `F5` reads `62` (bit 7 clear) and no retrigger is needed, which is
why NTSC S-video worked.

Fix: whenever the driver polls an analog input while waiting for a source
(the two cycles before tuning and every detection poll), it now reads
`0x98:F5` and retriggers the back end when bit 7 is set. For PAL sources
the same check runs while capturing (seen at the start of a 576i → 288p
change in the recordings).

## Acceptance criteria

1. PS1 on S-video (input 3), fresh start: `configuring 576i` or `288p`,
   picture.
2. 576i ↔ 288p while capturing: picture continues, as on composite.
3. Composite PAL and NTSC (if at hand) still work.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Then the same with `-i 2` (composite) as a regression check. Paste the
output into the maintainer comments below.

## Findings

- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

