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
  turns; `--world` prints a summary of the world every 100 turns.

## Interface (SWCORE_API_VERSION 1)

| Function | Does |
| --- | --- |
| `swcore_init(argc, argv)` | Sets the game up from the `syndwarsfx` options (e.g. `-m 1,2` starts a mission) |
| `swcore_step()` | One game turn; 1 while the game runs, 0 once it asked to exit |
| `swcore_shutdown()` | Frees what the game set up; it cannot be set up again |
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

## Still in the library

The library still contains the SDL display, input, sound, the software
renderer and the menus: `swcore_step()` draws and plays sound as the game
always did. For no window use `SDL_VIDEODRIVER=dummy
SDL_AUDIODRIVER=dummy`. The next steps take those out of the core, one
at a time, behind the interface:

1. Rendering: `draw_game()` behind a front end callback, so a front end
   draws the world from `swcore_things()` (some state still advances while
   drawing: `Frame` and `Timer1`, which the hash leaves out).
2. Input: the front end gives commands; keyboard and mouse handling move
   to the SDL front end.
3. Sound and menus.
4. The VR viewer on the library instead of shared memory (plan step 2).

## Verification

The traces of `swcore_headless` must equal the reference traces of the
original asm, and its printed hashes those traces:

```
SWARS_TRACE_EXE=swcore_headless python tools/decomp-agent/statetrace_win_par.py harness build/tr_lib --jobs 6
```

`statetrace_win_par.py` runs the 52 scenarios of `statetrace.sh` in
parallel on Windows and compares them with `build/tr_difftest`. Under
heavy load a few runs drift from the reference (some state follows the
drawing timing). The script reruns those scenarios alone and reports only
the ones that still differ.
