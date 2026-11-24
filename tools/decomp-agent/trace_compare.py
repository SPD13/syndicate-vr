#!/usr/bin/env python3
"""Compares a folder of state traces with a reference folder.

Usage: python3 tools/decomp-agent/trace_compare.py <outdir> <refdir>
Line ends are ignored. Where <outdir> also has <scenario>.out files (runs
of swcore_headless, SWARS_TRACE_EXE), the turn and state hash it read
through the library interface must equal the trace. Exit status 0 when all
are equal.
"""
import os
import re
import sys


def main():
    out, ref = sys.argv[1], sys.argv[2]
    names = sorted(n for n in os.listdir(ref) if n.endswith('.trace'))
    differ = []
    for name in names:
        theirs = open(os.path.join(ref, name), 'rb').read().replace(b'\r', b'')
        try:
            mine = open(os.path.join(out, name), 'rb').read().replace(b'\r', b'')
        except OSError:
            differ.append(name + '(missing)')
            continue
        if mine != theirs or not mine:
            differ.append(name)
            continue
        outf = os.path.join(out, name[:-len('.trace')] + '.out')
        if os.path.exists(outf):
            api = [ln.strip() for ln in open(outf, 'rb').read().decode('latin-1').splitlines()
                   if re.fullmatch(r'\d+ [0-9a-f]{8}', ln.strip())]
            game = [' '.join(ln.split()[:2]) for ln in mine.decode('latin-1').splitlines()]
            if api != game:
                differ.append(name + '(api)')
    print('%d scenarios, %d differ%s' % (len(names), len(differ),
                                         (': ' + ' '.join(differ)) if differ else ''))
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
