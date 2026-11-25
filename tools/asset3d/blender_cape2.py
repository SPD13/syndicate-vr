"""Blender (background) script: hangs a pleated cloth curtain from the shoulder
pads of a rigged character and bakes a cloth simulation of it into every clip.

blender -b --factory-startup -P blender_cape2.py -- in.glb out.glb [opts.json]

Second generation (the first, blender_cape.py, hung verlet chains from the
pads and is gone). What is different:
- Attach line: across the back just under the base of the helmet canopy
  (found from the Head vertices, or given as `helmet_z`, a fraction of the
  body height, for a cloth hood), then down or up on each side to the
  shoulder pads at `attach_frac` of their height, where it is tucked
  `clearance_pad` inside the pad and ends `front_trim` from the pad's
  middle (negative: behind the hanging arm). The front stays open.
- The whole line is fixed to the chest (`pin_bone`): pads swing with the
  arms and a canopy turns with the head, which wrings a cape hung from them.
- Options in metres are for a character 1.94 m tall and are scaled to the
  model (`unit`); `clip_opts` overrides options per clip, `leg_calm` lets
  the cloth meet a robe that the legs throw about at a calmer place.
- Shape: a curtain. The mesh is finer than the bone grid and carries
  vertical pleats (deeper toward the hem), which close and open as the
  columns move together and apart.
- Motion: a cloth simulation (position based dynamics) on a grid of
  `columns` x (2 * `bones` + 1) points: no stretch along the cloth, free
  bunching across it, collision with the animated body mesh itself (not
  capsules) with friction, floor, air drag, and a head wind in the walk and
  run clips (they are in place). Looping clips are simulated for three
  rounds and the last is kept and closed.
- Bones: `columns` chains of `bones` bones as before (names cape_CC_K), but
  every bone is a child of Spine2 and carries location + rotation keys taken
  straight from the simulation (no constraints to bake), with the cloth's
  own orientation, so the pleats turn with the surface.
"""
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
    "columns": 28, "bones": 10, "mesh_sub_u": 6, "mesh_sub_v": 3,
    "attach_frac": 0.22, "front_trim": -0.07, "clearance": 0.02, "clearance_pad": -0.03, "hem": -0.03,
    "helmet_under": 0.03, "helmet_depth": 0.07, "helmet_z": None, "colour_band": None, "colour_pct": 50, "hood": False, "max_half_width": None, "k_centre": 0.0, "start_angle": 0, "upper_arm": False, "hood_blue": 0.9, "hood_gap": 0.05, "hood_rim": True, "hood_tuck": 0.012, "hood_depth": 0.008, "pleat_top": 0.3, "rise_weight": 0.35, "unit": None,
    "pleat_amp": 0.011, "pleat_per_col": 0.4, "flare": 0.25,
    "margin": 0.045, "margin_top": 0.025, "margin_mid": 0.0, "friction": 0.6, "floor_friction": 0.25, "pin_smooth": 0, "pin_bone": "Spine2", "bounce": 0.0, "arm_r": 0.065, "arm_from": 0.3, "smooth_t": 1.5,
    "drag": 7.0, "max_speed": 4.0, "samples_per_frame": 2, "substeps": 3, "iters": 6, "passes": 2,
    "k_shear": 0.3, "k_bend_v": 0.45, "k_bend_h": 0.3, "k_spread": 0.03, "min_gap": 0.3,
    "colour": [0.86, 0.85, 0.82], "parent": "Spine2", "shape_clip": "idle",
    # backward push (m/s2) on the cloth in the clips that move forward in the game
    "wind": {"walk": 1.0, "pistol_walk": 1.0, "rifle_walk": 1.0, "run": 6.0, "pistol_run": 6.0, "rifle_run": 6.0},
    "looping": ["idle", "walk", "run", "pistol_idle", "pistol_walk", "pistol_run", "pistol_kneel",
                "pistol_shoot", "rifle_idle", "rifle_aim", "rifle_walk", "rifle_run", "rifle_crouch",
                "rifle_crouch_aim", "rifle_shoot"],
    "loop_rounds": 7, "loop_tol": 0.03,
    "leg_calm": 0.0,  # 0..1: the cloth meets the leg-driven parts (robe) partly at their average place over the clip
    "clip_opts": {"run": {"leg_calm": 0.6}, "pistol_run": {"leg_calm": 0.6}, "rifle_run": {"leg_calm": 0.6}},  # per clip overrides of the simulation options, e.g. {"run": {"friction": 0.2}}
    "clips": None,  # test: only simulate these (the others keep the cape at rest)
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
ad = arm.animation_data
tracks = {t.name: t for t in ad.nla_tracks}
for t in ad.nla_tracks:
    t.mute = True
FPS = scene.render.fps / scene.render.fps_base
AW = np.array(arm.matrix_world)


def use_action(act):
    ad.action = act
    if act is not None and hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]


def set_time(t):
    f = int(math.floor(t))
    scene.frame_set(f, subframe=t - f)


body.data.calc_loop_triangles()
TRIS = [tuple(t.vertices) for t in body.data.loop_triangles]
TRI_ARR = np.array(TRIS)
NV = len(body.data.vertices)


def body_verts():
    dg = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(dg)
    me = ev.to_mesh()
    co = np.empty(NV * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    ev.to_mesh_clear()
    co = co.reshape(-1, 3).astype(np.float64)
    mw = np.array(body.matrix_world)
    return co @ mw[:3, :3].T + mw[:3, 3]


def bone_mats(names):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return np.array([AW @ np.array(ev.pose.bones[n].matrix) for n in names])


# ---- shape pose ---------------------------------------------------------------
sact = tracks[opt["shape_clip"]].strips[0].action
use_action(sact)
set_time(sact.frame_range[0])
V0 = body_verts()
gname = {g.index: g.name for g in body.vertex_groups}
vweights = []
dominant = []
for v in body.data.vertices:
    ws = sorted(((g.weight, gname[g.group]) for g in v.groups), reverse=True)[:4]
    s = sum(w for w, _ in ws) or 1.0
    vweights.append([(n, w / s) for w, n in ws])
    dominant.append(ws[0][1] if ws else "")
dominant = np.array(dominant)


def head(name):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return np.array(arm.matrix_world @ ev.pose.bones["mixamorig:" + name].head)


lat = head("LeftArm") - head("RightArm")
lat[2] = 0
lat /= np.linalg.norm(lat)
fwd = np.cross([0, 0, 1.0], lat)
toes = head("LeftToeBase") + head("RightToeBase") - head("LeftFoot") - head("RightFoot")
if fwd @ toes < 0:
    fwd = -fwd
centre = head("Spine2")
floor_z = float(V0[:, 2].min())
# lengths and speeds in the options are metres on a character 1.94 m tall
# (the Church agent); a model of another size has them scaled to it
U = opt["unit"] if opt["unit"] else (float(V0[:, 2].max()) - floor_z) / 1.94
METRIC = ("max_half_width", "front_trim", "clearance", "clearance_pad", "hem", "helmet_under", "helmet_depth", "pleat_amp",
          "margin", "margin_top", "margin_mid", "arm_r", "max_speed", "loop_tol")


def scale_opts(d):
    for k in METRIC:
        if d.get(k) is not None:
            d[k] = d[k] * U
    if "wind" in d:
        d["wind"] = {k: v * U for k, v in d["wind"].items()}


scale_opts(opt)
for _d in opt["clip_opts"].values():
    scale_opts(_d)
print("unit %.3f" % U)
print("facing %s, floor %.3f" % (np.round(fwd, 2), floor_z))

pad = {s: np.nonzero(dominant == "mixamorig:%sArm" % s)[0] for s in ("Left", "Right")}
pz0 = min(V0[pad[s], 2].min() for s in pad)
pz1 = max(V0[pad[s], 2].max() for s in pad)
z_att = pz0 + opt["attach_frac"] * (pz1 - pz0)
print("pads z %.2f..%.2f, attach line at %.2f" % (pz0, pz1, z_att))

# cross-section of the body at the attach height, in (lateral, forward) coordinates
skip = ("ForeArm", "Hand")
edges = np.concatenate([TRI_ARR[:, [0, 1]], TRI_ARR[:, [1, 2]], TRI_ARR[:, [2, 0]]])
za, zb = V0[edges[:, 0], 2], V0[edges[:, 1], 2]
cross = (za - z_att) * (zb - z_att) < 0
edges = edges[cross]
tt = (z_att - V0[edges[:, 0], 2]) / (V0[edges[:, 1], 2] - V0[edges[:, 0], 2])
pts3 = V0[edges[:, 0]] + tt[:, None] * (V0[edges[:, 1]] - V0[edges[:, 0]])
keep = np.array([not any(k in dominant[a] for k in skip) for a in edges[:, 0]])
side = np.array([("L" if dominant[a].endswith("LeftArm") else "R" if dominant[a].endswith("RightArm") else "-")
                 for a in edges[:, 0]])
pts3, side = pts3[keep], side[keep]
P2 = np.stack([(pts3 - centre) @ lat, (pts3 - centre) @ fwd], axis=1)


def hull(p):
    """Convex hull, counter-clockwise (in lateral/forward axes), as indices."""
    idx = sorted(range(len(p)), key=lambda i: (p[i][0], p[i][1]))

    def half(seq):
        h = []
        for i in seq:
            while len(h) >= 2 and np.cross(p[h[-1]] - p[h[-2]], p[i] - p[h[-2]]) <= 0:
                h.pop()
            h.append(i)
        return h
    lo, up = half(idx), half(idx[::-1])
    return lo[:-1] + up[:-1]


def slice_at(z):
    """Cross-section of the body at height z: points (lateral, forward) and the side of each (L/R pad or -)."""
    e = np.concatenate([TRI_ARR[:, [0, 1]], TRI_ARR[:, [1, 2]], TRI_ARR[:, [2, 0]]])
    e = e[(V0[e[:, 0], 2] - z) * (V0[e[:, 1], 2] - z) < 0]
    f = (z - V0[e[:, 0], 2]) / (V0[e[:, 1], 2] - V0[e[:, 0], 2])
    q = V0[e[:, 0]] + f[:, None] * (V0[e[:, 1]] - V0[e[:, 0]])
    ok = np.array([not any(k in dominant[a] for k in skip) for a in e[:, 0]])
    sd_ = np.array([("L" if dominant[a].endswith("LeftArm") else "R" if dominant[a].endswith("RightArm") else "-")
                    for a in e[:, 0]])
    return np.stack([(q - centre) @ lat, (q - centre) @ fwd], axis=1)[ok], sd_[ok]


def densify(p, step=None):
    step = step or 0.005 * U
    out_ = [p[0]]
    for a, b in zip(p[:-1], p[1:]):
        n = max(1, int(np.linalg.norm(b - a) / step))
        out_ += [a + (b - a) * (j + 1) / n for j in range(n)]
    return np.array(out_)


H = hull(P2)
# the front edge of the hull bridges the two pads: the edge crossing lateral 0 furthest forward
best = None
for k in range(len(H)):
    a, b = P2[H[k]], P2[H[(k + 1) % len(H)]]
    if a[0] * b[0] < 0:
        ymid = (a[1] + b[1]) / 2
        if best is None or ymid > best[0]:
            best = (ymid, k)
k = best[1]
ring = [H[(k + 1 + j) % len(H)] for j in range(len(H))]
if P2[ring[0]][0] < 0:  # start at the left pad (lateral > 0)
    ring = ring[::-1]
# each pad's part of the outline: from the front (cut back to `front_trim`
# ahead of the pad's middle, so the front of the pad and the arm stay free)
# around the outside to where the outline leaves the pad at the back
pad_arc = {}
for sname, order in (("L", ring), ("R", ring[::-1])):
    pts = np.array([P2[j] for j in order if side[j] == sname and (P2[j][0] > 0) == (sname == "L")])
    pc = P2[side == sname].mean(axis=0)
    pts = densify(pts)
    keep_ = pts[:, 1] <= pc[1] + opt["front_trim"]
    pts = pts[np.argmax(keep_):]
    # inside the pad: pull the line in toward the pad's middle
    r = pts - pc
    pts = pc + r * (1 + opt["clearance_pad"] / np.maximum(np.linalg.norm(r, axis=1), 1e-6))[:, None]
    pad_arc[sname] = pts
# the base of the helmet canopy at the back: just under the lowest canopy (Head) vertices behind the body
back = -((V0 - centre) @ fwd)
if opt["helmet_z"] is not None:   # given: height of the hood's lower rim, as a fraction of the body height
    z_helm = floor_z + opt["helmet_z"] * (float(V0[:, 2].max()) - floor_z)
else:
    canopy = (dominant == "mixamorig:Head") & (back > 0.12 * U)
    z_helm = float(V0[canopy, 2].min()) - opt["helmet_under"]
Ph, _ = slice_at(z_helm)
hh = Ph[hull(Ph)]
hb = -hh[:, 1]
sel = hh[hb >= hb.max() - opt["helmet_depth"]]
sel = sel[np.argsort(-sel[:, 0])]               # from the left (lateral +) to the right
hc = Ph.mean(axis=0)
helm = densify(sel)
r = helm - hc
helm = hc + r * (1 + opt["clearance"] / np.maximum(np.linalg.norm(r, axis=1), 1e-6))[:, None]
if opt["colour_band"]:
    # the cape continues a cloth hood: take its colour from the body's texture
    # on the back between two heights (fractions of the body height)
    hgt = float(V0[:, 2].max()) - floor_z
    zlo, zhi = (floor_z + f * hgt for f in opt["colour_band"])
    latc = (V0 - centre) @ lat
    inb = (V0[:, 2] >= zlo) & (V0[:, 2] <= zhi) & (latc >= sel[:, 0].min()) & (latc <= sel[:, 0].max())
    inb &= back >= np.percentile(back[inb], 60)
    img = None
    for m_ in body.data.materials:
        for nd in (m_.node_tree.nodes if m_ and m_.use_nodes else []):
            if nd.type == "BSDF_PRINCIPLED" and nd.inputs["Base Color"].is_linked:
                src_ = nd.inputs["Base Color"].links[0].from_node
                if src_.type == "TEX_IMAGE" and src_.image:
                    img = src_.image
    if img is not None:
        w_, h_ = img.size
        px = np.empty(w_ * h_ * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        px = px.reshape(h_, w_, 4)
        uvd = body.data.uv_layers.active.data
        cols_ = []
        for lp in body.data.loops:
            if inb[lp.vertex_index]:
                u_, v_ = uvd[lp.index].uv
                cols_.append(px[int(v_ % 1.0 * (h_ - 1)), int(u_ % 1.0 * (w_ - 1)), :3])
        ca_ = np.array(cols_)
        lum_ = ca_ @ np.array([0.2126, 0.7152, 0.0722])
        c_ = ca_[lum_ >= np.percentile(lum_, opt["colour_pct"])].mean(axis=0)   # the lit cloth: creases are baked dark
        c_ = np.where(c_ <= 0.04045, c_ / 12.92, ((c_ + 0.055) / 1.055) ** 2.4)   # sRGB texels -> linear
        opt["colour"] = [float(x) for x in c_]
        print("cape colour from the hood: %s (%d texels)" % (np.round(c_, 3), len(cols_)))
HOOD3 = None
if opt["hood"]:
    # A cloth hood: the cape is its next layer. The hood is found on the body by
    # its colour in the texture (bluish, not the gold or brown of the rest),
    # spreading from the head over joined vertices; its lower rim is traced
    # from behind, and the cape's top edge is tucked just above and inside it.
    hgt = float(V0[:, 2].max()) - floor_z
    img = None
    for m_ in body.data.materials:
        for nd in (m_.node_tree.nodes if m_ and m_.use_nodes else []):
            if nd.type == "BSDF_PRINCIPLED" and nd.inputs["Base Color"].is_linked:
                src_ = nd.inputs["Base Color"].links[0].from_node
                if src_.type == "TEX_IMAGE" and src_.image:
                    img = src_.image
    w_, h_ = img.size
    px = np.empty(w_ * h_ * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h_, w_, 4)
    nl = len(body.data.loops)
    lv = np.empty(nl, dtype=np.int32)
    body.data.loops.foreach_get("vertex_index", lv)
    luv = np.empty(nl * 2, dtype=np.float32)
    body.data.uv_layers.active.data.foreach_get("uv", luv)
    luv = luv.reshape(-1, 2)
    vuv = np.zeros((NV, 2))
    vuv[lv[::-1]] = luv[::-1]
    vcol = px[(vuv[:, 1] % 1.0 * (h_ - 1)).astype(int), (vuv[:, 0] % 1.0 * (w_ - 1)).astype(int), :3].astype(np.float64)
    # hood cloth: not warm (gold, brown), not bright, and above the waist
    like = (vcol[:, 2] > vcol[:, 0] * opt["hood_blue"]) & (vcol.max(axis=1) < 0.6) & (V0[:, 2] > float(head("Spine")[2]))
    is_hood = np.zeros(NV, dtype=bool)
    is_hood[(dominant == "mixamorig:Head") & like] = True
    nbr = [[] for _ in range(NV)]
    for a_, b_, c_ in TRIS:
        nbr[a_] += [b_, c_]
        nbr[b_] += [a_, c_]
        nbr[c_] += [a_, b_]
    stack = list(np.nonzero(is_hood)[0])
    while stack:
        v_ = stack.pop()
        for o_ in nbr[v_]:
            if like[o_] and not is_hood[o_]:
                is_hood[o_] = True
                stack.append(o_)
    face_hood = is_hood[TRI_ARR].sum(axis=1) >= 2
    torso = np.array([any(k in d_ for k in ("Spine", "Hips")) for d_ in dominant])
    face_torso = torso[TRI_ARR].sum(axis=1) >= 2
    hb_ = BVHTree.FromPolygons(V0.tolist(), TRIS)
    fv = Vector(fwd.tolist())
    z_hi, z_lo = float(head("Head")[2]), float(head("Hips")[2])
    rim = []   # (lateral, forward, z) of the rim seen from behind, per lateral step
    for l_ in np.arange(-0.3 * hgt, 0.3 * hgt, 0.004 * U):
        run, last, below_torso, miss = 0, None, False, 0
        for z_ in np.arange(z_hi, z_lo, -0.003 * U):
            o_ = centre + l_ * lat - fwd * hgt
            o_[2] = z_
            hit = hb_.ray_cast(Vector(o_.tolist()), fv)
            if hit[0] is None:
                continue
            if face_hood[hit[2]]:
                run, last, miss = run + 1, np.array(hit[0]), 0
            elif run >= 5:
                miss += 1     # a crease between two folds may not read as hood: only a longer stretch ends it
                if miss * 0.003 >= opt["hood_gap"]:
                    below_torso = bool(face_torso[hit[2]])
                    break
        if last is not None and run >= 5 and below_torso:
            rim.append((l_, last))
    # the stretch of the rim around the middle of the back, without breaks
    ls_ = np.array([r_[0] for r_ in rim])
    mid_i = int(np.argmin(np.abs(ls_)))
    lo_i = hi_i = mid_i
    while lo_i > 0 and ls_[lo_i] - ls_[lo_i - 1] < 0.013 * U:
        lo_i -= 1
    while hi_i < len(ls_) - 1 and ls_[hi_i + 1] - ls_[hi_i] < 0.013 * U:
        hi_i += 1
    rim = rim[lo_i:hi_i + 1]
    rz = np.array([r_[1][2] for r_ in rim])
    for _ in range(4):
        rz[1:-1] = (rz[:-2] + 2 * rz[1:-1] + rz[2:]) / 4
    pts_ = []
    for (l_, _), z_ in zip(rim, rz):
        o_ = centre + l_ * lat - fwd * hgt
        o_[2] = z_ + opt["hood_tuck"] * U
        hit = hb_.ray_cast(Vector(o_.tolist()), fv)
        if hit[0] is None:
            continue
        q_ = np.array(hit[0]) + fwd * opt["hood_depth"] * U
        pts_.append((l_, (q_ - centre) @ fwd, q_[2]))
    HOOD3 = np.array(pts_)[::-1]            # from the left (lateral +) to the right
    hc_ = vcol[is_hood]
    lum_ = hc_ @ np.array([0.2126, 0.7152, 0.0722])
    c_ = hc_[lum_ >= np.percentile(lum_, opt["colour_pct"])].mean(axis=0)   # the lit cloth: creases are baked dark
    c_ = np.where(c_ <= 0.04045, c_ / 12.92, ((c_ + 0.055) / 1.055) ** 2.4)   # sRGB texels -> linear
    opt["colour"] = [float(x) for x in c_]
    if not opt["hood_rim"]:   # only the colour is taken from the hood; the line is the one at `helmet_z`
        HOOD3 = None
    print("hood: %d vertices, colour %s%s" % (is_hood.sum(), np.round(c_, 3), "" if HOOD3 is None else
          ", rim %.2f m wide at z %.2f..%.2f" % (HOOD3[0, 0] - HOOD3[-1, 0], HOOD3[:, 2].min(), HOOD3[:, 2].max())))
print("helmet base at z %.2f, back span %.2f m; pad arcs %.2f m each" % (
    z_helm, np.linalg.norm(np.diff(helm, axis=0), axis=1).sum(),
    np.linalg.norm(np.diff(pad_arc["L"], axis=0), axis=1).sum()))


def with_z(p2, z):
    return np.concatenate([p2, np.full((len(p2), 1), z)], axis=1)


L3, R3 = with_z(pad_arc["L"], z_att), with_z(pad_arc["R"], z_att)[::-1]
H3 = HOOD3 if HOOD3 is not None else with_z(helm, z_helm)
up_l = densify(np.array([L3[-1], H3[0]]))[1:-1]
up_r = densify(np.array([H3[-1], R3[0]]))[1:-1]
line = np.concatenate([L3, up_l, H3, up_r, R3])          # (n, 3): lateral, forward, z
free = np.zeros(len(line), dtype=bool)                   # not on a pad: kept outside the body
free[len(L3):len(line) - len(R3)] = True
if opt["max_half_width"]:
    # keep the top edge between the arms: a column that starts over a hanging
    # arm has to go through it and may come out on its inner side
    keep_w = np.abs(line[:, 0]) <= opt["max_half_width"]
    line, free = line[keep_w], free[keep_w]
for _ in range(20):  # round the corners
    sm = line.copy()
    sm[1:-1] = (line[:-2] + 2 * line[1:-1] + line[2:]) / 4
    line[free] = sm[free]


def lf_to_world(p):
    return centre + np.outer(p[:, 0], lat) + np.outer(p[:, 1], fwd) + np.outer(p[:, 2] - centre[2], [0, 0, 1.0])


bvh0 = BVHTree.FromPolygons(V0.tolist(), TRIS)
wl = lf_to_world(line)
for _ in range(3):  # the free part of the line stays `clearance` outside the body
    for j in np.nonzero(free)[0]:
        co, n, _, _ = bvh0.find_nearest(wl[j].tolist())
        d = (Vector(wl[j].tolist()) - co).dot(n)
        if d < opt["clearance"]:
            wl[j] += np.array(n) * (opt["clearance"] - d)
# columns: evenly along the line, a rise counting less than a run
C = opt["columns"]
rel = wl - centre
plan = np.stack([rel @ lat, rel @ fwd], axis=1)
ds = np.sqrt((np.diff(plan, axis=0) ** 2).sum(1) + (opt["rise_weight"] * np.diff(wl[:, 2])) ** 2)
acc_d = np.concatenate([[0], np.cumsum(ds)])
ss = np.linspace(0, acc_d[-1], C)
top = np.stack([np.interp(ss, acc_d, wl[:, j]) for j in range(3)], axis=1)   # (C, 3) pins in the shape pose
top2 = np.stack([(top - centre) @ lat, (top - centre) @ fwd], axis=1)
arc = float(np.linalg.norm(np.diff(top2, axis=0), axis=1).sum())
NB = opt["bones"]
P = 2 * NB + 1
length = top[:, 2] - floor_z - opt["hem"]        # (C,) every column reaches the floor
LV = (length / (P - 1))[:, None]                 # (C, 1) sim row spacing of each column
print("cape: arc %.2f m, %d columns, drop %.2f..%.2f m" % (arc, C, length.min(), length.max()))

t2 = np.gradient(top2, axis=0)
for _ in range(2):
    t2[1:-1] = (t2[:-2] + 2 * t2[1:-1] + t2[2:]) / 4
t2 /= np.maximum(np.linalg.norm(t2, axis=1), 1e-9)[:, None]
n2 = np.stack([t2[:, 1], -t2[:, 0]], axis=1)
if (n2 * top2).sum() < 0:
    n2 = -n2
col_n = np.outer(n2[:, 0], lat) + np.outer(n2[:, 1], fwd)    # outward normals
col_t = np.outer(t2[:, 0], lat) + np.outer(t2[:, 1], fwd)    # along the path
LH = np.linalg.norm(np.diff(top2, axis=0), axis=1)           # (C-1,) plan distance between columns
REST = top[:, None, :] + np.arange(P)[None, :, None] * LV[:, :, None] * np.array([0, 0, -1.0])   # (C, P, 3)

# pins follow the body through the skin weights of the nearest vertex, smoothed along the path
if opt["pin_bone"]:
    # the whole attach line is fixed to the chest: the pads swing with the arms
    # and the canopy turns with the head, and a cape hung from them is wrung
    # and lifted on one side in every aiming pose
    pin_bones = ["mixamorig:" + opt["pin_bone"]]
    PW = np.ones((C, 1))
else:
    pin_bones = sorted({n for i in range(C) for n, _ in
                        vweights[int(np.argmin(np.linalg.norm(V0 - top[i], axis=1)))]})
    PW = np.zeros((C, len(pin_bones)))
    for i in range(C):
        for n, w in vweights[int(np.argmin(np.linalg.norm(V0 - top[i], axis=1)))]:
            PW[i, pin_bones.index(n)] += w
for _ in range(opt["pin_smooth"]):
    PW[1:-1] = (PW[:-2] + 2 * PW[1:-1] + PW[2:]) / 4
PW /= PW.sum(1, keepdims=True)
M0inv = np.linalg.inv(bone_mats(pin_bones))
print("pins follow", ", ".join("%s %.0f%%" % (n.split(":")[1], 100 * PW[:, j].mean()) for j, n in enumerate(pin_bones)))
sd = []
for i in range(C):
    co, n, _, _ = bvh0.find_nearest(top[i].tolist())
    sd.append((Vector(top[i].tolist()) - co).dot(n))
print("pin distance to the body: %.3f..%.3f m (must be positive: outside)" % (min(sd), max(sd)))


def pins_now():
    M = bone_mats(pin_bones) @ M0inv                     # (nb, 4, 4)
    h = np.concatenate([top, np.ones((C, 1))], axis=1)   # (C, 4)
    return np.einsum("cb,bij,cj->ci", PW, M, h)[:, :3]


# ---- cape mesh (bind shape: hanging straight down from the shape-pose pins) -------
SU, SV = opt["mesh_sub_u"], opt["mesh_sub_v"]
MC, MR = (C - 1) * SU + 1, NB * SV + 1
rng = np.random.default_rng(7)
jit = rng.uniform(-0.25, 0.25, C + 1)
amp_u = rng.uniform(0.75, 1.2, C + 1)
verts, uvs = [], []
skin = []  # per vertex: [(bone name, weight)]
for a in range(MC):
    u = a / SU
    i0 = min(C - 2, int(u))
    fu = u - i0
    base = top[i0] * (1 - fu) + top[i0 + 1] * fu
    len_u = length[i0] * (1 - fu) + length[i0 + 1] * fu
    nn = col_n[i0] * (1 - fu) + col_n[i0 + 1] * fu
    nn /= np.linalg.norm(nn)
    ph = 2 * math.pi * (u * opt["pleat_per_col"] + np.interp(u, np.arange(C + 1), jit))
    au = np.interp(u, np.arange(C + 1), amp_u)
    for b in range(MR):
        v = b / (MR - 1)
        amp = opt["pleat_amp"] * au * (opt["pleat_top"] + (1 - opt["pleat_top"]) * min(1.0, v / 0.2)) * (1 + 0.35 * v)
        if a in (0, MC - 1):
            amp *= 0.5
        p = base + nn * amp * math.sin(ph) + np.array([0, 0, -len_u * v])
        verts.append(tuple(p))
        uvs.append((a / (MC - 1), 1 - v))
        x = b / SV                      # position along the chain in bones
        k0 = min(NB - 1, int(x - 1e-9)) if x > 0 else 0
        f = x - k0
        if f < 0.5:
            kw = [(k0, 0.5 + f), (k0 - 1, 0.5 - f)] if k0 > 0 else [(k0, 1.0)]
        else:
            kw = [(k0, 1.5 - f), (k0 + 1, f - 0.5)] if k0 < NB - 1 else [(k0, 1.0)]
        ws = []
        for ci, cw in ((i0, 1 - fu), (i0 + 1, fu)):
            for kk, w in kw:
                if cw * w > 1e-4:
                    ws.append(("cape_%02d_%d" % (ci, kk), cw * w))
        skin.append(ws)
faces = [(a * MR + b, (a + 1) * MR + b, (a + 1) * MR + b + 1, a * MR + b + 1)
         for a in range(MC - 1) for b in range(MR - 1)]
me = bpy.data.meshes.new("cape")
me.from_pydata(verts, [], faces)
me.update()
uvl = me.uv_layers.new(name="UVMap")
for poly in me.polygons:
    for li in poly.loop_indices:
        uvl.data[li].uv = uvs[me.loops[li].vertex_index]
    poly.use_smooth = True
cape = bpy.data.objects.new("cape", me)
scene.collection.objects.link(cape)
mat = bpy.data.materials.new("cape_cloth")
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get("Principled BSDF")
bsdf.inputs["Base Color"].default_value = tuple(opt["colour"]) + (1.0,)
bsdf.inputs["Roughness"].default_value = 0.85
mat.use_backface_culling = False
mat.diffuse_color = tuple(opt["colour"]) + (1.0,)
me.materials.append(mat)
print("cape mesh: %d x %d vertices, %d triangles" % (MC, MR, 2 * len(faces)))

# ---- bones ------------------------------------------------------------------------
use_action(None)
PARENT = "mixamorig:" + opt["parent"]
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
inv_arm = arm.matrix_world.inverted()
LB = length / NB
for i in range(C):
    for kk in range(NB):
        eb = arm.data.edit_bones.new("cape_%02d_%d" % (i, kk))
        h = Vector((top[i] + np.array([0, 0, -LB[i] * kk])).tolist())
        eb.head = inv_arm @ h
        eb.tail = inv_arm @ (h + Vector((0, 0, -LB[i])))
        eb.align_roll(inv_arm.to_3x3() @ Vector(col_n[i].tolist()))
        eb.parent = arm.data.edit_bones[PARENT]
        eb.use_connect = False
bpy.ops.object.mode_set(mode="OBJECT")
cape.parent = arm
mod = cape.modifiers.new("Armature", "ARMATURE")
mod.object = arm
vg = {}
for vi, ws in enumerate(skin):
    for n, w in ws:
        if n not in vg:
            vg[n] = cape.vertex_groups.new(name=n)
        vg[n].add([vi], w, "REPLACE")
R0 = np.array([[np.array(arm.data.bones["cape_%02d_%d" % (i, kk)].matrix_local) for kk in range(NB)]
               for i in range(C)])                       # (C, NB, 4, 4) armature space
PR_inv = np.linalg.inv(np.array(arm.data.bones[PARENT].matrix_local))
# sign of the bones' X axis against the path direction, to build the same frame from the simulation
xsign = np.sign(np.einsum("ci,ci->c", R0[:, 0, :3, 0], col_t @ AW[:3, :3]))
print("bones: %d chains x %d, children of %s" % (C, NB, opt["parent"]))

# ---- simulation ---------------------------------------------------------------------
W = np.ones((C, P))
W[:, 0] = 0.0
G = np.array([0, 0, -9.81 * U])


def project(A, B, wa, wb, rest, ks, kc):
    d = B - A
    n = np.linalg.norm(d, axis=-1)
    n = np.where(n > 1e-9, n, 1e-9)
    c = n - rest
    k = np.where(c > 0, ks, kc)
    ws = wa + wb
    ws = np.where(ws > 0, ws, 1.0)
    corr = (k * c / (n * ws))[..., None] * d
    A += wa[..., None] * corr
    B -= wb[..., None] * corr


# flared cut: the cloth is wider toward the hem than the attach line (the
# spare width is what the folds hold), so it can lie around the wide robe.
# Rest lengths come from the hanging shape (columns differ in length).
FLARE = 1 + opt["flare"] * np.clip(np.arange(P) / (0.3 * (P - 1)), 0, 1)        # (P,)


def rest_len(ia, ka, ib, kb):
    dh = np.linalg.norm(top2[ib] - top2[ia], axis=-1)[:, None] * (FLARE[ka] + FLARE[kb])[None, :] / 2
    dz = REST[ib][:, kb, 2] - REST[ia][:, ka, 2]
    return np.sqrt(dh ** 2 + dz ** 2)


_ci, _ck = np.arange(C - 1), np.arange(P)
LHK = rest_len(_ci, _ck, _ci + 1, _ck)                       # (C-1, P) across
SH1 = rest_len(_ci, _ck[:-1], _ci + 1, _ck[1:])              # (C-1, P-1) shear down to the next column
SH2 = rest_len(_ci + 1, _ck[:-1], _ci, _ck[1:])              # (C-1, P-1) shear the other way


def constraints(X):
    lh = LHK
    for par in (0, 1):  # across: no stretch, free bunching down to min_gap, weak spread
        A, B = X[par:C - 1:2], X[par + 1:C:2]
        m = min(len(A), len(B))
        A, B = A[:m], B[:m]
        r = lh[par:C - 1:2][:m]
        project(A, B, W[par:C - 1:2][:m], W[par + 1:C:2][:m], r, 1.0, opt["k_spread"])
        d = np.linalg.norm(B - A, axis=-1)
        if (d < r * opt["min_gap"]).any():
            project(A, B, W[par:C - 1:2][:m], W[par + 1:C:2][:m], r * opt["min_gap"], 0.0, 1.0)
    for sx in (1, -1):  # shear: stretch only
        A = X[:-1, :-1] if sx == 1 else X[1:, :-1]
        B = X[1:, 1:] if sx == 1 else X[:-1, 1:]
        wa = W[:-1, :-1] if sx == 1 else W[1:, :-1]
        wb = W[1:, 1:] if sx == 1 else W[:-1, 1:]
        r = SH1 if sx == 1 else SH2
        for par in (0, 1):  # two passes over disjoint column pairs
            project(A[par::2], B[par::2], wa[par::2], wb[par::2], r[par::2], opt["k_shear"], 0.0)
    for off in (0, 1, 2):  # bending along the drop: resist folding only
        A, B = X[:, off:P - 2:3], X[:, off + 2:P:3]
        m = min(A.shape[1], B.shape[1])
        project(A[:, :m], B[:, :m], W[:, off:P - 2:3][:, :m], W[:, off + 2:P:3][:, :m], 2 * LV, 0.0, opt["k_bend_v"])
    for off in (0, 1, 2):  # bending across
        A, B = X[off:C - 2:3], X[off + 2:C:3]
        m = min(len(A), len(B))
        r = (LHK[:-1] + LHK[1:])[off:C - 2:3][:m]
        project(A[:m], B[:m], W[off:C - 2:3][:m], W[off + 2:C:3][:m], r, 0.0, opt["k_bend_h"])
    for par in (0, 1):  # along the drop: fixed length
        A, B = X[:, par:P - 1:2], X[:, par + 1:P:2]
        m = min(A.shape[1], B.shape[1])
        project(A[:, :m], B[:, :m], W[:, par:P - 1:2][:, :m], W[:, par + 1:P:2][:, :m], LV, 1.0, 1.0)


MARGIN_ROW = opt["margin_top"] + (opt["margin"] - opt["margin_top"]) * np.clip((np.arange(1, P) - 1) / 4.0, 0, 1)
MARGIN_FLAT = np.tile(MARGIN_ROW, C)


def collide(Xn, Xold, bvh, Va, Vb, frac_dt, limit=None):
    """Pushes the points out of the body (nearest surface point, along its
    normal) with friction against the moving surface."""
    limit = limit or 0.15 * U
    fn = bvh.find_nearest
    # the middles of the cloth's squares must stay outside too: a knee or a pad
    # can come through between four points
    mid = (Xn[:-1, :-1] + Xn[1:, :-1] + Xn[:-1, 1:] + Xn[1:, 1:]) / 4
    mm = opt["margin_mid"]
    corr = np.zeros_like(Xn)
    cnt = np.zeros((C, P, 1))
    any_mid = False
    for idx, x in enumerate(mid.reshape(-1, 3).tolist() if mm > 0 else []):
        r = fn(x, limit)
        if r[0] is None:
            continue
        co, n, fi, dist = r
        d = (x[0] - co[0]) * n[0] + (x[1] - co[1]) * n[1] + (x[2] - co[2]) * n[2]
        if d < mm:
            i, k = divmod(idx, P - 1)
            c = np.array(n) * (mm - d)
            corr[i:i + 2, k:k + 2] += c
            cnt[i:i + 2, k:k + 2] += 1
            any_mid = True
    if any_mid:
        corr /= np.maximum(cnt, 1)
        corr[:, 0] = 0
        Xn += corr
    flat = Xn[:, 1:].reshape(-1, 3)
    old = Xold[:, 1:].reshape(-1, 3)
    margin = MARGIN_FLAT
    hit_i, hit_p, hit_n, hit_f = [], [], [], []
    for idx, x in enumerate(flat.tolist()):
        r = fn(x, limit)
        if r[0] is None:
            continue
        co, n, fi, dist = r
        d = (x[0] - co[0]) * n[0] + (x[1] - co[1]) * n[1] + (x[2] - co[2]) * n[2]
        if d < margin[idx]:
            hit_i.append(idx)
            hit_p.append((co[0], co[1], co[2], d, margin[idx]))
            hit_n.append((n[0], n[1], n[2]))
            hit_f.append(fi)
    if hit_i:
        hi = np.array(hit_i)
        hp = np.array(hit_p)
        hn = np.array(hit_n)
        x = flat[hi] + hn * (hp[:, 4] - hp[:, 3])[:, None]
        tri = TRI_ARR[np.array(hit_f)]
        surf = (Vb[tri] - Va[tri]).mean(axis=1) * frac_dt   # surface movement in this substep
        rel = (x - old[hi]) - surf
        rel_t = rel - hn * (rel * hn).sum(1)[:, None]
        x -= opt["friction"] * rel_t
        flat[hi] = x
    Xn[:, 1:] = flat.reshape(C, P - 1, 3)
    low = Xn[:, 1:, 2] < floor_z + 0.012 * U
    if low.any():
        sub = Xn[:, 1:]
        o = Xold[:, 1:]
        sub[low, 2] = floor_z + 0.012 * U
        sub[low, :2] -= opt["floor_friction"] * (sub[low, :2] - o[low, :2])


def arm_capsules():
    """Forearm and hand of each side as capsules, with the outward direction
    of that side: the cloth is always moved to the outside of an arm, never
    between the arm and the body, so a swinging arm parts the curtain instead
    of threading through it."""
    l = head("LeftArm") - head("RightArm")
    l[2] = 0
    l /= np.linalg.norm(l)
    f = np.cross([0, 0, 1.0], l)
    if f @ fwd < 0:
        f = -f
    res = []
    for s, sg in (("Left", 1.0), ("Right", -1.0)):
        a, b, c = head(s + "ForeArm"), head(s + "Hand"), head(s + "HandMiddle1")
        tip = c + (c - b) * 1.2
        lean = 0.3 * sg * l - f   # the arms hang in front of the cape's edges: push it out and back
        if opt["upper_arm"]:
            res.append((head(s + "Arm"), a, lean))
        res.append((a + (b - a) * opt["arm_from"], b, lean))
        res.append((b, tip, lean))
    return res


def collide_arms(Xn, capsules):
    R = opt["arm_r"]
    if R <= 0:
        return
    Y = Xn[:, 1:].reshape(-1, 3)
    for a, b, d in capsules:
        ab = b - a
        for _ in range(2):
            t = np.clip(((Y - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
            v = Y - (a + t[:, None] * ab)
            n2 = (v * v).sum(1)
            inside = n2 < R * R
            if not inside.any():
                break
            vi = v[inside]
            u = vi / np.maximum(np.sqrt(n2[inside]), 1e-9)[:, None] + d   # away from the arm, leaning outward
            un = np.linalg.norm(u, axis=1)
            u = np.where(un[:, None] > 1e-3, u / np.maximum(un, 1e-9)[:, None], d)
            vd = (vi * u).sum(1)
            lam = -vd + np.sqrt(np.maximum(vd * vd - (n2[inside] - R * R), 0))
            Y[inside] += lam[:, None] * u
    Xn[:, 1:] = Y.reshape(C, P - 1, 3)


class Sim:
    def __init__(self, pins):
        self.X = pins[:, None, :] + (REST - REST[:, :1])
        self.Xp = self.X.copy()

    def advance(self, pin_a, pin_b, col_a, col_b, dt, wind, capsules):
        """One body sample interval: pins move pin_a -> pin_b, body col_a -> col_b."""
        ns = opt["substeps"]
        h = dt / ns
        for s in range(ns):
            f = (s + 1) / ns
            X, Xp = self.X, self.Xp
            vel = (X - Xp) / h
            sp = np.linalg.norm(vel, axis=-1, keepdims=True)
            vel *= np.minimum(1.0, opt["max_speed"] / np.maximum(sp, 1e-9))
            acc = G + wind - opt["drag"] * vel
            Xn = X + vel * h + acc * h * h
            Xn[:, 0] = pin_a + (pin_b - pin_a) * f
            if opt["k_centre"]:
                # sideways hold: every strip of the cloth is drawn back under its own
                # attach point (along the attach line only; free to swing back and forth),
                # so a narrow cape does not roll off a rounded back to one side
                ld = Xn[0, 0] - Xn[-1, 0]
                ld[2] = 0
                ld /= max(np.linalg.norm(ld), 1e-9)
                off = (Xn[:, 1:] - Xn[:, :1]) @ ld
                # ... or a little further out toward the hem, by the flare of the cut
                want = ((Xn[:, 0] - Xn[:, 0].mean(axis=0)) @ ld)[:, None] * (FLARE[None, 1:] - 1)
                Xn[:, 1:] -= opt["k_centre"] * (off - want)[:, :, None] * ld
            push = np.zeros_like(Xn)
            for _ in range(opt["passes"]):
                for _ in range(opt["iters"]):
                    constraints(Xn)
                before = Xn.copy()
                collide_arms(Xn, capsules)
                collide(Xn, X, col_b[0], col_a[1], col_b[1], 1.0 / ns)
                push += Xn - before
            # no stretch along the drop, from the pins down, then out of the body once more
            for k in range(1, P if opt.get('ftl', 0) else 0):
                d = Xn[:, k] - Xn[:, k - 1]
                n = np.linalg.norm(d, axis=1)
                long = n > LV[:, 0]
                if long.any():
                    Xn[long, k] = Xn[long, k - 1] + d[long] * (LV[long, 0] / n[long])[:, None]
            before = Xn.copy()
            collide(Xn, X, col_b[0], col_a[1], col_b[1], 1.0 / ns)
            push += Xn - before
            # the push out of the body does not become speed (no bounce off the body)
            pn = np.linalg.norm(push, axis=-1, keepdims=True)
            pdir = push / np.maximum(pn, 1e-9)
            step = Xn - X
            out_speed = np.maximum(0.0, (step * pdir).sum(-1, keepdims=True)) * (pn > 1e-6)
            self.Xp, self.X = X + (1 - opt["bounce"]) * out_speed * pdir, Xn


def mat_to_quat(R):
    """(..., 3, 3) rotation matrices -> (..., 4) quaternions w, x, y, z."""
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


def channelbag(act):
    slot = ad.action_slot if hasattr(ad, "action_slot") else None
    if slot is not None and getattr(act, "layers", None):
        return act.layers[0].strips[0].channelbag(slot, ensure=True)
    return act


def write_curves(act, bone, path, values, times):
    """values (n, k) -> k fcurves of n linear keys."""
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


AWinv = np.linalg.inv(AW)
t_start = time.time()
LEG_W = np.array([sum(w for n, w in ws if any(k in n for k in ("UpLeg", "Leg", "Foot", "Toe"))) for ws in vweights])
# the cape at rest on the shape pose; every clip starts from it and the body
# is moved from the shape pose into the clip's first pose under the cloth, so
# the cloth never starts inside a bent body
use_action(sact)
set_time(sact.frame_range[0])
caps_shape = arm_capsules()
col_shape = (bvh0, V0)
sim0 = Sim(top)
if opt["start_angle"]:
    # start swept back behind the body and let it fall onto the back: hanging
    # straight down from the line it can start inside a back that sticks out
    # further, and is then pushed out to whichever side is nearer
    ang = math.radians(opt["start_angle"])
    drop = (REST[:, :1, 2] - REST[:, :, 2])[:, :, None]            # (C, P, 1) distance down the column
    sim0.X = top[:, None, :] + drop * (np.array([0, 0, -math.cos(ang)]) - fwd * math.sin(ang))
    sim0.Xp = sim0.X.copy()
for _ in range(int((1.5 + (1.5 if opt["start_angle"] else 0)) * FPS * opt["samples_per_frame"])):
    sim0.advance(top, top, col_shape, col_shape, 1.0 / FPS / opt["samples_per_frame"], np.zeros(3), caps_shape)
X_SHAPE = sim0.X.copy()
only = opt.get("clips")
base_opt = dict(opt)
for name, track in tracks.items():
    opt.clear()
    opt.update(base_opt)
    opt.update(base_opt["clip_opts"].get(name, {}))
    act = track.strips[0].action
    use_action(act)
    f0, f1 = float(act.frame_range[0]), float(act.frame_range[1])
    ns = max(2, int(round((f1 - f0) * opt["samples_per_frame"])))
    ts = f0 + np.arange(ns + 1) * (f1 - f0) / ns
    dt = (f1 - f0) / ns / FPS
    simulate = only is None or name in only
    pins, cols, pmats, hips, caps = [], [], [], [], []
    for t in (ts if simulate else ts[:1]):
        set_time(float(t))
        Vt = body_verts()
        pins.append(pins_now())
        cols.append((BVHTree.FromPolygons(Vt.tolist(), TRIS), Vt))
        pmats.append(bone_mats([PARENT])[0])
        caps.append(arm_capsules())
        hips.append(head("Hips"))
    if not simulate:
        continue
    if opt["leg_calm"] > 0:
        # a robe skinned to running legs is thrown about like no cloth would be:
        # let the cape meet a calmer robe, between its real and its average place
        Vall = np.array([c[1] for c in cols])
        calm = (opt["leg_calm"] * LEG_W)[None, :, None]
        Vall = Vall + calm * (Vall.mean(axis=0, keepdims=True) - Vall)
        cols = [(BVHTree.FromPolygons(v.tolist(), TRIS), v) for v in Vall]
    hips = np.array(hips)
    loop = name in opt["looping"]
    wind_speed = opt["wind"].get(name, 0.0)
    f_now = np.cross([0, 0, 1.0], pins[0][0] - pins[0][-1])
    f_now /= np.linalg.norm(f_now)
    if f_now @ fwd < 0:  # the clips are in place and face the way of the shape pose (a foot in mid-step may not)
        f_now = -f_now
    wind = -f_now * wind_speed
    sim = Sim(top)
    sim.X, sim.Xp = X_SHAPE.copy(), X_SHAPE.copy()
    n_in = max(2, int(opt.get("lead_in", 1.6) / dt))
    prev_c, prev_p = col_shape, top
    for q in range(1, n_in + 1):  # move the body from the shape pose into the first pose
        a = q / n_in
        Vq = (1 - a) * V0 + a * cols[0][1]
        cq = (BVHTree.FromPolygons(Vq.tolist(), TRIS), Vq)
        pq = (1 - a) * top + a * pins[0]
        sim.advance(prev_p, pq, prev_c, cq, dt, wind, caps[0] if a > 0.5 else caps_shape)
        prev_c, prev_p = cq, pq
    for _ in range(int(0.8 / dt)):  # settle on the first pose
        sim.advance(pins[0], pins[0], cols[0], cols[0], dt, wind, caps[0])
    rec = []
    if loop:
        # round after round until the cloth repeats itself (or the best of `loop_rounds`)
        best_rec, gap_max = None, 1e9
        for rnd in range(opt["loop_rounds"]):
            cur = [sim.X.copy()]
            for j in range(ns):
                a, b = j, (j + 1) % ns
                sim.advance(pins[a], pins[b], cols[a], cols[b], dt, wind, caps[b])
                cur.append(sim.X.copy())
            g = float(np.abs(cur[0] - cur[-1]).max())
            if rnd >= 1 and g < gap_max:
                best_rec, gap_max = np.array(cur), g
            if rnd >= 1 and g < opt["loop_tol"]:
                break
        rec = best_rec
        gap = rec[0] - rec[-1]
        rec += np.linspace(0, 1, ns + 1)[:, None, None, None] * gap  # close the loop
    else:
        rec.append(sim.X.copy())
        for j in range(ns):
            sim.advance(pins[j], pins[j + 1], cols[j], cols[j + 1], dt, wind, caps[j + 1])
            rec.append(sim.X.copy())
        rec = np.array(rec)
        gap_max = 0.0
    if opt["smooth_t"] > 0:  # take the shiver out: a short blur over time (around the loop for loops)
        sg = opt["smooth_t"]
        kr = int(3 * sg) + 1
        kern = np.exp(-0.5 * (np.arange(-kr, kr + 1) / sg) ** 2)
        kern /= kern.sum()
        body_t = rec[:-1] if loop else rec
        n_t = len(body_t)
        acc_s = np.zeros_like(body_t)
        for o, w in zip(range(-kr, kr + 1), kern):
            idx = (np.arange(n_t) + o) % n_t if loop else np.clip(np.arange(n_t) + o, 0, n_t - 1)
            acc_s += w * body_t[idx]
        acc_s[:, :, 0] = body_t[:, :, 0]   # the pins stay exact
        rec = np.concatenate([acc_s, acc_s[:1]]) if loop else acc_s
    stretch = (np.linalg.norm(np.diff(rec, axis=2), axis=-1).sum(-1) / length[None, :]).max() - 1
    if opt.get("debug_dir"):
        np.save("%s/rec_%s.npy" % (opt["debug_dir"], name), rec)
    # bones from the simulation: origin at every second row, Y down the chain, X along the cloth
    N = rec[:, :, ::2]                                     # (T, C, NB+1, 3)
    Tn = np.empty_like(N)
    Tn[:, 1:-1] = N[:, 2:] - N[:, :-2]
    Tn[:, 0] = N[:, 1] - N[:, 0]
    Tn[:, -1] = N[:, -1] - N[:, -2]
    Y = N[:, :, 1:] - N[:, :, :-1]
    Y /= np.maximum(np.linalg.norm(Y, axis=-1, keepdims=True), 1e-9)
    Xa = (Tn[:, :, 1:] + Tn[:, :, :-1]) * xsign[None, :, None, None]
    Xa -= Y * (Xa * Y).sum(-1, keepdims=True)
    Xa /= np.maximum(np.linalg.norm(Xa, axis=-1, keepdims=True), 1e-9)
    Z = np.cross(Xa, Y)
    Mw = np.zeros(Y.shape[:-1] + (4, 4))
    Mw[..., :3, 0], Mw[..., :3, 1], Mw[..., :3, 2], Mw[..., :3, 3] = Xa, Y, Z, N[:, :, :-1]
    Mw[..., 3, 3] = 1.0
    pm = np.array(pmats if not loop else pmats[:ns] + [pmats[0]])   # (T, 4, 4) world
    Lm = np.einsum("tij,jk,cbkl->tcbil", AWinv @ pm, PR_inv, R0)    # rest of the bone under the posed parent
    basis = np.linalg.inv(Lm) @ (AWinv @ Mw)
    loc = basis[..., :3, 3]
    quat = mat_to_quat(basis[..., :3, :3])
    for t in range(1, len(quat)):  # keep the quaternions on one side
        flip = (quat[t] * quat[t - 1]).sum(-1) < 0
        quat[t][flip] *= -1
    for i in range(C):
        for kk in range(NB):
            bn = "cape_%02d_%d" % (i, kk)
            arm.pose.bones[bn].rotation_mode = "QUATERNION"
            write_curves(act, bn, "location", loc[:, i, kk], ts)
            write_curves(act, bn, "rotation_quaternion", quat[:, i, kk], ts)
    # check: the baked bones stand where the simulation put them
    tchk = len(ts) // 2
    set_time(float(ts[tchk]))
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    err = max((arm.matrix_world @ ev.pose.bones["cape_%02d_%d" % (i, kk)].head
               - Vector(N[tchk, i, kk].tolist())).length for i in range(0, C, 5) for kk in range(NB))
    print("clip %-18s %3d samples%s wind %.1f  loop gap %.3f m  stretch %+.1f%%  bake error %.4f m  hips travel %.2f m  (%.0f s)"
          % (name, ns, " loop" if loop else "     ", wind_speed, gap_max, 100 * stretch, err,
             float(np.linalg.norm(hips[:, :2].max(0) - hips[:, :2].min(0))), time.time() - t_start), flush=True)
    del cols

use_action(None)
for t in ad.nla_tracks:
    t.mute = False
scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
