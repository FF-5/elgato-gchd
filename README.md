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
- The device firmware (below)
- For viewing and recording: `v4l-utils`, `ffmpeg` (`ffplay`)

## Firmware

The device needs firmware files that cannot be distributed with the driver.
They are extracted from Elgato's "Game Capture" installer for Windows
(`GameCaptureSetup*.msi`, available from Elgato) with the script in
[`firmware/`](firmware/), which needs `p7zip`:

```sh
cd firmware
# copy GameCaptureSetup*.msi into this directory, then:
sudo ./extract_firmware_windows
```

The files are installed to `/lib/firmware/gchd/`. Details:
[`src/README.md`](src/README.md#firmware).

## Install

With DKMS:

```sh
sudo dkms add ./src
sudo dkms install elgato-gchd/0.5.6
sudo modprobe elgato_gchd
```

Remembered settings (input, output size, format, bitrate across replug and
reboot) need the udev rule:

```sh
sudo cp udev-rules/56-elgato-gchd-settings.rules /etc/udev/rules.d/
sudo udevadm control --reload
```

Direct build without DKMS: see [`src/README.md`](src/README.md).

## Use

Select the input (0 HDMI, 1 component, 2 composite, 3 S-video) and watch
the transport stream with audio:

```sh
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ffplay -f mpegts /dev/video0
```

Record it:

```sh
ffmpeg -f mpegts -i /dev/video0 -c copy capture.ts
```

The output size (720 × 480/576, 640 × 480, 480 × 360), bitrate and other
settings are set with `v4l2-ctl` (`-v width=…,height=…`, `-c video_bitrate=…`);
see [`docs/video-output.md`](docs/video-output.md).

## Documentation

- [`src/README.md`](src/README.md): build, module parameters, remembered
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

The kernel driver is released under the GNU General Public License,
version 2 (`MODULE_LICENSE("GPL")`). The original project is licensed under
the MIT License; its copyright notice and permission text are in
[`LICENSE`](LICENSE).
