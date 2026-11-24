"""Composes Syndicate Wars person frames into PNGs and contact sheets.

Usage: python extract_sprites.py [--types agent,police,briefcase_m]
       [--out DIR] [--variant 0] [--scale 4]

Writes <out>/sprites/<type>/<anim>/d<angle>_f<n>.png, a sheet per type
(<out>/sheets/<type>.png: rows = animations, 8 directions of frame 0) and
<out>/meta/<type>.json (frame numbers, sizes, origin offsets).
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw  # noqa: E402

import swdata  # noqa: E402

DEFAULT_TYPES = "agent,police,briefcase_m"


def default_out(gamedata):
    return os.path.join(os.path.dirname(os.path.abspath(gamedata)), "assets3d")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--types", default=DEFAULT_TYPES, help="comma list, or 'all'")
    ap.add_argument("--gamedata")
    ap.add_argument("--out")
    ap.add_argument("--variant", type=int, default=0, help="mspr-N sprite set")
    ap.add_argument("--scale", type=int, default=4, help="sheet scale")
    args = ap.parse_args()

    gamedata = args.gamedata or swdata.find_gamedata()
    out = args.out or default_out(gamedata)
    repo = swdata.find_repo_root()
    pal = swdata.load_palette(gamedata)
    bank = swdata.SpriteBank(gamedata, args.variant)
    anims = swdata.AnimTables(gamedata)
    table = swdata.load_people_frames(repo)

    names = swdata.PERSON_TYPES[1:] if args.types == "all" else args.types.split(",")
    for name in names:
        st = swdata.PERSON_TYPES.index(name)
        meta = {"type": name, "subtype": st, "variant": args.variant, "anims": {}}
        sheet_rows = []
        done_bases = {}
        for ai, aname in enumerate(swdata.ANIM_NAMES):
            base = table[st][ai]
            ameta = {"base": base, "dirs": {}}
            row = []
            for d in range(8):
                if base + d >= len(anims.nstart):
                    break
                frames = anims.anim_frames(base + d)
                dmeta = []
                for fi, frm in enumerate(frames):
                    img, origin = swdata.compose_frame(bank, anims, frm, pal)
                    if img is None:
                        continue
                    rel = os.path.join("sprites", name, aname, "d%d_f%02d.png" % (d, fi))
                    path = os.path.join(out, rel)
                    os.makedirs(os.path.dirname(path), exist_ok=True)
                    img.save(path)
                    dmeta.append({"frame": frm, "file": rel.replace("\\", "/"),
                                  "w": img.width, "h": img.height,
                                  "ox": origin[0], "oy": origin[1]})
                    if fi == 0:
                        row.append((img, origin))
                ameta["dirs"][d] = dmeta
            meta["anims"][aname] = ameta
            if row and base not in done_bases:
                done_bases[base] = aname
                sheet_rows.append((aname, row))
        os.makedirs(os.path.join(out, "meta"), exist_ok=True)
        with open(os.path.join(out, "meta", name + ".json"), "w") as f:
            json.dump(meta, f, indent=1)
        write_sheet(os.path.join(out, "sheets", name + ".png"), sheet_rows, args.scale)
        print("%s: %d distinct animations" % (name, len(sheet_rows)))


def write_sheet(path, rows, scale):
    cw = max(img.width for _, r in rows for img, _ in r) + 4
    ch = max(img.height for _, r in rows for img, _ in r) + 4
    label_w = 120
    sheet = Image.new("RGBA", (label_w + 8 * cw * scale, len(rows) * ch * scale), (40, 40, 48, 255))
    draw = ImageDraw.Draw(sheet)
    for ri, (aname, row) in enumerate(rows):
        draw.text((4, ri * ch * scale + 4), aname, fill=(220, 220, 220, 255))
        for d, (img, _) in enumerate(row):
            big = img.resize((img.width * scale, img.height * scale), Image.NEAREST)
            x = label_w + d * cw * scale + (cw * scale - big.width) // 2
            y = ri * ch * scale + (ch * scale - big.height) // 2
            sheet.alpha_composite(big, (x, y))
        for d in range(len(row)):
            draw.text((label_w + d * cw * scale + 2, ri * ch * scale + 2), "d%d" % d, fill=(150, 150, 150, 255))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    sheet.save(path)


if __name__ == "__main__":
    main()
