# TASK T-047 — Close and reopen without errors

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** T-047 fixed; T-031, T-044, T-045 test
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

Use a build of ce4174f or later (`git log -1 --oneline`), N64 on composite.

**A. STREAMOFF → STREAMON on one file handle (T-031).** Test program from
the `tools` branch:

```sh
cd ~/elgato-gchd && git fetch origin
git show origin/tools:tools/stream-restart.c > /tmp/stream-restart.c
gcc -O2 -o /tmp/stream-restart /tmp/stream-restart.c
v4l2-ctl -d /dev/video0 -i 2
v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
/tmp/stream-restart /dev/video0 3 100
```

Expected: three rounds, each "100 buffers, ... bytes", no "no buffer for
30 s". Paste the program output.

**B. Reopen right after closing (T-044):**

```sh
timeout -s INT 15 ffplay -f mpegts /dev/video0; ffplay -f mpegts /dev/video0
```

Expected: the second ffplay shows the picture (it may wait for the close
of the first). Close it.

**C. H.264 format and ffplay's close messages (T-045):**

```sh
v4l2-ctl -d /dev/video0 -v pixelformat=H264
ffplay -f v4l2 -input_format h264 /dev/video0
```

Close after 10 s; note the ffplay messages. Then set MPEG again:
`v4l2-ctl -d /dev/video0 -v pixelformat=MPEG`.

After the test:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads|no analog family|pre-config raw_status|retrying detection"
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

## Maintainer comments

Output of tool:

'''console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ /tmp/stream-restart /dev/video0 3 100
round 1: STREAMON
round 1: 100 buffers, 6542400 bytes, first after 2.66 s, STREAMOFF after 7.89 s
round 2: STREAMON
round 2: 100 buffers, 6542400 bytes, first after 0.13 s, STREAMOFF after 4.46 s
round 3: STREAMON
round 3: 100 buffers, 6542400 bytes, first after 0.13 s, STREAMOFF after 4.45 s
'''

Test B didn't show anything on second open.

Test C:

'''console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=H264
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f v4l2 -input_format h264 /dev/video0
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
  Duration: N/A, start: 52138.248562, bitrate: N/A
  Stream #0:0: Video: h264 (High), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x480 [SAR 10:11 DAR 15:11], 60 fps, 60 tbr, 1000k tbn
[video4linux2,v4l2 @ 0x73ae18000c80] Some buffers are still owned by the caller on close.
ioctl(VIDIOC_QBUF): Bad file descriptor

ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
'''

dmesg for all (I did run some other commands in between):

'''console
ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads|no analog family|pre-config raw_status|retrying detection"
[51751.036510] usbcore: registered new interface driver elgato_gchd
[51827.530091] usb 1-2: new high-speed USB device number 49 using xhci_hcd
[51827.911116] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[51827.911129] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[51827.911131] usb 1-2: Product: GameCapture HD
[51827.911133] usb 1-2: Manufacturer: Elgato
[51827.911134] usb 1-2: SerialNumber: 04107F1267
[51827.916917] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[51827.919253] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[51827.934041] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24047 comm=v4l_id
[51827.936403] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[51827.936577] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[51827.937154] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.937166] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938365] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938399] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938407] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938415] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938510] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51827.938530] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[51828.906479] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24049 comm=v4l2-ctl
[51828.906520] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[51828.906525] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[51828.906527] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[51831.952778] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24050 comm=v4l2-ctl
[51857.625115] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24051 comm=stream-restart
[51857.627742] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[51857.782113] elgato_gchd 1-2:1.0: transcoder defaults loaded
[51857.847561] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[51858.142714] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[51858.220072] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[51858.280307] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51858.282406] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[51858.282408] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[51858.282409] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[51858.282411] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[51859.036219] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[51859.792434] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[51859.807806] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[51859.808009] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[51859.813397] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[51859.813946] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[51859.814368] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[51859.914168] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[51862.743770] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[51864.142651] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[51864.142977] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[51864.148278] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[51864.148799] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[51864.149227] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[51864.265246] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[51864.265248] elgato_gchd 1-2:1.0: analog mode now 480i
[51869.189145] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[51874.211866] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[51875.313941] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[51970.574662] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24069 comm=read_thread
[51970.574733] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[51970.687614] elgato_gchd 1-2:1.0: transcoder defaults loaded
[51970.753763] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[51970.837893] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[51971.024335] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[51971.054125] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x26 masked_mode=0x6
[51971.056310] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[51971.231547] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[51971.233936] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[51971.309352] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51971.387713] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51971.389944] elgato_gchd 1-2:1.0: analog format initial status=0x66; configuring 240p
[51971.389946] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=0 signal_present=1
[51971.389948] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=0
[51971.389950] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=0
[51972.201920] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[51972.977024] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[51972.992752] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[51972.993078] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[51972.999745] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[51973.000237] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[51973.000837] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[51973.099048] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[51975.539310] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[51976.882847] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[51976.883179] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[51976.888311] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[51976.888882] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[51976.889284] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[51976.989149] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[51976.989154] elgato_gchd 1-2:1.0: analog mode now 480i
[51979.957734] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[51981.571827] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24108 comm=read_thread
[51981.571919] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[51981.633377] elgato_gchd 1-2:1.0: transcoder defaults loaded
[51981.694510] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[51981.796984] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[51981.979255] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[51982.065211] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[51982.092388] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51982.095557] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[51982.159777] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[51982.189420] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[51982.191633] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[51982.258788] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51982.261242] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[51982.332102] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[51982.334707] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[51982.334710] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[51982.334713] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[51982.334716] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[51983.140924] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[51983.265688] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[51983.283537] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[51983.283768] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[51983.306703] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[51983.307634] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[51983.308135] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[51983.439744] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[51988.318377] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[51993.339584] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[51997.956982] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[52048.628879] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24149 comm=read_thread
[52048.628947] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[52048.752007] elgato_gchd 1-2:1.0: transcoder defaults loaded
[52048.811347] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[52048.912178] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[52049.143580] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[52049.178981] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52049.181245] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[52049.251877] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52049.254027] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=e9); waiting up to 5000 ms
[52049.323743] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52049.399016] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52049.472163] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52049.474173] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[52049.474175] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[52049.474177] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[52049.474179] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[52050.263354] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[52050.391091] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[52050.412382] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52050.412597] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52050.418309] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52050.419053] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[52050.419601] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[52050.535113] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52055.455616] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[52060.479721] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[52064.261008] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[52064.385455] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24188 comm=read_thread
[52064.385535] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[52065.123349] elgato_gchd 1-2:1.0: transcoder defaults loaded
[52065.185340] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[52104.659763] elgato_gchd 1-2:1.0: shutdown: device in STOP after RESET; IDLE and RESET again
[52104.675521] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0011 before, 0x0012 now)
[52128.404737] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24194 comm=v4l2-ctl
[52135.562548] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24208 comm=read_thread
[52135.589856] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[52135.696123] elgato_gchd 1-2:1.0: transcoder defaults loaded
[52135.760235] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[52135.846735] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[52135.966244] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[52136.050170] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[52136.077564] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52136.079665] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=89); waiting up to 5000 ms
[52136.154058] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52136.228901] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52136.304115] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52136.375461] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[52136.446171] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[52136.448781] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[52136.448784] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[52136.448787] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[52136.448789] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[52137.238332] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[52137.351318] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[52137.366039] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52137.366267] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52137.387645] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52137.388644] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[52137.389182] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[52137.505397] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52138.860996] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[52140.227404] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[52140.227695] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[52140.232898] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[52140.233737] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[52140.234096] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[52140.333266] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[52140.333269] elgato_gchd 1-2:1.0: analog mode now 240p
[52145.257423] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[52150.279406] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[52153.135036] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[52161.291627] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=24224 comm=v4l2-ctl
ff5@ff5-VirtualBox:~$ 
'''
