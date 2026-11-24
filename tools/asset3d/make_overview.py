"""Overview page of all finished characters and manifest.json update.

python make_overview.py
For each character with a review page under review/<page>/, shows a
still frame (idle, middle frame of the GIF) and links to the page; writes
review/index.html. Also records each character's final GLB and clips in
manifest.json.
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageSequence  # noqa: E402

import swdata  # noqa: E402

ROOT = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
CHARS = [  # label, name, subtype, review page, final glb
    ("A", "EuroCorp agent", "agent", "agent", "models/agent/agent_armed.glb"),
    ("B", "Church of the New Epoch agent", "zealot", "zealot_cape", "models/zealot/zealot3_cape_armed.glb"),
    ("1", "Police", "police", "police", "models/police/police_armed.glb"),
    ("2", "Mercenary", "mercenary", "mercenary", "models/mercenary/mercenary_armed.glb"),
    ("3", "Scientist", "scientist", "scientist", "models/scientist/scientist_armed.glb"),
    ("4", "High Priest", "high_priest", "high_priest", "models/high_priest/high_priest_cape.glb"),
    ("5", "Briefcase man", "briefcase_m", "briefcase_m", "models/briefcase_m/briefcase_m_armed.glb"),
    ("6", "Shady man", "shady_m", "shady_m", "models/shady_m/shady_m_armed.glb"),
    ("7", "Leather-jacket man", "leth_jackt_m", "leth_jackt_m", "models/leth_jackt_m/leth_jackt_m_armed.glb"),
    ("8", "Punk (male)", "punk_m", "punk_m", "models/punk_m/punk_m_armed.glb"),
    ("9", "Punk (female)", "punk_f", "punk_f", "models/punk_f/punk_f_armed.glb"),
    ("10", "Brunette woman", "white_brun_f", "white_brun_f", "models/white_brun_f/white_brun_f_armed.glb"),
    ("11", "Blonde woman (also the fast blonde woman)", "whit_blond_f", "whit_blond_f",
     "models/whit_blond_f/whit_blond_f_armed.glb"),
]


def glb_info(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<I", d, 12)[0]
    j = json.loads(d[20:20 + n])
    acc = j["accessors"]
    tris = sum(acc[p["indices"]]["count"] // 3 for m in j["meshes"] for p in m["primitives"] if "indices" in p)
    nodes = [nd.get("name", "") for nd in j.get("nodes", [])]
    return {"clips": sorted(a["name"] for a in j.get("animations", [])), "triangles": tris,
            "weapons": sorted(n for n in nodes if n.startswith("weapon_")), "mb": round(len(d) / 1e6, 1)}


def main():
    rev = os.path.join(ROOT, "review")
    man_p = os.path.join(ROOT, "manifest.json")
    man = json.load(open(man_p)) if os.path.exists(man_p) else {}
    man.setdefault("characters", {})
    cards = []
    for label, name, typ, page, glb in CHARS:
        gp = os.path.join(ROOT, glb)
        idle = os.path.join(rev, page, "idle.png")
        if not os.path.exists(idle):
            idle = os.path.join(rev, page, "idle.gif")
        if not (os.path.exists(gp) and os.path.exists(idle)):
            print("not ready:", typ)
            continue
        fr = [f.convert("RGB") for f in ImageSequence.Iterator(Image.open(idle))]
        thumb = "thumb_%s.jpg" % page
        fr[len(fr) // 2].save(os.path.join(rev, thumb), quality=88)
        info = glb_info(gp)
        man["characters"][typ] = dict(info, file=glb, label=label, name=name, review="review/%s/index.html" % page)
        cards.append((label, name, page, thumb, info))
    json.dump(man, open(man_p, "w"), indent=2)
    html = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>Syndicate Wars 3D characters</title><style>"
            "body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}a{color:#9cf}"
            ".grid{display:flex;flex-wrap:wrap;gap:16px}.c{background:#2a2b31;border-radius:8px;padding:10px;width:470px}"
            ".l{font-size:30px;font-weight:700;color:#ffcc33;margin-right:8px}img{width:100%;border-radius:4px}"
            ".m{color:#999;font-size:13px}</style></head><body><h1>Syndicate Wars 3D characters</h1>"
            "<p>Click a character for all its animations (front, side and back views).</p><div class=grid>"]
    for label, name, page, thumb, info in cards:
        html.append("<div class=c><a href='%s/index.html'><div><span class=l>%s</span>%s</div><img src='%s'></a>"
                    "<div class=m>%d clips, %d triangles, %s MB%s</div></div>"
                    % (page, label, name, thumb, len(info["clips"]), info["triangles"], info["mb"],
                       (", weapons: " + ", ".join(w[7:] for w in info["weapons"])) if info["weapons"] else ""))
    html.append("</div></body></html>")
    open(os.path.join(rev, "index.html"), "w", encoding="utf-8").write("\n".join(html))
    print("overview:", os.path.join(rev, "index.html"), len(cards), "characters")


if __name__ == "__main__":
    main()
