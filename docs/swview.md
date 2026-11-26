# The world as a front end draws it: `libswarsview`

`src/libswarsview.a` (interface `src/swview.h`, code `src/swview.c`) gives
a front end that draws the game in its own way (the VR viewer, a modern 3D
renderer, the web later) what it needs to draw, read from the game core
(`swcore.h`) and the engine's world data (`libswworld`). It only reads the
game: running it never changes play. It needs nothing from the desktop
front end (`libswarsfe`) or the software renderer (`libswrender`), so it
links with the core alone (`swcore_coreonly`), and has no threads, shared
memory or OS calls (file access goes through bflib), so it can build for
the web.

It was the VR spectator's export (`vrspectate.c` of branch `vr/spectator`,
where the desktop game published it in shared memory for a viewer
process, layout `vrshare.h`, the same as the block's).

## The VR viewer with the game inside (`--mission`)

`vr/viewer/main.cpp` (`vr\run_vr.cmd C,M`) runs the game core of
mission `--mission C,M` (default 0,1) in its own process (`CoreWorld`): `swcore_core_init()`,
`swview_load_assets()`, then `swcore_core_step()` on a fixed clock
(`--turn-ms`, default 50) and `swview_update()` into its own block, read
by `BlockReader`. Commands go to `swcore_command()`. Enter confirms
the end of a decided mission; the viewer quits 3 s after it ends. It
links `libswarsview`, `libswarscore`, `libswarsnull` (`swfe_null.c`, no
sound yet), `libswworld` and the platform libraries of `build/x64`
(`vr/viewer/build.sh`), not the renderer. Game data from `--data DIR`,
default `$SWARS_GAMEDATA`.

`--dump-block TURN FILE` writes the block of that turn: with the same
`SWARS_SEED`/`SWARS_DIFFTEST_FAST` settings it equals the `block.bin` of
`swcore_coreonly --view --view-dump TURN DIR -m C,M` (0,1 turn 200,
1,85 and 0,101 turn 600: identical).

Two looks, switched at any time with L (or B on the right controller),
or chosen with `--look classic|upgraded`: the classic one draws people
with the game's sprites; the upgraded one (`vr/viewer/look_upgraded.h`,
`gltf_model.h`) with the 3D models of the assets repository (`--assets
DIR`, default `$SWARS_ASSETS3D` or `assets3d` next to the repository,
only read). Models load on a worker thread when their person type first
appears; until then, and for types without a model, people stay sprites.
Each person's clip follows `swview_persons_render()` (`anim_mode`,
weapon, speed; table of the assets' `HANDOVER.md` 2.2) with crossfades;
the weapon in hand comes from `models/weapons/held` on the right hand's
socket of `agent_arsenal.glb`; on two-handed weapons, during `rifle_*`
clips, a two-bone IK of the left arm puts the hand on the weapon's
`lhand_` node, with that node's orientation (blended over 0.1 s).
Items lying on the ground, grenades and mines, and scenery statics are
their models too (`swview_props()`: dropped weapons are the held models,
the others sized by `models/scale.json`, statics by their animation as in
`statics.json`; S2/S4 ground dots stay sprites). Explosions, fires, smoke,
gas and impacts are particle effects (`vr/viewer/fx.h`, from
`effects/effects.json`, behaving as the assets' reference simulator
`particles_sim.py`), started from `swview_fx_events()` (scale effects by
their animation, `StartFrame + 1`), burning people (flag) and burning
barrels (static S30); the sprites they replace are hidden. A shielded
person gets a glowing shell (the body pushed out 6 cm, fresnel rim and
bands, additive); a burnt one, an ash pile once the burning clip is over.

The viewer draws the whole map around the table (`--table-only`, the
right stick click or C: only the table's square). Its fog of war
(`vr/viewer/fog.h`, on by default; `--no-fog`, the right grip or G) shows
the world around the player's agents only: a circle around each living
agent, half as wide as the desktop window at its furthest zoom
(`view_scale_min`; `--fog-scale`, `--fog-radius`), the circles joining, their outer edge
dashed on the ground. Ground and buildings seen before stay grey under a
slow smoke, without people, vehicles or effects; the rest is dark ground.
Without the fog, the dashes show the most the desktop window can see.
All options of the viewer: `docs/vr-viewer-options.md`.

Its VR HUD (`vr/viewer/hud_vr.h`) reads `swview_hud_info()` and
`swview_persons_render()` every turn: a wrist panel (agents with health,
energy and shield, the selected agent's weapons, the mission's outcome),
the objective board at the table's edge, and world markers of the assets
repository's `HANDOVER.md` 7.2: M1 selection (ring with turning ticks,
number plate), M2 hover (brackets), M3 lock (brackets on the selected
agent's target shrinking with its `weapon_timer`), M4 health (bar with a
trail after a hit; agents always, others when hit or pointed at), M6
persuaded (diamond, ring, tether to the persuader), M7 waypoint (the last
walk order: rings and chevrons), M8 damage (numbers rising from who was
hit), M9 objective (a light column over each scanner signal) and M5
charge (`swview_charges()`: a ring of 32 segments at the waist filling
while a laser, electron mace, graviton gun or devastator charges).
Markers are drawn twice: hidden by walls, then at 25 % over everything.

## The world block (`swview_update()`)

`swview_update(block)` fills a block of `swview_block_size()` bytes, laid
out as the `SWV_OFF_*` offsets, once per game turn:

| Section | What | When written |
|---|---|---|
| static verts | terrain (always the first 127×127×6 verts), then the buildings, as a triangle list (`SwvVert`: position, UV in a texture page, palette colour, light, flags) | map change, every 16 turns, or when buildings or the ground change (`static_rev`) |
| dyn verts, dyn groups | vehicles, tank turrets, mech parts, rockets, rotors, mounted guns, debris, shrapnel; groups give each moving thing's verts, to interpolate | every turn |
| things | people as sprite animation state (`SwvThing`) | every turn |
| effects, lines, glows, bars, shadows | items and effects as sprite frames, lasers and other line effects, glows, health bars, vehicle shadows | every turn |
| palette, texture pages | the 18 pages of 256×256 palette indices | map change, every 128 turns (`tmap_rev`) |
| sprites | decoded sprites packed in pages, the animation tables (`nstart`, frames, elements) | map change (`sprite_rev`) |
| HUD | the player's agents, objective text, weapon names | every turn |

Positions are in map units (256 per tile), Y engine height on the same
scale; the axes are left-handed, as in the game.

## For a renderer with its own models (`SWVIEW_API_VERSION` 2)

The upgraded look (the 3D models of the assets repository, `HANDOVER.md`
there) needs the state behind the sprites:

- `swview_persons_render()`: per person the type, animation state
  (`AnimMode`, `ANIM_PERS_*`), weapon in hand, facing, frames since the
  animation began, shield brightness, persuaded (and by whom), burning,
  inside a building or vehicle, dead, player agent, selected.
- `swview_fx_events()`: the effects that started or ended in the last
  turn: bangs (size from their smoke and shards: the game keeps no bang
  type), fires starting and going out, scale effects with their animation,
  smoke generators.

Since version 3:

- `swview_props()`: items lying on the ground, grenades and mines, scenery
  statics, with the id of their `SwvEffect` in the block, their weapon type
  (items, grenades) or animation (statics, `StartFrame + 1`).
- `swview_charges()`: the player's agents charging a weapon, and how far
  (0..15, as the game's `WeaponTimer` while `TngF_WepCharging`).

Since version 4:

- `SwvHeader.view_scale_min`: the game's view scale at its furthest zoom
  (the longest-range weapon's zoom, limited by `ZoomMin` of `rules.ini`).
  With `view_size` it gives the most ground the desktop game can show; the
  VR viewer outlines that area around the game camera.

## The panels' data (`swview_hud_info()`)

The data of the game's panels without their layout, so each front end
arranges its own HUD (the VR wrist panel, objective board and world
markers): per agent health, energy, shield, weapons, current weapon, its
charge or re-fire delay, mods, target; the selected agent; the places the
scanner points at for the current objective; mission status; objective
text.

## Assets (`swview_load_assets()`)

The classic look needs the texture pages and the mission's sprites. The
export uses the game's own when the desktop renderer loaded them; a front
end without it calls `swview_load_assets()` after the core's setup, which
loads them from the game data into the library. Differences from the
desktop's, by design or for now:

- Texture pages 4 to 6 hold what the desktop renderer draws at run time
  (screens on buildings, `embedanim.c`; the Breakout screen; the shadows
  of the sprites); on the core alone they are the files' content.
- The desktop animates textures by rewriting `game_textures`
  (`animate_textures()`); on the core alone the UVs stay at the first
  frame, and a front end animates them itself.
- The light the floor drawing writes (`ShadeR`, which also gives people
  their brightness, `calc_lighting()`) and lights it updates while
  drawing: on the core alone some verts and people have the light of the
  last setup instead.
- Flat terrain colour and the view area (`view_size`): the values the
  desktop would set (`game_setup_stuff()`, a 640×480 view).

## Time

Without `SWARS_DIFFTEST_FAST` the game's timers follow real time
(`fifties_per_gameturn`, `game_speed.c`), as in the original: a turn that
takes longer changes play. Comparisons of runs set it (`statetrace.sh`);
a front end that wants runs to replay exactly must step turns on a fixed
clock.

## Checks

- `python tools/decomp-agent/coreturn_compare.py harness --jobs 2
  --core-only --view`: the core alone with the export running every turn
  plays as the desktop without it (50 scenarios).
- `python tools/decomp-agent/coredeps.py build/harness/src`: "view uses 0
  front-end symbols".
- `swcore_headless --view --view-dump TURN DIR` (desktop) and
  `swcore_coreonly --view --view-dump TURN DIR` (core alone) write the
  export of a turn (`block.bin`, `persons.bin`, `fx.bin`, `hud.bin`) to
  compare; `--view` also prints the effect events counted at the end.
