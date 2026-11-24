# The game library (libswarscore)

Since 2026-11-23 the game is built as a library, `src/libswarscore.a`,
with a small C interface in `src/swcore.h`. Front ends link it and drive
the game one turn at a time. The goal is one verified game core for every
target (desktop, VR, mobile) with a front end per target. Multiplayer is
out of scope for now: the library is for solo play.

## Front ends

- `syndwarsfx` (`src/swmain.c`): the SDL desktop game. Its `main()` only
  calls `swcore_init()`, `swcore_step()` until it returns 0, and
  `swcore_shutdown()`. It links the whole library (`--whole-archive`), so
  the executable contains what it contained before.
- `swcore_headless` (`src/swcore_headless.c`, builds without asm only):
  a test front end that links the library as any front end would. It
  takes the `syndwarsfx` options, reads the world through the interface
  every turn, and prints `<turn> <state hash>`. `--turns N` stops after N
  turns; `--no-draw` leaves out drawing the world; `--world` prints a
  summary of the world every 100 turns.

## Interface (SWCORE_API_VERSION 1)

| Function | Does |
| --- | --- |
| `swcore_init(argc, argv)` | Sets the game up from the `syndwarsfx` options (e.g. `-m 1,2` starts a mission) |
| `swcore_step()` | One game turn; 1 while the game runs, 0 once it asked to exit |
| `swcore_shutdown()` | Frees what the game set up; it cannot be set up again |
| `swcore_set_world_drawing(on)` | Draws the world in missions or not; the game plays the same |
| `swcore_game_turn()`, `swcore_state_hash()` | Turn number and the hash the state traces compare |
| `swcore_world_info()` | Mission status, map, selected agent, camera, objective |
| `swcore_things()` | People, vehicles, buildings and effects: position, type, health, group, flags |
| `swcore_agents()`, `swcore_weapon_name()` | The player's agents and their weapons |
| `swcore_command()` | Queues a player command (select agent, go to, shoot, change weapon, enter vehicle) |

Commands are executed as the packets the mouse would build, one per turn,
in the turns the player gives no input of their own (`load_packet()` calls
`swcore_input_packet()`, in `src/swcore_world.c`). They come from the
VR spectator's command queue (`vrspectate.c` on `vr/spectator`).

The game keeps its state in globals: one game per process. The interface
does not show the game's structs; it depends only on `<stdint.h>`.

## Drawing and the game state

`swcore_set_world_drawing(0)` (`swcore_headless --no-draw`) leaves out
drawing the world in missions. The game plays the same: the 52 traces
with and without drawing are equal. Finding that out showed where the
original drawing code changes the game, which a front end with its own
renderer must keep in mind:

- Enlisting the things in view (`engine_draw_things()`) stays on without
  drawing, as it is part of the game:
  - `draw_rot_object_faces()` writes the faces of moving objects
    (vehicles): the darkening near the map edge goes into `GFlags` bits
    2-4, and bit 2 (`FGFlg_Unkn04`) makes line of sight ignore the face
    (`do_vect_colide()`); the hit flash goes into `WalkHeader`.
  - `check_mouse_overlap()` finds the thing under the pointer while the
    sprites are enlisted (player `Target`), which the panels and the input
    use.
- The panels (`draw_hud()`) stay on: they keep the player's locked target
  (`field_102`), from which input builds shoot packets.
- The camera transform (`prepare_drawlist()`) stays on: the pointer uses it.
- Left out: the floor, stars, explosions, rasterising the draw list,
  texture animation.
- Render-only state, left out of `game_state_hash()`
  (`state_hash_render_fields` in `game.c`): `Frame`/`Timer1` low halves,
  buildings' `DrawTurn`, people's `Brightness` (from the map `ShadeR`,
  which floor drawing writes), the hit flash flag `TngF_Unkn01000000` of
  vehicles, buildings and rockets (drawing clears it).

## Still in the library

The library still contains the SDL display, input, sound, the software
renderer and the menus. The next steps take those out of the core, one
at a time, behind the interface:

1. Rendering: the enlisting side effects above as game code of their own
   (a pass over the things in view that does not need the renderer), so
   the software renderer can move to the SDL front end.
2. Input: the front end gives commands; keyboard and mouse handling move
   to the SDL front end; the pointer target as a command.
3. Sound and menus. Game rules read sound state: `IsSamplePlaying()` in
   `people.c`, `thing.c`, `vehicle.c` decides whether to start sounds.
4. The VR viewer on the library instead of shared memory (plan step 2).

## Verification

The traces of `swcore_headless` must equal the reference traces of the
original asm, and its printed hashes those traces:

```
SWARS_TRACE_EXE=swcore_headless python tools/decomp-agent/statetrace_win_par.py harness build/tr_lib --jobs 6
SWARS_TRACE_EXE=swcore_headless SWARS_TRACE_EXE_ARGS=--no-draw python tools/decomp-agent/statetrace_win_par.py harness build/tr_lib_nd --jobs 6
```

`statetrace_win_par.py` runs the 52 scenarios of `statetrace.sh` in
parallel on Windows and compares them with `build/tr_difftest` (made from
the difftest build with `--no-compare`). On Linux use `statetrace_par.sh`
and `trace_compare.py`.

The trace runs use SDL's dummy video and audio drivers (`statetrace.sh`
sets them unless given; `msys.sh` passes `SDL_*` variables on). With a
real window the desktop's mouse pointer could lie over the game, which
then picks the thing under it: that made Windows runs drift under load.
`SWARS_DIFFTEST_NOPOINTER=1` moves the pointer off the screen every turn.
