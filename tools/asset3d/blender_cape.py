"""Blender (background) script: adds a simulated cloth cape to a rigged character.

blender -b --factory-startup -P blender_cape.py -- in.glb out.glb [opts.json]

The cape is generated, not taken from the scanned model, so it is never
fused with armour or legs:
- Shape: an arc around the body at `attach_z` (fraction of the shoulder
  height offset), from the front end of the left shoulder (angle
  `front_deg` from the facing) around the back to the front end of the
  right shoulder. Each column starts outside the body surface at that
  height (ray cast from outside, + clearance) and hangs straight down to
  the floor, pushed outward wherever the body (robe) is wider.
- Bones: one chain of `bones` bones per column, parented to the side's
  Shoulder bone (front/side columns) or Spine2 (back columns). The top row
  is pinned to the parent.
- Motion: per clip (NLA track) a verlet simulation of all chains together
  (gravity, damping, pull toward the hanging shape relative to the parent,
  fixed segment lengths, links between neighbouring columns, collision with
  capsules around legs, robe, torso and arms), baked into the clip via
  Damped Track constraints. Looping clips are simulated twice and the
  second pass is kept.
Options: columns 28, rows 32, bones 6, front_deg 55, attach_dz 0.02,
clearance 0.025 (m), stiffness 0.05, damping 0.9, iters 6, link_k 0.8,
colour [0.86, 0.85, 0.82], robe_r 0.14, leg_r 0.07, torso_r 0.16, arm_r 0.06
(m or fractions of height as noted in the code).
"""
import json
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
opt = {"columns": 28, "rows": 32, "bones": 6, "front_deg": 55, "attach_dz": 0.02, "clearance": 0.025,
       "stiffness": 0.05, "damping": 0.9, "iters": 6, "link_k": 0.8, "colour": [0.86, 0.85, 0.82],
       "robe_r": 0.075, "leg_r": 0.04, "torso_r": 0.085, "arm_r": 0.03,
       "looping": ["idle", "walk", "run", "pistol_idle", "pistol_walk", "pistol_run", "pistol_kneel",
                   "pistol_shoot", "rifle_idle", "rifle_aim", "rifle_walk", "rifle_run", "rifle_crouch",
                   "rifle_crouch_aim", "rifle_shoot"]}
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
ad.action = None
for pb in arm.pose.bones:
    pb.matrix_basis.identity()
bpy.context.view_layer.update()


def rest_head(name):
    return arm.matrix_world @ arm.data.bones["mixamorig:" + name].head_local


# ---- frame of reference -----------------------------------------------------
l_sh, r_sh = rest_head("LeftArm"), rest_head("RightArm")
lateral = (l_sh - r_sh).normalized()
up = Vector((0, 0, 1))
forward = up.cross(lateral).normalized()
toes = (rest_head("LeftToeBase") + rest_head("RightToeBase")) / 2 - (rest_head("LeftFoot") + rest_head("RightFoot")) / 2
if forward.dot(toes) < 0:
    forward = -forward
hips = rest_head("Hips")
axis = Vector((hips.x, hips.y, 0))
H = rest_head("Head").z + 0.25
floor_z = min(rest_head("LeftToeBase").z, rest_head("RightToeBase").z) - 0.03
attach_z = (l_sh.z + r_sh.z) / 2 + opt["attach_dz"] * H
print("height %.2f attach_z %.2f floor %.2f" % (H, attach_z, floor_z))

# shape the cape around a relaxed pose (first frame of idle, arms down), not
# the rig's rest pose with its arms out
shape_clip = opt.get("shape_clip", "idle")
sact = tracks[shape_clip].strips[0].action
ad.action = sact
if hasattr(ad, "action_slot") and len(getattr(sact, "slots", [])):
    ad.action_slot = sact.slots[0]
scene.frame_set(int(sact.frame_range[0]))
dg = bpy.context.evaluated_depsgraph_get()
ev_arm = arm.evaluated_get(dg)
ev_body = body.evaluated_get(dg)
emesh = ev_body.to_mesh()
skip = {"mixamorig:%s%s" % (s, b) for s in ("Left", "Right")
        for b in ("ForeArm", "Hand", "HandThumb1", "HandThumb2", "HandThumb3", "HandIndex1", "HandIndex2",
                  "HandIndex3", "HandMiddle1", "HandMiddle2", "HandMiddle3", "HandRing1", "HandRing2",
                  "HandRing3", "HandPinky1", "HandPinky2", "HandPinky3")}
gname = {g.index: g.name for g in body.vertex_groups}
dominant = []
for v in body.data.vertices:
    best = max(v.groups, key=lambda g: g.weight, default=None)
    dominant.append(gname.get(best.group) if best else None)
wverts = [tuple(body.matrix_world @ v.co) for v in emesh.vertices]
tris = []
emesh.calc_loop_triangles()
for t in emesh.loop_triangles:
    if any(dominant[i] in skip for i in t.vertices):
        continue
    tris.append(tuple(t.vertices))
bvh = BVHTree.FromPolygons(wverts, tris)
n_all = len(emesh.loop_triangles)
body.evaluated_get(dg).to_mesh_clear()
shape_par = {}
print("shape pose: %s frame %d, %d of %d triangles (forearms and hands left out)"
      % (shape_clip, scene.frame_current, len(tris), n_all))


def posed_head(name):
    return ev_arm.matrix_world @ ev_arm.pose.bones["mixamorig:" + name].head


hips_p = posed_head("Hips")
axis = Vector((hips_p.x, hips_p.y, 0))
sh_p = (posed_head("LeftArm") + posed_head("RightArm")) / 2
attach_z = sh_p.z + opt["attach_dz"] * H
floor_z = min(posed_head("LeftToeBase").z, posed_head("RightToeBase").z) - 0.03
lpose = (posed_head("LeftArm") - posed_head("RightArm"))
lpose.z = 0
lateral = lpose.normalized()
forward = up.cross(lateral).normalized()
ptoes = (posed_head("LeftToeBase") + posed_head("RightToeBase")) / 2 - (posed_head("LeftFoot") + posed_head("RightFoot")) / 2
ptoes.z = 0
if forward.dot(ptoes) < 0:
    forward = -forward
print("shape pose facing", tuple(round(v, 2) for v in forward))


def outer_radius(theta, z):
    """Distance from the vertical axis to the outermost body surface in the
    direction theta at height z (0 if nothing), in the shape pose."""
    d = forward * math.cos(theta) + lateral * math.sin(theta)
    origin = Vector((axis.x, axis.y, z)) + d * 2.0
    hit = bvh.ray_cast(origin, -d, 2.5)
    if hit[0] is None:
        return 0.0
    p = hit[0]
    return Vector((p.x - axis.x, p.y - axis.y, 0)).length


robe_samples = [outer_radius(2 * math.pi * k / 24, floor_z + 0.25 * H) for k in range(24)]
robe_r_m = float(np.median([r for r in robe_samples if r > 0] or [0.2]))
print("robe radius at knee height %.2f m" % robe_r_m)

# ---- 1. the cape mesh -------------------------------------------------------
C, R = opt["columns"], opt["rows"]
t0 = math.radians(opt["front_deg"])
thetas = [t0 + (2 * math.pi - 2 * t0) * i / (C - 1) for i in range(C)]  # left front -> back -> right front
clear = opt["clearance"]
# attach under the shoulder pads: per angle, scan down from above the
# shoulders for the pads' lower rim (the outer radius drops sharply), and
# attach just under it on the body surface beneath
step = 0.01
z_hi, z_lo = attach_z + 0.08 * H, attach_z - 0.30 * H
att_z, att_r = [], []
for th in thetas:
    zz = np.arange(z_hi, z_lo, -step)
    rr = np.array([outer_radius(th, z) for z in zz])
    k_max = int(np.argmax(rr))
    rim = None
    for k in range(k_max, len(zz) - 2):
        if rr[k] > 0 and rr[k + 2] < rr[k_max] * (1 - opt.get("rim_drop", 0.18)):
            rim = k + 2
            break
    if rim is None:  # no pad here (e.g. between the pads at the back): stay at the line
        rim = int(np.argmin(np.abs(zz - attach_z)))
    att_z.append(zz[rim] - opt.get("under", 0.01))
    att_r.append(outer_radius(th, zz[rim] - opt.get("under", 0.01)))
# smooth the attach line along the arc so it follows the pads' edge evenly
att_z = np.array(att_z)
att_r = np.array(att_r)
for _ in range(opt.get("line_smooth", 3)):
    att_z[1:-1] = (att_z[:-2] + 2 * att_z[1:-1] + att_z[2:]) / 4
    att_r[1:-1] = (att_r[:-2] + 2 * att_r[1:-1] + att_r[2:]) / 4
print("attach line: z %.2f..%.2f m, radius %.2f..%.2f m" % (att_z.min(), att_z.max(), att_r.min(), att_r.max()))
grid = np.zeros((C, R, 3))
for i, th in enumerate(thetas):
    d = forward * math.cos(th) + lateral * math.sin(th)
    r_top = att_r[i] + opt.get("top_clear", 0.012)
    for j in range(R):
        z = att_z[i] + (floor_z - att_z[i]) * j / (R - 1)
        r = max(r_top, outer_radius(th, z) + clear) if j > 0 else r_top
        p = Vector((axis.x, axis.y, z)) + d * r
        grid[i, j] = tuple(p)
# move each column from the shape pose to the rest pose through its parent bone
col_parent = []
for i, th in enumerate(thetas):
    rel = (th + math.pi) % (2 * math.pi) - math.pi
    if abs(rel) < math.radians(125):
        col_parent.append("mixamorig:%sShoulder" % ("Left" if rel > 0 else "Right"))
    else:
        col_parent.append("mixamorig:Spine2")
for i in range(C):
    m_pose = ev_arm.matrix_world @ ev_arm.pose.bones[col_parent[i]].matrix
    m_rest = arm.matrix_world @ arm.data.bones[col_parent[i]].matrix_local
    xf = m_rest @ m_pose.inverted()
    for j in range(R):
        grid[i, j] = tuple(xf @ Vector(grid[i, j]))
ad.action = None
for pb in arm.pose.bones:
    pb.matrix_basis.identity()
scene.frame_set(1)
bpy.context.view_layer.update()
me = bpy.data.meshes.new("cape")
verts = [tuple(grid[i, j]) for i in range(C) for j in range(R)]
faces = [(i * R + j, (i + 1) * R + j, (i + 1) * R + j + 1, i * R + j + 1) for i in range(C - 1) for j in range(R - 1)]
me.from_pydata(verts, [], faces)
me.update()
uv = me.uv_layers.new(name="UVMap")
for poly in me.polygons:
    for li in poly.loop_indices:
        vi = me.loops[li].vertex_index
        i, j = divmod(vi, R)
        uv.data[li].uv = (i / (C - 1), 1 - j / (R - 1))
cape = bpy.data.objects.new("cape", me)
scene.collection.objects.link(cape)
mat = bpy.data.materials.new("cape_cloth")
mat.use_nodes = True
bsdf = mat.node_tree.nodes.get("Principled BSDF")
bsdf.inputs["Base Color"].default_value = tuple(opt["colour"]) + (1.0,)
bsdf.inputs["Roughness"].default_value = 0.85
mat.use_backface_culling = False
me.materials.append(mat)
for p in me.polygons:
    p.use_smooth = True
print("cape: %d x %d vertices, arc %.0f deg" % (C, R, math.degrees(2 * math.pi - 2 * t0)))

# ---- 2. chains --------------------------------------------------------------
NB = opt["bones"]
rows_at = [round(k * (R - 1) / NB) for k in range(NB + 1)]
chains = []
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
inv_arm = arm.matrix_world.inverted()
for i, th in enumerate(thetas):
    parent = col_parent[i]
    pts = [Vector(grid[i, j]) for j in rows_at]
    names, prev = [], None
    for k in range(NB):
        eb = arm.data.edit_bones.new("cape_%02d_%d" % (i, k))
        eb.head = inv_arm @ pts[k]
        eb.tail = inv_arm @ pts[k + 1]
        eb.parent = prev if prev else arm.data.edit_bones[parent]
        eb.use_connect = prev is not None
        prev = eb
        names.append(eb.name)
    chains.append({"parent": parent, "points": pts, "bones": names})
bpy.ops.object.mode_set(mode="OBJECT")

cape.parent = arm
mod = cape.modifiers.new("Armature", "ARMATURE")
mod.object = arm
groups = {}
for c in chains:
    for b in c["bones"] + [c["parent"]]:
        if b not in groups:
            groups[b] = cape.vertex_groups.new(name=b)
for i, c in enumerate(chains):
    for j in range(R):
        vi = i * R + j
        if j == 0:
            groups[c["parent"]].add([vi], 1.0, "REPLACE")
            continue
        x = j / (R - 1) * NB
        k0 = min(NB - 1, int(math.floor(x - 1e-9)))
        f = x - k0
        # weight to the bone whose segment holds the vertex, blended toward the next
        if f < 0.5 or k0 == NB - 1:
            groups[c["bones"][k0]].add([vi], 1.0, "ADD")
        else:
            groups[c["bones"][k0]].add([vi], 1.5 - f, "ADD")
            groups[c["bones"][k0 + 1]].add([vi], f - 0.5, "ADD")
print("chains: %d x %d bones" % (len(chains), NB))

# ---- 3. simulation and bake -------------------------------------------------
caps_def = [("LeftUpLeg", "LeftLeg", opt["robe_r"]), ("RightUpLeg", "RightLeg", opt["robe_r"]),
            ("LeftLeg", "LeftFoot", opt["leg_r"]), ("RightLeg", "RightFoot", opt["leg_r"]),
            ("Hips", "Spine2", opt["torso_r"]), ("Spine2", "Neck", opt.get("upper_r", 0.1)),
            ("LeftArm", "LeftForeArm", opt["arm_r"]), ("RightArm", "RightForeArm", opt["arm_r"]),
            ("LeftForeArm", "LeftHand", opt["arm_r"]), ("RightForeArm", "RightHand", opt["arm_r"])]
caps_def = [(a, b, r * H) for a, b, r in caps_def]
caps_def.append(("Hips", "FEET", robe_r_m + opt.get("robe_margin", 0.05)))
parents = sorted({c["parent"] for c in chains})
rest_par = {p: arm.matrix_world @ arm.data.bones[p].matrix_local for p in parents}
P = NB + 1
nc = len(chains)
local = np.zeros((nc, P, 4))
par_idx = np.zeros(nc, dtype=int)
seg = np.zeros((nc, NB))
for ci, c in enumerate(chains):
    inv = rest_par[c["parent"]].inverted()
    for k, p in enumerate(c["points"]):
        q = inv @ p
        local[ci, k] = (q.x, q.y, q.z, 1.0)
    par_idx[ci] = parents.index(c["parent"])
    seg[ci] = [(c["points"][k + 1] - c["points"][k]).length for k in range(NB)]
link_rest = np.linalg.norm(
    np.array([[tuple(chains[i]["points"][k]) for k in range(P)] for i in range(nc)])[1:] -
    np.array([[tuple(chains[i]["points"][k]) for k in range(P)] for i in range(nc)])[:-1], axis=2)  # (nc-1, P)

empties = []
for ci, c in enumerate(chains):
    c["empties"] = []
    for k in range(1, P):
        e = bpy.data.objects.new("cape_t_%02d_%d" % (ci, k), None)
        scene.collection.objects.link(e)
        c["empties"].append(e)
        empties.append(e)
        con = arm.pose.bones[c["bones"][k - 1]].constraints.new("DAMPED_TRACK")
        con.target = e
        con.track_axis = "TRACK_Y"


def action_fcurves(action):
    if hasattr(action, "fcurves"):
        try:
            return list(action.fcurves)
        except Exception:
            pass
    curves = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []):
                curves.extend(bag.fcurves)
    return curves


def evaluated():
    return arm.evaluated_get(bpy.context.evaluated_depsgraph_get())


def collide(X, caps):
    """Pushes all points (nc, P, 3) out of the capsules (except row 0)."""
    Y = X[:, 1:, :].reshape(-1, 3)
    for a, b, r in caps:
        ab = b - a
        t = np.clip(((Y - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
        q = a + t[:, None] * ab
        d = Y - q
        n = np.linalg.norm(d, axis=1)
        inside = n < r
        if inside.any():
            nn = np.where(n[inside] > 1e-6, n[inside], 1.0)
            Y[inside] = q[inside] + d[inside] / nn[:, None] * r
    X[:, 1:, :] = Y.reshape(nc, P - 1, 3)


g = np.array([0.0, 0.0, -9.81])
dt = scene.render.fps_base / scene.render.fps
for name, track in tracks.items():
    act = track.strips[0].action
    ad.action = act
    if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    frames = list(range(f0, f1 + 1))
    targets, capsules = [], []
    for f in frames:
        scene.frame_set(f)
        ev = evaluated()
        mats = np.array([np.array(ev.matrix_world @ ev.pose.bones[p].matrix) for p in parents])  # (np,4,4)
        T = np.einsum("cij,ckj->cki", mats[par_idx], local)[:, :, :3]
        targets.append(T)
        def hp(n):
            if n == "FEET":
                a = ev.matrix_world @ ev.pose.bones["mixamorig:LeftFoot"].head
                b = ev.matrix_world @ ev.pose.bones["mixamorig:RightFoot"].head
                return np.array(tuple((a + b) / 2))
            return np.array(tuple(ev.matrix_world @ ev.pose.bones["mixamorig:" + n].head))
        capsules.append([(hp(a), hp(b), r) for a, b, r in caps_def])
    loop = name in opt["looping"]
    seq = (list(range(len(frames))) * 2) if loop else ([0] * 20 + list(range(len(frames))))
    keep_from = len(frames) if loop else 20
    X = targets[0].copy()
    Xp = X.copy()
    result = []
    for step, fi in enumerate(seq):
        T = targets[fi]
        Xn = X + (X - Xp) * opt["damping"] + g * dt * dt + opt["stiffness"] * (T - X)
        Xn[:, 0] = T[:, 0]
        for _ in range(opt["iters"]):
            for k in range(NB):
                d = Xn[:, k + 1] - Xn[:, k]
                n = np.linalg.norm(d, axis=1)
                n = np.where(n > 1e-9, n, 1.0)
                Xn[:, k + 1] = Xn[:, k] + d / n[:, None] * seg[:, k][:, None]
            d = Xn[1:, 1:] - Xn[:-1, 1:]
            n = np.linalg.norm(d, axis=2)
            n = np.where(n > 1e-9, n, 1.0)
            corr = ((n - link_rest[:, 1:]) / n * 0.5 * opt["link_k"])[:, :, None] * d
            Xn[:-1, 1:] += corr
            Xn[1:, 1:] -= corr
            collide(Xn, capsules[fi])
        Xp, X = X, Xn
        if step >= keep_from:
            result.append(X.copy())
    Rz = np.array(result)  # frames x nc x P x 3
    for ci, c in enumerate(chains):
        for k, e in enumerate(c["empties"], start=1):
            e.animation_data_clear()
            e.location = tuple(Rz[0, ci, k])
            e.keyframe_insert("location", frame=frames[0])
            for fc in action_fcurves(e.animation_data.action):
                if fc.data_path != "location":
                    continue
                kp = np.empty(len(frames) * 2, dtype=np.float32)
                kp[0::2] = frames
                kp[1::2] = Rz[:, ci, k, fc.array_index]
                fc.keyframe_points.clear()
                fc.keyframe_points.add(len(frames))
                fc.keyframe_points.foreach_set("co", kp)
                fc.update()
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm.pose.bones:
        sel = pb.name.startswith("cape_")
        if hasattr(pb, "select"):
            pb.select = sel
        else:
            pb.bone.select = sel
    bpy.ops.nla.bake(frame_start=frames[0], frame_end=frames[-1], only_selected=True, visual_keying=True,
                     clear_constraints=False, use_current_action=True, bake_types={"POSE"})
    bpy.ops.object.mode_set(mode="OBJECT")
    print("clip %-16s cape baked (%d frames)" % (name, len(frames)))

for pb in arm.pose.bones:
    for con in list(pb.constraints):
        if con.type == "DAMPED_TRACK":
            pb.constraints.remove(con)
for e in empties:
    bpy.data.objects.remove(e, do_unlink=True)
for a in list(bpy.data.actions):
    if a.users == 0 and not a.use_fake_user:
        bpy.data.actions.remove(a)
ad.action = None
for t in ad.nla_tracks:
    t.mute = False
scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
