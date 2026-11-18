# TASK T-046 — Format reporting for analog sources

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** compliance fixes confirmed; S_INPUT busy, no PAL-60 (fd8426b); hardware test
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

Build and load; `git log -1 --oneline` must show fd8426b or later:

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

1. Composite (`-i 2`), MPEG format, PS1 running: `ffplay -f mpegts
   /dev/video0`.
2. Second terminal while it plays: `v4l2-ctl -d /dev/video0 -i 3`.
   Expected: "VIDIOC_S_INPUT: failed: Device or resource busy"; ffplay
   keeps playing. Then the report command: PAL, 720×576, 25 fps.
3. Second terminal: `v4l2-compliance -d /dev/video0`. Expected: ffplay
   keeps playing; in dmesg no "input switch requested" lines with
   `streaming=1` from the S_INPUT test (the input menu control may still
   switch; TODO T-066). Paste the summary line.
4. Close ffplay; power the PS1 off and on while no capture runs; start
   ffplay: expected 576i after the wait; report: PAL.

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
- 4c326eb test: v4l2-compliance without capture 8 failures, all on the
  T-066 list; QUERYCAP (bus_info `usb-0000:00:0c.0-2`), STD and G/S_PARM
  pass. During capture (second terminal) compliance switched through all
  inputs and each switch stopped and restarted the running capture;
  fd8426b: S_INPUT to another input returns EBUSY for a file handle that does
  not own the buffers.
- PAL-60 (maintainer: probably a driver timing issue; source wanted): no
  source found that documents a PAL PS1 sending 60 Hz at start-up. The
  decoder is fully locked during it (`FA = 6`, `CA = 66`, `E7 = 59` for
  3-4 s) and the Windows recording reads the same at the PS1 start
  (`input-switching/gchd_PAL_composite_svideo_small` 10.70 s: `FA=16
  CA=66 E7=59`; Windows starts NTSC 240p there). Maintainer decision
  followed: PAL-60 is not reported; 60 Hz sources report NTSC-M (fd8426b).

## Maintainer comments

I was very clear when I uploaded the traces. I have the impression at least the version of the driver I'm using on Windows has a problem locking to PAL sources. It's not about the PS1. I could test it later when we tackle component input, but that's my guess: the Windows version has an unstable lock and reports wrong.

First terminal:
```console
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
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
  Stream #0:0[0x1011]: Video: h264 (High) (HDMV / 0x564D4448), yuv420p(tv, smpte170m/smpte170m/bt709, top first), 720x576 [SAR 12:11 DAR 15:11], 25 fps, 50 tbr, 90k tbn
  Stream #0:1[0x10f]: Audio: mp2 ([3][0][0][0] / 0x0003), 48000 Hz, stereo, fltp, 320 kb/s
[mpegts @ 0x7c3d04000c80] Packet corrupt (stream = 0, dts = 4746600).
[h264 @ 0x7c3d04160900] error while decoding MB 15 25, bytestream -32
[mpegts @ 0x7c3d04000c80] PES packet size mismatchsq=    0B f=1/0   
[mpegts @ 0x7c3d04000c80] Packet corrupt (stream = 1, dts = 4732200).
[mp2 @ 0x7c3d05675380] Header missingB vq=    0KB sq=    0B f=1/0   
  57.64 A-V:  0.000 fd= 333 aq=    0KB vq=    0KB sq=    0B f=1/1   
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
  17.42 A-V: -0.015 fd=   0 aq=   40KB vq= 1411KB sq=    0B f=0/0   
ff5@ff5-VirtualBox:~/elgato-gchd/dkms$ 
```

Second terminal:
```console
ff5@ff5-VirtualBox:~$ v4l2-ctl -d /dev/video0 -i 3
VIDIOC_S_INPUT: failed: Device or resource busy
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
		fail: v4l2-test-input-output.cpp(545): could not set input to 0
	test VIDIOC_G/S/ENUMINPUT: FAIL
	test VIDIOC_G/S_AUDIO: OK (Not Supported)
	Inputs: 0 Audio Inputs: 0 Tuners: 0

Output ioctls:
	test VIDIOC_G/S_MODULATOR: OK (Not Supported)
	test VIDIOC_G/S_FREQUENCY: OK (Not Supported)
	test VIDIOC_ENUMAUDOUT: OK (Not Supported)
	test VIDIOC_G/S/ENUMOUTPUT: OK (Not Supported)
	test VIDIOC_G/S_AUDOUT: OK (Not Supported)
	Outputs: 0 Audio Outputs: 0 Modulators: 0

Input/Output configuration ioctls:
	test VIDIOC_ENUM/G/S/QUERY_STD: OK (Not Supported)
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: OK (Not Supported)
	test VIDIOC_DV_TIMINGS_CAP: OK (Not Supported)
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls:
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
		fail: v4l2-test-controls.cpp(507): invalid minimum range check
	test VIDIOC_G/S_CTRL: FAIL
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 5 Private Controls: 1

Format ioctls:
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

Codec ioctls:
	test VIDIOC_(TRY_)ENCODER_CMD: OK (Not Supported)
	test VIDIOC_G_ENC_INDEX: OK (Not Supported)
	test VIDIOC_(TRY_)DECODER_CMD: OK (Not Supported)

Buffer ioctls:
		fail: v4l2-test-buffers.cpp(573): ret && ret != EINVAL (got 16)
	test VIDIOC_REQBUFS/CREATE_BUFS/QUERYBUF: FAIL
	test CREATE_BUFS maximum buffers: OK
	test VIDIOC_EXPBUF: OK (Not Supported)
	test Requests: OK (Not Supported)

Total for elgato-gchd device /dev/video0: 46, Succeeded: 41, Failed: 5, Warnings: 0
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
ff5@ff5-VirtualBox:~$ 
```

dmesg:
```console
ff5@ff5-VirtualBox:~$ sudo dmesg | grep -vE "bulk-IN data|START poll=|firmware chunk|SCMD write succeeded|SCMD interrupt|callbacks suppressed|bulk-IN first reads"
[55805.995974] usbcore: registered new interface driver elgato_gchd
[55813.552320] usb 1-2: new high-speed USB device number 54 using xhci_hcd
[55813.940538] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[55813.940553] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[55813.940559] usb 1-2: Product: GameCapture HD
[55813.940563] usb 1-2: Manufacturer: Elgato
[55813.940566] usb 1-2: SerialNumber: 04107F1267
[55813.946557] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[55813.950954] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[55813.970294] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26728 comm=v4l_id
[55813.972468] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[55813.973104] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1379 comm=wireplumber
[55813.973957] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.973968] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.974743] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.974778] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.974786] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.974794] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.975126] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55813.975200] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1377 comm=pipewire
[55831.402253] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26731 comm=v4l2-ctl
[55831.402298] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[55831.402303] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[55831.402305] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[55834.583051] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26732 comm=v4l2-ctl
[55845.118923] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26746 comm=read_thread
[55845.118988] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[55845.778082] elgato_gchd 1-2:1.0: transcoder defaults loaded
[55845.840126] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[55846.102461] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55846.197799] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55846.248181] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.256589] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.258612] elgato_gchd 1-2:1.0: 60 Hz signal with PAL colour (0x88:E7=59); waiting up to 5000 ms
[55846.258616] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.318301] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.330505] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.332975] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.400415] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.413944] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.416675] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.480042] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.492814] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.495195] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.573343] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.584142] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.586917] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.644244] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.653284] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.655389] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.719835] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.730152] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.732138] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.793385] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.802624] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.804672] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.865450] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.875953] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.878023] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55846.940889] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55846.949664] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55846.951639] elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
[55847.008055] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55847.017692] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55847.078604] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55847.088443] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55847.147021] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55847.155465] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55847.215191] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55847.223816] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55847.290997] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x06 mode=0x6
[55847.304861] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x06 masked_mode=0x6
[55847.368132] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55847.386013] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x26 mode=0x6
[55847.393921] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x0e masked_mode=0xe
[55847.454778] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55847.471801] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[55847.473979] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xee 0x98:F5=0xf2
[55847.529806] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x0f mode=0xf
[55847.532087] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xe6 0x98:F5=0x72
[55847.592188] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55847.611837] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
[55847.613945] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0xfe 0x98:F5=0xe2
[55847.669345] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55847.679852] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55847.681946] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55847.741889] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55847.759750] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55847.771999] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55847.773958] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55847.830453] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55847.843243] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55847.845387] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55847.900821] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55847.912242] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55847.914561] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55847.973351] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55847.985608] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55847.987754] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55848.049328] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55848.062796] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55848.065031] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55848.127840] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55848.138984] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55848.140996] elgato_gchd 1-2:1.0: analog format status 0xfe not locked yet
[55848.197354] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
[55848.209037] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[55848.210993] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55848.210996] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55848.210998] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55848.211000] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55849.043214] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[55849.279324] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55849.294486] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55849.294701] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55849.299921] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55849.300586] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55849.301002] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55849.440416] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55854.305106] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[55859.331820] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55864.360443] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55866.695858] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26770 comm=v4l2-ctl
[55869.386431] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55874.412470] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55879.437090] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55884.462741] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55889.488510] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55892.941111] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26773 comm=v4l2-ctl
[55894.513315] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[55899.540951] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=45
[55904.174700] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175090] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175136] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175139] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175141] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175143] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175145] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175147] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175150] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175152] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175154] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175157] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175160] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175163] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175165] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175168] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175170] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175172] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175175] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175177] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175179] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175182] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175184] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175186] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175189] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175191] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175193] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175195] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175197] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175199] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175204] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175206] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175208] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175212] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175214] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175215] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175217] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175219] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175222] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175224] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175225] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175228] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175230] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175233] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175237] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175239] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175241] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175243] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175245] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175248] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175249] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175251] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175253] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175255] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175257] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175259] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175260] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175262] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175264] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175266] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175268] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175270] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175273] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175274] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175276] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175279] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175280] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175282] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175284] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175287] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175312] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175314] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175316] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175318] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175320] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175322] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175324] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175326] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175328] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175330] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175332] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175335] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175337] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175339] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175341] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175343] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175345] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175347] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175349] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175351] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175352] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175354] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175362] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175364] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175366] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175368] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175370] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175373] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175375] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175377] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175379] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175382] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55904.175869] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=1 streaming=1
[55904.565822] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1253 bytes=82116608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=55
[55905.285995] elgato_gchd 1-2:1.0: input selected: input=0; detection scheduled
[55905.286016] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=1 streaming=1
[55905.286228] elgato_gchd 1-2:1.0: input selected: input=1; detection scheduled
[55905.286260] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=1 streaming=1
[55905.296211] elgato_gchd 1-2:1.0: gchd-diag detect component: raw6665=0xb2b2 raw6867=0xb2b2
[55905.296218] elgato_gchd 1-2:1.0: no signal on input=1; retrying detection
[55905.296409] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55905.296421] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=1 streaming=1
[55905.296699] elgato_gchd 1-2:1.0: input selected: input=3; detection scheduled
[55905.297855] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.297883] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.297890] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.297929] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.297934] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.297941] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=1 streaming=1
[55905.396708] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[55905.455474] elgato_gchd 1-2:1.0: gchd-diag detect s-video: pre-config raw_status=0xed mode=0xd
[55905.457557] elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0d 0x88:CA=0x6e 0x98:F5=0x62
[55905.457560] elgato_gchd 1-2:1.0: no signal on input=3; retrying detection
[55905.457742] elgato_gchd 1-2:1.0: input selected: input=2; detection scheduled
[55905.457762] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.457790] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55905.457794] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.457797] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=1 streaming=1
[55905.457805] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26774 comm=v4l2-compliance
[55905.718648] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55905.828336] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55905.836226] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55905.838318] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55905.838321] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55905.838323] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55905.838325] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55906.042022] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55906.057768] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55906.057990] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55906.063457] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55906.063968] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55906.064417] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[55906.203640] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55911.068343] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55916.094454] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55921.121990] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55926.148125] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55931.175122] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=563 bytes=36896768 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55936.200815] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=678 bytes=44433408 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55941.227511] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=793 bytes=51970048 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55946.252847] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=908 bytes=59506688 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55951.280317] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1023 bytes=67043328 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55956.306268] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1138 bytes=74579968 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55961.331962] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=1253 bytes=82116608 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[55965.086269] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[55986.248826] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26792 comm=read_thread
[55986.248915] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[55986.360696] elgato_gchd 1-2:1.0: transcoder defaults loaded
[55986.424243] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[55986.445397] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55986.593378] elgato_gchd 1-2:1.0: back-end status 0xf2 while polling; retrigger
[55986.611028] elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x17 mode=0x7
[55986.620292] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[55986.622552] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[55986.622556] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[55986.622559] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[55986.622561] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[55987.424146] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[55987.663868] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[55987.679046] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[55987.679360] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[55987.684330] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[55987.685190] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[55987.685712] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[55987.825150] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[55992.687676] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=21
[55997.714816] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=218 bytes=14286848 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[56001.537387] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=26815 comm=v4l2-ctl
[56002.742385] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=333 bytes=21823488 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[56007.767917] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=448 bytes=29360128 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=46
[56009.666639] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
ff5@ff5-VirtualBox:~$ 
```
