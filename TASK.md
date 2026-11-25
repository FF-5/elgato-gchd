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
gchd-test T-096 20261125-214721 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-096-20261125-214721.log
== build
$ git fetch -q origin
$ git checkout -q fix/stretch
$ git pull -q --ff-only
commit: 7c0f218 Installer: core always, settings programs and OBS plugin optional (asked for, or --core/--settings/--obs/--all); stretch documented as in the Windows software's files; version 0.5.10 (T-096)
commit: 7c0f218 Installer: core always, settings programs and OBS plugin optional (asked for, or --core/--settings/--obs/--all); stretch documented as in the Windows software's files; version 0.5.10 (T-096)
build rc=0 warnings=0

=== step 1: installer: uninstall everything, then the core only (answers: no, no)
$ sudo rmmod elgato_gchd
rmmod: ERROR: Module elgato_gchd is not currently loaded
$ bash /home/ff5/elgato-gchd/install-driver.sh --uninstall --in-terminal

[1m==> Removing the OBS plugin[0m
[32mOBS plugin removed (restart OBS).[0m

[1m==> Removing the settings programs[0m
[32mSettings programs removed (the driver keeps its settings).[0m

[1m==> Removing the driver[0m
Error! The module/version combo: elgato-gchd-0.5.10 is not located in the DKMS tree.
[32mDriver removed. The firmware in /lib/firmware/gchd was kept.[0m

$ have /usr/local/bin/gchd-settings-gui
absent
[PASS] settings programs removed
$ have /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so
absent
[PASS] plugin removed
$ sh -c printf 'n\nn\n' | bash '/home/ff5/elgato-gchd/install-driver.sh' --in-terminal
[1mThe driver (core) is always installed. Optional parts:[0m
[1mElgato Game Capture HD driver 0.5.10[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.10/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
Module version 0.5.8 for elgato_gchd.ko.zst
exactly matches what is already found in kernel 6.8.0-146-generic.
DKMS will not replace this module.
You may override by specifying --force.
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

$ sh -c modinfo -n elgato_gchd
/lib/modules/6.8.0-146-generic/updates/dkms/elgato_gchd.ko.zst
[PASS] core: driver installed
$ sh -c ls /etc/udev/rules.d | tr '\n' ' '
55-elgato-gchd-device.rules 56-elgato-gchd-settings.rules 60-vboxadd.rules 70-persistent-cd.rules 
[PASS] core: udev rules installed
$ have /usr/local/bin/gchd-settings-gui
absent
[PASS] no settings programs
$ have /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so
absent
[PASS] no plugin
PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
$ ls -l /dev/elgato-gchd
lrwxrwxrwx 1 root root 6 Nov 25 21:47 /dev/elgato-gchd -> video0
[PASS] /dev/elgato-gchd present
--- dmesg step 1
[  485.769484] mc: Linux media interface: v0.10
[  485.779958] videodev: Linux video capture interface: v2.00
[  485.785054] usbcore: registered new interface driver elgato_gchd
[  495.603672] usb 1-2: new high-speed USB device number 3 using xhci_hcd
[  495.993253] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  495.993270] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  495.993275] usb 1-2: Product: GameCapture HD
[  495.993279] usb 1-2: Manufacturer: Elgato
[  495.993282] usb 1-2: SerialNumber: 04107F1267
[  496.003654] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  496.009921] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 2: installer: optional parts by option
$ bash /home/ff5/elgato-gchd/install-driver.sh --settings --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.10[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.10/build/elgato_gchd.ko
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
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the application menu (gchd-settings-gui); text menu: gchd-settings[0m

$ have /usr/local/bin/gchd-settings-gui
present
[PASS] settings window installed
$ have /usr/local/bin/gchd-settings
present
[PASS] text menu installed
$ have /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so
absent
[PASS] still no plugin
$ bash /home/ff5/elgato-gchd/install-driver.sh --uninstall --settings --in-terminal

[1m==> Removing the settings programs[0m
[32mSettings programs removed (the driver keeps its settings).[0m

$ have /usr/local/bin/gchd-settings-gui
absent
[PASS] settings programs removed
$ sh -c modinfo -n elgato_gchd
/lib/modules/6.8.0-146-generic/updates/dkms/elgato_gchd.ko.zst
[PASS] driver kept
$ bash /home/ff5/elgato-gchd/install-driver.sh --obs --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.10[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.10/build/elgato_gchd.ko
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
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

$ have /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so
present
[PASS] plugin installed
$ have /usr/local/bin/gchd-settings-gui
absent
[PASS] still no settings programs
--- dmesg step 2
[  504.106273] usbcore: deregistering interface driver elgato_gchd
[  509.804094] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  509.810492] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  509.810520] usbcore: registered new interface driver elgato_gchd
[  514.838940] usbcore: deregistering interface driver elgato_gchd
[  520.403522] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  520.410670] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  520.410868] usbcore: registered new interface driver elgato_gchd

=== step 3: installer: questions answered with Enter (both optional parts)
$ sh -c printf '\n\n' | bash '/home/ff5/elgato-gchd/install-driver.sh' --in-terminal
[1mThe driver (core) is always installed. Optional parts:[0m
[1mElgato Game Capture HD driver 0.5.10[0m

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
Signing module /var/lib/dkms/elgato-gchd/0.5.10/build/elgato_gchd.ko
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
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

[1m==> Settings programs[0m
[32mSettings installed: 'Elgato Game Capture HD Settings' in the application menu (gchd-settings-gui); text menu: gchd-settings[0m

[1m==> OBS plugin[0m
[32mOBS plugin installed in /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd. Restart OBS and add the source 'Elgato Game Capture HD'.[0m

$ have /usr/local/bin/gchd-settings-gui
present
[PASS] settings window installed
$ have /home/ff5/.config/obs-studio/plugins/obs-elgato-gchd/bin/64bit/obs-elgato-gchd.so
present
[PASS] plugin installed
[FAIL] the installer asked two questions (settings programs, OBS plugin), each with [Y/n]? -- I was never prompted to select anything nor do I see any trace of a selection that was done automatically by the test script.
PROMPT: Unplug the Elgato and plug it in again; wait 5 s.
--- dmesg step 3
[  526.371878] usbcore: deregistering interface driver elgato_gchd
[  531.806908] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  531.812490] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  531.812549] usbcore: registered new interface driver elgato_gchd
[  818.729108] usb 1-2: USB disconnect, device number 3
[  823.798314] usb 1-2: new high-speed USB device number 4 using xhci_hcd
[  824.218635] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  824.218647] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  824.218651] usb 1-2: Product: GameCapture HD
[  824.218654] usb 1-2: Manufacturer: Elgato
[  824.218656] usb 1-2: SerialNumber: 04107F1267
[  824.224658] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  824.226986] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON

=== step 4: stretch in ffplay
$ v4l2-ctl -d /dev/elgato-gchd -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/elgato-gchd -v width=720,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/elgato-gchd -c stretch_sd_input=0
ffplay for 12s
[PASS] stretch off: normal 4:3 picture?
$ v4l2-ctl -d /dev/elgato-gchd -c stretch_sd_input=1
ffplay for 12s
[PASS] stretch on: the same picture horizontally stretched (wider window), as in the Windows software's preview? -- I guess this is fine. I was just under the impression that Windows keeps the output at 480/576, but I guess this is fine.
--- dmesg step 4
[  827.895200] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[  827.895255] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  828.447425] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  828.664304] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  828.740224] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  829.223662] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[  829.226460] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[  829.226466] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[  829.226469] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[  829.226471] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[  830.028001] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  830.271794] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  830.292845] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  830.293106] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  830.299119] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  830.300275] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[  830.300796] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  830.418706] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  835.326745] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[  840.353314] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=56
[  840.730219] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[  848.493214] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  848.597063] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  848.659944] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  848.771758] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[  848.842014] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[  848.844272] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[  848.844275] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[  848.844277] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[  848.844279] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[  849.626087] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  849.881137] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  849.899933] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  849.900142] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  849.906427] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  849.906971] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[  849.907691] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[  850.025753] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  854.934940] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[  859.960075] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[  861.247327] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 5: stretch in OBS
PROMPT: Start OBS with the source 'Elgato Game Capture HD' shown; stretch is on; wait 5 s.
[PASS] the plugin's picture is horizontally stretched (wider than 4:3), as in the Windows software?
PROMPT: Add a Media Source (untick Local File, input /dev/elgato-gchd, input format mpegts) next to it, or open your existing one; wait 5 s. (Hide the plugin's source first: only one program can capture.)
[PASS] the Media Source shows the picture NOT stretched (OBS ignores the aspect ratio; documented)?
PROMPT: Remove or hide the Media Source, close OBS.
$ v4l2-ctl -d /dev/elgato-gchd -c stretch_sd_input=0
--- dmesg step 5
[ 1044.598990] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1044.740901] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1044.814234] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1044.841967] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1044.995951] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[ 1044.998947] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[ 1044.998952] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 1044.998954] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1044.998956] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1045.934410] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1046.167538] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1046.194778] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1046.195012] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1046.207133] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1046.208744] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1046.209837] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1046.340732] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1051.205978] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1056.231506] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1061.257821] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1066.283295] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1071.309017] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1076.335797] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1081.363097] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1086.389424] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1091.414624] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1096.441598] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1098.850492] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[ 1127.239684] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[ 1127.329897] elgato_gchd 1-2:1.0: transcoder defaults loaded
[ 1127.399244] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[ 1127.433923] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[ 1127.642038] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[ 1127.644516] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1127.714619] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[ 1127.716825] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[ 1127.778717] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[ 1127.812776] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[ 1127.815303] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[ 1127.815308] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[ 1127.815312] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1127.815315] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1128.799502] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[ 1129.039792] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[ 1129.063310] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[ 1129.063546] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[ 1129.070421] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[ 1129.071327] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[ 1129.071968] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[ 1129.189138] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[ 1134.097234] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[ 1139.123971] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1144.149397] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1149.175666] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1154.201544] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1159.229951] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1164.254771] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1169.281679] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=909 bytes=59572224 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1174.306287] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1024 bytes=67108864 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1179.333714] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1139 bytes=74645504 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[ 1184.359008] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1254 bytes=82182144 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=2
[ 1189.384286] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1369 bytes=89718784 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=1
[ 1193.362324] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-096, 7c0f218 Installer: core always, settings programs and OBS plugin optional (asked for, or --core/--settings/--obs/--all); stretch documented as in the Windows software's files; version 0.5.10 (T-096))
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | settings programs removed
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | plugin removed
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | core: driver installed
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | core: udev rules installed
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | no settings programs
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | no plugin
PASS | step 1 installer: uninstall everything, then the core only (answers: no, no) | /dev/elgato-gchd present
PASS | step 2 installer: optional parts by option | settings window installed
PASS | step 2 installer: optional parts by option | text menu installed
PASS | step 2 installer: optional parts by option | still no plugin
PASS | step 2 installer: optional parts by option | settings programs removed
PASS | step 2 installer: optional parts by option | driver kept
PASS | step 2 installer: optional parts by option | plugin installed
PASS | step 2 installer: optional parts by option | still no settings programs
PASS | step 3 installer: questions answered with Enter (both optional parts) | settings window installed
PASS | step 3 installer: questions answered with Enter (both optional parts) | plugin installed
FAIL | step 3 installer: questions answered with Enter (both optional parts) | the installer asked two questions (settings programs, OBS plugin), each with [Y/n]? -- I was never prompted to select anything nor do I see any trace of a selection that was done automatically by the test script.
PASS | step 4 stretch in ffplay | stretch off: normal 4:3 picture?
PASS | step 4 stretch in ffplay | stretch on: the same picture horizontally stretched (wider window), as in the Windows software's preview? -- I guess this is fine. I was just under the impression that Windows keeps the output at 480/576, but I guess this is fine.
PASS | step 5 stretch in OBS | the plugin's picture is horizontally stretched (wider than 4:3), as in the Windows software?
PASS | step 5 stretch in OBS | the Media Source shows the picture NOT stretched (OBS ignores the aspect ratio; documented)?
