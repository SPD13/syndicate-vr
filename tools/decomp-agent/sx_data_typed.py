#!/usr/bin/env python3
"""Typed C definitions of the asm data that the C code uses.

The generated `*_data.c` files (sx_data2c.py) hold the asm data as packed
byte layouts, which the asm of difftest builds needs. Builds without the
asm need only the labels that C declares `extern`, and need them with
their real types, so that they work with any pointer size. This tool
writes those definitions: for every label of the data file that the C
sources declare, a definition of the declared type, with the initial
value decoded from the bytes of the data file through the type's layout
in the DWARF of a 32-bit build (read with gdb's python API).

Pointers to strings become string literals; pointers to asm code are NULL
(as they are in the data files without difftest). Arrays declared with an
unknown size get the size of the bytes up to the next declared label.

Usage:
  python tools/decomp-agent/sx_data_typed.py <data.c> <out_vars.c>|--inline [--exe <32-bit exe>]
--inline puts each definition into the C file of the variable (sx_data_inline.py).
Prints the labels with conflicting or missing declarations; the overrides
below settle them.
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
from sx_deps import strip_c  # noqa: E402

GDB = 'C:/msys64/mingw64/bin/gdb.exe'
TOPS = ('src', 'bflibrary', 'bfsoundlib', 'bfsmacklib', 'swrendersoft', 'bfariadne')

# Declarations that the source scan cannot read or that conflict: label ->
# (type, dims). Conflicts are settled for the type that the users agree on.
OVERRIDES = {
    'save_mortal_salt': ('u32', ''),
    'redo_scanner': ('struct rectangle', '[128]'),
    'map_craters': ('struct MapCreater', '[MAP_CRATERS_COUNT]'),
    'SCANNER_keep_arcs': ('u32', ''),
}


# sources built only with difftest: their declarations need no definitions
NOT_RELEASE = {'smack2ail.c', 'difftest_fuzz.c'}


def parse_data_file(path):
    """Objects of a generated data file in order: name -> dict(bytes, ptrs)."""
    t = open(os.path.join(REPO, path), encoding='utf-8').read()
    structs = {}
    for m in re.finditer(r'struct (SxD_\w+) \{\n(.*?)\n\} __attribute__\(\(packed\)\);', t, re.S):
        fields = []
        for fm in re.finditer(r'(ubyte d\d+\[(\d+)\]|void \*p\d+);', m.group(2)):
            fields.append(('d', int(fm.group(2))) if fm.group(2) else ('p', 4))
        structs[m.group(1)] = fields
    objs = {}
    order = []
    for m in re.finditer(r'^struct (SxD_\w+) (\w+) SXD_OBJ[^=;]*(=\s*\{\n(.*?)\n\};|;)', t, re.S | re.M):
        sname, name, body = m.group(1), m.group(2), m.group(4)
        fields = structs[sname]
        data = bytearray()
        ptrs = {}
        if body is None or not body.strip():
            for kind, n in fields:
                data += bytes(n)
        else:
            # remove the release branch of #if/#else blocks, keep difftest one
            body = re.sub(r'#if defined\(ENABLE_DIFFTEST\) && ENABLE_DIFFTEST\n(.*?)#else\n.*?#endif\n',
                          lambda mm: mm.group(1).replace('(void *)', '(void *)@code@'), body + '\n', flags=re.S)
            items = re.findall(r'\{([^}]*)\}|\(void \*\)(@code@)?&?(\w+)|\bNULL\b', body)
            if len(items) != len(fields):
                raise ValueError('%s: %d items for %d fields' % (name, len(items), len(fields)))
            for (kind, n), (blk, code, target) in zip(fields, items):
                if kind == 'd':
                    vals = [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', blk)]
                    if blk.strip() == '0':  # {0}: zeroed (.bss)
                        vals = [0] * n
                    assert len(vals) == n, name
                    data += bytes(vals)
                else:
                    if target:
                        ptrs[len(data)] = ('code' if code else 'data', target)
                    data += bytes(4)
        objs[name] = {'bytes': data, 'ptrs': ptrs}
        order.append(name)
    return order, objs


def scan_decls(labels):
    decls = {}
    for top in TOPS:
        for dp, dn, fn in os.walk(os.path.join(REPO, top)):
            if 'x86-dos' in dp:
                continue
            for x in fn:
                if not x.endswith(('.c', '.h')) or x.endswith('_data.c') or x in NOT_RELEASE:
                    continue
                p = os.path.join(dp, x)
                t = strip_c(open(p, encoding='utf-8', errors='replace').read())
                for m in re.finditer(r'(?m)^[ \t]*extern\s+([^;(){}"]*?);', t):
                    d = ' '.join(m.group(1).split())
                    # "T a, b, c" or "T *a" or "T a[3][4]"
                    mm = re.match(r'^((?:const |unsigned |signed |struct |union |enum )*\w+)\s*(.*)$', d)
                    if not mm:
                        continue
                    base, rest = mm.group(1), mm.group(2)
                    for part in rest.split(','):
                        pm = re.match(r'^\s*(\**)\s*(?:const\s+)?(\w+)\s*((?:\[[^\]]*\])*)\s*$', part)
                        if pm and pm.group(2) in labels:
                            typ = base + (' ' + pm.group(1) if pm.group(1) else '')
                            rel = os.path.relpath(p, REPO).replace(chr(92), '/')
                            decls.setdefault(pm.group(2), []).append((rel, typ, pm.group(3)))
    return decls


def eval_dims(dims, defines):
    out = []
    for d in re.findall(r'\[([^\]]*)\]', dims):
        d = d.strip()
        if d == '':
            out.append(None)
            continue
        expr = re.sub(r'\b[A-Za-z_]\w*\b', lambda m: '(%s)' % defines.get(m.group(0), m.group(0)), d)
        for _ in range(5):
            expr2 = re.sub(r'\b[A-Za-z_]\w*\b', lambda m: '(%s)' % defines.get(m.group(0), m.group(0)), expr)
            if expr2 == expr:
                break
            expr = expr2
        out.append(int(eval(expr)))
    return out


def scan_defines():
    defs = {}
    for top in TOPS:
        for dp, dn, fn in os.walk(os.path.join(REPO, top)):
            for x in fn:
                if x.endswith('.h') or x.endswith('.c'):
                    t = open(os.path.join(dp, x), encoding='utf-8', errors='replace').read()
                    for m in re.finditer(r'(?m)^#define\s+(\w+)\s+([^\n/]+)', t):
                        v = m.group(2).strip()
                        if re.match(r'^[\w\s\(\)\*\+\-/]+$', v):
                            defs.setdefault(m.group(1), v)
    return defs


GDB_SCRIPT = r'''
import gdb, json, struct, sys
NL = chr(10)
job = json.load(open(JOB))
out = []

def c_str(b):
    s = ''
    for ch in b:
        if ch == 0x22: s += '\\"'
        elif ch == 0x5c: s += '\\\\'
        elif ch == 10: s += '\\n'
        elif 32 <= ch < 127: s += chr(ch)
        else: s += '\\%03o' % ch
    return '"' + s + '"'

def is_text(b):
    if 0 not in b: return False
    n = b.index(0)
    return n > 0 and all(32 <= c < 127 or c in (9, 10) for c in b[:n]) and not any(b[n:])

def fmt(t, buf, off, ptrs, strings, ind):
    st = t.strip_typedefs()
    c = st.code
    sz = st.sizeof
    if c in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_CHAR, gdb.TYPE_CODE_BOOL, gdb.TYPE_CODE_ENUM):
        signed = st.is_signed if c != gdb.TYPE_CODE_ENUM else False
        v = int.from_bytes(bytes(buf[off:off + sz]), 'little', signed=signed)
        if v == 0: return None
        if v < 0 or v < 10: return str(v)
        return hex(v)
    if c == gdb.TYPE_CODE_FLT:
        v = struct.unpack('<f' if sz == 4 else '<d', bytes(buf[off:off + sz]))[0]
        return None if v == 0 else repr(v)
    if c == gdb.TYPE_CODE_PTR:
        p = ptrs.get(str(off))
        if p is not None:
            kind, target = p
            if kind == 'code':
                return None
            tt = st.target().strip_typedefs()
            if target in strings and tt.sizeof == 1:
                return c_str(bytes.fromhex(strings[target]).split(b'\0')[0])
            raise ValueError('pointer to data label %s' % target)
        v = int.from_bytes(bytes(buf[off:off + sz]), 'little')
        if v == 0: return None
        raise ValueError('raw pointer value 0x%x at %d' % (v, off))
    if c == gdb.TYPE_CODE_ARRAY:
        et = st.target()
        n = sz // et.strip_typedefs().sizeof
        ets = et.strip_typedefs()
        if ets.code in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_CHAR) and ets.sizeof == 1 and \
                str(ets).endswith('char') and is_text(bytes(buf[off:off + sz])):
            return c_str(bytes(buf[off:off + sz]).split(b'\0')[0])
        es = ets.sizeof
        if ets.code in (gdb.TYPE_CODE_INT, gdb.TYPE_CODE_CHAR, gdb.TYPE_CODE_BOOL):
            # scalars in one style: decimal if signed or all small, else hex
            vals = [int.from_bytes(bytes(buf[off + i * es:off + (i + 1) * es]), 'little',
                    signed=ets.is_signed) for i in range(n)]
            while vals and vals[-1] == 0: vals.pop()
            if not vals: return None
            if ets.is_signed or max(vals) < 10:
                items = [str(v) for v in vals]
            else:
                items = ['0x%0*x' % (2 * es, v) for v in vals]
            return wrap(items, ind, 12 if es == 1 else 8)
        items = [fmt(et, buf, off + i * es, ptrs, strings, ind + 1) for i in range(n)]
        while items and items[-1] is None: items.pop()
        if not items: return None
        items = [x if x is not None else ('0' if ets.code not in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION, gdb.TYPE_CODE_ARRAY) else '{0}') for x in items]
        return wrap(items, ind)
    if c in (gdb.TYPE_CODE_STRUCT, gdb.TYPE_CODE_UNION):
        fields = st.fields()
        if c == gdb.TYPE_CODE_UNION:
            fields = [f for f in fields if f.type.strip_typedefs().sizeof == sz][:1]
        items = []
        for f in fields:
            if f.bitsize:
                v = int.from_bytes(bytes(buf[off:off + sz]), 'little') >> (f.bitpos) & ((1 << f.bitsize) - 1)
                if v: items.append('.%s = %d' % (f.name, v))
                continue
            if f.name is None:
                fo = off + f.bitpos // 8
                if any(buf[fo:fo + f.type.strip_typedefs().sizeof]):
                    raise ValueError('anonymous member in %s' % st)
                continue
            v = fmt(f.type, buf, off + f.bitpos // 8, ptrs, strings, ind + 1)
            if v is not None: items.append('.%s = %s' % (f.name, v))
        if not items: return None
        return wrap(items, ind)
    raise ValueError('type code %d of %s' % (c, st))

def wrap(items, ind, per_line=None):
    s = '{' + ', '.join(items) + '}'
    if len(s) + 4 * ind < 76 and NL not in s:
        return s
    pad = '    ' * (ind + 1)
    lines, cur = [], []
    for x in items:
        if NL in x or len(x) > 60:
            if cur: lines.append(cur); cur = []
            lines.append([x])
            continue
        full = per_line is not None and len(cur) >= per_line
        if cur and (full or len(pad) + len(', '.join(cur)) + len(x) + 3 > 78):
            lines.append(cur); cur = []
        cur.append(x)
    if cur: lines.append(cur)
    return '{' + NL + NL.join(pad + ', '.join(l) + ',' for l in lines) + NL + '    ' * ind + '}'

for it in job['items']:
    try:
        t = gdb.parse_and_eval('(%s *)0' % it['type']).type.target()
        for n in reversed(it['dims']):
            t = t.array(n - 1)
        buf = bytearray.fromhex(it['bytes'])
        need = t.sizeof
        if len(buf) < need:
            buf += bytes(need - len(buf))
        v = fmt(t, buf, 0, it['ptrs'], job['strings'], 0)
        out.append({'name': it['name'], 'init': v, 'size': need, 'have': len(bytes.fromhex(it['bytes']))})
    except Exception as e:
        out.append({'name': it['name'], 'error': str(e)})
json.dump(out, open(OUT, 'w'), indent=1)
'''


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    exe = os.path.join(REPO, 'build', 'harness', 'src', 'syndwarsfx.exe')
    if '--exe' in sys.argv:
        exe = sys.argv[sys.argv.index('--exe') + 1]
        args.remove(exe)
    data_path = args[0]
    out_path = '--inline' if '--inline' in sys.argv else args[1]
    order, objs = parse_data_file(data_path)
    decls = scan_decls(set(order))
    defines = scan_defines()
    strings = {n: o['bytes'].hex() for n, o in objs.items()}
    used = [n for n in order if n in decls or n in OVERRIDES]
    report = []
    items = []
    for i, n in enumerate(order):
        if n not in used:
            continue
        kinds = sorted(set((d[1], d[2]) for d in decls.get(n, [])))
        if n in OVERRIDES:
            typ, dims = OVERRIDES[n]
        else:
            if len(kinds) > 1:
                report.append('CONFLICT %s: %s' % (n, kinds))
            typ, dims = kinds[0]
        # bytes: this label up to the next used label
        span = bytearray()
        ptrs = {}
        for m in order[i:]:
            if m != n and m in used:
                break
            for off, p in objs[m]['ptrs'].items():
                ptrs[str(len(span) + off)] = p
            span += objs[m]['bytes']
        own = len(objs[n]['bytes'])
        dl = eval_dims(dims, defines)
        items.append({'name': n, 'type': typ, 'dims': dl, 'dimtext': dims,
                      'bytes': span.hex(), 'ptrs': ptrs, 'own': own,
                      'decl_in': sorted(set(d[0] for d in decls.get(n, [])))})
    # sizes of [] dims: from the span, in units of the element
    elem_sizes = gdb_sizes(exe, [(it['type'], [d for d in it['dims'] if d is not None]) for it in items])
    for it, es in zip(items, elem_sizes):
        if None in it['dims']:
            span = len(it['bytes']) // 2
            k = it['dims'].index(None)
            inner = 1
            for d in it['dims'][k + 1:]:
                inner *= d
            it['dims'][k] = span // (es * inner)
            it['dimtext'] = it['dimtext'].replace('[]', '[%d]' % it['dims'][k], 1)
            report.append('SIZED %s%s from %d bytes up to the next declared label' % (it['name'], it['dimtext'], span))
    job = os.path.join(REPO, 'build', 'sx_data_typed_job.json')
    res = os.path.join(REPO, 'build', 'sx_data_typed_out.json')
    json.dump({'items': items, 'strings': strings}, open(job, 'w'))
    script = os.path.join(REPO, 'build', 'sx_data_typed_gdb.py')
    open(script, 'w').write('JOB = %r\nOUT = %r\n' % (job, res) + GDB_SCRIPT)
    subprocess.run([GDB, '-batch', '-x', script, exe], check=True, capture_output=True)
    out = json.load(open(res))
    lines = []
    for it, r in zip(items, out):
        if 'error' in r:
            report.append('ERROR %s: %s' % (it['name'], r['error']))
            continue
        if r['size'] > r['have']:
            report.append('SHORT %s: type needs %d bytes, data has %d' % (it['name'], r['size'], r['have']))
        if r['size'] < it['own']:
            report.append('LONGER %s: label has %d bytes, type %d' % (it['name'], it['own'], r['size']))
        typ = it['type']
        sep = '' if typ.endswith('*') else ' '
        d = '%s%s%s%s' % (typ, sep, it['name'], it['dimtext'])
        lines.append((it, d + (' = %s;' % r['init'] if r['init'] is not None else ';')))
    if out_path == '--inline':
        inline_out(data_path, lines, report)
    else:
        write_out(out_path, data_path, lines)
    for r in report:
        print(r)
    print('%d definitions written to %s' % (len(lines), out_path))
    return 0


def gdb_sizes(exe, types):
    job = os.path.join(REPO, 'build', 'sx_data_typed_sizes.json')
    res = os.path.join(REPO, 'build', 'sx_data_typed_sizes_out.json')
    json.dump([t for t, _ in types], open(job, 'w'))
    script = os.path.join(REPO, 'build', 'sx_data_typed_sizes.py')
    open(script, 'w').write(
        'import gdb, json\nts = json.load(open(%r))\n' % job +
        'json.dump([gdb.parse_and_eval("(%%s *)0" %% t).type.target().sizeof for t in ts], open(%r, "w"))\n' % res)
    subprocess.run([GDB, '-batch', '-x', script, exe], check=True, capture_output=True)
    return json.load(open(res))


from sx_data_inline import inline_out  # noqa: E402


def write_out(out_path, data_path, lines):
    headers = set()
    for it, _ in lines:
        for p in it['decl_in']:
            if p.endswith('.h'):
                headers.add(os.path.basename(p))
    name = os.path.basename(out_path)
    s = []
    s.append('/' + '*' * 78 + '/')
    s.append('// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.')
    s.append('/' + '*' * 78 + '/')
    s.append('/** @file %s' % name)
    s.append(' *     Variables of %s, with their C types.' % data_path)
    s.append(' * @par Purpose:')
    s.append(' *     The labels of the original asm data that the C code uses, as C')
    s.append(' *     variables of the types it declares them with, for builds without')
    s.append(' *     the asm. Builds with --enable-difftest take %s' % os.path.basename(data_path))
    s.append(' *     instead, the byte layout that the asm needs.')
    s.append(' * @par Comment:')
    s.append(' *     Made from the data by tools/decomp-agent/sx_data_typed.py; from')
    s.append(' *     now on edited as normal C. Variables declared only in a .c file')
    s.append(' *     are defined here with the type of that declaration.')
    s.append(' */')
    s.append('/' + '*' * 78 + '/')
    s.append('#include "bftypes.h"')
    s.append('')
    s.append('#ifdef HAVE_CONFIG_H')
    s.append('# include "config.h"')
    s.append('#endif')
    s.append('')
    s.append('#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)')
    s.append('')
    for h in sorted(headers):
        s.append('#include "%s"' % h)
    s.append('')
    s.append('#pragma GCC diagnostic ignored "-Wmissing-braces"')
    s.append('')
    for it, d in lines:
        where = [os.path.basename(p) for p in it['decl_in'] if not p.endswith('.h')]
        if where and not any(p.endswith('.h') for p in it['decl_in']):
            s.append('/* declared in %s */' % ', '.join(where))
        s.append(d)
    s.append('')
    s.append('#endif')
    s.append('/' + '*' * 78 + '/')
    open(os.path.join(REPO, out_path), 'w', encoding='utf-8', newline='\n').write('\n'.join(s) + '\n')


if __name__ == '__main__':
    sys.exit(main())
