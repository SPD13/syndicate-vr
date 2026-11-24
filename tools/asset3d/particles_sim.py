"""Reference simulator of the particle effects in effects.json, and their test page.

python particles_sim.py [effect ...]
Simulates each effect exactly as the engine should (spawn, forces, curves,
ground, on_death sub-effects, trails, light, ground ring) at 30 steps per
second, renders it from the game's three-quarter view (yaw 45, pitch 30,
orthographic) next to a 1.8 m reference figure, and writes animated PNGs and
review/effects/index.html. The C implementation must reproduce this
behaviour; the rendering here (soft sprites, additive / alpha blending,
filmic tone map) is the look to match with GL billboards.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import swdata  # noqa: E402

FPS = 30
DT = 1.0 / FPS
W, H = 520, 440
YAW, PITCH = math.radians(45), math.radians(30)
RNG = np.random.default_rng(7)
STATS = {}  # peak live particles per effect


# --- curves -------------------------------------------------------------------
def curve(points, t):
    """Piecewise linear curve [[t, value], ...]; value may be a list."""
    if t <= points[0][0]:
        return np.array(points[0][1], dtype=float)
    for (t0, v0), (t1, v1) in zip(points, points[1:]):
        if t <= t1:
            a = (t - t0) / max(1e-9, t1 - t0)
            return np.array(v0, dtype=float) * (1 - a) + np.array(v1, dtype=float) * a
    return np.array(points[-1][1], dtype=float)


def rand_dir(d, spread_deg):
    d = np.array(d, dtype=float)
    d /= np.linalg.norm(d) + 1e-9
    if spread_deg <= 0:
        return d
    cos_max = math.cos(math.radians(min(180, spread_deg)))
    z = RNG.uniform(cos_max, 1.0)
    phi = RNG.uniform(0, 2 * math.pi)
    r = math.sqrt(max(0, 1 - z * z))
    local = np.array([r * math.cos(phi), z, r * math.sin(phi)])
    up = np.array([0, 1.0, 0])
    if abs(d @ up) > 0.999:
        return local * (1 if d[1] > 0 else -1)
    a = np.cross(up, d)
    a /= np.linalg.norm(a)
    ang = math.acos(np.clip(up @ d, -1, 1))
    k = a
    return (local * math.cos(ang) + np.cross(k, local) * math.sin(ang) + k * (k @ local) * (1 - math.cos(ang)))


def spawn_pos(sp):
    if "sphere" in sp:
        v = RNG.normal(size=3)
        v /= np.linalg.norm(v) + 1e-9
        return v * sp["sphere"] * RNG.uniform(0, 1) ** (1 / 3)
    if "disc" in sp:
        a, r = RNG.uniform(0, 2 * math.pi), sp["disc"] * math.sqrt(RNG.uniform(0, 1))
        return np.array([r * math.cos(a), 0, r * math.sin(a)])
    if "box" in sp:
        return (RNG.uniform(-0.5, 0.5, 3)) * np.array(sp["box"])
    return np.zeros(3)


# --- simulation ------------------------------------------------------------------
class Particle:
    __slots__ = ("em", "pos", "vel", "age", "life", "jit", "seed", "prev", "trail_acc")

    def __init__(self, em, pos, vel, life):
        self.em, self.pos, self.vel, self.age, self.life = em, pos, vel, 0.0, life
        self.jit = 1 + RNG.uniform(-1, 1) * em.get("size_jitter", 0)
        self.seed = RNG.uniform(0, 100)
        self.prev = pos.copy()
        self.trail_acc = 0.0


class Instance:
    """A running effect at a position (effects spawn more instances on death)."""

    def __init__(self, fx, name, origin, t0):
        self.fx, self.name, self.d, self.origin, self.t0 = fx, name, fx[name], np.array(origin, float), t0
        self.acc = {i: 0.0 for i in range(len(self.d["emitters"]))}
        self.burst_done = set()


def turbulence(p, t):
    s = p.seed
    return np.array([math.sin(1.7 * t + s) + 0.5 * math.sin(3.1 * t + 2 * s),
                     0.4 * math.sin(2.3 * t + 3 * s),
                     math.cos(1.9 * t + 4 * s) + 0.5 * math.cos(2.9 * t + s)])


def simulate(fx, name):
    d = fx[name]
    dur = d.get("duration", 3.0)
    warm = 2.0 if d.get("loop") else 0.0
    total = dur + (0 if d.get("loop") else 1.0)
    insts = [Instance(fx, name, (0, 0, 0), -warm)]
    parts = []
    frames = []
    t = -warm
    while t < total:
        new_insts = []
        for ins in insts:
            lt = t - ins.t0
            idur = ins.d.get("duration", 1e9)
            for i, em in enumerate(ins.d["emitters"]):
                if lt < em.get("delay", 0):
                    continue
                if i not in ins.burst_done and em.get("burst"):
                    for _ in range(em["burst"]):
                        parts.append(make(em, ins.origin))
                    ins.burst_done.add(i)
                rate = em.get("rate", 0)
                edur = em.get("duration", idur if not ins.d.get("loop") else 1e9)
                if rate and lt < em.get("delay", 0) + edur:
                    fade = 1.0
                    if em.get("fade_out") and lt > edur - em["fade_out"]:
                        fade = max(0, (edur - lt) / em["fade_out"])
                    ins.acc[i] += rate * DT * fade
                    while ins.acc[i] >= 1:
                        ins.acc[i] -= 1
                        parts.append(make(em, ins.origin))
        alive = []
        for p in parts:
            em = p.em
            p.prev = p.pos.copy()
            p.age += DT
            if p.age >= p.life:
                if em.get("on_death"):
                    new_insts.append(Instance(fx, em["on_death"], (p.pos[0], 0, p.pos[2]), t))
                continue
            acc = np.array([0, -em.get("gravity", 0) + em.get("buoyancy", 0), 0])
            p.vel = p.vel + acc * DT
            p.vel = p.vel * math.exp(-em.get("drag", 0) * DT)
            move = p.vel * DT
            if em.get("turbulence"):
                move = move + turbulence(p, t) * em["turbulence"] * DT
            p.pos = p.pos + move
            if p.pos[1] < 0 and em.get("ground"):
                if em["ground"] == "die":
                    if em.get("on_death"):
                        new_insts.append(Instance(fx, em["on_death"], (p.pos[0], 0, p.pos[2]), t))
                    continue
                p.pos[1] = 0
                p.vel = np.array([p.vel[0] * 0.6, -p.vel[1] * 0.35, p.vel[2] * 0.6])
            if em.get("trail"):
                tr = fx[em["trail"]]
                for tem in tr["emitters"]:
                    p.trail_acc += tem.get("rate", 0) * DT
                    while p.trail_acc >= 1:
                        p.trail_acc -= 1
                        alive.append(make(tem, p.pos))
            alive.append(p)
        parts = alive
        insts += new_insts
        STATS[name] = max(STATS.get(name, 0), len(parts))
        if t >= 0:
            frames.append(render(parts, insts, t, d))
        t += DT
    return frames


def make(em, origin):
    pos = np.array(origin, float) + np.array(em.get("offset", [0, 0, 0]), float) + spawn_pos(em.get("spawn", {}))
    v = em.get("vel", {})
    speed = RNG.uniform(*v.get("speed", [0, 0]))
    vel = rand_dir(v.get("dir", [0, 1, 0]), v.get("spread", 180 if "dir" not in v else 0)) * speed
    return Particle(em, pos, vel, RNG.uniform(*em["life"]))


# --- rendering ---------------------------------------------------------------------
SCALE = 30.0  # px per metre, set per effect
CX, CY = W * 0.5, H * 0.72


def proj(p):
    x, y, z = p
    xr = x * math.cos(YAW) - z * math.sin(YAW)
    zr = x * math.sin(YAW) + z * math.cos(YAW)
    sx = CX + xr * SCALE
    sy = CY - (y * math.cos(PITCH) - zr * math.sin(PITCH)) * SCALE
    depth = zr * math.cos(PITCH) + y * math.sin(PITCH)  # bigger = nearer the camera
    return sx, sy, depth


_kern = {}


def kernel(shape, px, seed=0):
    px = max(2, int(px))
    key = (shape, px, int(seed) % 6 if shape == "puff" else 0)
    if key in _kern:
        return _kern[key]
    r = np.linspace(-1, 1, px)
    xx, yy = np.meshgrid(r, r)
    d2 = xx ** 2 + yy ** 2
    if shape == "chunk":
        k = (d2 < 0.55).astype(float)
    else:
        k = np.exp(-d2 * 3.2)
        k[d2 > 1] = 0
    if shape == "puff":
        rng = np.random.default_rng(key[2] + 11)
        n = rng.uniform(0, 1, (max(4, px // 4), max(4, px // 4)))
        n = np.array(Image.fromarray((n * 255).astype(np.uint8)).resize((px, px), Image.BICUBIC)) / 255.0
        k = k * (0.45 + 0.75 * n)
        k = np.clip(k, 0, 1)
    _kern[key] = k
    return k


def splat(buf, k, sx, sy, col, mode):
    h, w = k.shape
    x0, y0 = int(sx - w / 2), int(sy - h / 2)
    x1, y1 = x0 + w, y0 + h
    if x1 <= 0 or y1 <= 0 or x0 >= W or y0 >= H:
        return
    kx0, ky0 = max(0, -x0), max(0, -y0)
    kx1, ky1 = w - max(0, x1 - W), h - max(0, y1 - H)
    sub = buf[max(0, y0):min(H, y1), max(0, x0):min(W, x1)]
    kk = k[ky0:ky1, kx0:kx1][..., None]
    if mode == "add":
        sub += kk * col[:3] * col[3]
    else:
        a = kk * col[3]
        sub *= (1 - a)
        sub += a * col[:3]


def background():
    img = Image.new("RGB", (W, H), (14, 15, 18))
    dr = ImageDraw.Draw(img)
    for i in range(-12, 13):
        for a, b in (((i, 0, -12), (i, 0, 12)), ((-12, 0, i), (12, 0, i))):
            p0, p1 = proj(a), proj(b)
            dr.line([p0[:2], p1[:2]], fill=(32, 34, 40))
    return np.array(img, dtype=float) / 255.0


FIG = np.array([1.6, 0, 1.0])  # where the reference figure stands (set per effect)
BARREL = False


def figure(buf, light):
    """Plain 1.8 m reference figure (a capsule), lit by the effect's light."""
    if BARREL:  # a plain barrel under the barrel fire
        for y in np.linspace(0.05, 0.9, 10):
            sx, sy, _ = proj([0, y, 0])
            splat(buf, kernel("chunk", 0.62 * SCALE), sx, sy, np.array([0.35, 0.1, 0.07, 1.0]), "alpha")
    base = FIG
    col = np.array([0.32, 0.33, 0.36]) * (0.6 + 0.1 * light) + np.array([0.25, 0.12, 0.04]) * min(1.5, light * 0.15)
    for y in np.linspace(0.15, 1.65, 12):
        sx, sy, _ = proj(base + [0, y, 0])
        r = (0.22 if y < 1.45 else 0.14) * SCALE
        splat(buf, kernel("chunk", 2 * r), sx, sy, np.append(np.clip(col, 0, 1), 1.0), "alpha")


def render(parts, insts, t, d):
    buf = background()
    # light: glow on the ground + figure lighting
    light = 0.0
    for ins in insts:
        L = ins.d.get("light")
        if not L:
            continue
        lt = t - ins.t0
        inten = float(curve(L["intensity"], lt))
        if L.get("flicker"):
            inten *= 1 + L["flicker"] * (math.sin(lt * 23.0) * 0.6 + math.sin(lt * 37.0 + 1.3) * 0.4)
        if inten <= 0:
            continue
        light += inten / max(1, np.linalg.norm(ins.origin[[0, 2]] - FIG[[0, 2]]))
        sx, sy, _ = proj(ins.origin)
        rx = min(L["radius"] * SCALE, W * 0.45)
        k = kernel("soft", 2 * rx)
        k2 = np.array(Image.fromarray((k * 255).astype(np.uint8)).resize((int(2 * rx), max(2, int(2 * rx * math.sin(PITCH)))))) / 255.0
        splat(buf, k2, sx, sy, np.array(L["color"] + [min(0.45, inten * 0.03)]), "add")
        R = ins.d.get("ring")
        if R and lt < R["life"]:
            rr = float(curve(R["radius"], lt / R["life"])) * SCALE
            a = R["color"][3] * (1 - lt / R["life"])
            ring = Image.new("L", (int(2 * rr) + 8, int(2 * rr * math.sin(PITCH)) + 8), 0)
            ImageDraw.Draw(ring).ellipse([4, 4, 4 + 2 * rr, 4 + 2 * rr * math.sin(PITCH)], outline=255, width=max(2, int(rr * 0.08)))
            ring = np.array(ring.filter(ImageFilter.GaussianBlur(2)), dtype=float) / 255.0
            splat(buf, ring, sx, sy, np.array(R["color"][:3] + [a]), "add")
    figure(buf, light)
    items = []
    for p in parts:
        em = p.em
        u = p.age / p.life
        size = float(curve(em["size"], u)) * p.jit
        col = curve(em["color"], u)
        if em["blend"] == "add":
            col = col.copy()
            col[:3] = col[:3] * em.get("intensity", 1.0)
        sx, sy, dep = proj(p.pos)
        items.append((dep, em, p, size, col, sx, sy))
    items.sort(key=lambda it: it[0])  # far first
    for dep, em, p, size, col, sx, sy in items:
        if em["blend"] == "alpha":
            splat(buf, kernel(em["shape"], size * SCALE, p.seed), sx, sy, col, "alpha")
    for dep, em, p, size, col, sx, sy in items:
        if em["blend"] != "add":
            continue
        if em["shape"] == "streak":
            px0, py0, _ = proj(p.prev)
            n = 4
            for i in range(n):
                f = i / (n - 1)
                splat(buf, kernel("soft", max(3, size * SCALE * 3)), sx + (px0 - sx) * f * 3, sy + (py0 - sy) * f * 3,
                      col * np.array([1, 1, 1, 1 - 0.7 * f]), "add")
        else:
            splat(buf, kernel(em["shape"], size * SCALE, p.seed), sx, sy, col, "add")
    img = 1 - np.exp(-buf * 1.6)  # filmic-ish tone map: bright cores stay bright, no hard clip
    img = np.clip(img ** (1 / 1.1), 0, 1)
    out = Image.fromarray((img * 255).astype(np.uint8))
    ImageDraw.Draw(out).text((8, H - 16), "t = %.2f s  (game turn %d)" % (t, int(t * 20)), fill=(160, 160, 170))
    return out


def main():
    global SCALE, CY
    fx = json.load(open(os.path.join(HERE, "effects.json"), encoding="utf-8"))
    names = sys.argv[1:] or [k for k in fx if not k.startswith("_") and k not in ("explosion_secondary", "trail_smoke")]
    out = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d", "review", "effects")
    os.makedirs(out, exist_ok=True)
    cards = []
    for name in names:
        SCALE = {"explosion_large": 26.0, "fire_ground": 64.0, "burning_person": 80.0, "barrel_fire": 110.0}.get(name, 62.0)
        CY = H * {"explosion_large": 0.8, "burning_person": 0.86, "barrel_fire": 0.86}.get(name, 0.76)
        global FIG, BARREL
        FIG = np.array({"burning_person": [0, 0, 0], "fire_ground": [2.5, 0, 1.2],
                        "explosion_large": [5.0, 0, 3.0]}.get(name, [1.6, 0, 1.0]), dtype=float)
        BARREL = name == "barrel_fire"
        _kern.clear()
        frames = simulate(fx, name)
        p = os.path.join(out, name + ".png")
        frames[0].save(p, save_all=True, append_images=frames[1:], loop=0, duration=int(1000 / FPS))
        strip = Image.new("RGB", (W * 6 // 2, H // 2))
        moments = [0.05, 0.15, 0.35, 0.7, 1.4, 2.6] if name.startswith("explosion") else [0.3, 1.0, 2.0, 3.0, 4.5, 6.5]
        for i, k in enumerate([min(len(frames) - 1, int(m * FPS)) for m in moments]):
            strip.paste(frames[k].resize((W // 2, H // 2)), (i * W // 2, 0))
        strip.save(os.path.join(out, name + "_strip.jpg"), quality=88)
        cards.append((name, fx[name].get("game", ""), len(frames) / FPS, [e["name"] for e in fx[name]["emitters"]]))
        print("effect", name, len(frames), "frames")
    html = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>Particle effects</title><style>"
            "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}.c{background:#2a2b31;border-radius:8px;"
            "padding:14px;margin:0 0 22px}h2{margin:0 0 6px}.m{color:#aaa;font-size:13px}img{display:block;margin-top:8px;border-radius:4px}"
            "</style></head><body><h1>Particle effects - simulation</h1><p>Fires and explosions as the engine should run them: "
            "definitions in <code>tools/asset3d/effects.json</code>, simulated by <code>particles_sim.py</code> (30 steps/s; "
            "1 game turn = 1/20 s). Three-quarter game view, grid = 1 m, the grey figure is 1.8 m tall. Strips below each "
            "animation show six moments.</p>"]
    for name, game, secs, ems in cards:
        html.append("<div class=c><h2>%s</h2><div class=m>%s<br>%.1f s, emitters: %s</div><img src='%s.png'>"
                    "<img src='%s_strip.jpg' width=780></div>" % (name, game, secs, ", ".join(ems), name, name))
    html.append("</body></html>")
    if len(sys.argv) == 1:
        open(os.path.join(out, "index.html"), "w", encoding="utf-8").write("\n".join(html))
        json.dump({"peak_live_particles": STATS, "fps": FPS}, open(os.path.join(out, "stats.json"), "w"), indent=1)
        print("peak particles:", STATS)
    print("page", os.path.join(out, "index.html"))


if __name__ == "__main__":
    main()
