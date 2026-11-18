# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** PS1 without game may output 60 Hz; questions to the maintainer
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

The 7d8ed87 test log (S-video part included) shows the decoder reporting a
**60 Hz signal** on S-video throughout: `0x88:FA = 06/26` (low nibble 6),
lock `66` then `7E`, so 240p/480i was configured, correct for a 60 Hz
source. In the earlier S-video test with the same PS1 (game intro running)
it read `07/17` (50 Hz) and gave a correct 576i picture. Same driver code,
different signal.

Today the PS1 ran without a game. If its start-up menu outputs 60 Hz with
PAL colour ("PAL-60"), that explains S-video (correct 60 Hz handling; the
horizontal lines are how 240p looks in plain ffplay, see
`docs/video-output.md`: `-vf separatefields`) and composite (PAL colour at
60 Hz can confuse the composite decoder: `FA` low nibble `5`, then `D`).
The monitor test shows the PS1 output itself is fine.

## Questions (maintainer)

1. What does the PS1 show right now (start-up menu without disc, CD player,
   game)?
2. Does the monitor show 50 Hz or 60 Hz for the PS1 signal (on-screen
   info)?

## Hardware test (maintainer), depending on the answers

- If 60 Hz: PAL composite again with the game intro running (50 Hz), with
  `dev` and with this branch. PAL-60 becomes a separate item (a Windows
  recording of the start-up menu on composite would show how Windows
  handles it).
- If 50 Hz: the same commands as last time with the N64 on composite as a
  known-good check.

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

## Findings

- 7d8ed87 test: composite no sync with and without the switch; S-video
  detected 60 Hz (`FA = 06/26`) and captured 240p/480i; earlier, with the
  game intro, the same PS1 gave 50 Hz (576i/288p).
- One input at a time is always used (PS1 has one output): not cabling.
- Control test: `dev` fails with PAL composite like this branch (`FA` low
  nibble `5`, then `D`).
- 5a189e2 test: composite no sync at all (`FA = ED/FD`) on a fresh start.
- 13b1a5b test: composite broken also after a fresh plug-in; cause: the
  `F5` retrigger enabled for composite polling (b17d75e).
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

