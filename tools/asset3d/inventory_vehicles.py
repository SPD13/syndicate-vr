"""Catalog of the vehicle meshes (qdata/primveh.obj), private review page.

python inventory_vehicles.py
primveh.obj: s32 header, 7 ushort counts (points, faces3, faces4, objects,
floor textures, face textures, base), then SinglePoint[], SingleObjectFace3[],
SingleObjectFace4[], SingleObject[] (layouts as in the maps). A vehicle uses
prim object (base + StartFrame) (lvfiles.c, copy_prim_obj_to_game_object).
Every prim object is rendered (flat-shaded, game three-quarter view); the
level files are scanned for vehicle things (Type 2, 168-byte records) to
show which vehicle subtypes use which mesh and how often. Writes
assets3d/review/vehicles/index.html. Nothing is generated.
"""
import collections
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inventory_buildings as ib  # noqa: E402
import swdata  # noqa: E402

G = swdata.find_gamedata()
OUT = os.path.join(os.path.dirname(G), "assets3d", "review", "vehicles")
os.makedirs(OUT, exist_ok=True)
SUBNAMES = {0x1D: "TRAIN", 0x28: "GROUND (car)", 0x29: "SHUTTLE_POD", 0x32: "FLYING", 0x33: "TANK", 0x36: "SHIP",
            0x3B: "MECH"}
CODE_SPAWNED = {19: "turret put on vehicles (vehicle.c, mounted gun)", 27: "spawned in vehicle.c", 30: "spawned in vehicle.c"}


def load_prims():
    d = open(os.path.join(G, "qdata", "primveh.obj"), "rb").read()
    _, npt, nf3, nf4, nob, ntex4, ntex, base = struct.unpack_from("<i7H", d, 0)
    p = 18
    arrays = {}
    for key, n, es in (("pt", npt, 10), ("f3", nf3, 32), ("f4", nf4, 40), ("ob", nob, 36)):
        arrays[key] = (p, n, es)
        p += n * es
    # the map catalog's mesh() expects the quick-load array order
    arr = [None] * 10
    arr[3], arr[4], arr[5], arr[9] = arrays["pt"], arrays["f3"], arrays["ob"], arrays["f4"]
    return d, arr, nob, base


def scan_levels(nob):
    """(subtype, StartFrame) -> count, from vehicle things found in the level files."""
    use = collections.Counter()
    where = collections.defaultdict(set)
    for f in sorted(glob.glob(os.path.join(G, "levels", "*.dat"))):
        d = open(f, "rb").read()
        for off in range(0, len(d) - 168 - 36):
            if d[off + 9] != 2:
                continue
            sub = d[off + 8]
            if not 0x1D <= sub <= 0x3C:
                continue
            sf = struct.unpack_from("<H", d, off + 38)[0]
            if sf >= nob:
                continue
            x, y, z = struct.unpack_from("<iii", d, off + 24)
            if not (0 < x < (128 << 16) and 0 < z < (128 << 16) and -(1 << 20) < y < (1 << 22)):
                continue
            # the 3x3 rotation that follows a vehicle: rows of unit length (2^14 fixed point)
            m = struct.unpack_from("<9i", d, off + 168)
            ok = all(abs((m[3 * r] ** 2 + m[3 * r + 1] ** 2 + m[3 * r + 2] ** 2) ** 0.5 - 16384) < 300 for r in range(3))
            if not ok:
                continue
            use[(sub, sf)] += 1
            where[(sub, sf)].add(os.path.basename(f))
    return use, where


def main():
    d, arr, nob, base = load_prims()
    use, where = scan_levels(nob)
    by_obj = collections.defaultdict(list)
    for (sub, sf), n in use.items():
        by_obj[base + sf].append((sub, n, len(where[(sub, sf)])))
    cards = []
    for o in range(1, nob):
        tris = ib.mesh(d, arr, o, 1)
        if not tris:
            continue
        fn = "prim_%02d.png" % o
        ib.render(tris).save(os.path.join(OUT, fn))
        users = sorted(by_obj.get(o, []), key=lambda u: -u[1])
        label = ", ".join("%s (0x%02X): %d in %d levels" % (SUBNAMES.get(s, "UNKN%d" % s), s, n, nl) for s, n, nl in users)
        if o in CODE_SPAWNED:
            label = (label + "; " if label else "") + CODE_SPAWNED[o]
        cards.append((o, fn, len(tris), label or "not placed in the levels scanned"))
    h = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>Vehicles - catalog</title><style>"
         "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}.g{display:flex;flex-wrap:wrap;gap:14px}"
         ".c{background:#2a2b31;border-radius:8px;padding:10px;width:300px}.l{font-size:22px;font-weight:700;color:#ffcc33}"
         ".m{color:#aaa;font-size:13px}</style></head><body><h1>Vehicles - catalog</h1>"
         "<p>All meshes of <code>qdata/primveh.obj</code> (%d objects), flat-shaded from the game's three-quarter view, "
         "numbered V#. Under each: the vehicle subtypes placed with it in the levels (found by scanning the level files) "
         "and how often. Nothing generated.</p><div class=g>" % (nob - 1)]
    for o, fn, nt, label in cards:
        h.append("<div class=c><div class=l>V%d</div><img src='%s'><div class=m>%d triangles<br>%s</div></div>" % (o, fn, nt, label))
    h.append("</div></body></html>")
    open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write("\n".join(h))
    print("meshes", len(cards), "of", nob - 1, "; vehicle placements found", sum(use.values()))
    for o, fn, nt, label in cards:
        print("V%-3d %4d tris  %s" % (o, nt, label))


if __name__ == "__main__":
    main()
