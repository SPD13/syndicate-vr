"""Blender (background) script: converts a rigged GLB to an FBX for Mixamo.

blender -b --factory-startup -P blender_glb2fbx.py -- in.glb out.fbx
The model is scaled to HEIGHT metres (default 1.8); textures are left out.
"""
import sys

import bpy

args = sys.argv[sys.argv.index("--") + 1:]
src, dst = args[0], args[1]
HEIGHT = float(args[2]) if len(args) > 2 else 1.8
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
# Tripo models are 1 unit tall; Mixamo expects a human size, in cm.
for ob in bpy.data.objects:
    if ob.parent is None:
        ob.scale = [v * HEIGHT for v in ob.scale]
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
bpy.ops.export_scene.fbx(filepath=dst, path_mode="STRIP", embed_textures=False,
                         apply_scale_options="FBX_SCALE_ALL", add_leaf_bones=False,
                         bake_anim=False, object_types={"ARMATURE", "MESH"})
print("exported", dst)
