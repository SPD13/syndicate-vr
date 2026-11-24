"""Blender (background) script: exports each attached weapon as a GLB ready to
put in a hand at runtime.

blender -b -P blender_export_held_weapons.py -- agent_arsenal.glb outdir
For every weapon_<name> node of the arsenal (see blender_attach_weapons.py),
writes <outdir>/<name>.glb with the mesh in its grip frame: origin = where
the right hand closes on the handle, Blender +Y = muzzle direction, +Z = top
of the weapon, real size in metres (the glTF exporter turns this into glTF
-Z forward, +Y up). Two-handed weapons keep their lhand_<name> node: the
left hand's wanted wrist position and orientation on top of the body.
"""
import os
import sys

import bpy
from mathutils import Matrix

args = sys.argv[sys.argv.index("--") + 1:]
src, outdir = args[0], os.path.abspath(args[1])
os.makedirs(outdir, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
weapons = [o for o in bpy.data.objects if o.name.startswith("weapon_")]
for w in weapons:
    kids = [c for c in w.children if c.name.startswith("lhand_")]
    keep = [(c, c.matrix_local.copy()) for c in kids]
    w.parent = None
    w.matrix_world = Matrix.Identity(4)
    for c, ml in keep:  # children stay where they were relative to the weapon
        c.matrix_local = ml
for w in weapons:
    name = w.name[len("weapon_"):]
    bpy.ops.object.select_all(action="DESELECT")
    w.select_set(True)
    for c in w.children:
        c.select_set(True)
    bpy.ops.export_scene.gltf(filepath=os.path.join(outdir, name + ".glb"), export_format="GLB", use_selection=True,
                              export_animations=False, export_skins=False)
    print("held", name, len(w.children))
