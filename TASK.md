# TASK T-091 — User profiles

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (command, settings window, text menu, OBS plugin); hardware test next
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
3. Available in the settings window, the text menu and the OBS plugin
   (the same functions in all three: not everyone installs the settings
   programs; maintainer 2026-11-27), all using the same profiles.
4. Loading a profile leaves the driver's remembered settings at the
   loaded values (the udev rule keeps working as today).

## Decisions (maintainer, 2026-11-27)

- Profiles per user (`~/.config`).
- A profile holds the bitrate of its own output size only.
- The OBS plugin saves, renames and deletes too, not only loads.
- The settings window belongs to the system settings menu (*System >
  Preferences > Hardware*), not *Applications > Video* (players, editors).
- The format (H.264 / transport stream) is not part of a profile: the
  capturing program chooses it.

## Done so far

- `gchd-profile` (`src/settings/`, installed with the core in
  `/usr/local/bin`, removed with it; profiles kept): `save NAME
  [--replace]`, `load`, `list [--input N]`, `show`, `delete`, `rename`;
  files `~/.config/elgato-gchd/profiles/NAME.conf` (version line,
  `key=value`, values checked before a load); names letters, digits,
  space, `.`, `_`, `-`, at most 64. Load: input (only if different; refused:
  nothing else changed, status 2), output size, then all controls of that
  input in one call. Exit status 0 / 1 error / 2 not all applied / 3
  exists or missing. Checked with a simulated device (every setting back
  after a load, refusals, rename, delete).
- Settings window: Profile row at the top (list with the current input's
  profiles first; Load, Save as..., Rename..., Delete with confirmation);
  checked under a virtual display (save, load, rename, delete).
- Text menu: "Profiles" item (load, save with replace question, rename,
  delete); checked with a scripted whiptail.
- OBS plugin: "Profiles" group (Profile list, Load profile, Save to this
  profile and Delete profile with a second click, New name, Save as new
  profile, Rename to the new name, result message for a minute); runs
  `gchd-profile` with `posix_spawnp` (no shell); checked in a harness
  with the simulated device.
- Desktop entry: `Categories=Settings;HardwareSettings;`.
- READMEs (top level: Profiles section, menu location; OBS plugin).

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-091.sh > tests/T-091.sh
bash gchd-test.sh T-091 --branch feat/profiles
```

Needs the PS3 on composite and OBS with the plugin.

## Maintainer comments

