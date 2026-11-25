"""Compares the game core's turn (swcore_core_step) with the desktop turn.

Usage: python tools/decomp-agent/coreturn_compare.py [build] [--jobs N]
       [--turns N] [--scenarios "0,1 1,2"] [--outdir build/coreturn]
       [--core-only]
Runs swcore_headless on the scenarios (by default the 52 of statetrace.sh,
with the bot's commands and the test hooks) twice:
with the desktop turn and the world not drawn (--no-draw), and with the
core turn (--core-turn), both with the default fixes of original bugs
(--fixes 1). Their state hashes of every turn must be equal until the
desktop run decides the mission (the core turn does not play the mission's
end); the hashes after that turn are not compared.

With --core-only, the core turn runs in swcore_coreonly, the program on the
game core alone (swcore_core_init(), null front end), so the mission setup
is the core's too. It has no test hooks: by default only the scenarios
without them (no "+" suffix).
"""
import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
def trace_scenarios():
    """The scenarios of statetrace.sh (missions, with the bot and hooks)."""
    for ln in open(os.path.join(REPO, 'tools', 'decomp-agent', 'statetrace.sh')):
        m = re.match(r'SCENARIOS="\$\{\*:-(.*)\}"$', ln.strip())
        if m:
            return m.group(1)
    raise SystemExit('no scenarios in statetrace.sh')


def hashes(path):
    """The (turn, hash) lines, and the turn the mission was decided (won or
    lost, from the "# turn N status S" lines of --status), or None."""
    out = []
    decided = None
    try:
        for ln in open(path, 'rb').read().decode('latin-1').splitlines():
            p = ln.split()
            if len(p) == 2 and p[0].isdigit() and len(p[1]) == 8:
                out.append((int(p[0]), p[1]))
            elif (len(p) == 5 and p[:2] == ['#', 'turn'] and p[3] == 'status'
                  and p[4] in ('2', '3') and decided is None):
                decided = int(p[2])
    except OSError:
        pass
    return out, decided


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('build', nargs='?', default='harness')
    ap.add_argument('--jobs', type=int, default=2)
    ap.add_argument('--turns', type=int, default=1500)
    ap.add_argument('--scenarios', default=None)
    ap.add_argument('--outdir', default=os.path.join('build', 'coreturn'))
    ap.add_argument('--core-only', action='store_true')
    a = ap.parse_args()
    if a.scenarios is None:
        a.scenarios = trace_scenarios()
        if a.core_only:
            a.scenarios = ' '.join(sc for sc in a.scenarios.split() if '+' not in sc)
    base = os.path.join(REPO, a.outdir)
    runs = {'desktop': ('swcore_headless', '--no-draw'),
            'core': ('swcore_coreonly' if a.core_only else 'swcore_headless', '--core-turn')}
    for name, (exe, arg) in runs.items():
        env = dict(os.environ, SWARS_TRACE_EXE=exe,
                   SWARS_TRACE_EXE_ARGS='%s --status --fixes 1 --turns %d' % (arg, a.turns))
        subprocess.run([sys.executable, 'tools/decomp-agent/statetrace_win_par.py', a.build,
                        os.path.join(base, name), '--jobs', str(a.jobs), '--no-compare',
                        '--scenarios', a.scenarios], cwd=REPO, env=env, check=False,
                       stdout=subprocess.DEVNULL)
    differ = 0
    for sc in a.scenarios.split():
        f = sc.replace(',', '_', 1) + '.out'
        d, decided = hashes(os.path.join(base, 'desktop', f))
        c, _ = hashes(os.path.join(base, 'core', f))
        if decided is not None:
            # the core turn does not play the mission's end
            d = [h for h in d if h[0] <= decided]
            c = [h for h in c if h[0] <= decided]
        n = min(len(d), len(c))
        first = next((i for i in range(n) if d[i] != c[i]), None)
        end = '' if decided is None else ', mission decided at turn %d' % decided
        if not d or not c:
            print('%-12s no output (desktop %d turns, core %d turns)' % (sc, len(d), len(c)))
            differ += 1
        elif first is not None:
            print('%-12s differs from turn %d (of %d%s)' % (sc, d[first][0], n, end))
            differ += 1
        else:
            print('%-12s equal for %d turns%s' % (sc, n, end))
    print('%d scenarios, %d differ' % (len(a.scenarios.split()), differ))
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
