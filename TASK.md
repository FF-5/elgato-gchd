# TASK T-064 — Remembered settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; input-at-probe fix d06a7cf; hardware test with the test script
- **Type:** driver feature + udev rule of our own
- **Branch:** `feat/input-persistence` (from `dev` 0.5.4)
- **TODO items:** T-064 (stage 1)

## Why

Maintainer requirement: HDMI and the defaults on the first use after
installation; afterwards the input and all settings last used, also after
unplugging, module reload and reboot. Until now the driver remembered the
input only while the module stayed loaded, and the other settings only
while the device stayed connected.

## Implementation

1. Module parameters `input`, `output`, `format`, `bitrate`,
   `bitrate_mode`, `stretch`, `h264_profile`, `h264_level` hold the
   settings in use; at probe the device takes them, and the controls get
   them as defaults.
2. Every change (input, `S_FMT`, encoder controls) updates the parameters
   and sends a `change` uevent with `GCHD_*` values on the USB interface.
3. `udev-rules/56-elgato-gchd-settings.rules` (new file of ours) writes
   them as `options elgato_gchd ...` to
   `/etc/modprobe.d/elgato-gchd-settings.conf`; modprobe applies them at
   the next load. Docs: `dkms/README.md`.
4. User profiles (maintainer idea): TODO T-091, after the release.

## Hardware test (maintainer)

The test installs the udev rule (and removes the settings file at the end).
No console needed except for one capture step (PS1 on composite).

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-064.sh > tests/T-064.sh
bash gchd-test.sh T-064 --branch feat/input-persistence
```

Paste the summary; the log too if anything fails.

## Findings
- Maintainer (2026-11-18): remember all settings, not only the input;
  later user profiles that recall all settings, several per input
  (TODO T-091).
- d5678cc test: first use (HDMI, defaults), settings file, restored
  format/bitrate/mode/stretch, replug capture 640×480: PASS. 2 FAILs: after
  the reload the driver reported input 0 and 1280×720 until the first
  capture (the remembered input was applied only at STREAMON); d06a7cf
  applies it at probe.

## Maintainer comments

