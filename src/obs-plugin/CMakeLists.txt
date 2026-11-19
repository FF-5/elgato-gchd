# SPDX-License-Identifier: GPL-2.0
cmake_minimum_required(VERSION 3.16)
project(obs-elgato-gchd C)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OBS REQUIRED IMPORTED_TARGET libobs)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil)

add_library(obs-elgato-gchd MODULE src/gchd-source.c)
set_target_properties(obs-elgato-gchd PROPERTIES PREFIX "")
target_compile_options(obs-elgato-gchd PRIVATE -Wall -Wextra)
target_link_libraries(obs-elgato-gchd PRIVATE PkgConfig::OBS PkgConfig::FFMPEG)

# Per-user plugin folder of OBS (no administrator rights needed).
set(OBS_USER_PLUGINS "$ENV{HOME}/.config/obs-studio/plugins")
install(TARGETS obs-elgato-gchd
	LIBRARY DESTINATION "${OBS_USER_PLUGINS}/obs-elgato-gchd/bin/64bit")
