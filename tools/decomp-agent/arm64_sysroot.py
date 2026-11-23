#!/usr/bin/env python3
"""Builds an arm64 sysroot from Ubuntu ports .debs without root.

Usage: arm64_sysroot.py [sysroot=~/arm64-sysroot] [pkg...]
Downloads the Packages indexes of the running Ubuntu release, resolves
Depends recursively (first alternative; libc6 and friends come from the
cross toolchain, gcc-aarch64-linux-gnu), downloads the .debs into
<sysroot>/../arm64-debs and extracts them with dpkg-deb -x. The default
packages are the libraries of the game; `build.sh --arm64` uses the result.
"""
import lzma
import os
import re
import subprocess
import sys
import urllib.request

MIRROR = 'http://ports.ubuntu.com/ubuntu-ports'


def codename():
    for line in open('/etc/os-release'):
        if line.startswith('VERSION_CODENAME='):
            return line.split('=', 1)[1].strip().strip('"')
    raise SystemExit('no VERSION_CODENAME in /etc/os-release')


RELEASE = codename()
SUITES = (RELEASE, RELEASE + '-updates', RELEASE + '-security')
DEFAULT_PKGS = ('libsdl2-dev', 'libopenal-dev', 'libvorbis-dev', 'libpng-dev',
                'libwildmidi-dev', 'zlib1g-dev', 'libpulse0')
COMPONENTS = ('main', 'universe')
SKIP = {'libc6', 'libc6-dev', 'libgcc-s1', 'libstdc++6', 'libc-dev', 'linux-libc-dev',
        'libcrypt1', 'libcrypt-dev', 'gcc-15-base', 'libgcc-15-dev', 'libstdc++-15-dev',
        'debconf', 'dpkg', 'pkgconf', 'pkg-config', 'pkgconf-bin', 'perl', 'dpkg-dev',
        'adduser', 'init-system-helpers', 'xkb-data',
        'libwayland-bin', 'x11-common', 'mesa-common-dev', 'libglx-mesa0', 'libgl1-mesa-dri',
        'mesa-libgallium', 'libllvm21', 'libllvm20', 'libllvm19',
        'libudev-dev', 'libdbus-1-dev', 'libpulse-dev', 'libsndio-dev',
        'libibus-1.0-dev', 'libdecor-0-dev', 'libgbm-dev', 'libegl-dev', 'libgles-dev',
        'libgl-dev', 'libglu1-mesa-dev', 'libvulkan-dev', 'libxkbcommon-dev',
        'libwayland-dev', 'libx11-dev', 'libxcursor-dev', 'libxext-dev', 'libxfixes-dev',
        'libxi-dev', 'libxinerama-dev', 'libxrandr-dev', 'libxss-dev', 'libxv-dev',
        'libxxf86vm-dev', 'libdrm-dev', 'libpipewire-0.3-dev', 'libjack-dev',
        'libjack-jackd2-dev', 'libasound2-dev', 'libsamplerate0-dev', 'libpipewire-0.3-common'}


def fetch(url, dest):
    if not os.path.exists(dest):
        tmp = dest + '.part'
        req = urllib.request.Request(url, headers={'User-Agent': 'Debian APT-HTTP/1.3 (3.0)'})
        with urllib.request.urlopen(req) as r, open(tmp, 'wb') as o:
            o.write(r.read())
        os.rename(tmp, dest)
    return dest


def parse(text):
    pk = {}
    for block in text.split('\n\n'):
        d = {}
        key = None
        for line in block.split('\n'):
            if line[:1] in (' ', '\t') and key:
                d[key] += '\n' + line
            elif ':' in line:
                key, v = line.split(':', 1)
                d[key] = v.strip()
        if 'Package' in d:
            name = d['Package']
            prov = d.get('Provides', '')
            pk.setdefault(name, d)
            for p in prov.split(','):
                p = re.sub(r'\(.*\)', '', p).strip()
                if p:
                    pk.setdefault('virtual:' + p, d)
    return pk


def deps(d):
    out = []
    for field in ('Pre-Depends', 'Depends'):
        for dep in d.get(field, '').split(','):
            dep = dep.strip()
            if not dep:
                continue
            first = dep.split('|')[0]
            first = re.sub(r'\(.*?\)|\[.*?\]|<.*?>', '', first).strip()
            first = first.split(':')[0]
            out.append(first)
    return out


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                           os.path.expanduser('~/arm64-sysroot'))
    want = sys.argv[2:] or list(DEFAULT_PKGS)
    cache = os.path.join(os.path.dirname(root), 'arm64-debs')
    os.makedirs(cache, exist_ok=True)
    os.makedirs(root, exist_ok=True)
    pk = {}
    # later suites (updates, security) override release versions
    for suite in SUITES:
        for comp in COMPONENTS:
            f = os.path.join(cache, 'Packages_%s_%s.xz' % (suite, comp))
            fetch('%s/dists/%s/%s/binary-arm64/Packages.xz' % (MIRROR, suite, comp), f)
            for name, d in parse(lzma.open(f).read().decode('utf-8', 'replace')).items():
                pk[name] = d
    todo = list(want)
    seen = set()
    while todo:
        n = todo.pop()
        if n in seen or n in SKIP:
            continue
        seen.add(n)
        d = pk.get(n) or pk.get('virtual:' + n)
        if d is None:
            print('missing:', n)
            continue
        if d['Package'] != n:
            if d['Package'] in seen:
                continue
            seen.add(d['Package'])
        todo.extend(deps(d))
    files = []
    for n in sorted(seen):
        d = pk.get(n) or pk.get('virtual:' + n)
        if d is None or d.get('Architecture') not in ('arm64', 'all'):
            continue
        fn = d['Filename']
        dest = os.path.join(cache, os.path.basename(fn))
        if dest not in files:
            fetch(MIRROR + '/' + fn, dest)
            files.append(dest)
    for f in files:
        subprocess.check_call(['dpkg-deb', '-x', f, root])
    print('%d packages extracted into %s' % (len(files), root))
    for f in files:
        print('  ' + os.path.basename(f))


if __name__ == '__main__':
    main()
