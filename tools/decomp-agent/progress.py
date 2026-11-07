#!/usr/bin/env python3
"""Report decompilation progress.

Usage: python tools/decomp-agent/progress.py

Shows remaining asm size, routed-but-not-remade count, shadowed (remade
with difftest reference) count, and pass/fail history from ledger.jsonl.
"""
import collections
import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def main():
    subprocess.run([sys.executable, '-I', os.path.join(HERE, 'queue.py')], check=True)
    q = json.load(open(os.path.join(HERE, 'queue.json')))
    sx = glob.glob(os.path.join(REPO, 'src', '*.sx'))
    sx_bytes = sum(os.path.getsize(f) for f in sx)
    asm_count = 0
    for f in sx:
        asm_count += open(f, errors='replace').read().count('GLOBAL_FUNC(ASM_')
    wr = sum(1 for l in open(os.path.join(REPO, 'conf', 'wrappers_game.conf'))
             if len(l.split()) >= 3 and l.split()[1] == 'W')
    st = q['stats']
    print('asm in src/*.sx:        %.2f MB in %d files' % (sx_bytes / 1e6, len(sx)))
    print('ASM_ entry points:      %d' % asm_count)
    print('game wrappers:          %d' % wr)
    print('routed, still asm:      %d (%d eligible now)' % (st['routed_live'], st['queued']))
    print('remade with difftest:   %d' % st['shadowed'])
    ledger = os.path.join(HERE, 'ledger.jsonl')
    if os.path.exists(ledger):
        entries = [json.loads(l) for l in open(ledger) if l.strip()]
        c = collections.Counter(e['status'] for e in entries)
        cost = sum(e.get('cost_usd') or 0 for e in entries)
        print('ledger:                 %d attempts, $%.2f total' % (len(entries), cost))
        for k, v in c.most_common():
            print('  %-20s %d' % (k, v))
        print('last 10:')
        for e in entries[-10:]:
            print('  %s %-36s %-14s %s' % (e.get('time', ''), e['fn'], e['status'],
                                         e.get('commit', '')))


if __name__ == '__main__':
    main()
