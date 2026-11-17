# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** recorded encoder block before START; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Last test (2025d5f): every START still streamed for only ~0.5 s, then the
device stopped sending and NULL could not complete. Two findings:

1. **Encoder programming before START did not match the recordings.**
   Every recording (composite, S-video, HDMI, every restart) writes the same
   120 encoder fields in the same order before INIT A0 / START. The driver
   wrote a different set: `dma_sel_out = 1` (transport output DMA
   selection), `sout_mode`, `v_ip_format_bl` (twice), `ach_sel_bl`,
   `v_idr_interval_h_bl` and two unnamed fields were missing; the four
   stream-id fields (written by Windows only at initialization) were extra;
   the order differed and `num_units_in_tick_bl` was 1 instead of 0. The
   driver now writes exactly the recorded block (checked mechanically: 120
   of 120 fields in order).
2. **Back-end settle wait.** After the first pulse the status already read
   `72` (settled), and the driver waited 1.5 s for a flag. `72` now ends
   the wait; a second pulse is sent only when the flag appears, as in the
   recordings.

Open: at the start the decoder read `CA = 66` and the driver configured
240p, then saw 480i 0.3 s after START. Check in this test whether the
source is 480i or 240p when you start.

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

Note which mode the source outputs when you start, and for each step whether
a picture appeared. Paste the output into the
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
- 2025d5f test: encoder programming before START differed from every
  recording (missing `dma_sel_out` etc.); fixed.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

Now it's not showing anything, ffplay keeps saying nan. What happened? It was kind of working before with the exception of the mode change.

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
Killedn    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[sudo] password for ff5:           
[19295.800283] usbcore: registered new interface driver elgato_gchd
[19303.865945] usb 2-1: new high-speed USB device number 20 using ehci-pci
[19304.270514] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[19304.270523] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[19304.270525] usb 2-1: Product: GameCapture HD
[19304.270526] usb 2-1: Manufacturer: Elgato
[19304.270527] usb 2-1: SerialNumber: 04107F1267
[19304.272866] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[19304.275638] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[19308.850788] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[19308.850798] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[19308.850805] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[19320.542312] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[19320.685635] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[19320.687150] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[19321.397674] elgato_gchd 2-1:1.0: transcoder defaults loaded
[19321.400122] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[19321.401626] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[19321.609314] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[19322.451867] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[19322.574628] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[19322.605370] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[19322.605382] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[19322.605388] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[19322.605393] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[19328.577645] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[19328.580391] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[19330.270154] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[19330.576497] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[19330.579448] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[19330.581598] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[19330.585541] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[19330.586666] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[19330.595223] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[19330.597530] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[19330.598963] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[19335.681732] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[19340.681776] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=0 bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=1 ring_dropped=0 frames_waiting=0
[19345.370169] elgato_gchd 2-1:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[19345.379921] elgato_gchd 2-1:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[19345.379933] elgato_gchd 2-1:1.0: capture start transition failed: -110
[19345.521545] elgato_gchd 2-1:1.0: capture start failed: -110; stopped, retrying detection
[19346.569230] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[19346.689750] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[19346.717777] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[19346.717790] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[19346.717797] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[19346.717801] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[19348.659570] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[19348.955657] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[19348.958012] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[19348.960451] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0002
[19348.961505] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[19348.961766] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[19348.968640] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[19348.968653] elgato_gchd 2-1:1.0: START before SCMD: state=0x0002 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[19348.969688] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[19348.970548] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[19348.971566] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[19348.978447] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[19348.983387] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[19348.985427] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[19348.988260] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[19349.531204] elgato_gchd: input 2 signal lost; scheduling redetection
[19349.535041] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[19349.536469] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[19354.001455] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19359.005824] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19364.017309] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19369.017320] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19374.018274] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19379.018250] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19384.121194] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[19387.767675] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[19387.775107] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[19387.776525] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[19388.215290] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[19388.216784] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[19388.436421] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[19388.457811] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[19388.459413] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
ff5@ff5-VirtualBox:~$ 

The problem to what you noticed from my comment is that everytime ffplay starts, when it shows an output it usually takes a while for the output window to open. I guess the buffer hasn't settled or something.
