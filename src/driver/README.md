# elgato_gchd — V4L2 driver for the Elgato Game Capture HD

Linux kernel driver for the Elgato Game Capture HD (`0fd9:0044`,
`0fd9:004e`, `0fd9:0051`, `0fd9:005d`). The USB protocol is described in
[`../../docs`](../../docs).

> **Status:** composite and S-video are supported (NTSC and PAL, all modes,
> output sizes, bitrate); component and HDMI are in development.

## Firmware

The device needs two firmware files, loaded through the kernel firmware
loader from `/lib/firmware/gchd/`:

| Model | Files |
|---|---|
| `0044`, `004e`, `0051` | `gchd/MB86H57_H58_IDLE`, `gchd/MB86H57_H58_ENC_H` |
| `005d` | `gchd/MB86M01_ASSP_NSEC_IDLE`, `gchd/MB86M01_ASSP_NSEC_ENC_H` |

The files are not distributed with the driver. `../firmware/`
contains a script that extracts them from the vendor installer.

## Build and install

Direct build:

```sh
make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo insmod elgato_gchd.ko
```

DKMS:

```sh
sudo dkms add ./src/driver
sudo dkms install elgato-gchd/0.5.10
```

## Module parameters

| Parameter | Default | Meaning |
|---|---|---|
| `trace_mailbox` | `N` | Log every mailbox transfer. |
| `ring_bytes` | 64 MiB | Maximum size of the frame buffer. |
| `signal_active_retries` | 3 | Signal measurement retries when timing is detected. |
| `signal_inactive_retries` | 1 | Signal measurement retries when no timing is detected. |
| `signal_poll_delay_ms` | 200 | Delay between signal measurement batches. |
| `timing` | `N` | Log latency marks (`gchd-timing: <µs> <event>`): requests, device commands and their completion, source changes, first stream data and first buffer after START. |
| `input` | 0 | Input at load: 0 HDMI, 1 component, 2 composite, 3 S-video. |
| `output` | 0 | Composite/S-video output size: 0 source size (720 × 480 or 720 × 576), 1 640 × 480, 2 480 × 360. |
| `format` | 0 | Pixel format: 0 H.264, 1 MPEG transport stream. |
| `out_bitrate_mode` | `1,1,0,1,1` | Bitrate mode per output size (0 VBR, 1 CBR), in this order: composite/S-video source size, 640 × 480, 480 × 360 (VBR only), 1280 × 720, HDMI/component source size. |
| `out_bitrate_cbr` | `0,0,0,0,0` | Last CBR bitrate per output size (same order) in kbit/s, 0 automatic. |
| `out_bitrate_vbr` | `0,0,0,0,0` | Last VBR bitrate per output size (same order) in kbit/s (the control value, twice the average), 0 automatic. |
| `hd_output` | 0 | Component output size: 0 source size, 1 640 × 480, 2 480 × 360, 3 1280 × 720 (sizes the source does not allow give the source size). |
| `allow_5060` | 1 | Component: 1 encodes 720p at 60 and 576p/480p at 50/60 frames per second with the short GOP; 0 encodes 720p at 30 frames per second and 576p/480p with the long GOP. |
| `stretch` | 0 | Stretch standard-definition input (composite/S-video). |
| `h264_profile` | 4 | Composite/S-video H.264 profile (V4L2 value): 0 baseline, 2 main, 4 high. |
| `h264_level` | 0 | Composite/S-video H.264 level × 10 (30–51), 0 automatic. |
| `std_composite` | 0 | Last standard detected on composite: 0 unknown, 1 NTSC (60 Hz), 2 PAL (50 Hz); sets the format before the first detection. |
| `std_svideo` | 0 | The same for S-video. |
| `hd_h264_profile` | 4 | HDMI/component H.264 profile (V4L2 value). |
| `hd_h264_level` | 0 | HDMI/component H.264 level × 10 (30–51), 0 automatic. |
| `bitrate`, `bitrate_mode`, `bitrate_cbr`, `bitrate_vbr`, `bitrate_small`, `hd_bitrate_mode`, `hd_bitrate_cbr`, `hd_bitrate_vbr` | | Older settings files (bitrate settings per group): read only when no `out_bitrate_*` parameter is given; composite/S-video values for its output sizes, HDMI/component values for 1280 × 720 and its source size. |

### Settings per output size

Each output size keeps its own bitrate settings, the same for every input:
the bitrate mode last used there and the bitrate last set in each mode. The
sizes are the composite/S-video source size, 640 × 480, 480 × 360 (VBR
only), 1280 × 720 and the HDMI/component source size (its bitrates differ
from those of standard definition). A change of the output size or the
input loads the new size's mode and bitrate into the controls; a change of
`video_bitrate_mode` loads the bitrate last set in that mode, and
`video_bitrate` changes the bitrate in use. A kept component bitrate outside
the range of the output in effect gives the highest bitrate of that output.
The bitrate control is volatile: reading it always gives the bitrate in use.

H.264 profile and level are kept per group: composite and S-video share
one, HDMI and component another. Output size and stretch apply to composite
and S-video; output size and "Allow 50/60 fps" to component.

## Remembered settings

The parameters from `input` to `hd_h264_level` (except the older-file ones) hold the settings in use. When
an application changes one of them (or a new standard is detected), the
driver updates the parameters and sends a `change` uevent with all values. The udev rule
[`../../udev-rules/56-elgato-gchd-settings.rules`](../../udev-rules/56-elgato-gchd-settings.rules)
writes them to `/etc/modprobe.d/elgato-gchd-settings.conf`, so the next load
(unplug and replug, module reload, reboot) starts with the same settings.
Without settings (first use) the driver starts with HDMI and the defaults.

```sh
sudo cp ../../udev-rules/56-elgato-gchd-settings.rules /etc/udev/rules.d/
sudo udevadm control --reload
```

Delete `/etc/modprobe.d/elgato-gchd-settings.conf` to return to the
defaults. `modprobe` applies the file; with `insmod` pass the options by
hand.

## Video device

Inputs: HDMI (0), Component (1), Composite (2), S-Video (3). The hardware is
initialized on `VIDIOC_STREAMON`. The output format is described in
[`../../docs/video-output.md`](../../docs/video-output.md).

## Source layout

| File | Contents |
|---|---|
| `gchd_protocol.h` | Protocol constants: USB requests, register banks and registers, enable bits, SCMD commands and states, mailbox ports and devices, bridge commands, device registers, TS PIDs. |
| `gchd_mailbox.h` | Device access macros: `GCHD_FE_W/_R/_RX` (front end `0x9C`), `GCHD_DEC_*` (decoder `0x88`), `GCHD_BE_*` (back end `0x98`), `GCHD_AUX_W` (`0x94`), `GCHD_BRIDGE_W/_R/_SKIP`, `GCHD_TC_SET` (encoder field), `GCHD_SEQ_*` (table entries). These macros return from the calling function on error. |
| `gchd_transcoder_fields.h` | Encoder register fields, `GCHD_TC_<FIELD>` = `address, lsb, bits`. |
| `elgato_gchd.h` | Driver structures, configuration and prototypes. |
| `usb.c` | Control transfers, mailbox transport, SCMD and state completion, enable bits, firmware loading, initialization and shutdown. |
| `input.c` | Encoder start, input configuration and signal detection for each input, mode changes. |
| `protocol_common.c` | Back-end blocks shared by several inputs. |
| `transcoder.c` | Encoder defaults and per-mode configuration. |
| `core.c` | V4L2 device, buffer queue, device lifecycle, bulk-IN reception, USB probe and disconnect. |
| `fops.c` | V4L2 file operations. |

Example of the access macros:

```c
GCHD_FE_W(d, GCHD_FE_REG_INPUT_SEL, GCHD_FE_SEL_HDMI);   /* 0x9C:51 = 45 */
GCHD_DEC_R(d, GCHD_DEC_REG_LOCK, v);                     /* read 0x88:CA */
GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, 720);
```
