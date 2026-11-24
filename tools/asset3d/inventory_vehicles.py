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


# --- textured render of one prim object (reference images for redraws) -------
def textured_tris(o):
    """[(3 points, 3 uv, page)] of prim object o, with the game's own textures."""
    d, arr, nob, base = load_prims()
    _, npt, nf3, nf4, nobj, ntex4, ntex, _ = struct.unpack_from("<i7H", d, 0)
    t4p = 18 + npt * 10 + nf3 * 32 + nf4 * 40 + nobj * 36
    t3p = t4p + ntex4 * 18
    (pp, pn, _), (f3p, f3n, _), (op, on, _), (f4p, f4n, _) = arr[3], arr[4], arr[5], arr[9]
    sf, nf, _, sf4, nf4_ = struct.unpack_from("<HHHHH", d, op + o * 36)
    mx, mz = struct.unpack_from("<hh", d, op + o * 36 + 20)
    oy = struct.unpack_from("<h", d, op + o * 36 + 14)[0]

    def pt(i):
        _, px, py, pz = struct.unpack_from("<Hhhh", d, pp + i * 10)
        return (mx + px, oy + py, mz + pz)
    out = []
    for f in range(sf, sf + nf):
        a = struct.unpack_from("<hhh", d, f3p + f * 32)
        t = struct.unpack_from("<H", d, f3p + f * 32 + 6)[0]
        if t and t < ntex:
            u = struct.unpack_from("<7B", d, t3p + t * 16)
            out.append(([pt(i) for i in a], [(u[0], u[1]), (u[2], u[3]), (u[4], u[5])], u[6]))
        else:
            out.append(([pt(i) for i in a], None, 0))
    for f in range(sf4, sf4 + nf4_):
        a = struct.unpack_from("<hhhh", d, f4p + f * 40)
        t = struct.unpack_from("<H", d, f4p + f * 40 + 8)[0]
        v = [pt(i) for i in a]
        if t and t < ntex4:
            u = struct.unpack_from("<9B", d, t4p + t * 18)
            uv = [(u[0], u[1]), (u[2], u[3]), (u[4], u[5]), (u[6], u[7])]
            out.append(([v[0], v[1], v[2]], [uv[0], uv[1], uv[2]], u[8]))
            out.append(([v[1], v[3], v[2]], [uv[1], uv[3], uv[2]], u[8]))
        else:
            out += [([v[0], v[1], v[2]], None, 0), ([v[1], v[3], v[2]], None, 0)]
    return out


def render_textured(tris, yaw_deg=45, pitch_deg=30, size=640, flat=False):
    import math
    import numpy as np
    from PIL import Image
    pal = swdata.load_palette(G)
    pages = [np.frombuffer(open(os.path.join(G, "data", "tex%02d.dat" % i), "rb").read(), dtype=np.uint8).reshape(256, 256)
             for i in range(6)]
    P = np.array([t[0] for t in tris], dtype=float)
    P -= P.reshape(-1, 3).mean(0)
    yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
    x, y, z = P[..., 0], P[..., 1], P[..., 2]
    xr = x * math.cos(yaw) - z * math.sin(yaw)
    zr = x * math.sin(yaw) + z * math.cos(yaw)
    sx, sy = xr, -(y * math.cos(pitch) - zr * math.sin(pitch))
    dep = zr * math.cos(pitch) + y * math.sin(pitch)
    k = size * 0.86 / max(np.ptp(sx), np.ptp(sy))
    sx = size / 2 + (sx - (sx.max() + sx.min()) / 2) * k
    sy = size / 2 + (sy - (sy.max() + sy.min()) / 2) * k
    img = np.full((size, size, 3), 255, dtype=np.uint8)
    zb = np.full((size, size), -1e18)
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    shade = 0.55 + 0.45 * np.abs(n @ np.array([0.4, 0.8, 0.45])) / (np.linalg.norm(n, axis=1) + 1e-9)
    for i, (pts, uv, page) in enumerate(tris):
        X, Y, D = sx[i], sy[i], dep[i]
        x0, x1 = int(max(0, X.min())), int(min(size - 1, X.max())) + 1
        y0, y1 = int(max(0, Y.min())), int(min(size - 1, Y.max())) + 1
        if x1 <= x0 or y1 <= y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1) + 0.5, np.arange(y0, y1) + 0.5)
        den = (Y[1] - Y[2]) * (X[0] - X[2]) + (X[2] - X[1]) * (Y[0] - Y[2])
        if abs(den) < 1e-9:
            continue
        l0 = ((Y[1] - Y[2]) * (gx - X[2]) + (X[2] - X[1]) * (gy - Y[2])) / den
        l1 = ((Y[2] - Y[0]) * (gx - X[2]) + (X[0] - X[2]) * (gy - Y[2])) / den
        l2 = 1 - l0 - l1
        m = (l0 >= -0.01) & (l1 >= -0.01) & (l2 >= -0.01)
        dz = l0 * D[0] + l1 * D[1] + l2 * D[2]
        sub = zb[y0:y1, x0:x1]
        m &= dz > sub
        if not m.any():
            continue
        if uv is not None and page < 6:
            U = l0 * uv[0][0] + l1 * uv[1][0] + l2 * uv[2][0]
            V = l0 * uv[0][1] + l1 * uv[1][1] + l2 * uv[2][1]
            c = pal[pages[page][np.clip(V, 0, 255).astype(int), np.clip(U, 0, 255).astype(int)]].astype(float)
            if flat:  # one average colour per face: the colour layout without the pixel texture
                c = np.broadcast_to(c[m].mean(0), c.shape).copy()
        else:
            c = np.full(gx.shape + (3,), 150.0)
        c = np.clip(c * shade[i], 0, 255).astype(np.uint8)
        img[y0:y1, x0:x1][m] = c[m]
        sub[m] = dz[m]
    return Image.fromarray(img)
