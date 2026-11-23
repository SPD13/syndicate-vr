#!/usr/bin/env python3
"""Build the decompilation target queue.

Scans src/*.sx for asm functions and src/*.c for C wrappers which still
call into asm (`call ASM_name` outside `#if 0` / DIFFTEST reference
functions), and writes tools/decomp-agent/queue.json ordered by priority.

Usage: python tools/decomp-agent/queue.py [--print N]
"""
import collections
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
QUEUE = os.path.join(HERE, 'queue.json')
LEDGER = os.path.join(HERE, 'ledger.jsonl')
COVERAGE = os.path.join(HERE, 'coverage.json')
# Calls in the scenarios needed for a target to be verifiable (coverage.sh)
MIN_COVERAGE_CALLS = 5

# Subsystems left for platform rewrites instead of conversion. The game
# side of sound (src/sound*.sx) is converted since 2026-11-22; network
# stays out (solo play first).
EXCLUDE_FILE_RE = re.compile(r'(radica|net|ailssa|bfsmack|dos)', re.I)
EXCLUDE_NAME_RE = re.compile(r'^(net_|LbNet|ipx|smack|ail_|AIL_|dos_|joy_)', re.I)

LABEL_RE = re.compile(r'^\s*GLOBAL_FUNC\s*\(\s*(\w+)\s*\)|^(\w+):')
INSN_RE = re.compile(r'^\s+[a-z]')
CALL_RE = re.compile(r'^\s+call\s+\*?(\w+)')
ADDR_RE = re.compile(r'/\*\s*0x([0-9A-Fa-f]+)\s*\*/')
FUZZ_CASE_RE = re.compile(r'^\s*\{"(\w+)",\s*fuzz_\w+\}', re.M)


def fuzz_cases():
    """Names with a random-input case in src/difftest_fuzz.c; difftest_run.sh
    verifies them without scenario coverage."""
    path = os.path.join(REPO, 'src', 'difftest_fuzz.c')
    if not os.path.exists(path):
        return set()
    return set(FUZZ_CASE_RE.findall(open(path, errors='replace').read()))


def parse_asm():
    """Returns dict name -> {file, insns, calls, addr, line}."""
    funcs = {}
    for fn in sorted(os.listdir(os.path.join(REPO, 'src'))):
        if not fn.endswith('.sx'):
            continue
        path = os.path.join(REPO, 'src', fn)
        cur = None
        for lineno, line in enumerate(open(path, errors='replace'), 1):
            m = LABEL_RE.match(line)
            if m:
                name = m.group(1) or m.group(2)
                # local jump labels are not functions
                if name.startswith(('jump_', 'data_', 'func_')) and m.group(2):
                    if cur is None:
                        continue
                    continue
                a = ADDR_RE.search(line)
                cur = funcs.setdefault(name, {
                    'file': 'src/' + fn, 'line': lineno, 'insns': 0,
                    'calls': set(), 'addr': a.group(1) if a else None,
                    'global': bool(m.group(1))})
                continue
            if cur is not None and INSN_RE.match(line) and not line.strip().startswith('.'):
                cur['insns'] += 1
                c = CALL_RE.match(line)
                if c:
                    cur['calls'].add(c.group(1))
    return funcs


def strip_inactive(text):
    """Removes `#if 0` blocks and DIFFTEST reference functions."""
    out = []
    skip = 0
    for line in text.split('\n'):
        s = line.strip()
        if skip:
            if s.startswith('#if'):
                skip += 1
            elif s.startswith('#endif'):
                skip -= 1
            elif s.startswith('#else') and skip == 1:
                skip = 0
            continue
        if s == '#if 0':
            skip = 1
            continue
        out.append(line)
    text = '\n'.join(out)
    # reference functions: static DIFFTEST_REF_FUNC ... { ... }
    text = re.sub(r'static\s+DIFFTEST_REF_FUNC[^{]*\{.*?\n\}', '', text, flags=re.S)
    return text


C_FUNC_RE = re.compile(r'^[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^;{]*)\)\s*\{', re.M)


def parse_c():
    """Returns dict asm_name -> {cfile, cfunc} for live `call ASM_x`."""
    live = {}
    remade_refs = {}
    # bfariadne (triangulation) is compiled with the game sources
    files = [('src', f) for f in sorted(os.listdir(os.path.join(REPO, 'src')))]
    files += [('bfariadne/src', f) for f in sorted(os.listdir(os.path.join(REPO, 'bfariadne', 'src')))]
    for sub, fn in files:
        if not fn.endswith('.c'):
            continue
        path = os.path.join(REPO, sub, fn)
        raw = open(path, errors='replace').read()
        for m in re.finditer(r'DIFFTEST_SHADOW(?:_RET)?\s*\(\s*(\w+)', raw):
            remade_refs[m.group(1)] = sub + '/' + fn
        text = strip_inactive(raw)
        funcs = [(m.start(), m.group(1)) for m in C_FUNC_RE.finditer(text)]
        for m in re.finditer(r'call\s+_?ASM_(\w+)', text):
            cfunc = None
            for pos, name in funcs:
                if pos < m.start():
                    cfunc = name
            live[m.group(1)] = {'cfile': sub + '/' + fn, 'cfunc': cfunc}
    return live, remade_refs


def wrappers():
    res = {}
    for line in open(os.path.join(REPO, 'conf', 'wrappers_game.conf')):
        p = line.split()
        if len(p) >= 3 and p[1] == 'W':
            res[p[0]] = {'ret': p[2], 'args': p[3] if len(p) > 3 else ''}
    return res


def recent_upstream_names(days=30):
    """Function names mentioned in upstream commit subjects of last N days."""
    try:
        out = subprocess.run(
            ['git', '-C', REPO, 'log', 'upstream/master', '--since=%d.days' % days,
             '--format=%s'], capture_output=True, text=True, check=True).stdout
    except Exception:
        return set()
    return set(re.findall(r'(\w+)\(\)', out))


def ledger_state():
    st = {}
    if os.path.exists(LEDGER):
        for line in open(LEDGER):
            try:
                e = json.loads(line)
            except ValueError:
                continue
            st.setdefault(e['fn'], []).append(e)
    return st


def main():
    asm = parse_asm()
    live, remade_refs = parse_c()
    wr = wrappers()
    # Since 2026-11-22 we remake upstream-recent functions too (user
    # decision: our own versions, for an asm-free single player build);
    # set DECOMP_SKIP_UPSTREAM_RECENT=1 to exclude them again.
    recent = recent_upstream_names() if os.environ.get('DECOMP_SKIP_UPSTREAM_RECENT') else set()
    ledger = ledger_state()
    coverage = json.load(open(COVERAGE)) if os.path.exists(COVERAGE) else None
    fuzzed = fuzz_cases()
    targets = []
    for name, info in live.items():
        a = asm.get('ASM_' + name)
        if a is None:
            continue
        w = wr.get(name, {})
        internal_calls = sorted(c for c in a['calls']
                                if not c.startswith(('ac_', 'ASM_')) and c in asm)
        excluded = None
        if EXCLUDE_FILE_RE.search(info['cfile']) or EXCLUDE_NAME_RE.search(name):
            excluded = 'subsystem'
        elif name in recent:
            excluded = 'upstream-recent'
        cov_calls = coverage.get(name, {}).get('calls', 0) if coverage is not None else None
        if (excluded is None and cov_calls is not None and cov_calls < MIN_COVERAGE_CALLS
                and name not in fuzzed):
            excluded = 'low-coverage'
        attempts = ledger.get(name, [])
        if any(e.get('status') == 'merged' for e in attempts):
            continue
        # a 'reset' entry (blocker removed by hand) forgets earlier failures
        fails = 0
        for e in attempts:
            if e.get('status') == 'reset':
                fails = 0
            elif e.get('status') not in ('merged', 'base-build-failed'):
                fails += 1
        if fails >= 3:
            excluded = 'failed-3x'
        targets.append({
            'fn': name,
            'stage': 'routed',
            'cfile': info['cfile'],
            'asm_file': a['file'],
            'asm_line': a['line'],
            'addr': a['addr'],
            'insns': a['insns'],
            'internal_calls': internal_calls,
            'ret': w.get('ret'),
            'args': w.get('args'),
            'fails': fails,
            'coverage_calls': cov_calls,
            'excluded': excluded,
        })
    # Priority: leaves first (no calls into unrouted asm), then by size;
    # earlier failures go later
    targets.sort(key=lambda t: (t['excluded'] is not None, t['fails'],
                                len(t['internal_calls']) > 0, t['insns']))
    stats = {
        'asm_funcs': len([n for n in asm if n.startswith('ASM_')]),
        'routed_live': len(live),
        'shadowed': len(remade_refs),
        'queued': len([t for t in targets if not t['excluded']]),
    }
    json.dump({'stats': stats, 'targets': targets}, open(QUEUE, 'w'), indent=1)
    if '--print' in sys.argv:
        n = int(sys.argv[sys.argv.index('--print') + 1])
        print(json.dumps(stats))
        for t in targets[:n]:
            print('%-36s %4d insns  cov=%-8s calls=%-24s %s %s' % (
                t['fn'], t['insns'], t['coverage_calls'], ','.join(t['internal_calls'])[:24],
                t['cfile'], t['excluded'] or ''))


if __name__ == '__main__':
    main()
