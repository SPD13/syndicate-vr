"""Inventory of the game's HUD and menu art, as a private review page.

python inventory_ui.py
Decodes the 2D UI files of the game data (sprite banks: panel, pop, wepicon,
pointr, mouse, fontc; equip animations mod-NN / netNN; menu backgrounds
qdata/proj-*.raw) into contact sheets, and writes
assets3d/review/ui_inventory/index.html with, per group, what it is and how
it should be upgraded for VR. Nothing is sent to any generator.
"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

import flic  # noqa: E402
import swdata  # noqa: E402

G = swdata.find_gamedata()
OUT = os.path.join(os.path.dirname(G), "assets3d", "review", "ui_inventory")
os.makedirs(OUT, exist_ok=True)
PAL = swdata.load_palette(G)


def load_pal(path):
    raw = open(path, "rb").read()[:768]
    return np.frombuffer(raw, dtype=np.uint8).reshape(256, 3).astype(np.uint16) * 4


def bank(stem):
    """All sprites of data/<stem>.tab/.dat as (index, rgba image)."""
    tab = open(os.path.join(G, "data", stem + ".tab"), "rb").read()
    dat = open(os.path.join(G, "data", stem + ".dat"), "rb").read()
    out = []
    for i in range(len(tab) // 6):
        off, w, h = struct.unpack_from("<IBB", tab, i * 6)
        if w == 0 or h == 0 or off >= len(dat):
            continue
        img = np.full((h, w), -1, dtype=np.int16)
        p = off
        try:
            for y in range(h):
                x = 0
                while True:
                    c = struct.unpack_from("b", dat, p)[0]
                    p += 1
                    if c == 0:
                        break
                    if c < 0:
                        x -= c
                        continue
                    n = max(0, min(c, w - x))
                    if n:
                        img[y, x:x + n] = np.frombuffer(dat[p:p + n], dtype=np.uint8)
                    p += c
                    x += c
        except struct.error:
            continue
        out.append((i, swdata.indices_to_rgba(img, PAL)))
    return out


def sheet(items, name, scale=2, cols=16, cell=None, bg=(46, 48, 54)):
    if not items:
        return None
    mw = max(im.width for _, im in items)
    mh = max(im.height for _, im in items)
    cw = cell or min(160, max(24, mw * scale + 8))
    chh = min(160, max(24, mh * scale + 14))
    rows = (len(items) + cols - 1) // cols
    o = Image.new("RGB", (cols * cw, rows * chh), bg)
    d = ImageDraw.Draw(o)
    for k, (i, im) in enumerate(items):
        x, y = (k % cols) * cw, (k // cols) * chh
        s = scale
        while (im.width * s > cw - 4 or im.height * s > chh - 14) and s > 1:
            s -= 1
        t = im.resize((im.width * s, im.height * s), Image.NEAREST)
        if t.width > cw - 4 or t.height > chh - 14:
            t.thumbnail((cw - 4, chh - 14))
        o.paste(t, (x + 2, y + 12), t)
        d.text((x + 2, y), str(i), fill=(150, 150, 160))
    o.save(os.path.join(OUT, name + ".png"))
    return name + ".png"


def biggest(pattern):
    fs = sorted(glob.glob(os.path.join(G, "data", pattern + ".dat")), key=os.path.getsize)
    return os.path.splitext(os.path.basename(fs[-1]))[0] if fs else None


groups = []
# 1. in-mission HUD panels
for stem_pat, title, what, plan, scale in (
    ("panel?-*", "In-mission HUD panels (panel*.dat)",
     "Agent portraits and status frames, weapon slots, mood / drug (IPA) bars, mini-map frame, buttons of the in-mission screen.",
     "Not 3D models. Rebuild as a VR wrist / belt panel: vector UI (SVG / SDF shapes) at high resolution in the same style (cyan HUD lines). Agent portraits: renders of our 3D characters. Weapon slot icons: renders of the 3D weapons.", 2),
    ("pop?-*", "Pop-up and menu sprites (pop*.dat)",
     "Buttons, frames, icons and small graphics of the between-mission menus (equipment, research, cryo, world map, briefing).",
     "Redraw as vector UI on floating VR panels; keep the layout and colours. Small icons can be redrawn by an image model at 4-8x, but vector is sharper in VR.", 2),
    ("wepicon?-*", "Weapon icons (wepicon*.dat)",
     "Small icons of every weapon for the inventory and slots.",
     "Replace with renders of the 16 held weapon models (and the item models) on a transparent background: free, consistent with the 3D world.", 3),
    ("pointr?-*", "Pointers (pointr*.dat)",
     "The in-game mouse pointers: aim, go-to, pick-up, enter vehicle...",
     "In VR the pointer is the controller ray: replace by ray end markers (small 3D rings / icons, like the world markers M2-M3).", 3),
    ("mouse?-*", "Mouse cursors (mouse*.dat)",
     "Menu mouse cursors.",
     "Not needed in VR (laser pointer on panels); keep for the desktop front end.", 3),
):
    stem = biggest(stem_pat)
    if not stem:
        continue
    items = bank(stem)
    img = sheet(items, stem, scale=scale)
    groups.append((title, what, plan, [(img, "%s: %d sprites" % (stem, len(items)))]))

# 2. fonts
fonts = []
for f in sorted(glob.glob(os.path.join(G, "data", "fontc*-*.dat")), key=os.path.getsize)[-4:]:
    stem = os.path.splitext(os.path.basename(f))[0]
    items = bank(stem)[:96]
    fonts.append((sheet(items, stem, scale=3, cols=24), "%s: first 96 glyphs" % stem))
groups.append(("Fonts (fontc*.dat, 16 files)",
               "Bitmap fonts of the menus and HUD in several sizes and languages.",
               "Replace by one signed-distance-field font (an open font close in style, e.g. a squared techno face) rendered from vector: sharp at any size in VR. Keep the bitmap fonts for the desktop front end.",
               fonts))

# 3. equipment animations (mods, research)
eq = []
for pat, lab in (("mod-*.fli", "cyborg mods"), ("net*.fli", "research / network")):
    fs = sorted(glob.glob(os.path.join(G, "data", "equip", pat)))
    frames = []
    for f in fs:
        try:
            fr = flic.decode(f, os.path.join(G, "data", "s-proj.pal"))
        except Exception:
            continue
        im = fr[-1].convert("RGBA")
        bb = im.convert("L").point(lambda v: 255 if v > 8 else 0).getbbox()
        if bb:
            im = im.crop(bb)
        im.thumbnail((150, 150))
        frames.append((os.path.basename(f)[:-4], im))
    if frames:
        cols = 6
        o = Image.new("RGB", (cols * 160, ((len(frames) + cols - 1) // cols) * 176), (10, 10, 12))
        d = ImageDraw.Draw(o)
        for k, (n, im) in enumerate(frames):
            x, y = (k % cols) * 160, (k // cols) * 176
            o.paste(im, (x + 5, y + 20), im)
            d.text((x + 5, y + 4), n, fill=(200, 200, 120))
        name = "equip_" + pat.split("*")[0].strip("-")
        o.save(os.path.join(OUT, name + ".png"))
        eq.append((name + ".png", "%d %s animations (last frame)" % (len(frames), lab)))
groups.append(("Equipment shop and research renders (data/equip/*.fli)",
               "Rotating 1996 renders shown in the equipment shop: 31 weapons (wep-NN, already rebuilt in 3D), 12 cybernetic mods (mod-NN: limbs, chest, brain, eyes...), 18 research / network animations (netNN).",
               "Weapons: show our 3D weapon models turning on a VR display stand. Mods: 12 new 3D models (cyber arm, legs, chest, brain, eyes...) through the same redraw -> Tripo route as the weapons, about 50 credits each, or flat high-res redraws (5 credits each) on the panels. Research animations: redraws or new short loops.",
               eq))

# 4. menu backgrounds
bgs = []
for f in sorted(glob.glob(os.path.join(G, "qdata", "proj-*.raw"))):
    raw = open(f, "rb").read()
    w = 512
    h = len(raw) // w
    pal = load_pal(os.path.join(G, "data", "s-proj.pal"))
    arr = pal[np.frombuffer(raw[:w * h], dtype=np.uint8).reshape(h, w)].astype(np.uint8)
    im = Image.fromarray(arr, "RGB")
    im.thumbnail((256, 192))
    n = os.path.basename(f)[:-4]
    im.save(os.path.join(OUT, n + ".png"))
    bgs.append((n + ".png", n))
groups.append(("Menu backgrounds (qdata/proj-*.raw)",
               "Full-screen backgrounds of the between-mission menus, one set per faction (s = Syndicate / EuroCorp, z = Church of the New Epoch). Shown here with the menu palette; colours may be off where the game swaps palettes.",
               "In VR the menus float in a room: replace the flat backgrounds by a 3D briefing room per faction (an environment model + lighting), or 360 / skybox images generated at high resolution in the same style.",
               bgs))

# 5. videos and other
groups.append(("Videos and the world map (not decoded here)",
               "Intro and outro films (*.smk, outro-*.raw, flyby / demo-*.fli), mission go / fail clips (syn_go, syn_fail, zel-go...), the world map (cities.dat + menu sprites).",
               "Films: play on a large screen in the VR room (keep, maybe upscale with a video model). World map: a 3D globe with city markers (the world markers style) instead of the flat map.",
               []))

h = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>HUD and menu assets - inventory</title><style>"
     "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}.g{background:#2a2b31;border-radius:8px;"
     "padding:14px;margin:0 0 24px}h2{margin:0 0 6px;color:#ffcc33}.w{margin:4px 0}.p{margin:8px 0;color:#9fe}"
     "img{max-width:100%;image-rendering:pixelated;border-radius:4px;margin-top:6px;display:block}.cap{color:#999;font-size:13px}"
     ".row{display:flex;flex-wrap:wrap;gap:10px}</style></head><body><h1>HUD and menu assets - inventory</h1>"
     "<p>All 2D interface art of the game, decoded from the game data (private review page, nothing generated). "
     "For each group: what it is, and the proposed upgrade for VR. Sprite numbers are the indices in their file.</p>"]
for title, what, plan, imgs in groups:
    h.append("<div class=g><h2>%s</h2><div class=w>%s</div><div class=p><b>Upgrade:</b> %s</div>" % (title, what, plan))
    if any(n.startswith("proj-") for n, _ in imgs):
        h.append("<div class=row>" + "".join("<div><img src='%s'><div class=cap>%s</div></div>" % (n, c) for n, c in imgs) + "</div>")
    else:
        for n, c in imgs:
            if n:
                h.append("<div class=cap>%s</div><img src='%s'>" % (c, n))
    h.append("</div>")
h.append("</body></html>")
open(os.path.join(OUT, "index.html"), "w", encoding="utf-8").write("\n".join(h))
print("page", os.path.join(OUT, "index.html"), len(groups), "groups")
