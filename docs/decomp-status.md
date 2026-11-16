# Decompilation status — 2026-11-16

Handoff note for resuming the asm-to-C work in a new session (for example
after a reboot). How to remake a function is in `CLAUDE.md`; the overall
plan is in `docs/decompilation-agent-plan.md`.

## Where things stand

- Branch `decomp/auto` (local only, **never push**), on top of
  `vr/main`.
- `queue.py` stats (after the 45-scenario coverage, `33a7d18b`): 694 asm
  functions, 360 shadowed (remade in C and difftested), 62 routed labels
  still live, 0 queued.
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

## Remaining queue

Empty. 28 routed functions remain low-coverage (`queue.py --print 100`,
tag `low-coverage`), 20 are sound/network, 12 upstream-recent. Checked
and found unreachable in scenarios: the flying vehicle goto
(`process_vehicle_goto_point_fly/land`; nothing sets vehicle state 0x27),
`add_agent`/`remove_agent` (campaign bookkeeping after the mission),
`user_read_value` (front end), `process_tasered_person` (flag never set).
`BAT_unknsub_28` (bonus spawn) is never hit: the ball hits no brick in the
BAT scenarios. Also unreachable or nearly so: trains (`person_catch_train`,
`person_clear_route_and_teleport_to_vehicle`, `set_xz_for_unkn3`), support
persons (no mission uses them), `group_not_seen` (not reached even in
5000 turns), ships (`send_ship_to`, 2 calls) and mech explode
(`init_mech_explode`, `mech_unkn_func_09`, 1-2 calls, only 3 mech
missions). Pushing these to 5 calls would need artificial hooks.

## How to resume

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
- Low-coverage functions need new scenarios, bot actions or fuzz cases
  (`queue.py --print 100` lists them with the `low-coverage` tag).
- Statetrace is occasionally non-deterministic (one run of plain 0,1
  diverged); rerun a differing trace before trusting it. Cause unknown.
- `2,75+bot` hangs and `0,84+bot` asserts in `process_protect_person`;
  not used as scenarios.
- Still asm: 22 functions called only from asm, and 16 game internal
  labels (46 more in net/sound/radica).
