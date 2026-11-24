"""Catalog of the moving and special map parts (building things), private page.

python inventory_buildings.py
Reads every maps/*.mad: the mesh arrays at the start (points, faces3,
objects, faces4, layout as quick_load_pc in lvfiles.c), then the building
things that follow the static records (168-byte Thing records, plus an M33
matrix after rotors and subtype 0x37). Counts each special building
subtype across the maps and renders up to six different shapes per subtype
(flat-shaded, the game's three-quarter view) to
assets3d/review/buildings/index.html. Nothing is generated.
"""
import collections
import glob
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

import inventory_statics  # noqa: E402
import swdata  # noqa: E402

G = swdata.find_gamedata()
OUT = os.path.join(os.path.dirname(G), "assets3d", "review", "buildings")
os.makedirs(OUT, exist_ok=True)

# quick_load_pc: (has_count, size, extra) in file order
QL = [(False, 18, 16384), (True, 18, 800), (True, 16, 800), (True, 10, 2000), (True, 32, 2000), (True, 36, 120),
      (True, 6, 4000), (True, 32, 10), (True, 16, 3500), (True, 40, 2000), (True, 54, 50), (True, 36, 50),
      (True, 36, 50), (True, 6, 1000), (True, 14, 600), (True, 4, 0), (True, 2, 0), (True, 16, 0)]
SUB = {
    0x14: ("SHUTLDR", "Shuttle loader / transport tube", "moves (process_shuttle_loader), collapses"),
    0x18: ("DOME", "Opening dome", "opens and closes (process_dome1)"),
    0x20: ("MGUN", "Mounted gun / turret", "turns and fires (process_mounted_gun), destructible"),
    0x25: ("GATE", "Retractable gate", "opens for vehicles (process_gate1)"),
    0x36: ("MOVN_ROTOR", "Rotor (wind rotor, machinery)", "spins (process_bld36), stored with a rotation matrix"),
    0x37: ("BLD_37", "Subtype 0x37", "stored with a rotation matrix like rotors"),
    0x2F: ("STATION", "Train station", "special collapse (collapse_building_station)"),
    0x1C: ("TRAINTRK", "Train track", "special collapse"),
    0x15: ("BEZIER_ROAD", "Raised curved road", "special collapse"),
    0x1D: ("BLD_1D", "Subtype 0x1D", "special collapse, like track and roads"),
    0x3D: ("BLD_3D", "Subtype 0x3D (unnamed in the source)", "no special processing found in building.c; shown to identify it"),
}


def parse(path):
    d = open(path, "rb").read()
    if struct.unpack_from("<I", d, 0)[0] != 1:
        return None
    p = 4
    counts = []
    for has, _, _ in QL:
        if has:
            counts.append(struct.unpack_from("<H", d, p)[0])
            p += 2
        else:
            counts.append(None)
    arrays = []
    for (has, size, extra), c in zip(QL, counts):
        n, es = (extra + c, size) if has else (size, extra)
        arrays.append((p, n, es))
        p += n * es + 2
    off, recs = inventory_statics.scan(d, 2000)
    if off is None:
        return None
    tp = off + 2 + len(recs) * 60
    nth = struct.unpack_from("<H", d, tp)[0]
    tp += 2
    things = []
    for _ in range(nth):
        if tp + 168 > len(d):
            break
        sub, typ = d[tp + 8], d[tp + 9]
        x, y, z = struct.unpack_from("<iii", d, tp + 24)
        obj = struct.unpack_from("<h", d, tp + 82)[0]
        nob = d[tp + 86]
        tp += 168
        if sub in (0x36, 0x37):
            tp += 36
        things.append((typ, sub, obj, nob, x, y, z))
    return d, arrays, counts, things


def mesh(d, arrays, obj, nob):
    """Triangles of objects obj .. obj+nob-1 (world map units, y up)."""
    (pp, pn, _), (f3p, f3n, _), (op, on, _), (f4p, f4n, _) = arrays[3], arrays[4], arrays[5], arrays[9]
    tris = []
    for o in range(obj, obj + max(1, nob)):
        if o <= 0 or o >= on:
            continue
        sf, nf, _, sf4, nf4, _, _, oy, _, _, mx, mz = struct.unpack_from("<HHHHHhhhhhhh", d, op + o * 36)

        def pt(i):
            if i <= 0 or i >= pn:
                return None
            _, px, py, pz = struct.unpack_from("<Hhhh", d, pp + i * 10)
            return (mx + px, oy + py, mz + pz)
        for f in range(sf, sf + nf):
            if f >= f3n:
                break
            a = struct.unpack_from("<hhh", d, f3p + f * 32)
            v = [pt(i) for i in a]
            if None not in v:
                tris.append(v)
        for f in range(sf4, sf4 + nf4):
            if f >= f4n:
                break
            a = struct.unpack_from("<hhhh", d, f4p + f * 40)
            v = [pt(i) for i in a]
            if None not in v:
                tris += [[v[0], v[1], v[2]], [v[1], v[3], v[2]]]  # quads are stored in strip order 0,1,3,2
    return tris


def render(tris, size=300):
    img = Image.new("RGB", (size, size), (28, 30, 36))
    if not tris:
        return img
    T = np.array(tris, dtype=float)
    # heights are in the same units as x / z (ENGINE_Y_TO_MAP is the identity in the spectator)
    c = T.reshape(-1, 3).mean(0)
    T -= c
    yaw, pitch = math.radians(45), math.radians(30)
    x, y, z = T[..., 0], T[..., 1], T[..., 2]
    xr = x * math.cos(yaw) - z * math.sin(yaw)
    zr = x * math.sin(yaw) + z * math.cos(yaw)
    sx = xr
    sy = -(y * math.cos(pitch) - zr * math.sin(pitch))
    dep = zr * math.cos(pitch) + y * math.sin(pitch)
    span = max(np.ptp(sx), np.ptp(sy), 1e-6)
    k = (size * 0.85) / span
    sx = size / 2 + (sx - (sx.max() + sx.min()) / 2) * k
    sy = size / 2 + (sy - (sy.max() + sy.min()) / 2) * k
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0])
    nl = np.linalg.norm(n, axis=1) + 1e-9
    light = np.abs(n @ np.array([0.4, 0.8, 0.45])) / nl
    order = np.argsort(dep.mean(1))
    dr = ImageDraw.Draw(img)
    for i in order:
        g = int(60 + 170 * light[i])
        dr.polygon([(sx[i, j], sy[i, j]) for j in range(3)], fill=(g, int(g * 0.95), int(g * 1.05)), outline=(15, 16, 20))
    return img


def main():
    per = collections.defaultdict(lambda: {"count": 0, "maps": set(), "shapes": {}})
    allsub = collections.Counter()
    maps = sorted(glob.glob(os.path.join(G, "maps", "*.mad")))
    read = 0
    for m in maps:
        r = parse(m)
        if not r:
            continue
        read += 1
        d, arrays, counts, things = r
        for typ, sub, obj, nob, x, y, z in things:
            if typ != 9:
                continue
            allsub[sub] += 1
            if sub not in SUB:
                continue
            e = per[sub]
            e["count"] += 1
            e["maps"].add(os.path.basename(m))
            tris = mesh(d, arrays, obj, nob)
            key = (len(tris), nob)
            if key not in e["shapes"] and len(e["shapes"]) < 6 and tris:
                e["shapes"][key] = (os.path.basename(m), tris)
    cards = []
    for sub, (code, name, beh) in SUB.items():
        e = per.get(sub)
        if not e or not e["count"]:
            cards.append((code, name, beh, 0, 0, []))
            continue
        imgs = []
        for i, ((nt, nob), (mapn, tris)) in enumerate(e["shapes"].items()):
            fn = "%s_%d.png" % (code, i)
            render(tris).save(os.path.join(OUT, fn))
            imgs.append((fn, "%s, %d triangles, %d object(s)" % (mapn, nt, nob)))
        cards.append((code, name, beh, e["count"], len(e["maps"]), imgs))
    h = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>Moving and special map parts</title><style>"
         "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}.c{background:#2a2b31;border-radius:8px;"
         "padding:14px;margin:0 0 22px}h2{margin:0;color:#ffcc33}.m{color:#aaa;font-size:13px}.r{display:flex;flex-wrap:wrap;"
         "gap:10px;margin-top:8px}.r div{font-size:12px;color:#999;max-width:300px}</style></head><body>"
         "<h1>Moving and special map parts</h1><p>Building things of the %d maps with their own behaviour (the rest are "
         "ordinary buildings, %d placements). Meshes from the map files, flat-shaded, game three-quarter view; up to six "
         "different shapes per type. Nothing generated.</p>" % (read, allsub.get(0x1B, 0))]
    for code, name, beh, n, nm, imgs in cards:
        h.append("<div class=c><h2>%s <span class=m>(SubTT_BLD_%s)</span></h2><div class=m>%s<br>%d placed in %d maps</div>"
                 "<div class=r>%s</div></div>" % (name, code, beh, n, nm,
                                                   "".join("<div><img src='%s'><br>%s</div>" % (f, c) for f, c in imgs)))
    other = ", ".join("0x%02X: %d" % (k, v) for k, v in sorted(allsub.items()))
    h.append("<p class=m>All building subtypes found (subtype: placements): %s</p></body></html>" % other)
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write("\n".join(h))
    print("maps read", read, "of", len(maps))
    for code, name, beh, n, nm, imgs in cards:
        print("%-12s %5d in %2d maps, %d shapes" % (code, n, nm, len(imgs)))
    print("all subtypes:", other)


if __name__ == "__main__":
    main()
