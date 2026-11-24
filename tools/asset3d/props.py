"""Props (weapons) through Tripo: shop render -> redraw -> single-image model.

  python props.py ref    <name>             reference from data/equip/wep-NN.fli
  python props.py redraw <name> [--tag v1] [--extra "..."]
  python props.py model  <name> [--tag v1] [--multiview]
Options: --dry-run, --max-credits N, --out DIR. Outputs in <out>/work/props/<name>/.
Descriptions and the FLI number of each prop are in props.json.
"""
import argparse
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image  # noqa: E402

import flic  # noqa: E402
import swdata  # noqa: E402
from tripo import Tripo  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
MODEL_3D = "v3.1-20260211"


def load_props():
    return json.load(open(os.path.join(HERE, "props.json"), encoding="utf-8"))


def stage_ref(out, name, info, args):
    gamedata = swdata.find_gamedata()
    frames = flic.decode(os.path.join(gamedata, "data", "equip", "wep-%02d.fli" % info["fli"]),
                         os.path.join(gamedata, "data", "s-proj.pal"))
    img = frames[-1]
    bbox = img.convert("L").point(lambda v: 255 if v > 8 else 0).getbbox()
    img = img.crop(bbox)
    k = 900 / max(img.width, img.height)
    img = img.resize((int(img.width * k), int(img.height * k)), Image.LANCZOS)
    canvas = Image.new("RGB", (1024, 1024), (0, 0, 0))
    canvas.paste(img, ((1024 - img.width) // 2, (1024 - img.height) // 2))
    d = os.path.join(out, "work", "props", name)
    os.makedirs(d, exist_ok=True)
    canvas.save(os.path.join(d, "ref.png"))
    print("ref: %s" % os.path.join(d, "ref.png"))


def stage_redraw(t, out, name, info, args):
    d = os.path.join(out, "work", "props", name)
    token = t.upload(os.path.join(d, "ref.png"))
    props = load_props()
    prompt = props["_common"] + info["desc"] + ((" " + args.extra) if args.extra else "")
    dest = os.path.join(d, "redraw_" + args.tag)
    rec = t.run("/generation/image-to-image", {"inputs": [token], "prompt": prompt, "model": args.model, "size": "2K"},
                dest, estimate=20, label="%s redraw %s" % (name, args.tag))
    for p in rec.get("files", {}).values():
        if p.lower().endswith((".png", ".jpg", ".jpeg", ".webp")):
            shutil.copyfile(p, os.path.join(dest, "redraw.png"))
            print("review: %s" % os.path.join(dest, "redraw.png"))


def stage_model(t, out, name, info, args):
    dest = os.path.join(out, "work", "props", name, "redraw_" + args.tag)
    img = os.path.join(dest, "redraw.png")
    token = t.upload(img) if os.path.exists(img) else "dry"
    opts = {"model": MODEL_3D, "texture": True, "pbr": True,
            "texture_quality": "detailed", "face_limit": info.get("faces", 8000)}
    if args.multiview:
        # Four consistent generated views fix depth better than one image.
        mv = t.run("/generation/image-to-multiview", {"input": token}, os.path.join(dest, "multiview"),
                   estimate=10, label="%s multiview %s" % (name, args.tag))
        body = dict({"inputs": [{"task_id": mv["task_id"]}]}, **opts)
        rec = t.run("/generation/multiview-to-model", body, os.path.join(dest, "model_mv"),
                    estimate=50, label="%s model mv %s" % (name, args.tag))
    else:
        body = dict({"input": token}, **opts)
        rec = t.run("/generation/image-to-model", body, os.path.join(dest, "model"),
                    estimate=50, label="%s model %s" % (name, args.tag))
    print("files: %s" % ", ".join(rec.get("files", {}).values()))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", choices=["ref", "redraw", "model"])
    ap.add_argument("name")
    ap.add_argument("--tag", default="v1")
    ap.add_argument("--model", default="seedream_v5")
    ap.add_argument("--extra", default="")
    ap.add_argument("--multiview", action="store_true", help="model: image-to-multiview + multiview-to-model")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--max-credits", type=int, default=800)
    ap.add_argument("--out")
    args = ap.parse_args()
    out = args.out or os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
    props = load_props()
    if args.name not in props:
        raise SystemExit("unknown prop %r (props.json)" % args.name)
    info = props[args.name]
    if "same_as" in info:
        raise SystemExit("%s reuses the model of %s (props.json)" % (args.name, info["same_as"]))
    if args.stage == "ref":
        return stage_ref(out, args.name, info, args)
    t = Tripo(out, args.max_credits, args.dry_run)
    {"redraw": stage_redraw, "model": stage_model}[args.stage](t, out, args.name, info, args)


if __name__ == "__main__":
    main()
