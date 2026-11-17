# TASK T-041a — Composite 480i ↔ 240p while capturing

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** test whether any second START fails (stable source)
- **Type:** driver fix + hardware test
- **Branch:** `fix/mode-change-restart` (from `dev`)
- **TODO items:** Phase 5 T-041, T-043; Phase 3 T-023 (audio, not in this task)

## Why

In every test so far only the **first START after plugging in** streamed.
Every later START (after a mode change, after redetection, after closing and
reopening ffplay, even after reloading the firmware) delivered 1–7 transfers
and stopped, and NULL never completed afterwards. In the last test the
"source changed during setup" check worked, but it ran after the receive
transfers had been queued; they were cancelled without data, and from then
on every START stalled.

The open question: is this tied to the source switching, or does any second
START fail? The control test never had a second START. This test answers it
with a stable source and no mode change, on the same build:

- If the second session also stalls: the restart itself is broken (driver,
  or the USB passthrough of the VM). Test B then separates the two.
- If the second session works: the problem is specific to mode changes.

Change in this build: the mode check now runs before the receive transfers
are queued, so they are no longer queued and cancelled for nothing.

## Acceptance criteria (this test)

1. Test A answered: does a second session (close and reopen ffplay, no
   replug) show a picture with a stable 240p source?
2. If not: test B with the VirtualBox USB 3.0 (xHCI) controller.

## Hardware test (maintainer)

Test A: stable 240p source (as in the control test), this branch.

```sh
git fetch origin && git checkout fix/mode-change-restart && git pull
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
# unplug and replug the device, then:
sudo insmod elgato_gchd.ko
v4l2-ctl -d /dev/video0 -i 2
ffplay -f v4l2 -input_format h264 -i /dev/video0
# picture appears: let it run 20 s, close the window (q), wait 5 s, then
# start the same ffplay command again (no replug); wait up to 30 s
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
```

Test B (only if the second session in test A has no picture): shut the VM
down, set *Settings → USB → USB 3.0 (xHCI) Controller*, start the VM, and
repeat test A. If a Linux machine without virtualisation (or a live USB
stick) is available, that is even better than xHCI.

Paste the outputs (labelled A, B) into the maintainer comments below.

## Findings

- 8a4b2af: stopping the receive queue before NULL/STOP prevents NULL from
  completing.
- `gchd_240p-480i_composite`: NULL takes ~0.7 s and is preceded by the
  neutral back-end table; 480i → 240p is not decoder-only.
- 9382bcb test: the mode-change sequence completes, but every START after
  the first one in a power cycle streams only ~0.4 MB.
- `gchd_240p-480i_composite_input_part_*` (bulk data): the driver's receive
  queue handling during stop matches; no reads after STOP.
- 3df7591 test: after the setup-time mode check cancelled the queued
  receive transfers, every START stalled.
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

My impressions after test A:
- The first time I start ffplay it works but upon closing the window, it hangs and gives the error or warning. I still think that a clean shutdown signal is not being sent to the device.

Impressions after test B:
- Lag is either diminished or non-existent, so I guess the USB3 stack helps with that.
- The behavior was similar to test A. This time closing the window did manage to end the program (although I didn't check if the process was actually still there) and it did close quite fast. On second start it did nothing.

Test A:
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
  Duration: N/A, start: 21704.033521, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x781f54000c80] Some buffers are still owned by the caller on close.
Killed
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
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ vq=    0KB sq=    0B f=0/0   

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
[21665.742467] usbcore: registered new interface driver elgato_gchd
[21671.014904] usb 2-1: new high-speed USB device number 24 using ehci-pci
[21671.412505] usb 2-1: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[21671.412528] usb 2-1: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[21671.412535] usb 2-1: Product: GameCapture HD
[21671.412540] usb 2-1: Manufacturer: Elgato
[21671.412545] usb 2-1: SerialNumber: 04107F1267
[21671.415270] elgato_gchd 2-1:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[21671.418216] elgato_gchd 2-1:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[21675.700998] elgato_gchd 2-1:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[21675.701010] elgato_gchd 2-1:1.0: input selected: input=2; detection deferred until device open
[21675.701018] elgato_gchd 2-1:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[21688.757586] elgato_gchd 2-1:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[21689.601643] elgato_gchd 2-1:1.0: transcoder defaults loaded
[21689.797472] elgato_gchd 2-1:1.0: device idle, processor state 0x27f97b
[21690.654021] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[21690.782359] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21690.814972] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[21690.814984] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[21690.814990] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[21690.814995] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[21696.876355] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[21699.158102] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21699.467792] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21699.473154] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21699.506588] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21699.507016] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[21699.515376] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21699.625545] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21699.981641] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21700.027642] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21700.072202] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[21700.117153] elgato_gchd 2-1:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[21700.163089] elgato_gchd 2-1:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[21704.532394] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21709.556550] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21714.579389] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21719.602224] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21724.625070] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21729.648774] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21734.671043] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=795 bytes=52101120 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21739.693759] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=910 bytes=59637760 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21744.718964] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=1025 bytes=67174400 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21812.410306] elgato_gchd 2-1:1.0: hardware shutdown on last close failed: -110; keeping initialized state
[21823.413937] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[21823.542184] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[21823.573588] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[21823.573602] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[21823.573610] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[21823.573615] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[21829.735926] elgato_gchd 2-1:1.0: capture setup: encoder start completed
[21832.152462] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21832.455304] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21832.460333] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0002
[21832.491265] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21832.491607] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[21832.498487] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21832.498531] elgato_gchd 2-1:1.0: START before SCMD: state=0x0002 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21832.502824] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21832.510274] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[21832.512485] elgato_gchd 2-1:1.0: START failed: ret=-5 state=0x0004 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21832.512493] elgato_gchd 2-1:1.0: capture start transition failed: -5
[21834.376939] elgato_gchd 2-1:1.0: capture start failed: -5; stopped, retrying detection
[21835.462852] elgato_gchd 2-1:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[21835.583988] elgato_gchd 2-1:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[21835.615047] elgato_gchd 2-1:1.0: analog format initial status=0x66; configuring 240p
[21835.615060] elgato_gchd 2-1:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[21835.615066] elgato_gchd 2-1:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[21835.615071] elgato_gchd 2-1:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[21837.627597] elgato_gchd 2-1:1.0: capture setup: input configuration completed
[21837.921448] elgato_gchd 2-1:1.0: capture setup: encoder programming completed
[21837.926365] elgato_gchd 2-1:1.0: capture setup: state after final SCMD_INIT=0x0001
[21837.956593] elgato_gchd 2-1:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[21837.956897] elgato_gchd 2-1:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[21837.965456] elgato_gchd 2-1:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[21838.094616] elgato_gchd 2-1:1.0: gchd-diag START transition completed
[21838.480715] elgato_gchd 2-1:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[21838.525806] elgato_gchd 2-1:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[21838.571190] elgato_gchd 2-1:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[21839.101799] elgato_gchd: input 2 signal lost; scheduling redetection
[21842.968873] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21847.973730] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21852.984537] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21857.984509] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21863.080706] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21868.176860] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21873.180201] elgato_gchd 2-1:1.0: async bulk-IN stats: reads=3 bytes=196608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[21877.928712] elgato_gchd 2-1:1.0: redetection: stream stop failed: -110; retrying later
ff5@ff5-VirtualBox:~$ 

Test B:

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
make: Entering directory '/usr/src/linux-headers-6.8.0-146-generic'
  CLEAN   /home/ff5/elgato-gchd/dkms/Module.symvers
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: x86_64-linux-gnu-gcc-13 (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
  You are using:           gcc-13 (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
  CC [M]  /home/ff5/elgato-gchd/dkms/core.o
  CC [M]  /home/ff5/elgato-gchd/dkms/fops.o
  CC [M]  /home/ff5/elgato-gchd/dkms/usb.o
  CC [M]  /home/ff5/elgato-gchd/dkms/transcoder.o
  CC [M]  /home/ff5/elgato-gchd/dkms/input.o
  CC [M]  /home/ff5/elgato-gchd/dkms/protocol_common.o
  LD [M]  /home/ff5/elgato-gchd/dkms/elgato_gchd.o
  MODPOST /home/ff5/elgato-gchd/dkms/Module.symvers
  CC [M]  /home/ff5/elgato-gchd/dkms/elgato_gchd.mod.o
  LD [M]  /home/ff5/elgato-gchd/dkms/elgato_gchd.ko
  BTF [M] /home/ff5/elgato-gchd/dkms/elgato_gchd.ko
Skipping BTF generation for /home/ff5/elgato-gchd/dkms/elgato_gchd.ko due to unavailability of vmlinux
make: Leaving directory '/usr/src/linux-headers-6.8.0-146-generic'
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
[sudo] password for ff5:           
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo insmod elgato_gchd.ko
insmod: ERROR: could not insert module elgato_gchd.ko: Unknown symbol in module
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo modprobe videodev
sudo modprobe videobuf2-v4l2
sudo modprobe videobuf2-vmalloc
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ sudo insmod elgato_gchd.ko
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
  Duration: N/A, start: 161.626529, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x75b800000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

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
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ vq=    0KB sq=    0B f=0/0   

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|open requested|SCMD write succeeded|SCMD interrupt"
[sudo] password for ff5:           
[  122.846493] usbcore: registered new interface driver elgato_gchd
[  141.637776] usb 1-2: new high-speed USB device number 5 using xhci_hcd
[  142.023891] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[  142.023914] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[  142.023921] usb 1-2: Product: GameCapture HD
[  142.023926] usb 1-2: Manufacturer: Elgato
[  142.023931] usb 1-2: SerialNumber: 04107F1267
[  142.031016] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[  142.034404] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[  149.119066] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[  149.119071] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[  149.119075] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[  157.414824] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[  157.528281] elgato_gchd 1-2:1.0: transcoder defaults loaded
[  157.596016] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[  157.735450] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x7e mode=0xe
[  157.737670] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e
[  157.737674] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[  157.796553] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[  157.800003] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[  157.800012] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[  157.854470] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[  157.857617] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[  157.857623] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[  157.912030] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x3e mode=0xe
[  157.915158] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0xee
[  157.915164] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[  157.970897] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[  157.980654] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[  157.982993] elgato_gchd 1-2:1.0: analog format initial status=0xee; configuring 480i
[  157.982996] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[  157.982999] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[  157.983001] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[  158.764786] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[  158.903045] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  158.924596] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  158.925073] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[  158.947806] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  158.948479] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[  158.949019] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  159.592854] elgato_gchd 1-2:1.0: state-change timeout: current=0x01 expected=0x02 state=0x0001 completion=0x011a
[  159.593225] elgato_gchd 1-2:1.0: START failed: ret=-110 state=0x0001 (r=0) enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011a (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  159.593229] elgato_gchd 1-2:1.0: capture start transition failed: -110
[  159.620885] elgato_gchd 1-2:1.0: capture start failed: -110; stopped, retrying detection
[  160.664169] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[  160.679392] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[  160.682627] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[  160.682634] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[  160.682641] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[  160.682645] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[  161.551486] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[  161.573206] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[  161.573568] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0002
[  161.578539] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[  161.579410] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[  161.580330] elgato_gchd 1-2:1.0: START before SCMD: state=0x0002 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[  161.581524] elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
[  161.582244] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[  161.583993] elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
[  161.586702] elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
[  161.589309] elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
[  161.591908] elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
[  166.607474] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=126 bytes=8257536 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  171.630989] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=241 bytes=15794176 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  176.651457] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=356 bytes=23330816 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  181.673888] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=471 bytes=30867456 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  186.696142] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=586 bytes=38404096 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[  190.343792] elgato_gchd 1-2:1.0: hardware shutdown on last close failed: -5; keeping initialized state
[  263.261638] elgato_gchd 1-2:1.0: general input setup failed: input=2 error=-110
[  324.685651] elgato_gchd 1-2:1.0: hardware shutdown on last close failed: -110; keeping initialized state
ff5@ff5-VirtualBox:~$ 
