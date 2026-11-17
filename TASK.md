# TASK T-053 — PAL 576i and 288p (composite, S-video)

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** PAL implemented from the recordings; waiting for the hardware test
- **Type:** hardware test, then implementation
- **Branch:** `feat/pal` (from `dev`)
- **TODO items:** T-053

## Why

Analysis of the new recordings (details: `traces` branch,
`traces/ANALYSIS-2026-11-17.md`, "Upload 5"):

1. **PAL capture start (576i):** Windows writes **no** decoder apply block.
   The driver wrote it with the NTSC values (`0x88:07 = 8A, 08 = 9B,
   09 = 7A`), most likely the cause of the missing bottom quarter and the
   green bars. Fixed: no apply block for 576i.
2. **576i and 288p are told apart like NTSC:** `0x88:CA = 7E` 576i,
   `66` 288p (`76`/`6E` right after a change). The driver now detects 288p
   (before, every PAL source was treated as 576i).
3. **PAL 576i ↔ 288p while capturing is decoder-only** (no encoder restart,
   unlike NTSC): apply block with the PAL values `07 = 9A, 08 = 90,
   09 = A8`, `28 = 88` (576i) or `8A` (288p), then one back-end retrigger.
   Implemented; the stream keeps running.
4. Older recordings were partly mislabelled: the "288p PAL" ones are 576i,
   the "288p PAL noinput" ones are NTSC 240p (one is a copy of the 240p file).
5. Application settings (bitrate, Mobile 480×360, 640×480, stretch,
   cropping, HD sizes): documented in `docs/composite.md §10`; cropping and
   the HD sizes are done by the application on the PC. Implementing these
   as V4L2 controls is a separate task (TODO T-055, T-061).

The second ffplay start that did not always work is most likely TODO T-044
(reopening while the previous stop is still running): wait until the old
ffplay process has exited before starting it again.

## Acceptance criteria (this test)

1. PAL 576i and 288p, composite and S-video, fresh start: full picture
   (no cropped bottom, no green bars); dmesg shows `configuring 576i` or
   `configuring 288p` as appropriate.
2. 576i ↔ 288p while capturing: `analog mode change detected: 576i -> 288p`
   (and back), `analog mode now …`, picture continues.
3. Close and reopen (after the old ffplay process has exited) works.

## Hardware test (maintainer)

VirtualBox with the USB 3.0 (xHCI) controller.

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

Per source and input: picture complete? Mode switches: picture continues?
Paste the outputs into the maintainer comments below.

## Findings

- PAL with the previous driver (maintainer): bottom quarter cropped, green
  bars at the bottom; second ffplay start not always working.
- PAL recordings: no apply block at a 576i start; 576i ↔ 288p decoder-only;
  `CA` codes as NTSC; PAL apply values `9A 90 A8`.
- S-video (T-052): works like composite with the existing code (sixteenth
  test, `logs/hardware-test-log.md`).

---

## Maintainer comments

