# Elgato Game Capture HD — test suite

Hardware tests of the `elgato_gchd` driver and its programs. This branch
holds only the tests; the driver is on the repository's other branches.

## What it does

`gchd-test` runs parts (one per input or program) as a **fast test**
(general function, a few minutes per part) or a **deep test**
(everything the part can test). It checks what it can by itself: the
captured stream (size, frame rate, field order, audio, bitrate), the
picture settings (measured on captured frames), the audio gain (measured
with a steady sound), the driver's kernel messages, `v4l2-compliance`. It
asks you only for what needs hands or eyes: connect a cable, set the
source to a mode, unplug the device, look at a window.

Parts so far: **composite**. Still to come: S-video, component, HDMI,
settings window, text menu, OBS plugin, installer.

The deep test switches on the driver's detailed messages (its `debug`
parameter) while it runs and off at the end. The kernel log is read,
never cleared.

## What you need

- The driver installed (see the repository's README) and the device
  plugged in.
- `v4l-utils` (`v4l2-ctl`, `v4l2-compliance`), `ffmpeg` (`ffmpeg`,
  `ffprobe`, `ffplay`), `sudo`.
- A source for the part: for composite a console or player with a
  composite (yellow) cable and its audio (red/white). The suite asks
  which modes your source can output (NTSC 480i, PAL 576i, 240p, 288p);
  cases for modes you do not have are skipped (SKIP), not failed.
- For the deep test's audio case: a steady sound (a constant tone), for
  example a phone with a 3.5 mm to RCA cable. Without it that case is
  skipped.
- A desktop session (the deep test opens a window with the picture).

## Run it

Get the suite (only this branch):

```sh
git clone --branch tests --single-branch https://github.com/FF-5/elgato-gchd gchd-tests
```

Then the menu:

```sh
bash gchd-tests/gchd-test
```

or a part directly:

```sh
bash gchd-tests/gchd-test composite fast
bash gchd-tests/gchd-test composite deep
bash gchd-tests/gchd-test all fast
bash gchd-tests/gchd-test --list
```

## Report

At the end the suite prints a summary: the device (USB ID), driver
version, distribution, kernel, and PASS / FAIL / WARN / SKIP for each
case. Post it in an issue (also when everything passes, for models and
sources the developers cannot test). The full log is in
`~/gchd-test-logs/` (attach it when something failed).

The menu item "Cases that need other testers" lists what the developers
cannot test themselves, with the issue for each.

## For development

`--repo DIR [--branch NAME]` installs the driver of a checkout (its
installer, core part) before testing.
