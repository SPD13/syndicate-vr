#!/usr/bin/env python3
"""Export asm-local data labels that queued targets use, so C can link them.

A label `name:` followed by data directives becomes `GLOBAL(name)`, which
keeps the plain label for the asm and adds the exported symbol C links to
(`_name` on Windows). Nothing moves, so behaviour cannot change. The C side
declares them `extern` when a function using them is remade.

Usage:
  python tools/decomp-agent/export_data.py --list
  python tools/decomp-agent/export_data.py --apply
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SX = os.path.join(REPO, 'src', 'swars.sx')

DATA_DIR = re.compile(r'^\s+\.(long|short|word|byte|fill|ascii|asciz|string|space|zero|int|value)\b')
DEF_RE = re.compile(r'^(\w+):(.*)$')
FUNC_RE = re.compile(r'^\s*GLOBAL_FUNC\s*\(\s*(\w+)\s*\)|^(\w+):')
# a label is also used as an immediate address ($fwd_path); registers are %name
TOKEN = re.compile(r'(?<![%\w])([A-Za-z_]\w*)\b')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--apply', action='store_true')
    args = ap.parse_args()
    lines = open(SX, encoding='utf-8', newline='').read().split('\n')
    # data labels: a plain label whose next non-empty line is a data directive
    data = {}
    for i, line in enumerate(lines):
        m = DEF_RE.match(line)
        if not m or m.group(1).startswith('jump_'):
            continue
        j = i + 1
        while j < len(lines) and not lines[j].strip():
            j += 1
        if j < len(lines) and DATA_DIR.match(lines[j]):
            data[m.group(1)] = i
    # function bodies of queued targets
    bodies, cur = {}, None
    for line in lines:
        m = FUNC_RE.match(line)
        if m and not (m.group(2) or '').startswith('jump_'):
            name = m.group(1) or m.group(2)
            cur = [] if name.startswith('ASM_') else None
            if cur is not None:
                bodies[name] = cur
            continue
        if cur is not None:
            cur.append(line.split('#')[0].split('/*')[0])
    q = json.load(open(os.path.join(HERE, 'queue.json')))['targets']
    want = {}
    for t in q:
        if t['excluded'] not in (None, 'low-coverage', 'failed-3x'):
            continue
        for line in bodies.get('ASM_' + t['fn'], []):
            if 'EXPORT_SYMBOL' in line:
                line = re.sub(r'EXPORT_SYMBOL\s*\(\s*\w+\s*\)', '', line)
            parts = line.split(None, 1)
            if len(parts) < 2:
                continue
            for tok in TOKEN.findall(parts[1]):
                if tok in data:
                    want.setdefault(tok, set()).add(t['fn'])
    for name in sorted(want):
        print('%-28s used by %s' % (name, ', '.join(sorted(want[name]))))
    if not args.apply:
        return 0
    for name in want:
        i = data[name]
        m = DEF_RE.match(lines[i])
        lines[i] = 'GLOBAL(%s)%s' % (name, m.group(2))
    open(SX, 'w', encoding='utf-8', newline='').write('\n'.join(lines))
    print('exported %d data labels' % len(want))
    return 0


if __name__ == '__main__':
    sys.exit(main())
