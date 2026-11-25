#!/usr/bin/env python3
"""Inspect the VR spectator shared memory published by the game.

Usage: python vr/tools/inspect_share.py [--launch EXE GAMEDATA MISSION] [--wait S]
                                        [--ppm OUT.ppm]

With --launch, starts the game headless into a mission, waits, inspects the
shared memory, then stops the game. --ppm writes a top-down image of the
exported textured geometry, as a quick check of texture mapping.
"""
import argparse
import mmap
import os
import struct
import subprocess
import sys
import time

NAME = 'Local\\SyndWarsFX_VRSpectator'
HDR_FMT = '<4I I I I I I I I I 4f 5I 16I'
VERT_FMT = '<5f4B'
THING_FMT = '<3fH6B2hHBB5BB'
THING_SIZE = struct.calcsize(THING_FMT)
STATIC_MAX, DYN_MAX, THINGS_MAX = 600000, 120000, 2048
OFF_STATIC = 4096
OFF_DYN = OFF_STATIC + STATIC_MAX * 24
OFF_THINGS = OFF_DYN + DYN_MAX * 24
OFF_PALETTE = OFF_THINGS + THINGS_MAX * THING_SIZE
OFF_TMAP = OFF_PALETTE + 1024
OFF_SPRITE_RECTS = OFF_TMAP + 18 * 65536
OFF_SPRITE_PAGES = OFF_SPRITE_RECTS + 4096 * 6 + 4096 * 2 + 16384 * 8 + 32768 * 10
TOTAL = OFF_SPRITE_PAGES + 48 * 65536 * 2


def read_block():
    m = mmap.mmap(-1, TOTAL, tagname=NAME, access=mmap.ACCESS_READ)
    for _ in range(100):
        seq1 = struct.unpack_from('<I', m, 12)[0]
        if seq1 & 1:
            time.sleep(0.005)
            continue
        data = m[:]
        seq2 = struct.unpack_from('<I', m, 12)[0]
        if seq1 == seq2:
            return data
    raise RuntimeError('could not get a consistent snapshot')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--launch', nargs=3, metavar=('EXE', 'GAMEDATA', 'MISSION'))
    ap.add_argument('--wait', type=float, default=12)
    ap.add_argument('--ppm')
    args = ap.parse_args()
    proc = None
    if args.launch:
        exe, gamedata, mission = args.launch
        env = dict(os.environ, SWARS_VR_SPECTATOR='1', SDL_VIDEODRIVER='dummy',
                   SDL_AUDIODRIVER='dummy')
        proc = subprocess.Popen([exe, '-q', '-W', '-m', mission], cwd=gamedata, env=env,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(args.wait)
    try:
        d = read_block()
    finally:
        if proc:
            proc.kill()
    h = struct.unpack_from(HDR_FMT, d, 0)
    (magic, version, total, seq, turn, mapno, srev, trev, n_static, n_dyn, n_things,
     in_mission) = h[:12]
    cam = h[12:16]
    print('magic=%08x version=%d seq=%d turn=%d map=%d in_mission=%d' % (
        magic, version, seq, turn, mapno, in_mission))
    print('static=%d dyn=%d things=%d static_rev=%d tmap_rev=%d cam=%s' % (
        n_static, n_dyn, n_things, srev, trev, ['%.0f' % c for c in cam]))

    def bounds(off, n):
        if n == 0:
            return None
        mn = [1e9] * 3
        mx = [-1e9] * 3
        tex = 0
        for i in range(n):
            v = struct.unpack_from(VERT_FMT, d, off + i * 24)
            for k in range(3):
                mn[k] = min(mn[k], v[k])
                mx[k] = max(mx[k], v[k])
            tex += v[8] & 1
        return mn, mx, tex
    for name, off, n in (('static', OFF_STATIC, n_static), ('dyn', OFF_DYN, n_dyn)):
        b = bounds(off, n)
        if b:
            print('%s bounds min=%s max=%s textured=%d%%' % (
                name, ['%.0f' % x for x in b[0]], ['%.0f' % x for x in b[1]],
                100 * b[2] // max(n, 1)))
    # people vs terrain height: terrain is the first 127*127*6 static verts
    alts = {}
    for i in range(0, min(n_static, 127 * 127 * 6), 6):
        v = struct.unpack_from(VERT_FMT, d, OFF_STATIC + i * 24)
        alts[(int(v[0]) >> 8, int(v[2]) >> 8)] = v[1]
    agents = 0
    for i in range(min(n_things, 12)):
        t = struct.unpack_from(THING_FMT, d, OFF_THINGS + i * THING_SIZE)
        x, y, z, idx, typ, sub, flags = t[:7]
        agents += flags & 1
        print('  thing %4d type=%d sub=%2d flags=%d pos=(%.0f, %.1f, %.0f) ground=%.1f' % (
            idx, typ, sub, flags, x, y, z, alts.get((int(x) >> 8, int(z) >> 8), -1)))
    pal = d[OFF_PALETTE:OFF_PALETTE + 768]
    print('palette max component=%d (63 means 6-bit VGA)' % max(pal))
    if args.ppm:
        # top-down: sample one texel per textured static vertex triangle centroid
        W = 512
        img = bytearray(W * W * 3)
        scale = W / 32768.0
        for i in range(0, n_static, 3):
            vs = [struct.unpack_from(VERT_FMT, d, OFF_STATIC + (i + k) * 24) for k in range(3)]
            cx = sum(v[0] for v in vs) / 3
            cz = sum(v[2] for v in vs) / 3
            if vs[0][8] & 1:
                u = int(sum(v[3] for v in vs) / 3) & 255
                vv = int(sum(v[4] for v in vs) / 3) & 255
                page = vs[0][5]
                c = d[OFF_TMAP + page * 65536 + vv * 256 + u]
            else:
                c = vs[0][6]
            px, pz = int(cx * scale), int(cz * scale)
            if 0 <= px < W and 0 <= pz < W:
                o = (pz * W + px) * 3
                img[o:o + 3] = bytes(min(255, pal[c * 3 + k] * 4) for k in range(3))
        with open(args.ppm, 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (W, W) + bytes(img))
        print('wrote', args.ppm)


if __name__ == '__main__':
    main()
