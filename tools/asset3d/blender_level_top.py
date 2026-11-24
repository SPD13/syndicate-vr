"""Blender (background) script: straightens a pole-like static model.

blender -b -P blender_level_top.py -- in.glb out.glb [top_fraction=0.22]
1. the whole model is turned so its pole (middle 25-70 % of the height,
   main axis) stands vertical, about the centre of its base;
2. the vertices in the top `top_fraction` (e.g. a lamp head) are rotated
   about their centroid so their flattest direction points straight up;
3. that top part is moved sideways to sit centred over the pole.
"""
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
frac = float(args[2]) if len(args) > 2 else 0.22
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
ob = next(o for o in bpy.data.objects if o.type == "MESH")
me = ob.data
inv = ob.matrix_world.inverted()


def world():
    return np.array([tuple(ob.matrix_world @ v.co) for v in me.vertices])


def apply(m, mask=None):
    for i, vert in enumerate(me.vertices):
        if mask is None or mask[i]:
            vert.co = inv @ (m @ (ob.matrix_world @ vert.co))
    me.update()


def axis(pts, biggest):
    c = pts.mean(0)
    w, v = np.linalg.eigh(np.cov((pts - c).T))
    a = Vector(v[:, np.argmax(w) if biggest else np.argmin(w)])
    return (a if a.z > 0 else -a), c


# 1. pole upright
co = world()
z0, z1 = co[:, 2].min(), co[:, 2].max()
H = z1 - z0
pole = (co[:, 2] > z0 + 0.25 * H) & (co[:, 2] < z0 + 0.7 * H)
pa, pc = axis(co[pole], True)
base_c = Vector(co[co[:, 2] < z0 + 0.08 * H].mean(0))
r = pa.rotation_difference(Vector((0, 0, 1))).to_matrix().to_4x4()
apply(Matrix.Translation(base_c) @ r @ Matrix.Translation(-base_c))
print("pole was %.1f degrees off vertical" % np.degrees(pa.angle(Vector((0, 0, 1)))))
co = world()
z0, z1 = co[:, 2].min(), co[:, 2].max()
sel = co[:, 2] > z1 - frac * (z1 - z0)
pts = co[sel]
c = pts.mean(0)
w, v = np.linalg.eigh(np.cov((pts - c).T))
n = Vector(v[:, np.argmin(w)])
if n.z < 0:
    n = -n
tilt = n.angle(Vector((0, 0, 1)))
rot = n.rotation_difference(Vector((0, 0, 1))).to_matrix().to_4x4()
m = Matrix.Translation(Vector(c)) @ rot @ Matrix.Translation(-Vector(c))
apply(m, sel)
print("levelled %d top vertices, tilt was %.1f degrees" % (int(sel.sum()), np.degrees(tilt)))
# 3. centre the top over the pole
co = world()
pole = (co[:, 2] > z0 + 0.25 * (z1 - z0)) & (co[:, 2] < z0 + 0.7 * (z1 - z0))
pxy = co[pole][:, :2].mean(0)
hxy = co[sel][:, :2].mean(0)
d = Vector((pxy[0] - hxy[0], pxy[1] - hxy[1], 0))
apply(Matrix.Translation(d), sel)
print("top moved by %.3f to sit over the pole" % d.length)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB")
