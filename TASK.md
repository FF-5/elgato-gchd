# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** back-end settle wait before START; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Last test (0c64469): even the first START after plugging in did not
complete, and every later START streamed only a few transfers. That points
to the encoder not getting video from the analog back end.

The recordings show a wait the driver did not implement. Before every
capture start Windows does:

1. retrigger: read `0x98:F5`, `FD`, `FC`, `F3`, `F5`; `0D = C8`; read `F5`;
   `0E = pulse`, `pulse & ~1`, where the pulse is `05` or `15` depending on
   bit 4 of the `F5` read just before it (44 of 44 recorded pulses);
2. poll `0x98:F5` about every 4 ms: `62` until bit 7 is set (`F2`,
   40–700 ms);
3. retrigger again (now `15`);
4. poll `0x98:F5` until it reads `72`; only then INIT A0 and START.

The driver sent the second pulse right after the first, without the `F5`
reads and waits, so whether the back end delivered video depended on
timing. It now follows the recorded sequence (initial configuration and
mode change). If a wait times out, dmesg shows
`back-end status 0x.. after N polls, waiting for ...`.

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
maintainer comments below. Important lines: `back-end status …` (only
printed if a wait timed out), `bulk-IN first reads`, and the
`async bulk-IN stats` lines after each START.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

The first time it took a while to show something and it did show a 480i mode for a little bit then it stopped (I guess it changed mode at that point) and did nothing. On second try without unplugging the device ffplay says nan and nothing shows up. I guess as of now the driver is not relaying a clean shutdown signal to the device.

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
Input #0, video4linux2,v4l2, from '/dev/video0':B sq=    0B f=0/0   
  Duration: N/A, start: 18030.899495, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
^CKilled M-V:  0.018 fd=  28 aq=    0KB vq=    0KB sq=    0B f=0/23   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[17984.965960] usbcore: registered new interface driver elgato_gchd
[17992.096807] usb 2-1: new high-speed USB device number 19 using ehci-pci
[17992.498358] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[17992.498367] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[17992.498369] usb 2-1: Product: GameCapture HD
[17992.498371] usb 2-1: Manufacturer: Elgato
[17992.498373] usb 2-1: SerialNumber: 04107F1267
[17992.500795] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[17992.503181] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[17997.215471] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[17997.215481] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[17997.215486] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[18013.892300] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[18014.053101] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[18014.054452] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[18014.712905] elgato_gchd 2-1:1.0: transcoder defaults loaded
[18014.715671] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[18014.717022] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[18014.923420] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[18015.738413] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[18015.861784] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[18015.891761] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[18015.891769] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[18015.891774] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[18015.891778] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[18022.005728] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[18022.008015] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18025.592067] elgato_gchd 2-1:1.0: back-end status 0x72 after 45 polls, waiting for flag 0x80
[18025.982055] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[18026.036409] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[18026.304462] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[18026.306963] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18026.309800] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[18026.312264] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18026.312881] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[18026.321799] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18026.324546] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[18026.325978] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18026.446030] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[18026.732060] elgato_gchd 2-1:1.0: analog mode change detected: 240p -> 480i
[18026.801752] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[18026.848504] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[18026.893716] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[18026.937517] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[18026.982587] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[18028.252814] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18028.254267] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18028.928926] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[18028.930278] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18031.552084] elgato_gchd 2-1:1.0: back-end status 0x72 after 45 polls, waiting for flag 0x80
[18031.889449] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[18032.158691] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[18032.161308] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18032.163967] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[18032.166909] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18032.167698] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[18032.176486] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18032.178859] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[18032.180590] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18032.316265] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[18032.316282] elgato_gchd 2-1:1.0: analog mode now 480i
[18032.721537] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[18032.766258] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[18032.810996] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[18032.857732] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[18032.901454] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[18033.466153] elgato_gchd: input 2 signal lost; scheduling redetection
[18033.701418] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18033.702847] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18037.217638] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18042.241898] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18047.249689] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18052.275548] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18057.292187] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18062.297702] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18067.345538] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18072.353551] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18073.638617] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[18073.694005] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18073.695350] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18075.218948] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[18075.220478] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18075.384904] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[18075.499666] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[18075.529360] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[18075.529367] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[18075.529370] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[18075.529373] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[18078.346264] elgato_gchd 2-1:1.0: back-end status 0x72 after 47 polls, waiting for flag 0x80
[18078.722517] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[18078.771258] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[18079.024607] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[18079.026824] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18079.029471] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[18079.031799] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18079.032334] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[18079.041011] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18079.043369] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[18079.044808] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18079.166726] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[18079.550110] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[18079.594407] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[18079.640656] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[18079.684459] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[18079.730664] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[18080.340405] elgato_gchd: input 2 signal lost; scheduling redetection
[18080.344240] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18080.345531] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18084.083231] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18089.098860] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18094.193831] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18099.209993] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18104.217652] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18109.217839] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18114.227568] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18118.975277] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[18119.036245] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18119.037585] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18120.765280] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[18120.766718] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18120.950923] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[18121.070054] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[18121.101260] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[18121.101278] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[18121.101287] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[18121.101294] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[18124.539261] elgato_gchd 2-1:1.0: back-end status 0x72 after 43 polls, waiting for flag 0x80
[18124.943785] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[18124.999186] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[18125.265678] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[18125.268265] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18125.270787] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[18125.274403] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[18125.275579] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[18125.285570] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[18125.288207] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[18125.289985] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18125.408764] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[18125.794090] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[18125.838140] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[18125.884114] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[18126.478889] elgato_gchd: input 2 signal lost; scheduling redetection
[18126.483222] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18126.484898] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18130.297925] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18135.334482] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18140.403831] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18145.417818] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18150.433961] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18155.434663] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18160.465755] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18165.469101] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[18169.616112] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[18169.622724] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[18169.624103] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18171.189243] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[18171.190705] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[18171.426779] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[18171.445894] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[18171.447568] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
ff5@ff5-VirtualBox:~$ 

