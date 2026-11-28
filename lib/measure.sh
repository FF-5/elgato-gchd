# SPDX-License-Identifier: GPL-2.0
# shellcheck shell=bash
# Measurements on captured streams. Sourced by gchd-test.

# pic_stats FILE: the picture's average over its frames after the first
# second, from ffmpeg's signalstats: "YAVG SATAVG HUEAVG YLOW YHIGH"
# (luma average, saturation average, hue average in degrees, luma 10th
# and 90th percentile).
pic_stats() {
	# Decoded from the start (seeking into the stream loses the reference
	# pictures), the first second dropped by the filter.
	ffmpeg -v error -i "$1" -an -vf "trim=start=1,signalstats,metadata=print:file=-" \
		-frames:v 50 -f null - 2>/dev/null |
	awk -F= '
		/signalstats.YAVG=/   { y += $2; n++ }
		/signalstats.SATAVG=/ { s += $2 }
		/signalstats.HUEAVG=/ { h += $2 }
		/signalstats.YLOW=/   { lo += $2 }
		/signalstats.YHIGH=/  { hi += $2 }
		END { if (n) printf "%.1f %.1f %.1f %.1f %.1f\n", y / n, s / n, h / n, lo / n, hi / n }'
}

# pstat N "STATS": field N of pic_stats (1 YAVG, 2 SATAVG, 3 HUEAVG, 4 YLOW,
# 5 YHIGH).
pstat() { echo "$2" | awk -v n="$1" '{ print $n }'; }

# hue_diff A B: the smaller angle between two hue averages (degrees).
hue_diff() { awk -v a="$1" -v b="$2" 'BEGIN { d = a - b; if (d < 0) d = -d; d = d % 360; if (d > 180) d = 360 - d; printf "%.1f", d }'; }

# level FILE: mean audio volume in dB (ffmpeg volumedetect), after the
# first second.
level() {
	ffmpeg -hide_banner -i "$1" -map 0:a -af "atrim=start=1,volumedetect" -f null - 2>&1 |
		sed -n 's/.*mean_volume: \(-*[0-9.]*\) dB.*/\1/p'
}

# aspect FILE: the stream's display aspect ratio (e.g. 4:3, 16:9).
aspect() {
	ffprobe -v error -select_streams v:0 -show_entries stream=display_aspect_ratio \
		-of default=nw=1:nk=1 "$1" | head -1
}
