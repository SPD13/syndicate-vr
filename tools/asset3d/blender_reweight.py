"""Blender (background) script: rigid shoulder pieces and hanging drapes.

EXPERIMENTAL, not used for the published models: tried on the Church zealot
(2026-11-23). Tripo builds the figure as one closed surface where drapes,
robe and legs touch, so once the drapes stop following the legs, the legs
and the inner robe poke through them (torn, dark shards at the hem). Domes
alone made no visible gain. Cloth like this needs its own mesh (split out)
with spring bones or a baked cloth simulation.

blender -b --factory-startup -P blender_reweight.py -- in.glb out.glb [options.json]

The auto-rig binds everything near a limb to that limb: shoulder armour
that sits on the arm follows the arm, cloth beside the legs follows the
legs. This script re-weights, in the rest pose:
- drape vertices (white/light, low saturation texture colour, below the
  shoulders): a chain shoulder(side) -> Spine2 -> Spine1 -> Hips along the
  height, so the cloth hangs from the shoulders and follows the torso;
- shoulder piece vertices (not drape, weighted to an upper arm, at or above
  the shoulder joint or far from the arm's axis): 100% to the side's
  Shoulder bone, so they stay rigid and move with the torso.
Other vertices keep their weights. Options (json): white_min (0.55),
sat_max (0.18), dome_above (-0.06, fraction of height relative to the
shoulder joint), dome_far (0.10, fraction of height from the arm axis),
drape_strength (1.0: fully on the torso; lower keeps some leg follow),
smooth (8 passes): the scores are smoothed over the mesh edges and the new
weights blended with the old ones by score, so neighbours never tear apart.
"""
import json
import sys

import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
opt = {"white_min": 0.55, "sat_max": 0.18, "dome_above": -0.06, "dome_far": 0.10}
if len(args) > 2:
    opt.update(json.load(open(args[2])))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
meshes = [o for o in bpy.data.objects if o.type == "MESH" and o.parent is arm and not o.name.startswith("weapon_")]
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
ad = arm.animation_data
if ad:
    ad.action = None
    for t in ad.nla_tracks:
        t.mute = True


def head(name):
    return arm.matrix_world @ arm.data.bones["mixamorig:" + name].head_local


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


def vertex_colours(ob):
    """Average base-colour texture value per vertex (linear 0..1)."""
    me = ob.data
    img = None
    for mat in me.materials:
        if mat and mat.use_nodes:
            for n in mat.node_tree.nodes:
                if n.type == "BSDF_PRINCIPLED" and n.inputs["Base Color"].is_linked:
                    node = n.inputs["Base Color"].links[0].from_node
                    if node.type == "TEX_IMAGE":
                        img = node.image
    cols = np.full((len(me.vertices), 3), 0.5)
    if img is None or not me.uv_layers:
        print("  no base colour texture on", ob.name)
        return cols
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, img.channels)[:, :, :3]
    uv = np.zeros(len(me.loops) * 2, dtype=np.float32)
    me.uv_layers.active.data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    vidx = np.zeros(len(me.loops), dtype=np.int64)
    me.loops.foreach_get("vertex_index", vidx)
    x = np.clip((uv[:, 0] % 1.0) * (w - 1), 0, w - 1).astype(int)
    y = np.clip((uv[:, 1] % 1.0) * (h - 1), 0, h - 1).astype(int)
    lc = px[y, x]
    acc = np.zeros((len(me.vertices), 3))
    cnt = np.zeros(len(me.vertices))
    np.add.at(acc, vidx, lc)
    np.add.at(cnt, vidx, 1)
    cols = acc / np.maximum(cnt, 1)[:, None]
    return cols


l_sh, r_sh = head("LeftArm"), head("RightArm")
l_el, r_el = head("LeftForeArm"), head("RightForeArm")
hips, sp1, sp2 = head("Hips"), head("Spine1"), head("Spine2")
lateral = (l_sh - r_sh).normalized()
center = (l_sh + r_sh) / 2
height = max(head("HeadTop_End").z if "mixamorig:HeadTop_End" in arm.data.bones else head("Head").z + 0.2, 1e-3)
floor_z = min(head("LeftToeBase").z, head("RightToeBase").z) - 0.05
shoulder_z = center.z
print("height %.2f shoulder_z %.2f floor %.2f" % (height, shoulder_z, floor_z))

for ob in meshes:
    me = ob.data
    groups = {g.name: g.index for g in ob.vertex_groups}

    def group(name):
        if name not in groups:
            groups[name] = ob.vertex_groups.new(name=name).index
        return ob.vertex_groups[groups[name]]

    cols = vertex_colours(ob)
    bright = cols.min(axis=1)
    sat = cols.max(axis=1) - cols.min(axis=1)
    nv = len(me.vertices)
    pos = [ob.matrix_world @ v.co for v in me.vertices]
    old = [{ob.vertex_groups[g.group].name: g.weight for g in v.groups} for v in me.vertices]
    # raw scores: 1 for drape / shoulder-piece candidates
    s_drape = np.zeros(nv)
    s_dome = np.zeros(nv)
    for i, p in enumerate(pos):
        side = "Left" if (p - center).dot(lateral) > 0 else "Right"
        sh, el = (l_sh, l_el) if side == "Left" else (r_sh, r_el)
        upper = old[i].get("mixamorig:LeftArm", 0) + old[i].get("mixamorig:RightArm", 0)
        if bright[i] > opt["white_min"] and sat[i] < opt["sat_max"] and p.z < shoulder_z + 0.05 * height:
            s_drape[i] = 1.0
        elif upper > 0.2 and (p.z > shoulder_z + opt["dome_above"] * height
                              or seg_dist(p, sh, el) > opt["dome_far"] * height):
            s_dome[i] = 1.0
    # smooth over the mesh so neighbours agree (no tearing between bones)
    ev = np.zeros(len(me.edges) * 2, dtype=np.int64)
    me.edges.foreach_get("vertices", ev)
    ev = ev.reshape(-1, 2)
    deg = np.zeros(nv)
    np.add.at(deg, ev[:, 0], 1)
    np.add.at(deg, ev[:, 1], 1)
    for _ in range(opt.get("smooth", 8)):
        for arr in (s_drape, s_dome):
            acc = np.zeros(nv)
            np.add.at(acc, ev[:, 0], arr[ev[:, 1]])
            np.add.at(acc, ev[:, 1], arr[ev[:, 0]])
            arr[:] = 0.5 * arr + 0.5 * acc / np.maximum(deg, 1)
    n_drape = int((s_drape > 0.5).sum())
    n_dome = int((s_dome > 0.5).sum())
    s_drape *= opt.get("drape_strength", 1.0)
    for i, p in enumerate(pos):
        a, b = s_drape[i], s_dome[i]
        if a < 0.02 and b < 0.02:
            continue
        side = "Left" if (p - center).dot(lateral) > 0 else "Right"
        target = {}
        if a >= 0.02:
            t = max(0.0, min(1.0, (shoulder_z - p.z) / max(shoulder_z - floor_z, 1e-3)))
            anchors = [(0.0, "mixamorig:%sShoulder" % side), (0.2, "mixamorig:Spine2"),
                       (0.4, "mixamorig:Spine1"), (1.0, "mixamorig:Hips")]
            for (t0, b0), (t1, b1) in zip(anchors, anchors[1:]):
                if t <= t1:
                    f = (t - t0) / (t1 - t0)
                    target[b0] = target.get(b0, 0) + a * (1 - f)
                    target[b1] = target.get(b1, 0) + a * f
                    break
        if b >= 0.02:
            k = "mixamorig:%sShoulder" % side
            target[k] = target.get(k, 0) + b
        keep = max(0.0, 1.0 - a - b)
        if keep < 0:
            keep = 0.0
        blend = {k: w * keep for k, w in old[i].items()}
        for k, w in target.items():
            blend[k] = blend.get(k, 0) + w
        tot = sum(blend.values()) or 1.0
        v = me.vertices[i]
        for g in list(v.groups):
            ob.vertex_groups[g.group].remove([i])
        for k, w in sorted(blend.items(), key=lambda kv: -kv[1])[:4]:
            if w / tot > 1e-3:
                group(k).add([i], w / tot, "REPLACE")
    print("%s: %d drape vertices, %d shoulder-piece vertices re-weighted of %d"
          % (ob.name, n_drape, n_dome, len(me.vertices)))

if ad:
    for t in ad.nla_tracks:
        t.mute = False
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
