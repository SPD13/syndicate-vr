#!/usr/bin/env python3
"""Runs difftest_run.sh on many functions in parallel, each on a few of the
scenarios that reach it (from coverage.json).

Each worker uses its own copy of the game data (build/gd_par/<n>, made on
first use), since runs write into the game data folder.

Usage: python tools/decomp-agent/difftest_batch.py <fns.json|fn,fn,...>
         [--jobs 4] [--per-fn 3] [--out build/difftest_batch.txt]
Prints one verdict line per function; exit code 0 when all PASS.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import threading

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))


def scen_name(cov_name):
    """coverage.json writes suffix tokens with ',' (m,bat); scenarios use '_'."""
    base, plus, rest = cov_name.partition('+')
    return base + (plus + rest.replace(',', '_') if plus else '')


def pick(scens, k):
    if len(scens) <= k:
        return scens
    step = (len(scens) - 1) / (k - 1) if k > 1 else 0
    return [scens[round(i * step)] for i in range(k)]


def find_bash():
    """On Windows plain `bash` may be WSL's; the scripts need Git or MSYS2 bash."""
    for b in ('C:/Program Files/Git/bin/bash.exe', 'C:/msys64/usr/bin/bash.exe'):
        if os.name == 'nt' and os.path.exists(b):
            return b
    return 'bash'


def gamedata_root():
    common = subprocess.run(['git', '-C', REPO, 'rev-parse', '--path-format=absolute',
                             '--git-common-dir'], capture_output=True, text=True).stdout.strip()
    return os.environ.get('SWARS_GAMEDATA') or os.path.join(
        os.path.dirname(os.path.dirname(common)), 'gamedata')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('fns')
    ap.add_argument('--jobs', type=int, default=4)
    ap.add_argument('--per-fn', type=int, default=3)
    ap.add_argument('--out', default=os.path.join(REPO, 'build', 'difftest_batch.txt'))
    a = ap.parse_args()
    fns = json.load(open(a.fns)) if a.fns.endswith('.json') else a.fns.split(',')
    cov = json.load(open(os.path.join(HERE, 'coverage.json')))
    src = gamedata_root()
    work = []
    for fn in fns:
        scens = [scen_name(s) for s in cov.get(fn, {}).get('scenarios', [])]
        work.append((fn, pick(scens, a.per_fn)))
    work.reverse()
    results = []
    lock = threading.Lock()
    out = open(a.out, 'w')

    def worker(n):
        gd = os.path.join(REPO, 'build', 'gd_par', str(n))
        if not os.path.isdir(os.path.join(gd, 'data')):
            shutil.copytree(src, gd, dirs_exist_ok=True)
        env = dict(os.environ, SWARS_GAMEDATA=gd.replace(os.sep, '/'))
        while True:
            with lock:
                if not work:
                    return
                fn, scens = work.pop()
            if not scens:
                line = 'NOCOVER %s' % fn
            else:
                p = subprocess.run([find_bash(), os.path.join(HERE, 'difftest_run.sh').replace(os.sep, '/'), fn] + scens,
                                   capture_output=True, text=True, env=env, errors='replace')
                verdict = [l for l in p.stdout.splitlines()
                           if l.split(' ')[0] in ('PASS', 'MISMATCH', 'UNVERIFIED', 'ERROR')]
                line = '%s [%s] %s' % (verdict[-1] if verdict else 'NOVERDICT rc=%d' % p.returncode,
                                       ' '.join(scens), fn)
            with lock:
                results.append(line)
                print(line, flush=True)
                out.write(line + '\n')
                out.flush()

    ts = [threading.Thread(target=worker, args=(n,)) for n in range(a.jobs)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    bad = [r for r in results if not r.startswith('PASS')]
    print('== %d functions, %d not PASS' % (len(results), len(bad)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
