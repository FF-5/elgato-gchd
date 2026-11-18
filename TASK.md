# TASK T-066 — V4L2 compliance

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** analysis
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

## Plan

1. DV timings for HDMI and component: `G_DV_TIMINGS` returns the current
   or nominal timings (never an error while the input is selected),
   `S_DV_TIMINGS` accepts the supported ones, `DV_TIMINGS_CAP` and
   `ENUM_DV_TIMINGS` list the modes the encoder takes (720p, 1080i/p,
   480p/576p), `QUERY_DV_TIMINGS` keeps reporting the detected ones or
   `ENOLINK`.
2. The failing menu control: find which one rejects item 0 (input menu or
   H.264 profile with baseline masked) and fix its menu mask or handler.
3. `V4L2_CID_DV_RX_POWER_PRESENT` (read-only) for HDMI/component.
4. Input menu control: refuse a change while another file handle owns the
   buffers, like S_INPUT; or drop the private control if S_INPUT covers
   all applications (decide from guvcview/OBS behaviour).
5. Test file `tools/tests/T-066.sh`: `v4l2-compliance` without capture
   (expect 0 failures) and the smoke steps.

## Hardware test (maintainer)

Follows with the implementation.

## Findings

- T-071 accepted: gchd-test.sh T-071 10/10 PASS (firmware extracted from
  the .msi equal to the previous files, DKMS install, capture with the
  installed driver, uninstall). Merged as 0.5.7.

## Maintainer comments

