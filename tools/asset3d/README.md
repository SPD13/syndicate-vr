# asset3d: sprites to 3D assets (Tripo3D, Mixamo, Blender)

These tools rebuilt the game's 2D sprites as 3D models: people (rigged,
animated), handheld weapons, dropped/thrown items and the scenery objects
placed on the maps. The generated assets live in a separate **private**
repository (they derive from the game art); locally in
`../assets3d` next to the game data.

**Integrating them into the engine: read `HANDOVER.md` in the assets
repository first.** It lists every asset, the conventions (axes, scale,
skeleton), the clips and how they map to the game's `ANIM_PERS_*` states,
weapon attachment with left-hand IK, the scale of items and scenery, and
how effects such as the energy shield are meant to be done (shaders).

Each script documents its usage in its header. Main entry points:

| Script | Does |
|---|---|
| `extract_sprites.py`, `prepare_refs.py`, `swdata.py` | read the game sprites and animation tables |
| `pipeline.py`, `build_character.py` | character: redraw, views, model, rig, clips, weapons, review |
| `make_clips.py`, `blender_merge_anims.py`, `rebuild_clips.py` | Mixamo clips merged onto every rig |
| `blender_cape2.py` | generated cape for the Church agent (`cape2_zealot.json`) and the High Priest (`cape2_high_priest.json`): cloth simulation against the animated body, baked into bone chains; options in metres are scaled to the model's height |
| `props.py`, `props.json` | weapons, items and scenery: reference, redraw, model |
| `blender_attach_weapons.py`, `blender_export_held_weapons.py` | weapons in the right hand, left-hand IK target, runtime weapon GLBs |
| `inventory_statics.py` | the static (scenery) sprites placed in the maps |
| `blender_render_clips.py`, `make_gifs.py`, `rerender_reviews.py`, `make_pages_index.py` | review pages (`review/pages.html` indexes them) |
| `blender_shield_preview.py` | energy shield look (shader recipe) |
| `tripo.py` | Tripo API client with a ledger (no task is paid twice) and a credit cap |

The Tripo API key is read from the environment (`TRIPO_API_KEY`) and is
never printed or stored.
