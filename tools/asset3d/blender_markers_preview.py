"""Blender (background) script: VR world markers, built around real characters.

blender -b -P blender_markers_preview.py -- assets3d_root outdir [marker ...]
Markers (see markers.json for colours, sizes and behaviour):
  selection, hover, lock, health, charge, persuaded, waypoint, damage, objective
Each marker is made of simple emissive, partly transparent meshes; screen-
facing parts copy the camera rotation (billboards). Every marker loops over
48 frames (2 s at 24 fps) and is rendered with EEVEE from the game's
three-quarter view to <outdir>/<marker>/f###_0.png (+ info.json, for
make_gifs.py). The engine would draw the same shapes with its own shaders.
"""
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
root, outdir = args[0], os.path.abspath(args[1])
only = args[2:]
spec = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "markers.json"), encoding="utf-8"))
C = {k: tuple(v) for k, v in spec["colours"].items()}
N = 48

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.frame_start, scene.frame_end = 1, N
scene.render.fps = 24


# --- characters ---------------------------------------------------------------
def load_char(path, loc, yaw_deg, clip="idle"):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    arm = next(o for o in new if o.type == "ARMATURE")
    for o in new:
        if o.type == "MESH" and (o.parent is None or o.name.startswith("weapon_")):
            o.hide_render = True
    arm.location = loc
    arm.rotation_euler[2] += math.radians(yaw_deg)
    ad = arm.animation_data
    if ad:
        tr = {t.name: t for t in ad.nla_tracks}
        for t in ad.nla_tracks:
            t.mute = True
        act = tr[clip].strips[0].action
        ad.action = act
        if hasattr(ad, "action_slot") and len(getattr(act, "slots", [])):
            ad.action_slot = act.slots[0]
    return arm, new


M = os.path.join(root, "models")
agent, agent_objs = load_char(os.path.join(M, "agent", "agent_armed.glb"), (0, 0, 0), 0)
enemy, enemy_objs = load_char(os.path.join(M, "police", "police_armed.glb"), (2.4, 1.6, 0), 200)
civ, civ_objs = load_char(os.path.join(M, "whit_blond_f", "whit_blond_f_armed.glb"), (-1.6, 1.2, 0), 20)
CHARS = {"agent": agent_objs, "enemy": enemy_objs, "civ": civ_objs}

# --- camera, light, floor -------------------------------------------------------
engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
scene.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in engines else "BLENDER_EEVEE"
scene.render.resolution_x, scene.render.resolution_y = 640, 480
scene.view_settings.view_transform = "Standard"
world = bpy.data.worlds.new("w")
scene.world = world
world.use_nodes = True
world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.02, 0.022, 0.03, 1)
for loc, e in (((4, -5, 7), 450), ((-5, -2, 4), 180), ((0, 6, 5), 250)):
    ld = bpy.data.lights.new("l", "AREA")
    ld.energy, ld.size = e, 4
    lo = bpy.data.objects.new("l", ld)
    lo.location = loc
    lo.rotation_euler = (Vector((0.5, 0.5, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.collection.objects.link(lo)
bpy.ops.mesh.primitive_plane_add(size=14, location=(0.5, 0.8, 0))
fm = bpy.data.materials.new("floor")
fm.use_nodes = True
bsdf = fm.node_tree.nodes["Principled BSDF"]
bsdf.inputs["Base Color"].default_value = (0.012, 0.013, 0.017, 1)
bsdf.inputs["Roughness"].default_value = 0.45
bpy.context.active_object.data.materials.append(fm)
cd = bpy.data.cameras.new("cam")
cd.type = "ORTHO"
cd.ortho_scale = 6.6
cam = bpy.data.objects.new("cam", cd)
scene.collection.objects.link(cam)
scene.camera = cam
target = Vector((0.4, 0.6, 1.25))
yaw, pitch = math.radians(-35), math.radians(32)
cam.location = target + Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch))) * 12
cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()


# --- helpers ---------------------------------------------------------------------
GLOW = 0.45  # emission scale: keeps the colours from bleaching to white


def emit_mat(name, col, strength=3.0, alpha=0.9):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
        if hasattr(m, attr):
            try:
                setattr(m, attr, val)
            except Exception:
                pass
    nt = m.node_tree
    nt.nodes.clear()
    o = nt.nodes.new("ShaderNodeOutputMaterial")
    mix = nt.nodes.new("ShaderNodeMixShader")
    tr = nt.nodes.new("ShaderNodeBsdfTransparent")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs["Color"].default_value = col + (1,)
    em.inputs["Strength"].default_value = strength * GLOW
    mix.inputs["Fac"].default_value = alpha
    nt.links.new(tr.outputs[0], mix.inputs[1])
    nt.links.new(em.outputs[0], mix.inputs[2])
    nt.links.new(mix.outputs[0], o.inputs["Surface"])
    return m


def key_mat(m, frames_alpha=None, frames_strength=None):
    nt = m.node_tree
    mix = next(n for n in nt.nodes if n.type == "MIX_SHADER")
    em = next(n for n in nt.nodes if n.type == "EMISSION")
    for f, a in (frames_alpha or []):
        mix.inputs["Fac"].default_value = a
        mix.inputs["Fac"].keyframe_insert("default_value", frame=f)
    for f, s in (frames_strength or []):
        em.inputs["Strength"].default_value = s * GLOW
        em.inputs["Strength"].keyframe_insert("default_value", frame=f)


GROUPS = {}


def add(group, ob):
    GROUPS.setdefault(group, []).append(ob)
    return ob


def mesh_obj(name, verts, faces, mat, group, loc=(0, 0, 0)):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    scene.collection.objects.link(ob)
    me.materials.append(mat)
    return add(group, ob)


def annulus(name, r_in, r_out, mat, group, loc, a0=0.0, a1=2 * math.pi, seg=64, z=0.01):
    vs, fs = [], []
    for i in range(seg + 1):
        a = a0 + (a1 - a0) * i / seg
        vs += [(r_in * math.cos(a), r_in * math.sin(a), z), (r_out * math.cos(a), r_out * math.sin(a), z)]
        if i:
            k = 2 * i
            fs.append((k - 2, k - 1, k + 1, k))
    return mesh_obj(name, vs, fs, mat, group, loc)


def rect(name, w, h, mat, group, loc=(0, 0, 0), left_origin=False):
    x0 = 0 if left_origin else -w / 2
    vs = [(x0, -h / 2, 0), (x0 + w, -h / 2, 0), (x0 + w, h / 2, 0), (x0, h / 2, 0)]
    return mesh_obj(name, vs, [(0, 1, 2, 3)], mat, group, loc)


def billboard(ob, parent_loc):
    """Screen-facing: the object copies the camera's rotation (XY plane = screen)."""
    c = ob.constraints.new("COPY_ROTATION")
    c.target = cam
    ob.location = parent_loc
    return ob


def empty(name, group, loc):
    e = bpy.data.objects.new(name, None)
    e.location = loc
    scene.collection.objects.link(e)
    billboard(e, loc)
    return add(group, e)


def child(ob, parent):
    ob.parent = parent
    return ob


def text(name, body, size, mat, group, parent, loc=(0, 0, 0)):
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = body
    cu.size = size
    cu.align_x, cu.align_y = "CENTER", "CENTER"
    cu.extrude = 0.004
    ob = bpy.data.objects.new(name, cu)
    scene.collection.objects.link(ob)
    cu.materials.append(mat)
    ob.parent = parent
    ob.location = loc
    return add(group, ob)


def octa(name, r, mat, group, loc):
    vs = [(r, 0, 0), (-r, 0, 0), (0, r, 0), (0, -r, 0), (0, 0, 1.4 * r), (0, 0, -1.4 * r)]
    fs = [(0, 2, 4), (2, 1, 4), (1, 3, 4), (3, 0, 4), (2, 0, 5), (1, 2, 5), (3, 1, 5), (0, 3, 5)]
    return mesh_obj(name, vs, fs, mat, group, loc)


def brackets(name, w, h, arm, thick, mat, group, parent):
    """Four corner brackets of a w x h frame, in the parent's XY plane."""
    obs = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            cx, cy = sx * w / 2, sy * h / 2
            vs, fs = [], []
            for (x0, y0, x1, y1) in ((cx, cy, cx - sx * arm, cy + 0), (cx, cy, cx, cy - sy * arm)):
                ax, ay = (x1 - x0), (y1 - y0)
                ln = math.hypot(ax, ay) or 1
                nx, ny = -ay / ln * thick / 2, ax / ln * thick / 2
                b = len(vs)
                vs += [(x0 - nx, y0 - ny, 0), (x1 - nx, y1 - ny, 0), (x1 + nx, y1 + ny, 0), (x0 + nx, y0 + ny, 0)]
                fs.append((b, b + 1, b + 2, b + 3))
            obs.append(child(mesh_obj("%s_%d%d" % (name, sx, sy), vs, fs, mat, group), parent))
    return obs


def ground(ch):
    return Vector((ch.location.x, ch.location.y, 0))


def head(ch, up=2.25):
    return Vector((ch.location.x, ch.location.y, up))


# --- M1 selection ring + agent number ------------------------------------------------
s = spec["markers"]["selection"]
m_ring = emit_mat("sel_ring", C["friend"], 3.0, 0.85)
key_mat(m_ring, frames_strength=[(1, 2.5), (24, 4.0), (48, 2.5)])
ring = annulus("sel_ring", s["radius"] - 0.04, s["radius"], m_ring, "selection", ground(agent) + Vector((0, 0, 0.012)))
ticks = annulus("sel_ticks", s["radius"] + 0.06, s["radius"] + 0.16, m_ring, "selection", ground(agent) + Vector((0, 0, 0.012)),
                0, 2 * math.pi, 4)
for k in range(4):  # four short tick arcs
    a = k * math.pi / 2
    add("selection", annulus("sel_tick%d" % k, s["radius"] + 0.07, s["radius"] + 0.15, m_ring, "selection",
                             ground(agent) + Vector((0, 0, 0.012)), a - 0.12, a + 0.12, 6))
GROUPS["selection"].remove(ticks)
bpy.data.objects.remove(ticks, do_unlink=True)
for ob in [o for o in GROUPS["selection"] if o.name.startswith("sel_")]:
    ob.rotation_euler[2] = 0
    ob.keyframe_insert("rotation_euler", index=2, frame=1)
    ob.rotation_euler[2] = math.radians(90)
    ob.keyframe_insert("rotation_euler", index=2, frame=N + 1)
plate_e = empty("sel_plate", "selection", head(agent, 2.2))
m_plate = emit_mat("sel_plate", C["friend"], 1.2, 0.35)
add("selection", child(rect("sel_plate_bg", 0.34, 0.34, m_plate, "selection"), plate_e))
m_num = emit_mat("sel_num", (1, 1, 1), 4.0, 1.0)
text("sel_num", "1", 0.26, m_num, "selection", plate_e, (0, -0.01, 0.002))
m_plate_edge = emit_mat("sel_plate_edge", C["friend"], 4.0, 0.95)
brackets("sel_plate_br", 0.4, 0.4, 0.09, 0.025, m_plate_edge, "selection", plate_e)

# --- M2 hover brackets on the enemy ---------------------------------------------------
h = spec["markers"]["hover"]
hov = empty("hover", "hover", Vector((enemy.location.x, enemy.location.y, 0.95)))
m_hov = emit_mat("hover", C["neutral"], 3.0, 0.9)
brackets("hov", h["width"], h["height"], 0.18, 0.035, m_hov, "hover", hov)
for f, sc in ((1, 1.0), (12, 1.06), (24, 1.0), (36, 1.06), (48, 1.0)):
    hov.scale = (sc, sc, sc)
    hov.keyframe_insert("scale", frame=f)

# --- M3 target lock (shrinking brackets, red when locked) -----------------------------
lk = spec["markers"]["lock"]
lock = empty("lock", "lock", Vector((enemy.location.x, enemy.location.y, 0.95)))
m_lock = emit_mat("lock", C["acquiring"], 3.5, 0.9)
brackets("lock", 0.9, 2.0, 0.16, 0.035, m_lock, "lock", lock)
for f, sc in ((1, 2.0), (30, 1.0)):
    lock.scale = (sc, sc, sc)
    lock.keyframe_insert("scale", frame=f)
lock.rotation_euler = (0, 0, 0)
nt = m_lock.node_tree
em = next(n for n in nt.nodes if n.type == "EMISSION")
for f, col in ((1, C["acquiring"]), (29, C["acquiring"]), (31, C["hostile"]), (48, C["hostile"])):
    em.inputs["Color"].default_value = col + (1,)
    em.inputs["Color"].keyframe_insert("default_value", frame=f)
key_mat(m_lock, frames_strength=[(1, 2.5), (29, 3.0), (31, 9.0), (36, 4.0), (48, 4.0)])
m_dot = emit_mat("lock_dot", C["hostile"], 6.0, 0.0)
key_mat(m_dot, frames_alpha=[(1, 0.0), (30, 0.0), (31, 0.95), (48, 0.95)])
dot = add("lock", child(octa("lock_dot", 0.08, m_dot, "lock", (0, 0, 0)), lock))
dot.location = (0, 0, 0.002)
add("lock", dot)

# --- M4 health bar over the enemy (damage with a lagging trail) ----------------------
hb = spec["markers"]["health"]
hbe = empty("health", "health", head(enemy, 2.25))
m_bg = emit_mat("hb_bg", (0.05, 0.05, 0.07), 0.6, 0.75)
add("health", child(rect("hb_bg", hb["width"] + 0.04, hb["height"] + 0.04, m_bg, "health", (0, 0, -0.002)), hbe))
x0 = -hb["width"] / 2
m_lag = emit_mat("hb_lag", C["damage_trail"], 2.5, 0.95)
lag = add("health", child(rect("hb_lag", hb["width"], hb["height"], m_lag, "health", left_origin=True), hbe))
lag.location = (x0, 0, -0.001)
m_fill = emit_mat("hb_fill", C["health_ok"], 3.0, 0.95)
fill = add("health", child(rect("hb_fill", hb["width"], hb["height"], m_fill, "health", left_origin=True), hbe))
fill.location = (x0, 0, 0)
for f, v in ((1, 1.0), (10, 1.0), (12, 0.55), (48, 0.55)):
    fill.scale = (v, 1, 1)
    fill.keyframe_insert("scale", index=0, frame=f)
for f, v in ((1, 1.0), (16, 1.0), (34, 0.55), (48, 0.55)):
    lag.scale = (v, 1, 1)
    lag.keyframe_insert("scale", index=0, frame=f)
emf = next(n for n in m_fill.node_tree.nodes if n.type == "EMISSION")
for f, col in ((1, C["health_ok"]), (11, C["health_ok"]), (12, C["health_mid"]), (48, C["health_mid"])):
    emf.inputs["Color"].default_value = col + (1,)
    emf.inputs["Color"].keyframe_insert("default_value", frame=f)
for i in range(1, 5):  # segment ticks every 20 %
    t = add("health", child(rect("hb_tick%d" % i, 0.012, hb["height"], m_bg, "health"), hbe))
    t.location = (x0 + hb["width"] * i / 5, 0, 0.001)

# --- M5 weapon charge ring around the agent (fills, then flashes) ---------------------
ch = spec["markers"]["charge"]
segs = 32
m_charge_bg = emit_mat("charge_bg", C["friend"], 0.8, 0.25)
annulus("charge_bg", ch["radius"] - 0.05, ch["radius"], m_charge_bg, "charge", ground(agent) + Vector((0, 0, 0.9)))
for i in range(segs):
    m = emit_mat("charge_%d" % i, C["charge"], 4.0, 0.0)
    a0 = math.pi / 2 - 2 * math.pi * (i + 1) / segs
    annulus("charge_%d" % i, ch["radius"] - 0.05, ch["radius"], m, "charge", ground(agent) + Vector((0, 0, 0.9)),
            a0 + 0.02, a0 + 2 * math.pi / segs - 0.02, 3)
    on = 1 + int(i * 36 / segs)
    key_mat(m, frames_alpha=[(1, 0.0), (max(1, on - 1), 0.0), (on, 0.95), (48, 0.95)],
            frames_strength=[(1, 4.0), (37, 4.0), (39, 10.0), (44, 4.0)])

# --- M6 persuaded follower: diamond above the head, tether to the persuader ----------
pe = spec["markers"]["persuaded"]
m_pers = emit_mat("pers", C["persuaded"], 4.0, 0.9)
dia = octa("pers_dia", 0.1, m_pers, "persuaded", head(civ, 2.2))
dia.rotation_euler[2] = 0
dia.keyframe_insert("rotation_euler", index=2, frame=1)
dia.rotation_euler[2] = math.radians(360)
dia.keyframe_insert("rotation_euler", index=2, frame=N + 1)
for f, z in ((1, 2.2), (24, 2.28), (48, 2.2)):
    dia.location.z = z
    dia.keyframe_insert("location", index=2, frame=f)
a, b = head(civ, 1.2), head(agent, 1.2)
m_teth = emit_mat("pers_tether", C["persuaded"], 2.0, 0.0)
nseg = 14
for i in range(nseg):  # dashed tether, dashes flowing towards the persuader
    p0 = a.lerp(b, (i + 0.15) / nseg)
    p1 = a.lerp(b, (i + 0.65) / nseg)
    d = (p1 - p0)
    m = emit_mat("teth_%d" % i, C["persuaded"], 2.5, 0.0)
    key_mat(m, frames_alpha=[(f, 0.15 + 0.6 * max(0, math.cos(2 * math.pi * (f / 24.0 - i / nseg)))) for f in range(1, 49, 4)])
    w = 0.02
    vs = [(p0.x, p0.y - w, p0.z), (p1.x, p1.y - w, p1.z), (p1.x, p1.y + w, p1.z), (p0.x, p0.y + w, p0.z)]
    mesh_obj("teth_%d" % i, vs, [(0, 1, 2, 3)], m, "persuaded")
annulus("pers_ring", 0.32, 0.35, m_pers, "persuaded", ground(civ) + Vector((0, 0, 0.012)))

# --- M7 move-to waypoint and path -------------------------------------------------------
wp = spec["markers"]["waypoint"]
dest = Vector((1.5, -1.1, 0.012))
for k in range(3):  # three rings pulsing outwards
    m = emit_mat("wp_ring%d" % k, C["friend"], 3.0, 0.0)
    r = annulus("wp_ring%d" % k, 0.05, 0.08, m, "waypoint", dest)
    for f in range(1, 50, 2):
        ph = ((f - 1) / 48.0 + k / 3.0) % 1.0
        sc = 1 + 5 * ph
        r.scale = (sc, sc, 1)
        r.keyframe_insert("scale", frame=f)
    key_mat(m, frames_alpha=[(f, 0.9 * (1 - (((f - 1) / 48.0 + k / 3.0) % 1.0))) for f in range(1, 50, 2)])
m_wp = emit_mat("wp_core", C["friend"], 4.0, 0.9)
annulus("wp_core", 0.0, 0.07, m_wp, "waypoint", dest)
start = ground(agent)
nch = 7
for i in range(nch):  # chevrons along the path, lighting up in sequence
    p = start.lerp(dest, (i + 1) / (nch + 1))
    d = (dest - start).normalized()
    sd = Vector((-d.y, d.x, 0))
    tip = p + d * 0.1
    vs = [tuple(p - d * 0.05 + sd * 0.12), tuple(tip), tuple(p - d * 0.05 - sd * 0.12), tuple(p - d * 0.12)]
    vs = [(v[0], v[1], 0.013) for v in vs]
    m = emit_mat("wp_ch%d" % i, C["friend"], 3.0, 0.0)
    key_mat(m, frames_alpha=[(f, 0.2 + 0.7 * max(0, math.cos(2 * math.pi * (f / 24.0 - i / nch)))) for f in range(1, 49, 3)])
    mesh_obj("wp_ch%d" % i, vs, [(0, 1, 2, 3)], m, "waypoint")

# --- M8 floating damage number ------------------------------------------------------------
dm = empty("damage", "damage", head(enemy, 2.0))
m_dmg = emit_mat("dmg", C["hostile"], 5.0, 0.0)
text("dmg_txt", "-25", 0.3, m_dmg, "damage", dm)
for f, z, a in ((1, 2.0, 0.0), (4, 2.1, 1.0), (30, 2.6, 0.9), (44, 2.8, 0.0), (48, 2.8, 0.0)):
    dm.location.z = z
    dm.keyframe_insert("location", index=2, frame=f)
key_mat(m_dmg, frames_alpha=[(1, 0.0), (4, 1.0), (30, 0.9), (44, 0.0), (48, 0.0)])

# --- M9 objective beacon -------------------------------------------------------------------
ob_ = spec["markers"]["objective"]
pos = Vector((-1.0, 2.9, 0))
m_col = bpy.data.materials.new("obj_column")
m_col.use_nodes = True
for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
    if hasattr(m_col, attr):
        try:
            setattr(m_col, attr, val)
        except Exception:
            pass
nt = m_col.node_tree
nt.nodes.clear()
o = nt.nodes.new("ShaderNodeOutputMaterial")
mix = nt.nodes.new("ShaderNodeMixShader")
tr = nt.nodes.new("ShaderNodeBsdfTransparent")
emn = nt.nodes.new("ShaderNodeEmission")
emn.inputs["Color"].default_value = C["objective"] + (1,)
emn.inputs["Strength"].default_value = 1.2
tc = nt.nodes.new("ShaderNodeTexCoord")
sep = nt.nodes.new("ShaderNodeSeparateXYZ")
ramp = nt.nodes.new("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].color = (0.55, 0.55, 0.55, 1)
ramp.color_ramp.elements[1].color = (0, 0, 0, 1)
nt.links.new(tc.outputs["Generated"], sep.inputs[0])
nt.links.new(sep.outputs["Z"], ramp.inputs["Fac"])
nt.links.new(ramp.outputs["Color"], mix.inputs["Fac"])
nt.links.new(tr.outputs[0], mix.inputs[1])
nt.links.new(emn.outputs[0], mix.inputs[2])
nt.links.new(mix.outputs[0], o.inputs["Surface"])
bpy.ops.mesh.primitive_cylinder_add(radius=0.07, depth=ob_["height"], location=(pos.x, pos.y, ob_["height"] / 2))
colm = bpy.context.active_object
colm.data.materials.append(m_col)
add("objective", colm)
m_od = emit_mat("obj_dia", C["objective"], 5.0, 0.95)
od = octa("obj_dia", 0.22, m_od, "objective", (pos.x, pos.y, 2.4))
od.rotation_euler[2] = 0
od.keyframe_insert("rotation_euler", index=2, frame=1)
od.rotation_euler[2] = math.radians(180)
od.keyframe_insert("rotation_euler", index=2, frame=N + 1)
for f, z in ((1, 2.4), (24, 2.55), (48, 2.4)):
    od.location.z = z
    od.keyframe_insert("location", index=2, frame=f)
annulus("obj_ring", 0.4, 0.45, m_od, "objective", Vector((pos.x, pos.y, 0.012)))

# linear keys, so loops do not ease in and out
for act in bpy.data.actions:
    try:
        fcs = list(act.fcurves)
    except Exception:
        fcs = [fc for layer in getattr(act, "layers", []) for strip in layer.strips
               for bag in getattr(strip, "channelbags", []) for fc in bag.fcurves]
    for fc in fcs:
        for kp in fc.keyframe_points:
            if not fc.data_path.startswith("pose."):
                kp.interpolation = "LINEAR"

# --- render each marker ----------------------------------------------------------------------
SHOW = {"selection": ["agent"], "hover": ["agent", "enemy"], "lock": ["agent", "enemy"], "health": ["agent", "enemy"],
        "charge": ["agent"], "persuaded": ["agent", "civ"], "waypoint": ["agent"], "damage": ["agent", "enemy"],
        "objective": ["agent"], "all": ["agent", "enemy", "civ"]}
todo = only or list(SHOW)
for name in todo:
    vis = set(GROUPS) if name == "all" else {name}
    if name == "all":
        vis.discard("lock")  # hover and lock are the same place
    for g, obs in GROUPS.items():
        for ob in obs:
            ob.hide_render = g not in vis
    for c, obs in CHARS.items():
        for ob in obs:
            if ob.type == "MESH" and ob.parent is not None and not ob.name.startswith("weapon_"):
                ob.hide_render = c not in SHOW[name]
    d = os.path.join(outdir, name)
    os.makedirs(d, exist_ok=True)
    n = 0
    for f in range(1, N + 1, 2):
        scene.frame_set(f)
        scene.render.filepath = os.path.join(d, "f%03d_0.png" % n)
        bpy.ops.render.render(write_still=True)
        n += 1
    json.dump({"clip": name, "weapon": None, "frames": n, "fps": 12, "views_labels": ["three-quarter view"]},
              open(os.path.join(d, "info.json"), "w"))
    print("marker done", name, n)
