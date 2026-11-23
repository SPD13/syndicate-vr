#!/usr/bin/env python3
"""Inventory of the asm that a build without difftest still contains.

Lists live inline asm in C sources (outside comments, `#if 0` blocks and
DIFFTEST_REF_FUNC reference functions), with the enclosing function and
the asm symbol it calls, and the .sx files that src/Makefile.am assembles
unconditionally.

Usage: python tools/decomp-agent/asm_inventory.py [--summary]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from sx_deps import strip_c  # noqa: E402

TOPS = ('src', 'bflibrary/src', 'bfsoundlib/src', 'bfsmacklib/src', 'swrendersoft/src')
ASM_RE = re.compile(r'\basm\s+volatile\b|\b__asm__\b|\basm\s*\(')
FUNC_RE = re.compile(r'\n(\w[\w \*]*?\b(\w+)\s*\([^;{]*\))\s*\{')


def inline_asm():
    rows = []
    for top in TOPS:
        for dp, dn, fn in os.walk(os.path.join(REPO, top)):
            if 'x86-dos-watcom' in dp:
                continue
            for f in sorted(fn):
                if not f.endswith(('.c', '.cpp')):
                    continue
                p = os.path.join(dp, f)
                t = strip_c(open(p, encoding='utf-8', errors='replace').read())
                for m in ASM_RE.finditer(t):
                    fs = list(FUNC_RE.finditer(t[:m.start()]))
                    call = re.search(r'call\s+\*?(\w+)', t[m.start():m.start() + 400])
                    rows.append((os.path.relpath(p, REPO).replace('\\', '/'),
                                 fs[-1].group(2) if fs else '?',
                                 call.group(1) if call else '-'))
    return rows


def built_sx():
    mk = open(os.path.join(REPO, 'src', 'Makefile.am'), encoding='utf-8').read()
    # drop conditional blocks for difftest
    mk = re.sub(r'(?ms)^if ENABLE_DIFFTEST\n.*?^endif\n', '', mk)
    return sorted(set(re.findall(r'([\w./]+\.sx)\b', mk)))


def main():
    rows = inline_asm()
    if '--summary' not in sys.argv:
        for r in rows:
            print('%-40s %-40s %s' % r)
    per = {}
    for r in rows:
        per[r[0]] = per.get(r[0], 0) + 1
    print('== live inline asm: %d in %d files' % (len(rows), len(per)))
    for f, n in sorted(per.items(), key=lambda x: -x[1]):
        print('  %4d %s' % (n, f))
    print('== .sx assembled without difftest:')
    for s in built_sx():
        print('  ' + s)
    return 0


if __name__ == '__main__':
    sys.exit(main())
