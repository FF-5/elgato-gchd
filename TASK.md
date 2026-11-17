# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** diagnostics + post-STOP drain; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

The mode change now runs as recorded (9382bcb, TODO "Hardware test log"):
NULL completed in 0.73 s, STOP, back end, INIT A0 and START all succeeded.
But after that START, and after **every** later START, including a fresh
firmware load after closing ffplay, only 5–7 bulk transfers arrived and then
the stream stopped; the next NULL never completed. Only the first START after
plugging in the device streamed.

Before 8a4b2af the driver read the stream after STOP until it ran dry
(1 s timeouts), and a second capture session worked. Since 8a4b2af nothing
reads the stream after STOP. The recordings show `ABORT_PIPE` on `0x81`/`0x84`
right after STOP, but they contain no bulk data, so they cannot show whether
Windows reads remaining data first (TODO "Evidence wanted").

This round:

1. After STOP (and after the receive thread has been joined), the driver
   reads `0x81` until a read returns nothing within 200 ms (at most 100
   reads) and logs `post-stop drain: reads=… bytes=…`.
2. Diagnostics: the stats line shows failed receive transfers
   (`urb_errors`, `last_status`); the first five reads after each START are
   logged as `bulk-IN first reads`.

Audio is not delivered yet (TODO T-023).

## Acceptance criteria

1. 480i → 240p and 240p → 480i while ffplay runs: the picture returns.
2. Closing ffplay and starting it again gives a picture.
3. dmesg shows "analog mode now …", no "analog mode change failed", no
   "signal lost" right after a START.

## Hardware test (maintainer)

Unplug and replug the device first (the device may still be in the stuck
state from the last test).

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
# close ffplay, wait 5 s, start the same ffplay command again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

Note for each step whether a picture appeared. Paste the output into the
maintainer comments below. The `bulk-IN first reads` and `post-stop drain`
lines are the important ones.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

