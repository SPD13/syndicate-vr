"""Builds one character end to end from an approved redraw.

python build_character.py <type> <tag> [--height 1.8] [--clips-from mixamo/raw]
                          [--skip-tripo] [--no-weapons] [--max-credits N]

Steps (each one reused when its output exists; Tripo tasks are reused
through the ledger):
 1. Tripo: four views + model (pipeline.py model-b), rig (rig --skip-anims)
 2. FBX of the rig at --height metres (blender_glb2fbx.py)
 3. clips: the agent's Mixamo clips (--clips-from) merged onto this rig
    (make_clips.py + blender_merge_anims.py: world-space retarget, scaled
    to the hip height) -> models/<type>/<type>_combat.glb
 4. wrist check of the idle (hands must hang down)
 5. weapons on the right hand (models/weapons/attach_agent.json)
    -> models/<type>/<type>_armed.glb
 6. review GIFs, front/side/back -> review/<type>/index.html
Needs $TRIPO_API_KEY for step 1. Output root: the assets3d folder.
"""
import argparse
import glob
import json
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import swdata  # noqa: E402

ROOT = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d")


def blender():
    c = sorted(glob.glob(r"C:\Program Files\Blender Foundation\Blender*\blender.exe"))
    if not c:
        raise SystemExit("Blender not found")
    return c[-1]


def run(cmd, env=None, keep=None):
    print("$", " ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)), flush=True)
    e = dict(os.environ)
    if env:
        e.update(env)
    p = subprocess.run(cmd, cwd=ROOT, env=e, capture_output=True, text=True)
    out = p.stdout + p.stderr
    lines = [l for l in out.splitlines() if not keep or any(k in l for k in keep)]
    for l in lines[-25:]:
        print("   ", l, flush=True)
    if p.returncode != 0 or "Traceback" in out:
        raise SystemExit("step failed: %s" % " ".join(cmd[:4]))
    return out


def bl(script, *args, env=None, keep=("exported", "attached", "rendered", "clip done", "saved", "Error", "Traceback")):
    return run([blender(), "-b", "--factory-startup", "-P", os.path.join(HERE, script), "--"] + list(args),
               env=env, keep=keep)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("type")
    ap.add_argument("tag")
    ap.add_argument("--height", default="1.8")
    ap.add_argument("--clips-from", default="mixamo/raw")
    ap.add_argument("--skip-tripo", action="store_true")
    ap.add_argument("--no-weapons", action="store_true")
    ap.add_argument("--max-credits", default="2500")
    a = ap.parse_args()
    t, tag = a.type, a.tag
    mdir = os.path.join(ROOT, "models", t)
    os.makedirs(mdir, exist_ok=True)
    py = sys.executable
    rigged = os.path.join(mdir, t + "_rigged.glb")

    # 1. Tripo
    if not a.skip_tripo:
        run([py, os.path.join(HERE, "pipeline.py"), "model-b", t, "--tag", tag, "--max-credits", a.max_credits],
            keep=("done", "reuse", "model:", "Error", "Budget"))
        run([py, os.path.join(HERE, "pipeline.py"), "rig", t, "--route", "B", "--tag", tag, "--skip-anims",
             "--max-credits", a.max_credits], keep=("done", "reuse", "rig check", "Error", "Budget"))
        src = os.path.join(ROOT, "work", t, "B", "redraw_" + tag, "rig", "rigged", "model.glb")
        shutil.copyfile(src, rigged)
        shutil.copyfile(os.path.join(ROOT, "work", t, "B", "redraw_" + tag, "model", "model.glb"),
                        os.path.join(mdir, t + "_static.glb"))
    # 2. FBX base
    fbx = os.path.join(ROOT, "mixamo", t + "_rigged.fbx")
    bl("blender_glb2fbx.py", rigged, fbx, a.height)
    # 3. clips
    cj = os.path.join(ROOT, "mixamo", "clips_%s.json" % t)
    run([py, os.path.join(HERE, "make_clips.py"), os.path.join(ROOT, a.clips_from), fbx, rigged, cj])
    combat = os.path.join(mdir, t + "_combat.glb")
    bl("blender_merge_anims.py", cj, combat)
    # 4. wrist check
    out = bl("blender_wrist_check.py", combat + "@idle", keep=("  f",))
    ups = [float(x.split("up")[1].split("|")[0]) for x in out.splitlines() if " up " in x and "f" in x.split()[0]]
    print("   idle hand directions:", ups)
    if ups and max(ups) > -60:
        raise SystemExit("hands are not hanging down in idle: check the retarget")
    # 5. weapons
    final = combat
    if not a.no_weapons:
        final = os.path.join(mdir, t + "_armed.glb")
        bl("blender_attach_weapons.py", os.path.join(ROOT, "models", "weapons", "attach_agent.json"), combat, final)
    # 6. review
    clips_dir = os.path.join(ROOT, "work", t, "clips")
    shutil.rmtree(clips_dir, ignore_errors=True)
    jobs = os.path.join(ROOT, "work", "agent", "gif_jobs.json")
    if a.no_weapons:
        jobs_l = [j for j in json.load(open(jobs)) if not j.get("weapon")]
        jobs = os.path.join(ROOT, "work", t, "gif_jobs.json")
        json.dump(jobs_l, open(jobs, "w"))
    bl("blender_render_clips.py", final, clips_dir, jobs, env={"CLIP_VIEWS": "30,90,180"}, keep=("Error", "Traceback"))
    run([py, os.path.join(HERE, "make_gifs.py"), clips_dir, os.path.join(ROOT, "review", t),
         "%s - animation review" % t], keep=("page",))
    print("DONE", t, final, flush=True)


if __name__ == "__main__":
    main()
