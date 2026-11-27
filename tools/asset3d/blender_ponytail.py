"""Blender (background) script: frees a ponytail fused into a rigged character
(Tripo makes one surface, skinned rigidly to the Head) and gives it a
simulated bone chain baked into every clip.

blender -b --factory-startup -P blender_ponytail.py -- in.glb out.glb [opts.json]

- Finds the ponytail: the surface outside a sphere around the skull (from the
  Head and HeadTop_End bones) that is skinned to the Head or has the hair's
  colour, grown from the Head-skinned part over the welded surface; the
  largest connected piece is the tail. Its root is where it leaves the
  sphere (the hair tie).
- Centre line: the tail's points ordered by their distance over the surface
  from the root, averaged in slices; `bones` + 1 nodes spaced evenly along it.
- Bones `ponytail_0..` form a chain under the Head; the tail's vertices are
  re-weighted along it (smooth blend between neighbouring bones, blending
  into the Head at the root over `root_blend` bones).
- Motion: a chain simulation (position based dynamics): fixed segment
  lengths, a bend stiffness toward the sculpted curve, the first nodes held
  in the hair tie (`k_root`), gravity, air drag, a head wind in the walk and
  run clips (they are in place), collision of the nodes and of points
  between them (with the tail's thickness) against the animated body
  (the tail's own triangles left out) and the floor. Every clip starts from
  the tail hanging at rest in the shape pose; looping clips are simulated
  round after round until they repeat, then closed.
- Keys: location + rotation of every bone (the chain's own hierarchy), so a
  runtime spring-bone system can take over the same chain in the engine.
"""
import heapq
import json
import math
import sys
import time

import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
opt = {
    "bones": 6,
    "skull_c": 0.4, "skull_r": 0.6,        # skull sphere: centre at Head + skull_c * (HeadTop - Head), radius skull_r * |HeadTop - Head|
    "col_tol": 0.15,                        # hair colour distance (0..1 RGB) for vertices not skinned to the Head
    "root_blend": 0.7,                      # bones over which the tail blends into the Head at the root
    "k_root": [1.0, 0.08, 0.0],             # hold of node 1, 2, 3 at their place in the hair tie (relative to the head)
    "k_bend": 0.15, "k_stretch": 1.0,
    "drag": 0.8, "max_speed": 6.0, "gravity": 9.81,
    "samples_per_frame": 2, "substeps": 4, "iters": 6,
    "radius_scale": 0.9, "radius_min": 0.02, "friction": 0.3, "mids": 2,
    "shape_clip": "idle", "settle": 1.0, "shape_settle": 3.0,
    "wind": {"walk": 1.5, "pistol_walk": 1.5, "rifle_walk": 1.5, "run": 7.0, "pistol_run": 7.0, "rifle_run": 7.0},
    "looping": ["idle", "walk", "run", "pistol_idle", "pistol_walk", "pistol_run", "pistol_kneel",
                "pistol_shoot", "rifle_idle", "rifle_aim", "rifle_walk", "rifle_run", "rifle_crouch",
                "rifle_crouch_aim", "rifle_shoot"],
    "loop_rounds": 6, "loop_tol": 0.01, "smooth_t": 1.0,
    "clips": None, "debug_dir": None,
}
if len(args) > 2:
    opt.update(json.load(open(args[2])))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
scene = bpy.context.scene
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
body = max((o for o in bpy.data.objects if o.type == "MESH" and o.parent is arm
            and not o.name.startswith("weapon_")), key=lambda o: len(o.data.vertices))
PRE = "mixamorig:" if "mixamorig:Head" in arm.data.bones else ""
HEAD = PRE + "Head"
ad = arm.animation_data
tracks = {t.name: t for t in ad.nla_tracks}
for t in ad.nla_tracks:
    t.mute = True
FPS = scene.render.fps / scene.render.fps_base
AW = np.array(arm.matrix_world)
AWinv = np.linalg.inv(AW)
MW = np.array(body.matrix_world)


def use_action(act):
    ad.action = act
    if act is not None and hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]


def set_time(t):
    f = int(math.floor(t))
    scene.frame_set(f, subframe=t - f)


def rest_head(name):
    return (AW @ np.append(np.array(arm.data.bones[PRE + name].head_local), 1.0))[:3]


NV = len(body.data.vertices)
VR = np.array([v.co[:] for v in body.data.vertices]) @ MW[:3, :3].T + MW[:3, 3]   # rest (bind) pose, world

# ---- find the ponytail (rest pose) --------------------------------------------------
gname = {g.index: g.name for g in body.vertex_groups}
dom = np.array([max(((g.weight, gname[g.group]) for g in v.groups), default=(0, ""))[1] for v in body.data.vertices])
hd, ht = rest_head("Head"), rest_head("HeadTop_End")
CS = hd + opt["skull_c"] * (ht - hd)
RS = opt["skull_r"] * np.linalg.norm(ht - hd)

col = np.full((NV, 3), -1.0)
img = None
for m in body.data.materials:
    for n in (m.node_tree.nodes if m and m.node_tree else []):
        if n.type == "TEX_IMAGE" and n.image and img is None:
            img = n.image
if img is not None:
    Wi, Hi = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(Hi, Wi, 4)
    uvl = body.data.uv_layers.active.data
    nl = len(body.data.loops)
    uv = np.empty(nl * 2, dtype=np.float32)
    uvl.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    lv = np.empty(nl, dtype=np.int32)
    body.data.loops.foreach_get("vertex_index", lv)
    c = px[np.clip((uv[:, 1] % 1) * Hi, 0, Hi - 1).astype(int), np.clip((uv[:, 0] % 1) * Wi, 0, Wi - 1).astype(int), :3]
    acc = np.zeros((NV, 3))
    cnt = np.zeros(NV)
    np.add.at(acc, lv, c)
    np.add.at(cnt, lv, 1)
    col = acc / np.maximum(cnt, 1)[:, None]

# weld the vertices split at UV seams
_, first, wid = np.unique(np.round(VR, 5), axis=0, return_index=True, return_inverse=True)
wid = wid.ravel()
NW = len(first)
WP = VR[first]
E = np.empty(len(body.data.edges) * 2, dtype=np.int64)
body.data.edges.foreach_get("vertices", E)
E = wid[E.reshape(-1, 2)]
E = E[E[:, 0] != E[:, 1]]
adj = [[] for _ in range(NW)]
for a, b in E:
    adj[a].append(b)
    adj[b].append(a)
w_head = np.zeros(NW, bool)
np.logical_or.at(w_head, wid, dom == HEAD)
w_col = np.zeros((NW, 3))
np.add.at(w_col, wid, col)
w_col /= np.bincount(wid, minlength=NW)[:, None]
rw = np.linalg.norm(WP - CS, axis=1)
outside = rw > RS
seed = outside & w_head
hair = np.median(w_col[seed], axis=0)
cand = outside & (w_head | (np.abs(w_col - hair).max(1) < opt["col_tol"]))
lab = np.full(NW, -1)
nc = 0
for s in np.where(seed)[0]:
    if lab[s] >= 0:
        continue
    stack = [s]
    lab[s] = nc
    while stack:
        a = stack.pop()
        for b in adj[a]:
            if cand[b] and lab[b] < 0:
                lab[b] = nc
                stack.append(b)
    nc += 1
sizes = np.bincount(lab[lab >= 0], minlength=nc)
tail_w = lab == int(np.argmax(sizes))
# root ring: tail points next to the skull
ring = [a for a in np.where(tail_w)[0] if any(not outside[b] for b in adj[a])]
g = np.full(NW, np.inf)
hq = []
for a in ring:
    g[a] = 0.0
    hq.append((0.0, a))
heapq.heapify(hq)
while hq:
    d, a = heapq.heappop(hq)
    if d > g[a]:
        continue
    for b in adj[a]:
        if tail_w[b]:
            nd = d + float(np.linalg.norm(WP[a] - WP[b]))
            if nd < g[b]:
                g[b] = nd
                heapq.heappush(hq, (nd, b))
tail_w &= np.isfinite(g)
tw = np.where(tail_w)[0]
gmax = float(g[tw].max())
print("ponytail: %d of %d points, %d at the root, %.2f m over the surface (other pieces: %s)"
      % (len(tw), NW, len(ring), gmax, sorted(sizes.tolist(), reverse=True)[1:6]), flush=True)

# centre line from slices along the surface distance
NBIN = 24
edges_g = np.linspace(0, gmax, NBIN + 1)
cent, rad, gmid = [WP[ring].mean(0)], [], [0.0]
for i in range(NBIN):
    m = tw[(g[tw] >= edges_g[i]) & (g[tw] <= edges_g[i + 1])]
    if len(m) < 3:
        continue
    c = WP[m].mean(0)
    cent.append(c)
    gmid.append(float(g[m].mean()))
cent = np.array(cent)
for _ in range(2):   # smooth the line (ends kept)
    cent[1:-1] = 0.25 * cent[:-2] + 0.5 * cent[1:-1] + 0.25 * cent[2:]
seg = np.linalg.norm(np.diff(cent, axis=0), axis=1)
arc = np.concatenate([[0], np.cumsum(seg)])
# the last slice's centre is short of the tip: extend the line to the tip's distance
tipx = cent[-1] + (cent[-1] - cent[-2]) / max(seg[-1], 1e-9) * max(0.0, gmax - gmid[-1]) * 0.7
cent = np.vstack([cent, tipx])
arc = np.append(arc, arc[-1] + np.linalg.norm(tipx - cent[-2]))
gmid.append(gmax)
L = float(arc[-1])
NB = opt["bones"]
sn = np.linspace(0, L, NB + 1)
NODE = np.array([[np.interp(s, arc, cent[:, k]) for k in range(3)] for s in sn])
# surface distance -> place along the line, in bone units
u_w = np.interp(g, gmid, arc) / L * NB
# thickness of the tail around each node
dist_line = np.full(NW, np.inf)
for i in range(len(cent) - 1):
    a, b = cent[i], cent[i + 1]
    ab = b - a
    t = np.clip(((WP[tw] - a) @ ab) / max(ab @ ab, 1e-12), 0, 1)
    dist_line[tw] = np.minimum(dist_line[tw], np.linalg.norm(WP[tw] - (a + t[:, None] * ab), axis=1))
RAD = np.array([np.median(dist_line[tw][np.abs(u_w[tw] - k) < 0.6]) if np.any(np.abs(u_w[tw] - k) < 0.6) else opt["radius_min"]
                for k in range(NB + 1)])
RAD = np.maximum(RAD * opt["radius_scale"], opt["radius_min"])
print("centre line %.2f m, %d bones, radius %s, nodes %s" % (L, NB, np.round(RAD, 3).tolist(), np.round(NODE, 3).tolist()), flush=True)

# ---- bones and weights --------------------------------------------------------------
BN = ["ponytail_%d" % k for k in range(NB)]
lat = rest_head("LeftArm") - rest_head("RightArm")
fwd = np.cross(lat, [0, 0, 1.0])
fwd /= np.linalg.norm(fwd)
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
inv_arm = arm.matrix_world.inverted()
prev = arm.data.edit_bones[HEAD]
for k in range(NB):
    eb = arm.data.edit_bones.new(BN[k])
    eb.head = inv_arm @ Vector(NODE[k].tolist())
    eb.tail = inv_arm @ Vector(NODE[k + 1].tolist())
    eb.align_roll(inv_arm.to_3x3() @ Vector(fwd.tolist()))
    eb.parent = prev
    eb.use_connect = False
    prev = eb
bpy.ops.object.mode_set(mode="OBJECT")

tail_v = np.where(tail_w[wid])[0]
u_v = u_w[wid[tail_v]]
for vg in body.vertex_groups:
    vg.remove(tail_v.tolist())
vgs = {n: body.vertex_groups.get(n) or body.vertex_groups.new(name=n) for n in BN + [HEAD]}
for vi, u in zip(tail_v.tolist(), u_v.tolist()):
    hb = float(np.clip(1 - u / opt["root_blend"], 0, 1)) if opt["root_blend"] > 0 else 0.0
    x = float(np.clip(u - 0.5, 0, NB - 1))
    k0 = min(int(x), NB - 1)
    f = x - k0
    ws = {BN[k0]: (1 - f) * (1 - hb)}
    if f > 0 and k0 + 1 < NB:
        ws[BN[k0 + 1]] = f * (1 - hb)
    if hb > 0:
        ws[HEAD] = hb
    for n, w in ws.items():
        if w > 1e-4:
            vgs[n].add([vi], w, "REPLACE")
print("re-weighted %d vertices" % len(tail_v), flush=True)

BREST = [np.array(arm.data.bones[n].matrix_local) for n in BN]     # armature space
HREST = np.array(arm.data.bones[HEAD].matrix_local)
HREST_W = AW @ HREST
REST_LEN = np.linalg.norm(np.diff(NODE, axis=0), axis=1)
REST_BEND = np.linalg.norm(NODE[2:] - NODE[:-2], axis=1)


def unit_cols(M3):
    return M3 / np.linalg.norm(M3, axis=0, keepdims=True)


BREST_W_ROT = [unit_cols((AW @ B)[:3, :3]) for B in BREST]

# collision mesh: the body without the tail
body.data.calc_loop_triangles()
TRIS_ALL = np.array([tuple(t.vertices) for t in body.data.loop_triangles])
in_tail = np.zeros(NV, bool)
in_tail[tail_v] = True
TRIS = TRIS_ALL[~in_tail[TRIS_ALL].any(1)]
TRIS_L = TRIS.tolist()


def body_verts():
    dg = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(NV * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    co = co.reshape(-1, 3).astype(np.float64)
    return co @ MW[:3, :3].T + MW[:3, 3]


def head_pose():
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return np.array(ev.pose.bones[HEAD].matrix)        # armature space


def facing():
    """Forward of the posed body (from the hips: they are rotated from the bind pose in the clips)."""
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    lat = np.array(ev.pose.bones[PRE + "LeftUpLeg"].head) - np.array(ev.pose.bones[PRE + "RightUpLeg"].head)
    f = AW[:3, :3] @ np.cross(lat, np.linalg.inv(AW[:3, :3]) @ np.array([0, 0, 1.0]))
    f[2] = 0
    return f / max(np.linalg.norm(f), 1e-9)


def sample():
    V = body_verts()
    Hp = head_pose()
    D = (AW @ Hp) @ np.linalg.inv(HREST_W)             # rest -> now, world, rigid
    T = NODE @ D[:3, :3].T + D[:3, 3]
    return {"bvh": BVHTree.FromPolygons(V.tolist(), TRIS_L), "V": V, "Hp": Hp, "D": D, "T": T, "fwd": facing()}


# ---- simulation ---------------------------------------------------------------------
G = np.array([0, 0, -opt["gravity"]])
WINV = np.ones(NB + 1)
WINV[0] = 0.0
KR = list(opt["k_root"])
if KR and KR[0] >= 1.0:
    WINV[1] = 0.0
NOCOL = 2   # nodes 0, 1 sit in the hair tie
MIDT = [(k, j / (opt["mids"] + 1)) for k in range(1, NB) for j in range(1, opt["mids"] + 1)]


def constraints(X, T):
    for i, s in enumerate(KR):
        k = i + 1
        if k <= NB:
            X[k] += s * (T[k] - X[k])
    for k in range(NB):
        d = X[k + 1] - X[k]
        n = np.linalg.norm(d)
        ws = WINV[k] + WINV[k + 1]
        if n < 1e-9 or ws == 0:
            continue
        c = opt["k_stretch"] * (n - REST_LEN[k]) / n * d / ws
        X[k] += WINV[k] * c
        X[k + 1] -= WINV[k + 1] * c
    if opt["k_bend"] > 0:
        for k in range(NB - 1):
            d = X[k + 2] - X[k]
            n = np.linalg.norm(d)
            ws = WINV[k] + WINV[k + 2]
            if n < 1e-9 or ws == 0 or n >= REST_BEND[k]:
                continue   # resist folding only (a straighter tail is free)
            c = opt["k_bend"] * (n - REST_BEND[k]) / n * d / ws
            X[k] += WINV[k] * c
            X[k + 2] -= WINV[k + 2] * c


def collide(X, Xold, smp_a, smp_b, frac):
    bvh = smp_b["bvh"]
    Va, Vb = smp_a["V"], smp_b["V"]
    pts = [(k, None, 0.0) for k in range(NOCOL, NB + 1)] + [(k, k + 1, t) for k, t in MIDT]
    for ka, kb, t in pts:
        x = X[ka] if kb is None else X[ka] * (1 - t) + X[kb] * t
        r = RAD[ka] if kb is None else RAD[ka] * (1 - t) + RAD[kb] * t
        hit = bvh.find_nearest(Vector(x.tolist()), r + 0.08)
        if hit[0] is None:
            continue
        co, n, fi, _ = hit
        n = np.array(n)
        d = float((x - np.array(co)) @ n)
        if d >= r:
            continue
        push = n * (r - d)
        if kb is None:
            xn = X[ka] + push
            tri = TRIS[fi]
            surf = (Vb[tri] - Va[tri]).mean(0) * frac
            rel = (xn - Xold[ka]) - surf
            rel_t = rel - n * (rel @ n)
            X[ka] = xn - opt["friction"] * rel_t
        else:
            wa, wb = (1 - t) * WINV[ka], t * WINV[kb]
            s = wa * wa + wb * wb
            if s > 0:
                X[ka] += push * wa / s
                X[kb] += push * wb / s
    low = X[:, 2] < FLOOR + RAD
    low[:NOCOL] = False
    X[low, 2] = (FLOOR + RAD)[low]


class Sim:
    def __init__(self, X):
        self.X = X.copy()
        self.Xp = X.copy()

    def advance(self, a, b, dt, wind):
        ns = opt["substeps"]
        h = dt / ns
        for s in range(ns):
            f = (s + 1) / ns
            X, Xp = self.X, self.Xp
            T = a["T"] + (b["T"] - a["T"]) * f
            vel = (X - Xp) / h
            sp = np.linalg.norm(vel, axis=1, keepdims=True)
            vel *= np.minimum(1.0, opt["max_speed"] / np.maximum(sp, 1e-9))
            Xn = X + vel * h + (G + wind - opt["drag"] * vel) * h * h
            Xn[0] = T[0]
            for _ in range(opt["iters"]):
                constraints(Xn, T)
            before = Xn.copy()
            collide(Xn, X, a, b, 1.0 / ns)
            for _ in range(2):
                constraints(Xn, T)
            push = Xn - before
            pn = np.linalg.norm(push, axis=1, keepdims=True)
            pdir = push / np.maximum(pn, 1e-9)
            out_speed = np.maximum(0.0, ((Xn - X) * pdir).sum(1, keepdims=True)) * (pn > 1e-6)
            self.Xp, self.X = X + out_speed * pdir, Xn


def mat_to_quat(R):
    m00, m11, m22 = R[..., 0, 0], R[..., 1, 1], R[..., 2, 2]
    q = np.empty(R.shape[:-2] + (4,))
    q[..., 0] = np.sqrt(np.maximum(0, 1 + m00 + m11 + m22)) / 2
    q[..., 1] = np.sqrt(np.maximum(0, 1 + m00 - m11 - m22)) / 2
    q[..., 2] = np.sqrt(np.maximum(0, 1 - m00 + m11 - m22)) / 2
    q[..., 3] = np.sqrt(np.maximum(0, 1 - m00 - m11 + m22)) / 2
    q[..., 1] = np.copysign(q[..., 1], R[..., 2, 1] - R[..., 1, 2])
    q[..., 2] = np.copysign(q[..., 2], R[..., 0, 2] - R[..., 2, 0])
    q[..., 3] = np.copysign(q[..., 3], R[..., 1, 0] - R[..., 0, 1])
    return q / np.linalg.norm(q, axis=-1, keepdims=True)


def min_rot(a, b):
    """Rotation matrix turning unit vector a onto unit vector b."""
    v = np.cross(a, b)
    c = float(a @ b)
    if c < -0.9999:
        p = np.cross(a, [1.0, 0, 0])
        if np.linalg.norm(p) < 1e-3:
            p = np.cross(a, [0, 1.0, 0])
        p /= np.linalg.norm(p)
        return 2 * np.outer(p, p) - np.eye(3)
    K = np.array([[0, -v[2], v[1]], [v[2], 0, -v[0]], [-v[1], v[0], 0]])
    return np.eye(3) + K + K @ K / (1 + c)


def channelbag(act):
    slot = ad.action_slot if hasattr(ad, "action_slot") else None
    if slot is not None and getattr(act, "layers", None):
        return act.layers[0].strips[0].channelbag(slot, ensure=True)
    return act


def write_curves(act, bone, path, values, times):
    bag = channelbag(act)
    dp = 'pose.bones["%s"].%s' % (bone, path)
    n = len(times)
    for ix in range(values.shape[1]):
        fc = bag.fcurves.find(dp, index=ix)
        if fc is None:
            try:
                fc = bag.fcurves.new(dp, index=ix, group_name=bone)
            except TypeError:
                fc = bag.fcurves.new(dp, index=ix)
        fc.keyframe_points.clear()
        fc.keyframe_points.add(n)
        kp = np.empty(n * 2, dtype=np.float32)
        kp[0::2] = times
        kp[1::2] = values[:, ix]
        fc.keyframe_points.foreach_set("co", kp)
        fc.keyframe_points.foreach_set("interpolation", np.ones(n, dtype=np.int32))
        fc.update()


def bake(act, rec, smps, ts):
    """rec (T, NB+1, 3) world nodes -> location + rotation keys of the chain."""
    nT = len(rec)
    locs = np.zeros((NB, nT, 3))
    quats = np.zeros((NB, nT, 4))
    for t in range(nT):
        Hp = smps[t]["Hp"]
        Drot = unit_cols(smps[t]["D"][:3, :3])
        parent_pose, parent_rest = Hp, HREST
        for k in range(NB):
            Q = Drot @ BREST_W_ROT[k]                      # the bone carried rigidly with the head
            y = rec[t, k + 1] - rec[t, k]
            y /= max(np.linalg.norm(y), 1e-9)
            F = min_rot(Q[:, 1], y) @ Q
            Mw = np.eye(4)
            Mw[:3, :3], Mw[:3, 3] = F, rec[t, k]
            Ma = AWinv @ Mw
            Ma[:3, :3] = unit_cols(Ma[:3, :3])
            basis = np.linalg.inv(parent_pose @ np.linalg.inv(parent_rest) @ BREST[k]) @ Ma
            locs[k, t] = basis[:3, 3]
            quats[k, t] = mat_to_quat(basis[:3, :3])
            parent_pose, parent_rest = Ma, BREST[k]
    for k in range(NB):
        q = quats[k]
        for t in range(1, nT):
            if q[t] @ q[t - 1] < 0:
                q[t] *= -1
        pb = arm.pose.bones[BN[k]]
        pb.rotation_mode = "QUATERNION"
        write_curves(act, BN[k], "location", locs[k], ts)
        write_curves(act, BN[k], "rotation_quaternion", q, ts)


# shape pose: the tail falls from its sculpted place and settles
sact = tracks[opt["shape_clip"]].strips[0].action
use_action(sact)
set_time(float(sact.frame_range[0]))
S0 = sample()
FLOOR = float(S0["V"][:, 2].min())
sim = Sim(S0["T"])
dt0 = 1.0 / FPS / opt["samples_per_frame"]
for _ in range(int(opt["shape_settle"] / dt0)):
    sim.advance(S0, S0, dt0, np.zeros(3))
D0inv = np.linalg.inv(S0["D"])
X_SHAPE_H = (sim.X - S0["D"][:3, 3]) @ np.linalg.inv(S0["D"][:3, :3]).T   # in the head's rest frame
drop = np.degrees(np.arccos(np.clip(((NODE[-1] - NODE[0]) / L) @ ((X_SHAPE_H[-1] - X_SHAPE_H[0]) / max(np.linalg.norm(X_SHAPE_H[-1] - X_SHAPE_H[0]), 1e-9)), -1, 1)))
print("shape pose: tail settled, turned %.0f deg from the sculpt" % drop, flush=True)

t_start = time.time()
only = opt.get("clips")
for name, track in tracks.items():
    if only and name not in only:
        continue
    act = track.strips[0].action
    use_action(act)
    f0, f1 = float(act.frame_range[0]), float(act.frame_range[1])
    ns = max(2, int(round((f1 - f0) * opt["samples_per_frame"])))
    ts = f0 + np.arange(ns + 1) * (f1 - f0) / ns
    dt = (f1 - f0) / ns / FPS
    smps = []
    for t in ts:
        set_time(float(t))
        smps.append(sample())
    loop = name in opt["looping"]
    w_speed = opt["wind"].get(name, 0.0)
    fw = np.mean([s["fwd"] for s in smps], axis=0)
    wind = -fw / max(np.linalg.norm(fw), 1e-9) * w_speed
    s0 = smps[0]
    X = X_SHAPE_H @ s0["D"][:3, :3].T + s0["D"][:3, 3]
    sim = Sim(X)
    for _ in range(int(opt["settle"] / dt)):
        sim.advance(s0, s0, dt, wind)
    if loop:
        best, gap_best = None, 1e9
        for rnd in range(opt["loop_rounds"]):
            cur = [sim.X.copy()]
            for j in range(ns):
                sim.advance(smps[j], smps[(j + 1) % ns], dt, wind)
                cur.append(sim.X.copy())
            gp = float(np.abs(cur[0] - cur[-1]).max())
            if rnd >= 1 and gp < gap_best:
                best, gap_best = np.array(cur), gp
            if rnd >= 1 and gp < opt["loop_tol"]:
                break
        rec = best
        rec += np.linspace(0, 1, ns + 1)[:, None, None] * (rec[0] - rec[-1])
        smps_b = smps[:ns] + [smps[0]]
    else:
        rec = [sim.X.copy()]
        for j in range(ns):
            sim.advance(smps[j], smps[j + 1], dt, wind)
            rec.append(sim.X.copy())
        rec = np.array(rec)
        gap_best = 0.0
        smps_b = smps
    if opt["smooth_t"] > 0:
        sg = opt["smooth_t"]
        kr = int(3 * sg) + 1
        kern = np.exp(-0.5 * (np.arange(-kr, kr + 1) / sg) ** 2)
        kern /= kern.sum()
        bt = rec[:-1] if loop else rec
        n_t = len(bt)
        acc = np.zeros_like(bt)
        for o, w in zip(range(-kr, kr + 1), kern):
            idx = (np.arange(n_t) + o) % n_t if loop else np.clip(np.arange(n_t) + o, 0, n_t - 1)
            acc += w * bt[idx]
        # the pinned root follows the head exactly
        acc[:, 0] = bt[:, 0]
        if WINV[1] == 0:
            acc[:, 1] = bt[:, 1]
        rec = np.concatenate([acc, acc[:1]]) if loop else acc
    if opt.get("debug_dir"):
        np.save("%s/pt_%s.npy" % (opt["debug_dir"], name), rec)
    bake(act, rec, smps_b, ts)
    stretch = (np.linalg.norm(np.diff(rec, axis=1), axis=-1) / REST_LEN[None]).max() - 1
    tchk = len(ts) // 2
    set_time(float(ts[tchk]))
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    err = max(np.linalg.norm(np.array(arm.matrix_world @ ev.pose.bones[BN[k]].head) - rec[tchk, k]) for k in range(NB))
    tip_sw = float(np.linalg.norm(rec[:, -1] - rec[:, -1].mean(0), axis=1).max())
    print("clip %-18s %3d samples%s wind %.1f  loop gap %.3f m  stretch %+.1f%%  tip swing %.2f m  bake error %.4f m  (%.0f s)"
          % (name, ns, " loop" if loop else "     ", w_speed, gap_best, 100 * stretch, tip_sw, err, time.time() - t_start),
          flush=True)
    del smps, smps_b

use_action(None)
for t in ad.nla_tracks:
    t.mute = False
scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
