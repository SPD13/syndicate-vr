"""Blender (background) script: measures wrist bends, to find where hands break.

blender -b --factory-startup -P blender_wrist_check.py -- file[@clip] ...
For each file (GLB or FBX; @clip picks an action by name, else the first),
prints for the rest pose and a few frames: the angle between forearm
(ForeArm->Hand) and hand (Hand->HandMiddle1) for each side, and the hand
direction relative to world up. A relaxed hand is nearly straight (<30 deg).
"""
import math
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]


def angle(a, b):
    return math.degrees(a.angle(b)) if a.length > 1e-6 and b.length > 1e-6 else float("nan")


def measure(ev, side):
    def p(n):
        return ev.matrix_world @ ev.pose.bones["mixamorig:%s%s" % (side, n)].head
    fore = p("Hand") - p("ForeArm")
    hand = p("HandMiddle1") - p("Hand")
    return angle(fore, hand), math.degrees(math.asin(max(-1, min(1, hand.normalized().z))))


for spec in args:
    path, _, clip = spec.partition("@")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if path.lower().endswith(".fbx"):
        bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    else:
        bpy.ops.import_scene.gltf(filepath=path)
    arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
    ad = arm.animation_data or arm.animation_data_create()
    for t in ad.nla_tracks:
        t.mute = True
    acts = list(bpy.data.actions)
    act = next((a for a in acts if a.name == clip), None) if clip else (acts[0] if acts else None)
    print("== %s  action=%s  (actions: %s)" % (spec, act.name if act else None, ", ".join(a.name for a in acts[:6])))
    ad.action = None
    scene = bpy.context.scene
    scene.frame_set(0)
    for pb in arm.pose.bones:
        pb.matrix_basis.identity()
    bpy.context.view_layer.update()
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    print("  rest     L bend %5.1f up %6.1f | R bend %5.1f up %6.1f" % (*measure(ev, "Left"), *measure(ev, "Right")))
    if act:
        ad.action = act
        if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
            ad.action_slot = act.slots[0]
        f0, f1 = act.frame_range
        for frac in (0.0, 0.5):
            scene.frame_set(int(f0 + (f1 - f0) * frac))
            bpy.context.view_layer.update()
            ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
            print("  f%-4d    L bend %5.1f up %6.1f | R bend %5.1f up %6.1f"
                  % (scene.frame_current, *measure(ev, "Left"), *measure(ev, "Right")))
