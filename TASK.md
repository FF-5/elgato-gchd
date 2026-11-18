# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite after S-video: recorded switch sequence; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

Last test (a675bc8): S-video after composite works (576i, 288p change).
Composite gave a **vertically stretched picture**, both as the first test
and after switching back from S-video: the decoder reported the 60 Hz
family for the PAL source (`0x88:FA = 26`) and 720 × 480 was configured.

Cause: the analog decoder keeps its settings while powered, also across
module reloads. The previous S-video session left it set for S-video and
in progressive mode (`0x88:28 = 8A` after a 288p change). The driver wrote
only `02 = C9, 14 = D2` when going back to composite, and nothing at all
after a module reload. The recorded S-video → composite switch
(`input-switching/filtered_novideo_input_switching` 19.566-19.828) is
`02 = C9, 14 = D2, 3C = 6B`, a 0.22 s pause, `28 = 88`, `03 = 28`, then
the selector and routing. The driver now writes exactly that whenever the
decoder may still be set for S-video, including the first composite start
after loading the module.

## Acceptance criteria

1. Composite (PS1 PAL): 576i picture with correct proportions, also after
   S-video and after reloading the module without replugging.
2. S-video still works.

## Hardware test (maintainer)

PS1 connected to both composite and S-video. Do **not** unplug between the
steps.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# wait for a 288p change, close, wait until ffplay has exited
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; reload the module without unplugging:
sudo rmmod elgato_gchd; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 3
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; then -i 2 and ffplay once more
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

For each composite session: proportions correct? dmesg should say
`configuring 576i` (or `288p`), not `240p`/`480i`.

## Findings

- a675bc8 test: composite stretched vertically (60 Hz family detected for
  the PAL source); decoder still set for S-video / progressive.
- b17d75e test: PAL S-video works; composite after S-video needs a replug
  (decoder kept `0x88:02 = DC`).
- 13b3a79 and earlier: PAL S-video never locks; the PS1 outputs S-video
  (Windows recordings); cause: missing retrigger on `0x98:F5` bit 7.

---

## Maintainer comments

