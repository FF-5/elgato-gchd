# TASK T-066 — V4L2 compliance

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented (016477b); hardware test with the test script
- **Type:** driver fixes (V4L2 interface)
- **Branch:** `fix/v4l2-compliance` (from `dev` 0.5.7)
- **TODO items:** T-066 (stage 1b, before T-060 guvcview/OBS)

## Why

Applications such as guvcview and OBS, and the planned service that feeds
them (T-060), rely on the standard V4L2 behaviour that `v4l2-compliance`
checks. Without capture, 8 failures remain (fd8426b / 4c326eb runs):

- HDMI/component: `VIDIOC_G_DV_TIMINGS` fails without a signal although
  the inputs report `V4L2_IN_CAP_DV_TIMINGS`; `VIDIOC_DV_TIMINGS_CAP` is
  missing; both also break the buffer tests (`g_dv_timings`).
- "could not set valid menu item 0" for a menu control on every input.
- warning: `V4L2_CID_DV_RX_POWER_PRESENT` missing.
- The input menu control switches inputs for any process while another
  one captures (S_INPUT returns EBUSY since fd8426b).

## Implementation

1. HDMI/component: `G/S_DV_TIMINGS`, `DV_TIMINGS_CAP`, `ENUM_DV_TIMINGS`
   with the CEA-861 presets of the supported modes; `G_DV_TIMINGS` works
   without a signal (configured or last set mode, 720p60 at first);
   `QUERY_DV_TIMINGS` as before (detected mode or `ENOLINK`).
2. The failing menu control was the H.264 level: levels 1.0–2.2 were
   offered but rejected; now skipped.
3. `V4L2_CID_DV_RX_POWER_PRESENT` (read-only).
4. The input menu control refuses an input change while capture buffers
   exist (EBUSY), as `VIDIOC_S_INPUT`.

## Hardware test (maintainer)

PS1 on composite for the last step.

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-066.sh > tests/T-066.sh
bash gchd-test.sh T-066 --branch fix/v4l2-compliance
```

Paste the summary; if the compliance step fails, the log (it lists the
`fail:` lines).

## Findings

- T-071 accepted: gchd-test.sh T-071 10/10 PASS (firmware extracted from
  the .msi equal to the previous files, DKMS install, capture with the
  installed driver, uninstall). Merged as 0.5.7.
- Maintainer (2026-11-18): the firmware script looks for the installer
  file only in `firmware/` (a path can still be given by hand); a later
  download from Elgato lands there; the installer only calls the script
  and checks its result. Done in 8ba3e1f (README, TODO T-095, test file).

## Maintainer comments

gchd-test T-066 20261118-163535 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-066-20261118-163535.log
== build
$ git fetch -q origin
$ git checkout -q fix/v4l2-compliance
$ git pull -q --ff-only
commit: 4f4b51a TASK: T-066 implementation and test; firmware folder change recorded
commit: 4f4b51a TASK: T-066 implementation and test; firmware folder change recorded
build rc=0 warnings=0

=== step 1: load module (fresh plug-in)
PROMPT: Unplug the Elgato from the PC (or detach it in VirtualBox), wait 3 s, plug it in again.
$ sudo insmod /home/ff5/elgato-gchd/src/elgato_gchd.ko
module: 0.5.7 
--- dmesg step 1
[80706.570535] usb 1-2: USB disconnect, device number 78
[80716.230953] usb 1-2: new high-speed USB device number 79 using xhci_hcd
[80716.623660] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[80716.623687] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[80716.623695] usb 1-2: Product: GameCapture HD
[80716.623700] usb 1-2: Manufacturer: Elgato
[80716.623705] usb 1-2: SerialNumber: 04107F1267
[80716.674131] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[80716.675187] elgato_gchd 1-2:1.0: GCHD lifecycle: USB present; V4L2 registered; hardware stays powered off until STREAMON
[80716.675212] usbcore: registered new interface driver elgato_gchd

=== step 2: v4l2-compliance without capture
$ sh -c v4l2-compliance -d /dev/video0 2>&1 | grep -E 'fail:|warn:|Total for'
		fail: v4l2-test-buffers.cpp(517): node->g_dv_timings(timings)
		fail: v4l2-test-buffers.cpp(642): testCanSetSameTimings(node)
		fail: v4l2-test-buffers.cpp(517): node->g_dv_timings(timings)
		fail: v4l2-test-buffers.cpp(642): testCanSetSameTimings(node)
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
		warn: v4l2-test-buffers.cpp(693): VIDIOC_CREATE_BUFS not supported
Total for elgato-gchd device /dev/video0: 115, Succeeded: 113, Failed: 2, Warnings: 2
$ v4l2-compliance -d /dev/video0
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
	test VIDIOC_ENUM/G/S/QUERY_DV_TIMINGS: OK
	test VIDIOC_DV_TIMINGS_CAP: OK
	test VIDIOC_G/S_EDID: OK (Not Supported)

Control ioctls (Input 0):
	test VIDIOC_QUERY_EXT_CTRL/QUERYMENU: OK
	test VIDIOC_QUERYCTRL: OK
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

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
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

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
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

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
	test VIDIOC_G/S_CTRL: OK
	test VIDIOC_G/S/TRY_EXT_CTRLS: OK
	test VIDIOC_(UN)SUBSCRIBE_EVENT/DQEVENT: OK
	test VIDIOC_G/S_JPEGCOMP: OK (Not Supported)
	Standard Controls: 8 Private Controls: 2

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

Total for elgato-gchd device /dev/video0: 115, Succeeded: 113, Failed: 2, Warnings: 2
[FAIL] v4l2-compliance: no failures (expected /Failed: 0/, got: v4l2-compliance 1.26.1, 64 bits, 64-bit time_t  Compliance test for elgato-gchd device /dev/video0:  Driver Info: 	Driver name      : elgato-gchd 	Card type    )
--- dmesg step 2
[80716.709584] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.709587] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.709611] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710404] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710411] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710427] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710428] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710430] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710436] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710437] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710439] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710445] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710446] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710448] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710451] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710456] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710457] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710459] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710465] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710466] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710468] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710473] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710474] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710481] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710486] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710487] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710491] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710496] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710497] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710500] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710505] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710506] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710511] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710516] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710517] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710522] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710528] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710529] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710541] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710546] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710547] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710559] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710564] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710565] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710568] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710573] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710574] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710577] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710582] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710583] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710585] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710591] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710592] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710594] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710600] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710601] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710603] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710608] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710609] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710611] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710617] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710618] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710627] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710633] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710633] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.710636] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710641] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710642] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.710644] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710650] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.710651] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.710654] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.710659] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.710660] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.710729] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.710735] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.710736] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.710742] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.710743] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.710749] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.715461] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[80716.715477] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.715509] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.715570] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[80716.715578] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.715598] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.715605] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.715624] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.715630] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.715632] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.715640] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.721610] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[80716.721627] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.721665] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.721724] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[80716.721731] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.721752] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.721758] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.721776] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.721782] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.721845] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.721910] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.728749] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.728812] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.728838] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.728839] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.728846] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.728847] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.728854] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.728855] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.728861] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.733806] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.733819] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.733836] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.733847] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.733852] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738244] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738247] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738267] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738269] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738271] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738278] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738279] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738281] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738522] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738524] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738528] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738537] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738538] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738540] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738542] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738549] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738550] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738552] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738561] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738561] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738563] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738569] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738570] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738577] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738583] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738583] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738587] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738595] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738596] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738599] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738605] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738605] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738611] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738616] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738617] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738622] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738628] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738629] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738641] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738647] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738648] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738661] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738667] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738668] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738671] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738676] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738677] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738680] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738686] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738687] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738689] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738695] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738696] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738698] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738704] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738704] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738706] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738712] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738713] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738715] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738721] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738722] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738732] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738739] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738739] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.738742] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738748] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738749] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.738751] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738757] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.738758] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.738761] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.738768] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.738769] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.738837] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.738844] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.738845] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.738851] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.738853] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.738858] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.744593] elgato_gchd 1-2:1.0: input switch requested: old=3 new=1 hw_initialized=0 streaming=0
[80716.744612] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.744614] elgato_gchd 1-2:1.0: input switch requested: old=1 new=1 hw_initialized=0 streaming=0
[80716.744676] elgato_gchd 1-2:1.0: input switch requested: old=1 new=0 hw_initialized=0 streaming=0
[80716.744683] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.744684] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.744690] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.744692] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.744698] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.744699] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.744705] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.748563] elgato_gchd 1-2:1.0: input switch requested: old=3 new=2 hw_initialized=0 streaming=0
[80716.748583] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.748585] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80716.748651] elgato_gchd 1-2:1.0: input switch requested: old=2 new=0 hw_initialized=0 streaming=0
[80716.748658] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.748659] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.748665] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.748666] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.748672] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.748674] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.748680] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.753436] elgato_gchd 1-2:1.0: input switch requested: old=3 new=3 hw_initialized=0 streaming=0
[80716.753493] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.753508] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.753510] elgato_gchd 1-2:1.0: input switch requested: old=0 new=1 hw_initialized=0 streaming=0
[80716.753515] elgato_gchd 1-2:1.0: input selected: input=1; detection deferred until device open
[80716.753517] elgato_gchd 1-2:1.0: input switch requested: old=1 new=2 hw_initialized=0 streaming=0
[80716.753522] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80716.753523] elgato_gchd 1-2:1.0: input switch requested: old=2 new=3 hw_initialized=0 streaming=0
[80716.753530] elgato_gchd 1-2:1.0: input selected: input=3; detection deferred until device open
[80716.758484] elgato_gchd 1-2:1.0: input switch requested: old=3 new=0 hw_initialized=0 streaming=0
[80716.758501] elgato_gchd 1-2:1.0: input selected: input=0; detection deferred until device open
[80716.758511] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.758520] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0
[80716.758525] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0

=== step 3: DV timings on HDMI (no signal needed)
$ v4l2-ctl -d /dev/video0 -i 0
Video input set to 0 (HDMI: Camera, no signal)
$ v4l2-ctl -d /dev/video0 --get-dv-timings
DV timings:
	Active width: 1280
	Active height: 720
	Total width: 1650
	Total height: 750
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 74250000 Hz (60.00 frames per second)
	Horizontal frontporch: 110
	Horizontal sync: 40
	Horizontal backporch: 220
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 20
	Standards: CTA-861
	CTA-861 VIC: 4
	Flags: framerate can be reduced by 1/1.001, CE-video, has CTA-861 VIC
[PASS] G_DV_TIMINGS works without a signal
$ v4l2-ctl -d /dev/video0 --list-dv-timings
ioctl: VIDIOC_ENUM_DV_TIMINGS
	Index: 0
	Active width: 1920
	Active height: 1080
	Total width: 2200
	Total height: 1125
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 148500000 Hz (60.00 frames per second)
	Horizontal frontporch: 88
	Horizontal sync: 44
	Horizontal backporch: 148
	Vertical frontporch: 4
	Vertical sync: 5
	Vertical backporch: 36
	Standards: CTA-861, DMT
	CTA-861 VIC: 16
	Flags: framerate can be reduced by 1/1.001, CE-video, has CTA-861 VIC

	Index: 1
	Active width: 1920
	Active height: 1080
	Total width: 2200
	Total height: 1125
	Frame format: interlaced
	Polarities: +vsync +hsync
	Pixelclock: 74250000 Hz (60.00 fields per second)
	Horizontal frontporch: 88
	Horizontal sync: 44
	Horizontal backporch: 148
	Field 1:
	Vertical frontporch: 2
	Vertical sync: 5
	Vertical backporch: 15
	Field 2:
	Vertical frontporch: 2
	Vertical sync: 5
	Vertical backporch: 16
	Standards: CTA-861
	CTA-861 VIC: 5
	Flags: framerate can be reduced by 1/1.001, half-line, CE-video, has CTA-861 VIC

	Index: 2
	Active width: 1280
	Active height: 720
	Total width: 1650
	Total height: 750
	Frame format: progressive
	Polarities: +vsync +hsync
	Pixelclock: 74250000 Hz (60.00 frames per second)
	Horizontal frontporch: 110
	Horizontal sync: 40
	Horizontal backporch: 220
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 20
	Standards: CTA-861
	CTA-861 VIC: 4
	Flags: framerate can be reduced by 1/1.001, CE-video, has CTA-861 VIC

	Index: 3
	Active width: 720
	Active height: 576
	Total width: 864
	Total height: 625
	Frame format: progressive
	Polarities: -vsync -hsync
	Pixelclock: 27000000 Hz (50.00 frames per second)
	Horizontal frontporch: 12
	Horizontal sync: 64
	Horizontal backporch: 68
	Vertical frontporch: 5
	Vertical sync: 5
	Vertical backporch: 39
	Standards: CTA-861
	Picture aspect: 4:3
	CTA-861 VIC: 17
	Flags: CE-video, has picture aspect, has CTA-861 VIC

	Index: 4
	Active width: 720
	Active height: 480
	Total width: 858
	Total height: 525
	Frame format: progressive
	Polarities: -vsync -hsync
	Pixelclock: 27000000 Hz (59.94 frames per second)
	Horizontal frontporch: 16
	Horizontal sync: 62
	Horizontal backporch: 60
	Vertical frontporch: 9
	Vertical sync: 6
	Vertical backporch: 30
	Standards: CTA-861
	Picture aspect: 4:3
	CTA-861 VIC: 2
	Flags: CE-video, has picture aspect, has CTA-861 VIC
[FAIL] DV timings list (expected /1920x1080/, got: ioctl: VIDIOC_ENUM_DV_TIMINGS 	Index: 0 	Active width: 1920 	Active height: 1080 	Total width: 2200 	Total height: 1125 	Frame format: progressive 	Polarities: )
$ v4l2-ctl -d /dev/video0 -l

User Controls

                   input_source 0x009819f0 (menu)   : min=0 max=3 default=0 value=0 (HDMI)
               stretch_sd_input 0x009819f1 (bool)   : default=0 value=0

Codec Controls

             video_bitrate_mode 0x009909ce (menu)   : min=0 max=1 default=1 value=1 (Constant Bitrate) flags=update
                  video_bitrate 0x009909cf (int)    : min=32000 max=40000000 step=1000 default=11250000 value=11250000
                     h264_level 0x00990a67 (menu)   : min=0 max=15 default=12 value=12 (4.1)
                   h264_profile 0x00990a6b (menu)   : min=0 max=4 default=4 value=4 (High)

Digital Video Controls

                  power_present 0x00a00964 (bitmask): max=0x00000001 default=0x00000000 value=0 flags=read-only, volatile
[PASS] power present 0 without a signal
--- dmesg step 3
[80716.805648] elgato_gchd 1-2:1.0: input switch requested: old=0 new=0 hw_initialized=0 streaming=0

=== step 4: composite capture, input control refused while capturing
PROMPT: PS1 on composite, game running.
$ v4l2-ctl -d /dev/video0 -i 2
Video input set to 2 (Composite: Camera, no signal)
$ v4l2-ctl -d /dev/video0 -v width=720,height=576,pixelformat=MPEG
capturing 10s to step4-18249.ts
during capture: v4l2-ctl -d /dev/video0 -c input_source=0; v4l2-ctl -d /dev/video0 --get-input
input_source: Device or resource busy
VIDIOC_S_EXT_CTRLS: failed: Device or resource busy
Video input : 2 (Composite: ok)
capture: 11710464 bytes, ~9368 kbit/s; codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; decode errors 1
  codec_name=h264 width=720 height=576 field_order=tt r_frame_rate=50/1 ; ~9368 kbit/s; decode errors 1
audio: mp2
[PASS] stream 720x576 50/1 tt, ~9368 kbit/s
$ sh -c grep -A2 'during capture' '/home/ff5/gchd-test-logs/T-066-20261118-163535.log' | tail -3
VIDIOC_S_EXT_CTRLS: failed: Device or resource busy
--
$ sh -c grep -A2 'during capture' '/home/ff5/gchd-test-logs/T-066-20261118-163535.log' | tail -3
$ v4l2-ctl -d /dev/video0 --get-input
Video input : 2 (Composite: no signal)
[PASS] input still composite after the refused change
--- dmesg step 4
[80801.679684] elgato_gchd 1-2:1.0: input switch requested: old=0 new=2 hw_initialized=0 streaming=0
[80801.679785] elgato_gchd 1-2:1.0: input selected: input=2; detection deferred until device open
[80801.679793] elgato_gchd 1-2:1.0: input switch requested: old=2 new=2 hw_initialized=0 streaming=0
[80801.695270] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[80801.860604] elgato_gchd 1-2:1.0: transcoder defaults loaded
[80801.920952] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[80802.332614] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x17 masked_mode=0x7
[80802.334729] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[80802.334732] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[80802.334734] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[80802.334736] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[80803.112119] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[80803.349224] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[80803.364323] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[80803.364525] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[80803.370024] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[80803.370599] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[80803.371070] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0xd39e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0xd39e hw_state=0xd39e hw_enable=0x030e
[80803.490257] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[80808.399291] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=104 bytes=6815744 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[80812.742697] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== summary (T-066, 4f4b51a TASK: T-066 implementation and test; firmware folder change recorded)
FAIL | step 2 v4l2-compliance without capture | v4l2-compliance: no failures (expected /Failed: 0/, got: v4l2-compliance 1.26.1, 64 bits, 64-bit time_t  Compliance test for elgato-gchd device /dev/video0:  Driver Info: 	Driver name      : elgato-gchd 	Card type    )
PASS | step 3 DV timings on HDMI (no signal needed) | G_DV_TIMINGS works without a signal
FAIL | step 3 DV timings on HDMI (no signal needed) | DV timings list (expected /1920x1080/, got: ioctl: VIDIOC_ENUM_DV_TIMINGS 	Index: 0 	Active width: 1920 	Active height: 1080 	Total width: 2200 	Total height: 1125 	Frame format: progressive 	Polarities: )
PASS | step 3 DV timings on HDMI (no signal needed) | power present 0 without a signal
PASS | step 4 composite capture, input control refused while capturing | stream 720x576 50/1 tt, ~9368 kbit/s
PASS | step 4 composite capture, input control refused while capturing | input still composite after the refused change
