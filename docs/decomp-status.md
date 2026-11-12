# Decompilation status — 2026-11-11

Handoff note for resuming the asm-to-C work in a new session (for example
after a reboot). How to remake a function is in `CLAUDE.md`; the overall
plan is in `docs/decompilation-agent-plan.md`.

## Where things stand

- Branch `decomp/auto` (local only, **never push**), head `d72519cc`,
  291 commits on top of `vr/main`.
- `queue.py` stats: 691 asm functions, 254 shadowed (remade in C and
  difftested), 162 routed labels still live, **37 queued**.
- Ledger (`tools/decomp-agent/ledger.jsonl`): 252 merged, 1 unverified
  (`bul_hit_vector`, never reached by any scenario).
- Last commits:
  - `b69356d9` difftest: Scripted player bot for mission scenarios
  - `0aa74e00` person_init_follow_person
  - `428f7824` make_peep_protect_peep
  - `98f4957f` count_protect
  - `9d52207f` get_soul
  - `d72519cc` snap_razor_wire
- The loop was stopped (Claude Code killed it for low memory) while it
  was pre-building `clear_player_protectors_paths`. That leaves a stale
  worktree `worktrees/clear_player_protectors_paths` and branch
  `decomp/try/clear_player_protectors_paths`, both at `d72519cc` with no
  changes. They must be removed before restarting.
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
  `statetrace.sh`): 13 plain + 8 bot =
  `0,1 0,5 0,20 1,2 1,40 2,1 0,21 1,27 1,45 0,23 1,16 2,3 1,19 1,2+bot
  1,16+bot 1,40+bot 0,23+bot 0,21+bot 0,20+bot 0,5+bot 1,19+bot`.
- All bot missions are deterministic over two runs (1,27+bot ends at turn
  594 because the mission ends; 1,27 plain ends at turn 23).

Lessons learned, do not undo:

- Only command agents that are not running mission script commands
  (`person_is_executing_commands`). A scripted person's `ComCur` is a
  command index, not `plyr<<2|slot`, and packet handlers assert or crash.
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

## Remaining queue (37, in the order `queue.py` picks them)

clear_player_protectors_paths, person_init_drop_special,
init_electricity_bomb, try_and_kill_target, set_get_item, limit_mood,
call_unprotect, process_splash, peep_change_weapon, select_new_weapon,
ma_triangle_route_5, process_mushroom, call_protect,
person_go_enter_vehicle, process_stop_as_soon_as_you_can,
stop_being_persuaded, process_laser_guided, process_stasis_pod,
person_unkn_start_get_item, person_goto_point_rel,
player_chat_message_add_key, copy_owner_with_weapon,
send_another_player_peep, thing_goto_point_rel, person_init_get_item,
process_soul, elec_hit_building, thing_goto_point,
person_init_get_item_fast, triangle_route_do_unkn4,
triangle_route_do_unkn3, then (after their callers are done)
person_go_enter_vehicle_fast, thing_goto_point_rel_fast,
thing_goto_point_fast, delete_vect, process_persuaded,
make_peeps_scatter.

Excluded from the queue: 92 low-coverage (no scenario reaches them
often enough), 20 sound/network subsystem, 13 changed recently upstream.

## How to resume

From a shell in the repo (Git Bash):

```sh
cd /c/Users/sebbo/claude-workspace/syndicate-vr/syndicate-vr
git checkout decomp/auto
# 1. Remove the stale attempt
git worktree remove --force worktrees/clear_player_protectors_paths
git worktree prune
git branch -D decomp/try/clear_player_protectors_paths
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

- Regression of the 249 functions remade before the bot was added was
  not re-run on the bot scenarios. `SWARS_DIFFTEST_FN="*"` is too slow
  (>10 min per mission); a faster option is to run `difftest_run.sh`
  for the functions the bot scenarios reach, a few at a time.
- When a function is blocked by an internal asm label, route the label
  (`tools/decomp-agent/route_labels.py`, or by hand), verify with
  `tools/decomp-agent/statetrace.sh` against the previous build (all
  scenarios identical), then append `{"fn": X, "status": "reset"}` to
  the ledger so `queue.py` forgets earlier failures. Base traces made
  before `b69356d9` are invalid; regenerate them.
- After the queue is empty: the 92 low-coverage functions need new
  scenarios (other missions or more bot actions) before they can be
  difftested.
