"""Which front-end symbols do the core objects of libswarscore reference?

Usage: python tools/decomp-agent/coredeps.py <build/harness/src> [--detail module]
The core objects are those of libswarscore.a (libswarscore_a-*.o), the
front end those of libswarsfe.a (libswarsfe_a-*.o), split into the groups
below (objects not in the archives, left by a source moved to the other
library, are skipped); a module in the wrong library is reported. Reads the symbol tables
with nm and lists, per front-end group, the symbols the core objects use
and which core objects use them.
"""
import collections
import os
import re
import subprocess
import sys

NM = 'C:/msys64/mingw32/bin/nm.exe'
AR = 'C:/msys64/mingw32/bin/ar.exe'

GROUPS = {
    'render': '''lvdraw3d engindrwlstm_wrp engindrwlstx_tng tngobjdrw bflib_vidraw
        drawtext_wrp mydraw purpldrw purpldrwlst scandraw specblit guigraph guitext
        guiboxes hud_draw hud_panel hud_panecfg hud_target display thing_ovmous
        embedanim game_sprts'''.split(),
    'menus': '''feappbar febrief fecntrls fecryo fedebrief feequip felogin
        femain fenet feoptions fepanet fepause feresearch feshared festorage
        feworld packetfe devcreds game_options game_save'''.split(),
    'shell': 'game swapp swmain vrspectate'.split(),
    'input': 'keyboard mouse plyr_usrinp'.split(),
    'sound': 'sound fmvplay bfsmack smailfile smkdec'.split(),
    'test': '''difftest_fuzz difftest_hook_fe thing_debug pathtrig_debug'''.split(),
    # libswarstest.a: the test hooks of the turn, linked to the core alone
    'core-test': '''difftest_bot difftest_hook_misc difftest_hook_people
        difftest_hook_transport'''.split(),
    # libswarsview.a: the world export (swview.h), linked to the core alone
    'view': 'swview'.split(),
}

# Libraries besides the core which must not need the front end either
SIDE_LIBS = {'core-test': 'libswarstest', 'view': 'libswarsview'}


def module_of(objname):
    return re.sub(r'^libswars(core|fe|test|view)_a-', '', objname)[:-2]


def symbols(path):
    out = subprocess.run([NM, path], capture_output=True, text=True).stdout
    defined, undef = set(), set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == 'U':
            undef.add(parts[1])
        elif len(parts) == 3 and parts[1] in 'TDBRCtdbr' and parts[1].isupper() or (len(parts) == 3 and parts[1] in 'TDBRC'):
            defined.add(parts[2])
    return defined, undef


def main():
    d = sys.argv[1]
    detail = sys.argv[sys.argv.index('--detail') + 1] if '--detail' in sys.argv else None
    group_of = {m: g for g, ms in GROUPS.items() for m in ms}
    defs, undefs = {}, {}
    core = []
    # only the objects in the libraries: a source moved to the other
    # library leaves its old object in the build folder
    members = set()
    for lib in ('libswarscore.a', 'libswarsfe.a', 'libswarstest.a', 'libswarsview.a'):
        if os.path.exists(os.path.join(d, lib)):
            members.update(subprocess.run([AR, 't', os.path.join(d, lib)], capture_output=True,
                                          text=True).stdout.split())
    for f in sorted(os.listdir(d)):
        if members and f not in members:
            continue
        if re.match(r'libswars(core|fe|test|view)_a-', f) and f.endswith('.o'):
            m = module_of(f)
            defs[m], undefs[m] = symbols(os.path.join(d, f))
            in_core = f.startswith('libswarscore_a-')
            side = [g for g, lib in SIDE_LIBS.items() if f.startswith(lib + '_a-')]
            if in_core:
                core.append(m)
            if side or (group_of.get(m) in SIDE_LIBS):
                if not side or (group_of.get(m) != side[0]):
                    print('warning: %s is in %s but in group %s'
                          % (m, f.split('_a-')[0], group_of.get(m)))
            elif in_core == (m in group_of):
                print('warning: %s is in %s but %s' % (m, 'libswarscore' if in_core else 'libswarsfe',
                      'in group ' + group_of[m] if in_core else 'in no front-end group'))
    owner = {}
    for m, ds in defs.items():
        for s in ds:
            owner[s] = m
    print('core modules (%d): %s' % (len(core), ' '.join(sorted(core))))
    per_group = collections.defaultdict(lambda: collections.defaultdict(set))
    for m in core:
        for s in undefs[m]:
            o = owner.get(s)
            if o and o in group_of:
                per_group[group_of[o]][s].add(m)
    # the test hooks of the core (libswarstest) and the world export
    # (libswarsview) must not need the front end
    for sg in SIDE_LIBS:
        side_fe = collections.defaultdict(set)
        for m in GROUPS[sg]:
            for s in undefs.get(m, ()):
                o = owner.get(s)
                if o and group_of.get(o) not in (None,) + tuple(SIDE_LIBS):
                    side_fe[s].add(m)
        print('%s uses %d front-end symbols%s' % (sg, len(side_fe), ''.join(
            '\n   %-40s %-18s <- %s' % (s, owner[s], ' '.join(sorted(side_fe[s])))
            for s in sorted(side_fe))))
    for g in GROUPS:
        if g in SIDE_LIBS:
            continue
        syms = per_group.get(g, {})
        users = collections.Counter(u for us in syms.values() for u in us)
        print('\n%s: %d symbols used by core; users: %s' % (g, len(syms),
              ', '.join('%s(%d)' % kv for kv in users.most_common())))
        if detail == g:
            for s in sorted(syms):
                print('   %-40s %-18s <- %s' % (s, owner[s], ' '.join(sorted(syms[s]))))


if __name__ == '__main__':
    main()
