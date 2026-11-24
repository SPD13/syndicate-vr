"""Builds upscaled reference views of a person type for the 3D pipeline.

Usage: python prepare_refs.py [--types agent,police,briefcase_m] [--anim idle]
       [--size 512] [--bg 255,255,255]

Sprite directions (camera relative Angle, checked on the briefcase man):
  d4 front (face visible), d0 back, d2 the person's right side (facing
  the viewer's right), d6 its mirror = left side; d1/d3/d5/d7 are 3/4
  views, and d5..d7 are mirrored copies of d3..d1 in the game data.
Writes <out>/refs/<type>/<view>.png for front, back, left, right, front34,
back34, plus a 1-row strip refs/<type>/strip.png.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402

import swdata  # noqa: E402

VIEWS = {"front": 4, "back": 0, "right": 2, "left": 6, "front34": 3, "back34": 1}


def upscale(img, size, bg, fill=0.86):
    """Crops to the alpha bbox, scales by an integer factor (nearest) so the
    figure fills about `fill` of the height, centers it on a square."""
    img = img.crop(img.getbbox())
    k = max(1, int(size * fill) // max(img.width, img.height))
    big = img.resize((img.width * k, img.height * k), Image.NEAREST)
    out = Image.new("RGBA", (size, size), bg + (255,))
    out.alpha_composite(big, ((size - big.width) // 2, (size - big.height) // 2))
    return out.convert("RGB")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--types", default="agent,police,briefcase_m")
    ap.add_argument("--anim", default="idle")
    ap.add_argument("--size", type=int, default=512)
    ap.add_argument("--bg", default="255,255,255")
    ap.add_argument("--out")
    args = ap.parse_args()
    out = args.out or os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
    bg = tuple(int(v) for v in args.bg.split(","))
    for name in args.types.split(","):
        d = os.path.join(out, "refs", name)
        os.makedirs(d, exist_ok=True)
        imgs = []
        for view, angle in VIEWS.items():
            src = os.path.join(out, "sprites", name, args.anim, "d%d_f00.png" % angle)
            if not os.path.exists(src):
                raise SystemExit("missing %s: run extract_sprites.py first" % src)
            im = upscale(Image.open(src).convert("RGBA"), args.size, bg)
            im.save(os.path.join(d, view + ".png"))
            imgs.append(im)
        strip = Image.new("RGB", (args.size * len(imgs), args.size), bg)
        for i, im in enumerate(imgs):
            strip.paste(im, (i * args.size, 0))
        strip.save(os.path.join(d, "strip.png"))
        print("%s: %s" % (name, ", ".join(VIEWS)))


if __name__ == "__main__":
    main()
