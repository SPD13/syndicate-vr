"""Function-level reference graph of the libswarscore objects.

Usage: python tools/decomp-agent/refgraph.py <build/harness/src> <root> [<root>...]
       [--stop sym,sym] [--why sym]
Reads `objdump -dr` of every library object; a function references the
symbols of its relocations (calls and data). Prints the functions and data
reachable from the roots, grouped by object, and with --why the shortest
reference path from a root to a symbol.
"""
import collections
import os
import re
import subprocess
import sys

LIMIT = int(os.environ.get('REFGRAPH_LIMIT', '12'))
OBJDUMP = 'C:/msys64/mingw32/bin/objdump.exe'
FUNC_RE = re.compile(r'^[0-9a-f]+ <([^>]+)>:$')
RELOC_RE = re.compile(r'^\s+[0-9a-f]+: (?:dir32|DISP32|secrel32)\s+(\S+)')
CALL_RE = re.compile(r'\s(?:call|jmp)\s+[0-9a-f]+ <([^>+]+)>')


def load(d):
    refs = collections.defaultdict(set)
    owner = {}
    for f in sorted(os.listdir(d)):
        if not (f.startswith('libswarscore_a-') and f.endswith('.o')):
            continue
        mod = f[len('libswarscore_a-'):-2]
        out = subprocess.run([OBJDUMP, '-dr', os.path.join(d, f)],
                             capture_output=True, text=True).stdout
        cur = None
        for line in out.splitlines():
            m = FUNC_RE.match(line)
            if m:
                cur = m.group(1)
                if not cur.startswith('.'):
                    owner[cur] = mod
                continue
            m = RELOC_RE.search(line) or CALL_RE.search(line)
            if m and cur:
                tgt = m.group(1).split('+')[0].split('-')[0]
                refs[cur].add(tgt)
        # data symbols
        out = subprocess.run([OBJDUMP, '-t', os.path.join(d, f)],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 6 and '.text' not in line and ('.data' in line or '.bss' in line or '.rdata' in line):
                owner.setdefault(parts[-1], mod)
    return refs, owner


def main():
    d = sys.argv[1]
    args = sys.argv[2:]
    stop, why = set(), None
    roots = []
    i = 0
    while i < len(args):
        if args[i] == '--stop':
            stop = set(args[i + 1].split(','))
            i += 2
        elif args[i] == '--why':
            why = args[i + 1]
            i += 2
        else:
            roots.append(args[i] if args[i].startswith('_') else '_' + args[i])
            i += 1
    stop = {s if s.startswith('_') else '_' + s for s in stop}
    refs, owner = load(d)
    prev = {r: None for r in roots}
    queue = collections.deque(roots)
    while queue:
        s = queue.popleft()
        if s in stop:
            continue
        for t in refs.get(s, ()):
            if t not in prev:
                prev[t] = s
                queue.append(t)
    if why:
        w = why if why.startswith('_') else '_' + why
        path = []
        while w is not None and w in prev:
            path.append('%s[%s]' % (w, owner.get(w, '?')))
            w = prev[w]
        print(' <- '.join(path) if path else 'not reached')
        return
    by_mod = collections.defaultdict(list)
    for s in prev:
        by_mod[owner.get(s, '(lib)')].append(s)
    for mod in sorted(by_mod, key=lambda m: -len(by_mod[m])):
        print("%-20s %4d  %s" % (mod, len(by_mod[mod]), " ".join(sorted(by_mod[mod])[:LIMIT])))


main()
