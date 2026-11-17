# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** fix ready; waiting for the hardware test
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043

## Why

Composite 480i and 240p both start and stream (merged into `dev`). Switching
240p → 480i while capturing failed (TODO "Hardware test log"): the stop took
~70 s, then the driver put the device into IDLE (0x11), which INIT 0xa0 does
not leave, so START timed out. The driver now restarts from the stopped state
(0x01) as in the traces, and the stop no longer waits on 1 s timeouts.

480i → 240p still uses the same restart path; the traces show a
decoder-only change for that direction (next step once the restart works).

## Acceptance criteria

1. 240p → 480i while ffplay runs: the picture returns as 480i within a few
   seconds.
2. 480i → 240p while ffplay runs: the picture returns as 240p.
3. dmesg shows "analog mode change detected" and "analog mode now …", and no
   "START failed".

## Hardware test (maintainer)

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C; sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# switch the source 240p -> 480i, wait, then 480i -> 240p
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
```

ffplay may need to be restarted after a switch if it does not recover by
itself; note whether it did. Paste the output and observations into the
maintainer comments below.

## Findings

_(filled in during the task)_

---

## Maintainer comments

_Written by the maintainer only. Anything lasting is transferred to
`RULES.md` or `TODO.md` when the next task is prepared, and this section is
then emptied._

Output was similar to before, it managed to properly display the first source resolution it encountered (480i this time) but failed to switch to 240p or display again the 480i after that.

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
  Duration: N/A, start: 10362.990339, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
Killed21 M-V: -0.000 fd= 307 aq=    0KB vq=    0KB sq=    0B f=0/292   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested"
[sudo] password for ff5:           
[10323.408192] usbcore: registered new interface driver elgato_gchd
[10329.336902] usb 2-1: new high-speed USB device number 16 using ehci-pci
[10329.733150] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[10329.733158] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[10329.733160] usb 2-1: Product: GameCapture HD
[10329.733162] usb 2-1: Manufacturer: Elgato
[10329.733163] usb 2-1: SerialNumber: 04107F1267
[10329.735475] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[10329.737987] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[10335.390276] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[10335.390282] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[10335.390288] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[10348.344788] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[10348.492730] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
[10348.494146] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x01
[10349.203364] elgato_gchd 2-1:1.0: transcoder defaults loaded
[10349.206385] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
[10349.208013] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x04
[10349.409335] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[10350.245837] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[10350.368302] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[10350.398377] elgato_gchd 2-1:1.0: analog format initial status=0x7e; configuring 480i
[10350.398386] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[10350.398391] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[10350.398395] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[10356.370304] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[10356.372868] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[10358.072729] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[10358.126379] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[10358.398952] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[10358.401270] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[10358.403617] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[10358.404632] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[10358.404946] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[10358.412827] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[10358.415037] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[10358.416318] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[10358.536738] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[10363.445129] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[10368.469154] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[10373.489517] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[10378.513392] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[10378.662051] elgato_gchd 2-1:1.0: analog mode change detected: 480i -> 240p
[10378.809762] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[10378.811141] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[10441.907621] elgato_gchd 2-1:1.0: 240p-to-480i reconfiguration failed: -110
[10442.183440] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[10442.184730] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[10443.715796] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
[10443.717299] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[10443.884300] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[10444.008191] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[10444.040014] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[10444.040030] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[10444.040038] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[10444.040043] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[10446.081900] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[10446.132108] elgato_gchd 2-1:1.0: capture setup: transcoder final configuration completed
[10446.389236] elgato_gchd 2-1:1.0: capture setup: transcoder setup completed
[10446.391535] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
[10446.394169] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[10446.395187] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[10446.395433] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[10446.403727] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[10446.406197] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
[10446.407639] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
[10446.550222] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[10446.916270] elgato_gchd: input 2 signal lost; scheduling redetection
[10447.079170] elgato_gchd 2-1:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
[10447.080592] elgato_gchd 2-1:1.0: SCMD interrupt completed: command=0x05
ff5@ff5-VirtualBox:~$ 

Another issue I seemed to encounter now, because I'm only monitoring it now, is that I don't hear any audio.
