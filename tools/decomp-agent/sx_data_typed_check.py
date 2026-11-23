#!/usr/bin/env python3
"""Check typed variables (sx_data_typed.py) against the asm data layout.

For every variable that the last sx_data_typed.py run wrote, compares its
initial bytes in a 32-bit build with the typed definitions (build/harness)
with the bytes of the label in a build with the data file (build/difftest).
Pointers to strings must point to equal strings; pointers to asm code must
be NULL in the typed build.

Usage: python tools/decomp-agent/sx_data_typed_check.py <data.c> [typed.exe] [asm.exe]
Run sx_data_typed.py on the same data file first (its job file is read).
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from sx_data_check import symbols, sections, read, sym_addr  # noqa: E402
import sx_data_typed  # noqa: E402


def cstring(secs, addr):
    out = b''
    while True:
        b = read(secs, addr + len(out), 1)
        if b is None or b == b'\0':
            return out
        out += b


def main():
    data_path = sys.argv[1]
    typed_exe = sys.argv[2] if len(sys.argv) > 2 else os.path.join(REPO, 'build', 'harness', 'src', 'syndwarsfx.exe')
    asm_exe = sys.argv[3] if len(sys.argv) > 3 else os.path.join(REPO, 'build', 'difftest', 'src', 'syndwarsfx.exe')
    job = json.load(open(os.path.join(REPO, 'build', 'sx_data_typed_job.json')))
    out = json.load(open(os.path.join(REPO, 'build', 'sx_data_typed_out.json')))
    order, objs = sx_data_typed.parse_data_file(data_path)
    if not set(it['name'] for it in job['items']) <= set(order):
        print('ERROR: the last sx_data_typed.py run was for another data file')
        return 2
    st, ss = symbols(typed_exe), sections(typed_exe)
    sa, sas = symbols(asm_exe), sections(asm_exe)
    bad = 0
    for it, r in zip(job['items'], out):
        if 'error' in r:
            continue
        n = it['name']
        size = r['size']
        at, aa = sym_addr(st, n), sym_addr(sa, n)
        if at is None or aa is None:
            print('MISSING %s (typed %s, asm %s)' % (n, at, aa))
            bad += 1
            continue
        bt, ba = read(ss, at, size), read(sas, aa, size)
        diffs = []
        skip = set()
        for off, (kind, target) in it['ptrs'].items():
            off = int(off)
            if off + 4 > size:
                continue
            skip |= set(range(off, off + 4))
            pt = int.from_bytes(bt[off:off + 4], 'little')
            pa = int.from_bytes(ba[off:off + 4], 'little')
            if kind == 'code':
                if pt != 0:
                    diffs.append('code pointer at %d not NULL' % off)
            elif cstring(ss, pt) != cstring(sas, pa):
                diffs.append('string at %d: %r != %r' % (off, cstring(ss, pt), cstring(sas, pa)))
        for i in range(size):
            if i not in skip and bt[i] != ba[i]:
                diffs.append('byte %d: %02x != %02x' % (i, bt[i], ba[i]))
                if len(diffs) > 4:
                    break
        if diffs:
            bad += 1
            print('DIFF %s: %s' % (n, '; '.join(diffs)))
    print('%d of %d variables differ' % (bad, len(job['items'])))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
