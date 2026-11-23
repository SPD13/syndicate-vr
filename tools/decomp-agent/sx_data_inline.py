"""Placing typed definitions of asm data labels into their C files.

Used by sx_data_typed.py --inline: every variable goes to the C file that
owns it (the .c of the header that declares it, or the .c file that
declares it), in a block after its extern declaration (or after the
includes), guarded so that difftest builds use the data file instead.
Running again replaces the block of the same data file.
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

# label -> C file, where the rule picks a wrong one
OWNERS = {
    'selected_triangulation_no': 'bfariadne/src/trstate.c',
    'smack_draw_callback': 'bfsmacklib/src/bfsmack.c',
    'redo_scanner': 'src/scanner.c',
    'redo_scanner_last': 'src/scanner.c',
    'redo_scanner_first': 'src/scanner.c',
    # swrendersoft has its own config.h, without ENABLE_DIFFTEST
    'low_trans_grey_bright_limit': 'src/specblit.c',
}
GUARD = '#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)'
# the DOS network asm and its data are built with --enable-dos-network
GUARD_NET = '#if !(defined(ENABLE_DOS_NETWORK) && ENABLE_DOS_NETWORK)'


def guard_of(data_path):
    b = os.path.basename(data_path)
    return GUARD_NET if b.startswith(('net', 'radica')) else GUARD


BEGIN = '/* Variables of the asm data (%s), for builds without the asm;'
END_TAG = '#endif /* typed asm data */'


def uses(path, name):
    t = open(os.path.join(REPO, path), encoding='utf-8', errors='replace').read()
    return len(re.findall(r'\b%s\b' % re.escape(name), t))


def owner_of(it):
    if it['name'] in OWNERS:
        return OWNERS[it['name']]
    for p in it['decl_in']:
        if p.endswith('.h'):
            c = p[:-2] + '.c'
            if os.path.exists(os.path.join(REPO, c)):
                return c
    cs = [p for p in it['decl_in'] if p.endswith('.c')]
    if cs:
        return sorted(cs, key=lambda p: (-uses(p, it['name']), p))[0]
    return None


def inline_out(data_path, lines, report):
    groups = {}
    for it, d in lines:
        o = owner_of(it)
        if o is None or not os.path.exists(os.path.join(REPO, o)):
            report.append('NO OWNER %s (%s)' % (it['name'], o))
            continue
        groups.setdefault(o, []).append((it, d))
    begin = BEGIN % os.path.basename(data_path)
    guard = guard_of(data_path)
    builds = 'DOS network builds' if guard == GUARD_NET else 'difftest builds'
    block_re = re.compile(r'\n' + re.escape(guard) + r'\n' + re.escape(begin) + r'.*?\n' +
                          re.escape(END_TAG) + r'\n', re.S)
    # drop the blocks of an earlier run from every file, owners may change
    for top in ('src', 'bfariadne/src', 'bfsmacklib/src', 'swrendersoft/src', 'bflibrary/src'):
        for fn in os.listdir(os.path.join(REPO, top)):
            if not fn.endswith('.c'):
                continue
            full = os.path.join(REPO, top, fn)
            raw = open(full, encoding='utf-8', newline='').read()
            if begin not in raw:
                continue
            crlf = '\r\n' in raw
            t = block_re.sub('', raw.replace('\r\n', '\n'))
            open(full, 'w', encoding='utf-8', newline='').write(t.replace('\n', '\r\n') if crlf else t)
    for path, items in sorted(groups.items()):
        full = os.path.join(REPO, path)
        raw = open(full, encoding='utf-8', newline='').read()
        crlf = '\r\n' in raw
        t = raw.replace('\r\n', '\n')
        m = re.search(r'\n' + re.escape(guard) + r'\n' + re.escape(begin) + r'.*?\n' +
                      re.escape(END_TAG) + r'\n', t, re.S)
        if m:
            t = t[:m.start()] + t[m.end():]
        ls = t.split('\n')
        names = set(it['name'] for it, _ in items)
        pos = None
        depth = 0
        for i, line in enumerate(ls):
            if depth == 0 and line.startswith('extern ') and \
                    any(re.search(r'\b%s\b' % re.escape(n), line) for n in names):
                pos = i
                while not ls[pos].rstrip().endswith(';'):
                    pos += 1
            depth += line.count('{') - line.count('}')
        if pos is None:
            for i, line in enumerate(ls):
                if line.startswith('#include'):
                    pos = i
        blk = ['', guard, begin,
               ' * %s take the byte layout of the asm from that file. */' % builds]
        blk += [d for it, d in items]
        blk.append(END_TAG)
        ls[pos + 1:pos + 1] = blk
        t = '\n'.join(ls)
        open(full, 'w', encoding='utf-8', newline='').write(t.replace('\n', '\r\n') if crlf else t)
        print('%-40s %d variables' % (path, len(items)))
