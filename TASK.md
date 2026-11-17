# TASK T-051 — Component, all modes

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recordings and a first test of the current driver needed
- **Type:** Windows recordings, hardware test, then implementation
- **Branch:** `feat/component` (from `dev`)
- **TODO items:** T-051

## Why

Composite and S-video are done on `dev` (0.4.0), except PAL S-video, for
which no working source was available (TODO T-053b). Next in the bring-up
order is component.

The driver's component path still uses the blocks taken over from the C++
(`docs/component.md §3`: mode detection from `0x9C:68/67`, per-mode
front-end and back-end tables, `0x94:4A/4B = AF`, `0x98:0E` pulses `65`/`64`).
None of it has been compared with a recording, because the only component
recording (`input-switching/filtered_init_switch_to_component`) has no
source connected. For composite, following the recordings instead of the
C++ blocks fixed most problems.

## Acceptance criteria (this task)

1. Windows recordings of component capture starts with a signal, at least
   480i and 480p (and every other mode the source can output: 576i, 576p,
   720p, 1080i), plus one mode change while capturing.
2. The current driver tested with the same source, to see where it fails.
3. Then: component implemented from the recordings; picture in every mode.

## Hardware test (maintainer)

Please say first which component source(s) you have and which modes they
output.

1. **Windows recordings**, one per mode: select component in the Windows
   software, start capture with the source on, wait for the picture, stop.
   If the source can switch modes while capturing, one recording of a
   switch. Reduce with
   `tshark -r big.pcapng -Y "usb.transfer_type != 0x03 || usb.endpoint_address.direction == 0" -w small.pcapng`
   and upload to `traces/component/`, with the mode in the file name.
2. **Linux, current driver:**

```sh
git fetch origin && git checkout feat/component && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 1
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Per mode: picture? Paste the output into the maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

