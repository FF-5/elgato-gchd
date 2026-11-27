# TASK T-091 — User profiles

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** plan (2026-11-27); maintainer to confirm the open points
- **Type:** userspace feature (stage 5)
- **Branch:** `feat/profiles` (from `dev` 0.5.13)
- **TODO items:** T-091 (stage 5)

## Why

The Windows software has only fixed profiles. Users want named sets of
all settings (one per console or use: "PS3 component 720p", "N64
composite"), several per input, recalled at once.

## Goal (acceptance criteria, draft)

1. A profile holds every setting: input, output size, bitrate (and its
   mode), stretch, "Allow 50/60 fps", HDMI colour range, brightness,
   contrast, saturation, hue, analog audio gain.
2. Save the current settings as a named profile, load one (all settings
   applied at once, also while capturing), rename, delete, list (all or
   those of the current input).
3. Available in the settings window, the text menu and the OBS plugin,
   all using the same profiles.
4. Loading a profile leaves the driver's remembered settings at the
   loaded values (the udev rule keeps working as today).

## Plan (proposed defaults; open points below)

1. One command, `gchd-profile` (shell, installed with the settings
   programs): `save NAME`, `load NAME`, `list [--input N]`, `delete NAME`,
   `rename OLD NEW`, `show NAME`. It reads and writes the settings through
   `v4l2-ctl` like the other programs; no driver change.
2. Storage: one plain text file per profile,
   `${XDG_CONFIG_HOME:-~/.config}/elgato-gchd/profiles/NAME.conf`,
   `key=value` lines (versioned header line, unknown keys ignored, values
   checked against the control ranges before applying). Names: letters,
   digits, space, `-`, `_`, at most 64 characters.
3. Load order: input first, then the output size, then the other
   controls; one encoder restart at most.
4. Settings window: a "Profile" row (choose → load; "Save as…", "Delete").
   Text menu: a "Profiles" item (load, save, delete). OBS plugin: a
   profile list in the source properties with a "Load" button (calls
   `gchd-profile load`).
5. Docs: README sections for the three programs and `gchd-profile`.
6. Hardware test (`tools/tests/T-091.sh`): save, change everything, load,
   check every control; load while capturing (one restart at most); the
   same through the window, menu and OBS; profiles of another input.

## Open points (maintainer)

- Profiles per user (`~/.config`, proposed) or for every user of the
  computer (`/etc`, needs `sudo` to save)?
- Should a profile also hold the bitrates of every output mode (all six
  kept values), or only the bitrate of its own output size (proposed)?
- Should the OBS plugin only load profiles (proposed), or also save them?

## Hardware test (maintainer)

Not yet.

## Maintainer comments

