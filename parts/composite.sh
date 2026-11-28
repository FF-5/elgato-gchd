# SPDX-License-Identifier: GPL-2.0
# shellcheck shell=bash
# Composite input (input 2): the analog cases (analog.sh).

composite_setup() { AIN=2; ANAME=Composite; a_setup; }
part composite "Composite" composite_setup
analog_cases composite
