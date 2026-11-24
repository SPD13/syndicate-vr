"""Blender (background) script: builds one animated GLB from Mixamo FBX clips.

blender -b --factory-startup -P blender_merge_anims.py -- clips.json out.glb

clips.json:
  {"base_fbx": "...agent_rigged_v2.fbx",     # character as Mixamo returned it
   "material_glb": "...agent_rigged.glb",    # Tripo model with the textures
   "clips": {"rifle_walk": {"file": "...walk forward.fbx", "in_place": true}, ...}}

- The base armature and mesh come from the Mixamo FBX, so every clip has
  the same rest pose; the textured material comes from the Tripo GLB (the
  mesh and UVs are unchanged by Mixamo).
- Stray helper meshes without a parent (Tripo's icosphere) are removed.
- in_place: removes the travel of the Hips (a linear ramp from the first to
  the last key) on location channels that drift, keeping the bob and sway.
"""
import json
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
cfg = json.load(open(args[0], encoding="utf-8"))
out = args[1]

bpy.ops.wm.read_factory_settings(use_empty=True)


def import_new(path):
    before = set(bpy.data.objects)
    if path.lower().endswith((".glb", ".gltf")):
        bpy.ops.import_scene.gltf(filepath=path)
    else:
        bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False)
    return [o for o in bpy.data.objects if o not in before]


def remove(objs):
    for o in objs:
        bpy.data.objects.remove(o, do_unlink=True)


def action_fcurves(action):
    """F-curves of an action, for both the legacy and the layered (4.4+) API."""
    if hasattr(action, "fcurves") and len(getattr(action, "fcurves", [])):
        return list(action.fcurves)
    curves = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []):
                curves.extend(bag.fcurves)
    return curves


# ---- base character ------------------------------------------------------
base_objs = import_new(cfg["base_fbx"])
arm = next(o for o in base_objs if o.type == "ARMATURE")
meshes = [o for o in base_objs if o.type == "MESH" and o.parent is arm]
remove([o for o in base_objs if o.type == "MESH" and o.parent is None])
if arm.animation_data and arm.animation_data.action:
    bpy.data.actions.remove(arm.animation_data.action)
arm.name = "Agent"
hips_h = arm.data.bones["mixamorig:Hips"].head_local.length

# ---- textured material ---------------------------------------------------
mat_objs = import_new(cfg["material_glb"])
src_mesh = next(o for o in mat_objs if o.type == "MESH" and o.parent is not None)
for m in meshes:
    m.data.materials.clear()
    for mat in src_mesh.data.materials:
        m.data.materials.append(mat)
remove(mat_objs)
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)

# ---- clips ---------------------------------------------------------------
if not arm.animation_data:
    arm.animation_data_create()
for name, clip in cfg["clips"].items():
    objs = import_new(clip["file"])
    src = next(o for o in objs if o.type == "ARMATURE")
    act = src.animation_data.action
    act.name = name
    act.use_fake_user = True
    if clip.get("in_place"):
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
                    print("  in place: %s %s[%d] drift %.3f" % (name, fc.data_path, fc.array_index, v1 - v0))
    remove(objs)
    track = arm.animation_data.nla_tracks.new()
    track.name = name
    track.strips.new(name, int(act.frame_range[0]), act)
    print("clip %-16s frames %d-%d" % (name, act.frame_range[0], act.frame_range[1]))

arm.animation_data.action = None
for o in list(bpy.data.objects):
    if o is not arm and o not in meshes:
        bpy.data.objects.remove(o, do_unlink=True)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
print("exported", out)
