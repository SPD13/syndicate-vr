#!/usr/bin/env python3
"""Drops the `l` length modifier of printf directives (%ld, %lu, %08lx...)
in the string literals of our sources, after long_to_fixed.py made their
arguments s32/u32. Not %lld. Many log macros are not format-checked, so
the directives are changed everywhere rather than from warnings; a build
then shows -Wformat for any argument that is still long.

Usage: python tools/decomp-agent/long_formats.py   (from the repository root)
"""
import os, re
DIRS = ['src', 'bflibrary/src', 'bfsoundlib/src', 'bfsmacklib/src', 'swrendersoft/src', 'bfariadne/src',
        'bflibrary/include', 'swrendersoft/include', 'bfariadne/include', 'bfsoundlib/include',
        'bflibrary/tests', 'swrendersoft/tests']
STR = re.compile(r'"(?:\\.|[^"\\\n])*"')
DIR = re.compile(r'(%[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?)l([diouxX])')
tot = 0
for d in DIRS:
    for dp, dn, fn in os.walk(d):
        if 'x86-dos' in dp:
            continue
        for x in fn:
            if not x.endswith(('.c', '.h', '.cpp')) or x.endswith('_data.c'):
                continue
            p = os.path.join(dp, x)
            raw = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
            n = [0]
            def fixstr(m):
                s, k = DIR.subn(r'\1\2', m.group(0))
                n[0] += k
                return s
            new = STR.sub(fixstr, raw)
            if n[0]:
                tot += n[0]
                open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(new)
print('%d directives changed' % tot)
