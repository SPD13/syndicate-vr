"""Moves whole top-level C definitions from one file to another.

Usage: python tools/decomp-agent/movefuncs.py <from.c> <to.c> name [name...]
A definition is found by a line at column 0 that declares `name(` (or
`name[`/`name =` for data) and ends at the next line that is exactly `}`
or `};` (or the `;` line for one-line data). The comment block and blank
line just before it move along. A DIFFTEST reference `name_asm` moves too.
Definitions are appended to <to.c> in the order of <from.c>, before the
last `/****` separator line if there is one.
"""
import re
import sys


def find_defs(lines, names):
    """Returns {name: (start, end)} line ranges (end exclusive)."""
    found = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        if line and not line[0].isspace() and line[0] not in '#/}{*' and not line.startswith('//'):
            m = None
            for n in names:
                if re.search(r'(^|[\s\*])' + re.escape(n) + r'\s*(\(|\[|=|;)', line.split('//')[0]):
                    m = n
                    break
            if m and not line.rstrip().endswith(';') or (m and '=' in line):
                # end of the definition
                j = i
                if line.rstrip().endswith(';') and '{' not in line:
                    end = i + 1
                else:
                    while j < len(lines) and lines[j].rstrip() not in ('}', '};'):
                        j += 1
                    end = j + 1
                # leading comment block
                start = i
                k = i - 1
                if k >= 0 and lines[k].rstrip().endswith('*/'):
                    while k >= 0 and not lines[k].lstrip().startswith('/*'):
                        k -= 1
                    start = k
                elif k >= 0 and lines[k].startswith('//'):
                    while k >= 0 and lines[k].startswith('//'):
                        k -= 1
                    start = k + 1
                found.setdefault(m, []).append((start, end))
                i = end
                continue
        i += 1
    return found


def main():
    src, dst = sys.argv[1], sys.argv[2]
    names = sys.argv[3:]
    names = names + [n + '_asm' for n in names]
    lines = open(src, encoding='utf-8').read().split('\n')
    defs = find_defs(lines, names)
    missing = [n for n in sys.argv[3:] if n not in defs]
    if missing:
        print('not found: ' + ' '.join(missing))
    ranges = sorted(r for rs in defs.values() for r in rs)
    moved = []
    keep = []
    last = 0
    for (s, e) in ranges:
        if s < last:
            continue
        keep.extend(lines[last:s])
        block = lines[s:e]
        moved.append('\n'.join(block))
        last = e
        # drop one blank line after the moved block
        if last < len(lines) and lines[last].strip() == '':
            last += 1
    keep.extend(lines[last:])
    open(src, 'w', encoding='utf-8', newline='\n').write('\n'.join(keep))
    out = open(dst, encoding='utf-8').read().split('\n')
    sep = max((i for i, l in enumerate(out) if l.startswith('/****')), default=len(out))
    out[sep:sep] = ['\n\n'.join(moved), '']
    open(dst, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print('moved %d definitions' % len(moved))


main()
