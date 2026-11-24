"""Builds animated GIFs and an index.html from blender_render_clips.py frames.

python make_gifs.py <clips_dir> <out_dir> [title]
Each GIF shows the two views side by side with the clip name; the page
groups clips without weapon and with weapon, original sprite sheet first
if <out_dir>/../../sheets/<type>.png exists.
"""
import json
import os
import sys

from PIL import Image, ImageDraw


def build(clips_dir, out_dir, title):
    os.makedirs(out_dir, exist_ok=True)
    entries = []
    for name in sorted(os.listdir(clips_dir)):
        d = os.path.join(clips_dir, name)
        info_p = os.path.join(d, "info.json")
        if not os.path.exists(info_p):
            continue
        info = json.load(open(info_p))
        frames = []
        for i in range(info["frames"]):
            a = Image.open(os.path.join(d, "f%03d_0.png" % i)).convert("RGB")
            b = Image.open(os.path.join(d, "f%03d_1.png" % i)).convert("RGB")
            im = Image.new("RGB", (a.width + b.width, a.height))
            im.paste(a, (0, 0))
            im.paste(b, (a.width, 0))
            dr = ImageDraw.Draw(im)
            dr.text((6, 4), name, fill=(255, 255, 255))
            dr.text((6, a.height - 14), "3/4 front", fill=(200, 200, 200))
            dr.text((a.width + 6, a.height - 14), "right side", fill=(200, 200, 200))
            frames.append(im.quantize(colors=128, method=Image.Quantize.MEDIANCUT))
        gif = os.path.join(out_dir, name + ".gif")
        frames[0].save(gif, save_all=True, append_images=frames[1:], loop=0,
                       duration=int(1000 / info["fps"]), disposal=2)
        entries.append((name, info))
        print("gif", name, len(frames))

    def section(label, items):
        cells = "".join('<figure><img src="%s.gif" loading="lazy"><figcaption>%s</figcaption></figure>'
                        % (n, n) for n, _ in items)
        return "<h2>%s</h2><div class=grid>%s</div>" % (label, cells)

    plain = [e for e in entries if not e[1].get("weapon")]
    armed = [e for e in entries if e[1].get("weapon")]
    html = """<!DOCTYPE html><html><head><meta charset="utf-8"><title>%s</title>
<style>body{background:#1d1e22;color:#ddd;font:14px system-ui;margin:20px}
.grid{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0;background:#2a2b31;padding:6px;border-radius:6px}
figcaption{text-align:center;padding-top:4px;color:#aaa}img{display:block}</style></head>
<body><h1>%s</h1><p>Left: 3/4 front view. Right: right side view (character faces left).
Orthographic camera, Workbench render with texture colours.</p>%s%s</body></html>""" % (
        title, title, section("With weapon", armed), section("Without weapon", plain))
    open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8").write(html)
    print("page", os.path.join(out_dir, "index.html"))


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "Animation review")
