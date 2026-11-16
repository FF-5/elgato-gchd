#!/usr/bin/env python3
"""Generate dkms/gchd_transcoder_fields.h from src/gchd_hardware.hpp.

The C++ source is only read (RULES.md C1). Run from the repository root:
    python3 tools/gen-transcoder-fields.py
"""
import re
src = open('src/gchd_hardware.hpp').read()
fields = re.findall(r'bitfield_t\s+(\w+)\s*=\s*\{\s*(0x[0-9a-fA-F]+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}', src)
tables = re.findall(r'static const unsigned\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+)', src)
out = ['/* SPDX-License-Identifier: GPL-2.0 */', '/*',
       ' * Transcoder (Fujitsu MB86H57/H58/M01 encoder) register fields.', ' *',
       ' * GENERATED from src/gchd_hardware.hpp (namespace Transcoder) by',
       ' * tools/gen-transcoder-fields.py. Do not edit by hand.', ' *',
       ' * Each GCHD_TC_<name> expands to "address, lsb, bits" for gchd_sparam():',
       ' *     GCHD_TC_SET(d, GCHD_TC_V_HSIZE_OUT, 720);',
       ' * Names are the Fujitsu script names; meanings are in docs/commands.md §5.',
       ' */', '#ifndef GCHD_TRANSCODER_FIELDS_H', '#define GCHD_TRANSCODER_FIELDS_H', '']
for n, a, l, b in fields:
    out.append('#define GCHD_TC_%-40s 0x%04x, %2d, %2d' % (n.upper(), int(a, 16), int(l), int(b)))
out += ['', '/* Start addresses of PSI tables written with slsi() */']
for n, a in tables:
    out.append('#define GCHD_TC_%-40s 0x%04x' % (n.upper(), int(a, 16)))
out += ['', '#endif /* GCHD_TRANSCODER_FIELDS_H */', '']
open('dkms/gchd_transcoder_fields.h', 'w').write('\n'.join(out))
