"""Index of every review page under assets3d/review/, grouped, with status.

python make_pages_index.py
Writes review/pages.html. The list is curated here: add a line when a new
review page is made. Pages missing on disk are shown greyed out.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import swdata  # noqa: E402

GROUPS = [
    ("Overview", [
        ("index.html", "All 13 characters, one still each, links to their animation pages", "current"),
    ]),
    ("Characters - animation reviews (final models, 24 clips + weapon variants, front/side/back)", [
        ("agent", "A - EuroCorp agent (agent_armed.glb)", "current"),
        ("zealot_cape", "B - Church of the New Epoch agent, generated cape (zealot3_cape_armed.glb)", "current"),
        ("police", "1 - Police", "current"),
        ("mercenary", "2 - Mercenary", "current"),
        ("scientist", "3 - Scientist (no gun pose in the game, can still carry one)", "current"),
        ("high_priest", "4 - High Priest: Tripo presets idle/walk/run/hurt/fall + cape, no weapons", "current"),
        ("briefcase_m", "5 - Briefcase man (no gun pose in the game)", "current"),
        ("shady_m", "6 - Shady man", "current"),
        ("leth_jackt_m", "7 - Leather-jacket man", "current"),
        ("punk_m", "8 - Punk (male)", "current"),
        ("punk_f", "9 - Punk (female)", "current"),
        ("white_brun_f", "10 - Brunette woman", "current"),
        ("whit_blond_f", "11 - Blonde woman (also the fast blonde woman)", "current"),
    ]),
    ("Clip previews on the agent (choices made)", [
        ("agent_newclips", "pickup as a squat (Mixamo Lifting Object, spliced) + throw - pickup REJECTED", "superseded"),
        ("agent_newclips2", "pickup = Mixamo Picking Up (bend to the ground) + throw (Toss Grenade) - CHOSEN", "current"),
        ("agent_newclips3", "persuaded_a / persuaded_b (Zombie Transition) + burning (Shaking It Off) - persuaded_a CHOSEN", "current"),
        ("agent_shield", "Energy shield look: fresnel rim, electric lines, pulse (shader preview, not an asset)", "current"),
    ]),
    ("Weapons", [
        ("weapons_redraw", "W1-W14 redraws next to the 1996 shop renders (W10 v2)", "current"),
        ("weapons_all", "All 16 weapons held by the agent: fist fit + left hand on top of two-handed ones", "current"),
        ("weapons_agent", "History 1: first attach (frames brightened, model no longer exists)", "brightened"),
        ("weapons_agent2", "History 2: persuadertron turned over (frames brightened)", "brightened"),
        ("weapons_agent3", "History 3: brighter renders, PNG", "superseded"),
        ("weapons_agent4", "History 4: electron mace + flamer held by the pistol grip", "superseded"),
        ("weapons_agent5", "History 5: persuadertron levelled, launcher upright", "superseded"),
        ("weapons_agent6", "History 6: handle through the fist, left hand on the body (hand still off on some)", "superseded"),
        ("weapons_agent7", "History 7: handle seated at the index finger (8 weapons)", "superseded"),
    ]),
    ("Dropped and thrown items", [
        ("items_redraw", "I1-I14 redraws next to the shop renders", "current"),
        ("items_models", "I1-I15 3D models from four sides (I10 rebuilt with 4 arms, I15 ash pile)", "current"),
        ("ashpile_redraw", "I15 ash pile redraw next to the game's ash frame", "current"),
    ]),
    ("Scenery (static map sprites)", [
        ("statics", "Inventory: the 32 static sprites placed in the maps, S numbers, use counts", "current"),
        ("scenery_redraw", "22 scenery redraws next to their sprites (S numbers)", "current"),
        ("scenery_models", "22 scenery 3D models from four sides", "current"),
    ]),
    ("Effects (particles and shaders, not assets)", [
        ("effects", "Fires and explosions simulated from effects.json: small / large (with shrapnel secondaries) explosion, ground fire, burning person, barrel fire", "current"),
        ("agent_shield", "Energy shield shader look on the agent", "current"),
    ]),
    ("Style checks and history", [
        ("characters_v1", "First one-pose redraws of the 11 remaining characters (style validation)", "superseded"),
        ("zealot", "Church agent v1: scanned drapes on the Tripo rig (re-rendered bright)", "superseded"),
        ("zealot_cloth", "Church agent v1 with the drape cloth rig (re-rendered bright)", "superseded"),
        ("zealot2", "Church agent v2: back fixed with edit-multiview (re-rendered bright)", "superseded"),
        ("zealot2_cloth", "Church agent v2 with the drape cloth rig (re-rendered bright)", "superseded"),
    ]),
]


def main():
    rev = os.path.join(os.path.dirname(swdata.find_gamedata()), "assets3d", "review")
    css = ("body{background:#1d1e22;color:#ddd;font:15px system-ui;margin:24px}a{color:#9cf}h2{margin-top:28px}"
           "table{border-collapse:collapse;width:100%}td{padding:5px 10px;border-bottom:1px solid #333}"
           ".s{font-size:12px;padding:2px 7px;border-radius:9px}.current{background:#2d5a36}"
           ".superseded{background:#555}.brightened{background:#6a5a2a}.miss{color:#666}")
    out = ["<!DOCTYPE html><html><head><meta charset='utf-8'><title>All review pages</title><style>%s</style>"
           "</head><body><h1>All review pages</h1><p>Every page made while building the 3D assets. "
           "<b>current</b> = the final state; <b>superseded</b> = kept to show how we got there; "
           "<b>brightened</b> = old frames lightened (the model of that step no longer exists).</p>" % css]
    for title, pages in GROUPS:
        out.append("<h2>%s</h2><table>" % title)
        for page, desc, status in pages:
            href = page if page.endswith(".html") else page + "/index.html"
            ok = os.path.exists(os.path.join(rev, href))
            link = "<a href='%s'>%s</a>" % (href, page) if ok else "<span class=miss>%s (missing)</span>" % page
            out.append("<tr><td>%s</td><td>%s</td><td><span class='s %s'>%s</span></td></tr>" % (link, desc, status, status))
        out.append("</table>")
    out.append("</body></html>")
    open(os.path.join(rev, "pages.html"), "w", encoding="utf-8").write("\n".join(out))
    print("pages:", os.path.join(rev, "pages.html"))


if __name__ == "__main__":
    main()
