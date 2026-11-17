# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** mode change rewritten from the new trace; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Composite 480i and 240p both start and stream (merged into `dev`). The
first attempt at switching while capturing (8a4b2af) detected the change but
the NULL stop never completed (timeout after ~63 s), and the recovery path
then reported a false signal loss right after START.

The new recording `traces/composite/gchd_240p-480i_composite` shows the
complete sequence for both directions (`docs/composite.md §4`). The driver
now follows it:

1. Detection: `0x88:CA` = `76`/`7E` → 480i, `66` → 240p; on `6E` (leaving
   480i) one back-end retrigger `15`, then wait for `66`.
2. While still in START: decoder apply block (`28 = 88`/`8A`), 0.4 s, read
   `E7`, neutral back-end table (`90 = 88`, `91`–`A3 = 77`, …), retrigger
   `05`.
3. NULL and STOP with the receive queue still running.
4. Back-end table for the mode, retriggers `05`+`15` (480i) or `15`+`15`
   (240p), encoder setup, INIT A0, START. Routing and decoder tuning are no
   longer repeated, and `0x88:0A` is not written.

The video timestamp used by the signal-loss check is refreshed when START
completes.

Audio is not delivered yet (TODO T-023): only the video PES is passed to
V4L2. It comes with TS passthrough (T-020).

## Acceptance criteria

1. 480i → 240p while ffplay runs: the picture returns as 240p within a few
   seconds.
2. 240p → 480i while ffplay runs: the picture returns as 480i.
3. dmesg shows "analog mode change detected" and "analog mode now …", no
   "analog mode change failed", no "signal lost" right after a START.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 480i -> 240p, wait ~10 s, then 240p -> 480i
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

If ffplay stops showing a picture after a switch, close it and start it again
(same command) and note whether the picture comes back. Paste the output and
observations into the maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing (see TODO "Hardware test log").
- `gchd_240p-480i_composite`: in that recording NULL takes ~0.7 s and is
  preceded by the neutral back-end table; 480i → 240p is not decoder-only.

---

## Maintainer comments

Some progress. This time at least when the mode changes the picture goes to black, but I didn't see the 480i mode being displayed. On retry it fails I guess because the device is not cleanly shutdown.

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
  Duration: N/A, start: 13147.289646, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
^CKilled M-V:  0.072 fd= 175 aq=    0KB vq=    0KB sq=    0B f=0/208   
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
    nan    :  0.000 fd=   0 aq=    0KB vq=    0KB sq=    0B f=0/0   


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[sudo] password for ff5:           
[13107.240859] usbcore: registered new interface driver elgato_gchd
[13114.766899] workqueue: hub_event hogged CPU for >10000us 16 times, consider switching to WQ_UNBOUND
[13114.901927] usb 2-1: new high-speed USB device number 17 using ehci-pci
[13115.305515] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[13115.305555] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[13115.305562] usb 2-1: Product: GameCapture HD
[13115.305566] usb 2-1: Manufacturer: Elgato
[13115.305569] usb 2-1: SerialNumber: 04107F1267
[13115.308431] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[13115.310310] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[13119.692789] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[13119.692797] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[13119.692803] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[13132.228974] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[13132.379506] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[13132.381164] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[13133.041992] elgato_gchd 2-1:1.0: transcoder defaults loaded
[13133.044505] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[13133.045823] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[13133.252924] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[13134.063902] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[13134.182354] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[13134.216768] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[13134.216788] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[13134.216797] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[13134.216804] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[13140.120985] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[13140.123351] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13142.398354] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[13142.453588] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13142.711213] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13142.713747] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13142.716174] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13142.717899] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13142.718441] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[13142.726711] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[13142.730896] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13142.731646] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13142.837182] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13147.743335] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13152.765744] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13155.557181] elgato_gchd 2-1:1.0: analog mode change detected: 240p -> 480i
[13156.955864] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13156.957256] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13157.685207] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13157.686675] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13158.882870] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13159.139816] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13159.142338] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13159.144694] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13159.145780] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13159.146157] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[13159.154377] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[13159.156662] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13159.158106] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13159.298331] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13159.298340] elgato_gchd 2-1:1.0: analog mode now 480i
[13160.501111] elgato_gchd: input 2 signal lost; scheduling redetection
[13160.505547] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13160.506960] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13164.171049] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13169.173979] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13174.274941] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13179.373724] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13184.388890] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13189.420132] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13194.523105] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13199.531888] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13200.062912] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[13200.118352] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13200.119994] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13200.573337] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13200.574719] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13200.746270] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[13200.868529] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[13200.898631] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[13200.898638] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[13200.898641] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[13200.898643] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[13202.932328] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[13202.986174] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13203.239530] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13203.241999] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13203.244385] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13203.245944] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13203.246496] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[13203.254818] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[13203.259302] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13203.261231] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13203.363244] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13204.546889] elgato_gchd: input 2 signal lost; scheduling redetection
[13204.551927] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13204.553260] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13208.299981] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13213.315058] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13218.315153] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13223.379078] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13228.416885] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13233.495507] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13238.579079] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13243.619244] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13245.733785] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[13245.739693] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13245.741100] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13247.250021] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13247.251241] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13247.458671] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13247.479139] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[13247.480667] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[13252.674260] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[13252.731234] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[13252.732572] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[13253.409214] elgato_gchd 2-1:1.0: transcoder defaults loaded
[13253.411879] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[13253.413209] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[13253.634222] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[13254.469274] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[13254.585996] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[13254.614132] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[13254.614138] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[13254.614140] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[13254.614143] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[13260.680266] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[13260.682617] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13262.951305] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[13263.003100] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13263.266366] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13263.269021] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13263.271462] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13263.274386] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13263.275691] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[13263.284986] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[13263.287227] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13263.289625] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13263.423508] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13264.556062] elgato_gchd: input 2 signal lost; scheduling redetection
[13264.560988] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13264.562186] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13268.323080] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13273.470172] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13278.571411] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13283.570919] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13288.570988] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13293.580183] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13298.586918] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13303.594871] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13304.252260] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[13304.305870] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13304.307862] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13305.765425] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13305.766678] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13305.933147] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[13306.048784] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[13306.077606] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[13306.077620] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[13306.077627] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[13306.077632] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[13307.518433] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[13307.570973] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13307.817059] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13307.819512] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13307.821892] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13307.823279] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13307.824031] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[13307.832376] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[13307.834656] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13307.835935] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13307.941399] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13309.083792] elgato_gchd: input 2 signal lost; scheduling redetection
[13309.095872] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13309.097554] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13312.827439] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13317.826971] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13322.924086] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13327.932902] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13333.028540] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13338.133645] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13343.186864] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13348.284067] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13349.132489] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[13349.188753] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13349.190066] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13350.710174] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13350.711990] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13350.872520] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[13350.997367] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[13351.026914] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[13351.026925] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[13351.026931] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[13351.026935] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[13352.495862] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[13352.546640] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[13352.804586] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[13352.806740] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[13352.808960] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[13352.810502] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[13352.810916] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[13352.818036] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[13352.820367] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[13352.821700] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13352.940619] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[13354.036292] elgato_gchd: input 2 signal lost; scheduling redetection
[13354.040526] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13354.042044] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13357.900561] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13362.947885] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13367.970889] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13372.978994] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13377.995073] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13383.013136] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13388.106864] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13393.106834] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=5 bytes=327680 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[13394.025162] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[13394.095161] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[13394.096563] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13395.587507] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[13395.588828] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[13395.746778] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[13395.867957] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[13395.898857] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[13395.898873] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[13395.898882] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[13395.898889] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
ff5@ff5-VirtualBox:~$ 
