#!/usr/bin/env python3
"""Move the data of an asm file (.sx) into a generated C file.

Every label in the non-text sections (.data, .rodata, .bss) becomes a C
object of the same name, a packed struct of byte arrays and pointer fields,
defined in the same order (no_reorder) in .data, so that the layout stays
byte for byte as the asm had it: code relying on one label following
another keeps working. Pointers (`.long label+addend`) become pointer
fields; pointers to code labels of the asm (functions) are kept only in
difftest builds, where the asm exists, and are NULL otherwise.

The .sx keeps its code; its data sections are emptied and every reference
to a moved label becomes EXPORT_SYMBOL(label), the C symbol. Code labels
that the data points to are exported from the asm.

The generated objects are untyped byte layouts of a 32-bit build; giving
them real types is left for the 64-bit work.

Usage:
  python tools/decomp-agent/sx_data2c.py src/bat_s.sx src/bat_data.c [--apply]
Without --apply it only writes the C file to stdout-free check output.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))

SEC_RE = re.compile(r'^\s*\.(text|data|bss)\b|^\s*\.section\s+\.?(\w+)')
GLOBAL_RE = re.compile(r'^\s*GLOBAL\s*\(\s*(\w+)\s*\)')
LABEL_RE = re.compile(r'^(\w+):')
DIR_RE = re.compile(r'^\s*\.(\w+)\b\s*(.*)$')


def strip_comment(s):
    s = re.sub(r'/\*.*?\*/', '', s)
    i = s.find('#')
    # '#' starts a comment in GAS i386 unless inside a string
    if i >= 0 and '"' not in s[:i]:
        s = s[:i]
    return s.strip()


def parse_gas_string(s):
    """Bytes of a GAS string literal body (between the quotes)."""
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c != '\\':
            out += c.encode('latin-1')
            i += 1
            continue
        i += 1
        c = s[i]
        if c == 'x':
            j = i + 1
            while j < len(s) and s[j] in '0123456789abcdefABCDEF':
                j += 1
            out.append(int(s[i + 1:j], 16) & 0xFF)
            i = j
        elif c in '01234567':
            j = i
            while j < len(s) and j < i + 3 and s[j] in '01234567':
                j += 1
            out.append(int(s[i:j], 8) & 0xFF)
            i = j
        else:
            out.append({'n': 10, 't': 9, 'r': 13, 'b': 8, 'f': 12, '\\': 92,
                        '"': 34, "'": 39}[c])
            i += 1
    return bytes(out)


def split_args(a):
    out, cur, q = [], '', False
    for c in a:
        if c == '"':
            q = not q
        if c == ',' and not q:
            out.append(cur.strip())
            cur = ''
        else:
            cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def parse(path):
    """Returns (lines, items, text_labels).

    items: list of (section, kind, value, lineno); kinds 'label' (name,
    is_global), 'bytes', 'reloc' (target, addend), 'align' (n).
    """
    lines = open(path, encoding='utf-8', newline='').read().split('\n')
    # blank out block comments (also multi-line), keeping line numbers
    joined = '\n'.join(lines)
    joined = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), joined, flags=re.S)
    clean = joined.split('\n')
    sec = None
    items = []
    text_labels = set()
    for no, raw in enumerate(clean):
        raw = raw.rstrip('\r')
        m = SEC_RE.match(raw)
        if m:
            sec = m.group(1) or m.group(2)
            continue
        if sec is None:
            continue
        g = GLOBAL_RE.match(raw)
        lm = LABEL_RE.match(raw)
        if sec == 'text':
            if g:
                text_labels.add(g.group(1))
            elif lm:
                text_labels.add(lm.group(1))
            continue
        if g:
            items.append((sec, 'label', (g.group(1), True), no))
            continue
        if lm:
            items.append((sec, 'label', (lm.group(1), False), no))
            rest = strip_comment(raw[lm.end():])
            if rest:
                raise SystemExit('%s:%d: data after label' % (path, no + 1))
            continue
        s = strip_comment(raw)
        if not s:
            continue
        m = DIR_RE.match(s)
        if not m:
            raise SystemExit('%s:%d: cannot parse: %s' % (path, no + 1, raw))
        d, a = m.group(1), m.group(2).strip()
        if d in ('global', 'globl'):
            continue
        if d in ('ascii', 'string', 'asciz'):
            for part in split_args(a):
                b = parse_gas_string(part.strip()[1:-1])
                if d != 'ascii':
                    b += b'\0'
                items.append((sec, 'bytes', b, no))
        elif d in ('byte', 'short', 'word', 'long', 'int'):
            size = {'byte': 1, 'short': 2, 'word': 2, 'long': 4, 'int': 4}[d]
            for v in split_args(a):
                if re.match(r'^-?(0x[0-9a-fA-F]+|\d+)$', v):
                    n = int(v, 0) & ((1 << (8 * size)) - 1)
                    items.append((sec, 'bytes', n.to_bytes(size, 'little'), no))
                else:
                    if size != 4:
                        raise SystemExit('%s:%d: symbol in .%s' % (path, no + 1, d))
                    mm = re.match(r'^(\w+)\s*(?:([+-])\s*(0x[0-9a-fA-F]+|\d+))?$', v)
                    if not mm:
                        raise SystemExit('%s:%d: bad operand %s' % (path, no + 1, v))
                    add = int(mm.group(3), 0) if mm.group(3) else 0
                    if mm.group(2) == '-':
                        add = -add
                    items.append((sec, 'reloc', (mm.group(1), add), no))
        elif d in ('fill', 'space', 'zero', 'skip'):
            p = [int(x, 0) for x in split_args(a)]
            if d == 'fill':
                n, size, val = p[0], (p[1] if len(p) > 1 else 1), (p[2] if len(p) > 2 else 0)
                unit = (val & ((1 << (8 * min(size, 4))) - 1)).to_bytes(min(size, 4), 'little') + \
                    b'\0' * (size - min(size, 4))
                items.append((sec, 'bytes', unit * n, no))
            else:
                items.append((sec, 'bytes', bytes([p[1] if len(p) > 1 else 0]) * p[0], no))
        elif d in ('align', 'balign', 'p2align'):
            n = int(split_args(a)[0], 0)
            if d == 'p2align':
                n = 1 << n
            items.append((sec, 'align', n, no))
        else:
            raise SystemExit('%s:%d: unknown directive .%s' % (path, no + 1, d))
    return lines, items, text_labels


def build_objects(items):
    """Group items by section into labelled objects with padding resolved.

    Returns list of (section, [objects]); object = dict(name, glob, parts)
    with parts [('b', bytes) | ('p', target, addend)]. Data before the first
    label of a section gets an anonymous object.
    """
    secs = []
    cur_sec = None
    for sec, kind, val, no in items:
        if sec != cur_sec:
            # a section directive can repeat; continue its offset
            found = [s for s in secs if s[0] == sec]
            if found:
                entry = found[0]
            else:
                entry = [sec, [], 0]
                secs.append(entry)
            cur_sec = sec
        objs, off = entry[1], entry[2]
        if kind == 'label':
            objs.append({'name': val[0], 'glob': val[1], 'parts': [], 'off': off})
            continue
        if not objs:
            objs.append({'name': None, 'glob': False, 'parts': [], 'off': off})
        o = objs[-1]
        if kind == 'bytes':
            o['parts'].append(('b', val))
            off += len(val)
        elif kind == 'reloc':
            o['parts'].append(('p', val[0], val[1]))
            off += 4
        elif kind == 'align':
            pad = (-off) % val
            if pad:
                o['parts'].append(('b', b'\0' * pad))
                off += pad
        entry[2] = off
    return [(s[0], s[1]) for s in secs]


def c_name(obj, sec, idx, base):
    return obj['name'] or 'sxd_%s_%s_anon%d' % (base, sec, idx)


def gen_c(sx_rel, secs, text_labels, base):
    out = []
    out.append('/' + '*' * 78 + '/')
    out.append('// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.')
    out.append('/' + '*' * 78 + '/')
    out.append('/** @file %s_data.c' % base)
    out.append(' *     Data of %s, moved to C.' % sx_rel)
    out.append(' * @par Purpose:')
    out.append(' *     Generated by tools/decomp-agent/sx_data2c.py; do not edit by hand,')
    out.append(' *     regenerate. Every label of the asm data sections is an object of')
    out.append(' *     the same name, in the same order and without padding, so the')
    out.append(' *     layout is the one of the asm. The asm, kept for difftest builds,')
    out.append(' *     references these objects.')
    out.append(' * @par Comment:')
    out.append(' *     Byte layouts of the 32-bit build; real types are future work.')
    out.append(' *     Pointers to asm code exist only in difftest builds.')
    out.append(' */')
    out.append('/' + '*' * 78 + '/')
    out.append('#include "bftypes.h"')
    out.append('')
    out.append('#ifdef HAVE_CONFIG_H')
    out.append('# include "config.h"')
    out.append('#endif')
    out.append('')
    out.append('#define SXD_OBJ __attribute__((section(".data"), no_reorder, used))')
    out.append('')
    names = []
    defs = []
    code_targets = set()
    for sec, objs in secs:
        for i, o in enumerate(objs):
            n = c_name(o, sec, i, base)
            names.append(n)
            fields, inits = [], []
            for k, p in enumerate(o['parts']):
                pass
            # merge consecutive byte parts
            merged = []
            for p in o['parts']:
                if p[0] == 'b' and merged and merged[-1][0] == 'b':
                    merged[-1] = ('b', merged[-1][1] + p[1])
                else:
                    merged.append(p)
            for k, p in enumerate(merged):
                if p[0] == 'b':
                    if not p[1]:
                        continue
                    fields.append('    ubyte d%d[%d];' % (k, len(p[1])))
                    if any(p[1]):
                        rows = []
                        for j in range(0, len(p[1]), 16):
                            rows.append('        ' + ', '.join('0x%02x' % x for x in p[1][j:j + 16]) + ',')
                        inits.append('    {\n' + '\n'.join(rows) + '\n    },')
                    else:
                        inits.append('    {0},')
                else:
                    tgt, add = p[1], p[2]
                    fields.append('    void *p%d;' % k)
                    if tgt in text_labels:
                        code_targets.add(tgt)
                        expr = '(void *)((ubyte *)%s + %d)' % (tgt, add) if add else '(void *)%s' % tgt
                        inits.append('#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST\n    %s,\n#else\n    NULL,\n#endif' % expr)
                    else:
                        expr = '(void *)((ubyte *)&%s + %d)' % (tgt, add) if add else '(void *)&%s' % tgt
                        inits.append('    %s,' % expr)
            if not fields:
                # a label with no data of its own (followed by another label)
                fields.append('    ubyte d0[0];')
            # aligned(1) is user-set alignment, so GCC does not raise it
            # (x86 aligns data objects by size otherwise)
            attr = 'SXD_OBJ __attribute__((aligned(1)))'
            if i == 0:
                attr = 'SXD_OBJ __attribute__((aligned(32)))'
            defs.append('struct SxD_%s {\n%s\n} __attribute__((packed));' % (n, '\n'.join(fields)))
            body = '\n'.join(inits)
            defs.append('struct SxD_%s %s %s = {\n%s\n};\n' % (n, n, attr, body))
    out.append('/* Code of the asm that the data points to (difftest builds only) */')
    out.append('#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST')
    for t in sorted(code_targets):
        out.append('extern ubyte %s[];' % t)
    out.append('#endif')
    out.append('')
    # forward declarations so that pointers may refer to later objects
    struct_defs = [d for d in defs if d.startswith('struct SxD_') and d.endswith(';') and '=' not in d]
    obj_defs = [d for d in defs if d not in struct_defs]
    out += struct_defs
    out.append('')
    for n in names:
        out.append('extern struct SxD_%s %s;' % (n, n))
    out.append('')
    out += obj_defs
    out.append('/' + '*' * 78 + '/')
    return '\n'.join(out) + '\n', names, code_targets


def rewrite_sx(lines, secs, code_targets):
    moved = set()
    for sec, objs in secs:
        for o in objs:
            if o['name']:
                moved.add(o['name'])
    out = []
    sec = None
    pat = re.compile(r'(?<![\w(])(%s)\b' % '|'.join(sorted(moved, key=len, reverse=True))) if moved else None
    for raw in lines:
        m = SEC_RE.match(raw)
        if m:
            sec = m.group(1) or m.group(2)
            out.append(raw)
            if sec != 'text':
                out.append('/* data moved to C (sx_data2c.py) */')
            continue
        if sec is not None and sec != 'text':
            continue  # drop data
        if sec == 'text':
            lm = LABEL_RE.match(raw)
            if lm and lm.group(1) in code_targets:
                n = lm.group(1)
                out.append('.global EXPORT_SYMBOL(%s);' % n)
                out.append('EXPORT_SYMBOL(%s):' % n)
                out.append(raw)
                continue
            if pat and not raw.lstrip().startswith(('/*', '*')):
                code, cmt = raw, ''
                k = raw.find('/*')
                if k >= 0:
                    code, cmt = raw[:k], raw[k:]
                code = pat.sub(lambda mm: 'EXPORT_SYMBOL(%s)' % mm.group(1), code)
                # undo double wrapping of already exported references
                code = re.sub(r'EXPORT_SYMBOL\s*\(\s*EXPORT_SYMBOL\((\w+)\)\s*\)', r'EXPORT_SYMBOL(\1)', code)
                raw = code + cmt
        out.append(raw)
    return '\n'.join(out)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    sx, cfile = args[0], args[1]
    base = os.path.splitext(os.path.basename(cfile))[0]
    if base.endswith('_data'):
        base = base[:-5]
    lines, items, text_labels = parse(os.path.join(REPO, sx))
    secs = build_objects(items)
    text, names, code_targets = gen_c(sx, secs, text_labels, base)
    for sec, objs in secs:
        size = sum(len(p[1]) if p[0] == 'b' else 4 for o in objs for p in o['parts'])
        print('%s: %d objects, %d bytes' % (sec, len(objs), size))
    print('pointers to code:', ', '.join(sorted(code_targets)) or '-')
    if '--apply' in sys.argv:
        open(os.path.join(REPO, cfile), 'w', encoding='utf-8', newline='\n').write(text)
        new = rewrite_sx(lines, secs, code_targets)
        open(os.path.join(REPO, sx), 'w', encoding='utf-8', newline='').write(new)
        print('wrote', cfile, 'and rewrote', sx)
    else:
        sys.stdout.write('')
    # layout listing for checks: name offset
    with open(os.path.join(REPO, 'build', base + '_layout.txt') if os.path.isdir(os.path.join(REPO, 'build')) else os.devnull, 'w') as f:
        for sec, objs in secs:
            for o in objs:
                f.write('%s %s %d\n' % (sec, o['name'], o['off']))
    return 0


if __name__ == '__main__':
    sys.exit(main())
