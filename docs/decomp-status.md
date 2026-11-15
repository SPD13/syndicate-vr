# Decompilation status — 2026-11-15

Handoff note for resuming the asm-to-C work in a new session (for example
after a reboot). How to remake a function is in `CLAUDE.md`; the overall
plan is in `docs/decompilation-agent-plan.md`.

## Where things stand

- Branch `decomp/auto` (local only, **never push**), 360 commits on top of
  `vr/main`.
- `queue.py` stats: 693 asm functions, 318 shadowed (remade in C and
  difftested), 101 routed labels still live, **0 queued**.
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
- Remaining routed labels are all excluded from the queue: 69
  low-coverage, 20 sound/network subsystem, 12 changed recently upstream
  (kept excluded by choice; the 30-day window lapses about 2026-12-01,
  then `queue.py` queues them by itself).
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
  `statetrace.sh`): 13 plain + 16 bot =
  `0,1 0,5 0,20 1,2 1,40 2,1 0,21 1,27 1,45 0,23 1,16 2,3 1,19 1,2+bot
  1,16+bot 1,40+bot 0,23+bot 0,21+bot 0,20+bot 0,5+bot 1,19+bot
  1,102+bot 0,22+bot 1,100+bot 1,85+bot 1,71+bot 1,72+bot 0,49+bot
  3,4+bot` (29; `3,4+bot` added in `c973653d`).
- Bot actions (`0a7a883d`): goto/shoot/plant at walkable faces, select
  next agent, persuade, earlier self-destruct.
- Probing with 3000 turns: `0,22+bot` crashes (rc=3) after turn 1500 and
  `2,75+bot` hangs; not investigated. `0,22` plain at 3000 turns reaches
  `find_next_ship_tnode`, `2,75` reaches `search_things_for_uniqueid`.
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

## Remaining queue

Empty. To get more targets, widen the scenarios (see Open items), rebuild
`coverage.json` with `tools/decomp-agent/coverage.sh` and run
`queue.py --print` again.

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
- The queue is empty: the 69 low-coverage functions need new
  scenarios, more bot actions (the mech
  only stands still, so `process_mech` has 0 calls) or fuzz cases.
- Still asm: 22 functions called only from asm, and 16 game internal
  labels (46 more in net/sound/radica).
