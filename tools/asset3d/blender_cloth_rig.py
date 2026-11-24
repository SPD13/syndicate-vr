"""Blender (background) script: drapes as their own mesh on simulated bone chains.

blender -b --factory-startup -P blender_cloth_rig.py -- in.glb out.glb [opts.json]

1. Separate: faces whose vertices are drape cloth (light, unsaturated
   texture colour, below the shoulders; score smoothed over the mesh) become
   a separate mesh "drapes".
2. Chains: drape vertices are grouped by their angle around the body; each
   group with enough vertices gets a chain of `bones` bones from the top of
   the cloth to its hem, parented to the side's Shoulder bone (or Spine2 at
   the back). The drape mesh is weighted only to these chains (angular blend
   between the two nearest chains, linear blend along the height).
3. Motion: for every clip (NLA track), a verlet spring simulation of the
   chain points (gravity, damping, pull toward the rest shape relative to
   the parent bone, fixed segment lengths, collision with capsules around
   the legs and the hips/torso) runs over the clip (twice for looping clips,
   keeping the second pass). The points drive the chain bones through Damped
   Track constraints, baked into the clip's action as keyframes.

Options (json): bones 4, min_verts 150, sectors 12, stiffness 0.08,
damping 0.92, iters 4, white_min 0.55, sat_max 0.18, smooth 12,
leg_r 0.055, shin_r 0.045, torso_r 0.10 (fractions of height), margin 0.012,
stage ("all" | "split": stop after steps 1-2, no simulation).
"""
import json
import math
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
opt = {"bones": 4, "min_verts": 150, "sectors": 12, "stiffness": 0.08, "damping": 0.92,
       "iters": 4, "white_min": 0.55, "sat_max": 0.18, "smooth": 12,
       "leg_r": 0.055, "shin_r": 0.045, "torso_r": 0.10, "margin": 0.012, "stage": "all",
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


def vertex_colours(ob):
    me = ob.data
    img = None
    for mat in me.materials:
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED" and n.inputs["Base Color"].is_linked:
                    node = n.inputs["Base Color"].links[0].from_node
                    if node.type == "TEX_IMAGE":
                        img = node.image
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[:, :, :3]
    uv = np.zeros(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vidx = np.zeros(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vidx)
    x = np.clip((uv[:, 0] % 1.0) * (w - 1), 0, w - 1).astype(int)
    y = np.clip((uv[:, 1] % 1.0) * (h - 1), 0, h - 1).astype(int)
    acc = np.zeros((len(me.vertices), 3))
    cnt = np.zeros(len(me.vertices))
    np.add.at(acc, vidx, px[y, x])
    np.add.at(cnt, vidx, 1)
    return acc / np.maximum(cnt, 1)[:, None]


# ---- 1. separate the drapes -------------------------------------------------
height = rest_head("Head").z + 0.25
l_sh, r_sh = rest_head("LeftArm"), rest_head("RightArm")
center = (l_sh + r_sh) / 2
shoulder_z = center.z
lateral = (l_sh - r_sh).normalized()
up = Vector((0, 0, 1))
forward = up.cross(lateral).normalized()  # check the sign against the toes
toes = (rest_head("LeftToeBase") + rest_head("RightToeBase")) / 2 - (rest_head("LeftFoot") + rest_head("RightFoot")) / 2
if forward.dot(toes) < 0:
    forward = -forward
axis_xy = rest_head("Hips")

me = body.data
nv = len(me.vertices)
cols = vertex_colours(body)
bright = cols.min(axis=1)
sat = cols.max(axis=1) - cols.min(axis=1)
pos = np.array([tuple(body.matrix_world @ v.co) for v in me.vertices])
# outermost layer: a ray cast horizontally outward from the body axis hits nothing
from mathutils.bvhtree import BVHTree  # noqa: E402

dg = bpy.context.evaluated_depsgraph_get()
bvh = BVHTree.FromObject(body, dg)
inv_body = body.matrix_world.inverted()
outer = np.zeros(nv, dtype=bool)
for i, p in enumerate(pos):
    d = Vector((p[0] - axis_xy.x, p[1] - axis_xy.y, 0))
    if d.length < 1e-4:
        continue
    d.normalize()
    o = inv_body @ (Vector(p) + d * 0.004)
    dl = (inv_body.to_3x3() @ d).normalized()
    hit = bvh.ray_cast(o, dl, 2.0)
    outer[i] = hit[0] is None
relp = pos - np.array(tuple(axis_xy))
ang_all = np.arctan2(relp @ np.array(tuple(lateral)), relp @ np.array(tuple(forward)))
not_front = np.abs(ang_all) > math.radians(opt.get("front_deg", 30))
light = bright > opt.get("light_min", 0.30)
white = (bright > opt["white_min"]) & (sat < opt["sat_max"])
below = (pos[:, 2] < shoulder_z + 0.03 * height) & (pos[:, 2] > 0.02 * height)
score = (below & ((outer & not_front & light) | white)).astype(float)
print("drape candidates: %d outer, %d white, %d total" % ((outer & not_front & light & below).sum(), (white & below).sum(), score.sum()))
ev = np.zeros(len(me.edges) * 2, dtype=np.int64)
me.edges.foreach_get("vertices", ev)
ev = ev.reshape(-1, 2)
deg = np.zeros(nv)
np.add.at(deg, ev[:, 0], 1)
np.add.at(deg, ev[:, 1], 1)
for _ in range(opt["smooth"]):
    acc = np.zeros(nv)
    np.add.at(acc, ev[:, 0], score[ev[:, 1]])
    np.add.at(acc, ev[:, 1], score[ev[:, 0]])
    score = 0.5 * score + 0.5 * acc / np.maximum(deg, 1)

import bmesh  # noqa: E402

bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode="EDIT")
bm = bmesh.from_edit_mesh(me)
bm.verts.ensure_lookup_table()
for f in bm.faces:
    f.select_set(False)
for e in bm.edges:
    e.select_set(False)
for v in bm.verts:
    v.select_set(False)
n_faces = 0
for f in bm.faces:
    if sum(score[v.index] for v in f.verts) / len(f.verts) > opt.get("face_thresh", 0.45):
        f.select_set(True)
        n_faces += 1
bm.select_flush_mode()
bmesh.update_edit_mesh(me)
before = set(bpy.data.objects)
bpy.ops.mesh.separate(type="SELECTED")
bpy.ops.object.mode_set(mode="OBJECT")
new_objs = [o for o in bpy.data.objects if o not in before]
print("objects after separate:", [(o.name, len(o.data.vertices)) for o in bpy.data.objects if o.type == "MESH"])
drapes = new_objs[0]
drapes.name = "drapes"
print("separated %d faces into %s (%d vertices); body keeps %d"
      % (n_faces, drapes.name, len(drapes.data.vertices), len(body.data.vertices)))

# ---- 2. chains --------------------------------------------------------------
dme = drapes.data
dpos = np.array([tuple(drapes.matrix_world @ v.co) for v in dme.vertices])
rel = dpos - np.array(tuple(axis_xy))
ang = np.arctan2(rel @ np.array(tuple(lateral)), rel @ np.array(tuple(forward)))  # 0 = front, +pi/2 = left
nsec = opt["sectors"]
sec = ((ang + math.pi) / (2 * math.pi) * nsec).astype(int) % nsec
chains = []
for s in range(nsec):
    idx = np.where(sec == s)[0]
    if len(idx) < opt["min_verts"]:
        continue
    z = dpos[idx, 2]
    top_i = idx[z >= np.percentile(z, 92)]
    bot_i = idx[z <= np.percentile(z, 8)]
    top = Vector(dpos[top_i].mean(0))
    bot = Vector(dpos[bot_i].mean(0))
    a = math.atan2((top - axis_xy).dot(lateral), (top - axis_xy).dot(forward))
    side = "Left" if a > 0 else "Right"
    parent = "mixamorig:%sShoulder" % side if abs(a) < 2.4 else "mixamorig:Spine2"
    chains.append({"sector": s, "angle": a, "top": top, "bot": bot, "parent": parent})
print("chains:", ", ".join("%d(%.0f deg, %s)" % (c["sector"], math.degrees(c["angle"]), c["parent"][10:])
                           for c in chains))

nb = opt["bones"]
bpy.ops.object.select_all(action="DESELECT")
arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
inv = arm.matrix_world.inverted()
for ci, c in enumerate(chains):
    prev = None
    c["bones"] = []
    c["points"] = [c["top"].lerp(c["bot"], k / nb) for k in range(nb + 1)]
    for k in range(nb):
        eb = arm.data.edit_bones.new("cloth_%d_%d" % (ci, k))
        eb.head = inv @ c["points"][k]
        eb.tail = inv @ c["points"][k + 1]
        eb.parent = prev if prev else arm.data.edit_bones[c["parent"]]
        eb.use_connect = prev is not None
        eb.use_deform = True
        prev = eb
        c["bones"].append(eb.name)
bpy.ops.object.mode_set(mode="OBJECT")

# weights: drapes only on the chains
for g in list(drapes.vertex_groups):
    drapes.vertex_groups.remove(g)
groups = {}
for c in chains:
    for b in c["bones"]:
        groups[b] = drapes.vertex_groups.new(name=b)
chain_ang = np.array([c["angle"] for c in chains])
sigma = 2 * math.pi / nsec
for c in chains:
    if c["parent"] not in groups:
        groups[c["parent"]] = drapes.vertex_groups.new(name=c["parent"])
for i, p in enumerate(dpos):
    a = ang[i]
    d = np.abs((chain_ang - a + math.pi) % (2 * math.pi) - math.pi)
    order = np.argsort(d)[:3]
    wts = np.exp(-(d[order] / sigma) ** 2)
    wts /= wts.sum()
    for ci, wc in zip(order, wts):
        if wc < 1e-3:
            continue
        c = chains[ci]
        span = c["top"].z - c["bot"].z
        t = 0.0 if span < 1e-4 else min(1.0, max(0.0, (c["top"].z - p[2]) / span))
        pin = max(0.0, 1.0 - t / opt.get("pin", 0.08))  # top rows follow the parent bone
        if pin > 0:
            groups[c["parent"]].add([i], wc * pin, "ADD")
        x = t * nb - 0.5
        k0 = int(max(0, min(nb - 1, math.floor(x))))
        k1 = min(nb - 1, k0 + 1)
        f = min(1.0, max(0.0, x - k0))
        rest = wc * (1 - pin)
        groups[c["bones"][k0]].add([i], rest * (1 - f), "ADD")
        if k1 != k0:
            groups[c["bones"][k1]].add([i], rest * f, "ADD")
print("drape weights assigned to %d chain bones" % len(groups))


def export():
    ad.action = None
    for t in ad.nla_tracks:
        t.mute = False
    scene.frame_set(1)
    bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                              export_force_sampling=True, export_frame_range=False)
    print("exported", out)


if opt["stage"] == "split":
    export()
    sys.exit(0)

# ---- 3. simulation and bake -------------------------------------------------
H = height
caps = [("LeftUpLeg", "LeftLeg", opt["leg_r"] * H), ("RightUpLeg", "RightLeg", opt["leg_r"] * H),
        ("LeftLeg", "LeftFoot", opt["shin_r"] * H), ("RightLeg", "RightFoot", opt["shin_r"] * H),
        ("Hips", "Spine2", opt["torso_r"] * H)]
rest_par = {c["parent"]: (arm.matrix_world @ arm.data.bones[c["parent"]].matrix_local) for c in chains}

# empties driving the chain bones
empties = []
for ci, c in enumerate(chains):
    c["empties"] = []
    for k in range(1, nb + 1):
        e = bpy.data.objects.new("cloth_t_%d_%d" % (ci, k), None)
        scene.collection.objects.link(e)
        c["empties"].append(e)
        empties.append(e)
        pb = arm.pose.bones[c["bones"][k - 1]]
        con = pb.constraints.new("DAMPED_TRACK")
        con.target = e
        con.track_axis = "TRACK_Y"


def action_fcurves(action):
    """F-curves of an action, for both the legacy and the layered (4.4+) API."""
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


def posed_world(name):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return ev.matrix_world @ ev.pose.bones[name].matrix


def posed_head(name):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return np.array(tuple(ev.matrix_world @ ev.pose.bones["mixamorig:" + name].head))


def push_out(p, a, b, r):
    ab = b - a
    t = np.clip(np.dot(p - a, ab) / max(np.dot(ab, ab), 1e-9), 0, 1)
    q = a + ab * t
    d = p - q
    n = np.linalg.norm(d)
    if n < r:
        return q + (d / n if n > 1e-6 else np.array([0.0, 0.0, 1.0])) * r
    return p


g = np.array([0, 0, -9.81])
dt = scene.render.fps_base / scene.render.fps
for name, track in tracks.items():
    act = track.strips[0].action
    ad.action = act
    if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    frames = list(range(f0, f1 + 1))
    passes = 2 if name in opt["looping"] else 1
    # per frame: parent matrices and capsules (evaluated once)
    par_m, capsules = [], []
    for f in frames:
        scene.frame_set(f)
        par_m.append({c["parent"]: posed_world(c["parent"]) for c in chains})
        capsules.append([(posed_head(a), posed_head(b), r) for a, b, r in caps])
    state = []
    for c in chains:
        local = [rest_par[c["parent"]].inverted() @ p for p in c["points"]]
        c["local"] = local
        m = par_m[0][c["parent"]]
        x = np.array([tuple(m @ p) for p in local])
        seg = np.array([(c["points"][k + 1] - c["points"][k]).length for k in range(nb)])
        state.append({"x": x, "xp": x.copy(), "seg": seg})
    # neighbouring chains (by angle) are linked point to point
    if "links" not in opt:
        order_c = sorted(range(len(chains)), key=lambda i: chains[i]["angle"])
        links = []
        for a_i, b_i in zip(order_c, order_c[1:] + order_c[:1]):
            gap = abs((chains[a_i]["angle"] - chains[b_i]["angle"] + math.pi) % (2 * math.pi) - math.pi)
            if a_i != b_i and gap < math.radians(70):
                rest_d = [(chains[a_i]["points"][k] - chains[b_i]["points"][k]).length for k in range(nb + 1)]
                links.append((a_i, b_i, rest_d))
        opt["links"] = links
        print("lateral links:", len(links))
    out_pos = {}
    settle = 0 if passes == 2 else 15
    seq = [0] * settle + list(range(len(frames))) * passes
    for step, fi in enumerate(seq):
        last_pass = step >= settle + len(frames) * (passes - 1)
        for c, st in zip(chains, state):
            m = par_m[fi][c["parent"]]
            target = np.array([tuple(m @ p) for p in c["local"]])
            x, xp = st["x"], st["xp"]
            vel = (x - xp) * opt["damping"]
            xn = x + vel + g * dt * dt + opt["stiffness"] * (target - x)
            xn[0] = target[0]
            for _ in range(opt["iters"]):
                for k in range(nb):
                    d = xn[k + 1] - xn[k]
                    n = np.linalg.norm(d)
                    if n > 1e-9:
                        xn[k + 1] = xn[k] + d / n * st["seg"][k]
                for k in range(1, nb + 1):
                    for a, b, r in capsules[fi]:
                        xn[k] = push_out(xn[k], a, b, r + opt["margin"] * H)
            st["xp"], st["x"] = x, xn
        for _ in range(2):
            for a_i, b_i, rest_d in opt["links"]:
                xa, xb = state[a_i]["x"], state[b_i]["x"]
                for k in range(1, nb + 1):
                    d = xb[k] - xa[k]
                    n = np.linalg.norm(d)
                    if n > 1e-9:
                        corr = (n - rest_d[k]) / n * 0.5 * opt.get("link_k", 0.6)
                        xa[k] += d * corr
                        xb[k] -= d * corr
        if last_pass:
            for c, st in zip(chains, state):
                out_pos.setdefault(id(c), []).append(st["x"].copy())
    # write the empties' animation
    for c in chains:
        P = np.array(out_pos[id(c)])  # frames x (nb+1) x 3
        for k, e in enumerate(c["empties"], start=1):
            e.animation_data_clear()
            e.location = tuple(P[0, k])
            e.keyframe_insert("location", frame=frames[0])
            curves = action_fcurves(e.animation_data.action)
            for fc in curves:
                if not fc.data_path == "location":
                    continue
                kp = np.empty(len(frames) * 2, dtype=np.float32)
                kp[0::2] = frames
                kp[1::2] = P[:, k, fc.array_index]
                fc.keyframe_points.clear()
                fc.keyframe_points.add(len(frames))
                fc.keyframe_points.foreach_set("co", kp)
                fc.update()
    # bake the chain bones into the clip
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    for pb in arm.pose.bones:
        sel = pb.name.startswith("cloth_")
        if hasattr(pb, "select"):
            pb.select = sel
        else:
            pb.bone.select = sel
    bpy.ops.nla.bake(frame_start=frames[0], frame_end=frames[-1], only_selected=True, visual_keying=True,
                     clear_constraints=False, use_current_action=True, bake_types={"POSE"})
    bpy.ops.object.mode_set(mode="OBJECT")
    print("clip %-16s cloth baked (%d frames, %d pass%s)" % (name, len(frames), passes, "es" if passes > 1 else ""))

# remove helpers
for pb in arm.pose.bones:
    for con in list(pb.constraints):
        if con.type == "DAMPED_TRACK":
            pb.constraints.remove(con)
for e in empties:
    bpy.data.objects.remove(e, do_unlink=True)
for a in list(bpy.data.actions):
    if a.users == 0 and not a.use_fake_user:
        bpy.data.actions.remove(a)
export()
