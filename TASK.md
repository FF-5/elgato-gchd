# TASK T-080 — Picture and audio settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** done, merged into `dev` as 0.5.13 (2026-11-27)
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
- Composite and S-video: the picture settings are the decoder's registers
  `0x88:10`–`15` (recordings R15, analysis §8): `gchd_dec_picture()`,
  written as the third tuning pass of the input setup and while capturing
  (mode worker); every recorded end value reproduced except contrast +100
  (driver 162, the recording's highest 155).
- Audio: the recording of every step (R16) has exactly the driver's codes;
  the second run measures the levels with a steady sound.

## Findings

- T-080 (6d94540), first run: 15 PASS, 5 FAIL. PASS: controls present;
  audio gain while capturing (audible, no encoder restart); −12 dB measured
  13.0 dB quieter than 0 dB; settings kept after a replug; HDMI RGB
  (laptop): saturation −100 black and white, contrast +50 right colours,
  with Expanded too; settings window (sliders, Reset picture, gain hidden
  for HDMI), OBS (hue −60), text menu (contrast 25); v4l2-compliance 0
  failures. FAIL: (1) the picture settings had no effect on composite
  (maintainer). The Windows software never writes the colour conversion
  block for composite/S-video (none of the composite and S-video
  recordings has it): the analog decoder path does not pass through it;
  its picture settings are elsewhere (decoder `0x88`?), not recorded.
  The recordings of the picture settings were made on component, which the
  test did not check. (2) +6 dB measured 3.1 dB quieter than 0 dB (0 dB
  −10.1, −12 dB −23.1, +6 dB −13.2 dB mean volume, music, 8 s each): the
  "+20 dB" reading of `0x94:41 = 57` is wrong (from this one value about
  +11 dB, ±2 for the changing music); the positive half needs a recording
  with single steps and a steady sound.
- Recordings R15/R16 (2026-11-27): see Done so far.
- T-080 second run (2026-11-27): built 6d94540, not 74d6482: that commit
  had carried the build file `src/driver/.fops.o.d`, the runner's
  `git pull` aborted on the untracked copy and the test went on with the
  old code (fixed: file removed, `.*.o.d` ignored, the runner stops when
  the update fails). So the composite and S-video picture FAILs (no effect,
  saturation −100 not kept after the signal returned) are of the old code.
  PASS: component, all four picture settings, no encoder restart; audio
  with a steady tone, mean volume 0 dB −18.3, −6 −24.3, −12 −30.2, +1
  −17.3, +6 −12.2, +12 −6.0, 0 again −18.2: exactly 1 dB per step on both
  sides, the positive half confirmed (`41 = 57` is +20 dB; the first run's
  −3.1 dB came from the music).
- T-080 third run (6df6492): composite and S-video, all four picture
  settings while capturing PASS, no encoder restart. FAIL: saturation −100
  set before the capture started was not shown (colour from the start).
  Cause: the capture start (after the encoder start) wrote fixed decoder
  values `0x88:10`–`15` = `88 D4 D0 08 08 88` over the settings; Windows
  writes the picture settings there (`6F EA D0 1B 1B 88` at neutral).
  Fixed: composite/S-video write `gchd_dec_picture()` there (so the
  neutral picture now also matches Windows from the start).
- T-080 fourth run (0da30db): composite and S-video, saturation −100 and
  brightness +40 set before the capture shown from the start, kept after
  the signal returned, normal at 0: 6 PASS, 0 FAIL. Acceptance criteria
  met; merged as 0.5.13.
- Maintainer (2026-11-27): `master` holds only the product (sources, docs,
  READMEs); contributor guides and agent files stay on `dev` (RULES B2,
  TODO T-100). The test suite lives on its own branch `tests` (RULES B3,
  TODO T-102).

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-080.sh > tests/T-080.sh
bash gchd-test.sh T-080 --branch feat/picture-audio
```

## Maintainer comments

