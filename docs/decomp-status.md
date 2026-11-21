# Decompilation status — 2026-11-16

Handoff note for resuming the asm-to-C work in a new session (for example
after a reboot). How to remake a function is in `CLAUDE.md`; the overall
plan is in `docs/decompilation-agent-plan.md`.

## Where things stand

- Branch `decomp/auto` (pushed to `origin` = SPD13/syndicate-vr since
  2026-11-21, user request; **never** to `upstream`), on top of
  `vr/main`.
- **State at the handoff of 2026-11-16 evening** (`decomp/auto` at
  `e1a2e8ac`): 694 asm functions, 361 shadowed (remade in C and
  difftested), 27 queued, all reached only through the new hook
  scenarios (see "Hook scenarios" below). `coverage.json` is current
  (51 scenarios, built at `d9f32963`); no need to rebuild it before
  resuming.
- The loop on the 28 hooked functions was stopped by Claude Code for low
  memory. `person_clear_route_and_teleport_to_vehicle` merged
  (`e1a2e8ac`); `process_vehicle_goto_point_land` was interrupted while
  its agent ran. Its worktree `worktrees/process_vehicle_goto_point_land`
  and branch `decomp/try/process_vehicle_goto_point_land` are left over;
  remove them before restarting (see "How to resume").
- Before that: after the 45-scenario coverage (`33a7d18b`) there were 360
  shadowed, 62 routed labels still live, 0 queued.
- 2026-11-16: suffixes `+kbd`, `+crater`, `+wreck` and a bezier-road bot
  action (see below) reached 7 more; all merged on the first pass
  (about $3.7): `check_unlock_target`, `track_player`,
  `get_target_priority`, `find_a_target`, `process_map_crater`,
  `init_mgun_explode`, `find_bezier_nav_point`.
- 2026-11-15 evening: 6 BAT targets and `nullsub_7` merged;
  `BAT_unknsub_22` was blocked by the internal label `BAT_unknsub_28`,
  routed in `f2edc6f7` (state traces of 37 scenarios identical), then
  merged. With the `+tri` scenarios and the bot's group save/restore,
  15 more merged: 10 pathtrig functions (`make_edge`, `edge_find`,
  `make_clip_list`, `make_clipped_edges`, `fill_concave`,
  `point_quad_status`, `func_81db4`, `collide_coords`, `unused_func_115`,
  `triangulation_allocate`), `unkn_player_group_add/prot`,
  `call_protect_specific`, `yank_from_vehicle` (about $8.5 in all).
- 2026-11-14 run: 38 targets, all merged after routing
  `set_player_peeps_to_run` (own epilogue `jump_57dc6`) and
  `del_col_vect_from_mapwho` in `31a4febd`; about $20.
- 2026-11-14/15 run, after the bot extension and 28 scenarios
  (`0a7a883d`): 24 targets, 23 merged on the first pass (about $11).
  `mech_unkn_func_20` was blocked: the asm takes its output vector in
  `esi`. `efd1df5e` makes the asm caller pass it in `ecx` (saved around
  the call) and the C wrapper take it as 4th argument; state traces of all
  28 scenarios identical. After a ledger reset it merged (`964b26ab`).
- Persuasion: group persuasion is only reached by mission script commands
  (`PCmd_PERSUADE_*`), never by player packets. Scenario `3,4+bot`
  (Hong Kong, zealot de-re-persuasion; `c973653d`) runs them; then
  `is_group_all_persuaded_by_me` and `person_becomes_persuaded` merged
  (`63acc138`, `25386b61`, about $0.87).
- Fuzz route: pure functions no scenario reaches get a random-input case
  in `src/difftest_fuzz.c`; `queue.py` no longer counts a function with a
  fuzz case as low-coverage. Merged this way: `save_crypto_make_hashtable`,
  `save_crypto_transform1/2/3`, `func_5cff0`, `line_intersect8`,
  `my_preprocess_text`.
- 2026-11-15 run on 8 targets after the 35 seeded scenarios: all merged
  (about $4), incl. `process_mech`.
- `my_preprocess_text` (`b1351b2f`) deliberately differs from the asm on
  the `\l` escape (insert login name): the asm finds the name end by
  reading `net_host_player_no+3+k`, which was `login_name[k]` in the
  original binary layout; in the port they are separate variables, so the
  asm inserts nothing. The C follows the original intent; the fuzz
  alphabet omits `l`/`L`. Accepted after review.
- Remaining routed labels excluded from the queue: low-coverage ones (see
  `queue.py --print 100`), 20 sound/network subsystem, 12 changed
  recently upstream (kept excluded by choice; the 30-day window lapses
  about 2026-12-01, then `queue.py` queues them by itself).
- Untracked: `tools/decomp-agent/loop.log` (the loop's log, keep it).

## Player bot scenarios (added this session)

Many functions only run when a player gives orders (packet handlers,
protect/follow, pickups, vehicles). `src/difftest_bot.c` plays the mission
with random but deterministic player commands:

- Enabled with env `SWARS_DIFFTEST_BOT=<seed>`; the scripts set it for
  scenarios written with a `+bot` suffix, e.g. `1,2+bot`.
- `SWARS_DIFFTEST_BOT_LOG=<file>` writes every packet the bot sends.
- Called from `game_process()` right after `load_packet()`; acts every 6
  turns, only in `DpM_ENGINEPLY` and only when the packet is empty.
- Default scenarios (in `coverage.sh`, `difftest_run.sh`,
  `statetrace.sh`): 45, 13 plain + 25 bot + 3 tri + 2 kbd + 2 crater
  (wreck runs combine with bot); see the `SCENARIOS` line in those
  scripts. Suffixes combine, e.g. `1,2+kbd+bot`. Latest additions: `3,4+bot`, `0,92 3,91 3,38
  3,61 1,51` (+bot), `0,101+bot`, `0,1+bot 0,8+bot` (BAT minigame),
  `0,1+tri 2,1+tri 1,2+tri`.
- Suffix `+tri` (env `SWARS_DIFFTEST_RETRI`): on the first engine turn
  the map triangulations are rebuilt (fresh `triangulation_allocate`
  storage, then the triangulation steps of `generate_map_triangulation`
  without the collision steps, which overflow the columns loaded from
  the file). The port loads triangulations from the level files, so the
  pathtrig builder never ran otherwise.
- Suffix `+kbd` (`SWARS_DIFFTEST_KBD`): switches the local player to
  keyboard control on the first engine turn; that path runs the
  auto-targeting (`find_a_target`, `get_target_priority`,
  `check_unlock_target`, `track_player`).
- Suffix `+crater` (`SWARS_DIFFTEST_CRATER`): calls `create_crater` near
  a player agent every 64 turns. The port uses `quick_crater` for
  explosions, so `process_map_crater` ran only through this hook.
- Suffix `+wreck` (`SWARS_DIFFTEST_WRECK`): every 32 turns hits one
  vehicle (no player agent aboard) or mounted gun building with
  `thing_hit_by_bullet` until it is destroyed (`init_mgun_explode`).
  `1,100+bot+wreck` crashes in `set_person_dead`; not used.
- Bezier roads (`SubTT_BLD_BEZIER_ROAD`, only maps 2, 4 and 45): on
  every second bot step a driving agent is sent to the nearest walkable
  quad of such a road with `PAct_AGENT_GOTO_FACE_PT_ABS`, reaching
  `find_bezier_nav_point` (scenario `2,1+bot`). No extra `bot_rand`
  draws.
- The bot sends `PAct_AGENT_UNKGROUP_ADD` on turn 150 and `_PROT` on turn
  294 of every 300 (fixed turns keep the random sequence of the other
  actions), only when no agent is scripted.
- Runs are seeded: all three scripts set `SWARS_SEED=12345` (before
  `5b97696e` coverage and difftest seeded from the clock, so call counts
  varied between runs).
- Bot actions (`0a7a883d`, later extended): goto/shoot/plant at walkable
  faces, select next agent, persuade, earlier self-destruct; teleport
  cheat (`PAct_CHEAT_AGENT_TELEPORT`) next to a vehicle of a randomly
  chosen subtype, then board it on the next turn (weapon put away first).
- BAT easter egg: on map 30 (`maps/map030.b&b` area tiles) a player agent
  standing in the area with the Persuadertron selected starts Breakout.
  The bot teleports one agent there and selects it with `WepSel_TOGGLE`
  (`PAct_SELECT_SPECIFIC_WEAPON` rejects `WepSel_SELECT`, so other bot
  selects are no-ops; making them real broke 0,5+bot and 0,49+bot).
  `60707f43` also fixed the `BAT_bricks_load` wrapper, which did not pass
  the level in `eax`.
- Probing with 3000 turns: `0,22+bot` crashes (rc=3) after turn 1500 and
  `2,75+bot` hangs; not investigated. `0,22` plain at 3000 turns reaches
  `find_next_ship_tnode`, `2,75` reaches `search_things_for_uniqueid`.
- All bot missions are deterministic over two runs (1,27+bot ends at turn
  594 because the mission ends; 1,27 plain ends at turn 23).

Lessons learned, do not undo:

- Only command agents that are not running mission script commands
  (`person_is_executing_commands`). A scripted person's `ComCur` is a
  command index, not `plyr<<2|slot`, and packet handlers assert or crash.
- Skip agents whose `ComCur>>2` is not the local player (0,84+bot) and
  agents flagged `TngF_InVehicle` while still on the mapwho (shuttle pod
  quirk; self-destruct then made a mapwho cycle and `scare_civs` hung in
  3,91+bot).
- Put the weapon away before boarding a vehicle, otherwise a running laser
  shot asserts "Vehicle subtype with no shooting ability" (0,21+bot).
- No cheats while any player agent is scripted (crashed in
  `give_all_weapons_to_all_agents`).
- Never set `PActF_TriggerUse`: it keeps firing and asserts "Vehicle
  subtype with no shooting ability" once the agent enters a car. While in
  a vehicle the bot only drives or leaves.
- `fifties_per_gameturn` comes from `clock()`; `src/game_speed.c` pins it
  to 80 when `difftest_skip_turn_wait()`, otherwise bot runs are not
  deterministic.
- Debugging the 32-bit exe works with the mingw64 gdb,
  `C:/msys64/mingw64/bin/gdb.exe` (no mingw32 gdb package exists).

Harness lessons of 2026-11-21:

- `rand()`: msvcrt keeps its state in per-thread data outside our image,
  so the snapshots missed it. Difftest builds now define `rand()` and
  `srand()` in `src/difftest.c` (msvcrt algorithm, checked against
  msvcrt at start) so the state is snapshotted (`6fe4c34d`).
- Some asm reads variables at their offsets in the original binary
  layout (e.g. the list head of `process_things_mines_explode` as the
  upper half of `sthings_empty_head`, which is padding in the port). The
  C uses the real variable; in difftest builds the reference function
  fills the original slot for the call and restores it, so the asm is
  still compared (`c602f72d`). `my_preprocess_text` is the older case.
- A loop attempt fails with "scope ... still calls ASM_x outside
  reference" when another C function still calls the asm directly; fix
  that caller by hand (`find_nearest_player`, `b5ed9496`).
- Merge commits before `e34152e5` print the first scenario's compared
  count (often 0), not the total; the merges were verified.
- `decomp/auto` is pushed to `origin` (SPD13 fork) after each merge by
  `tools/decomp-agent/autopush.sh`; never to `upstream`.

## Hook scenarios (2026-11-16)

The 28 functions no mission reached are now reached by hooks, one file
per group, all called every turn from `difftest_bot_turn()`:

- `380c622e`: the scripts pass the whole scenario suffix (e.g.
  `+bot+t_train`) in `SWARS_DIFFTEST_SUFFIX`; `difftest_suffix_has(name)`
  tests for a whole token. A hook acts only when its token is present,
  so scenarios without it are unchanged.
- `src/difftest_hook_transport.c` (tokens `t_*`, `28e2b108`):
  - `t_train`: CATCH_TRAIN commands (as level loading builds them) given
    to wandering civilians near stations every 24 turns, 40 in all; the
    game's command code runs them (`person_catch_train`,
    `person_clear_route_and_teleport_to_vehicle`, `set_xz_for_unkn3`).
    `person_catch_train` compares OnFace with a positive face index, so
    people on quad faces re-path until a train comes (original
    behaviour; explains its 40000 calls).
  - `t_ship`: ships without player agents get `start_goto_vehicle()` to
    ship traffic nodes every 60 turns (`send_ship_to`).
  - `t_fly`: parked flying vehicles get state 0x27 (nothing in the game
    sets it) or a goto then flying wander
    (`process_vehicle_goto_point_fly/land`, `scan_max_height_ahead`,
    `continue_flying_wander`).
  - `t_traffic`: the credits flyby camera `traffic_unkn_func_01` every
    turn with the game camera saved and restored; every 40 turns a
    traffic node cloned as a SimpleThing and removed again
    (`new_thing_traffic_clone`).
- `src/difftest_hook_people.c` (tokens `p_*`, `71c29ca7`):
  - `p_people`: tasered flag and timer set on idle civilians every 64
    turns (`process_tasered_person`); `person_init_support_person` on a
    group member (`process_support_person`); an agent sent to a face via
    `build_navigate_path_to_face`; direct calls of `group_not_seen` and
    `find_nearest_player` on real groups/persons.
  - `p_net`: the network-game cleanups `process_things_bang`,
    `process_things_mines_explode`, `process_things_unkn_sub2` called
    directly with cycling arguments.
  - `p_mech`: the mech is wrecked with `thing_hit_by_bullet`, then placed
    again 48 turns later with the vehicle steps of
    `load_level_pc_handle` (`init_mech_explode`, `mech_unkn_func_09`),
    up to 7 times (local matrices run out). The game reuses the prim
    tables after level load, so the hook reads `qdata/primveh.obj` again
    into private buffers and swaps them in only while placing.
    `0,109+bot+p_mech` crashed on an early build; use `0,101`.
- `src/difftest_hook_misc.c` (tokens `m_*`, `15dc9649`):
  - `m_fissure`: crater "walkers" of the unused crater helpers: every 64
    turns `unused_func_025` starts one, every turn `unused_func_026`
    advances each, which sometimes splits through `unused_func_192`.
  - `m_bat`: during Breakout, `BAT_unknsub_28` (bonus spawn) at the
    centre of a standing brick every turn.
  - `m_cryo`: `add_agent` then `remove_agent` every 16 turns, with
    roster, list box, weapons and mission-agent bits saved and restored.
  - `m_typing`: a fixed key sequence fed into the key buffer for
    `user_read_value`.
- `ca56031c`: the static helper `stasis_pod_set_dist_volume()` in
  `src/thing.c` renamed `set_dist_volume`, so coverage counts the calls
  of `ASM_set_dist_volume`.
- Default scenarios (`d9f32963`), 51 now; the six new ones:
  `0,23+t_train+t_traffic 3,61+t_ship+t_fly
  1,2+m_fissure+m_cryo+m_typing 0,1+bot+m_bat 1,2+bot+p_people+p_net
  0,101+bot+p_mech`. Each was run twice by statetrace with identical
  traces. Old scenarios were not re-traced against a build without the
  hooks (only `1,2` and `0,1+bot` were; identical).
- Weak spots: tasered, network cleanups, `group_not_seen` and the crater
  helpers are reached only by direct calls or state the game never sets;
  thin counts: train passenger functions 6-9 calls, mech explosion 7,
  `new_thing_traffic_clone` 30 (hook-built SimpleThing).
- Wrapper issues to fix when remade: `unused_func_026` and
  `user_read_value` return via `"=r"` instead of `"=a"`;
  `unused_func_025/026` declare no clobbers.

## Remaining queue

27 queued (the hooked functions minus the merged
`person_clear_route_and_teleport_to_vehicle`); then only the 20
sound/network and 12 upstream-recent remain. History before the hooks:
checked and found unreachable in normal scenarios: the flying vehicle goto
(`process_vehicle_goto_point_fly/land`; nothing sets vehicle state 0x27),
`add_agent`/`remove_agent` (campaign bookkeeping after the mission),
`user_read_value` (front end), `process_tasered_person` (flag never set).
`BAT_unknsub_28` (bonus spawn) is never hit: the ball hits no brick in the
BAT scenarios. Also unreachable or nearly so: trains (`person_catch_train`,
`person_clear_route_and_teleport_to_vehicle`, `set_xz_for_unkn3`), support
persons (no mission uses them), `group_not_seen` (not reached even in
5000 turns), ships (`send_ship_to`, 2 calls) and mech explode
(`init_mech_explode`, `mech_unkn_func_09`, 1-2 calls, only 3 mech
missions). Hence the hooks.

## How to resume

After the restart of 2026-11-16 the leftover is
`process_vehicle_goto_point_land`; first check that no stray game runs
are left (`tasklist | grep -i syndwarsfx`).

From a shell in the repo (Git Bash):

```sh
cd /c/Users/sebbo/claude-workspace/syndicate-vr/syndicate-vr
git checkout decomp/auto
# 1. If a run was interrupted, remove its stale attempt
#    (git worktree list; then for the leftover <fn>:)
git worktree remove --force worktrees/<fn>
git worktree prune
git branch -D decomp/try/<fn>
# 2. Check the queue
python tools/decomp-agent/queue.py --print 10
# 3. Start the loop (runs for hours; run it in the background)
python tools/decomp-agent/run.py --loop >> tools/decomp-agent/loop.log 2>&1
```

In parallel, keep the PC awake while the loop runs (it exits by itself
when `run.py` ends):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/decomp-agent/keepawake.ps1
```

Also push every merge to the fork (exits when `run.py` ends):

```sh
tools/decomp-agent/autopush.sh
```

Follow progress with `tail -f tools/decomp-agent/loop.log`; each target
ends with `MERGED <commit>`, `FAILED` or `BLOCKED`.

Notes:

- `run.py` gives each target its own worktree and a Claude agent with a
  $5 budget (`--budget`), checks the result itself (build, `make check`,
  `difftest_run.sh`), then fast-forwards `decomp/auto` and appends to
  the ledger. Each target takes about 15–20 min.
- Game data is the default `../gamedata` next to the repo
  (`C:\Users\sebbo\claude-workspace\syndicate-vr\gamedata`). The copy in
  the old session's temp folder (`gamedata2`) is identical and not
  needed.
- Claude Code kills background tasks when memory runs low (about every
  1–4 hours in this session). Starting Claude Code with
  `CLAUDE_CODE_DISABLE_BG_SHELL_PRESSURE_REAP=1` set prevents that.
  Alternatively, run the loop in a separate terminal outside Claude Code.
- `coverage.json` and `queue.json` are gitignored local files. Rebuild
  them with `tools/decomp-agent/coverage.sh` (slow) only if the scenario
  list changes.

## Open items

- Regression of the functions remade before the bot (247 in the ledger)
  on the bot scenarios was done on 2026-11-14: 243 reach a bot scenario,
  all pass after the `do_shockwave` fix (it negated its own argument
  slot). The other 4 are not reached by any bot scenario.
- When a function is blocked by an internal asm label, route the label
  (`tools/decomp-agent/route_labels.py`, or by hand), verify with
  `tools/decomp-agent/statetrace.sh` against the previous build (all
  scenarios identical), then append `{"fn": X, "status": "reset"}` to
  the ledger so `queue.py` forgets earlier failures. Base traces made
  before `b69356d9` are invalid; regenerate them.
- Low-coverage functions need new scenarios, bot actions, hooks
  (`src/difftest_hook_*.c`, token in the suffix) or fuzz cases
  (`queue.py --print 100` lists them with the `low-coverage` tag).
- If a hooked function fails its difftest because of the hook (not the
  remake), fix the hook, re-run its scenario twice with statetrace, then
  ledger-reset the function.
- Statetrace is occasionally non-deterministic (one run of plain 0,1
  diverged); rerun a differing trace before trusting it. Cause unknown.
- `2,75+bot` hangs and `0,84+bot` asserts in `process_protect_person`;
  not used as scenarios.
- Still asm: 22 functions called only from asm, and 16 game internal
  labels (46 more in net/sound/radica).
