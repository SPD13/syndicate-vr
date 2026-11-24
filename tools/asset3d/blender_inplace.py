"""Blender (background) script: makes locomotion clips play in place.

blender -b --factory-startup -P blender_inplace.py -- in.glb out.glb clip[,clip...]
For each named clip, location channels of the Hips that drift by more than
5% of the hips height between the first and last key lose that drift (a
linear ramp) and are centred on the rest position, keeping the bob and sway.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, out, names = args[0], args[1], args[2].split(",")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
hips_h = arm.data.bones["mixamorig:Hips"].head_local.length


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


for name in names:
    act = bpy.data.actions.get(name)
    if act is None:
        print("no clip", name)
        continue
    for fc in action_fcurves(act):
        if fc.data_path.endswith('"mixamorig:Hips"].location') and len(fc.keyframe_points) > 1:
            k = fc.keyframe_points
            f0, v0 = k[0].co
            f1, v1 = k[-1].co
            if abs(v1 - v0) > 0.05 * hips_h and f1 > f0:
                for kp in k:
                    d = (v1 - v0) * (kp.co[0] - f0) / (f1 - f0)
                    kp.co[1] -= d
                    kp.handle_left[1] -= d
                    kp.handle_right[1] -= d
                # also centre the channel on the rest position (0): Tripo clips
                # may start with the hips already moved along the travel axis
                mean = sum(kp.co[1] for kp in k) / len(k)
                for kp in k:
                    kp.co[1] -= mean
                    kp.handle_left[1] -= mean
                    kp.handle_right[1] -= mean
                print("in place: %s [%d] drift %.3f, offset %.3f" % (name, fc.array_index, v1 - v0, mean))
ad = arm.animation_data
if ad:
    ad.action = None
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
