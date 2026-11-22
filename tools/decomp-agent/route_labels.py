#!/usr/bin/env python3
"""Route internal asm labels through C wrappers, so that their callers can be
remade.

For each label: GLOBAL_FUNC(ASM_<name>) in the .sx, asm calls go through
ac_<name>, a line in conf/wrappers_game.conf and a C wrapper that calls the
asm with the registers it takes. As in upstream's "Routed ... through C".

Arguments are found by register liveness over the label's control flow (a
call inside counts as reading all four argument registers, so values passed
through are kept). The return is eax when a caller reads it. Registers the
label does not save are declared clobbered in the wrapper. Labels are skipped
when unsafe to wrap: stack arguments, jumps out of the body, results in flags
or in registers other than eax, ebp not saved.

Usage:
  python tools/decomp-agent/route_labels.py --list         # analysis only
  python tools/decomp-agent/route_labels.py --apply [names...]
  --sx src/other.sx works on another asm file (default src/swars.sx).
Check the result with statetrace.sh against the base build: routing must not
change behaviour, and the per-function difftest cannot see wrapper mistakes.
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
SX = os.path.join(REPO, 'src', 'swars.sx')
CONF = os.path.join(REPO, 'conf', 'wrappers_game.conf')

ARGREGS = ['eax', 'edx', 'ebx', 'ecx']
CONSTR = {'eax': 'a', 'edx': 'd', 'ebx': 'b', 'ecx': 'c'}
ALLREGS = ARGREGS + ['esi', 'edi', 'ebp']
SUBREG = {}
for r in ['a', 'b', 'c', 'd']:
    SUBREG['e%sx' % r] = ('e%sx' % r, True)
    SUBREG['%sx' % r] = ('e%sx' % r, False)
    SUBREG['%sl' % r] = ('e%sx' % r, False)
    SUBREG['%sh' % r] = ('e%sx' % r, False)
for r in ['si', 'di', 'bp']:
    SUBREG['e' + r] = ('e' + r, True)
    SUBREG[r] = ('e' + r, False)
SUBREG['esp'] = ('esp', True)
SUBREG['sp'] = ('esp', False)

LABEL_RE = re.compile(r'^\s*GLOBAL_FUNC\s*\(\s*(\w+)\s*\)|^(\w+):')
LOCAL_RE = re.compile(r'^\s*(jump_\w+|data_\w+|loc_\w+):')
REG_RE = re.compile(r'%(\w+)')
JCC = re.compile(r'^j(?!mp)\w+$')


def reg_of(name):
    return SUBREG.get(name)


def split_operands(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def regs_in(op):
    rs = set()
    for m in REG_RE.finditer(op):
        r = reg_of(m.group(1))
        if r:
            rs.add(r[0])
    return rs


def parse_insn(line):
    code = line.split('#')[0].split('/*')[0].strip()
    if not code or code.endswith(':'):
        return None
    parts = code.split(None, 1)
    mn = parts[0]
    ops = split_operands(parts[1]) if len(parts) > 1 else []
    if mn in ('rep', 'repe', 'repne', 'repz', 'repnz') and ops:
        sub = ops[0].split(None, 1)
        mn = mn + ' ' + sub[0]
        ops = split_operands(sub[1]) if len(sub) > 1 else []
    return mn, ops


def use_def(mn, ops):
    """Registers read and fully written by one instruction (conservative:
    unknown instructions read all registers they name and write none)."""
    use, dfn = set(), set()
    base = mn.split()[-1]

    def is_reg(op):
        return op.startswith('%') and reg_of(op[1:]) is not None

    def full(op):
        return reg_of(op[1:])[1]

    for op in ops[:-1] if ops else []:
        use |= regs_in(op)
    if ops:
        dst = ops[-1]
        if not is_reg(dst):
            use |= regs_in(dst)  # address registers
    if base.startswith('call'):
        use |= set(ARGREGS) | {'esi', 'edi', 'ebp'}
        return use, dfn
    if mn.startswith('rep'):
        use |= {'ecx', 'esi', 'edi', 'eax'}
        return use, dfn
    if base in ('cltd', 'cdq'):
        return {'eax'}, {'edx'}
    if base in ('cwtl', 'cwde'):
        return {'eax'}, {'eax'}
    if base in ('cwtd', 'cwd'):
        return {'eax', 'edx'}, set()
    if base in ('ret', 'leave', 'nop', 'jmp') or JCC.match(base):
        return use, dfn
    if base in ('div', 'divl', 'divw', 'divb', 'idiv', 'idivl', 'idivw', 'idivb',
                'mul', 'mull', 'mulw', 'mulb') or (base.startswith('imul') and len(ops) == 1):
        use |= {'eax', 'edx'}
        for op in ops:
            use |= regs_in(op)
        return use, dfn  # defs of eax/edx only partial for byte forms; keep live
    if not ops:
        return use, dfn
    dst = ops[-1]
    if not is_reg(dst):
        if base in ('push', 'pushl'):
            pass
        return use, dfn
    r = reg_of(dst[1:])[0]
    isfull = full(dst)
    if base in ('push', 'pushl', 'cmp', 'cmpl', 'cmpw', 'cmpb', 'test', 'testl',
                'testw', 'testb', 'bt', 'btl'):
        use.add(r)
        return use, dfn
    if base in ('pop', 'popl'):
        if isfull:
            dfn.add(r)
        else:
            use.add(r)
        return use, dfn
    if base in ('mov', 'movl', 'movw', 'movb') or base.startswith(('movz', 'movs', 'lea')) \
            or (base.startswith('set')) or (base.startswith('imul') and len(ops) == 3):
        if base.startswith(('movz', 'movs', 'lea')) and isfull:
            dfn.add(r)
        elif isfull and base in ('mov', 'movl'):
            dfn.add(r)
        elif base.startswith('imul') and isfull:
            dfn.add(r)
        else:
            use.add(r)
        return use, dfn
    if base in ('xor', 'xorl', 'sub', 'subl', 'sbb') and len(ops) == 2 and ops[0] == ops[1]:
        if isfull and base != 'sbb':
            return set(), {r}
        use.add(r)
        return use, dfn
    if base in ('xchg', 'xchgl'):
        use |= regs_in(ops[0]) | {r}
        return use, dfn
    # two-operand ALU, shifts, inc/dec/neg/not: read and write dst
    use.add(r)
    return use, dfn


NO_WRITE = ('cmp', 'cmpl', 'cmpw', 'cmpb', 'test', 'testl', 'testw', 'testb',
            'push', 'pushl', 'bt', 'btl', 'jmp', 'ret')


def written_regs(body):
    """Registers an instruction list may change, fully or partly
    (conservative: calls to anything but a C wrapper change all)."""
    w = set()
    for _, mn, ops in body:
        base = mn.split()[-1]
        if base.startswith('call'):
            t = ops[0] if ops else ''
            if t.startswith('ac_'):
                w.add('eax')  # C via a wrapper keeps the rest
            else:
                w |= set(ALLREGS)
            continue
        if mn.startswith('rep') or base.startswith(('movs', 'stos', 'lods', 'scas', 'cmps')) \
                and not base.startswith(('movsb', 'movsw', 'movsl', 'movsx')) or base in ('movsb', 'movsw', 'movsl'):
            w |= {'ecx', 'esi', 'edi', 'eax'}
            continue
        if base in ('cltd', 'cdq', 'cwtd', 'cwd'):
            w.add('edx')
            continue
        if base in ('cwtl', 'cwde'):
            w.add('eax')
            continue
        if base.startswith(('div', 'idiv', 'mul')) or (base.startswith('imul') and len(ops) == 1):
            w |= {'eax', 'edx'}
            continue
        if base in ('xchg', 'xchgl', 'xchgw', 'xchgb'):
            for op in ops:
                if op.startswith('%') and reg_of(op[1:]):
                    w.add(reg_of(op[1:])[0])
            continue
        if base in NO_WRITE or JCC.match(base) or not ops:
            continue
        dst = ops[-1]
        if dst.startswith('%') and reg_of(dst[1:]):
            w.add(reg_of(dst[1:])[0])
    return w


def parse_asm():
    lines = open(SX, encoding='utf-8', newline='').read().split('\n')
    funcs = {}
    order = []
    cur = None
    for i, line in enumerate(lines):
        m = LABEL_RE.match(line)
        if m and not (m.group(2) or '').startswith(('jump_', 'data_', 'loc_')):
            name = m.group(1) or m.group(2)
            cur = {'name': name, 'start': i, 'lines': []}
            funcs[name] = cur
            order.append(name)
            continue
        if cur is not None:
            cur['lines'].append((i, line))
    return lines, funcs, order


def analyse(f, funcs, callers, force=False):
    """Returns dict with args, ret, saved, skip reason."""
    body = []
    labels = {}
    for i, line in f['lines']:
        m = LOCAL_RE.match(line)
        if m:
            labels[m.group(1)] = len(body)
            continue
        ins = parse_insn(line)
        if ins:
            body.append((i, ins[0], ins[1]))
    if not body:
        return {'skip': 'empty'}
    # entry pushes
    saved = []
    k = 0
    while k < len(body) and body[k][1] in ('push', 'pushl') and body[k][2] and body[k][2][0].startswith('%'):
        saved.append(body[k][2][0][1:])
        k += 1
    frame = 4 * len(saved)
    if k < len(body) and body[k][1] in ('sub', 'subl') and body[k][2][1] == '%esp':
        frame += int(body[k][2][0].lstrip('$'), 0)
    # every ret must pop the saved registers; stack args the same on all
    nstack = None
    for n, (i, mn, ops) in enumerate(body):
        if mn == 'ret':
            ns = int(ops[0].lstrip('$'), 0) // 4 if ops else 0
            if nstack is not None and ns != nstack:
                return {'skip': 'rets pop different stack sizes'}
            nstack = ns
            pops = []
            j = n - 1
            while j >= 0 and body[j][1] in ('pop', 'popl'):
                pops.append(body[j][2][0][1:])
                j -= 1
            if sorted(pops) != sorted(saved):
                return {'skip': 'ret without matching pops'}
        if mn == 'jmp' or JCC.match(mn):
            t = ops[0] if ops else ''
            if t.startswith('*') or t not in labels:
                return {'skip': 'jump out of body (%s)' % t}
        for op in ops:
            m = re.match(r'^(-?0x[0-9a-fA-F]+|-?\d+)\(%esp\)$', op)
            if m and int(m.group(1), 0) >= frame + 4 + 128:
                return {'skip': 'reads caller stack'}
    if 'ebp' not in saved and any('ebp' in use_def(mn, ops)[1] | regs_in(','.join(ops))
                                  for _, mn, ops in body if mn not in ('push', 'pushl', 'pop', 'popl')):
        return {'skip': 'ebp not saved'}
    # liveness: successors
    n = len(body)
    succ = []
    for idx, (i, mn, ops) in enumerate(body):
        s = []
        if mn == 'ret':
            pass
        elif mn == 'jmp':
            s.append(labels[ops[0]])
        elif JCC.match(mn):
            s += [labels[ops[0]], idx + 1]
        else:
            s.append(idx + 1)
        succ.append([x for x in s if x < n])
    ud = [use_def(mn, ops) for _, mn, ops in body]
    live = [set() for _ in range(n + 1)]
    changed = True
    while changed:
        changed = False
        for idx in range(n - 1, -1, -1):
            out = set()
            for s in succ[idx]:
                out |= live[s]
            new = ud[idx][0] | (out - ud[idx][1])
            if new != live[idx]:
                live[idx] = new
                changed = True
    entry = live[0]
    nargs = 0
    for k2, r in enumerate(ARGREGS):
        if r in entry:
            nargs = k2 + 1
    if nstack:
        nargs = 4  # stack arguments come after all four registers
    written = written_regs(body)
    clobbered = [r for r in ALLREGS if r in written and r not in saved]
    # callers: outputs other than eax, flags, return use
    ret_used = False
    for (cname, cbody) in callers:
        for ci, (i, mn, ops) in enumerate(cbody):
            if mn.startswith('call') and ops and ops[0] == f['name']:
                after = cbody[ci + 1:ci + 12]
                if after and (JCC.match(after[0][1]) or after[0][1].startswith(('set', 'adc', 'sbb', 'cmov'))):
                    return {'skip': 'flags used after call in %s' % cname}
                written = set()
                for (_, amn, aops) in after:
                    if amn.startswith(('call', 'jmp', 'ret')) or JCC.match(amn):
                        u, d = use_def(amn, aops) if not amn.startswith('call') else (set(ARGREGS), set())
                        for r in u - written:
                            if r == 'eax':
                                ret_used = True
                            elif r in clobbered and not amn.startswith('call'):
                                if not force:
                                    return {'skip': 'caller %s reads %s after call' % (cname, r)}
                        break
                    u, d = use_def(amn, aops)
                    for r in u - written:
                        if r == 'eax':
                            ret_used = True
                        elif r in clobbered and r in ('edx', 'ebx', 'ecx', 'esi', 'edi'):
                            if not force:
                                return {'skip': 'caller %s reads %s after call' % (cname, r)}
                    written |= d
    return {'args': ARGREGS[:nargs], 'ret': ret_used, 'saved': saved,
            'clobbered': clobbered, 'nstack': nstack or 0}


def callers_of(name, funcs):
    out = []
    for f in funcs.values():
        cbody = []
        hit = False
        for i, line in f['lines']:
            ins = parse_insn(line)
            if ins:
                cbody.append((i, ins[0], ins[1]))
                if ins[0].startswith('call') and ins[1] and ins[1][0] == name:
                    hit = True
        if hit:
            out.append((f['name'], cbody))
    return out


def wrapper_c(name, a):
    nall = len(a['args']) + a['nstack']
    params = ', '.join('int a%d' % (k + 1) for k in range(nall)) or 'void'
    rtype = 'int' if a['ret'] else 'void'
    lines = ['/** Routed asm (internal label). Arguments and return value are the',
             ' * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when',
             ' * remade. Registers it does not preserve are declared clobbered.',
             ' */',
             '%s %s(%s)' % (rtype, name, params),
             '{']
    decl, outs, clob = [], [], []
    for k, r in enumerate(ARGREGS):
        var = 'r' + r[1]
        if r in a['args']:
            decl.append('%s = a%d' % (var, k + 1))
            outs.append('"+%s" (%s)' % (CONSTR[r], var))
        elif r == 'eax' and a['ret']:
            decl.append('%s = 0' % var)
            outs.append('"+a" (%s)' % var)
        elif r in a['clobbered']:
            clob.append('"%s"' % r)
    for r in ('esi', 'edi'):
        if r in a['clobbered'] and not (r == 'esi' and a['nstack']):
            clob.append('"%s"' % r)
    clob += ['"cc"', '"memory"']
    if decl:
        lines.append('    int %s;' % ', '.join(decl))
    pushes = ''
    if a['nstack']:
        # pushed through a register holding the array, see bul_path_end()
        lines.append('    int stkargs[%d];' % a['nstack'])
        lines.append('    int *p_stk = stkargs;')
        for k in range(a['nstack']):
            lines.append('    stkargs[%d] = a%d;' % (k, 5 + k))
        outs.append('"+S" (p_stk)')
        idx = len(outs) - 1
        for k in range(a['nstack'] - 1, -1, -1):
            pushes += '"push %d(%%%d)\\n"' % (4 * k, idx)
    lines.append('    asm volatile (%s"call ASM_%s\\n"' % (pushes, name))
    lines.append('        : %s : : %s);' % (', '.join(outs), ', '.join(clob)))
    if a['ret']:
        lines.append('    return ra;')
    lines.append('}')
    return '\n'.join(lines) + '\n\n'


def conf_line(name, a):
    tabs = '\t' * max(1, (24 - len(name) + 3) // 4)
    return '%s%sW\t%s\t%s\n' % (name, tabs, 'i' if a['ret'] else 'v',
                               'i' * (len(a['args']) + a['nstack']))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--force', action='append', default=[])
    ap.add_argument('--sx', default=None, help='asm file (default src/swars.sx)')
    ap.add_argument('names', nargs='*')
    args = ap.parse_args()
    if args.sx:
        global SX
        SX = os.path.join(REPO, args.sx)
    lines, funcs, order = parse_asm()
    q = json.load(open(os.path.join(HERE, 'queue.json')))['targets']
    # internal labels called by queued (or low-coverage) targets
    want = {}
    for t in q:
        if t['excluded'] not in (None, 'low-coverage', 'failed-3x'):
            continue
        f = funcs.get('ASM_' + t['fn'])
        if not f:
            continue
        # the body may go on past plain labels (e.g. func_67864 inside
        # set_static_on_fire), up to the next global function
        body = list(f['lines'])
        k = order.index(f['name']) + 1
        while k < len(order) and not order[k].startswith('ASM_'):
            body += funcs[order[k]]['lines']
            k += 1
        for i, line in body:
            ins = parse_insn(line)
            if ins and ins[0].startswith('call') and ins[1]:
                c = ins[1][0]
                if c in funcs and not c.startswith(('ac_', 'ASM_')):
                    want.setdefault(c, []).append(t)
    if args.names:
        want = {k: v for k, v in want.items() if k in args.names}
    csrc = ''
    for top, dirs, files in os.walk(REPO):
        dirs[:] = [d for d in dirs if d not in ('build', 'worktrees', '.git', 'tools')]
        for fn in files:
            if fn.endswith(('.c', '.h')):
                csrc += open(os.path.join(top, fn), encoding='utf-8', errors='replace').read()
    plan = {}
    for name in sorted(want):
        if re.search(r'\b%s\s*\(' % re.escape(name), csrc):
            print('SKIP  %-34s already a C function; the asm copy is still called' % name)
            continue
        a = analyse(funcs[name], funcs, callers_of(name, funcs), name in args.force)
        if 'skip' in a:
            print('SKIP  %-34s %s' % (name, a['skip']))
            continue
        plan[name] = a
        print('ROUTE %-34s args=%s ret=%s saved=%s' % (name, ','.join(a['args']) or '-',
              a['ret'], ','.join(a['saved'])))
    if not args.apply:
        return 0
    # .sx: label and calls
    text = '\n'.join(lines)
    for name in plan:
        text, n1 = re.subn(r'(?m)^%s:' % re.escape(name), 'GLOBAL_FUNC(ASM_%s)' % name, text)
        text, n2 = re.subn(r'(?m)^(\s+call\s+)%s\s*$' % re.escape(name), r'\1ac_%s' % name, text)
        assert n1 == 1, (name, n1)
    open(SX, 'w', encoding='utf-8', newline='').write(text)
    # conf
    conf = open(CONF, encoding='utf-8', newline='').read()
    if not conf.endswith('\n'):
        conf += '\n'
    conf += ''.join(conf_line(n, plan[n]) for n in plan)
    open(CONF, 'w', encoding='utf-8', newline='').write(conf)
    # C wrappers, before the first blocked caller in its file
    for name, a in plan.items():
        t = want[name][0]
        path = os.path.join(REPO, t['cfile'])
        s = open(path, encoding='utf-8', newline='').read()
        m = re.search(r'(?m)^(?:static\s+)?[\w\s\*]+?\b%s\s*\([^;{]*\)\s*\{' % re.escape(t['fn']), s)
        assert m, (name, t['fn'], t['cfile'])
        # include the comment block right above the function
        start = m.start()
        pre = s[:start]
        cm = re.search(r'/\*\*(?:(?!\*/).)*\*/\s*$', pre, re.S)
        if cm:
            start = cm.start()
        s = s[:start] + wrapper_c(name, a) + s[start:]
        open(path, 'w', encoding='utf-8', newline='').write(s)
    print('routed %d labels' % len(plan))
    return 0


if __name__ == '__main__':
    sys.exit(main())
