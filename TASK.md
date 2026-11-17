# TASK T-053 — PAL 576i and 288p (composite, S-video)

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite PAL accepted; S-video PAL: no signal from the PS1, to be checked
- **Type:** hardware test, then implementation
- **Branch:** `feat/pal` (from `dev`)
- **TODO items:** T-053

## Why

Last test (PS1, PAL): **composite works** — 576i start, 20 changes
576i ↔ 288p while capturing (~0.5 s each, decoder only, picture continues),
close and reopen.

**S-video showed no signal**, also after unplugging and reloading the
module: the decoder reported `0x88:FA = ED`/`FD` and `CA = 6E`/`EE` the
whole time, its state with no sync at all (the same as composite with
nothing connected). The driver's S-video setup is the one recorded for a
PAL S-video start (`0x88:02 = DC`, `0x88:14 = D2`), and S-video worked with
the N64. The most likely cause is that the PS1/cable does not deliver an
S-video signal; this test checks that.

## Acceptance criteria (this test)

1. Does the PS1 give a picture on S-video with the Windows software?
2. If yes: a Windows recording of that S-video start (reduced as usual) and
   the Linux driver test again; if no: S-video PAL is checked later with
   another PAL source, and `feat/pal` is merged with composite PAL.

## Hardware test (maintainer)

1. Windows: PS1 on S-video, start capture in the Windows software. Picture?
   If yes, record the start (input selection to first picture), reduce with
   `tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`,
   upload to `traces/s-video/`.
2. Optionally, another PAL source with S-video, on Linux:

```sh
git fetch origin && git checkout feat/pal && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the result into the maintainer comments below.

## Findings

- 1bb95d5 test: composite PAL 576i/288p and changes work; S-video with the
  PS1: no sync (`0x88:FA = ED/FD`).
- PAL with the previous driver (maintainer): bottom quarter cropped, green
  bars at the bottom; second ffplay start not always working.
- PAL recordings: no apply block at a 576i start; 576i ↔ 288p decoder-only;
  `CA` codes as NTSC; PAL apply values `9A 90 A8`.
- S-video (T-052): works like composite with the existing code (sixteenth
  test, `logs/hardware-test-log.md`).

---

## Maintainer comments

