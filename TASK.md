# TASK T-053 — PAL 576i and 288p (composite, S-video)

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** hardware test of the current driver, and Windows recordings
- **Type:** hardware test, then implementation
- **Branch:** `feat/pal` (from `dev`)
- **TODO items:** T-053

## Why

Composite and S-video work for NTSC (480i, 240p, changes while capturing;
S-video accepted on `feat/svideo`, which had no code change). Next are the PAL
modes.

What the driver does today with a 50 Hz source (`0x88:FA` low nibble `7`):
720 × 576, always configured as **576i**, PAL back-end table, encoder
`v_format = 5`. The check for mode changes while capturing only runs for
480-line sources, so a 576i ↔ 288p change is not handled.

What is recorded (`traces/composite/filtered_288p_pal_*`,
`traces/s-video/filtered_288p_pal_*`): **288p only**. With a 288p source the
decoder reads `CA = 7E` (the same value as 480i), `E7 = 79`, `F1 = 63`.
When the source is already locked at input selection, Windows writes no
decoder apply block and no `0x88:0A`; it loads the PAL table and starts.
There is no recording of 576i, so it is unknown how 576i and 288p differ in
the status registers. That is needed for switching between them.

## Acceptance criteria (this test)

1. PAL 576i and PAL 288p, each on composite and S-video, fresh start: does
   a picture appear with the current driver?
2. Windows recordings (control traffic is enough): composite PAL **576i**
   start, and a **576i ↔ 288p** change while capturing, if the source does
   that.

## Hardware test (maintainer)

Please first say which PAL source(s) you use and which mode each outputs
(576i, 288p, or switching).

Test with the current driver (VirtualBox xHCI):

```sh
git fetch origin && git checkout feat/pal && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2      # 3 for S-video
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
```

For each source/input: note whether a picture appears, close ffplay, wait
2 s, next. A switching source: note when it switched (expect no recovery
yet).

Windows recordings: upload to `traces/composite/` (and `traces/s-video/` if
recorded), reduced to control traffic as before.

Paste the outputs into the maintainer comments below.

## Findings

- S-video (T-052): works like composite with the existing code (sixteenth
  test, `logs/hardware-test-log.md`).

---

## Maintainer comments

