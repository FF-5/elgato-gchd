#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Install the elgato_gchd driver for the Elgato Game Capture HD.
#
#   ./install-driver.sh              install (or update)
#   ./install-driver.sh --uninstall  remove the driver and its udev rule
#   ./install-driver.sh --yes        do not ask before installing packages
#
# Steps: 1. firmware present? If not, extract it with firmware/ (stop when
# that fails). 2. build the module in build/. 3. install it (DKMS when
# available), the udev rule for remembered settings, and load it.
# Can be started from a file manager: it opens a terminal window.

set -u

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
NAME=elgato-gchd
MODULE=elgato_gchd
VERSION=$(sed -n 's/^PACKAGE_VERSION="\(.*\)"/\1/p' "$HERE/src/driver/dkms.conf")
FIRMWARE_DIR=/lib/firmware/gchd
FIRMWARE_FILES="MB86M01_ASSP_NSEC_IDLE MB86M01_ASSP_NSEC_ENC_H MB86H57_H58_IDLE MB86H57_H58_ENC_H"
UDEV_RULE=56-elgato-gchd-settings.rules
UDEV_DEVICE_RULE=55-elgato-gchd-device.rules
SETTINGS=/etc/modprobe.d/elgato-gchd-settings.conf
BUILD=$HERE/build
OBJ=$BUILD/objects/kernel
KDIR=${KDIR:-/lib/modules/$(uname -r)/build}
ASSUME_YES=0
ACTION=install

for a in "$@"; do
	case "$a" in
	--uninstall) ACTION=uninstall ;;
	--yes|-y) ASSUME_YES=1 ;;
	--in-terminal) ;;
	-h|--help) sed -n '3,13p' "$0"; exit 0 ;;
	*) echo "unknown option: $a"; exit 2 ;;
	esac
done

# Started from a file manager (no terminal): reopen in a terminal window.
if [ ! -t 0 ] && [[ " $* " != *" --in-terminal "* ]]; then
	for t in x-terminal-emulator gnome-terminal konsole xfce4-terminal xterm; do
		if command -v "$t" >/dev/null 2>&1; then
			case "$t" in
			gnome-terminal) exec "$t" -- bash "$0" "$@" --in-terminal ;;
			*) exec "$t" -e bash "$0" "$@" --in-terminal ;;
			esac
		fi
	done
	exit 1
fi

if [[ " $* " == *" --in-terminal "* ]]; then
	trap 'echo; read -r -p "Press Enter to close this window." _' EXIT
fi

B=$'\e[1m'; R=$'\e[31m'; G=$'\e[32m'; N=$'\e[0m'
step() { printf '\n%s==> %s%s\n' "$B" "$*" "$N"; }
ok()   { printf '%s%s%s\n' "$G" "$*" "$N"; }
die()  { printf '%sError: %s%s\n' "$R" "$*" "$N" >&2; exit 1; }
ask()  { [ $ASSUME_YES = 1 ] && return 0; read -r -p "$* [y/N] " a; [[ "$a" == [yYjJ]* ]]; }

# Package names per package manager: install command, headers, dkms, 7z.
pkg_cmd() {
	if command -v apt-get >/dev/null; then echo "apt-get install -y"
	elif command -v dnf >/dev/null; then echo "dnf install -y"
	elif command -v pacman >/dev/null; then echo "pacman -S --needed --noconfirm"
	elif command -v zypper >/dev/null; then echo "zypper install -y"
	fi
}
pkg_name() { # what -> package name for this distribution
	local pm
	pm=$(pkg_cmd | cut -d' ' -f1)
	case "$1:$pm" in
	headers:apt-get) echo "linux-headers-$(uname -r)" ;;
	headers:dnf) echo "kernel-devel-$(uname -r)" ;;
	headers:pacman) echo "linux-headers" ;;
	headers:zypper) echo "kernel-default-devel" ;;
	7z:apt-get) echo "p7zip-full" ;;
	7z:pacman) echo "7zip" ;;
	7z:*) echo "p7zip" ;;
	build:pacman) echo "base-devel" ;;
	build:*) echo "make gcc" ;;
	dkms:*) echo "dkms" ;;
	esac
}
need() { # need WHAT TEST-COMMAND...: offer to install a missing package
	local what=$1 pkgs cmd
	shift
	"$@" >/dev/null 2>&1 && return 0
	pkgs=$(pkg_name "$what")
	cmd=$(pkg_cmd)
	[ -n "$cmd" ] && [ -n "$pkgs" ] || die "missing: $what. Install it with your package manager and start again."
	echo "Missing: $what (package: $pkgs)."
	ask "Install it now with: sudo $cmd $pkgs ?" || die "missing: $what."
	# shellcheck disable=SC2086
	sudo $cmd $pkgs || die "installing $pkgs failed."
	"$@" >/dev/null 2>&1 || die "$what still missing after installing $pkgs."
}

uninstall() {
	step "Removing the driver"
	sudo modprobe -r "$MODULE" 2>/dev/null
	if command -v dkms >/dev/null && dkms status "$NAME" 2>/dev/null | grep -q .; then
		sudo dkms remove "$NAME/$VERSION" --all || true
		sudo rm -rf "/usr/src/$NAME-$VERSION"
	fi
	sudo rm -f "/lib/modules/$(uname -r)/updates/$MODULE.ko"
	sudo depmod -a
	sudo rm -f "/etc/udev/rules.d/$UDEV_RULE" "/etc/udev/rules.d/$UDEV_DEVICE_RULE" "$SETTINGS"
	sudo udevadm control --reload 2>/dev/null
	ok "Driver removed. The firmware in $FIRMWARE_DIR was kept."
}

firmware_present() {
	local f
	for f in $FIRMWARE_FILES; do
		[ -s "$FIRMWARE_DIR/$f" ] || return 1
	done
}

install() {
	echo "${B}Elgato Game Capture HD driver $VERSION${N}"
	sudo -v || die "administrator rights (sudo) are needed."

	step "1/3 Firmware"
	if firmware_present; then
		ok "Firmware found in $FIRMWARE_DIR."
	else
		echo "Firmware not found in $FIRMWARE_DIR; extracting it."
		need 7z command -v 7z
		bash "$HERE/firmware/extract_firmware_windows" ||
			die "the firmware could not be installed; the driver was not installed."
		firmware_present || die "firmware still incomplete in $FIRMWARE_DIR."
		ok "Firmware installed."
	fi

	step "2/3 Building the driver"
	need build command -v make
	need build command -v gcc
	need headers test -d "$KDIR"
	rm -rf "$OBJ"
	mkdir -p "$OBJ" || die "cannot create $OBJ"
	cp -r "$HERE/src/driver/." "$OBJ/"
	make -C "$KDIR" M="$OBJ" clean >/dev/null 2>&1
	make -C "$KDIR" M="$OBJ" modules > "$BUILD/build.log" 2>&1 ||
		die "build failed; see $BUILD/build.log"
	cp "$OBJ/$MODULE.ko" "$BUILD/" || die "build produced no $MODULE.ko"
	ok "Built $BUILD/$MODULE.ko."

	step "3/3 Installing"
	sudo modprobe -r "$MODULE" 2>/dev/null
	if ! command -v dkms >/dev/null; then
		cmd=$(pkg_cmd)
		if [ -n "$cmd" ] && ask "DKMS is not installed. It rebuilds the driver after kernel updates. Install it now with: sudo $cmd dkms ?"; then
			# shellcheck disable=SC2086
			sudo $cmd dkms || echo "Installing dkms failed; installing without it."
		fi
	fi
	if command -v dkms >/dev/null; then
		if dkms status "$NAME/$VERSION" 2>/dev/null | grep -q .; then
			sudo dkms remove "$NAME/$VERSION" --all >/dev/null 2>&1
		fi
		sudo rm -rf "/usr/src/$NAME-$VERSION"
		sudo cp -r "$OBJ" "/usr/src/$NAME-$VERSION" || die "cannot copy the sources to /usr/src."
		sudo make -C "$KDIR" M="/usr/src/$NAME-$VERSION" clean >/dev/null 2>&1
		sudo dkms add "$NAME/$VERSION" >/dev/null || die "dkms add failed."
		sudo dkms install "$NAME/$VERSION" || die "dkms install failed."
		ok "Installed with DKMS (rebuilt automatically after kernel updates)."
	else
		sudo install -D -m 0644 "$BUILD/$MODULE.ko" "/lib/modules/$(uname -r)/updates/$MODULE.ko" ||
			die "cannot install the module."
		sudo depmod -a
		ok "Installed without DKMS: run this installer again after a kernel update."
	fi
	sudo install -D -m 0644 "$HERE/udev-rules/$UDEV_RULE" "/etc/udev/rules.d/$UDEV_RULE"
	sudo install -D -m 0644 "$HERE/udev-rules/$UDEV_DEVICE_RULE" "/etc/udev/rules.d/$UDEV_DEVICE_RULE"
	sudo udevadm control --reload
	ok "udev rules installed (settings are remembered; device name /dev/elgato-gchd)."
	sudo modprobe "$MODULE" || die "the driver could not be loaded (see: sudo dmesg)."
	ok "Driver loaded. Plug in the Game Capture HD; it appears as a /dev/video device."
	echo "Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd"
}

case $ACTION in
install) install ;;
uninstall) uninstall ;;
esac
