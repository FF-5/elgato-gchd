# TASK T-053b — PAL S-video

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** composite lock depends on the back-end retrigger; test switch
- **Type:** driver fix + hardware test
- **Branch:** `fix/pal-svideo` (from `dev`); component (`feat/component`,
  T-051) continues after this
- **TODO items:** T-053b

## Why

The maintainer always tested one input at a time (a PS1 has one video
output), so cabling is not the cause. Looking at all composite runs with the
PS1:

| Build | Retrigger while polling composite | Result |
|---|---|---|
| `dev`, seventeenth test | no | 576i detected, picture correct |
| b17d75e … 13b1a5b | yes | lock, but 60 Hz family detected, stretched |
| 5a189e2 | no | no lock (`FA` low nibble `D`) |
| `dev`, control test | no | no lock (`FA` low nibble `5`, then `D`) |

So at the moment the decoder locks onto the PS1's composite signal only
when the back end is retriggered, and then with the wrong family; without
the retrigger it does not lock at all, although the same code locked in the
seventeenth test. `FA` low nibble `5` means "50 Hz seen, not locked", `D`
"no sync".

This build has a test switch, `composite_retrigger=1`, that brings the
retrigger back for composite. Each retrigger is logged
(`back-end status 0x.. while polling; retrigger`).

## Acceptance criteria (this test)

PS1 on composite (no game needed), `composite_retrigger=0` and `=1`: lock,
detected family, picture. The Windows reference already exists (the
recordings were made with this PS1).

## Hardware test (maintainer)

PS1 on composite:

```sh
git fetch origin && git checkout fix/pal-svideo && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko composite_retrigger=0
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# close; then:
sudo rmmod elgato_gchd
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko composite_retrigger=1
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt|bulk-IN stats|callbacks suppressed"
```

Paste the outputs (labelled 0 and 1) into the maintainer comments below.

## Findings

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

