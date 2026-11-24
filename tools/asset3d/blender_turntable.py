"""Blender (background) script: renders a static GLB from 4 sides into one sheet.

blender -b --factory-startup -P blender_turntable.py -- model.glb out.png
"""
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], os.path.abspath(args[1])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.data.objects if o.type == "MESH"]
pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
lo = Vector([min(p[i] for p in pts) for i in range(3)])
hi = Vector([max(p[i] for p in pts) for i in range(3)])
center, size = (lo + hi) / 2, max(hi - lo)

scene = bpy.context.scene
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x = scene.render.resolution_y = 512
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.4, 0.41, 0.45, 1)
scene.world = world
cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = size * 1.25
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
for i, (rot, energy) in enumerate((((50, 0, -35), 4.0), ((60, 0, 150), 1.5))):
    ld = bpy.data.lights.new("sun%d" % i, "SUN")
    ld.energy = energy
    lob = bpy.data.objects.new("sun%d" % i, ld)
    lob.rotation_euler = [math.radians(v) for v in rot]
    scene.collection.objects.link(lob)

tiles = []
for k, yaw in enumerate((30, 120, 210, 300)):
    a = math.radians(yaw)
    cam.location = center + Vector((math.sin(a), -math.cos(a), 0.45)).normalized() * size * 3
    cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
    path = out + ".%d.png" % k
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    tiles.append(path)

sheet = bpy.data.images.new("sheet", 2048, 512)
px = [0.0] * (2048 * 512 * 4)
for k, path in enumerate(tiles):
    im = bpy.data.images.load(path)
    src_px = list(im.pixels)
    for y in range(512):
        row = src_px[y * 512 * 4:(y + 1) * 512 * 4]
        start = (y * 2048 + k * 512) * 4
        px[start:start + 512 * 4] = row
    os.remove(path)
sheet.pixels = px
sheet.filepath_raw = out
sheet.file_format = "PNG"
sheet.save()
print("saved", out)
