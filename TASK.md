# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** second fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Composite 480i and 240p both start and stream (merged into `dev`). The
first attempt at switching while capturing (8a4b2af) detected the change but
the stop did not complete:

- The receive queue (bulk IN `0x81` and `0x84`) was stopped before NULL, and
  the synchronous drain gave up at the first empty read, so the device never
  reported NULL complete (timeout after ~63 s).
- After the recovery path restarted capture in 240p, the signal-loss check used
  a video timestamp taken before configuration and stopped capture 0.37 s
  after START.

Now the receive queue keeps running through NULL and STOP and is joined
afterwards (bulk IN `0x81` keeps completing after NULL is sent), and the
video timestamp is refreshed when START completes.

480i → 240p still uses the restart path; the traces show a decoder-only
change for that direction (next step once the restart works).

Audio is not delivered yet (TODO T-023): only the video PES is passed to
V4L2. It comes with TS passthrough (T-020).

## Acceptance criteria

1. 480i → 240p while ffplay runs: the picture returns as 240p within a few
   seconds.
2. 240p → 480i while ffplay runs: the picture returns as 480i.
3. dmesg shows "analog mode change detected" and "analog mode now …", no
   "analog mode change failed", no "signal lost" right after a START.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

If ffplay stops showing a picture after a switch, close it and start it again
(same command) and note whether the picture comes back. Paste the output and
observations into the maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing (see TODO "Hardware test log").

---

## Maintainer comments

