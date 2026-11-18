# TASK T-046 — Format reporting for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** format reporting confirmed; compliance fixes 4c326eb; hardware test
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

Build and load; `git log -1 --oneline` must show 4c326eb or later:

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

1. Composite (`-i 2`), MPEG format: report.
2. PS1 running, `ffplay -f mpegts /dev/video0`; in a second terminal while
   it plays: report, then `v4l2-compliance -d /dev/video0` (paste the
   lines with "fail" or "warn" and the final summary line). Close ffplay.
3. `v4l2-compliance -d /dev/video0` again without capture; same lines.

Expected: QUERYCAP, ENUM/G/S/QUERY_STD and G/S_PARM no longer fail. The
DV-timings and control failures remain (TODO T-066).

dmesg after the test:

```sh
sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
```

## Findings
- 4a51a75 test (composite, N64 then PS1): before capture NTSC-M, 720×480,
  Interlaced Top-Bottom, 29.97 fps, detected standard none, as expected;
  HDMI: G_STD/QUERYSTD "No data available" as intended. After the PS1
  session: PAL, 25 fps, but 720×480: the size was only updated while no
  buffers were allocated, and ffplay allocates them before detection.
  77ee6ed: G_FMT of composite and S-video reports the current source size
  (also while capturing), else that of the last detected standard. The
  report while ffplay played (step 2) was not in the comments.
- 77ee6ed test: **format reporting works.** Second terminal while
  capturing: PS1 "Composite: ok", PAL detected, 720×576, 25 fps; N64
  NTSC-M detected, 720×480, 29.97 fps. Before and after capture: the last
  standard with "no signal". (I first overlooked these lines at the end
  of the comments.)
- v4l2-compliance 1.26.1 (77ee6ed): fixed in 4c326eb: bus_info without the
  `usb-` path, standards enumerable on HDMI/component, `readbuffers` 0.
  Remaining (older, TODO T-066): DV timings on HDMI/component without a
  signal, `DV_TIMINGS_CAP`, a menu control rejecting item 0,
  `DV_RX_POWER_PRESENT` missing.
- Maintainer question, PAL-60: Elgato lists PAL, NTSC and SECAM for this
  device and does not mention PAL-60. The device captures it anyway (the
  PS1 sends it while booting; it is encoded as a 60 Hz 480-line picture),
  so the driver reports it as `V4L2_STD_PAL_60` when the decoder reports
  60 Hz with PAL colour. SECAM is not reported: no SECAM source tested.

## Maintainer comments

I think the problem you are referring with PAL-60 is just a timing issue with the driver, where it couldn't lock fast enough onto PAL to report it as PAL-50. Otherwise find me a source for a PAL region PS1 that outputs PAL-60.

First terminal:

```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -v pixelformat=MPEG
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: no signal)
Video Standard = 0x00001000
	NTSC-M
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
	Frames per second: 29.970 (30000/1001)
	Read buffers     : 2
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
[mpegts @ 0x7fe4a8000c80] Packet corrupt (stream = 0, dts = 3331800).
[mpegts @ 0x7fe4a8000c80] Packet corrupt (stream = 0, dts = 41400). 
[h264 @ 0x7fe4a84312c0] error while decoding MB 10 19, bytestream -70
[mpegts @ 0x7fe4a8000c80] Packet corrupt (stream = 0, dts = 48600). 
[h264 @ 0x7fe4a84312c0] error while decoding MB 5 18, bytestream -11
[mpegts @ 0x7fe4a8000c80] Packet corrupt (stream = 0, dts = 48600). 
[h264 @ 0x7fe4a830e600] illegal short term buffer state detected0   
[h264 @ 0x7fe4a830e600] Missing reference picture, default is 65537
    Last message repeated 1 times
[h264 @ 0x7fe4a84312c0] error while decoding MB 33 16, bytestream -9
[h264 @ 0x7fe4a815ad40] Missing reference picture, default is 65537 
    Last message repeated 1 times
[h264 @ 0x7fe4a84312c0] Missing reference picture, default is 65537
[mpegts @ 0x7fe4a8000c80] PES packet size mismatchsq=    0B f=4/2   
[mpegts @ 0x7fe4a8000c80] Packet corrupt (stream = 1, dts = 3321720).
[h264 @ 0x7fe4a84312c0] mmco: unref short failure
[mp2 @ 0x7fe4a82ed940] Header missingB vq=    0KB sq=    0B f=4/3   
  17.24 A-V:  0.015 fd= 153 aq=    0KB vq=    0KB sq=    0B f=4/4   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-compliance -d /dev/video0
v4l2-compliance 1.26.1, 64 bits, 64-bit time_t

Compliance test for elgato-gchd device /dev/video0:

Driver Info:
	Driver name      : elgato-gchd
	Card type        : Elgato Game Capture HD
	Bus info         : usb-0000:00:0c.0-2
	Driver version   : 6.8.12
	Capabilities     : 0x85200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format
		Device Capabilities
	Device Caps      : 0x05200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format

Required ioctls:
	test VIDIOC_QUERYCAP: OK
	test invalid ioctls: OK

Allow for multiple opens:
	test second /dev/video0 open: OK
	test VIDIOC_QUERYCAP: OK
	test VIDIOC_G/S_PRIORITY: OK
	test for unlimited opens: OK

Debug ioctls:
	test VIDIOC_DBG_G/S_REGISTER: OK (Not Supported)
	test VIDIOC_LOG_STATUS: OK (Not Supported)

Input ioctls:
	test VIDIOC_G/S_TUNER/ENUM_FREQ_BANDS: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_S_HW_FREQ_SEEK: OK (Not Supported)
	test VIDIOC_ENUMAUDIO: OK (Not Supported)
	test VIDIOC_G/S/ENUMINPUT: OK
	test VIDIOC_G/S_AUDIO: OK (Not Supported)
	Inputs: 4 Audio Inputs: 0 Tuners: 0

Output ioctls:
	test VIDIOC_G/S_MODULATOR: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_ENUMAUDOUT: OK (Not Supported)
	test VIDIOC_G/S/ENUMOUTPUT: OK (Not Supported)
	test VIDIOC_G/S_AUDOUT: OK (Not Supported)
	Outputs: 0 Audio Outputs: 0 Modulators: 0

Input/Output configuration ioctls:
	test VIDIOC_ENUM/G/S/QUERY_STD: OK
		fail: v4l2-test-io-config.cpp(166): TIMINGS cap set, but could not get current timings
		fail: v4l2-test-io-config.cpp(386): Timings check failed for input 0.
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: FAIL
		fail: v4l2-test-io-config.cpp(418): TIMINGS cap set, but could not get timings caps
		fail: v4l2-test-io-config.cpp(475): Timings cap failed for input 0.
	test VIDIOC_DV_TIMINGS_CAP: FAIL
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls (Input 0):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(552): could not set valid menu item 0
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
		warn: v4l2-test-controls.cpp(1139): V4L2_CID_DV_RX_POWER_PRESENT not found for input 3
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 0):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 0):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 0):
		fail: v4l2-test-buffers.cpp(517): node->g_dv_timings(timings)
		fail: v4l2-test-buffers.cpp(642): testCanSetSameTimings(node)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 1):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(552): could not set valid menu item 0
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
		warn: v4l2-test-controls.cpp(1139): V4L2_CID_DV_RX_POWER_PRESENT not found for input 3
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 1):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 1):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 1):
		fail: v4l2-test-buffers.cpp(517): node->g_dv_timings(timings)
		fail: v4l2-test-buffers.cpp(642): testCanSetSameTimings(node)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 2):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(552): could not set valid menu item 0
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 2):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 2):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 2):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 3):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(552): could not set valid menu item 0
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 3):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
	test VIDIOC_S_FMT: OK
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
	test Scaling: OK (Not Supported)

Codec ioctls (Input 3):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 3):
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: OK
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/video0: 115, Succeeded: 107, Failed: 8, Warnings: 4
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
```

Second terminal:
```console
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 --get-input --get-fmt-video --get-parm --get-standard --get-detected-standard
Video input : 2 (Composite: ok)
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Video Standard = 0x000000ff
	PAL-B/B1/G/H/I/D/D1/K
Format Video Capture:
	Width/Height      : 720/576
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
	Read buffers     : 2
ff5@ff5-VirtualBox:~$ v4l2-compliance -d /dev/video0 
v4l2-compliance 1.26.1, 64 bits, 64-bit time_t

Compliance test for elgato-gchd device /dev/video0:

Driver Info:
	Driver name      : elgato-gchd
	Card type        : Elgato Game Capture HD
	Bus info         : usb-0000:00:0c.0-2
	Driver version   : 6.8.12
	Capabilities     : 0x85200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format
		Device Capabilities
	Device Caps      : 0x05200001
		Video Capture
		Read/Write
		Streaming
		Extended Pix Format

Required ioctls:
	test VIDIOC_QUERYCAP: OK
	test invalid ioctls: OK

Allow for multiple opens:
	test second /dev/video0 open: OK
	test VIDIOC_QUERYCAP: OK
	test VIDIOC_G/S_PRIORITY: OK
	test for unlimited opens: OK

Debug ioctls:
	test VIDIOC_DBG_G/S_REGISTER: OK (Not Supported)
	test VIDIOC_LOG_STATUS: OK (Not Supported)

Input ioctls:
	test VIDIOC_G/S_TUNER/ENUM_FREQ_BANDS: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_S_HW_FREQ_SEEK: OK (Not Supported)
	test VIDIOC_ENUMAUDIO: OK (Not Supported)
	test VIDIOC_G/S/ENUMINPUT: OK
	test VIDIOC_G/S_AUDIO: OK (Not Supported)
	Inputs: 4 Audio Inputs: 0 Tuners: 0

Output ioctls:
	test VIDIOC_G/S_MODULATOR: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_ENUMAUDOUT: OK (Not Supported)
	test VIDIOC_G/S/ENUMOUTPUT: OK (Not Supported)
	test VIDIOC_G/S_AUDOUT: OK (Not Supported)
	Outputs: 0 Audio Outputs: 0 Modulators: 0

Input/Output configuration ioctls:
	test VIDIOC_ENUM/G/S/QUERY_STD: OK
		fail: v4l2-test-io-config.cpp(166): TIMINGS cap set, but could not get current timings
		fail: v4l2-test-io-config.cpp(386): Timings check failed for input 0.
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: FAIL
		fail: v4l2-test-io-config.cpp(418): TIMINGS cap set, but could not get timings caps
		fail: v4l2-test-io-config.cpp(475): Timings cap failed for input 0.
	test VIDIOC_DV_TIMINGS_CAP: FAIL
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls (Input 0):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(507): invalid minimum range check
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
		warn: v4l2-test-controls.cpp(1139): V4L2_CID_DV_RX_POWER_PRESENT not found for input 3
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 0):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
		fail: v4l2-test-formats.cpp(462): expected EINVAL, but got 16 when getting format for buftype 1
	test VIDIOC_S_FMT: FAIL
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
		fail: v4l2-test-formats.cpp(1824): doioctl(node, VIDIOC_S_FMT, &fmt)
		fail: v4l2-test-formats.cpp(1972): testBasicScaling(node, fmt)
	test Scaling: FAIL

Codec ioctls (Input 0):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 0):
		fail: v4l2-test-buffers.cpp(573): ret && ret != EINVAL (got 16)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 1):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(507): invalid minimum range check
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
		warn: v4l2-test-controls.cpp(1139): V4L2_CID_DV_RX_POWER_PRESENT not found for input 3
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 1):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
		fail: v4l2-test-formats.cpp(462): expected EINVAL, but got 16 when getting format for buftype 1
	test VIDIOC_S_FMT: FAIL
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
		fail: v4l2-test-formats.cpp(1824): doioctl(node, VIDIOC_S_FMT, &fmt)
		fail: v4l2-test-formats.cpp(1972): testBasicScaling(node, fmt)
	test Scaling: FAIL

Codec ioctls (Input 1):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 1):
		fail: v4l2-test-buffers.cpp(573): ret && ret != EINVAL (got 16)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 2):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(507): invalid minimum range check
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 2):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
		fail: v4l2-test-formats.cpp(462): expected EINVAL, but got 16 when getting format for buftype 1
	test VIDIOC_S_FMT: FAIL
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
		fail: v4l2-test-formats.cpp(1824): doioctl(node, VIDIOC_S_FMT, &fmt)
		fail: v4l2-test-formats.cpp(1972): testBasicScaling(node, fmt)
	test Scaling: FAIL

Codec ioctls (Input 2):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 2):
		fail: v4l2-test-buffers.cpp(573): ret && ret != EINVAL (got 16)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Control ioctls (Input 3):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(507): invalid minimum range check
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls (Input 3):
	test VIDIOC_ENUM_FMT/FRAMESIZES/FRAMEINTERVALS: OK
	test VIDIOC_G/S_PARM: OK
	test VIDIOC_G_FBUF: OK (Not Supported)
	test VIDIOC_G_FMT: OK
	test VIDIOC_TRY_FMT: OK
		fail: v4l2-test-formats.cpp(462): expected EINVAL, but got 16 when getting format for buftype 1
	test VIDIOC_S_FMT: FAIL
	test VIDIOC_G_SLICED_VBI_CAP: OK (Not Supported)
	test Cropping: OK (Not Supported)
	test Composing: OK (Not Supported)
		fail: v4l2-test-formats.cpp(1824): doioctl(node, VIDIOC_S_FMT, &fmt)
		fail: v4l2-test-formats.cpp(1972): testBasicScaling(node, fmt)
	test Scaling: FAIL

Codec ioctls (Input 3):
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls (Input 3):
		fail: v4l2-test-buffers.cpp(573): ret && ret != EINVAL (got 16)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/video0: 115, Succeeded: 97, Failed: 18, Warnings: 2
ff5@ff5-VirtualBox:~$ 
```

dmesg:
```console
ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[sudo] password for ff5:           
[55116.107663] usbcore: registered new interface driver elgato_gchd
[55126.080350] usb 1-2: new high-speed USB device number 53 using xhci_hcd
[55126.466641] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[55126.466649] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[55126.466651] usb 1-2: Product: GameCapture HD
[55126.466653] usb 1-2: Manufacturer: Elgato
[55126.466654] usb 1-2: SerialNumber: 04107F1267
[55126.470099] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[55126.471354] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[55126.489520] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26224 comm=v4l_id
[55126.491257] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[55126.491485] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[55126.492038] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.492053] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.492907] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.492945] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.492954] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.492961] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.494334] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55126.494365] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55136.706834] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26226 comm=v4l2-ctl
[55136.707005] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[55136.707023] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55136.707030] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55152.669893] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26227 comm=v4l2-ctl
[55156.374593] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26228 comm=v4l2-ctl
[55206.128944] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26244 comm=read_thread
[55206.129074] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[55206.262873] elgato_gchd 1-2:1.0: transcoder defaults loaded
[55206.327499] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[55206.682147] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55206.692451] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55206.694402] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55206.694404] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55206.694407] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55206.694409] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55207.452555] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[55207.697772] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55207.717133] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55207.717421] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55207.723511] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55207.724081] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55207.724873] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55207.842905] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55212.750143] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=22
[55217.776598] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=219 bytes=14352384 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55221.673201] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26269 comm=v4l2-ctl
[55222.803165] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=334 bytes=21889024 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55227.828468] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=449 bytes=29425664 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55232.855038] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=564 bytes=36962304 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55237.881702] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=679 bytes=44498944 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55242.908136] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=794 bytes=52035584 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55246.875072] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875236] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875259] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875262] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875265] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875268] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875271] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875273] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875276] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875279] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875282] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875285] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875288] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875291] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875294] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875297] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875300] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875303] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875307] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875310] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875312] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875315] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875318] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875321] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875324] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875326] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875329] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875332] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875335] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875338] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875341] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875343] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875346] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875349] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875353] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875356] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875359] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875362] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875369] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875372] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875374] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875377] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875379] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875381] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875384] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875386] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875388] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875391] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875393] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875395] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875400] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875402] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875404] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875407] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875409] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875412] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875415] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875417] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875419] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875422] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875425] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875427] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875430] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875432] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875436] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875438] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875441] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875443] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875445] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875447] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875450] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875453] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875455] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875457] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875461] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875463] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875465] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875468] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875470] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875472] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875475] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875478] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875481] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875483] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875486] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875488] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875490] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875494] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875495] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875497] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875498] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875500] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875502] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875503] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875505] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875507] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875509] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875510] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875512] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875514] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875515] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875517] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55246.875595] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[55248.064979] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55248.064986] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55248.065007] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55248.065193] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55248.065194] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=1 streaming=1
[55248.065305] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55248.076234] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[55248.076241] elgato_gchd 1-2:1.0: no signal on input=1; retrying detection
[55248.076432] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55248.076444] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55248.076460] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55248.168820] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55248.219902] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55248.228789] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55248.231793] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55248.231797] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55248.231800] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55248.231802] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55248.460523] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55248.482557] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55248.482847] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55248.488231] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55248.488756] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55248.489177] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55248.608310] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55250.483957] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55250.483969] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=1 streaming=1
[55250.484237] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[55250.592500] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55250.646441] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55250.648785] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55250.648790] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55250.649094] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55250.649098] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55250.649800] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[55250.898925] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55251.031573] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55251.040094] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55251.042199] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55251.042201] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55251.042203] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55251.042205] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55251.258215] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55251.274588] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55251.275048] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55251.281486] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55251.282041] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55251.282599] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55251.420498] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55253.401730] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55253.401740] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55253.401777] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55253.402196] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55253.402201] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=1 streaming=1
[55253.402217] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55253.402535] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55253.402566] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55253.402592] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55253.429214] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55253.573829] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[55253.576477] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x6e 0x98:F5=0x62
[55253.576481] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55253.576841] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55253.576845] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=1 streaming=1
[55253.577003] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=1 streaming=1
[55253.607505] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55253.750385] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55253.752831] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55253.752837] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55253.753121] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55253.753126] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55253.753158] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55253.753457] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55253.753459] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=1 streaming=1
[55253.753467] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55253.753721] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55253.753733] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55253.753744] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55253.754120] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55253.754123] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=1 streaming=1
[55253.754184] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=1 streaming=1
[55253.782454] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55253.914114] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55253.916394] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55253.916403] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55253.916628] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55253.916632] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55253.916886] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55253.916911] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55253.916943] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55253.930873] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[55253.930882] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[55253.931162] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55253.931166] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=1 streaming=1
[55253.931184] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55253.931405] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55253.931406] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55253.931410] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55253.931592] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55253.931594] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=1 streaming=1
[55253.931639] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=1 streaming=1
[55253.955312] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55254.080011] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[55254.082544] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55254.082549] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55254.082852] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55254.082856] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=1 streaming=1
[55254.082976] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55254.083235] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55254.083238] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55254.083431] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55254.083444] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55254.336884] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55254.461114] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55254.469529] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55254.471606] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55254.471608] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55254.471610] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55254.471612] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55254.697484] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55254.713806] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55254.714089] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55254.720435] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55254.721299] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55254.722015] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55254.860113] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55256.930024] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55256.931570] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55256.931588] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55256.931596] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55256.931938] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55256.931978] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=1 streaming=1
[55256.964136] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55257.111701] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55257.130778] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[55257.133997] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0xf2
[55257.134009] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55257.134835] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55257.134844] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=1 streaming=1
[55257.134977] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=1 streaming=1
[55257.135912] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55257.135938] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55257.152175] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[55257.152182] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[55257.152417] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55257.152429] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55257.152652] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55257.152656] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55257.152905] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55257.153962] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.154033] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.154061] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.154202] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.154220] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[55257.258089] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55257.316815] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55257.319066] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55257.319068] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55257.319401] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55257.319404] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55257.319485] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[55257.320209] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55257.320230] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55257.333653] elgato_gchd 1-2:1.0: gchd-diag detect HDMI: samples6665=1 avg6665=0xb2b2 samples6463=1 avg6463=0xb2b2 rgb=0
[55257.333657] elgato_gchd 1-2:1.0: no signal on input=0; retrying detection
[55257.333861] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55257.333869] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55257.334127] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55257.334129] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55257.334312] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55257.335138] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.335153] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.335160] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.335198] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.335203] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=1 streaming=1
[55257.335259] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=1 streaming=1
[55257.358412] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55257.499354] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55257.501954] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55257.501957] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55257.502309] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55257.502326] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55257.502576] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55257.502579] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55257.503080] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55257.503084] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55257.503302] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55257.504302] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.504319] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.504326] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.504372] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.504377] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.504380] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[55257.527042] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55257.652456] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xfd mode=0xd
[55257.655272] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55257.655547] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55257.655568] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55257.655628] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.655642] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55257.655645] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55257.655649] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55257.655654] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26270 comm=v4l2-compliance
[55258.058294] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55258.067085] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55258.069197] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55258.069199] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55258.069201] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55258.069202] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55258.296399] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55258.312726] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55258.312940] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55258.318992] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55258.319525] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55258.320365] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55258.458655] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55263.321307] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55268.347756] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55273.375589] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55276.854331] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[55288.493332] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493829] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493842] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493845] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493846] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493848] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493850] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493852] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493854] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493856] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493858] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493864] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493865] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493867] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493869] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493871] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493872] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493875] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493878] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493879] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493881] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493883] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493885] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493887] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493889] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493891] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493892] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493894] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493896] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493898] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493899] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493901] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493903] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493905] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493907] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493909] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493910] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493912] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493914] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493916] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493917] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493919] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493921] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493924] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493925] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493927] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493929] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493931] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493932] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493934] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493936] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493938] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493940] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493942] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493944] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493946] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493948] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493950] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493951] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493954] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493955] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493957] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493959] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493960] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493962] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493964] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493965] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493967] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493969] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493971] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493973] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493975] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493976] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493978] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493980] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493981] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493983] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493987] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493988] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493990] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493992] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493995] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493997] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.493999] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494001] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494002] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494005] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494006] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494008] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494010] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494012] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494014] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494015] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494017] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494019] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494021] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494023] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494026] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494027] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494029] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494031] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494034] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.494132] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[55288.494136] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.494138] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494145] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.494148] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.494149] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[55288.494151] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.494153] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494154] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.494156] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.494159] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.494159] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[55288.494161] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[55288.494164] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494165] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.494167] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[55288.494170] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.494171] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494173] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.494175] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.494176] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[55288.494178] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.494180] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494181] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.494183] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.494186] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.494186] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[55288.494200] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[55288.494203] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.494204] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494207] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.494209] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.494210] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[55288.494213] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.494215] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494216] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.494221] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.494224] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.494225] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[55288.494230] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[55288.494233] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.494234] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494242] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494247] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494255] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.494257] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.494258] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[55288.494261] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.494263] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494264] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.494266] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.494269] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.494270] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[55288.494274] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[55288.494277] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.494278] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[55288.494332] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.494335] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.494336] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.494339] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.494340] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.494343] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.497754] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.497813] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.497820] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.497883] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.497888] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[55288.497893] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.497894] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[55288.497946] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[55288.497948] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.497950] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.497953] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.497954] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.497956] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.497958] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.497960] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.499889] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.499988] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.500009] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.500115] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.500131] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[55288.500134] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.500136] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.500253] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[55288.500256] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.500258] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.500261] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.500262] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.500265] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.500267] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.500269] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.501329] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.501364] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.501370] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.501401] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.501405] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[55288.501452] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[55288.501455] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[55288.501457] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[55288.501459] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[55288.501461] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[55288.501463] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.501465] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[55288.501467] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[55288.502101] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502175] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502194] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502279] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502302] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502305] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[55288.502311] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55288.502312] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.502319] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502323] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.502336] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
[55288.502340] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55288.502353] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26279 comm=v4l2-compliance
ff5@ff5-VirtualBox:~$ 
```
