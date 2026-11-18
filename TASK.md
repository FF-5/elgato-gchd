# TASK T-064 — Input persistence

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** design
- **Type:** driver feature + udev rule of our own
- **Branch:** `feat/input-persistence` (from `dev` 0.5.4)
- **TODO items:** T-064 (stage 1)

## Why

Maintainer requirement: HDMI on the first use after installation;
afterwards the input last selected, also after unplugging, module reload
and reboot. Today the driver remembers the input only while the module
stays loaded (`gchd_last_input`).

## Implementation (plan)

1. Module parameter `default_input` (0-3, default 0 = HDMI), read when the
   device is probed.
2. On every input change the driver sends a `change` uevent with
   `GCHD_INPUT=<n>` on the USB interface.
3. A new udev rule of ours in `udev-rules/` (the original file stays
   untouched, RULES B4) runs a small helper that writes
   `options elgato_gchd default_input=<n>` to
   `/etc/modprobe.d/elgato-gchd-input.conf`; modprobe applies it at the
   next load. The install step (T-071) installs the rule and helper.
4. Optional, to decide with the maintainer: the same for the output size,
   pixel format and bitrate (they are kept only while the device stays
   connected).
5. Test file `tools/tests/T-064.sh`: select an input, reload the module,
   unplug/replug, check `--get-input`; the reboot check by prompt.

## Hardware test (maintainer)

Follows with the implementation.

## Findings

## Maintainer comments

