#!/usr/bin/env python3
"""Deletes `#if 0` blocks that only keep an old asm call.

Remade functions often start with the asm wrapper they replaced, disabled:

    #if 0
        asm volatile ("call ASM_x\\n" : : "a" (a1));
        return;
    #endif

Such a block is dropped when it contains inline asm. A block with `#else`
keeps its `#else` part. Other `#if 0` blocks (disabled C) stay.
Run with --apply to write the files; without, it lists the blocks.

Usage: python tools/decomp-agent/drop_dead_asm.py [--apply] [files...]
"""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
GLOBS = ('src/*.c', 'src/*.h', 'bflibrary/src/**/*.c', 'bfsoundlib/src/*.c',
         'bfsmacklib/src/*.c', 'swrendersoft/src/*.c', 'bfariadne/src/*.c')
ASM_RE = re.compile(r'\b(asm|__asm__)\s*(volatile|__volatile__)?\s*\(')
IF_RE = re.compile(r'^\s*#\s*if(def|ndef)?\b')
IF0_RE = re.compile(r'^\s*#\s*if\s+0\s*(//.*|/\*.*\*/\s*)?$')
ELSE_RE = re.compile(r'^\s*#\s*(else|elif)\b')
ENDIF_RE = re.compile(r'^\s*#\s*endif\b')


def process(lines):
    """Returns (new lines, list of (first line no, block length))."""
    out = []
    found = []
    i = 0
    n = len(lines)
    while i < n:
        if not IF0_RE.match(lines[i]):
            out.append(lines[i])
            i += 1
            continue
        # find the matching #else/#endif at depth 0
        depth = 0
        j = i + 1
        else_at = None
        while j < n:
            if IF_RE.match(lines[j]):
                depth += 1
            elif ENDIF_RE.match(lines[j]):
                if depth == 0:
                    break
                depth -= 1
            elif ELSE_RE.match(lines[j]) and depth == 0 and else_at is None:
                else_at = j
            j += 1
        if j >= n:
            out.append(lines[i])
            i += 1
            continue
        dead = lines[i + 1:(else_at if else_at is not None else j)]
        if else_at is not None and not re.match(r'^\s*#\s*else\b', lines[else_at]):
            dead = None  # #elif: leave it
        if dead is not None and any(ASM_RE.search(x) for x in dead):
            found.append((i + 1, j - i + 1))
            if else_at is not None:
                out.extend(lines[else_at + 1:j])
            i = j + 1
            continue
        out.append(lines[i])
        i += 1
    return out, found


def main():
    apply = '--apply' in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        args = sorted(set(p for g in GLOBS for p in glob.glob(os.path.join(REPO, g), recursive=True)
                          if 'x86-dos' not in p))
    total = 0
    for p in args:
        raw = open(p, 'rb').read()
        crlf = b'\r\n' in raw
        text = raw.decode('utf-8', 'surrogateescape').replace('\r\n', '\n')
        lines = text.split('\n')
        new, found = process(lines)
        if not found:
            continue
        total += len(found)
        print('%3d %s' % (len(found), os.path.relpath(p, REPO)))
        if apply:
            t = '\n'.join(new)
            if crlf:
                t = t.replace('\n', '\r\n')
            open(p, 'wb').write(t.encode('utf-8', 'surrogateescape'))
    print('== %d blocks' % total)


if __name__ == '__main__':
    main()
