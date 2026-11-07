#!/usr/bin/env python3
"""Show everything needed to remake one function.

Usage: python tools/decomp-agent/show_fn.py <name>

Prints the asm body of ASM_<name> (up to the next function separator),
the wrappers_game.conf line, the current C function, and callers in asm
and C. Labels of shared code reached by jumps outside the body are listed
so they can be followed.
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(REPO, 'src')
SEP_RE = re.compile(r'^/\*-{10,}\*/\s*$')


def asm_body(name):
    for fn in sorted(os.listdir(SRC)):
        if not fn.endswith('.sx'):
            continue
        lines = open(os.path.join(SRC, fn), errors='replace').read().split('\n')
        for i, l in enumerate(lines):
            if re.match(r'^\s*GLOBAL_FUNC\s*\(\s*(ASM_)?%s\s*\)' % re.escape(name), l) or \
               re.match(r'^%s:' % re.escape(name), l):
                j = i + 1
                if j < len(lines) and SEP_RE.match(lines[j]):
                    j += 1
                k = j
                while k < len(lines) and not SEP_RE.match(lines[k]):
                    k += 1
                return 'src/%s' % fn, i + 1, lines[i:k]
    return None, 0, []


def main():
    name = sys.argv[1]
    if name.startswith('ASM_'):
        name = name[4:]
    fn, ln, body = asm_body('ASM_' + name)
    if not body:
        fn, ln, body = asm_body(name)
    print('=== asm %s:%d (%d lines)' % (fn, ln, len(body)))
    print('\n'.join(body).rstrip())
    labels = set(re.findall(r'^\s*(jump_\w+):', '\n'.join(body), re.M))
    jumps = set(re.findall(r'\bj\w+\s+(jump_\w+)', '\n'.join(body)))
    outside = sorted(jumps - labels)
    if outside:
        print('\n!!! jumps to labels outside this body (shared code): %s' % ', '.join(outside))
    calls = sorted(set(re.findall(r'^\s+call\s+\*?(\w+)', '\n'.join(body), re.M)))
    print('\n=== calls from asm body: %s' % (', '.join(calls) or '(none)'))
    print('\n=== conf/wrappers_game.conf')
    for l in open(os.path.join(REPO, 'conf', 'wrappers_game.conf')):
        if l.split() and l.split()[0] == name:
            print(l.rstrip())
    print('\n=== C definition')
    for cf in sorted(os.listdir(SRC)):
        if not cf.endswith('.c'):
            continue
        text = open(os.path.join(SRC, cf), errors='replace').read()
        for m in re.finditer(r'^[A-Za-z_][^\n;]*\b%s\s*\([^;{]*\)\s*\{' % re.escape(name), text, re.M):
            end = text.find('\n}\n', m.start())
            start_ln = text.count('\n', 0, m.start()) + 1
            print('src/%s:%d' % (cf, start_ln))
            print(text[m.start():end + 2])
    print('=== asm callers (call ac_%s / %s)' % (name, name))
    for f in sorted(os.listdir(SRC)):
        if f.endswith('.sx'):
            for i, l in enumerate(open(os.path.join(SRC, f), errors='replace'), 1):
                if re.search(r'\bcall\s+(ac_)?%s\b' % re.escape(name), l):
                    print('  src/%s:%d %s' % (f, i, l.strip()))
    print('=== C callers')
    for f in sorted(os.listdir(SRC)):
        if f.endswith('.c'):
            for i, l in enumerate(open(os.path.join(SRC, f), errors='replace'), 1):
                if re.search(r'\b%s\s*\(' % re.escape(name), l) and not re.match(r'^\w', l):
                    print('  src/%s:%d %s' % (f, i, l.strip()))


if __name__ == '__main__':
    main()
