# TASK T-052 — S-video 480i and 240p

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** hardware test first (no driver change yet)
- **Type:** hardware test, then fixes as needed
- **Branch:** `feat/svideo` (from `dev`)
- **TODO items:** T-052

## Why

Composite is done on `dev` (0.3.0): 480i and 240p from a fresh start,
480i ↔ 240p changes while capturing, close and reopen. Next in the bring-up
order is S-video with the same resolutions. S-video shares the analog path
with composite; the recorded differences are `0x88:02 = DC` (composite `C9`)
and `0x88:14 = D2` (`docs/s-video.md`). The driver already sets both for
input 3, so S-video may work as it is. This test shows what is missing.

## Acceptance criteria

1. S-video (input 3), fresh start in 480i (menu) and in 240p (demo): picture.
2. 480i ↔ 240p changes while capturing: picture returns, as on composite.
3. Close and reopen works.
4. Switching between composite (2) and S-video (3) with `v4l2-ctl -i` while
   ffplay is closed: the next start uses the selected input.

## Hardware test (maintainer)

VirtualBox with the USB 3.0 (xHCI) controller; the N64 connected by S-video.

```sh
git fetch origin && git checkout feat/svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# let the game switch menu <-> demo twice; close ffplay; wait until the
# ffplay process has exited (about 2 s); start ffplay again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats"
```

If an S-video cable is not available, say so in the comments; the next
task is then PAL (needs a PAL source) or component.

Paste the output into the maintainer comments below and note when the game
switched.

## Findings

_(filled in during the task)_

---

## Maintainer comments

