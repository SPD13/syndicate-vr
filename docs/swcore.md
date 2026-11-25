# The game library (libswarscore)

Since 2026-11-23 the game is built as a library, `src/libswarscore.a`,
with a small C interface in `src/swcore.h`. Since 2026-11-24 the library
is the game core alone; the desktop front end is `src/libswarsfe.a`. Front ends link it and drive
the game one turn at a time. The goal is one verified game core for every
target (desktop, VR, mobile) with a front end per target. Multiplayer is
out of scope for now: the library is for solo play.

## Front ends

- `syndwarsfx` (`src/swmain.c`): the SDL desktop game. Its `main()` only
  calls `swcore_init()`, `swcore_step()` until it returns 0, and
  `swcore_shutdown()`. It links both libraries whole (`--whole-archive`),
  so the executable contains what it contained before.
- `swcore_headless` (`src/swcore_headless.c`, builds without asm only):
  a test front end that links the library as any front end would. It
  takes the `syndwarsfx` options, reads the world through the interface
  every turn, and prints `<turn> <state hash>`. `--turns N` stops after N
  turns; `--no-draw` leaves out drawing the world; `--core-turn` steps
  with `swcore_core_step()` instead; `--status` prints `# turn N status S`
  when the mission status changes; `--world` prints a summary of the world
  every 100 turns.
- `swcore_coreonly` (the same source built with `SWCORE_CORE_ONLY`, builds
  without asm only): the game core alone, linked with `libswarscore.a` and
  the null front end `src/swfe_null.c`, without the desktop front end
  (no display, input, sound, menus). It starts the mission of
  `-m campaign,mission` with `swcore_core_init()` and plays it with
  `swcore_core_step()`; of the game's options it reads only that and
  `--original-bugs`/`--fix-bugs`. It links the engine's world data
  (`libswworld`), not its drawing (`libswrender`), and `libbullfrog`
  (SDL) for helpers the core takes from it.

## Interface (SWCORE_API_VERSION 4)

| Function | Does |
| --- | --- |
| `swcore_init(argc, argv)` | Sets the game up from the `syndwarsfx` options (e.g. `-m 1,2` starts a mission) |
| `swcore_step()` | One game turn; 1 while the game runs, 0 once it asked to exit |
| `swcore_core_step()` | One turn of the game core only, in a mission: no input devices, screens or drawing; 0 once the mission is over, outside a mission or once the game asked to exit (API 2; plays the mission's end since API 4) |
| `swcore_shutdown()` | Frees what the game set up; it cannot be set up again |
| `swcore_core_init(campaign, mission)` | Instead of `swcore_init()`: sets up the game core alone (data in the current directory, default settings) and starts the mission (API 3) |
| `swcore_core_shutdown()` | Frees what `swcore_core_init()` set up (API 3) |
| `swcore_set_world_drawing(on)` | Draws the world in missions or not; the game plays the same |
| `swcore_game_turn()`, `swcore_state_hash()` | Turn number and the hash the state traces compare |
| `swcore_world_info()` | Mission status, map, selected agent, camera, objective |
| `swcore_things()` | People, vehicles, buildings and effects: position, type, health, group, flags |
| `swcore_agents()`, `swcore_weapon_name()` | The player's agents and their weapons |
| `swcore_command()` | Queues a player command (select agent, go to, shoot, change weapon, enter vehicle) |
| `swcore_mission_conclude()` | The player confirmed the end of a decided mission (the desktop's Enter or Esc on the end message): the next core turn ends it (API 4) |

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
  test) that core objects reference. The core objects are those of
  `libswarscore.a`, the front end those of `libswarsfe.a`
  (`libswarsfe_a_SOURCES` in `src/Makefile.am`); a module in the wrong
  library for its group is reported. `difftest.c` is core: the core's
  `DIFFTEST_*` macros need it.
- `python tools/decomp-agent/refgraph.py build/harness/src <root>...
  [--stop sym,...] [--why sym]` follows references function by function
  from roots (e.g. `process_things process_packets init_game`); `--why`
  prints the path to a symbol.
- `python tools/decomp-agent/movefuncs.py <from.c> <to.c> name...` moves
  whole definitions (with their comment and `_asm` reference) between
  files.

- `tools/decomp-agent/coreprobe.sh [build]` links the core objects alone
  with the null front end (`src/swfe_null.c`) and lists what is still
  missing, and which members of `libswworld`, `libswrender` (none) and
  `libbullfrog` the core pulls in (`build/coreprobe/members.txt`).

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
(no sound playing, no key pressed); with it, the core links alone
(`swcore_coreonly`). The network layer stays with the core (no devices;
multiplayer is out of scope).

Setting up the game and a mission: the core's steps are in
`game_coreinit.c`. The desktop's `game_setup()`, `setup_host()` and
`prep_single_mission()` call them between their own steps (screen,
sprites, fonts, sound, user settings, replays), in the order they had:
`game_core_setup_data()` (engine memory, the animation files, rules and
config files, explosions, fire), `game_core_setup_memory()` (the game's
arrays), `game_core_setup_world()` (things, people stats, campaigns,
vehicle objects, the starting camera), then `game_core_mission_load()`
(the campaign's missions and texts) and `game_core_mission_init()` (map,
level, player, the preprocessed turns). `swcore_core_init()` calls them
in a row, with the option `-m` and the default settings of the core
(`set_default_player_control()`, the visual preferences), all data in the
data folder. Found on the way:

- The animation files (`mele-0.ani`, `nsta-0.ani`, `nfra-0.ani`) are game
  data: people's frames decide the timing of their actions. They are
  loaded by the core now, not with the sprites.
- Level loading moved floor and face textures whose atlas page the
  software renderer had not loaded to page 0 (`fix_map_outranged_properties()`);
  it now asks the game data how many pages it has
  (`texturemap_page_exists()`), which gives the same answer.
- `stop_looped_weapon_sample()` clears the person's flag of a looped
  weapon sound playing (`TgF2_Unkn0200`), which the game rules read: it was
  a sound service of the front end, so without one the flag stayed set
  (mission 1,2 differed from turn 740). It is in the core now (`weapon.c`),
  calling only `swfe_sample_release_looped()`.
- The palette is game data too: sparks and other effects keep their
  colour (`colour_lookup[]`) in the thing, which the state hash covers. The
  core loads `qdata/pal.pal` and sets up the colour lookups
  (`setup_color_lookups()`).
- The game's view is game state: the view pass sets the face flags that
  shots (`bul_path_end()`) and line of sight read only for the vehicles in
  the view area (`render_area_a/b`, tiles around the camera) and the camera
  scale (`overall_scale`), which the desktop sets from the screen size when
  a mission starts (`game_core_view_setup()`, from
  `adjust_mission_engine_to_video_mode()`). The core alone uses the
  desktop's game screen size, 640x480 (view area 64, scale 334); with a
  view area of 24 a minigun shot hit something else in mission 1,16.
- Without the software renderer there are no textures: the rain and snow
  effects still draw their random numbers but leave the texture alone, and
  the mission start reloads `tex00.dat` only if it is loaded.
- Front-end code that sat in core files: `SCANNER_init()` (now in
  `hud_panel.c`), the collision debug drawing of `tngcolisn.c` (now in
  `thing_debug.c`); `low_trans_grey_bright_limit`, a table of
  `libswrender`, is defined in `game_state.c`.

What the core still needs from the desktop front end and the libraries:

- The view area (`render_area_a/b`, `overall_scale`) follows the screen
  size; the core alone fixes it at the desktop's 640x480. A front end with
  another view still plays the game of that view.
- The engine's world data is its own library, `libswworld.a` (in
  `swrendersoft/`, beside the drawing in `libswrender.a`): objects, faces
  and textures (`enginsngobjs`, `enginsngtxtr`, `enginpriobjs`,
  `enginpritxtr`, `enginfloor`, `engintxtrmap`), lights, explosion faces
  (`enginfexpl`; their drawing is `enginfexpldrw`), shrapnel, animation
  frames (`frame_sprani`), the camera and zoom (`engincam`, `enginzoom`),
  colours, the scene effect (`enginpeff`: the rain on the floor; falling
  rain, snow and stars are drawn in `enginpeffdrw`), the size limits
  (`enginprops`), and the buffers the game allocates for the renderer with
  its world (`engindrwbuf`: draw lists, screen points, special faces; the
  lights option; the light of a nuclear explosion). Every program links
  `libswworld` (`SWRENDERLIB_LIBS`); only the front ends that draw link
  `libswrender`. `draw_explode()`, which enlists the explode faces for
  drawing, is in the front end (`lvdraw3d.c`); the core's debug views and
  objective marks call five drawing functions of `libswrender` through
  `swfe.h` (null in `swfe_null.c`).
- `libbullfrog`'s platform layer (SDL): `lbDisplay` (screen size, pointer),
  logging time, the mouse sprite.

### The core turn

`game_core_turn()` (`game_turn.c`, `swcore_core_step()`) is one turn of a
mission without input devices, screens or drawing. It does what the
desktop turn (`game_process_turn()`) changes in the game, in the same
order, and plays the same game: `coreturn_compare.py` (see Verification)
checks it on the trace scenarios. What it takes from the desktop turn:

- The camera (`game_camera.c`): the zoom for the agent's weapon, following
  the selected agent, scrolling and rotating with the velocities the input
  set (`game_camera_update()`, `game_camera_input()`; the shell's
  `do_scroll_map()`/`do_rotate_map()` only read keys and the pointer). The
  game's camera belongs to the core: the view pass (`game_view_pass()`)
  sets the face flags line of sight reads only for vehicles in its view.
- What showing the game screen changed in the game (`game_turn_camera()`,
  `game_turn_world_view()`, `game_turn_view_end()`): mechs, craters, the
  view pass, the BAT game, navigation counters; and what drawing the
  panels changed: the agent's flag of standing on a dropped item, cleared
  once the item is gone (`update_dropped_item_under_agent_exists()`, now
  in `people.c`; the core calls it every viewed turn, the desktop while
  the panels are visible, which they always are in play).
- The player's turn without input: the commands of `swcore_command()`, or
  else the packet the input builds when nothing is pressed (no action,
  trigger released, `input_user_control_agent()`); processing it stops the
  controlled agent's shooting, so leaving it out changes the game.
- The rain and snow on the floor textures (`scene_post_effect_prepare()`,
  in `libswworld`): the rain draws from the game's random numbers
  (`LbRandomAnyShort()`) every turn, so it is game state although it only
  changes a texture. The core alone has no texture pages (`vec_tmap`):
  the effect then draws its numbers without touching a texture, and the
  Breakout minigame (`bat.c`, which some missions start) draws into a
  page of its own nobody shows (its rules never read the pixels).
- The test hooks of the desktop turn (`difftest_bot_turn()`) through
  `game_turn_test_hook`, which the front end sets (`swapp.c`), and the state
  trace (`game_turn_statetrace()`, `SWARS_STATETRACE`). The hooks are a
  library of their own, `libswarstest.a` (the scripted player
  `difftest_bot.c` and `difftest_hook_{misc,people,transport}.c`), which
  needs nothing from the front end; `swcore_coreonly` links it and sets the
  hook. The hooks of front-end code (`m_cryo`, `m_typing`, `m_pause`:
  the cryo roster's list box, typing into a text field, the pause screen's
  sound) are in `libswarsfe` (`difftest_hook_fe.c`), called through
  `difftest_hook_fe_turn`, which `swapp.c` sets; the core alone plays the
  scenarios without these.
- The mission's end (`game_misend.c`). Once the mission is decided
  (`mission_status` of `swcore_world_info()`), it goes on until the
  mission's fade-out (`WaitToFade`) has run or the player confirms the end
  (`swcore_mission_conclude()`, the desktop's Enter on the end message);
  then the rules of `mission_over()` run: the agents' weapons are hidden,
  the world stops (`DpM_PURPLEMNU`: no more things or packets processed),
  rewards and the campaign's outcome (cash, persuaded people, mission
  state, the mails and missions it opens, the next part of a compound
  mission), game over or not. The desktop's `mission_over()` shows the
  videos and stops the sound between these steps
  (`game_core_mission_over_start/outcome/end()`), the core alone calls
  `game_core_mission_over()`. The mail queue (`femail.c`) is campaign
  state and part of the core.

## Still to do

The core library (`libswarscore.a`) no longer contains the SDL display,
input, sound, the menus or the renderer's game side, and links the
engine's world data (`libswworld`) without its drawing; it still links
`libbullfrog` (SDL platform layer). The next steps, behind the
interface:

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

The core turn must play the same game as the desktop turn, the mission's
end included:

```
python tools/decomp-agent/coreturn_compare.py harness --jobs 2
```

runs `swcore_headless` with `--no-draw` and with `--core-turn` (both with
the default fixes) and compares their hashes turn by turn, and that both
end at the same turn (the desktop's first step is before the mission, so
at the cap of `--turns` it has one mission turn less). `swcore_coreonly
--conclude N` confirms the end N turns after the decision. With `--core-only` the
core turn runs in `swcore_coreonly` instead, so the core's setup is
compared too (the scenarios without hooks of front-end code):

```
python tools/decomp-agent/coreturn_compare.py harness --jobs 2 --core-only
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
