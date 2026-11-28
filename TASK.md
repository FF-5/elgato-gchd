# TASK T-103 — Release logging

**Development-only file** (never merged to `master`, RULES B2).

- **Status:** implemented; hardware test next
- **Type:** driver (stage 5), before T-102
- **Branch:** `feat/release-logging` (from `dev` 0.5.15)
- **TODO items:** T-103 (stage 5)

## Why

The driver logged every step of a capture start to the kernel log (65
informational messages): useful for troubleshooting and for the tests,
too much for users. One build for everybody, the details behind a switch
(maintainer, 2026-11-28; T-102 decision).

## Goal (acceptance criteria)

1. Always logged: warnings, errors, and the main events: the device ready
   (with its video device), the source detected with its mode, a lost
   signal, mode changes while capturing.
2. Everything else only with the `debug` module parameter (default off,
   changeable while the driver is loaded); `timing` and `trace_mailbox`
   stay separate switches; per-buffer messages stay `dev_dbg` (the
   kernel's dynamic debug).
3. The test runner switches `debug` on for its tests and off at the end.
4. Documented: parameter table, troubleshooting in the README.

## Done so far

- `gchd_dbg()` / `gchd_dbg_ratelimited()` (`elgato_gchd.h`): `dev_info`
  while `debug` is on. 59 messages moved to them; kept as `dev_info`:
  "signal detected", "analog mode now …", "… output now …", "input N:
  signal lost; detecting again" (was `pr_info`), "Elgato Game Capture HD
  ready as /dev/videoN" (was the probe's lifecycle line).
- Build without warnings; `debug` in the module's parameters.
- `tools/gchd-test.sh`: `debug_on` at each step, after prompts and
  commands (a reinstall or replug loads the module again), `debug_off` at
  the end and on Ctrl-C.
- Driver README (parameter table), README (Troubleshooting).

## Findings

## Hardware test (maintainer)

```sh
cd ~ && git -C elgato-gchd fetch origin
git -C elgato-gchd show origin/tools:tools/gchd-test.sh > gchd-test.sh
mkdir -p tests && git -C elgato-gchd show origin/tools:tools/tests/T-103.sh > tests/T-103.sh
bash gchd-test.sh T-103 --branch feat/release-logging
```

Needs the PS3 on composite. Note: `gchd-test.sh` changed (it switches
`debug` on); fetch it again as above.

## Maintainer comments

gchd-test T-103 20261128-031722 host 6.8.0-146-generic
log: /home/ff5/gchd-test-logs/T-103-20261128-031722.log
== build
$ git fetch -q origin
$ git checkout -q feat/release-logging
$ git pull -q --ff-only
commit: 5a2330d TODO: T-103 (from feat/test-suite), in work
commit: 5a2330d TODO: T-103 (from feat/test-suite), in work
build rc=0 warnings=0

=== step 1: install
$ sudo rmmod elgato_gchd
$ bash /home/ff5/elgato-gchd/install-driver.sh --core --yes --in-terminal
[1mElgato Game Capture HD driver 0.5.15[0m

[1m==> 1/3 Firmware[0m
[32mFirmware found in /lib/firmware/gchd.[0m

[1m==> 2/3 Building the driver[0m
[32mBuilt /home/ff5/elgato-gchd/build/elgato_gchd.ko.[0m

[1m==> 3/3 Installing[0m
Sign command: /usr/bin/kmodsign
Signing key: /var/lib/shim-signed/mok/MOK.priv
Public certificate (MOK): /var/lib/shim-signed/mok/MOK.der

Building module:
Cleaning build area...
make -j4 KERNELRELEASE=6.8.0-146-generic KDIR=/lib/modules/6.8.0-146-generic/build...
Signing module /var/lib/dkms/elgato-gchd/0.5.15/build/elgato_gchd.ko
Cleaning build area...

elgato_gchd.ko.zst:
Running module version sanity check.
 - Original module
   - This kernel never originally had a module by this name
 - Installation
   - Installing to /lib/modules/6.8.0-146-generic/updates/dkms/
depmod...
[32mInstalled with DKMS (rebuilt automatically after kernel updates).[0m
[32mudev rules installed (settings are remembered; device name /dev/elgato-gchd).[0m
[32mProfile command installed: gchd-profile (named sets of all settings).[0m
[32mDriver loaded. Plug in the Game Capture HD; it appears as a /dev/video device.[0m
Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd

PROMPT: PS3 on composite. Unplug the Elgato and plug it in again; wait 5 s.
$ sh -c modinfo -p elgato_gchd | grep '^debug'
debug:Log detailed messages for troubleshooting (0 off, 1 on) (bool)
[PASS] debug parameter exists
$ sh -c sudo modprobe -r elgato_gchd && sudo modprobe elgato_gchd && sleep 3 && cat /sys/module/elgato_gchd/parameters/debug
N
[PASS] debug off after loading
$ sh -c sudo dmesg | grep elgato_gchd
[24326.860178] usbcore: deregistering interface driver elgato_gchd
[24335.759797] usbcore: registered new interface driver elgato_gchd
[24357.338905] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[24357.340130] elgato_gchd 1-2:1.0: Elgato Game Capture HD ready as video0
[24357.356046] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=39797 comm=v4l_id
[24357.357311] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1373 comm=wireplumber
[24357.357568] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1373 comm=wireplumber
[24357.358230] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.358242] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.358495] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.358521] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.358531] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.358540] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.359237] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24357.359266] elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=1370 comm=pipewire
[24361.075235] usbcore: deregistering interface driver elgato_gchd
[24361.158062] elgato_gchd 1-2:1.0: Elgato Game Capture HD ready as video0
[24361.158082] usbcore: registered new interface driver elgato_gchd
[FAIL] device ready message (expected /Elgato Game Capture HD ready as /dev/video/, got: [24326.860178] usbcore: deregistering interface driver elgato_gchd [24335.759797] usbcore: registered new interface driver elgato_gchd [24357.338905] elgato_gch)
--- dmesg step 1
[24326.860178] usbcore: deregistering interface driver elgato_gchd
[24335.759797] usbcore: registered new interface driver elgato_gchd
[24356.936446] usb 1-2: new high-speed USB device number 14 using xhci_hcd
[24357.333054] usb 1-2: New USB device found, idVendor=0fd9, idProduct=005d, bcdDevice= 2.00
[24357.333081] usb 1-2: New USB device strings: Mfr=1, Product=2, SerialNumber=3
[24357.333089] usb 1-2: Product: GameCapture HD
[24357.333095] usb 1-2: Manufacturer: Elgato
[24357.333100] usb 1-2: SerialNumber: 04107F1267
[24357.338905] elgato_gchd 1-2:1.0: GCHD lifecycle: USB probe; registering V4L2 only\n
[24357.340130] elgato_gchd 1-2:1.0: Elgato Game Capture HD ready as video0
[24361.075235] usbcore: deregistering interface driver elgato_gchd
[24361.158062] elgato_gchd 1-2:1.0: Elgato Game Capture HD ready as video0
[24361.158082] usbcore: registered new interface driver elgato_gchd

=== step 2: quiet capture (debug off)
$ kdmesg
elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[PASS] source detected logged
$ sh -c sudo dmesg | grep elgato_gchd | grep -cE 'capture setup|gchd-diag|bulk-IN|lifecycle|input switch|transcoder defaults|START'
0
[PASS] no detailed messages
$ sh -c sudo dmesg | grep elgato_gchd | sed 's/^\[[^]]*\] //'
elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[PASS] The quiet log above: only a few lines (source detected and perhaps a mode change), no step-by-step messages?
--- dmesg step 2
[24364.975466] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1

=== step 3: detailed capture (debug switched on while loaded)
$ kdmesg
elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=39934 comm=bash
elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
elgato_gchd 1-2:1.0: transcoder defaults loaded
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x7e 0x98:F5=0x62
elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: capture setup: encoder start completed
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: capture setup: input configuration completed
elgato_gchd 1-2:1.0: capture setup: encoder programming completed
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=439 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
elgato_gchd 1-2:1.0: gchd-diag START transition completed
elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] detailed messages again
$ kdmesg
elgato_gchd 1-2:1.0: GCHD lifecycle: V4L2 open requested by pid=39934 comm=bash
elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
elgato_gchd 1-2:1.0: transcoder defaults loaded
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x04
elgato_gchd 1-2:1.0: firmware chunk offset=0 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=32768 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=65536 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=98304 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=131072 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=163840 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=196608 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=229376 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=262144 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=294912 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=327680 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=360448 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=393216 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=425984 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=458752 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=491520 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=524288 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=557056 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=589824 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=622592 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=655360 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=688128 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=720896 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=753664 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=786432 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=819200 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=851968 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=884736 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=917504 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=950272 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=983040 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1015808 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1048576 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1081344 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1114112 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1146880 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1179648 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1212416 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1245184 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1277952 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1310720 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1343488 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1376256 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1409024 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1441792 size=32768 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: firmware chunk offset=1474560 size=26224 OUT ep 0x02 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x1f mode=0xf
elgato_gchd 1-2:1.0: gchd-diag no analog family: 0x88:FA=0x0f 0x88:CA=0x7e 0x98:F5=0x62
elgato_gchd 1-2:1.0: no signal on input=2; retrying detection
elgato_gchd 1-2:1.0: gchd-diag detect composite: pre-config raw_status=0x07 mode=0x7
elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: capture setup: encoder start completed
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: capture setup: input configuration completed
elgato_gchd 1-2:1.0: capture setup: encoder programming completed
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 02
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
elgato_gchd 1-2:1.0: START poll=0 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=1 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=2 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=3 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=4 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=5 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=6 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=7 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=8 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=9 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=10 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=11 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=12 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=13 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=14 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=15 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=100 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=200 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=300 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=400 state=0x0001 completion(0900:0074)=0x011a clear_index(0900:01b0)=0x0150 ack=0
elgato_gchd 1-2:1.0: START poll=439 state=0x0002 completion(0900:0074)=0x011e clear_index(0900:01b0)=0x0150 ack=1
elgato_gchd 1-2:1.0: gchd-diag START transition completed
elgato_gchd 1-2:1.0: bulk-IN first reads: read=1 ep=0x81 actual=65536 total_bytes=65536
elgato_gchd 1-2:1.0: bulk-IN first reads: read=2 ep=0x81 actual=65536 total_bytes=131072
elgato_gchd 1-2:1.0: bulk-IN first reads: read=3 ep=0x81 actual=65536 total_bytes=196608
elgato_gchd 1-2:1.0: bulk-IN first reads: read=4 ep=0x81 actual=65536 total_bytes=262144
elgato_gchd 1-2:1.0: bulk-IN first reads: read=5 ep=0x81 actual=65536 total_bytes=327680
elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 04
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=05 00 00 01
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x05
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=04 a0 00 00
elgato_gchd 1-2:1.0: SCMD write succeeded: req=0xb8 value=0x0000 index=0x0000 payload=01 00 00 00
elgato_gchd 1-2:1.0: SCMD interrupt completed: command=0x01
elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)
[PASS] transfer statistics
--- dmesg step 3
[24462.004118] elgato_gchd 1-2:1.0: GCHD lifecycle: starting hardware boot/init from V4L2 STREAMON
[24462.068294] elgato_gchd 1-2:1.0: transcoder defaults loaded
[24462.135080] elgato_gchd 1-2:1.0: device idle, processor state 0x27f97b
[24462.235968] elgato_gchd 1-2:1.0: back-end status 0xe2 while polling; retrigger
[24462.379778] elgato_gchd 1-2:1.0: gchd-diag detect composite: raw_status=0x07 masked_mode=0x7
[24462.383384] elgato_gchd 1-2:1.0: analog format initial status=0x7e; configuring 576i
[24462.383389] elgato_gchd 1-2:1.0: gchd-diag detected input=2 mode=720x576 fps=50/1 interlaced=1 signal_present=1
[24462.383394] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1
[24462.383398] elgato_gchd 1-2:1.0: capture setup: input=2 mode=720x576 fps=50/1 interlaced=1
[24463.175024] elgato_gchd 1-2:1.0: capture setup: encoder start completed
[24463.453974] elgato_gchd 1-2:1.0: capture setup: input configuration completed
[24463.476390] elgato_gchd 1-2:1.0: capture setup: encoder programming completed
[24463.476728] elgato_gchd 1-2:1.0: capture setup: state after final SCMD_INIT=0x0001
[24463.482710] elgato_gchd 1-2:1.0: async bulk-IN starting: ep=0x81 URBs=32 maxpacket=512; ep=0x84 URBs=8 maxpacket=512 speed=3 alt=0
[24463.483322] elgato_gchd 1-2:1.0: gchd-diag issuing START: input=2 mode=720x576 fps=50/1 interlaced=1
[24463.483862] elgato_gchd 1-2:1.0: START before SCMD: state=0x0001 enable_state=0x031e (r=0) enable=0x030e (r=0) completion=0x011e (r=0) saved=0x031e hw_state=0x031e hw_enable=0x030e
[24463.622342] elgato_gchd 1-2:1.0: gchd-diag START transition completed
[24468.494134] elgato_gchd 1-2:1.0: async bulk-IN stats: reads=103 bytes=6750208 ep84_bytes=0 empty=0 errors=0 urb_errors=0 last_status=0 queued=0 start_pending=0 ring_dropped=0 frames_waiting=0
[24470.834140] elgato_gchd 1-2:1.0: hardware shutdown done (state 0x0001 before, 0x0012 now)

=== step 4: switched off again
$ sh -c sudo dmesg | grep elgato_gchd | grep -cE 'capture setup|gchd-diag|bulk-IN'
0
[PASS] quiet again
--- dmesg step 4
[24472.658788] elgato_gchd 1-2:1.0: signal detected: input=2 mode=720x576 fps=50/1 interlaced=1

=== summary (T-103, 5a2330d TODO: T-103 (from feat/test-suite), in work)
PASS | step 1 install | debug parameter exists
PASS | step 1 install | debug off after loading
FAIL | step 1 install | device ready message (expected /Elgato Game Capture HD ready as /dev/video/, got: [24326.860178] usbcore: deregistering interface driver elgato_gchd [24335.759797] usbcore: registered new interface driver elgato_gchd [24357.338905] elgato_gch)
PASS | step 2 quiet capture (debug off) | source detected logged
PASS | step 2 quiet capture (debug off) | no detailed messages
PASS | step 2 quiet capture (debug off) | The quiet log above: only a few lines (source detected and perhaps a mode change), no step-by-step messages?
PASS | step 3 detailed capture (debug switched on while loaded) | detailed messages again
PASS | step 3 detailed capture (debug switched on while loaded) | transfer statistics
PASS | step 4 switched off again | quiet again
