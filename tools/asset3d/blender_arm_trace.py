"""Blender (background) script: traces the right arm through a clip.

blender -b --factory-startup -P blender_arm_trace.py -- model.glb clip [step]
Per frame: arm extension (shoulder-hand distance / arm length, 1 = straight),
hand height above the hips, and the hand's forward reach along the chest
normal. Used to find the part of a clip worth keeping (e.g. the shooting
after a holster draw).
"""
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, clip = args[0], args[1]
step = int(args[2]) if len(args) > 2 else 5
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
ad = arm.animation_data
for t in ad.nla_tracks:
    t.mute = True
act = bpy.data.actions[clip]
ad.action = act
if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
    ad.action_slot = act.slots[0]
scene = bpy.context.scene


def p(ev, n):
    return ev.matrix_world @ ev.pose.bones["mixamorig:" + n].head


f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
print("frame  ext  hand_h  reach")
for f in range(f0, f1 + 1, step):
    scene.frame_set(f)
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    sh, el, ha, hips = p(ev, "RightArm"), p(ev, "RightForeArm"), p(ev, "RightHand"), p(ev, "Hips")
    length = (el - sh).length + (ha - el).length
    side = p(ev, "LeftShoulder") - p(ev, "RightShoulder")
    fwd = side.cross(Vector((0, 0, 1)))
    fwd.z = 0
    fwd.normalize()
    print("%5d %5.2f %7.2f %6.2f" % (f, (ha - sh).length / length, ha.z - hips.z, (ha - sh).dot(fwd)))
