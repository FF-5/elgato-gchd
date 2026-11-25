# TASK T-096 — Stretch SD input as in the Windows software

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** evidence in (Flashback files); implemented; hardware test
  T-096 open
- **Type:** fix (documentation, OBS plugin as is) and installer change
- **Branch:** `fix/stretch` (from `dev` 0.5.9)
- **TODO items:** T-096 (stage 1b); installer parts (maintainer,
  2026-11-25)

## Why

"Stretch SD input" is meant for games that offer a 16:9 output by
squashing the picture horizontally into 4:3, so that a widescreen TV
stretches it back. In the Windows software the output size stays the same
and the picture is visibly stretched (maintainer, 2026-11-19). With the
driver, OBS's Media Source shows no difference.

## What is known

- The USB recording `traces/composite/gchd_288p_stretch_input_small`:
  the only change is the encoder field `v_in_source_type = 2` instead of
  `3` (`docs/composite.md §10`). The driver sends the same.
- Driver capture (T-060-settings, c8c8944, NTSC N64, 720 × 480): the
  encoded size stays 720 × 480; the stream's sample aspect ratio changes
  from 10:11 (display 4:3) to 40:33 (display 16:9), also when stretch is
  switched while capturing. Players that follow the aspect ratio
  (ffplay at the start of a stream) show it stretched; OBS's Media Source
  and OBS itself ignore it.
- The OBS plugin widens a picture with a 16:9 aspect ratio itself
  (720 × 480 → 872 × 480, 720 × 576 → 1048 × 576), so the source in OBS
  gets wider (accepted as a stop-gap, T-060). The maintainer is not sure
  this matches the Windows software.

## Evidence (maintainer, 2026-11-25)

`traces/composite/gchd_PAL_composite_flashback_stretch_ffprobe.txt`:
`ffprobe` of two files saved with the Windows software's "Flashback"
option, PAL composite, stretch off and on. Both 720 × 576, H.264 High
level 3.1, top field first; sample aspect ratio 12:11 (display 15:11, 4:3)
without stretch, 16:11 (display 20:11, 16:9) with stretch. The Windows
software stores the device's picture as it is (it repackages the stream:
other stream IDs); its preview shows the aspect ratio. The driver's stream
is the same (40:33 / 10:11 at 720 × 480, 16:11 / 12:11 at 720 × 576).

## Decisions

1. (Maintainer, 2026-11-25) Stretch: the output size stays, the picture is
   shown stretched (as the Windows software's preview).
2. (Maintainer, 2026-11-25) Installer: the core (kernel driver, firmware,
   udev rules) is always installed; the settings programs and the OBS
   plugin are optional, and the installer asks for each. Done together
   with T-096.

## Goal (acceptance criteria)

1. The driver's stream with stretch on and off is as the Windows
   software's (size kept, 16:9 / 4:3 sample aspect ratio): already so.
2. Players that follow the aspect ratio (ffplay) and the OBS plugin show a
   stretched picture with stretch on; OBS's Media Source cannot (OBS does
   not use the aspect ratio): documented in `README.md`.
3. Installer: interactive installation asks for the settings programs and
   the OBS plugin ([Y/n] each); `--core`, `--settings`, `--obs`, `--all`
   without questions; `--uninstall` everything or with `--settings` /
   `--obs` one optional part.

## Implementation

- Stretch: no driver change (the stream already matches the Windows
  software). Documentation: `docs/composite.md §10` (Flashback files),
  `docs/video-output.md` (aspect ratio of the stretch control),
  `README.md` (Media Source), `src/obs-plugin/README.md`.
- `install-driver.sh`: parts core, settings, OBS; `choose_parts` asks for
  the optional ones; `install_settings` / `uninstall_settings` split from
  the driver part; `--driver` kept as another name for `--core`. Checked
  here with the install functions replaced by stubs: every option and the
  answers (Enter, n) call the right parts, exit status 0; shellcheck
  without warnings. Version 0.5.10.

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-096.sh > tests/T-096.sh
bash gchd-test.sh T-096 --branch fix/stretch
```

## Findings

- Component recording (maintainer, 2026-11-25):
  `traces/component/gchd_component_modes_output-sizes_5060fps_small_part1`
  and `_part2` (decoded listings next to them): 576i, then 576p, 720p,
  1080i, 1080p (1080p30 only, USB 2.0), output sizes and the "Allow
  50/60fps" option, order not noted. For T-051 (stage 2); single-option
  recordings can be asked for then.

## Maintainer comments

