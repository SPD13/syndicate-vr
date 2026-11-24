"""Builds a numbered review page of character redraws next to their sprites.

python make_gallery.py <out_dir> <title> <entry> [<entry> ...]
entry: "<label>=<type>:<tag>" e.g. "1=police:v1" or "A=agent:v3".
Writes <out_dir>/index.html with, per character, the label, the type name,
the sprite views (front, 3/4, side, back of the idle frame, scaled up) and
the redraw (work/<type>/B/redraw_<tag>/redraw.png).
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402

import swdata  # noqa: E402

NAMES = {
    "agent": "EuroCorp agent", "zealot": "Church of the New Epoch agent", "police": "Police",
    "mercenary": "Mercenary", "scientist": "Scientist", "high_priest": "High Priest",
    "briefcase_m": "Briefcase man", "shady_m": "Shady man", "leth_jackt_m": "Leather-jacket man",
    "punk_m": "Punk (male)", "punk_f": "Punk (female)", "white_brun_f": "Brunette woman",
    "whit_blond_f": "Blonde woman",
}


def sprite_strip(base, typ, height=360):
    ims = []
    for d in (4, 3, 2, 0):
        p = os.path.join(base, "sprites", typ, "idle", "d%d_f00.png" % d)
        im = Image.open(p).convert("RGBA")
        im = im.crop(im.getbbox())
        k = max(1, height // im.height)
        ims.append(im.resize((im.width * k, im.height * k), Image.NEAREST))
    w = sum(i.width for i in ims) + 20 * (len(ims) + 1)
    h = max(i.height for i in ims) + 20
    strip = Image.new("RGB", (w, h), (70, 72, 80))
    x = 20
    for i in ims:
        strip.paste(i, (x, h - i.height - 10), i)
        x += i.width + 20
    return strip


def main():
    out, title, entries = sys.argv[1], sys.argv[2], sys.argv[3:]
    base = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
    os.makedirs(out, exist_ok=True)
    cards = []
    for e in entries:
        label, rest = e.split("=")
        typ, tag = rest.split(":")
        red = os.path.join(base, "work", typ, "B", "redraw_" + tag, "redraw.png")
        if not os.path.exists(red):
            print("missing", red)
            continue
        sprite_strip(base, typ).save(os.path.join(out, "%s_%s_sprites.png" % (label, typ)))
        im = Image.open(red).convert("RGB")
        im.thumbnail((1100, 1100))
        im.save(os.path.join(out, "%s_%s_%s.jpg" % (label, typ, tag)), quality=90)
        cards.append((label, typ, tag))
    html = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>%s</title><style>"
            "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}"
            ".card{background:#2a2b31;border-radius:8px;padding:14px;margin:0 0 22px;display:flex;gap:18px;align-items:flex-start}"
            ".lab{font-size:42px;font-weight:700;color:#ffcc33;min-width:64px}"
            ".name{font-size:20px;margin-bottom:8px}.type{color:#999}"
            "img.red{height:560px;border-radius:4px;background:#fff}img.spr{height:180px;display:block;margin-top:10px}"
            "</style></head><body><h1>%s</h1><p>Number, name, the original sprites (front, 3/4, side, back of the idle frame) "
            "and the redraw. Refer to a character by its number when asking for changes.</p>" % (title, title)]
    for label, typ, tag in cards:
        html.append("<div class=card><div class=lab>%s</div><div><div class=name>%s <span class=type>(%s, %s)</span></div>"
                    "<img class=spr src='%s_%s_sprites.png'></div><img class=red src='%s_%s_%s.jpg'></div>"
                    % (label, NAMES.get(typ, typ), typ, tag, label, typ, label, typ, tag))
    html.append("</body></html>")
    open(os.path.join(out, "index.html"), "w", encoding="utf-8").write("\n".join(html))
    print("page", os.path.join(out, "index.html"), len(cards), "characters")


if __name__ == "__main__":
    main()
