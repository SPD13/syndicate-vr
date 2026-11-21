#!/usr/bin/env python3
"""Decompilation agent orchestrator.

For each target from queue.json: create a worktree from decomp/auto, run a
headless Claude Code session with /decompile-fn, verify the result
independently (build, difftest, unit tests, diff scope), then commit and
fast-forward decomp/auto, or archive the failed attempt.

Usage:
  python tools/decomp-agent/run.py --once [--fn NAME]
  python tools/decomp-agent/run.py --loop [--max-targets N]
Options:
  --model M         model for the agent (default: CLI default)
  --budget USD      max spend per target (default 5)
  --timeout MIN     wall clock limit per agent session (default 45)
  --keep            keep the worktree after the attempt
Stop a loop by creating tools/decomp-agent/STOP.
"""
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
WORKTREES = os.path.join(REPO, 'worktrees')
ATTEMPTS = os.path.join(HERE, 'attempts')
LEDGER = os.path.join(HERE, 'ledger.jsonl')
STOP = os.path.join(HERE, 'STOP')
BASE = 'decomp/auto'
BASH = shutil.which('bash') or 'bash'

ALLOWED_TOOLS = [
    'Read', 'Edit', 'Write', 'Grep', 'Glob',
    'Bash(tools/decomp-agent/*)',
    'Bash(python tools/decomp-agent/*)',
    'Bash(python -I tools/decomp-agent/*)',
    'Bash(git diff*)', 'Bash(git status*)',
]
# Files an attempt may change
ALLOWED_CHANGE_RE = re.compile(r'^src/[\w]+\.(c|h)$')


def log(msg):
    print('[%s] %s' % (datetime.datetime.now().strftime('%H:%M:%S'), msg), flush=True)


def run(cmd, cwd=REPO, timeout=None, check=False):
    p = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       timeout=timeout, errors='replace')
    if check and p.returncode != 0:
        raise RuntimeError('%s failed (%d): %s%s' % (' '.join(cmd), p.returncode,
                                                     p.stdout[-2000:], p.stderr[-2000:]))
    return p


def git(*args, cwd=REPO, check=True):
    return run(['git'] + list(args), cwd=cwd, check=check)


def script(cwd, name, *args, timeout=1800):
    try:
        return run([BASH, 'tools/decomp-agent/' + name] + list(args), cwd=cwd, timeout=timeout)
    except subprocess.TimeoutExpired as e:
        out = e.stdout.decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or '')
        return subprocess.CompletedProcess(e.cmd, 124, out + ' TIMEOUT after %ds' % timeout, '')


def ledger_add(entry):
    entry['time'] = datetime.datetime.now().isoformat(timespec='seconds')
    with open(LEDGER, 'a') as f:
        f.write(json.dumps(entry) + '\n')


def next_targets(n, only=None):
    run([sys.executable, '-I', os.path.join(HERE, 'queue.py')], check=True)
    q = json.load(open(os.path.join(HERE, 'queue.json')))['targets']
    if only:
        return [t for t in q if t['fn'] == only][:1]
    return [t for t in q if not t['excluded']][:n]


def cleanup_worktree(wt, branch):
    if os.path.isdir(wt):
        git('worktree', 'remove', '--force', wt, check=False)
        if os.path.isdir(wt):
            shutil.rmtree(wt, ignore_errors=True)
        git('worktree', 'prune', check=False)
    git('branch', '-D', branch, check=False)


def changed_files(wt):
    out = git('status', '--porcelain', cwd=wt).stdout
    files = []
    for line in out.splitlines():
        path = line[3:].strip()
        if path.endswith('pkg.m4') or path == 'RESULT.json':
            continue
        if line.startswith('??') and not path.startswith('src/'):
            # agent's scratch files; never committed, so drop them
            p = os.path.join(wt, path)
            if os.path.isdir(p):
                shutil.rmtree(p, ignore_errors=True)
            elif os.path.exists(p):
                os.remove(p)
            continue
        files.append(path)
    return files


def verify(wt, fn):
    """Independent verification. Returns (ok, status, details)."""
    files = changed_files(wt)
    if not files:
        return False, 'no-change', 'agent made no changes'
    bad = [f for f in files if not ALLOWED_CHANGE_RE.match(f)]
    if bad:
        return False, 'scope', 'unexpected files changed: %s' % ', '.join(bad)
    diff = git('diff', cwd=wt).stdout
    if not re.search(r'DIFFTEST_SHADOW(_RET)?\s*\(\s*%s\b' % re.escape(fn), diff):
        return False, 'scope', 'no DIFFTEST_SHADOW for %s in diff' % fn
    # The function must no longer call asm outside its reference function
    run([sys.executable, '-I', os.path.join(wt, 'tools/decomp-agent/queue.py')], cwd=wt)
    q = json.load(open(os.path.join(wt, 'tools/decomp-agent/queue.json')))
    if any(t['fn'] == fn for t in q['targets']):
        return False, 'scope', '%s still calls ASM_%s outside reference' % (fn, fn)
    p = script(wt, 'build.sh', '--difftest')
    if p.returncode != 0:
        return False, 'build', p.stdout[-3000:]
    p = script(wt, 'difftest_run.sh', fn, timeout=3600)
    verdict = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else 'ERROR no output'
    if p.returncode == 4:
        return False, 'unverified', p.stdout[-3000:]
    if p.returncode != 0:
        return False, 'mismatch' if p.returncode == 3 else 'difftest-error', p.stdout[-4000:]
    p2 = script(wt, 'build.sh', '--check')
    if p2.returncode != 0:
        return False, 'check', p2.stdout[-3000:]
    return True, 'pass', p.stdout[-3000:] + '\n' + verdict


def attempt(t, args):
    fn = t['fn']
    branch = 'decomp/try/' + fn
    wt = os.path.join(WORKTREES, fn)
    os.makedirs(WORKTREES, exist_ok=True)
    cleanup_worktree(wt, branch)
    git('worktree', 'add', '-q', '-b', branch, wt, BASE)
    # git ignores skip-worktree for new worktrees; keep pkg.m4 noise out
    log('%s: worktree ready, pre-building' % fn)
    t0 = time.time()
    p = script(wt, 'build.sh', '--difftest')
    if p.returncode != 0:  # once more: a sleeping PC or a stray lock can break the first try
        log('%s: base build failed (%d), retrying' % (fn, p.returncode))
        p = script(wt, 'build.sh', '--difftest')
    if p.returncode != 0:
        log('%s: base build failed!\n%s' % (fn, p.stdout[-2000:]))
        ledger_add({'fn': fn, 'status': 'base-build-failed'})
        cleanup_worktree(wt, branch)
        return False

    log('%s: running agent' % fn)
    prompt = '/decompile-fn %s' % fn
    cmd = ['claude', '-p', prompt, '--output-format', 'json',
           '--max-budget-usd', str(args.budget),
           '--allowedTools', ' '.join(ALLOWED_TOOLS)]
    if args.model:
        cmd += ['--model', args.model]
    agent_out = ''
    try:
        p = run(cmd, cwd=wt, timeout=args.timeout * 60)
        agent_out = p.stdout
    except subprocess.TimeoutExpired:
        agent_out = '{"error": "timeout"}'
    cost = None
    try:
        aj = json.loads(agent_out)
        cost = aj.get('total_cost_usd')
    except ValueError:
        aj = {}
    result = {}
    rpath = os.path.join(wt, 'RESULT.json')
    if os.path.exists(rpath):
        try:
            result = json.load(open(rpath))
        except ValueError:
            result = {'status': 'bad-json'}
    log('%s: agent says %s (cost %s)' % (fn, result.get('status'), cost))

    ok, status, details = verify(wt, fn)
    if result.get('status') == 'blocked' and not ok:
        status = 'blocked'
    entry = {'fn': fn, 'status': 'merged' if ok else status,
             'agent_status': result.get('status'), 'cost_usd': cost,
             'minutes': round((time.time() - t0) / 60, 1),
             'notes': result.get('notes', '')[:1000]}
    if ok:
        # The verdict line (last) holds the total; earlier lines are per scenario
        m = re.findall(r'compared=(\d+)', details)
        compared = m[-1] if m else '?'
        git('checkout', '-q', '--', *[f for f in git('ls-files', cwd=wt).stdout.split()
                                       if f.endswith('pkg.m4')], cwd=wt, check=False)
        git('add', '-A', 'src', cwd=wt)
        msg = ('syndwarsfx: Remade %s()\n\n'
               'Verified by difftest, %s compared calls.\n%s\n\n'
               'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n'
               % (fn, compared, wrap(result.get('notes', ''))))
        subprocess.run(['git', 'commit', '-q', '-F', '-'], cwd=wt, input=msg,
                       text=True, check=True)
        # Bring the attempt onto the current tip, then fast-forward
        head = git('rev-parse', BASE).stdout.strip()
        if git('merge-base', '--is-ancestor', head, branch, check=False).returncode != 0:
            p = git('rebase', BASE, cwd=wt, check=False)
            if p.returncode != 0:
                git('rebase', '--abort', cwd=wt, check=False)
                entry['status'] = 'rebase-conflict'
                ok = False
        if ok:
            cur = git('rev-parse', '--abbrev-ref', 'HEAD').stdout.strip()
            if cur == BASE:
                git('merge', '-q', '--ff-only', branch)
            else:
                git('fetch', '-q', '.', '%s:%s' % (branch, BASE))
            entry['commit'] = git('rev-parse', '--short', BASE).stdout.strip()
            entry['compared'] = compared
            log('%s: MERGED %s' % (fn, entry['commit']))
    if not ok:
        d = os.path.join(ATTEMPTS, fn, datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, 'patch.diff'), 'w').write(git('diff', cwd=wt).stdout)
        open(os.path.join(d, 'verify.txt'), 'w').write(details)
        open(os.path.join(d, 'agent.json'), 'w').write(agent_out)
        if result:
            json.dump(result, open(os.path.join(d, 'RESULT.json'), 'w'), indent=1)
        log('%s: FAILED (%s), saved to %s' % (fn, entry['status'], d))
    ledger_add(entry)
    if not args.keep:
        cleanup_worktree(wt, branch)
    return ok


def wrap(text, width=72):
    words, lines, cur = text.split(), [], ''
    for w in words:
        if len(cur) + len(w) + 1 > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + ' ' + w).strip()
    if cur:
        lines.append(cur)
    return '\n'.join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--once', action='store_true')
    ap.add_argument('--loop', action='store_true')
    ap.add_argument('--fn')
    ap.add_argument('--max-targets', type=int, default=1000)
    ap.add_argument('--model')
    ap.add_argument('--budget', type=float, default=5.0)
    ap.add_argument('--timeout', type=int, default=45)
    ap.add_argument('--keep', action='store_true')
    args = ap.parse_args()
    if git('status', '--porcelain', '--untracked-files=no').stdout.strip():
        log('main worktree has uncommitted changes; commit or stash them first')
        return 1
    done = 0
    tried = set()
    while True:
        if os.path.exists(STOP):
            log('STOP file found, ending')
            break
        targets = [t for t in next_targets(50, args.fn) if t['fn'] not in tried]
        if not targets:
            log('no targets left')
            break
        t = targets[0]
        tried.add(t['fn'])
        log('=== target %s (%d insns, %s)' % (t['fn'], t['insns'], t['cfile']))
        try:
            attempt(t, args)
        except Exception as e:  # keep the loop going; record and clean up
            log('%s: FAILED (exception %r)' % (t['fn'], e))
            ledger_add({'fn': t['fn'], 'status': 'exception', 'notes': repr(e)[:1000]})
            cleanup_worktree(os.path.join(WORKTREES, t['fn']), 'decomp/try/' + t['fn'])
        done += 1
        if args.once or args.fn or done >= args.max_targets:
            break
    return 0


if __name__ == '__main__':
    sys.exit(main())
