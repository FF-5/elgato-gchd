#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
#
# Install the Elgato Game Capture HD support. The core (kernel driver) is
# always installed; the settings programs and the OBS plugin are optional.
#
#   ./install-driver.sh              install (or update) the core; asks for
#                                    each optional part
#   ./install-driver.sh --core       the core only
#   ./install-driver.sh --settings   the core and the settings programs
#   ./install-driver.sh --obs        the core and the OBS plugin
#   ./install-driver.sh --all        the core and both optional parts
#                                    (--settings --obs is the same)
#   ./install-driver.sh --uninstall  remove everything, the firmware after
#                                    asking (with --settings or --obs: only
#                                    that optional part)
#   ./install-driver.sh --yes        do not ask (installs everything and the
#                                    packages it needs)
#
# Core: 1. firmware present? If not, extract it with firmware/ (stop when
# that fails). 2. build the module in build/. 3. install it (DKMS when
# available), the udev rules (remembered settings, device name
# /dev/elgato-gchd) and the profile command gchd-profile (used by the
# settings programs and the OBS plugin), and load it.
# Settings programs: the settings window gchd-settings-gui (application
# menu) and the text menu gchd-settings.
# OBS plugin: build it in build/obs-plugin and install it for the current
# user (~/.config/obs-studio/plugins).
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
PROFILE_TOOL=/usr/local/bin/gchd-profile
SETTINGS_TOOL=/usr/local/bin/gchd-settings
SETTINGS_GUI=/usr/local/bin/gchd-settings-gui
SETTINGS_DESKTOP=/usr/local/share/applications/elgato-gchd-settings.desktop
OBS_BUILD=$BUILD/obs-plugin
OBS_PLUGIN_DIR=$HOME/.config/obs-studio/plugins/obs-elgato-gchd
ASSUME_YES=0
ACTION=install
CHOSEN=0	# an option named the parts
WITH_SETTINGS=0
WITH_OBS=0

for a in "$@"; do
	case "$a" in
	--uninstall) ACTION=uninstall ;;
	--all) CHOSEN=1; WITH_SETTINGS=1; WITH_OBS=1 ;;
	--core|--driver) CHOSEN=1 ;;
	--settings) CHOSEN=1; WITH_SETTINGS=1 ;;
	--obs) CHOSEN=1; WITH_OBS=1 ;;
	--yes|-y) ASSUME_YES=1 ;;
	--in-terminal) ;;
	-h|--help) sed -n '3,27p' "$0"; exit 0 ;;
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
# The question is printed (not read -p): it also shows when the answers
# come from a pipe, and the answer is echoed then.
ask()  { [ $ASSUME_YES = 1 ] && return 0; printf '%s [y/N] ' "$*"; read -r a; [ -t 0 ] || echo "$a"; [[ "$a" == [yYjJ]* ]]; }
# ask_yes QUESTION: yes unless answered no (Enter = yes).
ask_yes() { [ $ASSUME_YES = 1 ] && return 0; printf '%s [Y/n] ' "$*"; read -r a; [ -t 0 ] || echo "$a"; [[ "$a" != [nN]* ]]; }

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
	cxx:apt-get) echo "g++" ;;
	cxx:pacman) echo "base-devel" ;;
	cxx:*) echo "gcc-c++" ;;
	qtdev:apt-get) echo "qt6-base-dev" ;;
	qtdev:dnf) echo "qt6-qtbase-devel" ;;
	qtdev:pacman) echo "qt6-base" ;;
	qtdev:*) echo "qt6-base-devel" ;;
	obsdev:apt-get) echo "libobs-dev" ;;
	obsdev:pacman) echo "obs-studio" ;;
	obsdev:*) echo "obs-studio-devel" ;;
	ffmpegdev:apt-get) echo "libavformat-dev libavcodec-dev libavutil-dev" ;;
	ffmpegdev:pacman) echo "ffmpeg" ;;
	ffmpegdev:dnf) echo "ffmpeg-free-devel" ;;
	ffmpegdev:*) echo "ffmpeg-devel" ;;
	cmake:*) echo "cmake" ;;
	pkgconfig:pacman) echo "pkgconf" ;;
	pkgconfig:*) echo "pkg-config" ;;
	whiptail:pacman) echo "libnewt" ;;
	whiptail:dnf) echo "newt" ;;
	whiptail:*) echo "whiptail" ;;
	gtk:apt-get) echo "python3-gi gir1.2-gtk-3.0" ;;
	gtk:pacman) echo "python-gobject gtk3" ;;
	gtk:zypper) echo "python3-gobject typelib-1_0-Gtk-3_0" ;;
	gtk:*) echo "python3-gobject gtk3" ;;
	v4lutils:*) echo "v4l-utils" ;;
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

uninstall_driver() {
	step "Removing the driver"
	sudo modprobe -r "$MODULE" 2>/dev/null
	if command -v dkms >/dev/null && dkms status "$NAME" 2>/dev/null | grep -q .; then
		sudo dkms remove "$NAME/$VERSION" --all || true
		sudo rm -rf "/usr/src/$NAME-$VERSION"
	fi
	sudo rm -f "/lib/modules/$(uname -r)/updates/$MODULE.ko"
	sudo depmod -a
	sudo rm -f "/etc/udev/rules.d/$UDEV_RULE" "/etc/udev/rules.d/$UDEV_DEVICE_RULE" "$SETTINGS" "$PROFILE_TOOL"
	sudo udevadm control --reload 2>/dev/null
	ok "Driver removed. Saved profiles (~/.config/elgato-gchd) were kept."
	uninstall_firmware
}

# The firmware files the installer extracted (other files in the folder
# stay; the folder goes when it is empty). Asks first (Enter = yes).
uninstall_firmware() {
	local f found=0
	for f in $FIRMWARE_FILES; do
		[ -e "$FIRMWARE_DIR/$f" ] && found=1
	done
	[ $found = 1 ] || return 0
	if ! ask_yes "Remove the firmware in $FIRMWARE_DIR too? (A new installation then needs Elgato's installer file again.)"; then
		ok "Firmware kept in $FIRMWARE_DIR."
		return 0
	fi
	for f in $FIRMWARE_FILES; do
		sudo rm -f "$FIRMWARE_DIR/$f"
	done
	sudo rmdir "$FIRMWARE_DIR" 2>/dev/null
	ok "Firmware removed from $FIRMWARE_DIR."
}

uninstall_settings() {
	step "Removing the settings programs"
	sudo rm -f "$SETTINGS_TOOL" "$SETTINGS_GUI" "$SETTINGS_DESKTOP"
	ok "Settings programs removed (the driver keeps its settings)."
}

uninstall_obs() {
	step "Removing the OBS plugin"
	rm -rf "$OBS_PLUGIN_DIR"
	ok "OBS plugin removed (restart OBS)."
}

firmware_present() {
	local f
	for f in $FIRMWARE_FILES; do
		[ -s "$FIRMWARE_DIR/$f" ] || return 1
	done
}

install_driver() {
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
	sudo install -D -m 0755 "$HERE/src/settings/gchd-profile" "$PROFILE_TOOL" ||
		die "cannot install $PROFILE_TOOL."
	ok "Profile command installed: gchd-profile (named sets of all settings)."
	sudo modprobe "$MODULE" || die "the driver could not be loaded (see: sudo dmesg)."
	ok "Driver loaded. Plug in the Game Capture HD; it appears as a /dev/video device."
	echo "Watch: v4l2-ctl -d /dev/elgato-gchd -v pixelformat=MPEG && ffplay -f mpegts /dev/elgato-gchd"
}

install_settings() {
	step "Settings programs"
	sudo install -D -m 0755 "$HERE/src/settings/gchd-settings" "$SETTINGS_TOOL"
	if ! command -v whiptail >/dev/null; then
		cmd=$(pkg_cmd)
		pk=$(pkg_name whiptail)
		if [ -n "$cmd" ] && ask "The settings tool needs whiptail. Install it now with: sudo $cmd $pk ?"; then
			# shellcheck disable=SC2086
			sudo $cmd $pk || echo "Installing $pk failed; gchd-settings needs it."
		fi
	fi
	sudo install -D -m 0755 "$HERE/src/settings/gchd-settings-gui" "$SETTINGS_GUI"
	sudo install -D -m 0644 "$HERE/src/settings/elgato-gchd-settings.desktop" "$SETTINGS_DESKTOP"
	need v4lutils command -v v4l2-ctl
	if ! /usr/bin/python3 -c "import gi; gi.require_version('Gtk', '3.0')" 2>/dev/null; then
		cmd=$(pkg_cmd)
		pk=$(pkg_name gtk)
		if [ -n "$cmd" ] && ask "The settings window needs $pk. Install it now with: sudo $cmd $pk ?"; then
			# shellcheck disable=SC2086
			sudo $cmd $pk || echo "Installing $pk failed; the settings window needs it."
		fi
	fi
	ok "Settings installed: 'Elgato Game Capture HD' in the system settings menu (Preferences > Hardware; gchd-settings-gui); text menu: gchd-settings"
}

install_obs() {
	step "OBS plugin"
	if ! command -v obs >/dev/null 2>&1; then
		if command -v flatpak >/dev/null 2>&1 &&
		   flatpak info com.obsproject.Studio >/dev/null 2>&1; then
			die "OBS is installed as a Flatpak; it cannot load this plugin. Install OBS from your distribution (or the OBS PPA) and start again."
		fi
		echo "OBS was not found; the plugin is installed anyway (for an OBS installed later)."
	fi
	need cmake command -v cmake
	need pkgconfig command -v pkg-config
	need cxx command -v g++
	need obsdev pkg-config --exists libobs
	need qtdev pkg-config --exists Qt6Widgets
	need ffmpegdev pkg-config --exists libavformat libavcodec libavutil
	rm -rf "$OBS_BUILD"
	mkdir -p "$BUILD"
	{ cmake -S "$HERE/src/obs-plugin" -B "$OBS_BUILD" -DCMAKE_BUILD_TYPE=Release &&
	  cmake --build "$OBS_BUILD"; } > "$BUILD/obs-plugin.log" 2>&1 ||
		die "plugin build failed; see $BUILD/obs-plugin.log"
	cmake --install "$OBS_BUILD" >> "$BUILD/obs-plugin.log" 2>&1 ||
		die "plugin install failed; see $BUILD/obs-plugin.log"
	ok "OBS plugin installed in $OBS_PLUGIN_DIR. Restart OBS and add the source 'Elgato Game Capture HD'."
}

# Optional parts when no option names them: ask for each.
choose_parts() {
	[ $ASSUME_YES = 1 ] && { WITH_SETTINGS=1; WITH_OBS=1; return; }
	echo "${B}The driver (core) is always installed. Optional parts:${N}"
	ask_yes "Install the settings programs (settings window and text menu)?" && WITH_SETTINGS=1
	ask_yes "Install the OBS plugin (source 'Elgato Game Capture HD')?" && WITH_OBS=1
	return 0
}

case $ACTION in
install)
	[ $CHOSEN = 1 ] || choose_parts
	install_driver
	if [ $WITH_SETTINGS = 1 ]; then install_settings; fi
	if [ $WITH_OBS = 1 ]; then install_obs; fi
	;;
uninstall)
	if [ $WITH_SETTINGS = 1 ] || [ $WITH_OBS = 1 ]; then
		if [ $WITH_SETTINGS = 1 ]; then uninstall_settings; fi
		if [ $WITH_OBS = 1 ]; then uninstall_obs; fi
	else
		uninstall_obs
		uninstall_settings
		uninstall_driver
	fi
	;;
esac
