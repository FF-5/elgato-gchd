# TASK T-091 — User profiles

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** first run: loads failed, controls too complicated; fixed and simplified; second hardware run next
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
  and deleting need a second click within 30 s; the result below the
  buttons for a minute. Runs `gchd-profile` with `posix_spawnp` (no
  shell).
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

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-091.sh > tests/T-091.sh
bash gchd-test.sh T-091 --branch feat/profiles
```

Needs the PS3 on composite and OBS with the plugin.

## Maintainer comments

Although the test shows no fails there were a couple of things not quite right with the OBS plugin:
- Upon overwriting an existing profile I wasn't prompted with any question.
- Upon deleting a profile it was removed from the selected item in the list, but the profile was still displayed in the list.
- After saving a profile or loading/deleting, a permanent message appeared at the lower left part of the profile frame. This was somewhat annoying and maybe not necessary. When loading a profile the loaded profile is selected in the profile list. When deleting a profile the selected item is cleared (as it did now) and the profile is removed from the list. These are clear action result that don't need any further validation. With saving a profile is different, but I guess we can just live without that permanent message at the bottom left.

gchd-test T-091 20261128-021126 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-091-20261128-021126.log
== build
$ git fetch -q origin
$ git checkout -q feat/profiles
$ git pull -q --ff-only
commit: 468c0f0 Remove a committed Python cache; ignore __pycache__
commit: 468c0f0 Remove a committed Python cache; ignore __pycache__
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --all --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.13[0m

[1m==> 1/3 Firmware[0m
[32mFirmware found in /lib/firmware/gchd.[0m

[1m==> 2/3 Building the driver[0m
[32mBuilt /home/ff5/elgato-gchd/build/elgato_gchd.ko.[0m

[1m==> 3/3 Installing[0m
Sign command: /usr/bin/kmodsign
Signing key: /var/lib/shim-signed/mok/MOK.priv
Public certificate (MOK): /var/lib/shim-signed/mok/MOK.der

Building module:
Cleaning build area...
make -j4 KERNELRELEASE=6.8.0-146-generic KDIR=/lib/modules/6.8.0-146-generic/build...
Signing module /var/lib/dkms/elgato-gchd/0.5.13/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
 - Original module
   - No original module exists within this kernel
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mProfile command installed: gchd-profile (named sets of all settings).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD' in the system settings menu (Preferences > Hardware; gchd-settings-gui); text menu: gchd-settings[0m

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

PROMPT: PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s.
$ cat /usr/local/share/applications/elgato-gchd-settings.desktop
[Desktop Entry]
Type=Application
Name=Elgato Game Capture HD
Comment=Input, output size, encoder and picture settings and profiles of the Elgato Game Capture HD
Exec=gchd-settings-gui
Icon=camera-video
Terminal=false
Categories=Settings;HardwareSettings;
[PASS] menu entry named 'Elgato Game Capture HD'
--- dmesg step 1
[20371.201257] usbcore: deregistering interface driver elgato_gchd
[20381.458821] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[20381.463330] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[20381.463394] usbcore: registered new interface driver elgato_gchd
[20385.119397] usb 1-2: USB disconnect, device number 10
[20390.967299] usb 1-2: new high-speed USB device number 11 using xhci_hcd
[20391.359757] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[20391.359773] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[20391.359778] usb 1-2: Product: GameCapture HD
[20391.359782] usb 1-2: Manufacturer: Elgato
[20391.359785] usb 1-2: SerialNumber: 04107F1267
[20391.363666] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[20391.366394] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: gchd-profile: every setting back after a load
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=14000000,stretch_sd_input=1,brightness=30,contrast=-20,saturation=-40,hue=15,analog_audio_gain_db=-4
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: -4 brightness: 30 contrast: -20 hdmi_color_range: 0 hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0 
[PASS] composite settings set
$ P save T-091 composite
rc=0
[PASS] save composite profile
$ grep ^video_bitrate_mode= /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf
video_bitrate_mode=0
[PASS] saved as numbers
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0,brightness=-10,contrast=10,saturation=20,hue=-30,analog_audio_gain_db=3
$ P save T-091 component
rc=0
[PASS] save component profile
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,allow_50_60_fps=1
$ P load T-091 composite
rc=0
[PASS] load composite profile
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: -4 brightness: 30 contrast: -20 hdmi_color_range: 0 hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0 
[PASS] composite profile: every setting back
$ P load T-091 component
rc=0
[PASS] load component profile
$ cur
input=1 size=1280x720 allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 hdmi_color_range: 0 hue: -30 saturation: 20 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[PASS] component profile: every setting back
--- dmesg step 2
[20397.557123] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[20397.622567] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[20397.622597] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[20397.708173] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[20397.708266] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[20397.772891] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[20397.772922] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 3: load while capturing
$ gchd-profile -d /dev/video0 load T-091 composite
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,stretch_sd_input=0
PROMPT: A window 'Elgato composite' shows the PS3 in normal colours.
$ P load T-091 composite
rc=0
[PASS] load while capturing
$ cur
input=2 size=640x480 allow_50_60_fps: 0 analog_audio_gain_db: -4 brightness: 30 contrast: -20 hdmi_color_range: 0 hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0 
[PASS] settings of the profile while capturing
$ count_new encoder restart
1
[PASS] at most one encoder restart
[PASS] The window (maybe reopened by itself) shows the PS3 paler and brighter (saturation -40, brightness +30)?
$ P load T-091 component
rc=0
[PASS] load the component profile while capturing
$ cur
input=1 size=640x480 allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 hdmi_color_range: 0 hue: -30 saturation: 20 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[PASS] input changed while capturing
$ count_new failed|error
0
[PASS] no driver error
$ gchd-profile -d /dev/video0 load T-091 composite
--- dmesg step 3
[20397.872872] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[20397.872903] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[20397.899618] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[20398.011872] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20398.136428] elgato_gchd 1-2:1.0: transcoder defaults loaded
[20398.198168] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[20398.566048] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[20398.568026] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[20398.568027] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[20398.568029] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[20398.568031] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[20399.309983] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[20399.531404] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[20399.545876] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20399.546064] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20399.551017] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20399.551613] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20399.552009] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[20399.691542] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20404.570949] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=52 bytes=3407872 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20409.628258] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=111 bytes=7274496 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=33
[20410.145201] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[20411.570326] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20411.570669] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20411.576340] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20411.576780] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20411.577451] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[20411.696290] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20411.696293] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[20412.081380] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[20416.618336] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=51 bytes=3342336 ep84_bytes=3342336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20421.648212] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=108 bytes=7077888 ep84_bytes=7077888 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20426.712501] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=166 bytes=10878976 ep84_bytes=10878976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20431.752235] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=223 bytes=14614528 ep84_bytes=14614528 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20436.787861] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=280 bytes=18350080 ep84_bytes=18350080 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20441.825417] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=337 bytes=22085632 ep84_bytes=22085632 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20444.265066] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=1 streaming=1
[20445.724376] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[20445.737382] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20446.000806] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20446.272172] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20446.536144] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20446.801954] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20447.072451] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20447.347010] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20447.608311] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20447.885247] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20448.154636] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20448.418069] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20448.682464] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20448.947661] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20449.214047] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20449.510721] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20449.779936] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20449.793682] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20449.828029] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[20452.893584] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[20452.893616] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 4: settings window
PROMPT: Open 'Elgato Game Capture HD' (system settings in the application menu).
[PASS] Top of the window: a 'Profile' list with only the names (no input in brackets), and the buttons Load, Save, Delete (no Rename, no '...')?
PROMPT: Choose 'T-091 component' and click Load; wait 3 s.
$ cur
input=1 size=1280x720 allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 hdmi_color_range: 0 hue: -30 saturation: 20 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[PASS] window: component profile loaded
[PASS] Window: Input shows Component and the sliders the profile's values (brightness -10, hue -30)? -- contrast also at 10 and saturation at 20.
PROMPT: Click Save: a dialog asks for a new name or a profile to replace. Type T-091 window, click Save.
$ grep ^input= /home/ff5/.config/elgato-gchd/profiles/T-091 window.conf
input=1
[PASS] window: new profile saved
PROMPT: Choose Input Composite in the window; wait 3 s. Click Save, open the dialog's list, choose 'T-091 window', click Save and confirm Replace with Yes.
$ grep ^input= /home/ff5/.config/elgato-gchd/profiles/T-091 window.conf
input=2
[PASS] window: profile replaced
PROMPT: Choose 'T-091 window', click Delete and confirm with Yes. Close the window.
$ sh -c [ -e '/home/ff5/.config/elgato-gchd/profiles/T-091 window.conf' ] && echo there || echo gone
gone
[PASS] window: profile deleted
--- dmesg step 4
[20479.994528] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20480.114418] elgato_gchd 1-2:1.0: transcoder defaults loaded
[20480.192534] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[20480.221156] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[20480.371603] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[20480.373790] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[20480.373792] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[20480.373794] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[20480.373796] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[20481.318697] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[20481.587394] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[20481.614604] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20481.614855] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20481.620838] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20481.621420] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20481.621817] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20481.741016] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20482.014275] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1, settings changed
[20484.558071] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20484.558618] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20484.568208] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20484.568827] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20484.569854] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20484.704185] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20484.704188] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[20485.088981] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[20488.530113] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[20551.149622] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[20551.149652] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[20652.704848] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[20652.704910] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open

=== step 5: text menu
PROMPT: Run gchd-settings in another terminal: Profiles, Load, choose 'T-091 composite'; OK. Then Profiles, Save, '(new profile)', type T-091 menu; OK. Then Quit.
$ cur
input=2 size=640x480 allow_50_60_fps: 0 analog_audio_gain_db: -4 brightness: 30 contrast: -20 hdmi_color_range: 0 hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0 
[PASS] text menu: composite profile loaded
$ grep ^input= /home/ff5/.config/elgato-gchd/profiles/T-091 menu.conf
input=2
[PASS] text menu: new profile saved
[PASS] Text menu: Profiles offered only Load, Save, Delete, and the lists showed only names?
$ gchd-profile delete T-091 menu
--- dmesg step 5


=== step 6: OBS plugin
PROMPT: Start OBS (source 'Elgato Game Capture HD'); open the source's properties.
[PASS] OBS: a 'Profiles' group with one Profile field (a list you can also type in) and the buttons Load, Save, Delete?
PROMPT: In Profile choose 'T-091 component', click Load; wait 3 s.
$ cur
input=1 size=640x480 allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 hdmi_color_range: 0 hue: -30 saturation: 20 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[PASS] OBS: component profile loaded
[PASS] OBS: the message "Profile 'T-091 component' loaded." and the properties show Component with brightness -10?
PROMPT: In Profile type T-091 obs, click Save.
$ grep ^input= /home/ff5/.config/elgato-gchd/profiles/T-091 obs.conf
input=1
[PASS] OBS: new profile saved
PROMPT: Change Input to Composite in the properties; wait 3 s. Profile 'T-091 obs': click Save (a question appears below), then Save again.
$ grep ^input= /home/ff5/.config/elgato-gchd/profiles/T-091 obs.conf
input=2
[PASS] OBS: profile replaced
PROMPT: Profile 'T-091 obs': click Delete (a question appears), then Delete again.
$ sh -c [ -e '/home/ff5/.config/elgato-gchd/profiles/T-091 obs.conf' ] && echo there || echo gone
gone
[PASS] OBS: profile deleted
PROMPT: Close the properties (OK) and OBS.
--- dmesg step 6
[20827.830307] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20827.957544] elgato_gchd 1-2:1.0: transcoder defaults loaded
[20828.038570] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[20828.067943] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[20828.339963] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[20828.342603] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[20828.342606] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[20828.342608] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[20828.342610] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[20829.251027] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[20829.526896] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[20829.556500] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20829.556739] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20829.563574] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20829.564189] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20829.564817] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20829.702709] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20829.959066] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[20832.411404] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20832.411626] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20832.417780] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20832.418408] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20832.418961] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20832.537727] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20832.537730] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[20832.922273] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[20837.479079] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=52 bytes=3407872 ep84_bytes=3407872 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20842.513468] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=109 bytes=7143424 ep84_bytes=7143424 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20847.549163] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=166 bytes=10878976 ep84_bytes=10878976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20852.580747] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=223 bytes=14614528 ep84_bytes=14614528 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20857.622821] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=280 bytes=18350080 ep84_bytes=18350080 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20862.665587] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=337 bytes=22085632 ep84_bytes=22085632 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20867.702938] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=394 bytes=25821184 ep84_bytes=25821184 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20872.740921] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=451 bytes=29556736 ep84_bytes=29556736 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20877.780367] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=508 bytes=33292288 ep84_bytes=33292288 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20882.798859] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20887.831849] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=621 bytes=40697856 ep84_bytes=40697856 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20892.847376] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20897.886846] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=735 bytes=48168960 ep84_bytes=48168960 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20902.899596] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20907.918821] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=849 bytes=55640064 ep84_bytes=55640064 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20909.453405] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=1 streaming=1
[20912.317305] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[20912.326951] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20912.335955] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20912.604604] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20912.869813] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20913.142750] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20913.414027] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20913.682833] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20913.947268] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20914.213191] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20914.483955] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20914.747638] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20915.009219] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20915.275520] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20915.537132] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20915.805090] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20916.073343] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20916.341526] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20916.616019] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20916.883824] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20917.147281] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20917.416324] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20917.680448] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20917.948525] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20918.209129] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20918.478181] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20918.753585] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20919.016563] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20919.283007] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20919.547748] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20919.811194] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20920.073831] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20920.340237] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20920.604810] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20920.872604] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20921.140430] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20921.403783] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20921.673894] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20921.937888] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20922.207549] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20922.479541] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20922.752019] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20923.028701] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20923.296019] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20923.562722] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20923.826629] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20924.094714] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20924.367140] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20924.643252] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20924.906441] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20925.177356] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20925.444813] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20925.721077] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20925.993694] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20926.258004] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20926.530645] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20926.793440] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20927.064607] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20927.331986] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20927.596669] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20927.866254] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20928.131465] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20928.401938] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20928.667092] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20928.933443] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20929.208091] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20929.476934] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20929.747565] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20930.012188] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20930.279745] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20930.552124] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20930.822660] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20931.102063] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20931.372538] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20931.634145] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20931.900147] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20932.160273] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20932.430422] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20932.701575] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20932.972698] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20933.235592] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20933.499413] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20933.761313] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20934.027107] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20934.292838] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20934.563927] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20934.827790] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20935.091945] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20935.356695] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20935.621799] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20935.891402] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20936.156387] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20936.419829] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20936.685693] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20936.957180] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20937.224174] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20937.497179] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20937.765582] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20938.033689] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20938.302508] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20938.570259] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20938.837824] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20939.111173] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20939.384229] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20939.655488] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20939.930745] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20940.196363] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20940.458131] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20940.720716] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20940.989474] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20941.262744] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20941.536148] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20941.801934] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20942.066065] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20942.336517] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20942.603627] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20942.866283] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20943.128874] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20943.393119] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20943.664425] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20943.934337] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20944.201385] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20944.467779] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20944.727385] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20944.993936] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20945.260713] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20945.531313] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20945.794540] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20946.059201] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20946.323460] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20946.588856] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20946.858857] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20947.122668] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20947.388248] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20947.652233] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20947.917236] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20948.187789] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20948.453814] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20948.722296] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20948.988908] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20949.266589] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20949.532409] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20949.801381] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20950.071931] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20950.352701] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20950.626878] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20950.892707] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20951.159401] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20951.428545] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20951.690620] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20951.959146] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20952.231360] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20952.496667] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20952.764749] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20953.031596] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20953.299031] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20953.563371] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20953.828602] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20954.095306] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20954.364915] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20954.634245] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20954.903950] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20955.173039] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20955.444462] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20955.712277] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20955.981000] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20956.253443] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20956.535909] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20956.807663] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20957.081975] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20957.352710] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20957.623267] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20957.891706] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20958.162637] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20958.433190] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20958.702156] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20958.972528] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20959.234524] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20959.500207] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20959.764051] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20960.028226] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20960.293035] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20960.564329] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20960.825503] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20961.088424] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20961.352490] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20961.618149] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20961.881936] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20962.147211] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20962.410524] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20962.680761] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20962.946149] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20963.214275] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20963.481192] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20963.750789] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20964.022714] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20964.294062] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20964.563921] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20964.833342] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20965.100111] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20965.365310] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20965.632443] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20965.901113] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20966.172477] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20966.440933] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20966.708321] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20966.973808] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20967.244050] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20967.512098] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20967.780612] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20968.046561] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20968.312476] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20968.588062] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20968.859030] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20969.126798] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20969.397285] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20969.670536] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20969.937075] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20970.205523] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20970.471929] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20970.746061] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20971.016710] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20971.287344] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20971.559685] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20971.841531] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20972.117517] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20972.398863] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20972.673216] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20972.944395] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20973.218841] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20973.491572] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20973.756592] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20974.032351] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20974.298426] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20974.567426] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20974.840277] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20975.110729] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20975.388191] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20975.650628] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20975.924328] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20976.200766] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20976.469456] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20976.744808] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20977.015408] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20977.285842] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20977.557004] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20977.829790] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20978.100067] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20978.363553] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20978.631278] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20978.901464] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20979.185768] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20979.450792] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20979.720628] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20979.985414] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20980.261857] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20980.534235] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20980.813034] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20981.086318] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20981.368706] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20981.635214] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20981.903558] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20982.170050] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20982.439316] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20982.707388] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20982.973337] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20983.242751] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20983.513019] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20983.779428] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[20983.945306] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[20983.946725] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[20983.976591] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[20984.135737] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[20984.138419] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[20984.138425] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[20984.138428] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[20984.138431] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[20984.467515] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[20984.505886] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20984.506252] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20984.513619] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20984.514260] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20984.514736] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20984.631939] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20984.904412] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1, settings changed
[20987.447888] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[20987.448377] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[20987.459658] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20987.460297] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[20987.461596] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[20987.601490] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[20987.601496] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[20987.988882] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[20992.528888] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=50 bytes=3276800 ep84_bytes=3276800 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20997.536144] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21002.575740] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=161 bytes=10551296 ep84_bytes=10551296 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21007.614898] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21012.654745] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=275 bytes=18022400 ep84_bytes=18022400 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21017.695712] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=332 bytes=21757952 ep84_bytes=21757952 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21022.734774] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=389 bytes=25493504 ep84_bytes=25493504 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21027.774793] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=446 bytes=29229056 ep84_bytes=29229056 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21032.815953] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=503 bytes=32964608 ep84_bytes=32964608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21037.854760] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=560 bytes=36700160 ep84_bytes=36700160 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21042.871255] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=618 bytes=40501248 ep84_bytes=40501248 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21047.910998] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=675 bytes=44236800 ep84_bytes=44236800 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21052.949951] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=732 bytes=47972352 ep84_bytes=47972352 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21057.982722] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=788 bytes=51642368 ep84_bytes=51642368 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21063.024075] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=845 bytes=55377920 ep84_bytes=55377920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21068.042412] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=903 bytes=59179008 ep84_bytes=59179008 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21073.094898] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=960 bytes=62914560 ep84_bytes=62914560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21076.138177] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: clean up
$ gchd-profile delete T-091 composite
$ gchd-profile delete T-091 component
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,stretch_sd_input=0,allow_50_60_fps=1
--- dmesg step 7
[21078.840970] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== summary (T-091, 468c0f0 Remove a committed Python cache; ignore __pycache__)
PASS | step 1 install | menu entry named 'Elgato Game Capture HD'
PASS | step 2 gchd-profile: every setting back after a load | composite settings set
PASS | step 2 gchd-profile: every setting back after a load | save composite profile
PASS | step 2 gchd-profile: every setting back after a load | saved as numbers
PASS | step 2 gchd-profile: every setting back after a load | save component profile
PASS | step 2 gchd-profile: every setting back after a load | load composite profile
PASS | step 2 gchd-profile: every setting back after a load | composite profile: every setting back
PASS | step 2 gchd-profile: every setting back after a load | load component profile
PASS | step 2 gchd-profile: every setting back after a load | component profile: every setting back
PASS | step 3 load while capturing | load while capturing
PASS | step 3 load while capturing | settings of the profile while capturing
PASS | step 3 load while capturing | at most one encoder restart
PASS | step 3 load while capturing | The window (maybe reopened by itself) shows the PS3 paler and brighter (saturation -40, brightness +30)?
PASS | step 3 load while capturing | load the component profile while capturing
PASS | step 3 load while capturing | input changed while capturing
PASS | step 3 load while capturing | no driver error
PASS | step 4 settings window | Top of the window: a 'Profile' list with only the names (no input in brackets), and the buttons Load, Save, Delete (no Rename, no '...')?
PASS | step 4 settings window | window: component profile loaded
PASS | step 4 settings window | Window: Input shows Component and the sliders the profile's values (brightness -10, hue -30)? -- contrast also at 10 and saturation at 20.
PASS | step 4 settings window | window: new profile saved
PASS | step 4 settings window | window: profile replaced
PASS | step 4 settings window | window: profile deleted
PASS | step 5 text menu | text menu: composite profile loaded
PASS | step 5 text menu | text menu: new profile saved
PASS | step 5 text menu | Text menu: Profiles offered only Load, Save, Delete, and the lists showed only names?
PASS | step 6 OBS plugin | OBS: a 'Profiles' group with one Profile field (a list you can also type in) and the buttons Load, Save, Delete?
PASS | step 6 OBS plugin | OBS: component profile loaded
PASS | step 6 OBS plugin | OBS: the message "Profile 'T-091 component' loaded." and the properties show Component with brightness -10?
PASS | step 6 OBS plugin | OBS: new profile saved
PASS | step 6 OBS plugin | OBS: profile replaced
PASS | step 6 OBS plugin | OBS: profile deleted
