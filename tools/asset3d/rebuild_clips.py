"""Re-merges every finished character's Mixamo clips (after clips were added to
make_clips.py) and rebuilds what depends on them.

python rebuild_clips.py [type ...]
Per character: make_clips.py -> blender_merge_anims.py -> <combat>.glb, wrist
check, then the Church agent's cape (blender_cape2.py) or a simulated
ponytail (blender_ponytail.py, characters in PONYTAIL), then the weapons
(attach_agent.json; the agent also gets attach_arsenal.json). The High
Priest is not handled: it uses Tripo's presets.
"""
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_character as bc  # noqa: E402

R = bc.ROOT
M = os.path.join(R, "models")
MX = os.path.join(R, "mixamo")
W = os.path.join(M, "weapons")
REGULAR = ["police", "mercenary", "scientist", "briefcase_m", "shady_m", "leth_jackt_m", "punk_m", "punk_f",
           "white_brun_f", "whit_blond_f"]
# type: (raw clip dir, base fbx, rigged glb, combat glb, cape json or None, armed glb, extra attach)
CHARS = {
    "agent": ("raw", "raw/pistol_pack/agent_rigged_v2.fbx", "agent/agent_rigged.glb", "agent/agent_combat.glb",
              None, "agent/agent_armed.glb", ("attach_arsenal.json", "agent/agent_arsenal.glb")),
    "zealot": ("raw_zealot3", "raw_zealot3/pistol_pack/zealot3_rigged.fbx", "zealot/zealot3_rigged.glb",
               "zealot/zealot3_combat.glb", "cape2_zealot.json", "zealot/zealot3_cape_armed.glb", None),
}
PONYTAIL = {"punk_f": None}   # type: options json (or None for the script's defaults)
for t in REGULAR:
    CHARS[t] = ("raw", "%s_rigged.fbx" % t, "%s/%s_rigged.glb" % (t, t), "%s/%s_combat.glb" % (t, t), None,
                "%s/%s_armed.glb" % (t, t), None)


def main():
    only = sys.argv[1:] or list(CHARS)
    for t in only:
        raw, fbx, rigged, combat, cape, armed, extra = CHARS[t]
        raw_d = os.path.join(MX, raw)
        if raw != "raw":  # single clips are shared: copy any the character's folder lacks
            for f in os.listdir(os.path.join(MX, "raw", "single")):
                dst = os.path.join(raw_d, "single", f)
                if not os.path.exists(dst):
                    shutil.copyfile(os.path.join(MX, "raw", "single", f), dst)
        cj = os.path.join(MX, "clips_%s.json" % (t if t != "zealot" else "zealot3"))
        bc.run([sys.executable, os.path.join(HERE, "make_clips.py"), raw_d, os.path.join(MX, fbx),
                os.path.join(M, rigged), cj])
        combat_p = os.path.join(M, combat)
        bc.bl("blender_merge_anims.py", cj, combat_p)
        out = bc.bl("blender_wrist_check.py", combat_p + "@idle", keep=("  f",))
        ups = [float(x.split("up")[1].split("|")[0]) for x in out.splitlines() if " up " in x and "f" in x.split()[0]]
        if ups and max(ups) > -60:
            raise SystemExit("%s: hands are not hanging down in idle" % t)
        src = combat_p
        if cape:
            src = os.path.join(M, "zealot", "zealot3_cape.glb")
            bc.bl("blender_cape2.py", combat_p, src, os.path.join(HERE, cape))
        elif t in PONYTAIL:
            src = os.path.join(M, t, "%s_ponytail.glb" % t)
            bc.bl("blender_ponytail.py", combat_p, src, *([os.path.join(HERE, PONYTAIL[t])] if PONYTAIL[t] else []))
        bc.bl("blender_attach_weapons.py", os.path.join(W, "attach_agent.json"), src, os.path.join(M, armed))
        if extra:
            bc.bl("blender_attach_weapons.py", os.path.join(W, extra[0]), src, os.path.join(M, extra[1]))
        print("DONE", t, "idle hands", ups, flush=True)


if __name__ == "__main__":
    main()
