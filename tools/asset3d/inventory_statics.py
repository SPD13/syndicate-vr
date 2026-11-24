"""Inventory of the static (scenery) sprites placed in the maps and levels.

python inventory_statics.py [out_dir]
Static objects (SmTT_STATIC) are SimpleThing records (60 bytes) stored after
a ushort count in maps/*.mad and, for level format 5+, in levels/*.dat. This
scans each file for the longest run of such records (count + records whose
types are all scenery: STATIC 5, TRAFFIC 10, SMOKE_GENERATOR 0x18), counts
the static sprites (animation index = StartFrame + 1) per file, and writes a
numbered sheet (first frame, scaled up) with how often and where each is used:
<out_dir>/index.html, default assets3d/review/statics.
"""
import collections
import glob
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402

import swdata  # noqa: E402

REC = 60
STATIC, TRAFFIC, SMOKE = 5, 10, 0x18
OK_TYPES = {STATIC, TRAFFIC, SMOKE}


def scan(data, n_anims):
    """Best run: (offset, records) with the most records, all valid."""
    best = (None, [])
    for off in range(0, len(data) - 2 - REC):
        n = struct.unpack_from("<H", data, off)[0]
        if n < 1 or n > 3000 or off + 2 + n * REC > len(data) or n <= len(best[1]):
            continue
        recs = []
        for i in range(n):
            r = off + 2 + i * REC
            typ = data[r + 9]
            if typ not in OK_TYPES:
                break
            start = struct.unpack_from("<h", data, r + 38)[0]
            x, y, z = struct.unpack_from("<iii", data, r + 24)
            if typ == STATIC and not (0 <= start + 1 < n_anims):
                break
            if not (0 <= x < (128 << 16) * 2 and 0 <= z < (128 << 16) * 2):
                break
            recs.append((typ, start + 1))
        else:
            if len(recs) > len(best[1]):
                best = (off, recs)
    return best


def main():
    gamedata = swdata.find_gamedata()
    root = os.path.join(os.path.dirname(gamedata), "assets3d")
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(root, "review", "statics")
    os.makedirs(out, exist_ok=True)
    anims = swdata.AnimTables(gamedata)
    bank = swdata.SpriteBank(gamedata)
    pal = swdata.load_palette(gamedata)
    uses = collections.Counter()
    where = collections.defaultdict(set)
    files = sorted(glob.glob(os.path.join(gamedata, "maps", "*.mad")) + glob.glob(os.path.join(gamedata, "levels", "*.dat")))
    scanned = 0
    for f in files:
        data = open(f, "rb").read()
        off, recs = scan(data, len(anims.nstart))
        if off is None:
            continue
        scanned += 1
        for typ, anim in recs:
            if typ == STATIC:
                uses[anim] += 1
                where[anim].add(os.path.basename(f))
    items = []
    for k, (anim, n) in enumerate(uses.most_common(), 1):
        frames = anims.anim_frames(anim)
        img = None
        if frames:
            img, _ = swdata.compose_frame(bank, anims, frames[0], pal)
        name = "S%d" % k
        if img is not None:
            s = max(1, 160 // max(img.size))
            img.resize((img.width * s, img.height * s), Image.NEAREST).save(os.path.join(out, name + ".png"))
        items.append({"label": name, "anim": anim, "uses": n, "files": sorted(where[anim]), "frames": len(frames),
                      "image": img is not None})
    json.dump({"files_scanned": scanned, "files_total": len(files), "statics": items},
              open(os.path.join(out, "statics.json"), "w"), indent=1)
    cards = "".join(
        "<div class=c><div class=l>%s</div>%s<div class=m>anim %d, %d frame(s)<br>%d placed in %d map/level file(s)</div></div>"
        % (it["label"], "<img src='%s.png'>" % it["label"] if it["image"] else "<i>no image</i>", it["anim"],
           it["frames"], it["uses"], len(it["files"])) for it in items)
    html = ("<!DOCTYPE html><html><head><meta charset='utf-8'><title>Static map sprites</title><style>"
            "body{background:#1d1e22;color:#ddd;font:14px system-ui;margin:24px}.g{display:flex;flex-wrap:wrap;gap:12px}"
            ".c{background:#2a2b31;border-radius:8px;padding:10px;width:190px;text-align:center}"
            ".l{font-size:24px;font-weight:700;color:#ffcc33}img{image-rendering:pixelated;max-width:170px;"
            "background:#46484f;margin:6px 0}.m{color:#aaa;font-size:12px}</style></head><body>"
            "<h1>Static map sprites</h1><p>%d different sprites placed as scenery across %d of %d map and level "
            "files, most used first. Refer to one by its S number.</p><div class=g>%s</div></body></html>"
            % (len(items), scanned, len(files), cards))
    open(os.path.join(out, "index.html"), "w", encoding="utf-8").write(html)
    print("statics:", len(items), "sprites,", sum(uses.values()), "placements,", scanned, "of", len(files), "files")


if __name__ == "__main__":
    main()
