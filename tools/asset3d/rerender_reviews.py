"""Re-renders every finished character's review page (current lighting/format).

python rerender_reviews.py [type ...]
Uses make_overview.CHARS (final GLB and page); the jobs are every clip of
the GLB without weapon, plus pistol_* clips with the Uzi and rifle_* clips
with the minigun when the GLB carries them. Frames go to
work/<type>/clips_review, the page to review/<page>/.
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_character  # noqa: E402
import make_overview  # noqa: E402

ROOT = make_overview.ROOT
only = set(sys.argv[1:])
for label, name, typ, page, glb in make_overview.CHARS:
    if only and typ not in only:
        continue
    gp = os.path.join(ROOT, glb)
    info = make_overview.glb_info(gp)
    weps = {w[7:] for w in info["weapons"]}
    jobs = [{"clip": c, "weapon": None, "name": c} for c in info["clips"]]
    for c in info["clips"]:
        if c.startswith("pistol_") and "uzi" in weps:
            jobs.append({"clip": c, "weapon": "uzi", "name": c + "_uzi"})
        if c.startswith("rifle_") and "minigun" in weps:
            jobs.append({"clip": c, "weapon": "minigun", "name": c + "_minigun"})
    wd = os.path.join(ROOT, "work", typ)
    jp = os.path.join(wd, "jobs_review.json")
    json.dump(jobs, open(jp, "w"))
    clips = os.path.join(wd, "clips_review")
    build_character.bl("blender_render_clips.py", gp, clips, jp, env={"CLIP_VIEWS": "30,90,180"},
                       keep=("Error", "Traceback"))
    build_character.run([sys.executable, os.path.join(HERE, "make_gifs.py"), clips, os.path.join(ROOT, "review", page),
                         "%s - animation review" % name], keep=("page",))
    print("DONE", typ, len(jobs), "clips", flush=True)
