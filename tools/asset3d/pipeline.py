"""Drives Tripo for one person type. Run the stages in order:

  python pipeline.py redraw  <type> [--tag v1] [--model seedream_v5] [--extra "..."] [--ref IMG]
      Route B step 1: the sprite views -> one clean front reference.
      Writes work/<type>/B/redraw_<tag>/. Review it before going on.
  python pipeline.py model-b <type> --tag v1
      Route B step 2: approved redraw -> image-to-multiview -> 3D model.
  python pipeline.py model-a <type>
      Route A (control): 4 upscaled sprite views -> multiview-to-model.
  python pipeline.py rig     <type> --route A|B [--tag v1]
      Rig check, rig (mixamo skeleton), retarget idle/walk/run.
  python pipeline.py status
      Credits spent and the tasks in the ledger.

Common options: --dry-run, --max-credits N (default 800), --out DIR.
Needs $TRIPO_API_KEY (not for --dry-run). Outputs in <out>/work/<type>/.
"""
import argparse
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swdata  # noqa: E402
from tripo import Tripo  # noqa: E402

MODEL_3D = "v3.1-20260211"
GEN_OPTS = {"texture": True, "pbr": True, "texture_quality": "detailed", "face_limit": 20000}
RIG_MODEL = "v3.0-20260909"
ANIMS = ["preset:idle", "preset:walk", "preset:run"]
HERE = os.path.dirname(os.path.abspath(__file__))


def prompt_for(name, extra=""):
    chars = json.load(open(os.path.join(HERE, "characters.json"), encoding="utf-8"))
    if name not in chars:
        raise SystemExit("no description for %s in characters.json" % name)
    return chars["_common"] + chars[name] + ((" " + extra) if extra else "")


def find_image(files):
    for k, p in files.items():
        if p.lower().endswith((".png", ".jpg", ".jpeg", ".webp")):
            return p
    return None


def find_model(files):
    for k, p in files.items():
        if p.lower().endswith((".glb", ".fbx", ".gltf", ".zip")) and "pbr" not in k:
            return p
    for k, p in files.items():
        if p.lower().endswith((".glb", ".fbx", ".gltf", ".zip")):
            return p
    return None


def stage_redraw(t, out, name, args):
    refs = os.path.join(out, "refs", name)
    tokens = [t.upload(os.path.join(refs, v + ".png")) for v in ("front", "front34", "right")]
    extra = args.extra
    if args.ref:
        tokens.append(t.upload(args.ref))
        chars = json.load(open(os.path.join(HERE, "characters.json"), encoding="utf-8"))
        extra = (chars.get("_ref_note", "") + " " + extra).strip()
    body = {"inputs": tokens, "prompt": prompt_for(name, extra),
            "model": args.model, "size": "2K"}
    dest = os.path.join(out, "work", name, "B", "redraw_" + args.tag)
    rec = t.run("/generation/image-to-image", body, dest, estimate=20, label="%s redraw %s" % (name, args.tag))
    img = find_image(rec.get("files", {}))
    if img:
        shutil.copyfile(img, os.path.join(dest, "redraw.png"))
        print("review: %s" % os.path.join(dest, "redraw.png"))


def stage_model_b(t, out, name, args):
    dest = os.path.join(out, "work", name, "B", "redraw_" + args.tag)
    img = os.path.join(dest, "redraw.png")
    if not os.path.exists(img) and not args.dry_run:
        raise SystemExit("no redraw at %s: run the redraw stage first" % img)
    token = t.upload(img) if os.path.exists(img) else "dry"
    mv = t.run("/generation/image-to-multiview", {"input": token}, os.path.join(dest, "multiview"),
               estimate=10, label="%s multiview %s" % (name, args.tag))
    body = dict({"inputs": [{"task_id": mv["task_id"]}], "model": MODEL_3D}, **GEN_OPTS)
    rec = t.run("/generation/multiview-to-model", body, os.path.join(dest, "model"),
                estimate=50, label="%s model B %s" % (name, args.tag))
    print("model: %s" % find_model(rec.get("files", {})))


def stage_model_a(t, out, name, args):
    refs = os.path.join(out, "refs", name)
    views = [{v: t.upload(os.path.join(refs, v + ".png"))} for v in ("front", "left", "back", "right")]
    body = dict({"inputs": views, "model": MODEL_3D}, **GEN_OPTS)
    rec = t.run("/generation/multiview-to-model", body, os.path.join(out, "work", name, "A", "model"),
                estimate=50, label="%s model A" % name)
    print("model: %s" % find_model(rec.get("files", {})))


def model_task(t, name, route, tag):
    label = "%s model A" % name if route == "A" else "%s model B %s" % (name, tag)
    for e in reversed(t._entries()):
        if e.get("kind") == "task" and e.get("label") == label and e.get("status") == "success":
            return e["task_id"]
    raise SystemExit("no finished task '%s' in the ledger" % label)


def stage_rig(t, out, name, args):
    src = model_task(t, name, args.route, args.tag) if not args.dry_run else "dry_model"
    dest = os.path.join(out, "work", name, args.route if args.route == "A" else "B/redraw_" + args.tag, "rig")
    chk = t.run("/animations/rig-check", {"input": src}, os.path.join(dest, "check"), estimate=0,
                label="%s rigcheck %s" % (name, args.route))
    print("rig check: %s" % json.dumps(chk.get("output")))
    if not args.dry_run and chk.get("output", {}).get("riggable") is False:
        raise SystemExit("Tripo says the model is not riggable")
    rig = t.run("/animations/rig", {"input": src, "model": RIG_MODEL, "rig_type": "biped",
                                     "spec": "mixamo", "out_format": "glb"},
                os.path.join(dest, "rigged"), estimate=25, label="%s rig %s" % (name, args.route))
    anim = t.run("/animations/retarget", {"input": rig["task_id"], "animations": ANIMS, "out_format": "glb"},
                 os.path.join(dest, "anims"), estimate=10 * len(ANIMS), label="%s anims %s" % (name, args.route))
    print("rigged: %s" % ", ".join(rig.get("files", {}).values()))
    print("anims: %s" % ", ".join(anim.get("files", {}).values()))


def stage_status(t, out, name, args):
    for e in t._entries():
        if e.get("kind") == "task" and e.get("status") in ("success", "failed", "cancelled"):
            print("%-28s %-8s %6s  %s" % (e.get("label"), e.get("status"), e.get("credits"), e.get("task_id")))
    print("credits spent: %.0f of %d" % (t.credits_spent(), t.max_credits))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("stage", choices=["redraw", "model-b", "model-a", "rig", "status"])
    ap.add_argument("type", nargs="?", default="")
    ap.add_argument("--tag", default="v1")
    ap.add_argument("--model", default="seedream_v5", help="image model for the redraw")
    ap.add_argument("--extra", default="", help="extra prompt text for the redraw")
    ap.add_argument("--ref", help="extra reference image for the redraw (image[4], e.g. box art)")
    ap.add_argument("--route", choices=["A", "B"], default="B")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--max-credits", type=int, default=800)
    ap.add_argument("--out")
    args = ap.parse_args()
    out = args.out or os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")
    if args.stage != "status" and args.type not in swdata.PERSON_TYPES:
        raise SystemExit("unknown person type %r" % args.type)
    t = Tripo(out, args.max_credits, args.dry_run)
    {"redraw": stage_redraw, "model-b": stage_model_b, "model-a": stage_model_a,
     "rig": stage_rig, "status": stage_status}[args.stage](t, out, args.type, args)


if __name__ == "__main__":
    main()
