#!/usr/bin/env python3
"""Windows: runs the state trace scenarios of statetrace.sh split over N
jobs, each with its own game data copy (build/gd_par/<n>, as
difftest_batch.py makes them), and compares the traces with a reference.

Usage: python tools/decomp-agent/statetrace_win_par.py <build> <outdir>
           [--jobs 6] [--ref build/tr_difftest]
<build> is a build/ folder (harness, x64, difftest). Exit status 0 when
every trace equals the reference (line ends ignored).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import threading

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GIT_BASH = r'C:\Program Files\Git\bin\bash.exe'


def scenarios():
    txt = open(os.path.join(REPO, 'tools', 'decomp-agent', 'statetrace.sh'), encoding='utf-8').read()
    m = re.search(r'^SCENARIOS="\$\{\*:-(.*)\}"$', txt, re.M)
    return m.group(1).split()


def gamedata_copy(n):
    gd = os.path.join(REPO, 'build', 'gd_par', str(n))
    if not os.path.isdir(gd):
        src = os.environ.get('SWARS_GAMEDATA') or os.path.join(
            os.path.dirname(os.path.dirname(REPO)), 'gamedata')
        shutil.copytree(src, gd)
    return gd


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('build')
    ap.add_argument('outdir')
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--ref', default=os.path.join('build', 'tr_difftest'))
    ap.add_argument('--scenarios', help='scenarios separated by spaces, '
                    'e.g. "0,1 1,2+bot"; default all of statetrace.sh')
    ap.add_argument('--no-compare', action='store_true',
                    help='only run, e.g. to make a reference from the difftest build')
    a = ap.parse_args()
    out = os.path.abspath(a.outdir)
    os.makedirs(out, exist_ok=True)
    todo = a.scenarios.split() if a.scenarios else scenarios()
    lock = threading.Lock()
    bash = GIT_BASH if os.path.exists(GIT_BASH) else 'bash'

    def worker(n):
        gd = gamedata_copy(n)
        env = dict(os.environ, SWARS_GAMEDATA=gd.replace(os.sep, '/'),
                   SWARS_TRACE_BUILD=a.build)
        while True:
            with lock:
                if not todo:
                    return
                sc = todo.pop(0)
            r = subprocess.run([bash, 'tools/decomp-agent/statetrace.sh',
                                out.replace(os.sep, '/'), sc], cwd=REPO, env=env,
                               capture_output=True, text=True)
            with lock:
                print((r.stdout.strip() or sc), flush=True)

    def run_all(jobs):
        ts = [threading.Thread(target=worker, args=(n,)) for n in range(jobs)]
        for t in ts:
            t.start()
        for t in ts:
            t.join()

    ref = os.path.join(REPO, a.ref) if not os.path.isabs(a.ref) else a.ref

    def compare(scs):
        differ = []
        for sc in scs:
            name = sc.replace(',', '_', 1) + '.trace'
            try:
                mine = open(os.path.join(out, name), 'rb').read().replace(b'\r', b'')
                theirs = open(os.path.join(ref, name), 'rb').read().replace(b'\r', b'')
            except OSError:
                differ.append(sc)
                continue
            if mine != theirs or not mine:
                differ.append(sc)
                continue
            # A run of swcore_headless (SWARS_TRACE_EXE) also printed the
            # turn and state hash read through the library interface
            outf = os.path.join(out, name[:-len('.trace')] + '.out')
            if os.path.exists(outf):
                api = [ln.strip() for ln in open(outf, 'rb').read().decode('latin-1').splitlines()
                       if re.fullmatch(r'\d+ [0-9a-f]{8}', ln.strip())]
                game = [' '.join(ln.split()[:2]) for ln in mine.decode('latin-1').splitlines()]
                if api != game:
                    differ.append(sc + '(api)')
        return differ

    run_all(a.jobs)
    if a.no_compare:
        return 0
    differ = compare(a.scenarios.split() if a.scenarios else scenarios())
    for _ in range(3):
        if not differ:
            break
        # Under heavy load a few runs drift (state that follows the drawing
        # timing); a real difference persists when the scenario runs alone
        print('rechecking alone: ' + ' '.join(differ), flush=True)
        todo.extend(sc.replace('(api)', '') for sc in differ)
        run_all(1)
        differ = compare([sc.replace('(api)', '') for sc in differ])
    print('%d scenarios, %d differ%s' % (len(a.scenarios.split() if a.scenarios else scenarios()), len(differ),
                                         (': ' + ' '.join(differ)) if differ else ''))
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
