"""Blender (background) script: replaces a warped round top (a lamp head) by a
perfectly round one built from its own average profile.

blender -b -P blender_lathe_top.py -- in.glb out.glb
Run after blender_level_top.py (pole vertical). The head starts where the
radius around the pole axis grows past 1.8x the pole radius. Its profile
(median radius per height slice) and its look per slice (bright = glass,
dark = metal, from the base colour texture) are measured, the head faces are
deleted, and the profile is spun into a 64-sided surface of revolution with
two materials: dark metal and milky emissive glass.
"""
import math
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
ob = next(o for o in bpy.data.objects if o.type == "MESH")
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
me = ob.data
co = np.array([tuple(v.co) for v in me.vertices])
z0, z1 = co[:, 2].min(), co[:, 2].max()
H = z1 - z0
pole = (co[:, 2] > z0 + 0.3 * H) & (co[:, 2] < z0 + 0.6 * H)
cx, cy = co[pole][:, 0].mean(), co[pole][:, 1].mean()
rad = np.hypot(co[:, 0] - cx, co[:, 1] - cy)
pole_r = np.median(rad[pole])

# head bottom: first slice from the top half where the radius jumps
bins = np.linspace(z0 + 0.5 * H, z1, 60)
zb = None
for a, b in zip(bins[:-1], bins[1:]):
    m = (co[:, 2] >= a) & (co[:, 2] < b)
    if m.any() and np.percentile(rad[m], 90) > 1.8 * pole_r:
        zb = a
        break
if zb is None:
    raise SystemExit("no head found")

# brightness per vertex from the base colour texture at its UV
uv_layer = me.uv_layers.active.data
img = None
for mat in me.materials:
    for n in mat.node_tree.nodes if mat and mat.use_nodes else []:
        if n.type == "TEX_IMAGE" and n.image:
            img = n.image
            break
    if img:
        break
bright = np.zeros(len(me.vertices))
if img:
    w, h = img.size
    px = np.array(img.pixels[:]).reshape(h, w, img.channels)
    for loop in me.loops:
        u, v = uv_layer[loop.index].uv
        x = min(w - 1, max(0, int(u % 1.0 * w)))
        y = min(h - 1, max(0, int(v % 1.0 * h)))
        bright[loop.vertex_index] = px[y, x, :3].mean()

# profile: median radius and brightness per slice
head = co[:, 2] >= zb
slices = np.linspace(zb, z1, 25)
prof = []
for a, b in zip(slices[:-1], slices[1:]):
    m = head & (co[:, 2] >= a) & (co[:, 2] <= b)
    if m.sum() < 5:
        continue
    prof.append(((a + b) / 2, float(np.percentile(rad[m], 75)), float(np.median(bright[m]))))
print("head from %.3f to %.3f, pole r %.3f, %d slices" % (zb, z1, pole_r, len(prof)))

# delete the old head faces
bm = bmesh.new()
bm.from_mesh(me)
gone = [f for f in bm.faces if all(v.co.z >= zb for v in f.verts)]
bmesh.ops.delete(bm, geom=gone, context="FACES")
bm.to_mesh(me)
bm.free()

# new head: surface of revolution, closed at the top
metal = bpy.data.materials.new("lamp_metal")
metal.use_nodes = True
p = metal.node_tree.nodes["Principled BSDF"]
p.inputs["Base Color"].default_value = (0.05, 0.055, 0.065, 1)
p.inputs["Metallic"].default_value = 0.9
p.inputs["Roughness"].default_value = 0.35
glass = bpy.data.materials.new("lamp_glass")
glass.use_nodes = True
g = glass.node_tree.nodes["Principled BSDF"]
g.inputs["Base Color"].default_value = (0.85, 0.92, 0.88, 1)
g.inputs["Roughness"].default_value = 0.2
for key in ("Emission Color", "Emission"):
    if key in g.inputs:
        g.inputs[key].default_value = (0.9, 1.0, 0.95, 1)
        break
if "Emission Strength" in g.inputs:
    g.inputs["Emission Strength"].default_value = 1.5
thr = np.median([b for _, _, b in prof]) if prof else 0.5
glass_cut = max(0.35, thr)
seg = 64
pts = [(pole_r, zb)] + [(r, z) for z, r, _ in prof] + [(0.0, z1)]
looks = [prof[0][2] if prof else 0] + [b for _, _, b in prof] + [0]
bm = bmesh.new()
rings = []
for r, z in pts:
    ring = [bm.verts.new((cx + r * math.cos(2 * math.pi * k / seg), cy + r * math.sin(2 * math.pi * k / seg), z))
            for k in range(seg)]
    rings.append(ring)
mats = []
for i in range(len(rings) - 1):
    is_glass = (looks[i] + looks[i + 1]) / 2 > glass_cut and looks[i] > 0.45
    for k in range(seg):
        f = bm.faces.new((rings[i][k], rings[i][(k + 1) % seg], rings[i + 1][(k + 1) % seg], rings[i + 1][k]))
        f.material_index = 1 if is_glass else 0
        f.smooth = True
    mats.append(is_glass)
bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-6)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
hm = bpy.data.meshes.new("lamp_head")
bm.to_mesh(hm)
bm.free()
hm.materials.append(metal)
hm.materials.append(glass)
hob = bpy.data.objects.new("lamp_head", hm)
bpy.context.scene.collection.objects.link(hob)
print("new head: %d rings, glass slices %d of %d" % (len(rings), sum(mats), len(mats)))
bpy.ops.object.select_all(action="SELECT")
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB")
print("exported", out)
