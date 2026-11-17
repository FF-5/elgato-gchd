# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** diagnostics; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

The mode change now runs as recorded (9382bcb, TODO "Hardware test log"):
NULL completed in 0.73 s, STOP, back end, INIT A0 and START all succeeded.
But after that START, and after **every** later START, including a fresh
firmware load after closing ffplay, only 5–7 bulk transfers arrived and then
the stream stopped; the next NULL never completed. Only the first START after
plugging in the device streamed.

Before 8a4b2af the driver read the stream after STOP until it ran dry
(1 s timeouts), and a second capture session worked. Since 8a4b2af nothing
reads the stream after STOP. The recordings show `ABORT_PIPE` on `0x81`/`0x84`
right after STOP, but they contain no bulk data, so they cannot show whether
Windows reads remaining data first (TODO "Evidence wanted").

The recording with bulk data (`gchd_240p-480i_composite_input_part_*`)
shows that Windows handles the stream exactly like the driver already does:
the receive transfers stay queued through NULL and STOP, data keeps coming
for ~0.4 s after NULL, the pending transfers are cancelled right after STOP,
and **nothing is read after STOP**. New transfers are queued just before the
encoder writes, and data resumes ~0.5 s after START. So the stall is not
caused by data left in the device, and the post-STOP drain from 9eee2e0 was
removed again (not in the recording).

This round therefore only adds diagnostics:

- the stats line shows failed receive transfers (`urb_errors`,
  `last_status`);
- the first five reads after each START are logged as `bulk-IN first reads`
  (dmesg timestamps show when data arrives relative to START).

Audio is not delivered yet (TODO T-023).

## Acceptance criteria

1. 480i → 240p and 240p → 480i while ffplay runs: the picture returns.
2. Closing ffplay and starting it again gives a picture.
3. dmesg shows "analog mode now …", no "analog mode change failed", no
   "signal lost" right after a START.

## Hardware test (maintainer)

Unplug and replug the device first (the device may still be in the stuck
state from the last test).

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
# close ffplay, wait 5 s, start the same ffplay command again
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

Note for each step whether a picture appeared. Paste the output into the
maintainer comments below. The `bulk-IN first reads` and `async bulk-IN
stats` lines after each START are the important ones.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

ffplay didn't even start. So no picture. Upon killing the terminal command and executing it again without unplugging the device I still get no output.

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f v4l2 -input_format h264 -i /dev/video0
ffplay version 6.1.1-3ubuntu5 Copyright (c) 2003-2023 the FFmpeg developers
  built with gcc 13 (Ubuntu 13.2.0-23ubuntu3)
  configuration: --prefix=/usr --extra-version=3ubuntu5 --toolchain=hardened --libdir=/usr/lib/x86_64-linux-gnu --incdir=/usr/include/x86_64-linux-gnu --arch=amd64 --enable-gpl --disable-stripping --disable-omx --enable-gnutls --enable-libaom --enable-libass --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libglslang --enable-libgme --enable-libgsm --enable-libharfbuzz --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-openal --enable-opencl --enable-opengl --disable-sndio --enable-libvpl --disable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-ladspa --enable-libbluray --enable-libjack --enable-libpulse --enable-librabbitmq --enable-librist --enable-libsrt --enable-libssh --enable-libsvtav1 --enable-libx264 --enable-libzmq --enable-libzvbi --enable-lv2 --enable-sdl2 --enable-libplacebo --enable-librav1e --enable-pocketsphinx --enable-librsvg --enable-libjxl --enable-shared
  WARNING: library configuration mismatch
  avcodec     configuration: --prefix=/usr --extra-version=3ubuntu5 --toolchain=hardened --libdir=/usr/lib/x86_64-linux-gnu --incdir=/usr/include/x86_64-linux-gnu --arch=amd64 --enable-gpl --disable-stripping --disable-omx --enable-gnutls --enable-libaom --enable-libass --enable-libbs2b --enable-libcaca --enable-libcdio --enable-libcodec2 --enable-libdav1d --enable-libflite --enable-libfontconfig --enable-libfreetype --enable-libfribidi --enable-libglslang --enable-libgme --enable-libgsm --enable-libharfbuzz --enable-libmp3lame --enable-libmysofa --enable-libopenjpeg --enable-libopenmpt --enable-libopus --enable-librubberband --enable-libshine --enable-libsnappy --enable-libsoxr --enable-libspeex --enable-libtheora --enable-libtwolame --enable-libvidstab --enable-libvorbis --enable-libvpx --enable-libwebp --enable-libx265 --enable-libxml2 --enable-libxvid --enable-libzimg --enable-openal --enable-opencl --enable-opengl --disable-sndio --enable-libvpl --disable-libmfx --enable-libdc1394 --enable-libdrm --enable-libiec61883 --enable-chromaprint --enable-frei0r --enable-ladspa --enable-libbluray --enable-libjack --enable-libpulse --enable-librabbitmq --enable-librist --enable-libsrt --enable-libssh --enable-libsvtav1 --enable-libx264 --enable-libzmq --enable-libzvbi --enable-lv2 --enable-sdl2 --enable-libplacebo --enable-librav1e --enable-pocketsphinx --enable-librsvg --enable-libjxl --enable-shared --enable-version3 --disable-doc --disable-programs --disable-static --enable-libaribb24 --enable-libopencore_amrnb --enable-libopencore_amrwb --enable-libtesseract --enable-libvo_amrwbenc --enable-libsmbclient
  libavutil      58. 29.100 / 58. 29.100
  libavcodec     60. 31.102 / 60. 31.102
  libavformat    60. 16.100 / 60. 16.100
  libavdevice    60.  3.100 / 60.  3.100
  libavfilter     9. 12.100 /  9. 12.100
  libswscale      7.  5.100 /  7.  5.100
  libswresample   4. 12.100 /  4. 12.100
  libpostproc    57.  3.100 / 57.  3.100
^C  nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[sudo] password for ff5:           
[17215.271622] usbcore: registered new interface driver elgato_gchd
[17223.100486] usb 2-1: new high-speed USB device number 18 using ehci-pci
[17223.520392] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[17223.520401] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[17223.520404] usb 2-1: Product: GameCapture HD
[17223.520406] usb 2-1: Manufacturer: Elgato
[17223.520407] usb 2-1: SerialNumber: 04107F1267
[17223.522562] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[17223.525878] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[17232.230650] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[17232.230654] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[17232.230656] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[17246.636047] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[17246.805530] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[17246.806839] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[17247.456783] elgato_gchd 2-1:1.0: transcoder defaults loaded
[17247.459305] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[17247.460720] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[17247.663924] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[17248.471248] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[17248.587592] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[17248.617324] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[17248.617331] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[17248.617334] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[17248.617337] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[17254.378466] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[17254.381047] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[17256.045771] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[17256.095736] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[17256.339562] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[17256.341771] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[17256.343869] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[17256.345513] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17256.346005] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[17256.353001] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[17256.355125] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[17256.356247] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[17261.353773] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[17266.363211] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[17270.427375] elgato_gchd 2-1:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[17270.437369] elgato_gchd 2-1:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[17270.437379] elgato_gchd 2-1:1.0: capture start transition failed: -110
[17270.589587] elgato_gchd 2-1:1.0: capture start failed: -110; stopped, retrying detection
[17271.622617] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[17271.743597] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[17271.773002] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[17271.773014] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[17271.773017] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[17271.773020] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[17273.788299] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[17273.837988] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[17274.086299] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[17274.088463] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[17274.090599] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0002
[17274.092077] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[17274.092347] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[17274.099377] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[17274.099396] elgato_gchd 2-1:1.0: START before SCMD: state=0x0002 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[17274.101481] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[17274.102267] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[17274.103326] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[17274.110264] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[17274.116383] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[17274.116391] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[17274.634742] elgato_gchd: input 2 signal lost; scheduling redetection
[17274.638131] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[17274.639447] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[17279.114076] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17284.187681] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17289.229506] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17294.329835] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17299.338877] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17304.347052] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17309.370262] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=4 bytes=262144 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[17314.277092] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[17314.282193] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[17314.283631] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
ff5@ff5-VirtualBox:~$ 



