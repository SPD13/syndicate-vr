#!/usr/bin/env python3
"""Widens raw register arguments that carry pointers to intptr_t.

Functions remade from asm often keep the asm's raw register arguments
(`int a1, int a2, ...`) and cast them to pointers inside; callers cast
pointers to int. On 64-bit that truncates the pointers. Reading the
warnings of a 64-bit build (-Wint-to-pointer-cast, -Wpointer-to-int-cast)
this tool
- changes every `int aN` parameter that the body casts to a pointer to
  `intptr_t aN`, in the definition and in every prototype;
- for every `(int)` cast of a pointer that is an argument of a call to
  such a function, widens that parameter as well and writes `(intptr_t)`.
On 32-bit, intptr_t is int: the build is the same. Casts it cannot place
(not in a call, unknown callee) are listed for manual work.

Usage: python tools/decomp-agent/widen_regs.py [--apply] [build/x64/make.log]
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
TOPS = ('src', 'bfariadne', 'swrendersoft', 'bflibrary', 'bfsoundlib', 'bfsmacklib')
WARN_RE = re.compile(r'([\w./\\:-]+\.[ch]):(\d+):(\d+): warning: .*\[-W(pointer-to-int-cast|int-to-pointer-cast)\]')


def all_files():
    paths = {}
    for top in TOPS:
        for dp, dn, fn in os.walk(os.path.join(REPO, top)):
            if 'x86-dos' in dp:
                continue
            for x in fn:
                if x.endswith(('.c', '.h')):
                    paths.setdefault(x, os.path.join(dp, x))
    return paths


def load(p):
    raw = open(p, encoding='utf-8', newline='').read()
    return raw.replace('\r\n', '\n'), ('\r\n' in raw)


def save(p, t, crlf):
    open(p, 'w', encoding='utf-8', newline='').write(t.replace('\n', '\r\n') if crlf else t)


def enclosing_function(lines, n):
    """Name and parameter list of the function definition around line n (1-based)."""
    for i in range(n - 1, -1, -1):
        m = re.match(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^;{}]*)\)\s*$', lines[i])
        if m and i + 1 < len(lines) and lines[i + 1].startswith('{'):
            return m.group(1), [p.strip() for p in m.group(2).split(',')]
        if lines[i].startswith('}'):
            return None, None
    return None, None


def call_at(lines, n, col):
    """Callee name and argument index of the call whose argument starts at
    line n, column col; the call may start on an earlier line."""
    text = '\n'.join(lines[:n - 1] + [lines[n - 1][:col - 1]])
    depth = 0
    k = 0
    i = len(text) - 1
    stop = max(0, i - 2000)
    while i >= stop:
        ch = text[i]
        if ch == ')':
            depth += 1
        elif ch == '(':
            if depth == 0:
                m = re.search(r'(\w+)\s*$', text[:i])
                return (m.group(1) if m else None), k
            depth -= 1
        elif ch == ',' and depth == 0:
            k += 1
        elif ch in ';{}' and depth == 0:
            return None, None
        i -= 1
    return None, None


def main():
    apply = '--apply' in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    log = args[0] if args else os.path.join(REPO, 'build', 'x64', 'make.log')
    paths = all_files()
    texts = {}

    def lines_of(f):
        if f not in texts:
            texts[f] = load(paths[f])[0].split('\n')
        return texts[f]

    widen = {}      # function -> set of parameter indexes
    casts = set()   # (file, line, col) of (int) casts to rewrite
    manual = []
    seen = set()
    for wl in open(log, errors='replace'):
        m = WARN_RE.search(wl)
        if not m:
            continue
        f = re.split(r'[\\/]', m.group(1))[-1]
        n, c, kind = int(m.group(2)), int(m.group(3)), m.group(4)
        if (f, n, c) in seen or f not in paths:
            continue
        seen.add((f, n, c))
        lines = lines_of(f)
        line = lines[n - 1]
        if kind == 'int-to-pointer-cast':
            mm = re.match(r'\([^()]*\)\s*(a\d+)\b', line[c - 1:])
            if mm:
                fn, params = enclosing_function(lines, n)
                if fn:
                    for k, p in enumerate(params):
                        if re.match(r'^int\s+%s$' % mm.group(1), p):
                            widen.setdefault(fn, set()).add(k)
                            break
                    else:
                        manual.append('%s:%d param %s of %s not int' % (f, n, mm.group(1), fn))
                    continue
            manual.append('%s:%d:%d %s' % (f, n, c, line.strip()[:100]))
        else:
            if not line[c - 1:].startswith('(int)'):
                manual.append('%s:%d:%d %s' % (f, n, c, line.strip()[:100]))
                continue
            callee, k = call_at(lines, n, c)
            if callee is None or callee in ('if', 'while', 'return', 'switch', 'sizeof', 'DIFFTEST_RETURN'):
                manual.append('%s:%d:%d %s' % (f, n, c, line.strip()[:100]))
                continue
            widen.setdefault(callee, set()).add(k)
            casts.add((f, n, c))
    # --source: explicit truncations `(int)(intptr_t)x` passed to calls
    src_casts = set()   # (file, line, col, length)
    if '--source' in sys.argv:
        for f in sorted(paths):
            if not f.endswith('.c'):
                continue
            lines = lines_of(f)
            for n, line in enumerate(lines, 1):
                for mm in re.finditer(r'\((?:int|s32)\)\s*\(u?intptr_t\)', line):
                    c = mm.start() + 1
                    callee, k = call_at(lines, n, c)
                    if callee is None or callee in ('if', 'while', 'return', 'switch', 'sizeof',
                                                    'DIFFTEST_RETURN', 'DIFFTEST_SHADOW',
                                                    'DIFFTEST_SHADOW_RET') or callee.endswith('_asm'):
                        continue
                    widen.setdefault(callee, set()).add(k)
                    src_casts.add((f, n, c, mm.end() - mm.start()))
    # parameter types in definitions and prototypes
    changed = {}
    for f, p in paths.items():
        t, crlf = load(p)
        t0 = t
        for fn, ks in widen.items():
            def fix(mm):
                params = mm.group(2).split(',')
                for k in ks:
                    if k < len(params):
                        params[k] = re.sub(r'^(\s*)int(\s+\w+\s*)$', r'\1intptr_t\2', params[k])
                return mm.group(1) + ','.join(params) + mm.group(3)
            t = re.sub(r'(\b(?:int|void|short|ushort|ubyte|sbyte|u32|s32|TbBool|TbResult|long|ulong|intptr_t|struct \w+ \*|char|uint)\s+\**%s\s*\()([^;{}()]*)(\)\s*(?:;|\n\{))'
                       % re.escape(fn), fix, t)
        if t != t0:
            changed[p] = (t, crlf)
    # casts at the warned positions
    for (f, n, c) in sorted(casts, key=lambda x: (x[0], x[1], -x[2])):
        p = paths[f]
        t, crlf = changed.get(p, load(p))
        ls = t.split('\n')
        line = ls[n - 1]
        if line[c - 1:].startswith('(int)'):
            ls[n - 1] = line[:c - 1] + '(intptr_t)' + line[c - 1 + 5:]
        changed[p] = ('\n'.join(ls), crlf)
    for (f, n, c, ln) in sorted(src_casts, key=lambda x: (x[0], x[1], -x[2])):
        p = paths[f]
        t, crlf = changed.get(p, load(p))
        ls = t.split('\n')
        line = ls[n - 1]
        ls[n - 1] = line[:c - 1] + '(intptr_t)' + line[c - 1 + ln:]
        changed[p] = ('\n'.join(ls), crlf)
    for fn in sorted(widen):
        print('widen %s params %s' % (fn, sorted(widen[fn])))
    print('== %d functions, %d casts, %d source casts, %d files' % (len(widen), len(casts), len(src_casts), len(changed)))
    print('== manual:')
    for x in manual:
        print('  ' + x)
    if apply:
        for p, (t, crlf) in changed.items():
            save(p, t, crlf)
    return 0


if __name__ == '__main__':
    sys.exit(main())
