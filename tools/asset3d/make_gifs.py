"""Builds animations and an index.html from blender_render_clips.py frames.

python make_gifs.py <clips_dir> <out_dir> [title]
Animated PNGs (full colour) by default; MAKE_GIFS_FORMAT=gif for 128-colour GIFs.
Each GIF shows the two views side by side with the clip name; the page
groups clips without weapon and with weapon, original sprite sheet first
if <out_dir>/../../sheets/<type>.png exists.
"""
import json
import os
import sys

from PIL import Image, ImageDraw

EXT = os.environ.get("MAKE_GIFS_FORMAT", "png")


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
        nviews = 0
        while os.path.exists(os.path.join(d, "f000_%d.png" % nviews)):
            nviews += 1
        labels = info.get("views_labels") or ["3/4 front", "right side", "back", "view 4"]
        for i in range(info["frames"]):
            tiles = [Image.open(os.path.join(d, "f%03d_%d.png" % (i, k))).convert("RGB") for k in range(nviews)]
            w0, h0 = tiles[0].size
            im = Image.new("RGB", (w0 * nviews, h0))
            dr = ImageDraw.Draw(im)
            for k, t in enumerate(tiles):
                im.paste(t, (k * w0, 0))
                dr.text((k * w0 + 6, h0 - 14), labels[k] if k < len(labels) else "", fill=(200, 200, 200))
            dr.text((6, 4), name, fill=(255, 255, 255))
            frames.append(im if EXT == "png" else im.quantize(colors=128, method=Image.Quantize.MEDIANCUT))
        gif = os.path.join(out_dir, name + "." + EXT)
        frames[0].save(gif, save_all=True, append_images=frames[1:], loop=0,
                       duration=int(1000 / info["fps"]), **({} if EXT == "png" else {"disposal": 2}))
        entries.append((name, info))
        print("gif", name, len(frames))

    def section(label, items):
        cells = "".join('<figure><img src="%s.%s" loading="lazy"><figcaption>%s</figcaption></figure>'
                        % (n, EXT, n) for n, _ in items)
        return "<h2>%s</h2><div class=grid>%s</div>" % (label, cells)

    plain = [e for e in entries if not e[1].get("weapon")]
    armed = [e for e in entries if e[1].get("weapon")]
    html = """<!DOCTYPE html><html><head><meta charset="utf-8"><title>%s</title>
<style>body{background:#1d1e22;color:#ddd;font:14px system-ui;margin:20px}
.grid{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0;background:#2a2b31;padding:6px;border-radius:6px}
figcaption{text-align:center;padding-top:4px;color:#aaa}img{display:block}</style></head>
<body><h1>%s</h1><p>Views left to right: 3/4 front, right side (character faces left), and back when rendered.
Orthographic camera, Workbench render with texture colours, brightened.</p>%s%s</body></html>""" % (
        title, title, section("With weapon", armed), section("Without weapon", plain))
    open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8").write(html)
    print("page", os.path.join(out_dir, "index.html"))


if __name__ == "__main__":
    build(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "Animation review")
