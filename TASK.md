# TASK T-064 — Remembered settings

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (7051640); hardware test with the test script
- **Type:** driver feature + udev rule of our own
- **Branch:** `feat/input-persistence` (from `dev` 0.5.4)
- **TODO items:** T-064 (stage 1)

## Why

Maintainer requirement: HDMI and the defaults on the first use after
installation; afterwards the input and all settings last used, also after
unplugging, module reload and reboot. Until now the driver remembered the
input only while the module stayed loaded, and the other settings only
while the device stayed connected.

## Implementation

1. Module parameters `input`, `output`, `format`, `bitrate`,
   `bitrate_mode`, `stretch`, `h264_profile`, `h264_level` hold the
   settings in use; at probe the device takes them, and the controls get
   them as defaults.
2. Every change (input, `S_FMT`, encoder controls) updates the parameters
   and sends a `change` uevent with `GCHD_*` values on the USB interface.
3. `udev-rules/56-elgato-gchd-settings.rules` (new file of ours) writes
   them as `options elgato_gchd ...` to
   `/etc/modprobe.d/elgato-gchd-settings.conf`; modprobe applies them at
   the next load. Docs: `dkms/README.md`.
4. User profiles (maintainer idea): TODO T-091, after the release.

## Hardware test (maintainer)

The test installs the udev rule (and removes the settings file at the end).
No console needed except for one capture step (PS1 on composite).

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-064.sh > tests/T-064.sh
bash gchd-test.sh T-064 --branch feat/input-persistence
```

Paste the summary; the log too if anything fails.

## Findings
- Maintainer (2026-11-18): remember all settings, not only the input;
  later user profiles that recall all settings, several per input
  (TODO T-091).

## Maintainer comments

gchd-test T-064 20261118-120532 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-064-20261118-120532.log
== build
$ git fetch -q origin
$ git checkout -q feat/input-persistence
$ git pull -q --ff-only
commit: d5678cc TASK: T-064 remembered settings, test via gchd-test.sh
commit: d5678cc TASK: T-064 remembered settings, test via gchd-test.sh
build rc=0 warnings=0

=== step 1: install the udev rule, forget saved settings (first use)
$ sudo cp /home/ff5/elgato-gchd/udev-rules/56-elgato-gchd-settings.rules /etc/udev/rules.d/
$ sudo udevadm control --reload
$ sudo rm -f /etc/modprobe.d/elgato-gchd-settings.conf
--- dmesg step 1


=== step 2: first use: HDMI and defaults
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/dkms/elgato_gchd.ko
module: 0.5.4 
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 0 (HDMI: no signal)
[PASS] input HDMI on first use
$ v4l2-ctl -d /dev/video0 --get-fmt-video
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'H264' (H.264)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 8388608
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] format H.264 on first use
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=0 (HDMI)
               stretch_sd_input 0x009819f1 (bool)   : default=0 value=0

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=1 value=1 (Constant Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=11250000 value=11250000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=4 value=4 (High)
[PASS] bitrate default 11.25 Mbit/s
--- dmesg step 2
[64497.226304] usbcore: deregistering interface driver elgato_gchd
[64502.898363] usb 1-2: USB disconnect, device number 71
[64510.178789] usb 1-2: new high-speed USB device number 72 using xhci_hcd
[64510.563055] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[64510.563077] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[64510.563083] usb 1-2: Product: GameCapture HD
[64510.563088] usb 1-2: Manufacturer: Elgato
[64510.563092] usb 1-2: SerialNumber: 04107F1267
[64510.818478] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[64510.821192] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[64510.821232] usbcore: registered new interface driver elgato_gchd

=== step 3: change settings
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=640,height=480,pixelformat=MPEG
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=0,video_bitrate=6000000,stretch_sd_input=1
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1 h264_profile=4 h264_level=0
[PASS] settings file written
--- dmesg step 3
[64510.866045] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[64510.866070] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[64510.866072] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0

=== step 4: module reload with the saved settings
$ sudo insmod /home/ff5/elgato-gchd/dkms/elgato_gchd.ko input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1 h264_profile=4 h264_level=0
module: 0.5.4 input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1 h264_profile=4 h264_level=0
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 0 (HDMI: no signal)
[FAIL] input composite after reload (expected /Video input : 2/, got: Video input : 0 (HDMI: no signal))
$ v4l2-ctl -d /dev/video0 --get-fmt-video
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[FAIL] 640x480 MPEG after reload (expected /640/480/, got: Format Video Capture: 	Width/Height      : 1280/720 	Pixel Format      : 'MPEG' (MPEG-1/2/4) 	Field             : None 	Bytes per Line    : 0 	Size Image       )
$ v4l2-ctl -d /dev/video0 --get-fmt-video
Format Video Capture:
	Width/Height      : 1280/720
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : None
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
[PASS] MPEG after reload
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=0 (HDMI)
               stretch_sd_input 0x009819f1 (bool)   : default=1 value=1

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=0 value=0 (Variable Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=6000000 value=6000000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=4 value=4 (High)
[PASS] bitrate 6000 VBR, stretch after reload
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=0 (HDMI)
               stretch_sd_input 0x009819f1 (bool)   : default=1 value=1

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=0 value=0 (Variable Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=6000000 value=6000000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=4 value=4 (High)
[PASS] VBR after reload
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=0 (HDMI)
               stretch_sd_input 0x009819f1 (bool)   : default=1 value=1

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=0 value=0 (Variable Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=6000000 value=6000000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=4 value=4 (High)
[PASS] stretch after reload
--- dmesg step 4
[64511.954402] usbcore: deregistering interface driver elgato_gchd
[64511.991143] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[64511.997124] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[64511.997144] usbcore: registered new interface driver elgato_gchd

=== step 5: replug with the saved settings, capture
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/dkms/elgato_gchd.ko input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1 h264_profile=4 h264_level=0
module: 0.5.4 input=2 output=1 format=1 bitrate=6000 bitrate_mode=0 stretch=1 h264_profile=4 h264_level=0
PROMPT: PS1 on composite, game running.
capturing 10s to step5-3844.ts
capture: 1830912 bytes, ~1464 kbit/s; codec_name=h264 width=640 height=480 field_order=progressive r_frame_rate=25/1 ; decode errors 1
  codec_name=h264 width=640 height=480 field_order=progressive r_frame_rate=25/1 ; ~1464 kbit/s; decode errors 1
audio: mp2
[PASS] stream 640x480 25/1 progressive, ~1464 kbit/s
--- dmesg step 5
[64512.063862] usbcore: deregistering interface driver elgato_gchd
[64519.614166] usb 1-2: USB disconnect, device number 72
[64526.871540] usb 1-2: new high-speed USB device number 73 using xhci_hcd
[64527.277670] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[64527.277695] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[64527.277703] usb 1-2: Product: GameCapture HD
[64527.277708] usb 1-2: Manufacturer: Elgato
[64527.277713] usb 1-2: SerialNumber: 04107F1267
[64530.003584] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[64530.007518] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[64530.007540] usbcore: registered new interface driver elgato_gchd
[64545.147345] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[64545.989967] elgato_gchd 1-2:1.0: transcoder defaults loaded
[64546.060967] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[64546.356526] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[64546.515314] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[64546.517886] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[64546.517888] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[64546.517890] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[64546.517892] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[64547.316139] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[64547.616074] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[64547.638429] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[64547.638651] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[64547.644507] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[64547.645305] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[64547.645954] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[64547.784181] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[64548.051358] elgato_gchd 1-2:1.0: encoder restart: output 0 -> 1
[64550.420045] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[64550.420447] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[64550.426092] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[64550.426638] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[64550.427759] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[64550.545804] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[64550.545806] elgato_gchd 1-2:1.0: analog mode now 576i, output 1
[64551.073240] elgato_gchd 1-2:1.0: bulk-IN: first data on ep=0x84 (65536 bytes)
[64555.462318] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=31 bytes=2031616 ep84_bytes=2031616 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=3
[64556.895804] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 6: back to defaults; forget the settings file
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=H264
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -c video_bitrate_mode=1,video_bitrate=11250000,stretch_sd_input=0
$ cat /etc/modprobe.d/elgato-gchd-settings.conf
options elgato_gchd input=0 output=0 format=0 bitrate=11250 bitrate_mode=1 stretch=0 h264_profile=4 h264_level=0
$ sudo rm -f /etc/modprobe.d/elgato-gchd-settings.conf
--- dmesg step 6
[64557.216299] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[64557.216317] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open

=== summary (T-064, d5678cc TASK: T-064 remembered settings, test via gchd-test.sh)
PASS | step 2 first use: HDMI and defaults | input HDMI on first use
PASS | step 2 first use: HDMI and defaults | format H.264 on first use
PASS | step 2 first use: HDMI and defaults | bitrate default 11.25 Mbit/s
PASS | step 3 change settings | settings file written
FAIL | step 4 module reload with the saved settings | input composite after reload (expected /Video input : 2/, got: Video input : 0 (HDMI: no signal))
FAIL | step 4 module reload with the saved settings | 640x480 MPEG after reload (expected /640/480/, got: Format Video Capture: 	Width/Height      : 1280/720 	Pixel Format      : 'MPEG' (MPEG-1/2/4) 	Field             : None 	Bytes per Line    : 0 	Size Image       )
PASS | step 4 module reload with the saved settings | MPEG after reload
PASS | step 4 module reload with the saved settings | bitrate 6000 VBR, stretch after reload
PASS | step 4 module reload with the saved settings | VBR after reload
PASS | step 4 module reload with the saved settings | stretch after reload
PASS | step 5 replug with the saved settings, capture | stream 640x480 25/1 progressive, ~1464 kbit/s
