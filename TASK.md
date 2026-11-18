# TASK T-061 — Bitrate and stretch for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (4d1a342); hardware test with the test script
- **Type:** driver feature
- **Branch:** `feat/bitrate` (from `dev` 0.5.3)
- **TODO items:** T-061 (stage 1)

## Why

The capture application sets the video bitrate (a slider per output size)
and offers "stretch" for standard-definition input; the driver always
encodes the default bitrate and never stretches. Recorded
(`docs/composite.md §10`, `traces/composite/`):
`gchd_288p_standard_bitrate_change_small`,
`gchd_288p_mobile_bitrate_change_small`, `gchd_288p_stretch_input_small`.

| Setting | Encoder fields |
|---|---|
| Standard, maximum (default) | CBR (`v_rate_mode 0`), `v_max_cpb_delay 3`, 11250 / 10125 / 5625 / 2812 |
| Standard, lower | VBR (`v_rate_mode 1`), `v_max_cpb_delay 10`, e.g. 6000 / 5400 / 3000 / 600 |
| Mobile | VBR, 4000 … 15000 |
| Stretch | `v_in_source_type 2` instead of 3 |

## Implementation

Recorded values (`docs/composite.md §10`): maximum 0.9·B, average B/2,
minimum 0 / B/10 / B/4; `system_rate` ⌊1.07285·B + 461.7⌋ (+8 at 60 Hz);
CBR only at the default 11250, VBR otherwise; stretch = `v_in_source_type`
2. Controls `video_bitrate`, `video_bitrate_mode`, `stretch_sd_input`;
on composite/S-video a change while capturing restarts the encoder.

## Hardware test (maintainer)

First run of the test script (`tools/gchd-test.sh`, `TODO.md` "Testing").
It builds the branch, loads the module, runs everything and asks you for
the console and for two visual checks. Needs `v4l-utils` and `ffmpeg`
(already installed). In your home directory:

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-061.sh > tests/T-061.sh
bash gchd-test.sh T-061 --branch feat/bitrate
```

Paste the summary block it prints at the end. If any line says FAIL or
WARN, paste the whole log file too (path printed at the end, in
`~/gchd-test-logs/`). If the script itself misbehaves (wrong prompt,
hangs), say where; Ctrl+C stops it.

## Findings
- Maintainer requests (2026-11-18, before T-061): all wanted Windows
  recordings listed in `TODO.md` "Evidence wanted" (R1-R10, marked
  needed / optional / not needed); hardware tests through a script
  (`TODO.md` "Testing", `tools/gchd-test.sh` with `tools/tests/smoke.sh`
  on the `tools` branch, CLAUDE.md "Hardware testing"); pre-release test
  T-090.

## Maintainer comments

