# TASK T-047 — Close and reopen without errors

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** T-047, T-031 done; T-045 application-side; T-044 reboot gap (96382eb) test
- **Type:** driver fix
- **Branch:** `fix/close-lifecycle` (from `dev` 0.5.0)
- **TODO items:** T-047, T-044, T-031, T-045 (stage 1)

## Why

`dev` 0.5.0 captures composite and S-video in all NTSC and PAL modes, with
H.264 or MPEG-TS with audio. Closing is not clean in every case:

- **T-047:** `hardware shutdown on last close failed: -5; keeping initialized
  state` after a session in which no source was found (seen in four tests
  on `feat/ts-audio`). -5 (`-EIO`) comes from a state change that finds the
  device in an unexpected state; the log does not say which step.
- **T-044:** a new open while the previous close is still running failed
  once.
- **T-031:** STREAMOFF → STREAMON without closing is untested.
- **T-045:** ffplay on close (H.264 format): "VIDIOC_QBUF: Bad file
  descriptor", "Some buffers are still owned by the caller".

## Implementation

1. Diagnostic (this build): the shutdown logs each step it runs with the
   device state before and after, and the step that fails.
2. Fix from the diagnostic: the shutdown sequence for a device that never
   started capturing.
3. T-044, T-031, T-045 after that, one at a time.

## Hardware test (maintainer)

Build and load (once); `git log -1 --oneline` must show dc1035a or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout fix/close-lifecycle && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
```

Use a build of 96382eb or later (`git log -1 --oneline`), N64 on composite,
MPEG format.

**Reopen right after closing (T-044), three times in a row:**

```sh
timeout -s INT 15 ffplay -f mpegts /dev/video0; ffplay -f mpegts /dev/video0
```

Expected each time: the second ffplay shows the picture after about 3-4 s
(dmesg: `waiting ... ms after the previous shutdown`). Close it, wait a
few seconds, repeat.

After the test, the log **with** the detection lines:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
```

## Findings

- 9e098d3 test (composite, no console, two sessions closed with Ctrl+C):
  first close failed at RESET (state `0x11` IDLE before, `0x01` STOP
  after): the session never started capture; after the shutdown sequence
  RESET found the device in STOP. The second close, from STOP, succeeded
  (`0x01` → `0x12`). dc1035a: when RESET leaves the device in STOP, IDLE and
  RESET are sent once more (the sequence that worked from STOP).
- dc1035a test: both closes after sessions without a source now end with
  "device in STOP after RESET; IDLE and RESET again" and "hardware shutdown
  done (0x0011 → 0x0012)"; closes after capture `0x0001 → 0x0012`. T-047
  fixed. Session 2 (ffplay alone, after the first close, N64 meant to be
  on) found no source for 33 s; session 3 (after `v4l2-ctl -i 2` and
  `-v pixelformat=MPEG`) and session 4 (`-v pixelformat=MPEG` and ffplay)
  showed the N64. With input 2 already selected `-i 2` changes nothing, and
  the format is kept across closes, so both commands should have no
  effect; the detection lines of session 2 were not in the filtered log.
- dc1035a reproduction: after a close from a session without a source,
  `ffplay` alone (no `v4l2-ctl`) detected the N64 (240p, then 480i during
  setup) and captured; closes `0x11 → 0x12` and `0x01 → 0x12`. The earlier
  session without detection did not recur (not reproduced; possibly the
  N64 was not on yet). **T-047 done.** The wait line called the N64's
  unsettled standard (`E7 = E9`) "60 Hz signal with PAL colour"; message
  fixed in ce4174f.
- ce4174f test: **T-031 works** (stream-restart, 3 rounds of STREAMON /
  100 buffers / STREAMOFF on one handle; first buffer after 2.66 s, then
  0.13 s). **T-045 is application-side:** "Some buffers are still owned by
  the caller on close" and "ioctl(VIDIOC_QBUF): Bad file descriptor" are
  printed by ffmpeg's V4L2 input, which closes the file while it still
  holds buffers and returns them afterwards. **T-044:** an open 0.12 s
  after the previous shutdown booted but found no source for 39 s (no
  back-end retrigger at the start, unlike every other session); opens
  1.6 s and later after a shutdown worked. 96382eb: a boot waits until 1.5 s
  after the last shutdown. (My log filter hid the detection lines of that
  session; the new command keeps them.)

## Maintainer comments

This time it worked:

'''console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ timeout -s INT 15 ffplay -f mpegts /dev/video0; ffplay -f mpegts /dev/video0
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
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
Input #0, mpegts, from '/dev/video0':B vq=    0KB sq=    0B f=0/0   
  Duration: N/A, start: 0.500000, bitrate: N/A
  Program 1 
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 29.97 fps, 59.94 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x75242c000c80] Packet corrupt (stream = 0, dts = 363318).
[mpegts @ 0x75242c000c80] PES packet size mismatch
[mpegts @ 0x75242c000c80] Packet corrupt (stream = 1, dts = 343080).
[mp2 @ 0x75242c163c40] Header missingB vq= 2099KB sq=    0B f=0/0   
   5.36 A-V:  0.010 fd=  20 aq=   40KB vq= 1604KB sq=    0B f=1/1   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 


ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[52418.198125] usbcore: registered new interface driver elgato_gchd
[52422.905499] usb 1-2: new high-speed USB device number 50 using xhci_hcd
[52423.282248] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[52423.282265] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[52423.282270] usb 1-2: Product: GameCapture HD
[52423.282274] usb 1-2: Manufacturer: Elgato
[52423.282277] usb 1-2: SerialNumber: 04107F1267
[52423.286963] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[52423.288396] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[52423.303975] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24655 comm=v4l_id
[52423.305394] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[52423.305547] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[52423.305949] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.305959] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.306181] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.306205] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.306213] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.306219] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.306983] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52423.307013] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[52451.028147] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24659 comm=v4l2-ctl
[52451.028248] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[52451.028259] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[52451.028278] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[52460.873698] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24660 comm=v4l2-ctl
[52469.094565] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24675 comm=read_thread
[52469.094634] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[52469.244251] elgato_gchd 1-2:1.0: transcoder defaults loaded
[52469.305560] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[52469.643636] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[52469.693597] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[52469.701971] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52469.703991] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[52469.703993] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[52469.703995] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[52469.703997] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[52470.477820] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[52471.261096] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[52471.276457] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52471.276925] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52471.282122] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52471.282705] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[52471.283144] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[52471.399866] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52476.280718] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[52481.303745] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[52484.842968] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[52484.976747] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24713 comm=read_thread
[52484.976931] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[52484.976940] elgato_gchd 1-2:1.0: waiting 1366 ms after the previous shutdown
[52487.047381] elgato_gchd 1-2:1.0: transcoder defaults loaded
[52487.109733] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[52487.237423] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[52487.239564] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0e 0x88:CA=0x6e 0x98:F5=0x62
[52487.239567] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[52487.301496] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[52487.311972] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52487.314372] elgato_gchd 1-2:1.0: analog format status 0x6e not locked yet
[52487.314375] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[52487.374086] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[52487.386196] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[52487.388471] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[52487.388476] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[52487.444893] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[52487.456318] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[52487.458701] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[52487.458704] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[52487.458707] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[52487.458709] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[52488.246689] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[52488.988752] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[52489.006143] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52489.006370] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52489.012766] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52489.013307] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[52489.014080] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[52489.112904] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52492.368182] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[52493.912361] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52493.912601] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52493.918746] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52493.919284] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[52493.919746] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[52494.035499] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52494.035503] elgato_gchd 1-2:1.0: analog mode now 480i
[52498.956329] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=9
[52502.238923] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
ff5@ff5-VirtualBox:~$ 
'''
