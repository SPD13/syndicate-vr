"""Blender (background) script: renders every clip of an animated GLB to frames.

blender -b --factory-startup -P blender_render_clips.py -- model.glb outdir jobs.json

jobs.json: [{"clip": "pistol_walk", "weapon": "uzi" | null, "name": "pistol_walk_uzi"}, ...]
Writes outdir/<name>/f###_<view>.png: 3/4 front and right side views,
placed from the facing of each clip's first frame (chest normal), Workbench with texture colours, half the scene frame rate or less, at most 48 frames.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, outdir, jobs = args[0], os.path.abspath(args[1]), json.load(open(args[2]))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
ad = arm.animation_data
for t in ad.nla_tracks:
    t.mute = True
actions = {t.name: t.strips[0].action for t in ad.nla_tracks}

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
sh = scene.display.shading
sh.light = "STUDIO"
sh.color_type = "TEXTURE"
sh.show_shadows = True
sh.show_cavity = True
scene.render.resolution_x, scene.render.resolution_y = 300, 380
world = bpy.data.worlds.new("w")
scene.world = world
world.color = (0.32, 0.33, 0.37)
scene.render.film_transparent = False

def posed(name):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return ev.matrix_world @ ev.pose.bones[name].head


def facing():
    """Facing of the current pose: the chest's normal, from the shoulders
    (left - right) crossed with up, checked against the toes."""
    side = posed("mixamorig:LeftShoulder") - posed("mixamorig:RightShoulder")
    f = side.cross(Vector((0, 0, 1)))
    f.z = 0
    f.normalize()
    toes = sum((posed("mixamorig:%sToeBase" % s) - posed("mixamorig:%sFoot" % s) for s in ("Left", "Right")),
               Vector((0, 0, 0)))
    toes.z = 0
    if toes.length > 1e-4 and f.dot(toes) < 0:
        print("  warning: chest and toes disagree")
    return f


cams = []
VIEWS = [int(v) for v in __import__("os").environ.get("CLIP_VIEWS", "30,90").split(",")]
for k in range(len(VIEWS)):
    cd = bpy.data.cameras.new("cam%d" % k)
    cd.type = "ORTHO"
    cd.ortho_scale = float(__import__("os").environ.get("CLIP_ORTHO", "2.5"))
    cam = bpy.data.objects.new("cam%d" % k, cd)
    scene.collection.objects.link(cam)
    cams.append(cam)


def place_cameras(fwd):
    right = fwd.cross(Vector((0, 0, 1)))
    target = Vector((0, 0, float(__import__("os").environ.get("CLIP_TARGET_Z", "0.95"))))
    for cam, yaw in zip(cams, VIEWS):  # 3/4 front from the right-front, right side
        a = math.radians(yaw)
        cam.location = target + (fwd * math.cos(a) + right * math.sin(a)) * 6 + Vector((0, 0, 1.2))
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()

# floor grid reference: a thin plane
bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0, 0))
floor = bpy.context.active_object
mat = bpy.data.materials.new("floor")
mat.diffuse_color = (0.22, 0.23, 0.26, 1)
floor.data.materials.append(mat)

for job in jobs:
    act = actions[job["clip"]]
    ad.action = act
    if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]
    for o in bpy.data.objects:
        if o.name.startswith("weapon_"):
            o.hide_render = job.get("weapon") is None or o.name != "weapon_" + job["weapon"]
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    scene.frame_set(f0)
    fwd = facing()
    place_cameras(fwd)
    print("  %s facing %s" % (job["name"], tuple(round(v, 2) for v in fwd)))
    step = max(2, math.ceil((f1 - f0 + 1) / 48))
    d = os.path.join(outdir, job["name"])
    os.makedirs(d, exist_ok=True)
    n = 0
    for f in range(f0, f1 + 1, step):
        scene.frame_set(f)
        for k, cam in enumerate(cams):
            scene.camera = cam
            scene.render.filepath = os.path.join(d, "f%03d_%d.png" % (n, k))
            bpy.ops.render.render(write_still=True)
        n += 1
    with open(os.path.join(d, "info.json"), "w") as fh:
        json.dump({"clip": job["clip"], "weapon": job.get("weapon"), "frames": n,
                   "fps": scene.render.fps / scene.render.fps_base / step}, fh)
    print("clip done", job["name"], n)
