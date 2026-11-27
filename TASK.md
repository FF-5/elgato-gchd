# TASK T-080 — Picture and audio settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (driver, settings window, text menu, OBS plugin); hardware test next
- **Type:** driver feature (stage 4)
- **Branch:** `feat/picture-audio` (from `dev` 0.5.12)
- **TODO items:** T-080 (stage 4)

## Why

The Windows software offers brightness, contrast, saturation and hue and
an analog audio gain. They change the device itself (recorded), so they
belong to the driver, remembered like the other settings, and to the
settings window, text menu and OBS plugin.

## Goal (acceptance criteria, draft)

1. Brightness, contrast, saturation (−100 … +100) and hue (−180° … +180°)
   as V4L2 controls (`V4L2_CID_BRIGHTNESS`, `_CONTRAST`, `_SATURATION`,
   `_HUE`), the recorded register values at 0, the ends and in between;
   applied while capturing without an encoder restart; for every input
   (HDMI RGB sources: combined with the RGB conversion).
2. Analog audio gain −12 … +12 dB, default 0 dB (maintainer 2026-11-27);
   the recorded register values; the positive half confirmed by a level
   measurement.
3. Remembered (module parameters, udev rule); shown in the settings
   window, text menu and OBS plugin.

## Evidence

- `traces/gchd_{brightness,contrast,saturation,hue}_small`: each slider
  0 → −100 → 0 → +100 → 0 (hue ±180°), component 576p, while capturing.
- `traces/gchd_analog_audio_gain_small`: 0 → −12 dB in 1 dB steps, back to
  0, then towards +12 dB (component).
- Analysis: `traces/ANALYSIS-2026-11-27-hdmi-picture-audio-bitrate.md`
  §1–§4: the colour conversion block `0x9C:92`–`AA` (header, 3 × 3 matrix
  of 15-bit coefficients, 4096 = 1.0, word `in × 3 + out`, outputs Cr, Y,
  Cb; offsets × 64); contrast = Y → Y coefficient v × 32 (v 0 … 255,
  128 neutral); saturation = Cr → Cr and Cb → Cb, same scale; hue = Cr/Cb
  rotation in 0.5° steps, ±61.5° at ±180°; brightness = Y offset
  16 + ⌊(v − 128) × 1.25⌋; audio: `0x94:41/4A/4B`, code XOR `0x08`, 2 codes
  per dB, `41 = 37` for 0 … −12 dB (`AF` 0 dB), `41 = 57` (+20 dB) for
  +1 … +12 dB.
- Driver: `gchd_csc_put()` / `gchd_csc_of()` (`src/driver/input.c`) build
  the block from a matrix; picture settings multiply into it.

## Plan

1. Driver controls and the matrix: adjustment (contrast, saturation, hue)
   × the source's conversion; brightness on the Y offset; written while
   capturing through the mode worker (as the colour range).
2. Audio gain control writing `0x94:41/4A/4B` (while capturing through the
   mode worker); every input setup uses it instead of the fixed values.
3. Parameters, uevent, udev rule; settings window, text menu, OBS plugin.
4. Emulator check against the recordings (each recorded block reproduced
   from its slider value); hardware test with a level measurement for the
   audio gain.

## Done so far

- Driver: controls `brightness`, `contrast`, `saturation` (−100 … 100),
  `hue` (−180 … 180) and "Analog Audio Gain (dB)" (−12 … 12, default 0);
  the colour conversion of the source times the picture adjustment
  (`gchd_csc_picture()`), the gain registers (`gchd_audio_put()`, also in
  the input setup instead of the fixed values); changes while capturing
  through the mode worker without an encoder restart; parameters
  `picture`, `audio_gain`, uevent, udev rule.
- Check against the recordings (the driver's code in userspace): brightness
  ends exact (Y offset words `5C01`, `2B81`), hue ±61.5° in 0.5° steps
  (cos/sin within 2 of the recorded), contrast/saturation ends 8160 (the
  recordings 8159; Windows writes some values one lower), every recorded
  gain code (`A1` −1, `B7` −12, `C5` +1, `C1` +3, `C9` +7, `B1` +11,
  `BF` +12).
- Settings window (sliders, "Reset picture"), text menu (Picture, Audio
  gain), OBS plugin (sliders; audio gain hidden for HDMI).

## Findings

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-080.sh > tests/T-080.sh
bash gchd-test.sh T-080 --branch feat/picture-audio
```

## Maintainer comments

