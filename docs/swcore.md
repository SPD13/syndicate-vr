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

- What drawing the things in view changed in the game is game code now,
  `game_view_pass()` (`thing_view.c`), run every frame before drawing, in
  the order drawing visits the things; the renderer only reads it:
  - the faces of moving objects (vehicles, a tank's gun, mech parts):
    the darkening near the map edge goes into `GFlags` bits 2-4, and bit 2
    (`FGFlg_Unkn04`) makes line of sight ignore the face
    (`do_vect_colide()`); the hit flash goes into `WalkHeader` (which
    walking and `people.c` read), and the thing's hit flag is cleared
    (`thing_view_object_faces()`);
  - the mech parts: positions of the part things, their matrices and the
    rocket launch points (`mech_view_update()` in `vehicle.c`).
- Enlisting the things in view (`engine_draw_things()`) still runs without
  drawing, because of an original bug: for a triangle face,
  `do_vect_colide()` reads `game_object_faces3[-face]`, before the start of
  the array, in `game_object_points`, whose `PointOffset` and `Flags`
  enlisting writes (screen point numbers in drawing order). So what was
  drawn, and the resolution, change what people see. That cannot be done
  without the original renderer. The fix `SWCORE_FIX_LOS_FACE_READ` (on by
  default since 2026-11-24, `SWCORE_FIXES_DEFAULT`; off with
  `swcore_set_fixes(0)` or the option `--original-bugs`) reads the right
  face instead. With it, 4 of the 52 traces differ from
  the original (0,20+bot 1,19+bot 3,61+bot 3,61+t_ship+t_fly), and
  nothing of the renderer runs without drawing: the 52 traces without
  drawing then equal those with drawing (pointer away,
  `SWARS_DIFFTEST_NOPOINTER=1`). So with the fix the game core needs no
  renderer.
- `check_mouse_overlap()` finds the thing under the pointer while the
  sprites are enlisted (player `Target`), which the panels and the input
  use.
- The panels (`draw_hud()`) stay on: they keep the player's locked target
  (`field_102`), from which input builds shoot packets.
- The camera transform (`prepare_drawlist()`) stays on: the pointer uses it.
- Left out: the floor, stars, explosions, rasterising the draw list,
  texture animation.
- The renderer draws random numbers from `LbRandomPosShort()`, whose
  seed `lbSeed_ps` the game rules do not use.
- Render-only state, left out of `game_state_hash()`
  (`state_hash_render_fields` in `game.c`): `Frame`/`Timer1` low halves,
  buildings' `DrawTurn`, rockets' temporary `MatrixIndex`, razor wire's
  `LeisurePlace` (the turn it was drawn), people's `Brightness` (from the
  map `ShadeR`, which floor drawing writes), the hit flash flag
  `TngF_Unkn01000000` of vehicles, buildings and rockets (drawing clears
  it). The drawing values stay in a thing slot when it is reused for
  another type, so `DrawTurn`, `MatrixIndex` and `LeisurePlace` are left
  out for every type that has no game state there.

## Core and front end

The goal: the game core (rules, missions, world) links without the SDL
display, input devices, the software renderer, the menus and sound, with
only `src/swfe.h` between them. `swfe.h` lists the services the core calls
a front end for (sound, messages and panels, brightness, debug drawing);
none of them may change the game state. Sound is output only: no random
draws, and the one rule that asks whether a sound plays (the spider mech's
loop) only decides whether to start it again (`swfe_sample_is_playing()`).

Tools (on the objects of a `--harness` build):

- `python tools/decomp-agent/coredeps.py build/harness/src [--detail group]`
  lists the front-end symbols (groups render, menus, shell, input, sound,
  network, test) that core objects reference. Every object not in a group
  is core.
- `python tools/decomp-agent/refgraph.py build/harness/src <root>...
  [--stop sym,...] [--why sym]` follows references function by function
  from roots (e.g. `process_things process_packets init_game`); `--why`
  prints the path to a symbol.
- `python tools/decomp-agent/movefuncs.py <from.c> <to.c> name...` moves
  whole definitions (with their comment and `_asm` reference) between
  files.

- `tools/decomp-agent/coreprobe.sh [build]` links the core objects alone
  with the null front end (`src/swfe_null.c`) and lists what is still
  missing, and which members of `libswrender` and `libbullfrog` the core
  pulls in (`build/coreprobe/members.txt`).

Done so far: the mission setup and world functions of `game.c` are in
`game_world.c` (core; `game.c` is the shell: main loop, screens, menus);
the player input state is in `plyr_usrstate.c` (core; reading keys stays in
`plyr_usrinp.c`); the core's global state that was defined in front-end
files is in `game_state.c`. Running the whole application (options, config
file, screens, the main loop behind `swcore_init()`/`swcore_step()`) is in
`swapp.c`, part of the desktop front end; `swcore.c` keeps the core's part
of the interface. The core reaches front-end code only through the
functions of `swfe.h` (about 370 references before), sound included: it
does not call the sound library. `swfe_null.c` implements them as nothing
(no sound playing, no key pressed); with it, the core objects link with
only two symbols missing (`difftest_skip_turn_wait`, the game speed in test
runs; `low_trans_grey_bright_limit`, a table of the renderer). The network
layer stays with the core (no devices; multiplayer is out of scope).

What the core still needs from the desktop front end and the libraries:

- The camera and the mission flow are in the shell's input code
  (`game.c`): following the selected agent (`track_player()`,
  `move_camera()`), the mission status (`test_missions()` in
  `load_packet()`) and the mission end. The game's camera belongs to the
  core: the view pass (`game_view_pass()`) sets the face flags line of
  sight reads only for vehicles in the camera's view, and the view area
  (`render_area_a/b`, `overall_scale`) follows the screen size.
- Setting up and stepping a mission without the shell: the core's parts of
  `game_setup()`, `setup_host()`, `prep_single_mission()` and
  `game_process_turn()`.
- `libswrender` holds world data the core uses: objects, faces and
  textures (`enginsngobjs`, `enginsngtxtr`, `enginpriobjs`), lights,
  explosion faces, shrapnel, animation frames (`frame_sprani`), the camera
  and zoom (`engincam`, `enginzoom`). Debug drawing and the scanner pull its
  drawing code in.
- `libbullfrog`'s platform layer (SDL): `lbDisplay` (screen size, pointer),
  logging time, the mouse sprite.

## Still in the library

The library still contains the SDL display, input, sound, the software
renderer and the menus. The next steps take those out of the core, one
at a time, behind the interface:

1. Rendering: move the software renderer to the SDL front end. The line
   of sight fix is on by default; with `--original-bugs` the core still
   needs the software renderer's enlisting, so that mode stays a feature
   of the desktop front end, for comparisons with the original.
2. Input: the front end gives commands; keyboard and mouse handling move
   to the SDL front end; the pointer target as a command.
3. Sound and menus. Game rules read sound state: `IsSamplePlaying()` in
   `people.c`, `thing.c`, `vehicle.c` decides whether to start sounds.
4. The VR viewer on the library instead of shared memory (plan step 2).

## Verification

The trace scripts start the game with `--original-bugs`, so the traces
compare with the original. The traces of `swcore_headless` must equal the
reference traces of the original asm, and its printed hashes those traces:

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
