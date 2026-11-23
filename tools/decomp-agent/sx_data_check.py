#!/usr/bin/env python3
"""Check data moved by sx_data2c.py against the asm build it replaces.

Compares two executables (the build with the data in asm, the build with
the generated C file) for the labels of an .sx file as parsed by
sx_data2c.py: within each section, every label must have the same offset
from the first label, and the bytes must be equal except in pointer slots
(which hold addresses that differ between builds).

Usage:
  python tools/decomp-agent/sx_data_check.py <old.exe> <new.exe> <file.sx-from-old-tree>
Give the .sx from a tree before the move (it still has the data).
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import sx_data2c  # noqa: E402

TOOLS = 'C:/msys64/mingw32/bin/'


def symbols(exe):
    out = subprocess.run([TOOLS + 'nm.exe', exe], capture_output=True, text=True, check=True).stdout
    syms = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) == 3:
            name = p[2]
            # prefer global symbols over file-static ones of the same name
            if name not in syms or (p[1].isupper() and not syms[name][1]):
                syms[name] = (int(p[0], 16), p[1].isupper())
    syms = {k: v[0] for k, v in syms.items()}
    return syms


def sections(exe):
    """[(vma, raw bytes)] of all sections, from the PE file."""
    data = open(exe, 'rb').read()
    pe = int.from_bytes(data[0x3C:0x40], 'little')
    nsec = int.from_bytes(data[pe + 6:pe + 8], 'little')
    optsz = int.from_bytes(data[pe + 20:pe + 22], 'little')
    base = int.from_bytes(data[pe + 24 + 28:pe + 24 + 32], 'little')
    secs = []
    off = pe + 24 + optsz
    for i in range(nsec):
        h = data[off + 40 * i:off + 40 * (i + 1)]
        vsize = int.from_bytes(h[8:12], 'little')
        vaddr = int.from_bytes(h[12:16], 'little')
        rsize = int.from_bytes(h[16:20], 'little')
        rptr = int.from_bytes(h[20:24], 'little')
        raw = data[rptr:rptr + min(rsize, vsize)] + b'\0' * max(0, vsize - rsize)
        secs.append((base + vaddr, raw))
    return secs


def read(secs, addr, n):
    for vma, raw in secs:
        if vma <= addr and addr + n <= vma + len(raw):
            return raw[addr - vma:addr - vma + n]
    return None


def sym_addr(syms, name):
    for cand in ('_' + name, name):
        if cand in syms:
            return syms[cand]
    return None


def main():
    cvars = set()
    for a in sys.argv[4:]:
        if a.startswith('--c-vars='):
            cvars = set(x for x in a.split('=', 1)[1].split(',') if x)
    old_exe, new_exe, sx = sys.argv[1:4]
    lines, items, text_labels = sx_data2c.parse(sx)
    secs = sx_data2c.build_objects(items)
    for sec, objs in secs:
        objs[:] = [o for o in objs if not (o['name'] and o['name'].startswith('_')
                                           and o['name'][1:] in cvars)]
    so, sn = symbols(old_exe), symbols(new_exe)
    ro, rn = sections(old_exe), sections(new_exe)
    bad = 0
    checked = 0
    for sec, objs in secs:
        named = [o for o in objs if o['name']]
        if not named:
            continue
        first = named[0]
        bo, bn = sym_addr(so, first['name']), sym_addr(sn, first['name'])
        if bo is None or bn is None:
            print('MISSING first label %s (old=%s new=%s)' % (first['name'], bo, bn))
            bad += 1
            continue
        for o in named:
            ao, an = sym_addr(so, o['name']), sym_addr(sn, o['name'])
            if ao is None or an is None:
                print('MISSING %s (old=%s new=%s)' % (o['name'], ao, an))
                bad += 1
                continue
            if ao - bo != an - bn:
                print('OFFSET %s: old +%d new +%d' % (o['name'], ao - bo, an - bn))
                bad += 1
            # bytes, skipping pointer slots
            pos = 0
            for p in o['parts']:
                if p[0] == 'b':
                    n = len(p[1])
                    xo, xn = read(ro, ao + pos, n), read(rn, an + pos, n)
                    if xo != xn:
                        print('BYTES %s+%d (%d bytes) differ' % (o['name'], pos, n))
                        bad += 1
                    elif xo != p[1]:
                        print('PARSE %s+%d: old exe bytes differ from parsed asm' % (o['name'], pos))
                        bad += 1
                    pos += n
                else:
                    pos += 4
            checked += 1
    print('checked %d labels, %d problems' % (checked, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
