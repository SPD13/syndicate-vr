"""Blender (background) script: attaches weapon GLBs to a character's right hand.

blender -b --factory-startup -P blender_attach_weapons.py -- weapons.json in.glb out.glb

weapons.json: {"uzi": {"file": "...", "length": 0.30, "pose": "pistol_shoot",
                       "aim": "forearm", "frac": 0.3, "grip_in": 0.0,
                       "flip": false}, ...}

For each weapon:
- its own frame is found from the mesh: forward = the longest principal
  axis, pointing away from the grip; up = the second axis, pointing away
  from the grip (the grip is the long tail below the body); grip point =
  centroid of the lowest 15% of the vertices along up, the top of it.
- in the reference pose (clip + position in it) the wanted world frame is:
  forward along the forearm ("forearm") or from the right hand to the left
  hand ("hands"), up = world up made orthogonal; the grip sits at the palm
  (between the hand bone and the middle finger base).
- the weapon is scaled to `length` metres and parented to RightHand with
  that relative transform, so it follows every clip. Nodes are named
  weapon_<name>; the game shows one at a time.
- "flip_up": true turns the weapon over when the grip heuristic fails.
- "grip_at": 0..1 places the handle by hand, as a fraction of the length
  from the back, when the lowest part is not the handle (a fuel canister,
  a long barrel); "grip_h": 0..1 the hand's height on it.
- "fit": "fist" (default) lines the handle's axis up with the knuckles and
  centres it in the curled fingers; "palm" only puts the grip at the palm.
  "palm_gap" (0.025 m): how far the handle's axis is out from the palm.
- two-handed weapons get an empty lhand_<name> on top of the body at
  "support_at" (default 0.62 of the length from the back), "support_up"
  (0.03 m) above it: the left hand's IK target.
- "pitch": degrees to raise the nose when the main axis is off (L shapes).
- <out>_sockets.json records each weapon's matrix relative to RightHand
  (the bone tail), for attaching weapon GLBs at runtime.
"""
import json
import sys

import bpy
import numpy as np
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
cfg = json.load(open(args[0], encoding="utf-8"))
src, out = args[1], args[2]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
for o in list(bpy.data.objects):
    if o.type == "MESH" and o.parent is None:
        bpy.data.objects.remove(o, do_unlink=True)
ad = arm.animation_data
tracks = {t.name: t for t in ad.nla_tracks} if ad else {}
scene = bpy.context.scene


def set_pose(clip, frac):
    for t in ad.nla_tracks:
        t.mute = True
    act = tracks[clip].strips[0].action
    ad.action = act
    if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
        ad.action_slot = act.slots[0]
    f0, f1 = act.frame_range
    scene.frame_set(int(round(f0 + (f1 - f0) * frac)))
    bpy.context.view_layer.update()


def pose_bone(name):
    ev = arm.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return ev, ev.pose.bones[name]


def bone_world(name, tail=False):
    ev, pb = pose_bone(name)
    return ev.matrix_world @ (pb.tail if tail else pb.head)


def weapon_frame(objs, flip_up=False, grip_at=None, pitch=0.0, grip_h=None, support_at=None):
    pts = np.array([tuple(o.matrix_world @ v.co) for o in objs for v in o.data.vertices])
    c = pts.mean(0)
    w, v = np.linalg.eigh(np.cov((pts - c).T))
    order = np.argsort(w)[::-1]
    fwd, up = v[:, order[0]], v[:, order[1]]
    pu = (pts - c) @ up
    if (np.mean(pu ** 3) > 0) != bool(flip_up):  # the grip is the long tail: make it point down
        up = -up
        pu = -pu
    grip_pts = pts[pu < np.percentile(pu, 15)]
    grip = grip_pts.mean(0)
    pf = (pts - c) @ fwd
    if np.mean(pf) - (grip - c) @ fwd < 0 or (pf.max() - (grip - c) @ fwd) < ((grip - c) @ fwd - pf.min()):
        fwd = -fwd
        pf = -pf
    up = up - fwd * (up @ fwd)
    up /= np.linalg.norm(up)
    if pitch:  # raise the nose by `pitch` degrees (PCA follows the diagonal of L-shaped guns)
        a = np.radians(pitch)
        fwd, up = fwd * np.cos(a) + up * np.sin(a), up * np.cos(a) - fwd * np.sin(a)
        pf, pu = (pts - c) @ fwd, (pts - c) @ up
    if grip_at is not None:  # handle given by hand: fraction of the length from the back
        t = (pf - pf.min()) / (pf.max() - pf.min())
        win = np.abs(t - grip_at) < 0.05
        grip_pts = pts[win][pu[win] < np.percentile(pu[win], 50)]
    if grip_at is not None and grip_h is not None:  # and the height on it (0 bottom, 1 top)
        h = (pu - pu.min()) / (pu.max() - pu.min())
        sel = win & (np.abs(h - grip_h) < 0.08)
        top = pts[sel].mean(0) if sel.any() else grip_pts.mean(0)
    else:
        # hold the grip near its top, where the hand closes
        top = grip_pts[((grip_pts - c) @ up) > np.percentile((grip_pts - c) @ up, 60)].mean(0)
    length = pf.max() - pf.min()
    # handle axis: main axis of the handle's points, pointing down (index -> little finger)
    g = grip_pts - grip_pts.mean(0)
    gw, gv = np.linalg.eigh(np.cov(g.T))
    hax = gv[:, np.argmax(gw)]
    if gw.max() < 2.5 * np.sort(gw)[-2] or abs(hax @ up) < 0.5:
        hax = -up  # no clear handle shape: straight down
    if hax @ up > 0:
        hax = -hax
    # support point for the other hand: on top of the body, `support_at` of the length from the back
    sup = None
    if support_at is not None:
        t = (pf - pf.min()) / (pf.max() - pf.min())
        win = np.abs(t - support_at) < 0.04
        sup = Vector(pts[win][pu[win] > np.percentile(pu[win], 85)].mean(0))
    return Vector(fwd), Vector(up), Vector(top), length, Vector(hax), sup


def frame_matrix(fwd, up, origin):
    fwd = fwd.normalized()
    up = (up - fwd * up.dot(fwd)).normalized()
    right = fwd.cross(up)
    m = Matrix((right, fwd, up)).transposed().to_4x4()  # columns: X=right, Y=fwd, Z=up
    m.translation = origin
    return m


sockets = {}
for name, w in cfg.items():
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=w["file"])
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    for o in new:
        if o.type != "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    gun = bpy.context.view_layer.objects.active
    gun.parent = None
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    gun.name = "weapon_" + name

    two_hands = w.get("aim") == "hands"
    fwd, up, grip, length, hax, sup = weapon_frame(
        [gun], w.get("flip_up"), w.get("grip_at"), w.get("pitch", 0.0), w.get("grip_h"),
        w.get("support_at", 0.62) if two_hands else None)
    if w.get("flip"):  # the muzzle heuristic picked the wrong end
        fwd = -fwd
    s = w["length"] / length
    # weapon-local frame: grip at origin, Y forward, Z up, scaled to size
    local = frame_matrix(fwd, up, grip).inverted()
    to_local = Matrix.Scale(s, 4) @ local
    gun.data.transform(to_local)
    hd = (local.to_3x3() @ hax).normalized()  # handle axis (down) in the weapon frame

    set_pose(w["pose"], w.get("frac", 0.5))
    hand = bone_world("mixamorig:RightHand")
    mid = bone_world("mixamorig:RightHandMiddle1")
    palm = hand.lerp(mid, 0.55)
    if w.get("aim") == "hands":
        f = (bone_world("mixamorig:LeftHand") - hand).normalized()
    else:
        f = (hand - bone_world("mixamorig:RightForeArm")).normalized()
    u = Vector((0, 0, 1))
    if abs(f.dot(u)) > 0.95:
        u = Vector((0, 1, 0))
    target = frame_matrix(f, u, palm + f * w.get("grip_in", 0.0))
    if w.get("fit", "fist") == "fist":
        # the handle runs through the closed fist: along the knuckles (index -> little
        # finger), centred in the curl of the fingers; the muzzle as close to the aim
        # as the handle's slant allows
        knuck = (bone_world("mixamorig:RightHandPinky1") - bone_world("mixamorig:RightHandIndex1")).normalized()
        # the top of the handle sits against the palm at the index finger's level, a
        # handle's half-thickness out from the palm (palm normal: towards the thumb's curl)
        evr, rpb = pose_bone("mixamorig:RightHand")
        hy = ((evr.matrix_world @ rpb.matrix).to_3x3().col[1]).normalized()
        i1, m1 = bone_world("mixamorig:RightHandIndex1"), bone_world("mixamorig:RightHandMiddle1")
        n = hy.cross(i1 - bone_world("mixamorig:RightHandPinky1")).normalized()
        if n.dot(bone_world("mixamorig:RightHandThumb3") - m1) < 0:
            n = -n
        hole = palm + (i1 - m1) + n * w.get("palm_gap", 0.025)
        # aim first (muzzle along the aim), then the roll: the handle as close to the knuckles as it goes
        y = Vector((0, 1, 0))
        hp = (hd - y * hd.dot(y)).normalized()
        src_m = Matrix((y, hp, y.cross(hp))).transposed()
        kp = (knuck - f * knuck.dot(f)).normalized()
        dst_m = Matrix((f, kp, f.cross(kp))).transposed()
        target = Matrix.Translation(hole + f * w.get("grip_in", 0.0)) @ (dst_m @ src_m.transposed()).to_4x4()
    print("  %s frame %d hand %s lhand %s fwd %s" % (name, scene.frame_current, tuple(round(v, 2) for v in hand),
          tuple(round(v, 2) for v in bone_world("mixamorig:LeftHand")), tuple(round(v, 2) for v in f)))

    ev, pb = pose_bone("mixamorig:RightHand")
    bone_world_m = ev.matrix_world @ pb.matrix
    gun.parent = arm
    gun.parent_type = "BONE"
    gun.parent_bone = "mixamorig:RightHand"
    # parenting to a bone places the child at the bone tail
    tail_m = bone_world_m @ Matrix.Translation((0, pb.length, 0))
    gun.matrix_parent_inverse = Matrix.Identity(4)
    gun.matrix_basis = tail_m.inverted() @ target
    sockets[name] = {"bone": "mixamorig:RightHand", "matrix": [list(r) for r in gun.matrix_basis],
                      "pose": w["pose"], "hold": "two hands" if two_hands else "one hand"}
    if sup is not None:
        # left-hand target: an empty on top of the body, a hand's thickness above it
        lh = bpy.data.objects.new("lhand_" + name, None)
        scene.collection.objects.link(lh)
        lh.parent = gun
        # the left hand bone's wanted pose: palm down on the top, fingers over the far side
        # (forward and to the right), wrist behind the palm; its location is the wrist
        set_pose(w["pose"], w.get("frac", 0.5))
        ev, lpb = pose_bone("mixamorig:LeftHand")
        hm = (ev.matrix_world @ lpb.matrix).to_3x3()
        hy = hm.col[1].normalized()
        across = bone_world("mixamorig:LeftHandIndex1") - bone_world("mixamorig:LeftHandPinky1")
        n = hy.cross(across).normalized()
        if n.dot(bone_world("mixamorig:LeftHandThumb3") - bone_world("mixamorig:LeftHandMiddle1")) < 0:
            n = -n  # n: palm normal
        axes = [(i, sg) for i in (0, 2) for sg in (1, -1)]
        pi, ps = max(axes, key=lambda a: a[1] * hm.col[a[0]].normalized().dot(n))
        gw = gun.matrix_world.to_3x3()
        fwd_w, right_w, up_w = gw.col[1].normalized(), gw.col[0].normalized(), gw.col[2].normalized()
        fy = (fwd_w * w.get("support_fwd", 0.45) + right_w).normalized()
        cols = [None, fy, None]
        cols[pi] = -up_w * ps  # palm axis looks down onto the gun
        k = 2 - pi
        cols[k] = (cols[(k + 1) % 3].cross(cols[(k + 2) % 3])).normalized()
        cols[pi] = cols[(pi + 1) % 3].cross(cols[(pi + 2) % 3]).normalized()
        rot = Matrix((cols[0], cols[1], cols[2])).transposed()
        palm_c = gun.matrix_world @ (to_local @ sup) + up_w * w.get("support_up", 0.025)
        wrist = palm_c - fy * lpb.length * 0.55
        lh.matrix_world = Matrix.Translation(wrist) @ rot.to_4x4()
        sockets[name]["left_hand_target"] = [list(r) for r in lh.matrix_basis]
        print("  %s left hand target at %.2f of the length, palm axis %s%s" % (name, w.get("support_at", 0.62),
              "-+"[ps > 0], "XYZ"[pi]))
    print("attached %s: length %.2f m (scale %.3f), pose %s" % (name, w["length"], s, w["pose"]))

ad.action = None
for t in ad.nla_tracks:
    t.mute = False
scene.frame_set(1)
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", export_animation_mode="NLA_TRACKS",
                          export_force_sampling=True, export_frame_range=False)
import os  # noqa: E402
json.dump(sockets, open(os.path.splitext(out)[0] + "_sockets.json", "w"), indent=1)
print("exported", out)
