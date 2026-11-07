# Syndicate VR: automated decompilation agent plan

## Getting started on the Windows PC
1. You need an x86-64 (Intel or AMD) Windows PC. Install WSL2 with Ubuntu 22.04: `wsl --install -d Ubuntu-22.04`.
2. Inside WSL, install `gh` and Claude Code, then clone the repo:
   `gh repo clone SPD13/syndicate-vr && cd syndicate-vr && git checkout vr/main`
   The upstream remote (`swfans/syndwarsfx`) is added automatically because the repo is a fork.
3. Copy your Syndicate Wars game data to a path outside the repo, for example `~/syndicate-vr/gamedata`.
4. Start Claude Code in the repo and ask it to carry out this plan, starting with "Build environment" and then "Verification" step 1 (baseline).

## Context

The long-term goal is a VR tabletop version of Syndicate Wars built on the open-source port `swfans/syndwarsfx`. That port can't run on arm64 (or any 64-bit target) yet because it still contains about 3.6 MB of 32-bit x86 assembly:
- `src/swars.sx` is 2.5 MB, with 446 `ASM_*` entry points.
- `thing_fire_s.sx`, `radica*.sx`, `bat_s.sx`, the smacker and sound asm, and others make up the rest.

The upstream maintainer converts this by hand and expects it to take years (issue #293). Step one is to speed this up with a local, headless Claude Code agent loop on an x86-64 Windows PC (in WSL2):
- It converts asm functions to C on a local branch of our fork.
- It accepts a change only if the change builds, passes the existing tests, and passes a **differential test** comparing the original asm against the new C.
- Changes that pass are merged automatically into the local working branch.

The rendering path (camera and projection in `swrendersoft/src/engincam.c` and `engintrns.c`, the rasterizer in `bflibrary/src/general/poly*.c`) is already mostly C. The asm that blocks arm64 is mostly game logic: `people.c`, `thing.c`, `weapon.c`, `vehicle.c`, `game.c`.

### How upstream converts a function (the agent must copy this)

1. **Route:** in `swars.sx`, change `label:` to `GLOBAL_FUNC(ASM_label)` and point asm callers at `ac_label`. Add `label W <ret> <args>` to `conf/wrappers_game.conf`. `util/mkwrappers` then generates the Watcom→cdecl `ac_` stub. Add a C function whose body is `asm volatile ("call ASM_label" : ... : "a"(arg1), "d"(arg2), "b"(arg3), "c"(arg4))`. Functions with more than 4 arguments use the `stkargs[]` + `"S"` push pattern from `src/weapon.c`.
2. **Remake:** write the C body and keep the asm call inside `#if 0` as a reference.
3. **Clean up:** once no asm calls the function, delete the asm body and its `wrappers_game.conf` line.
4. **Shared globals:** asm refers to them as `EXPORT_SYMBOL(name)`; C declares them `extern` in headers. Headers use `#pragma pack(1)`, `extern "C"`, and the doxygen template.

## Setup

### 1. Fork and local repo
- The fork is `SPD13/syndicate-vr`. Remotes: `origin` = SPD13/syndicate-vr, `upstream` = swfans/syndwarsfx. Our work lives on `vr/main`.
- Create the long-lived local branch `decomp/auto` from `vr/main`. Each agent attempt runs in its own `git worktree` under `syndicate-vr/worktrees/<fn>`.
- Every N iterations, rebase `decomp/auto` onto `upstream/master`, because upstream converts functions almost daily.
  - Skip any function upstream has touched in the last 30 days, to avoid duplicating work and causing conflicts.
- Commits are local only. Nothing is pushed upstream (the maintainer prefers hand rewrites).

### 2. Build environment: x86-64 Windows PC with WSL2
The game needs 32-bit x86. Running it natively is far faster and more reliable than emulating it on the arm64 Mac.
- **WSL2 with Ubuntu 22.04**, matching upstream's automated build (`.github/workflows/build.yaml`).
  - Add the i386 architecture and install gcc-multilib, autotools, and the `:i386` packages for SDL2, OpenAL, vorbis, libpng and zlib.
  - Also install python3-polib, gdb and ccache.
  - Claude Code runs inside WSL2, and WSLg lets you playtest the same build.
- `tools/decomp-agent/setup-wsl.sh` installs all of the above in one go.
- `tools/decomp-agent/build.sh` runs `autoreconf -if && ./configure --disable-lb-wscreen-control CFLAGS/CXXFLAGS/LDFLAGS="-m32 -fno-PIC -fno-PIE" && make -j && make check`.
  - It keeps a build directory per worktree and uses ccache.
- Keep the repo and game data on the WSL Linux filesystem (`~/...`), not under `/mnt/c`, because file access there is much slower.
- Configure with `--enable-difftest` to get the build used for differential testing (step 4).
- The Mac stays useful later for the arm64 port and the VR work.

## The agent loop — `tools/decomp-agent/` (in the fork)

### 3. Choosing targets — `queue.py`
Parse `src/*.sx`, `conf/wrappers_game.conf`, and the C sources to build `queue.json`. Each target records:
- its stage: routed but not remade (C wrapper still contains a live `call ASM_`), not yet routed (internal asm label), or remade with dead asm still present (cleanup only);
- its asm size in instructions, and which other asm functions it calls (a leaf score);
- the calling convention and argument count;
- the subsystem, taken from the file name and the original address.

Ordering:
- **First:** remade functions that only need cleanup, then small routed leaf functions.
- **Then:** larger functions, working upward through the call graph.
- **Excluded for now:** network (`radica*`, `net*`), `bfsmacklib`, `ailssa`, and DOS-only asm. These get platform rewrites later instead.

`progress.py` reports the remaining `.sx` bytes, `ASM_` count, wrapper count, and pass/fail history from `ledger.jsonl`.

### 4. Differential testing — `src/difftest.c` / `src/difftest.h` (new, built only with `--enable-difftest`)

**Shadow execution inside the real game:**
- Each remade function keeps its asm reference in a `DIFFTEST_SHADOW(name, { asm call })` block instead of `#if 0`.
- With difftest enabled, if `name` matches `$SWARS_DIFFTEST_FN`, the hook does the following:
  1. Snapshot the writable state: `.data` and `.bss` via the linker symbols `__data_start`/`_end`, the bflibrary `LbMemory` heap pools, and a window of the stack above SP.
  2. Run the asm and save the post-state and return value.
  3. Restore the snapshot and run the C.
  4. Compare the two results byte for byte. Any mismatch is written to `difftest.log` with the symbol name (from `nm`) and offset, and the game aborts with a non-zero exit code.
- Known nondeterministic regions can be excluded with an allow-list (`difftest_ignore.conf`), for example timers and the sound queue.

**Driving the game:**
- `difftest_run.sh` runs the game headless (`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`) using **your own game data**, which is mounted read-only from outside the repo.
- A small scenario driver (`src/difftest_scenario.c`) loads a fixed list of missions. It issues scripted agent commands (move, fire, persuade) from a seed and runs N game turns, so that most game-logic paths get called.
- A call-coverage counter checks that the target function was actually called at least K times. If it never runs, the attempt is marked "unverified" and is not merged.

**Pure helpers** (math in `src/matrix.c`, for example) also get a random-input fuzz test that calls `ASM_x` and `x` directly. It runs under `make check` and needs no game data.

### 5. Agent instructions — `CLAUDE.md` + `.claude/commands/decompile-fn.md` (in the fork)
- `CLAUDE.md` describes the route → remake → cleanup pattern above, using the commit examples and the Watcom register order.
- It sets these rules:
  - Do not edit unrelated functions.
  - Keep the signedness and widths of the original asm (`s32`/`u16`, etc.).
  - Name unknown fields `unkn_*`.
  - Build only with `tools/decomp-agent/build.sh`.
- The `/decompile-fn <name>` command walks through these steps:
  1. Read the asm and the call sites.
  2. Route it if it isn't routed yet.
  3. Write the C, switching the reference block to `DIFFTEST_SHADOW`.
  4. Build.
  5. Run the difftest and fix mismatches, with at most 3 fix attempts.
  6. Remove the dead asm if it has no remaining asm callers.
  7. Write `RESULT.json` (status and notes).

### 6. Orchestrator — `tools/decomp-agent/run.py`
For each target taken from the queue:
1. `git worktree add worktrees/<fn> decomp/auto`.
2. Run `claude -p "/decompile-fn <fn>"` with `--max-turns`, a cost budget, `--allowedTools` limited to Read/Edit/Write/Grep/Glob and `Bash(tools/decomp-agent/*)`/`Bash(git diff*)`, and the worktree as the working directory.
3. Verify independently, without trusting the agent's own report: a clean build, `make check`, the difftest run with that function enabled, and a diff check that only the expected files changed.
4. **If everything passes:** commit with a `Remade <fn>` message and the Co-Authored-By trailer, then merge with `--ff-only` into `decomp/auto` (rebase first if needed). Append to `ledger.jsonl`.
5. **If anything fails:** save the patch and logs to `attempts/<fn>/`, mark the target failed in the ledger (retry later with that context), and remove the worktree.
6. After every M merges, run a full regression: difftest over all functions converted so far, across all scenarios.

The loop can run 1–2 targets at once, which is mostly limited by emulated build time. `run.py --once` runs a single target, and `--loop` runs until a stop file or budget limit is reached.

## Later milestones (not in this first step, listed for direction)
1. **64-bit cleanup** as asm reaches zero per subsystem:
   - Use `s32`/`u32` types.
   - Fix 32-bit pointer fields in packed structs and data files.
   - Remove the `-m32`, `-fno-PIE`, and `asfilter`/`mkwrappers` build steps.
2. **arm64 native build** on macOS and Quest-class Android. Replace or stub the remaining platform asm (smacker, sound, network).
3. **VR tabletop renderer:**
   - Hook `camera_setup_view()` and `transform_*()`, or replace them with a real 3D view matrix.
   - Render the drawlist per eye, or extract the map and things as 3D geometry, onto a table plane in OpenXR.
   - Use upstream PR #422/#433 (render interpolation separate from the 16 Hz simulation).

## Critical files
- **New in the fork:**
  - `tools/decomp-agent/{run.py,queue.py,progress.py,setup-wsl.sh,build.sh,difftest_run.sh}`
  - `src/difftest.{c,h}`, `src/difftest_scenario.c`, `conf/difftest_ignore.conf`
  - `CLAUDE.md`, `.claude/commands/decompile-fn.md`
- **Modified:**
  - `configure.ac` and `src/Makefile.am`: the `--enable-difftest` option and the new sources.
  - `src/swars.sx`, `conf/wrappers_game.conf`, and the matching `src/*.c`/`*.h`, by the agent for each function.
- **Existing pieces reused:**
  - `util/mkwrappers` (generates `ac_` wrappers).
  - `util/asfilter`.
  - The existing `make check` tests in `bflibrary/tests` and `swrendersoft/tests`.
  - The inline-asm call patterns in `src/weapon.c` and `src/bmbang.c`.

## Verification
1. **Baseline:** the unmodified fork builds in WSL2 with `-m32`, `make check` passes, and the game reaches the main menu headless with your data. The difftest build passes a no-op scenario run.
2. **Harness self-test:** deliberately break an already-converted function (for example swap two struct fields). Difftest must fail with a useful diff. Then restore it.
3. **Agent dry run:** `run.py --once` on a small routed leaf function, with the whole loop checked by hand: the worktree, the agent, independent verification, the ff-merge, and the ledger.
4. **Supervised batch:** `run.py --loop --max-targets 10`. Review the ledger and `progress.py` output, and play one mission yourself in the i386 build.
5. **Unattended:** the loop runs on its own, and you check `progress.py` regularly.

## Prerequisite from you
- Original Syndicate Wars data (GOG, Steam, or CD) placed in WSL2 at a path outside the repo, for example `~/syndicate-vr/gamedata`. Without it only the fuzz tests can run, so most game-logic functions can't be verified.
