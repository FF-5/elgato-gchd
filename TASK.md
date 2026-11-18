# TASK T-066 — V4L2 compliance

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (016477b); hardware test with the test script
- **Type:** driver fixes (V4L2 interface)
- **Branch:** `fix/v4l2-compliance` (from `dev` 0.5.7)
- **TODO items:** T-066 (stage 1b, before T-060 guvcview/OBS)

## Why

Applications such as guvcview and OBS, and the planned service that feeds
them (T-060), rely on the standard V4L2 behaviour that `v4l2-compliance`
checks. Without capture, 8 failures remain (fd8426b / 4c326eb runs):

- HDMI/component: `VIDIOC_G_DV_TIMINGS` fails without a signal although
  the inputs report `V4L2_IN_CAP_DV_TIMINGS`; `VIDIOC_DV_TIMINGS_CAP` is
  missing; both also break the buffer tests (`g_dv_timings`).
- "could not set valid menu item 0" for a menu control on every input.
- warning: `V4L2_CID_DV_RX_POWER_PRESENT` missing.
- The input menu control switches inputs for any process while another
  one captures (S_INPUT returns EBUSY since fd8426b).

## Implementation

1. HDMI/component: `G/S_DV_TIMINGS`, `DV_TIMINGS_CAP`, `ENUM_DV_TIMINGS`
   with the CEA-861 presets of the supported modes; `G_DV_TIMINGS` works
   without a signal (configured or last set mode, 720p60 at first);
   `QUERY_DV_TIMINGS` as before (detected mode or `ENOLINK`).
2. The failing menu control was the H.264 level: levels 1.0–2.2 were
   offered but rejected; now skipped.
3. `V4L2_CID_DV_RX_POWER_PRESENT` (read-only).
4. The input menu control refuses an input change while capture buffers
   exist (EBUSY), as `VIDIOC_S_INPUT`.

## Hardware test (maintainer)

PS1 on composite for the last step.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-066.sh > tests/T-066.sh
bash gchd-test.sh T-066 --branch fix/v4l2-compliance
```

Paste the summary; if the compliance step fails, the log (it lists the
`fail:` lines).

## Findings

- T-071 accepted: gchd-test.sh T-071 10/10 PASS (firmware extracted from
  the .msi equal to the previous files, DKMS install, capture with the
  installed driver, uninstall). Merged as 0.5.7.
- Maintainer (2026-11-18): the firmware script looks for the installer
  file only in `firmware/` (a path can still be given by hand); a later
  download from Elgato lands there; the installer only calls the script
  and checks its result. Done in 8ba3e1f (README, TODO T-095, test file).

## Maintainer comments

