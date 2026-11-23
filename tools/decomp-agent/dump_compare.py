#!/usr/bin/env python3
"""Compares two things-memory dumps of the state trace (SWARS_STATETRACE_DUMP)
of a 32-bit and a 64-bit build, field by field.

The dump is a 4-byte base address, then the simple things and things. In
a 64-bit build a thing is 4 bytes longer: PTarget is a 8-byte pointer.
Pointers are compared as thing indexes.

Usage: python tools/decomp-agent/dump_compare.py <dump32> <dump64> [max]
"""
import struct
import subprocess
import sys

STHINGS_LIMIT = 1500
THINGS_LIMIT = 1000
STHING_SIZE = 60
THING32 = 168
PTARGET = 64   # offset of PTarget


def fields():
    """(offset, size, name) of struct Thing in the 32-bit build, from gdb."""
    exe = 'build/harness/src/syndwarsfx.exe'
    out = subprocess.run(['C:/msys64/mingw64/bin/gdb.exe', '-batch', '-ex', 'ptype/o struct Thing', exe],
                         capture_output=True, text=True).stdout
    res = []
    depth = 0
    for line in out.splitlines():
        import re
        m = re.match(r'^/\*\s*(\d+)(?::\s*\d+)?\s*\|\s*(\d+)\s*\*/\s*(.*?);?\s*$', line.strip())
        if m and depth <= 1:
            res.append((int(m.group(1)), int(m.group(2)), m.group(3)))
        body = re.sub(r'^/\*.*?\*/', '', line.strip())
        depth += body.count('{') - body.count('}')
    return res


def main():
    d32 = open(sys.argv[1], 'rb').read()
    d64 = open(sys.argv[2], 'rb').read()
    limit = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    s32, s64 = d32[4:4 + STHINGS_LIMIT * STHING_SIZE], d64[4:4 + STHINGS_LIMIT * STHING_SIZE]
    shown = 0
    for i in range(STHINGS_LIMIT):
        a = s32[i * STHING_SIZE:(i + 1) * STHING_SIZE]
        b = s64[i * STHING_SIZE:(i + 1) * STHING_SIZE]
        if a != b and shown < limit:
            diffs = [k for k in range(STHING_SIZE) if a[k] != b[k]]
            print('sthing slot %d (index %d): bytes %s' % (i, i - STHINGS_LIMIT, diffs[:12]))
            shown += 1
    t32 = d32[4 + STHINGS_LIMIT * STHING_SIZE:]
    t64 = d64[4 + STHINGS_LIMIT * STHING_SIZE:]
    th64 = len(t64) // THINGS_LIMIT
    fl = fields()
    for i in range(THINGS_LIMIT):
        a = t32[i * THING32:(i + 1) * THING32]
        r = t64[i * th64:(i + 1) * th64]
        b = r[:PTARGET] + r[PTARGET:PTARGET + 4] + r[PTARGET + 8:]
        if a == b:
            continue
        names = []
        for (o, sz, decl) in fl:
            if o == PTARGET:
                continue
            if a[o:o + sz] != b[o:o + sz]:
                names.append('%s: %s != %s' % (decl.split()[-1], a[o:o + sz].hex(), b[o:o + sz].hex()))
        if names and shown < limit:
            print('thing %d: %s' % (i, '; '.join(names[:6])))
            shown += 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
