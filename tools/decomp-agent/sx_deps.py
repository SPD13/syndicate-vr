#!/usr/bin/env python3
"""What still ties an asm file into builds without difftest.

For each given .sx file: the symbols it defines (functions and data) that
code outside it uses. C uses inside DIFFTEST_REF_FUNC reference functions
and inside `#if 0` blocks do not count, since those are dropped without
difftest.

Usage: python tools/decomp-agent/sx_deps.py src/bat_s.sx [...]
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def strip_c(text):
    """Drop comments, #if 0 blocks and DIFFTEST_REF_FUNC function bodies."""
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    out, depth, skip = [], 0, []
    for line in text.split('\n'):
        s = line.strip()
        if re.match(r'#\s*if\b', s):
            skip.append(bool(re.match(r'#\s*if\s+0\b', s)) or (skip and skip[-1]))
            continue
        if re.match(r'#\s*endif\b', s) and skip:
            skip.pop()
            continue
        if re.match(r'#\s*else\b', s) and skip:
            parent = len(skip) > 1 and skip[-2]
            skip[-1] = parent or not skip[-1]
            continue
        if skip and skip[-1]:
            continue
        out.append(line)
    text = '\n'.join(out)
    # remove reference functions: from DIFFTEST_REF_FUNC to matching brace
    res, i = [], 0
    for m in re.finditer(r'DIFFTEST_REF_FUNC', text):
        if m.start() < i:
            continue
        j = text.find('{', m.end())
        if j < 0:
            break
        d, k = 0, j
        while k < len(text):
            if text[k] == '{':
                d += 1
            elif text[k] == '}':
                d -= 1
                if d == 0:
                    break
            k += 1
        res.append(text[i:m.start()])
        i = k + 1
    res.append(text[i:])
    return ''.join(res)


def defined_syms(sx):
    t = open(sx, encoding='utf-8', errors='replace').read()
    funcs = set(re.findall(r'GLOBAL_FUNC\s*\(\s*(\w+)\s*\)', t))
    data = set(re.findall(r'^\s*GLOBAL\s*\(\s*(\w+)\s*\)', t, re.M))
    data.discard('sym')
    return funcs, data


def main():
    files = sys.argv[1:]
    sxs = glob.glob(os.path.join(REPO, 'src', '*.sx')) + \
        glob.glob(os.path.join(REPO, '*', 'src', '*.sx'))
    cs = []
    for top in ('src', 'bflibrary', 'bfsoundlib', 'bfsmacklib', 'swrendersoft'):
        for dp, dn, fn in os.walk(os.path.join(REPO, top)):
            cs += [os.path.join(dp, f) for f in fn if f.endswith(('.c', '.h', '.cpp'))]
    ctext = {f: strip_c(open(f, encoding='utf-8', errors='replace').read()) for f in cs}
    sxtext = {f: open(f, encoding='utf-8', errors='replace').read() for f in sxs}
    for sx in files:
        sx = os.path.join(REPO, sx)
        funcs, data = defined_syms(sx)
        print('== %s: %d functions, %d data symbols' % (os.path.relpath(sx, REPO), len(funcs), len(data)))
        for sym in sorted(funcs | data):
            users = []
            pat = re.compile(r'\b%s\b' % re.escape(sym))
            for f, t in sxtext.items():
                if os.path.samefile(f, sx):
                    continue
                n = len(pat.findall(t))
                if n:
                    users.append('%s(%d)' % (os.path.relpath(f, REPO), n))
            for f, t in ctext.items():
                n = len(pat.findall(t))
                if n:
                    users.append('%s(%d)' % (os.path.relpath(f, REPO), n))
            kind = 'func' if sym in funcs else 'data'
            if users:
                print('  %-4s %-36s %s' % (kind, sym, ' '.join(users)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
