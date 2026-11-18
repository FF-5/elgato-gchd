# TASK T-046 — Format reporting for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (168cb35); hardware test
- **Type:** driver feature (V4L2 reporting)
- **Branch:** `feat/format-reporting` (from `dev` 0.5.1)
- **TODO items:** T-046, T-022, T-042 (stage 1)

## Why

Applications learn the source format from V4L2 before and while capturing.
Today:

- **T-046:** before the first detection the frame rate is the NTSC value;
  ffplay showed 60 fps for a PAL source on the first open. G_PARM reports
  the field rate as an integer (60/1, 50/1), while the encoded stream has
  29.97 (NTSC) or 25 (PAL) frames per second (ffprobe of every recording).
- **T-022:** field order: analog sources are always encoded interlaced
  (`top first` in ffprobe), also 240p/288p; G_FMT reports
  `V4L2_FIELD_INTERLACED`. To be checked against `V4L2_FIELD_INTERLACED_TB`
  / `_BT` for NTSC and PAL.
- **T-042:** the current analog mode (NTSC 480i/240p, PAL 576i/288p, PAL
  60) is visible only in dmesg.

## Implementation (plan)

1. Frame rate: G_PARM and ENUM_FRAMEINTERVALS report the encoded frame
   rate, 30000/1001 for the 60 Hz family and 25/1 for the 50 Hz family.
   Before detection: the family last detected on this input while the
   module is loaded (NTSC on first use).
2. Field order from the stream (`top first` for both families per
   ffprobe); G_FMT and the buffer `field` agree.
3. Analog standard: `VIDIOC_QUERYSTD`, `VIDIOC_G_STD` and the `std` field
   of ENUMINPUT for inputs 2 and 3 (`V4L2_STD_NTSC_M`, `V4L2_STD_PAL`,
   `V4L2_STD_PAL_60` for 60 Hz with PAL colour); ENUMINPUT `status`
   `V4L2_IN_ST_NO_SIGNAL` while no source is detected. 240p/288p versus
   480i/576i in the input status log and the source-change event.

## Hardware test (maintainer)

Build and load; `git log -1 --oneline` must show 168cb35 or later:

```sh
cd ~/elgato-gchd && git fetch origin && git checkout feat/format-reporting && git pull
git log -1 --oneline
cd dkms && make -C /lib/modules/$(uname -r)/build M=$PWD clean modules
sudo rmmod elgato_gchd 2>/dev/null; sudo dmesg -C
sudo modprobe -a videodev videobuf2-v4l2 videobuf2-vmalloc
sudo insmod ./elgato_gchd.ko
```

The report command used below (paste its output each time):

```sh
v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
```

1. Composite (`-i 2`), MPEG format, before any capture: report.
   Expected: 720×480, field "Interlaced Top-Bottom", 29.97 fps, standard
   NTSC-M, detected standard: none/unknown.
2. PS1 on, game running, `ffplay -f mpegts /dev/video0`. In a second
   terminal while it plays: report. Expected: 576 lines, 25 fps, PAL, detected PAL.
3. Close ffplay; report again. Expected: still 576 / 25 fps / PAL
   (detected: none).
4. N64 instead of the PS1, ffplay again; report while it plays. Expected:
   480 lines, 29.97 fps, NTSC-M.
5. Optional: `v4l2-ctl -d /dev/video0 --list-inputs` and
   `v4l2-compliance -d /dev/video0` (package `v4l-utils`); paste the
   summary lines.

dmesg after the test:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
```

## Findings

## Maintainer comments

PAL seems to be misreporting to be 720/480:

```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 0 (HDMI: no signal)
VIDIOC_G_STD: failed: No data available
VIDIOC_QUERYSTD: failed: No data available
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
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 60.000 (60/1)
	Read buffers     : 0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: no signal)
Video Standard = 0x00001000
	NTSC-M
Video Standard = 0x00000000
	
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'H264' (H.264)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 8388608
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 29.970 (30000/1001)
	Read buffers     : 0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0
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
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 0, dts = 1111065).
[mpegts @ 0x7629f8000c80] PES packet size mismatchsq=    0B f=0/0   
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 1, dts = 1092600).
[mp2 @ 0x7629f82c9a40] Header missingB vq=  919KB sq=    0B f=0/0   
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 0, dts = 2729682).
[mpegts @ 0x7629f8000c80] PES packet size mismatchsq=    0B f=2/1   
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 1, dts = 2708280).
[mp2 @ 0x7629f82c9a40] Header missingB vq=    0KB sq=    0B f=2/1   
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 0, dts = 2957910).
[h264 @ 0x7629f8239840] error while decoding MB 26 29, bytestream -16
[mpegts @ 0x7629f8000c80] PES packet size mismatchsq=    0B f=3/2   
[mpegts @ 0x7629f8000c80] Packet corrupt (stream = 1, dts = 2941560).
[mp2 @ 0x7629f82c9a40] Header missingB vq=    0KB sq=    0B f=3/2   
   6.30 A-V: -0.012 fd= 111 aq=    0KB vq=    0KB sq=    0B f=3/3   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ ffplay -f mpegts /dev/video0
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
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
  33.98 A-V: -0.011 fd=   0 aq=   40KB vq= 1435KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: no signal)
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Video Standard = 0x00000000
	
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 25.000 (25/1)
	Read buffers     : 0
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 

ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[sudo] password for ff5:           
[53667.362243] usbcore: registered new interface driver elgato_gchd
[53671.175609] usb 1-2: new high-speed USB device number 51 using xhci_hcd
[53671.567734] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[53671.567756] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[53671.567762] usb 1-2: Product: GameCapture HD
[53671.567767] usb 1-2: Manufacturer: Elgato
[53671.567771] usb 1-2: SerialNumber: 04107F1267
[53671.574488] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[53671.586649] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[53671.592296] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25165 comm=v4l_id
[53671.593410] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[53671.593718] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[53671.594160] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.594171] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595152] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595190] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595202] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595213] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595398] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53671.595428] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[53694.094595] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25168 comm=v4l2-ctl
[53721.150466] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25169 comm=v4l2-ctl
[53721.150616] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[53721.150634] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[53721.150640] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[53723.573403] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25170 comm=v4l2-ctl
[53742.384592] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25171 comm=v4l2-ctl
[53763.886737] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25185 comm=read_thread
[53763.886803] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[53764.060630] elgato_gchd 1-2:1.0: transcoder defaults loaded
[53764.126779] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[53764.488574] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53764.498414] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53764.500518] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 480i
[53764.500520] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x480 fps=60/1 interlaced=1 signal_present=1
[53764.500522] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x480 fps=60/1 interlaced=1
[53764.500523] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x480 fps=60/1 interlaced=1
[53765.298475] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[53765.421867] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[53765.438290] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[53765.438576] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[53765.461022] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[53765.461582] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[53765.462037] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[53765.561955] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[53770.483264] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=23
[53775.506036] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[53778.549016] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[53780.014113] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[53780.014470] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[53780.020270] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[53780.021749] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[53780.022441] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[53780.120861] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[53780.120867] elgato_gchd 1-2:1.0: analog mode now 240p
[53783.517156] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25225 comm=v4l2-ctl
[53785.043540] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53790.066724] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53795.088569] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53800.111117] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53805.133404] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53809.473209] elgato_gchd 1-2:1.0: analog mode change detected: 240p -> 480i
[53810.156609] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53810.871865] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[53810.872199] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[53810.878182] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[53810.878641] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=1
[53810.879629] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[53810.994891] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[53810.994893] elgato_gchd 1-2:1.0: analog mode now 480i
[53813.775682] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25227 comm=v4l2-ctl
[53815.916931] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=105 bytes=6881280 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53820.940330] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=220 bytes=14417920 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53825.962260] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=335 bytes=21954560 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53830.984293] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=450 bytes=29491200 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53836.007448] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=565 bytes=37027840 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53841.031154] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=680 bytes=44564480 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53842.776358] elgato_gchd 1-2:1.0: analog mode change detected: 480i -> 240p
[53844.284445] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[53844.284801] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[53844.289940] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[53844.290540] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x480 fps=60/1 interlaced=0
[53844.291028] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[53844.407170] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[53844.407172] elgato_gchd 1-2:1.0: analog mode now 240p
[53849.288779] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[53851.923713] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[53884.532926] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25245 comm=read_thread
[53884.533056] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[53885.157193] elgato_gchd 1-2:1.0: transcoder defaults loaded
[53885.216681] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[53885.321856] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[53885.375773] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[53885.378023] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[53885.378027] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.441667] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[53885.448696] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[53885.448708] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.517422] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[53885.520214] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[53885.520217] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.586788] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[53885.590569] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x72
[53885.590575] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.654733] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[53885.657228] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53885.657231] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.719542] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[53885.722784] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53885.722788] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.786711] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[53885.789203] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53885.789207] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.855474] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xed mode=0xd
[53885.862134] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53885.862139] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.930458] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[53885.938585] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53885.938593] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53885.998039] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0xfd mode=0xd
[53886.001507] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0xee 0x98:F5=0x72
[53886.001512] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53886.060153] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[53886.082322] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[53886.147136] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0e mode=0xe
[53886.217812] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x2e mode=0xe
[53886.289558] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[53886.301477] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[53886.304052] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[53886.371725] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[53886.389159] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x36 mode=0x6
[53886.397841] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x36 masked_mode=0x6
[53886.400216] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[53886.457821] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53886.467476] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53886.470727] elgato_gchd 1-2:1.0: analog format status 0xe6 not locked yet
[53886.531341] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53886.544223] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53886.546432] elgato_gchd 1-2:1.0: decoder standard not settled (0x88:E7=d9); waiting up to 5000 ms
[53886.604222] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53886.612789] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53886.678685] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53886.688172] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53886.749218] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53886.758404] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53886.823669] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53886.834580] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53886.898458] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53886.907501] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53886.969060] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53886.979473] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.042652] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.051624] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.115837] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.125680] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.192012] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53887.203394] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53887.276768] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53887.292937] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.361342] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53887.376866] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53887.441039] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53887.453650] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.522763] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.544333] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53887.605825] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.614744] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53887.678284] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.689706] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.756164] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.765288] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.826110] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53887.836919] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53887.911218] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.919687] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53887.979681] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53887.988936] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.058212] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.070702] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53888.137020] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.148680] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.212385] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.221624] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.292257] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.305934] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53888.377894] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.392641] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.451683] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.460509] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.530762] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.542908] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53888.614342] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.624990] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.697827] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.710699] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53888.778692] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.794890] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.854990] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53888.867056] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53888.937807] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53888.946674] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.013931] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.026687] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53889.094795] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.105585] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.165760] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.177124] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.244034] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.263674] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53889.339691] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.349762] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.410935] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.421594] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.489612] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.499274] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53889.563766] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.577966] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.649717] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.660551] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53889.733075] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.745503] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.810177] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x16 mode=0x6
[53889.820277] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53889.890947] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.903428] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x16 masked_mode=0x6
[53889.968917] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[53889.982247] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[53890.045632] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[53890.066116] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x36 mode=0x6
[53890.076129] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x2e masked_mode=0xe
[53890.136734] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[53890.156143] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[53890.217236] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[53890.235689] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[53890.297175] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[53890.307854] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[53890.310127] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.368683] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[53890.379653] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[53890.382613] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.382617] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.445528] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[53890.465792] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[53890.474289] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[53890.476604] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.476612] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.537280] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[53890.560063] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[53890.568050] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[53890.570056] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.570059] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.637017] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[53890.649511] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[53890.651836] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.651839] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.716898] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[53890.734457] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[53890.737354] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.737359] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.796969] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[53890.807477] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[53890.809802] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[53890.809805] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[53890.886262] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[53890.898710] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[53890.900834] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[53890.900837] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[53890.900839] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[53890.900841] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[53891.691563] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[53891.923534] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[53891.938517] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[53891.938825] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[53891.944460] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[53891.945165] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[53891.945827] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[53892.084759] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[53896.949403] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[53899.640464] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25268 comm=v4l2-ctl
[53901.977125] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[53907.004332] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[53912.029584] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[53913.783495] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25269 comm=v4l2-ctl
[53917.055410] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=44
[53922.081640] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[53927.108382] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[53932.149644] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[53938.804888] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=25275 comm=v4l2-ctl
ff5@ff5-VirtualBox:~$ 
```

These were taken while ffplay was running:

```console
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: ok)
Video Standard = 0x00001000
	NTSC-M
Video Standard = 0x00001000
	NTSC-M
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 29.970 (30000/1001)
	Read buffers     : 0
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: ok)
Video Standard = 0x00001000
	NTSC-M
Video Standard = 0x00001000
	NTSC-M
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 29.970 (30000/1001)
	Read buffers     : 0
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: ok)
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 25.000 (25/1)
	Read buffers     : 0
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: ok)
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Format Video Capture:
	Width/Height      : 720/480
	Pixel Format      : 'MPEG' (MPEG-1/2/4)
	Field             : Interlaced Top-Bottom
	Bytes per Line    : 0
	Size Image        : 65424
	Colorspace        : Rec. 709
	Transfer Function : Default (maps to Rec. 709)
	YCbCr/HSV Encoding: Default (maps to Rec. 709)
	Quantization      : Default (maps to Full Range)
	Flags             : 
Streaming Parameters Video Capture:
	Capabilities     : timeperframe
	Frames per second: 25.000 (25/1)
	Read buffers     : 0
ff5@ff5-VirtualBox:~$ 

```
