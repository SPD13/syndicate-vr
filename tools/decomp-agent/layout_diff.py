#!/usr/bin/env python3
"""Struct layouts of two builds compared, for porting to 64-bit.

Reads the DWARF of a 32-bit and a 64-bit syndwarsfx.exe with gdb and lists
every struct whose size or member offsets differ between them, with the
members that cause it (pointers, `long` on LP64 systems). With --lp64 it
also lists structs that would change on an LP64 system (Linux, macOS),
where `long` is 8 bytes while it stays 4 on Windows: members whose type
resolves to `long`/`unsigned long`.

Usage: python tools/decomp-agent/layout_diff.py [--lp64] [--all] [struct ...]
  Defaults: build/release/src/syndwarsfx.exe (32-bit) and
  build/x64/src/syndwarsfx.exe (64-bit); read with the MINGW64 gdb. Without names, all
  structs; --all also prints structs that do not differ.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
MSYS = 'C:/msys64'
EXE32 = os.path.join(REPO, 'build', 'release', 'src', 'syndwarsfx.exe')
EXE64 = os.path.join(REPO, 'build', 'x64', 'src', 'syndwarsfx.exe')
# the 64-bit gdb reads the DWARF of both
GDB = {32: MSYS + '/mingw64/bin/gdb.exe', 64: MSYS + '/mingw64/bin/gdb.exe'}
LINE_RE = re.compile(r'^/\*\s*(\d+)(?::\s*\d+)?\s*\|\s*(\d+)\s*\*/\s*(.*?);?\s*$')
SIZE_RE = re.compile(r'/\* total size \(bytes\):\s*(\d+)\s*\*/')


def gdb(bits, exe, cmds):
    # a failing command would end a plain gdb script; run each from python
    path = os.path.join(REPO, 'build', 'layout_cmds_%d.py' % bits)
    with open(path, 'w') as f:
        f.write('import gdb\n')
        f.write('for c in %r:\n' % (['set pagination off', 'set width 0',
                                     'set print type hex off'] + list(cmds)))
        f.write('    try:\n        print(gdb.execute(c, to_string=True), end="")\n')
        f.write('    except gdb.error as e:\n        print("ERROR", e)\n')
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(GDB[bits]) + os.pathsep + env.get('PATH', '')
    r = subprocess.run([GDB[bits], '-batch', '-x', path, exe], capture_output=True,
                       text=True, errors='replace', env=env)
    return r.stdout


def struct_names(bits, exe):
    out = gdb(bits, exe, ['info types .'])
    names = set()
    for line in out.splitlines():
        m = re.match(r'^\d*:?\s*struct (\w+);$', line.strip())
        if m:
            names.add(m.group(1))
    return sorted(names)


def layouts(bits, exe, names):
    """name -> (size, [(offset, size, decl)]) for top-level members."""
    cmds = []
    for n in names:
        cmds += ['echo @@@ %s\\n' % n, 'ptype/o struct %s' % n]
    out = gdb(bits, exe, cmds)
    res = {}
    for chunk in out.split('@@@ ')[1:]:
        lines = chunk.splitlines()
        name = lines[0].strip()
        size, members, depth = None, [], 0
        for line in lines[1:]:
            m = SIZE_RE.search(line)
            if m and depth <= 1:
                size = int(m.group(1))
            body = re.sub(r'^/\*.*?\*/', '', line).strip()
            if depth == 1:
                m = LINE_RE.match(line.strip())
                if m:
                    members.append((int(m.group(1)), int(m.group(2)), m.group(3)))
            depth += body.count('{') - body.count('}')
        if size is not None:
            res[name] = (size, members)
    return res


def member_name(decl):
    m = re.search(r'(\w+)\s*(\[[^\]]*\])*\s*(:\s*\d+)?$', decl)
    return m.group(1) if m else decl


def resolve_long_types(bits, exe, decls):
    """Type names among member declarations that resolve to C `long`."""
    types = set()
    for d in decls:
        t = re.sub(r'\s*\b\w+\s*(\[[^\]]*\])*\s*(:\s*\d+)?$', '', d).strip()
        if '*' in t or t.startswith(('struct ', 'union ', 'enum ')) or '(' in t:
            continue
        types.add(t)
    types = sorted(types)
    cmds = []
    for t in types:
        cmds += ['echo @@@ %s\\n' % t, 'ptype %s' % t]
    out = gdb(bits, exe, cmds)
    longs = set()
    for chunk in out.split('@@@ ')[1:]:
        lines = chunk.splitlines()
        base = ' '.join(lines[1:]).replace('type =', '').strip()
        if re.search(r'\blong\b', base) and not re.search(r'\blong long\b', base) \
                and '{' not in base:
            longs.add(lines[0].strip())
    return longs


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    lp64 = '--lp64' in sys.argv
    show_all = '--all' in sys.argv
    names = args or struct_names(32, EXE32)
    l32 = layouts(32, EXE32, names)
    l64 = layouts(64, EXE64, names)
    longs = set()
    if lp64:
        decls = [d for s in l32.values() for (_, _, d) in s[1]]
        longs = resolve_long_types(32, EXE32, decls)
    ndiff = 0
    for n in names:
        if n not in l32 or n not in l64:
            continue
        s32, m32 = l32[n]
        s64, m64 = l64[n]
        causes = []
        o64 = {member_name(d): (o, sz, d) for (o, sz, d) in m64}
        for (o, sz, d) in m32:
            mn = member_name(d)
            if mn in o64 and o64[mn][1] != sz:
                causes.append('%s (%d->%d)' % (d, sz, o64[mn][1]))
        lp = []
        if lp64:
            for (o, sz, d) in m32:
                t = re.sub(r'\s*\b\w+\s*(\[[^\]]*\])*\s*(:\s*\d+)?$', '', d).strip()
                if t in longs:
                    lp.append(d)
        differs = s32 != s64 or causes
        if differs or lp or show_all:
            ndiff += 1
            print('struct %s: %d -> %d%s' % (n, s32, s64, '' if differs else ' (same on win64)'))
            for c in causes:
                print('    win64: ' + c)
            for d in lp:
                print('    lp64:  ' + d)
    print('== %d of %d structs listed' % (ndiff, len(names)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
