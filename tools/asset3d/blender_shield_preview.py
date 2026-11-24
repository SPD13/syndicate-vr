"""Blender (background) script: energy shield look on a character, as a preview.

blender -b -P blender_shield_preview.py -- model.glb outdir clip[,clip...]
A copy of each skinned mesh, pushed out along the normals after the armature
(so it follows every clip), gets a shield material: transparent facing the
camera, cyan glow at grazing angles (fresnel), a scrolling noise shimmer and a
pulsing strength. Renders with EEVEE from 3/4 front, right side and back
(CLIP_VIEWS), frames written like blender_render_clips.py so make_gifs.py can
build the page. Only the look is decided here; the game would do it as a shader.
"""
import json
import math
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, outdir, clips = args[0], os.path.abspath(args[1]), args[2].split(",")
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and (o.parent is None or o.name.startswith("weapon_")):
        bpy.data.objects.remove(o, do_unlink=True)
scene = bpy.context.scene

# --- shield material --------------------------------------------------------
mat = bpy.data.materials.new("shield")
mat.use_nodes = True
for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
    if hasattr(mat, attr):
        try:
            setattr(mat, attr, val)
        except Exception:
            pass
if hasattr(mat, "use_backface_culling"):
    mat.use_backface_culling = True
nt = mat.node_tree
nt.nodes.clear()
N = nt.nodes.new
out = N("ShaderNodeOutputMaterial")
mix = N("ShaderNodeMixShader")
transp = N("ShaderNodeBsdfTransparent")
emit = N("ShaderNodeEmission")
emit.inputs["Color"].default_value = (0.25, 0.85, 1.0, 1)
lw = N("ShaderNodeLayerWeight")
lw.inputs["Blend"].default_value = 0.45
ramp = N("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].position = 0.05
ramp.color_ramp.elements[1].position = 0.6
noise = N("ShaderNodeTexNoise")
noise.inputs["Scale"].default_value = 9.0
noise.inputs["Detail"].default_value = 2.0
mapping = N("ShaderNodeMapping")
texco = N("ShaderNodeTexCoord")
band = N("ShaderNodeValToRGB")  # thin bright bands from the noise
band.color_ramp.elements[0].position = 0.49  # black - white - black: thin lines, not half the surface
band.color_ramp.elements[1].position = 0.51
band.color_ramp.elements[1].color = (0, 0, 0, 1)
mid = band.color_ramp.elements.new(0.5)
mid.color = (1, 1, 1, 1)
add = N("ShaderNodeMath")
add.operation = "ADD"
mul = N("ShaderNodeMath")
mul.operation = "MULTIPLY"
mul.inputs[1].default_value = 0.5
clamp = N("ShaderNodeClamp")
L = nt.links.new
L(texco.outputs["Object"], mapping.inputs["Vector"])
L(mapping.outputs["Vector"], noise.inputs["Vector"])
L(noise.outputs["Fac"], band.inputs["Fac"])
L(band.outputs["Color"], mul.inputs[0])
powr = N("ShaderNodeMath")  # glow only at grazing angles: facing^3
powr.operation = "POWER"
powr.inputs[1].default_value = 3.0
L(lw.outputs["Facing"], powr.inputs[0])
L(powr.outputs["Value"], ramp.inputs["Fac"])
L(ramp.outputs["Color"], add.inputs[0])
L(mul.outputs["Value"], add.inputs[1])
L(add.outputs["Value"], clamp.inputs["Value"])
L(clamp.outputs["Result"], mix.inputs["Fac"])
L(transp.outputs["BSDF"], mix.inputs[1])
L(emit.outputs["Emission"], mix.inputs[2])
L(mix.outputs["Shader"], out.inputs["Surface"])
# pulse and shimmer driven by the frame
d = emit.inputs["Strength"].driver_add("default_value").driver
d.expression = "2.2 + 1.2 * sin(frame * 0.35)"
for i, expr in enumerate(("frame * 0.004", "0", "frame * 0.012")):
    dd = mapping.inputs["Location"].driver_add("default_value", i).driver
    dd.expression = expr

# --- shield shells ------------------------------------------------------------
for body in [o for o in bpy.data.objects if o.type == "MESH" and o.parent is arm]:
    shell = body.copy()
    shell.data = body.data.copy()
    shell.name = body.name + "_shield"
    scene.collection.objects.link(shell)
    import bmesh  # generated meshes have some inward normals: make them all point out
    bm = bmesh.new()
    bm.from_mesh(shell.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(shell.data)
    bm.free()
    shell.data.materials.clear()
    shell.data.materials.append(mat)
    disp = shell.modifiers.new("push", "DISPLACE")  # after the Armature modifier
    disp.direction = "NORMAL"
    disp.strength = 0.06
    disp.mid_level = 0.0

# --- scene, cameras ---------------------------------------------------------------
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 420, 520
scene.view_settings.view_transform = "Standard"
scene.view_settings.exposure = 0.0
world = bpy.data.worlds.new("w")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.05, 0.055, 0.07, 1)
world.node_tree.nodes["Background"].inputs["Strength"].default_value = 1.0
for loc, energy in (((3, -4, 5), 900), ((-4, -2, 3), 400), ((0, 5, 4), 500)):
    ld = bpy.data.lights.new("l", "AREA")
    ld.energy, ld.size = energy, 3
    lo = bpy.data.objects.new("l", ld)
    lo.location = loc
    lo.rotation_euler = (Vector((0, 0, 1)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(lo)
bpy.ops.mesh.primitive_plane_add(size=4)
floor = bpy.context.active_object
fm = bpy.data.materials.new("floor")
fm.use_nodes = True
fm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.03, 0.03, 0.035, 1)
fm.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.35
floor.data.materials.append(fm)

VIEWS = [int(v) for v in os.environ.get("CLIP_VIEWS", "30,90,180").split(",")]
cams = []
for k in VIEWS:
    cd = bpy.data.cameras.new("c")
    cd.type = "ORTHO"
    cd.ortho_scale = 2.4
    c = bpy.data.objects.new("c", cd)
    scene.collection.objects.link(c)
    cams.append(c)


def posed(n):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return ev.matrix_world @ ev.pose.bones[n].head


ad = arm.animation_data
tracks = {t.name: t for t in ad.nla_tracks}
for t in ad.nla_tracks:
    t.mute = True
for clip in clips:
    act = tracks[clip].strips[0].action
    ad.action = act
    if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]
    f0, f1 = int(act.frame_range[0]), int(act.frame_range[1])
    scene.frame_set(f0)
    side = posed("mixamorig:LeftShoulder") - posed("mixamorig:RightShoulder")
    fwd = side.cross(Vector((0, 0, 1)))
    fwd.z = 0
    fwd.normalize()
    right = fwd.cross(Vector((0, 0, 1)))
    target = Vector((0, 0, 0.95))
    for c, yaw in zip(cams, VIEWS):
        a = math.radians(yaw)
        c.location = target + (fwd * math.cos(a) + right * math.sin(a)) * 6 + Vector((0, 0, 1.2))
        c.rotation_euler = (target - c.location).to_track_quat("-Z", "Y").to_euler()
    step = max(2, math.ceil((f1 - f0 + 1) / 36))
    dd = os.path.join(outdir, "shield_" + clip)
    os.makedirs(dd, exist_ok=True)
    n = 0
    for f in range(f0, f1 + 1, step):
        scene.frame_set(f)
        for k, c in enumerate(cams):
            scene.camera = c
            scene.render.filepath = os.path.join(dd, "f%03d_%d.png" % (n, k))
            bpy.ops.render.render(write_still=True)
        n += 1
    json.dump({"clip": clip, "weapon": None, "frames": n, "fps": scene.render.fps / scene.render.fps_base / step},
              open(os.path.join(dd, "info.json"), "w"))
    print("clip done", clip, n)
