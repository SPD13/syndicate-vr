#!/usr/bin/env python3
"""Replaces `long` and `ulong` with the fixed-width s32/u32 in our sources.

The original is 32-bit code: its `long` is 32 bits. On LP64 systems
(Linux, macOS, Android) `long` has 64 bits, which changes struct layouts,
file records and arithmetic that wraps at 32 bits. Windows keeps 32-bit
long, so the 64-bit Windows build cannot show these problems. Using s32
and u32 makes all targets the same.

Only code is changed, not comments or strings:
  unsigned long [int], long unsigned [int], ulong  -> u32
  [signed] long [int]                              -> s32
`long long` and `long double` stay. Run with --apply to write the files;
without, it prints the counts per file.

Usage: python tools/decomp-agent/long_to_fixed.py [--apply] [paths...]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
DIRS = ('src', 'bflibrary/src', 'bflibrary/include', 'bfsoundlib/src', 'bfsoundlib/include',
        'bfsmacklib/src', 'bfsmacklib/include', 'swrendersoft/src', 'swrendersoft/include',
        'bfariadne/src', 'bfariadne/include', 'tests', 'bflibrary/tests', 'swrendersoft/tests')
SKIP_DIRS = ('x86-dos-watcom',)
# Windows API declarations, which must say long
SKIP_FILES = ('ssemphr.cpp',)
# lines that must keep `long` (typedefs of the base names, C library APIs)
KEEP_RE = re.compile(r'typedef\s+(unsigned\s+)?long\s+(ulong|DwBool)\b|typedef\s+long\s+long\b')

TOKEN_RE = re.compile(r'''
    (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')
  | (?P<pre>^[ \t]*\#[ \t]*include[^\n]*)
  | (?P<code>[^"'/\n]+|/|\n)
''', re.S | re.X | re.M)

RULES = [
    (re.compile(r'\b(?:unsigned\s+long\s+long|long\s+long\s+unsigned|long\s+long|long\s+double)\b'), None),
    (re.compile(r'\b(?:unsigned\s+long\s+int|unsigned\s+long|long\s+unsigned\s+int|long\s+unsigned)\b'), 'u32'),
    (re.compile(r'\b(?:signed\s+long\s+int|signed\s+long|long\s+signed\s+int|long\s+signed|long\s+int|long)\b'), 's32'),
    (re.compile(r'\bulong\b'), 'u32'),
]
ALL_RE = re.compile('|'.join('(%s)' % r.pattern for r, _ in RULES))


# bfsoundlib does not use bftypes.h; it gets the <stdint.h> names
STDINT_DIRS = ('bfsoundlib',)
NAMES = {'u32': 'u32', 's32': 's32'}


def convert_code(code):
    def rep(m):
        for k, (r, new) in enumerate(RULES):
            if m.group(k + 1) is not None:
                return m.group(0) if new is None else NAMES[new]
        return m.group(0)
    return ALL_RE.sub(rep, code)


def convert(text):
    out = []
    n = 0
    for line in text.split('\n'):
        if KEEP_RE.search(line):
            out.append(line)
            continue
        parts = []
        for m in TOKEN_RE.finditer(line):
            if m.group('code') is not None:
                c = convert_code(m.group('code'))
                if c != m.group('code'):
                    n += 1
                parts.append(c)
            else:
                parts.append(m.group(0))
        out.append(''.join(parts))
    return '\n'.join(out), n


def files(args):
    if args:
        for a in args:
            yield a
        return
    for d in DIRS:
        for dp, dn, fn in os.walk(os.path.join(REPO, d)):
            if any(s in dp for s in SKIP_DIRS):
                continue
            for x in sorted(fn):
                if x.endswith(('.c', '.h', '.cpp', '.hpp')) and not x.endswith('_data.c') \
                        and x not in SKIP_FILES:
                    yield os.path.join(dp, x)


def main():
    apply = '--apply' in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    total = 0
    for p in files(args):
        raw = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
        crlf = '\r\n' in raw
        t = raw.replace('\r\n', '\n')
        stdint = any(os.path.relpath(p, REPO).replace(os.sep, '/').startswith(d + '/')
                     for d in STDINT_DIRS)
        NAMES.update({'u32': 'uint32_t', 's32': 'int32_t'} if stdint else {'u32': 'u32', 's32': 's32'})
        # multi-line comments: protect by handling the file as a whole
        new, n = convert_whole(t)
        if n:
            total += n
            print('%5d %s' % (n, os.path.relpath(p, REPO)))
            if apply:
                open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(
                    new.replace('\n', '\r\n') if crlf else new)
    print('== %d changes' % total)
    return 0


WHOLE_RE = re.compile(r'''
    (?P<comment>//[^\n]*|/\*.*?\*/)
  | (?P<string>"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])*')
  | (?P<include>^[ \t]*\#[ \t]*include[^\n]*)
  | (?P<code>[^"'/\n]+|/|\n)
''', re.S | re.X | re.M)


def convert_whole(t):
    parts = []
    n = 0
    # keep lines matching KEEP_RE untouched: mark them
    for m in WHOLE_RE.finditer(t):
        if m.group('code') is not None:
            seg = m.group('code')
            line_start = t.rfind('\n', 0, m.start()) + 1
            line_end = t.find('\n', m.start())
            line = t[line_start:line_end if line_end >= 0 else len(t)]
            if KEEP_RE.search(line):
                parts.append(seg)
                continue
            c = convert_code(seg)
            if c != seg:
                n += 1
            parts.append(c)
        else:
            parts.append(m.group(0))
    return ''.join(parts), n


if __name__ == '__main__':
    sys.exit(main())
