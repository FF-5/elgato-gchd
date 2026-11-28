# TASK T-091 — User profiles

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** second run all PASS; OBS profile controls reworked (dialogs, list update); third run (OBS only) next
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
- After the first run: the menu entry is named "Elgato Game Capture HD"
  (no "Settings"); buttons without "..."; Load, Save, Delete only (no
  Rename); Save asks for a new name or a profile to replace; the list
  shows only the names (users put the input in the name if they want).

## Done so far

- `gchd-profile` (`src/settings/`, installed with the core in
  `/usr/local/bin`, removed with it; profiles kept): `save NAME
  [--replace]`, `load`, `list [--input N]`, `show`, `delete`, `rename`
  (command line only); files `~/.config/elgato-gchd/profiles/NAME.conf`
  (version line, `key=value` numbers, values checked before a load);
  names letters, digits, space, `.`, `_`, `-`, at most 64. Load: input
  (only if different; refused: nothing else changed, status 2), output
  size, then all controls of that input in one call. Exit status 0 / 1
  error / 2 not all applied / 3 exists or missing.
- Settings window: Profile row at the top (names), Load, Save, Delete;
  Save opens a dialog: type a new name or choose a profile to replace
  (asks before replacing); Delete asks.
- Text menu: "Profiles" with Load, Save ("(new profile)" or a profile to
  replace, asks), Delete (asks).
- OBS plugin: "Profiles" group: one Profile field (choose a saved profile
  or type a new name; OBS editable list), Load, Save, Delete; replacing
  and deleting ask in a Qt question box (`gchd-dialog.cpp`, over the
  properties); no result messages (the field and the list show the
  result), errors in a warning box; the list is refilled in the shown
  properties after each action. Runs `gchd-profile` with `posix_spawnp`
  (no shell). Builds with Qt 6 Widgets and the OBS frontend API (installer:
  `qt6-base-dev`, `g++`).
- Desktop entry "Elgato Game Capture HD", `Categories=Settings;HardwareSettings;`.
- READMEs (top level: Profiles section, menu location; OBS plugin).
- Checked with a simulated device that prints menu controls as
  `v4l2-ctl` does: every setting back after a load; window (virtual
  display), text menu (scripted), plugin actions (harness).

## Findings

- T-091 first run (04ae5cc): 18 PASS, 17 FAIL. Every load failed:
  `v4l2-ctl` prints menu controls with their name (`video_bitrate_mode: 0
  (Variable Bitrate)`, `hdmi_color_range: 0 (Standard)`) and the profile
  stored that text, which the load then refused (fixed: numbers only;
  older files are read too). All load-dependent checks (window, text menu,
  OBS) failed for this reason. The "driver error" count matched the
  `errors=0` of the transfer statistics (test fixed). The OBS delete did
  not happen (second click outside 5 s; now 30 s). PASS: gchd-profile
  installed with the core, menu entry under the system settings, list,
  save refusal, invalid name, rename, missing profile, save from the
  window, text menu and OBS. Maintainer: the controls were too
  complicated (decisions above).

- T-091 second run (468c0f0): 32 PASS, 0 FAIL: every setting back after a
  load (also while capturing, input change included, at most one encoder
  restart), settings window, text menu, OBS. Maintainer on OBS: no
  question when replacing a profile; a deleted profile stayed in the
  list; a permanent message below the buttons (unwanted: loading selects
  the profile, deleting clears it). Cause: after a button OBS redraws the
  properties it shows without building them again, so the refilled list
  and the second-click question (a new message line) never appeared.
  Fixed: Qt question boxes for replace and delete, the list refilled in
  the shown properties, no messages.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-091.sh > tests/T-091.sh
bash gchd-test.sh T-091 --branch feat/profiles
```

Needs the PS3 on composite and OBS with the plugin.

## Maintainer comments

