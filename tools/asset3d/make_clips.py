"""Writes the clip list for blender_merge_anims.py from a Mixamo download folder.

python make_clips.py <raw_dir> <base_fbx_name or path> <material_glb> <out.json>

A base path outside <raw_dir> lets one character reuse another's Mixamo clips
(the merge scales them to the hip height).

<raw_dir> holds loco_pack/, pistol_pack/, rifle_pack/ (unzipped Mixamo packs)
and single/ (shoot_pistol_v2, shoot_rifle, hit, push_back, picking_up,
throw_grenade .fbx). The same 22 clips as the agent; locomotion clips are made in place.
"""
import json
import os
import sys

raw, base_name, material, out = sys.argv[1:5]
R = os.path.abspath(raw).replace(os.sep, "/")
P, F, L, S = R + "/pistol_pack/", R + "/rifle_pack/", R + "/loco_pack/", R + "/single/"
clips = {
    "idle": (L + "idle.fbx", False), "walk": (L + "walking.fbx", True), "run": (L + "running.fbx", True),
    "pistol_idle": (P + "pistol idle.fbx", False), "pistol_walk": (P + "pistol walk.fbx", True),
    "pistol_run": (P + "pistol run.fbx", True), "pistol_kneel": (P + "pistol kneeling idle.fbx", False),
    "pistol_shoot": (S + "shoot_pistol_v2.fbx", False),
    "rifle_idle": (F + "idle.fbx", False), "rifle_aim": (F + "idle aiming.fbx", False),
    "rifle_walk": (F + "walk forward.fbx", True), "rifle_run": (F + "run forward.fbx", True),
    "rifle_crouch": (F + "idle crouching.fbx", False), "rifle_crouch_aim": (F + "idle crouching aiming.fbx", False),
    "rifle_shoot": (S + "shoot_rifle.fbx", False),
    "hit": (S + "hit.fbx", False), "push_back": (S + "push_back.fbx", False),
    # drop / pick up / plant a mine (the game's one crouch, anim state 13), grenade throw
    "pickup": (S + "picking_up.fbx", False), "throw": (S + "throw_grenade.fbx", False),
    # being persuaded (Mixamo "Zombie Transition", Alert To Ready), burning ("Shaking It Off")
    "persuaded": (S + "zombie_alert_to_ready.fbx", False),
    "burning": (S + "shaking_it_off.fbx", False),
    "death_front": (F + "death from the front.fbx", False), "death_back": (F + "death from the back.fbx", False),
    "death_headshot": (F + "death from front headshot.fbx", False),
}
base_path = base_name if os.path.isabs(base_name) or os.path.exists(base_name) else P + base_name
RANGES = {"pickup": [[16, 148]]}  # Mixamo "Picking Up": bend to the ground and back up (it then lifts the object to the chest)
cfg = {"base_fbx": os.path.abspath(base_path).replace(os.sep, "/"), "material_glb": os.path.abspath(material).replace(os.sep, "/"),
       "clips": {k: dict({"file": f, "in_place": ip}, **({"ranges": RANGES[k]} if k in RANGES else {}))
                 for k, (f, ip) in clips.items()}}
missing = [c["file"] for c in cfg["clips"].values() if not os.path.exists(c["file"])]
if missing or not os.path.exists(cfg["base_fbx"]):
    raise SystemExit("missing: %s" % (missing or cfg["base_fbx"]))
json.dump(cfg, open(out, "w"), indent=1)
print(len(clips), "clips ->", out)
