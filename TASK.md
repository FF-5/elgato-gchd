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

Many comments:
- I think the menu shortcut doesn't need the word "Setting" at the end.
- The profile user interaction is too complicated. Especially in the OBS plugin.
- First none of the button need ellipses ("...").
- "Save as" Can be just "Save".
- There's no need for a "Rename" button. Upon clicking on "Save" I think it's best to have a save dialog where the user can choose to save as a new file or overwrite an existing file.
- I'm not sure of having the input source in brackets after the profile name. On one hand it's nice to have, but on the other hand users might want to do it on their own. I would prefer to let the user decide this, ie them writing in the profile name what input source the profile uses.

Here's the log:
gchd-test T-091 20261128-014310 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-091-20261128-014310.log
== build
$ git fetch -q origin
$ git checkout -q feat/profiles
$ git pull -q --ff-only
commit: 04ae5cc TASK: T-091 decisions, done so far, hardware test; TODO
commit: 04ae5cc TASK: T-091 decisions, done so far, hardware test; TODO
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
   - This kernel never originally had a module by this name
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mProfile command installed: gchd-profile (named sets of all settings).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the system settings menu (Preferences > Hardware; gchd-settings-gui); text menu: gchd-settings[0m

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

PROMPT: PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s.
$ command -v gchd-profile
/usr/local/bin/gchd-profile
[PASS] gchd-profile installed with the core
$ cat /usr/local/share/applications/elgato-gchd-settings.desktop
[Desktop Entry]
Type=Application
Name=Elgato Game Capture HD Settings
Comment=Input, output size, encoder and picture settings and profiles of the Elgato Game Capture HD
Exec=gchd-settings-gui
Icon=camera-video
Terminal=false
Categories=Settings;HardwareSettings;
[PASS] settings window in the system settings menu
--- dmesg step 1
[18677.096652] usbcore: deregistering interface driver elgato_gchd
[18685.879044] usbcore: registered new interface driver elgato_gchd
[18697.385902] workqueue: hub_event hogged CPU for >10000us 8 times, consider switching to WQ_UNBOUND
[18697.503285] usb 1-2: new high-speed USB device number 10 using xhci_hcd
[18697.915194] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[18697.915201] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[18697.915203] usb 1-2: Product: GameCapture HD
[18697.915205] usb 1-2: Manufacturer: Elgato
[18697.915207] usb 1-2: SerialNumber: 04107F1267
[18697.918866] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[18697.922147] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: gchd-profile: save, list, load
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
$ v4l2-ctl -d /dev/video0 -i 1
Video input set to 1 (Component: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c allow_50_60_fps=0,brightness=-10,contrast=10,saturation=20,hue=-30,analog_audio_gain_db=3
$ P save T-091 component
rc=0
[PASS] save component profile
$ sh -c gchd-profile list | tr '\n' ' '
T-091 component	1 T-091 composite	2 
[PASS] list: both with their inputs
$ sh -c gchd-profile list --input 2 | grep T-091 | tr '\n' ' '
T-091 composite	2 
[PASS] list --input 2: only composite
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,allow_50_60_fps=1
$ P load T-091 composite
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)'
rc=1
[FAIL] load composite profile (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)' rc=1)
$ cur
input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[FAIL] composite profile: every setting back (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bi)
$ P load T-091 component
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)'
rc=1
[FAIL] load component profile (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)' rc=1)
$ cur
input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bitrate: 11250000 video_bitrate_mode: 1 
[FAIL] component profile: every setting back (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bi)
$ P save T-091 component
gchd-profile: profile 'T-091 component' exists (--replace overwrites it)
rc=3
[PASS] save over an existing profile refused
$ P save T-091 component --replace
rc=0
[PASS] save --replace
$ P save ../x
gchd-profile: invalid profile name '../x' (letters, digits, space, '.', '_', '-'; at most 64)
rc=1
[PASS] invalid name refused
$ P rename T-091 component T-091 comp2
rc=0
[PASS] rename
$ gchd-profile list
T-091 comp2	1
T-091 composite	2
[PASS] renamed in the list
$ P rename T-091 comp2 T-091 component
rc=0
[PASS] rename back
$ P load T-091 none
gchd-profile: no profile 'T-091 none'
rc=3
[PASS] load of a missing profile
--- dmesg step 2
[18703.731509] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[18703.803712] elgato_gchd 1-2:1.0: input switch requested: old=2 new=1 hw_initialized=0 streaming=0
[18703.803741] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open

=== step 3: load while capturing
$ gchd-profile -d /dev/video0 load T-091 composite
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)'
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,stretch_sd_input=0
PROMPT: A window 'Elgato composite' shows the PS3 in normal colours.
$ P load T-091 composite
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)'
rc=1
[FAIL] load while capturing (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)' rc=1)
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bitrate: 14000000 video_bitrate_mode: 0 
[FAIL] settings of the profile while capturing (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
$ count_new encoder restart
0
[PASS] at most one encoder restart
[FAIL] The window (maybe reopened by itself) shows the PS3 paler and brighter (saturation -40, brightness +30)?
$ P load T-091 component
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)'
rc=1
[FAIL] load the component profile while capturing (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)' rc=1)
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bitrate: 14000000 video_bitrate_mode: 0 
[FAIL] input changed while capturing (expected /^input=1 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
$ count_new failed|error
1
[FAIL] no driver error (expected /^0$/, got: 1)
$ gchd-profile -d /dev/video0 load T-091 composite
gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)'
--- dmesg step 3
[18704.075503] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[18704.075553] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[18704.197900] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18704.331404] elgato_gchd 1-2:1.0: transcoder defaults loaded
[18704.388676] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[18704.739368] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[18704.741733] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[18704.741738] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[18704.741741] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[18704.741744] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[18705.508029] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[18705.715653] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[18705.730853] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18705.731168] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18705.736288] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18705.736974] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[18705.737393] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18705.876742] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18706.128154] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[18708.506891] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[18708.507194] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[18708.512302] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18708.512797] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[18708.513238] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18708.632581] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[18708.632583] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[18709.026789] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[18713.566689] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=51 bytes=3342336 ep84_bytes=3342336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18718.607808] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=108 bytes=7077888 ep84_bytes=7077888 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=33
[18723.670251] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=165 bytes=10813440 ep84_bytes=10813440 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18728.708434] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=222 bytes=14548992 ep84_bytes=14548992 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18733.784013] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=279 bytes=18284544 ep84_bytes=18284544 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18738.838604] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=336 bytes=22020096 ep84_bytes=22020096 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18743.936303] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=392 bytes=25690112 ep84_bytes=25690112 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=30
[18749.025263] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=30
[18754.031635] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=506 bytes=33161216 ep84_bytes=33161216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=31
[18759.104100] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=562 bytes=36831232 ep84_bytes=36831232 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=30
[18764.112663] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=619 bytes=40566784 ep84_bytes=40566784 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18769.185804] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=676 bytes=44302336 ep84_bytes=44302336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=32
[18772.450218] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: settings window
PROMPT: Open the application menu: find 'Elgato Game Capture HD Settings' and open it.
[PASS] Was it among the system settings (for example System > Preferences > Hardware), not under Sound & Video?
[PASS] Top of the window: a 'Profile' list with 'T-091 composite (Composite)' first, and buttons Load, Save as..., Rename..., Delete?
PROMPT: Choose 'T-091 component (Component)' and click Load; wait 3 s.
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bitrate: 14000000 video_bitrate_mode: 0 
[FAIL] window: component profile loaded (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
[FAIL] Window: Input shows Component and the sliders the profile's values (brightness -10, hue -30)? -- It failed to open due to an invalid bitrate or something.
PROMPT: Click 'Save as...', type T-091 window, OK.
$ cat /home/ff5/.config/elgato-gchd/profiles/T-091 window.conf
# elgato-gchd profile 1
input=2
output=1
video_bitrate_mode=0 (Variable Bitrate)
video_bitrate=14000000
stretch_sd_input=0
allow_50_60_fps=1
hdmi_color_range=0 (Standard)
brightness=0
contrast=0
saturation=0
hue=0
analog_audio_gain_db=0
[FAIL] window: profile saved (expected /input=1/, got: # elgato-gchd profile 1 input=2 output=1 video_bitrate_mode=0 (Variable Bitrate) video_bitrate=14000000 stretch_sd_input=0 allow_50_60_fps=1 hdmi_color_range=0 )
PROMPT: Choose 'T-091 window', click Delete and confirm with Yes. Close the window.
$ sh -c [ -e '/home/ff5/.config/elgato-gchd/profiles/T-091 window.conf' ] && echo there || echo gone
gone
[PASS] window: profile deleted
--- dmesg step 4


=== step 5: text menu
PROMPT: Run gchd-settings in another terminal: Profiles, Load a profile, choose 'T-091 composite'; OK; then Quit.
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bitrate: 14000000 video_bitrate_mode: 0 
[FAIL] text menu: composite profile loaded (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
--- dmesg step 5


=== step 6: OBS plugin
PROMPT: Start OBS (source 'Elgato Game Capture HD'); open the source's properties.
[PASS] OBS: a 'Profiles' group with a Profile list, Load profile, Save to this profile, Delete profile, New name, Save as new profile, Rename to the new name? -- It's too complicated. Buttons with too much text, too many buttons, etc.
PROMPT: Choose 'T-091 component (Component)', click 'Load profile'; wait 3 s.
$ cur
input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bitrate: 14000000 video_bitrate_mode: 0 
[FAIL] OBS: component profile loaded (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
[FAIL] OBS: the message 'Profile ... loaded.' and the properties show Component with brightness -10? -- Nothing happened.
PROMPT: Type T-091 obs in 'New name', click 'Save as new profile'.
$ cat /home/ff5/.config/elgato-gchd/profiles/T-091 obs.conf
# elgato-gchd profile 1
input=2
output=1
video_bitrate_mode=0 (Variable Bitrate)
video_bitrate=14000000
stretch_sd_input=0
allow_50_60_fps=1
hdmi_color_range=0 (Standard)
brightness=0
contrast=0
saturation=0
hue=0
analog_audio_gain_db=0
[FAIL] OBS: profile saved (expected /input=1/, got: # elgato-gchd profile 1 input=2 output=1 video_bitrate_mode=0 (Variable Bitrate) video_bitrate=14000000 stretch_sd_input=0 allow_50_60_fps=1 hdmi_color_range=0 )
PROMPT: Choose 'T-091 obs', click 'Delete profile' once (a question appears), then again.
$ sh -c [ -e '/home/ff5/.config/elgato-gchd/profiles/T-091 obs.conf' ] && echo there || echo gone
there
[FAIL] OBS: profile deleted (expected /^gone$/, got: there)
PROMPT: Close the properties (OK) and OBS.
--- dmesg step 6
[19143.109667] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[19143.268955] elgato_gchd 1-2:1.0: transcoder defaults loaded
[19143.348899] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[19143.452813] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[19143.584020] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[19143.586298] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[19143.586301] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[19143.586308] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[19143.586310] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[19144.525806] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[19144.783053] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[19144.802041] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[19144.802422] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[19144.807876] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[19144.808415] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[19144.809031] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[19144.930403] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[19145.184036] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[19147.751073] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[19147.751431] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[19147.757041] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[19147.757697] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[19147.758522] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[19147.897738] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[19147.897742] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[19148.289888] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[19152.773361] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=51 bytes=3342336 ep84_bytes=3342336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19157.841003] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=108 bytes=7077888 ep84_bytes=7077888 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19162.898107] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=167 bytes=10944512 ep84_bytes=10944512 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19167.943179] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=225 bytes=14745600 ep84_bytes=14745600 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19172.990082] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=283 bytes=18546688 ep84_bytes=18546688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19177.991642] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=340 bytes=22282240 ep84_bytes=22282240 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19183.041849] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=397 bytes=26017792 ep84_bytes=26017792 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19188.098571] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=456 bytes=29884416 ep84_bytes=29884416 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19193.152692] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=514 bytes=33685504 ep84_bytes=33685504 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19198.210586] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=572 bytes=37486592 ep84_bytes=37486592 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19203.219163] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=629 bytes=41222144 ep84_bytes=41222144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19208.267102] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=687 bytes=45023232 ep84_bytes=45023232 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19213.332165] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=745 bytes=48824320 ep84_bytes=48824320 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19218.337603] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=802 bytes=52559872 ep84_bytes=52559872 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19223.389323] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=860 bytes=56360960 ep84_bytes=56360960 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19228.399826] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=916 bytes=60030976 ep84_bytes=60030976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19233.462175] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=975 bytes=63897600 ep84_bytes=63897600 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19238.532282] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1033 bytes=67698688 ep84_bytes=67698688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19243.541305] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1090 bytes=71434240 ep84_bytes=71434240 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19248.612134] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1148 bytes=75235328 ep84_bytes=75235328 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19253.624017] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1205 bytes=78970880 ep84_bytes=78970880 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19258.689464] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1263 bytes=82771968 ep84_bytes=82771968 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19263.700296] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1320 bytes=86507520 ep84_bytes=86507520 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19268.752505] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1378 bytes=90308608 ep84_bytes=90308608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19273.760956] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1434 bytes=93978624 ep84_bytes=93978624 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19278.826623] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1493 bytes=97845248 ep84_bytes=97845248 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19283.890515] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1551 bytes=101646336 ep84_bytes=101646336 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19288.900855] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1608 bytes=105381888 ep84_bytes=105381888 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19293.974163] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1666 bytes=109182976 ep84_bytes=109182976 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19299.013905] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1723 bytes=112918528 ep84_bytes=112918528 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19304.025193] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1780 bytes=116654080 ep84_bytes=116654080 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19309.040202] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1836 bytes=120324096 ep84_bytes=120324096 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19314.110664] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1895 bytes=124190720 ep84_bytes=124190720 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19319.144887] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1952 bytes=127926272 ep84_bytes=127926272 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19324.161827] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2008 bytes=131596288 ep84_bytes=131596288 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19329.168315] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2065 bytes=135331840 ep84_bytes=135331840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19334.175014] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2123 bytes=139132928 ep84_bytes=139132928 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19339.281671] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2180 bytes=142868480 ep84_bytes=142868480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19344.287753] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2237 bytes=146604032 ep84_bytes=146604032 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19349.298716] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2295 bytes=150405120 ep84_bytes=150405120 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19354.400737] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2352 bytes=154140672 ep84_bytes=154140672 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19359.469617] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2411 bytes=158007296 ep84_bytes=158007296 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19364.495456] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2468 bytes=161742848 ep84_bytes=161742848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19369.538224] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=2515 bytes=164823040 ep84_bytes=164823040 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=7
[19369.756024] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 7: clean up
$ gchd-profile -d /dev/video0 delete T-091 composite
$ gchd-profile -d /dev/video0 delete T-091 component
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=480
$ v4l2-ctl -d /dev/video0 -c brightness=0,contrast=0,saturation=0,hue=0,analog_audio_gain_db=0,stretch_sd_input=0,allow_50_60_fps=1
--- dmesg step 7
[19372.151676] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== summary (T-091, 04ae5cc TASK: T-091 decisions, done so far, hardware test; TODO)
PASS | step 1 install | gchd-profile installed with the core
PASS | step 1 install | settings window in the system settings menu
PASS | step 2 gchd-profile: save, list, load | composite settings set
PASS | step 2 gchd-profile: save, list, load | save composite profile
PASS | step 2 gchd-profile: save, list, load | save component profile
PASS | step 2 gchd-profile: save, list, load | list: both with their inputs
PASS | step 2 gchd-profile: save, list, load | list --input 2: only composite
FAIL | step 2 gchd-profile: save, list, load | load composite profile (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)' rc=1)
FAIL | step 2 gchd-profile: save, list, load | composite profile: every setting back (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bi)
FAIL | step 2 gchd-profile: save, list, load | load component profile (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)' rc=1)
FAIL | step 2 gchd-profile: save, list, load | component profile: every setting back (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=1 size=1280x720 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 1 video_bi)
PASS | step 2 gchd-profile: save, list, load | save over an existing profile refused
PASS | step 2 gchd-profile: save, list, load | save --replace
PASS | step 2 gchd-profile: save, list, load | invalid name refused
PASS | step 2 gchd-profile: save, list, load | rename
PASS | step 2 gchd-profile: save, list, load | renamed in the list
PASS | step 2 gchd-profile: save, list, load | rename back
PASS | step 2 gchd-profile: save, list, load | load of a missing profile
FAIL | step 3 load while capturing | load while capturing (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 composite.conf: invalid video_bitrate_mode '0 (Variable Bitrate)' rc=1)
FAIL | step 3 load while capturing | settings of the profile while capturing (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
PASS | step 3 load while capturing | at most one encoder restart
FAIL | step 3 load while capturing | The window (maybe reopened by itself) shows the PS3 paler and brighter (saturation -40, brightness +30)?
FAIL | step 3 load while capturing | load the component profile while capturing (expected /rc=0/, got: gchd-profile: /home/ff5/.config/elgato-gchd/profiles/T-091 component.conf: invalid video_bitrate_mode '1 (Constant Bitrate)' rc=1)
FAIL | step 3 load while capturing | input changed while capturing (expected /^input=1 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
FAIL | step 3 load while capturing | no driver error (expected /^0$/, got: 1)
PASS | step 4 settings window | Was it among the system settings (for example System > Preferences > Hardware), not under Sound & Video?
PASS | step 4 settings window | Top of the window: a 'Profile' list with 'T-091 composite (Composite)' first, and buttons Load, Save as..., Rename..., Delete?
FAIL | step 4 settings window | window: component profile loaded (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
FAIL | step 4 settings window | Window: Input shows Component and the sliders the profile's values (brightness -10, hue -30)? -- It failed to open due to an invalid bitrate or something.
FAIL | step 4 settings window | window: profile saved (expected /input=1/, got: # elgato-gchd profile 1 input=2 output=1 video_bitrate_mode=0 (Variable Bitrate) video_bitrate=14000000 stretch_sd_input=0 allow_50_60_fps=1 hdmi_color_range=0 )
PASS | step 4 settings window | window: profile deleted
FAIL | step 5 text menu | text menu: composite profile loaded (expected /input=2 size=640x480 .*analog_audio_gain_db: -4 brightness: 30 contrast: -20 .*hue: 15 saturation: -40 stretch_sd_input: 1 video_bitrate: 14000000 video_bitrate_mode: 0/, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
PASS | step 6 OBS plugin | OBS: a 'Profiles' group with a Profile list, Load profile, Save to this profile, Delete profile, New name, Save as new profile, Rename to the new name? -- It's too complicated. Buttons with too much text, too many buttons, etc.
FAIL | step 6 OBS plugin | OBS: component profile loaded (expected /input=1 .*allow_50_60_fps: 0 analog_audio_gain_db: 3 brightness: -10 contrast: 10 .*hue: -30 saturation: 20 /, got: input=2 size=640x480 allow_50_60_fps: 1 analog_audio_gain_db: 0 brightness: 0 contrast: 0 hdmi_color_range: 0 hue: 0 saturation: 0 stretch_sd_input: 0 video_bit)
FAIL | step 6 OBS plugin | OBS: the message 'Profile ... loaded.' and the properties show Component with brightness -10? -- Nothing happened.
FAIL | step 6 OBS plugin | OBS: profile saved (expected /input=1/, got: # elgato-gchd profile 1 input=2 output=1 video_bitrate_mode=0 (Variable Bitrate) video_bitrate=14000000 stretch_sd_input=0 allow_50_60_fps=1 hdmi_color_range=0 )
FAIL | step 6 OBS plugin | OBS: profile deleted (expected /^gone$/, got: there)
