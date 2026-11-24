"""Blender (background) script: renders poses of an animated GLB to PNGs.

blender -b --factory-startup -P blender_render_poses.py -- model.glb outdir clip:frac [clip:frac ...]
frac is the position in the clip (0 = start, 1 = end). A 3/4 front view,
EEVEE, 384x512, one PNG per pose named <clip>.png.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, outdir, poses = args[0], os.path.abspath(args[1]), args[2:]
os.makedirs(outdir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
if not arm.animation_data:
    arm.animation_data_create()
for t in list(arm.animation_data.nla_tracks):
    arm.animation_data.nla_tracks.remove(t)

scene = bpy.context.scene
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 384, 512
scene.render.film_transparent = False
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.35, 0.36, 0.4, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 1.0
scene.world = world

cam_data = bpy.data.cameras.new("cam")
cam_data.type = "ORTHO"
cam_data.ortho_scale = 2.3
cam = bpy.data.objects.new("cam", cam_data)
scene.collection.objects.link(cam)
scene.camera = cam
ang = math.radians(-30)
cam.location = Vector((math.sin(ang) * -6, -6 * math.cos(ang), 2.2))
target = Vector((0, 0, 0.9))
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
for i, (rot, energy) in enumerate((((50, 0, -35), 4.0), ((60, 0, 150), 1.5))):
    ld = bpy.data.lights.new("sun%d" % i, "SUN")
    ld.energy = energy
    lo = bpy.data.objects.new("sun%d" % i, ld)
    lo.rotation_euler = [math.radians(v) for v in rot]
    scene.collection.objects.link(lo)

for spec in poses:
    name, frac = spec.split(":")
    act = bpy.data.actions.get(name)
    if act is None:
        print("no action", name)
        continue
    arm.animation_data.action = act
    if hasattr(arm.animation_data, "action_slot") and len(getattr(act, "slots", [])):
        arm.animation_data.action_slot = act.slots[0]
    f0, f1 = act.frame_range
    scene.frame_set(int(round(f0 + (f1 - f0) * float(frac))))
    scene.render.filepath = os.path.join(outdir, name + ".png")
    bpy.ops.render.render(write_still=True)
    print("rendered", name)
