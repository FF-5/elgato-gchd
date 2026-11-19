# elgato_gchd — Linux driver for the Elgato Game Capture HD

A Linux kernel driver (V4L2, installable with DKMS) for the Elgato Game
Capture HD. The device appears as a standard video capture device
(`/dev/videoN`) that delivers H.264 video, or an MPEG transport stream with
H.264 video and MPEG audio. This is an unofficial driver, not supported by
Elgato.

## Status

| Input | State |
|---|---|
| Composite | Supported: NTSC 480i/240p, PAL 576i/288p, mode changes while capturing, output sizes, bitrate |
| S-video | Supported, as composite |
| Component | In development |
| HDMI | In development |

Supported device: Elgato Game Capture HD (USB `0fd9:005d`). The older
revisions `0fd9:0044`, `0fd9:004e` and `0fd9:0051` are recognized but not
tested. Not supported: Game Capture HD60, HD60 S, HD60 Pro.

## Requirements

- Linux with the kernel headers of the running kernel, `make`, `gcc`
- `dkms` (recommended, rebuilds the driver after kernel updates)
- Elgato's "Game Capture" installer for Windows, for the firmware (see Install)
- For viewing and recording: `v4l-utils`, `ffmpeg` (`ffplay`)

## Install

1. Download Elgato's "Game Capture" software for Windows
   (`GameCaptureSetup*.msi`) and put the file into the `firmware/`
   folder. The driver needs firmware from it that cannot be distributed
   with the driver.
2. Run the installer (double-click `install-driver.sh` in the file
   manager, or in a terminal):

   ```sh
   ./install-driver.sh
   ```

   It asks what to install: everything, the kernel driver only, or the
   OBS plugin only (`--all`, `--driver`, `--obs` choose without asking).

   - **Kernel driver:** checks for the firmware in `/lib/firmware/gchd/`
     and extracts it from the installer file when missing (it stops if
     that fails), builds the driver in `build/`, installs it with DKMS
     (when available; the driver is then rebuilt after kernel updates),
     installs the udev rules (remembered settings, device name
     `/dev/elgato-gchd`) and the settings window (application menu) and
     text menu, and loads the driver.
   - **OBS plugin:** builds the plugin in `build/obs-plugin` and installs
     it for the current user (`~/.config/obs-studio/plugins`). It needs
     the kernel driver and an OBS installed from the distribution (an OBS
     Flatpak cannot load it).

   Missing packages (kernel headers, compiler, `dkms`, `7z`, `whiptail`,
   `v4l-utils`, the GTK 3 Python bindings,
   the OBS and FFmpeg development packages, `cmake`) are named and can be
   installed on request.

Remove everything with `./install-driver.sh --uninstall`, or one part with
`--uninstall --driver` or `--uninstall --obs` (the firmware is kept).

### Manual installation

Firmware (needs `7z`):

```sh
sudo firmware/extract_firmware_windows path/to/GameCaptureSetup.msi
```

Driver with DKMS, and the udev rule:

```sh
sudo dkms add ./src/driver
sudo dkms install elgato-gchd/0.5.8
sudo modprobe elgato_gchd
sudo cp udev-rules/55-elgato-gchd-device.rules udev-rules/56-elgato-gchd-settings.rules /etc/udev/rules.d/
sudo udevadm control --reload
```

OBS plugin: see [`src/obs-plugin/README.md`](src/obs-plugin/README.md).

Firmware files and a build without DKMS: see [`src/driver/README.md`](src/driver/README.md).

## Use

### OBS Studio

The OBS source "Elgato Game Capture HD" ([`src/obs-plugin`](src/obs-plugin))
finds the device by itself and offers the input, output size and encoder
settings in its properties; video and audio are decoded in OBS. Without
the plugin, OBS's *Media Source* can be used: untick *Local File*, input
`/dev/elgato-gchd` (with the udev rule
[`udev-rules/55-elgato-gchd-device.rules`](udev-rules/55-elgato-gchd-device.rules)),
input format `mpegts`; deinterlacing in the source's context menu.

### Settings

"Elgato Game Capture HD Settings" in the application menu
([`src/settings/gchd-settings-gui`](src/settings/gchd-settings-gui))
changes the input, output size and encoder settings in a window (menus,
bitrate slider, switch); the driver remembers them. Changes apply at once,
also to a running capture, and the window shows changes made by other
programs. Composite and S-video share one group of settings, HDMI and
component another; each bitrate mode keeps its last bitrate. Settings that
do not apply to the selected input are hidden. Without a desktop:
[`src/settings/gchd-settings`](src/settings/gchd-settings) (text menu) or
`v4l2-ctl`.

### Command line

Select the input (0 HDMI, 1 component, 2 composite, 3 S-video) and watch
the transport stream with audio:

```sh
v4l2-ctl -d /dev/elgato-gchd -i 2
v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG
ffplay -f mpegts /dev/elgato-gchd
```

Record it:

```sh
ffmpeg -f mpegts -i /dev/elgato-gchd -c copy capture.ts
```

The output size (720 × 480/576, 640 × 480, 480 × 360), bitrate and other
settings are set with `v4l2-ctl` (`-v width=…,height=…`, `-c video_bitrate=…`);
see [`docs/video-output.md`](docs/video-output.md).

## Documentation

- [`src/driver/README.md`](src/driver/README.md): build, module parameters, remembered
  settings
- [`docs/`](docs/): video output, the device protocol per input

## Origin and credits

This driver grew out of the Game Capture HD Linux driver project by Tolga
Cakir and contributors (<https://github.com/tolga9009/elgato-gchd>), a
userspace driver that is no longer developed. Its research into the device
and its firmware made this driver possible; see
[`CREDITS.md`](CREDITS.md). The userspace program is not part of this
repository any more; it remains in the git history and in the original
repository.

## Licence

The repository has two licence files:

- [`LICENSE`](LICENSE): the GNU General Public License, version 2. The
  kernel driver and everything else in this repository are released under
  it (`MODULE_LICENSE("GPL")`).
- [`LICENSE.MIT`](LICENSE.MIT): the MIT License of the original project
  (copyright Tolga Cakir). Parts of the driver are derived from the
  original project's research and code; the MIT licence allows this and
  requires that its copyright and permission notice stay with them.
