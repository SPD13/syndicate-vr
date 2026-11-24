"""Numbered review page of weapon redraws next to their shop renders.

python make_props_gallery.py <out_dir> <title> <entry> [<entry> ...]
entry: "<label>=<name>:<tag>" e.g. "W1=laser:v1".
Writes <out_dir>/index.html with, per weapon, the label, the name, how it is
held (props.json "hold"), the shop render (work/props/<name>/ref.png) and the
redraw (work/props/<name>/redraw_<tag>/redraw.png).
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402

import swdata  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    out, title, entries = sys.argv[1], sys.argv[2], sys.argv[3:]
    base = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
    props = json.load(open(os.path.join(HERE, "props.json"), encoding="utf-8"))
    os.makedirs(out, exist_ok=True)
    cards = []
    for e in entries:
        label, rest = e.split("=")
        name, tag = rest.split(":")
        d = os.path.join(base, "work", "props", name)
        red = os.path.join(d, "redraw_" + tag, "redraw.png")
        if not os.path.exists(red):
            print("missing", red)
            continue
        ref = Image.open(os.path.join(d, "ref.png")).convert("RGB")
        ref.crop(ref.convert("L").point(lambda v: 255 if v > 8 else 0).getbbox()).save(
            os.path.join(out, "%s_%s_ref.png" % (label, name)))
        im = Image.open(red).convert("RGB")
        im.thumbnail((900, 900))
        im.save(os.path.join(out, "%s_%s_%s.jpg" % (label, name, tag)), quality=90)
        cards.append((label, name, tag, props.get(name, {}).get("hold", "?")))
    html = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>%s</title><style>"
            "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}"
            ".card{background:#2a2b31;border-radius:8px;padding:14px;margin:0 0 22px;display:flex;gap:18px;align-items:center}"
            ".lab{font-size:38px;font-weight:700;color:#ffcc33;min-width:72px}"
            ".name{font-size:20px;margin-bottom:8px}.type{color:#999}"
            "img.red{height:420px;border-radius:4px;background:#fff}img.ref{width:320px;display:block;background:#000}"
            "</style></head><body><h1>%s</h1><p>Number, weapon, how it is held (light = one hand, pistol poses; "
            "heavy = two hands, rifle poses), the 1996 shop render and the redraw. Refer to a weapon by its number "
            "when asking for changes.</p>" % (title, title)]
    for label, name, tag, hold in cards:
        html.append("<div class=card><div class=lab>%s</div><div><div class=name>%s <span class=type>(%s, %s)</span></div>"
                    "<img class=ref src='%s_%s_ref.png'></div><img class=red src='%s_%s_%s.jpg'></div>"
                    % (label, name, hold, tag, label, name, label, name, tag))
    html.append("</body></html>")
    open(os.path.join(out, "index.html"), "w", encoding="utf-8").write("\n".join(html))
    print("page", os.path.join(out, "index.html"), len(cards), "weapons")


if __name__ == "__main__":
    main()
