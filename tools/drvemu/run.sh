#!/bin/bash
# drvemu: run the driver's own component configuration (input.c, transcoder.c,
# protocol_common.c of a checkout) in userspace and compare its register
# writes with a decoded recording.
#
#   tools/drvemu/run.sh REPO RECORDING.ops.txt FROM_S TO_S
#
# The C files are preprocessed by kbuild (kernel headers needed), compiled as
# userspace objects and linked with mock.c, which logs every mailbox write
# and answers reads with the recording's values in order (the detection
# registers 0x9C:65-68 with their last value). app.c is appended to input.i
# and calls gchd_configure_component_exact() and the encoder programming.
# FROM_S..TO_S: the part of the recording to compare (seconds), e.g. from
# the previous START to the START after a mode change.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$1 REC=$2 A=$3 B=$4
K=${KSRC:-$(ls -d /usr/src/linux-headers-*-generic | head -1)}
W=$(mktemp -d)
for f in input protocol_common transcoder; do
	make -s -C "$K" M="$REPO/src/driver" $f.i >/dev/null 2>&1
	mv "$REPO/src/driver/$f.i" "$W/"
done
rm -f "$REPO"/src/driver/.*.o.d
cat "$W/input.i" "$HERE/app.c" > "$W/input_emu.i"
for f in input_emu protocol_common transcoder; do
	gcc -c -w -O1 -fno-stack-protector -mno-red-zone -fcf-protection=none -o "$W/$f.o" "$W/$f.i"
done
gcc -w -o "$W/emu" "$HERE/mock.c" "$W"/input_emu.o "$W"/protocol_common.o "$W"/transcoder.o
awk -v a="$A" -v b="$B" '$1+0>=a && $1+0<=b' "$REC" > "$W/rec.txt"
timeout 20 "$W/emu" "$W/rec.txt" 1 > "$W/out.txt"
tail -1 "$W/out.txt"
python3 -I "$HERE/cmp.py" "$W/rec.txt" "$W/out.txt"
echo "work files: $W"
