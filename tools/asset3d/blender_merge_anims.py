"""Blender (background) script: builds one animated GLB from Mixamo FBX clips.

blender -b --factory-startup -P blender_merge_anims.py -- clips.json out.glb

clips.json:
  {"base_fbx": "...agent_rigged_v2.fbx",     # character as Mixamo returned it
   "material_glb": "...agent_rigged.glb",    # Tripo model with the textures
   "clips": {"rifle_walk": {"file": "...walk forward.fbx", "in_place": true}, ...}}

- The base armature and mesh come from the Mixamo FBX; the textured
  material comes from the Tripo GLB (the mesh and UVs are unchanged by
  Mixamo). Clips are retargeted by world-space bone orientation and baked,
  since pack clips rest in a T-pose and our rig in an A-pose.
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
def retarget(src, name):
    """Bakes the clip of armature `src` onto `arm` by world-space bone
    orientation (Copy Rotation) and hips position (Copy Location). The clip
    files from Mixamo packs rest in a T-pose while our rig rests in Tripo's
    A-pose, so their local rotations cannot be copied as they are."""
    src_act = src.animation_data.action
    f0, f1 = int(src_act.frame_range[0]), int(src_act.frame_range[1])
    # clips made on another character: scale them to this character's hip
    # height, so the copied hips position (and stride) fits the legs
    a_h = (arm.matrix_world @ arm.data.bones["mixamorig:Hips"].head_local).z
    s_h = (src.matrix_world @ src.data.bones["mixamorig:Hips"].head_local).z
    if s_h > 1e-4 and abs(a_h / s_h - 1) > 0.01:
        src.scale = [v * a_h / s_h for v in src.scale]
        bpy.context.view_layer.update()
    cons = []
    for pb in arm.pose.bones:
        if pb.name not in src.pose.bones:
            continue
        c = pb.constraints.new("COPY_ROTATION")
        c.target, c.subtarget = src, pb.name
        c.target_space = c.owner_space = "WORLD"
        cons.append((pb, c))
        if pb.name == "mixamorig:Hips":
            c = pb.constraints.new("COPY_LOCATION")
            c.target, c.subtarget = src, pb.name
            c.target_space = c.owner_space = "WORLD"
            cons.append((pb, c))
    arm.animation_data.action = None
    bpy.ops.object.mode_set(mode="OBJECT")
    bpy.ops.object.select_all(action="DESELECT")
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.nla.bake(frame_start=f0, frame_end=f1, only_selected=True, visual_keying=True,
                     clear_constraints=True, use_current_action=False, bake_types={"POSE"})
    bpy.ops.object.mode_set(mode="OBJECT")
    act = arm.animation_data.action
    arm.animation_data.action = None
    act.name = name
    act.use_fake_user = True
    return act


if not arm.animation_data:
    arm.animation_data_create()
for name, clip in cfg["clips"].items():
    objs = import_new(clip["file"])
    src = next(o for o in objs if o.type == "ARMATURE")
    act = retarget(src, name)
    for a in list(bpy.data.actions):
        if a is not act and a.users == 0 and not a.use_fake_user:
            bpy.data.actions.remove(a)
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
