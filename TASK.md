# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** mode switches during setup and before stop handled; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

Control test: with a stable 240p source both `dev` and this branch start and
stream without problems. The source (N64 *Star Wars Episode I: Racer*)
switches between 480i and 240p by itself (menu ↔ attract demo), and every
failure so far happened around such a switch:

1. **Switch during setup** (0c64469, 5d31e07 tests): setup takes ~9 s after
   opening (6 s of it firmware loading). The driver configured the mode seen
   at detection; the source changed meanwhile and START never completed.
   Now: right before START the decoder status is read again; if the mode
   changed, the driver logs `source changed to … during setup; configuring
   again` and configures the new mode instead of starting.
2. **Switch noticed by the signal-loss check first:** the stop then went
   straight to NULL, which does not complete while the back end is set up
   for the old mode (40 s timeouts in several tests). Windows always applies
   the decoder setting and the neutral back-end table before NULL on a mode
   change. The redetection path now does the same when the decoder reports
   the other mode (`redetection: source is now …; preparing before stop`).
3. **Still open:** after a mode change while capturing (9382bcb and 2025d5f
   tests) the restarted stream ran for ~0.6 s and stopped. This test shows
   whether that still happens with the recorded encoder block and settle
   wait.

Audio is not delivered yet (TODO T-023).

## Acceptance criteria (this test)

1. Fresh start in 480i (menu) gives a picture.
2. The menu ↔ demo switches while capturing: picture returns after each
   switch within a few seconds.

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# start while the game shows the menu; then let it run through at least
# two menu <-> demo switches
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
```

Note when the game switched (menu → demo, demo → menu) and whether the
picture came back each time. Paste the output into the maintainer comments
below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- Control test: stable 240p works on `dev` and on this branch; the source
  switches 480i ↔ 240p by itself.
- 0c64469 / 5d31e07 / 2025d5f tests: decoder status flips between 480i and
  240p around the first START; START only completes for the matching mode.
- 2025d5f test: encoder programming before START differed from every
  recording (missing `dma_sel_out` etc.); fixed.
- 0c64469 test: START hangs or the stream stops when START is issued before
  the back end has settled (`0x98:F5` wait missing).
- `gchd_output_switch_hdmi`: output size changes with a fixed 1080p60 source
  (`docs/hdmi.md §4`, TODO T-055).

---

## Maintainer comments

First a comment:
- How much of the logs are you going to write to TODO.md? It's getting quite long. Do you need the information forever? TODO.md is supposed to be a roadmap, a list of things that we need to do.

Now to the output. ffplay didn't open any window, so there was nothing.

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
[1]+  Stopped                 ffplay -f v4l2 -input_format h264 -i /dev/video0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
[20912.011647] usbcore: registered new interface driver elgato_gchd
[20915.833637] usb 2-1: new high-speed USB device number 23 using ehci-pci
[20916.248584] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[20916.248593] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[20916.248595] usb 2-1: Product: GameCapture HD
[20916.248596] usb 2-1: Manufacturer: Elgato
[20916.248598] usb 2-1: SerialNumber: 04107F1267
[20916.251563] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[20916.253297] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[20921.754099] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[20921.754102] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[20921.754105] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[20942.112056] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[20943.241888] elgato_gchd 2-1:1.0: transcoder defaults loaded
[20943.459152] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[20944.324870] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[20944.448228] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[20944.477740] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[20944.477747] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[20944.477750] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[20944.477753] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[20950.578451] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[20952.663128] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[20952.966741] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[20952.971750] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[20952.974289] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20953.006702] elgato_gchd 2-1:1.0: source changed to 480i during setup; configuring again
[20953.160298] elgato_gchd 2-1:1.0: capture start failed: -11; stopped, retrying detection
[20954.248417] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[20954.365675] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[20954.395930] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[20954.395934] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[20954.395941] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[20954.395942] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[20955.877377] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[20956.183844] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[20956.188986] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[20956.190236] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[20956.221170] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[20956.231736] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[20956.339344] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[20956.797150] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[20956.842267] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[20956.887610] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[20957.413525] elgato_gchd: input 2 signal lost; scheduling redetection
[20961.280832] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20966.308276] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20971.324972] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20976.393354] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20981.418190] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20986.425832] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20991.427985] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20996.489996] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[20997.736675] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[20997.821970] elgato_gchd 2-1:1.0: redetection: source is now 240p; preparing before stop
[21001.160077] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[21001.279135] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21001.308297] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[21001.308306] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[21001.308326] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[21001.308332] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[21002.748704] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21003.034737] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21003.039308] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21003.041663] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21003.069721] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[21003.079076] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21003.208639] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21003.593721] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21003.638594] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21003.683275] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[21003.728480] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[21003.774052] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[21004.394552] elgato_gchd: input 2 signal lost; scheduling redetection
[21008.057801] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21013.137005] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21018.160901] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21023.193016] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21028.289989] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21033.299251] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21038.304859] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21043.352845] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=7 bytes=458752 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21045.648645] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[21045.736520] elgato_gchd 2-1:1.0: redetection: source is now 240p; preparing before stop
[21047.999176] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[21048.132589] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21048.161427] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[21048.161441] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[21048.161445] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[21048.161448] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[21050.238787] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21050.526767] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21050.531983] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21050.536173] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21050.568586] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[21050.578575] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21050.702381] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21051.087678] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21051.132147] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21051.751851] elgato_gchd: input 2 signal lost; scheduling redetection
[21055.635167] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21060.673146] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21065.755195] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21070.825533] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21075.906234] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21080.957424] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21086.033404] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21091.123652] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=2 bytes=131072 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21096.211745] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[21098.132948] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[21098.250988] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21098.280119] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[21098.280126] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[21098.280128] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[21098.280130] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[21100.368401] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21100.673068] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21100.678018] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21100.679552] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21100.709865] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[21100.719759] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21100.846440] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21101.234190] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21101.279149] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21101.323880] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[21101.368146] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[21101.413735] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[21102.020035] elgato_gchd: input 2 signal lost; scheduling redetection
[21105.699705] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21110.770468] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21115.827047] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21120.864720] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21125.896813] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21130.914537] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21135.949347] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21140.984964] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=6 bytes=393216 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21145.894880] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
[21145.995316] elgato_gchd 2-1:1.0: redetection: source is now 480i; preparing before stop
[21148.309299] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[21148.441063] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21148.474311] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[21148.474331] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[21148.474341] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[21148.474348] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[21150.012728] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21150.340429] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21150.345871] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21150.349451] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21150.381853] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[21150.392820] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21150.533062] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21150.916184] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21151.432832] elgato_gchd: input 2 signal lost; scheduling redetection
[21155.392801] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1 bytes=65536 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21160.392775] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1 bytes=65536 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21165.408739] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1 bytes=65536 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
ff5@ff5-VirtualBox:~$ 
