# SyndWarsFX / Syndicate VR — notes for Claude

Source port of Syndicate Wars. Since 2026-11-23 a build without difftest
contains no asm: every function a single player game runs is C. The
original 32-bit x86 asm (`src/swars.sx` and other `*.sx`, their data now in
generated `*_data.c`) is assembled only with `--enable-difftest`, as the
reference of the differential tests; the DOS network asm only with
`--enable-dos-network`. New asm-calling code must not appear outside
DIFFTEST_REF_FUNC reference functions (`tools/decomp-agent/asm_inventory.py`
must stay empty).
Our work (branch `decomp/auto`, pushed to `origin` = SPD13 fork, never to `upstream`) converts
asm functions to C, verified by differential testing. Plan:
`docs/decompilation-agent-plan.md`. Current state and how to resume the
loop: `docs/decomp-status.md`.

## Build and test (Windows, MSYS2 MINGW32)

Always use the scripts; they enter MSYS2 by themselves when needed.

- `tools/decomp-agent/build.sh --difftest` — build with the difftest harness
  into `build/difftest/`. Add `--check` to also run `make check`; omit
  `--difftest` for the plain build in `build/release/`.
- `tools/decomp-agent/difftest_run.sh <fn>` — verdict for one function:
  random-input cases (if any) + 52 mission scenarios, ~8 min. A scenario
  "1,2+bot" plays the mission with scripted player commands
  (`src/difftest_bot.c`), reaching code that needs a player; "0,1+tri"
  first rebuilds the map triangulations (pathtrig asm); "+kbd" uses
  keyboard control, "+crater" starts a crater every 64 turns, "+wreck"
  hits a vehicle or mounted gun hard every 32 turns. Tokens `t_*`, `p_*`, `m_*` in the
  suffix switch on hooks in `src/difftest_hook_{transport,people,misc}.c`
  that reach functions no mission runs by itself.
- `python tools/decomp-agent/difftest_batch.py <fns.json|fn,fn> [--jobs 6]
  [--per-fn 3]` — difftest of many functions in parallel, each on a few
  scenarios that reach it (coverage.json), with own game data copies.
- `python tools/decomp-agent/show_fn.py <fn>` — asm body, wrapper conf
  line, current C, callers.
- `tools/decomp-agent/offsets.sh <header.h> <Struct> [field...]` — sizeof
  and field offsets, to map `0x48(%ebx)` to a field.
- `python tools/decomp-agent/queue.py --print 20` — next targets.

Build only with `-O0` (the scripts do). Many existing inline-asm wrappers
lack clobber lists and break under optimisation.

## 64-bit (since 2026-11-23)

- `build.sh --x64` builds the 64-bit game (MSYS2 MINGW64, `build/x64`,
  with the test harness); `build.sh --harness` the 32-bit one without asm.
  `SWARS_TRACE_BUILD=x64 tools/decomp-agent/statetrace.sh <dir>` traces
  it; the traces must equal those of the 32-bit build (`diff -r`).
  `tools/decomp-agent/run_x64.sh [build] [mission] [secs]` runs it.
- Linux (WSL Ubuntu, clone in `~/swars`, game data copied to
  `~/gamedata`): `build.sh --x64` builds natively (data path `.`);
  `build.sh --arm64` cross builds for aarch64 against
  `~/arm64-sysroot` (made by `tools/decomp-agent/arm64_sysroot.py`, no
  root needed; C++ cross compiler from `SWARS_ARM64_CXX` if
  `aarch64-linux-gnu-g++` is not installed). The arm64 exe runs under
  qemu with `QEMU_LD_PREFIX=/usr/aarch64-linux-gnu` and `LD_LIBRARY_PATH`
  set to the sysroot's `usr/lib/aarch64-linux-gnu` (and its
  `pulseaudio/`); `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` for
  traces; `statetrace_par.sh <build> <outdir> [jobs]` runs the traces in
  parallel and sets these. Traces of Linux x64 and arm64 equal the
  32-bit reference.
- All code is built with `-fsigned-char`: the decompiled code reads bytes
  as plain `char` and expects them signed, as on x86 (ARM's `char` is
  unsigned). New code uses `sbyte`/`ubyte` anyway.
- Never put two calls with side effects (random draws above all) in one
  expression or argument list: their order is unspecified, and gcc
  evaluates arguments right to left on x86 but left to right on arm64.
  Use separate statements, in the order the x86 build used.
- Include the headers you use (`<string.h>` for `strlen`, ...): MinGW
  pulls them in indirectly, glibc does not.
- Write code that works with 64-bit pointers and LP64 `long`:
  - no `long`/`ulong`: use `s32`/`u32` (`int32_t`/`uint32_t` in
    bfsoundlib) and `%d`/`%u`/`%x` formats; `long` is 64 bits on Linux.
    Same for limits: `INT32_MAX`/`UINT32_MAX`, not `LONG_MAX`/`ULONG_MAX`.
  - never store a pointer in an int type: parameters that carry pointers
    are typed pointers (only `*_asm` references take raw `intptr_t`
    registers), address arithmetic uses `uintptr_t`.
  - no C library `rand()`/`srand()` in game code: `game_rand()`/
    `game_srand()` (crtrand.c, the msvcrt algorithm) give the same draws
    on every target. Game randomness is otherwise `LbRandom*`.
  - structs read from or written to files keep the file layout: if they
    hold pointers, convert records at load/save (see `thing_from_record`
    in lvfiles.c, `LbSpriteTabExpand`); never `sizeof` a struct with a
    pointer for a file record size.
- Inline asm of `DIFFTEST_REF_FUNC` reference functions is written
  `DIFFTEST_ASM(...)` instead of `asm volatile (...)`, so that it exists
  only in difftest builds (other compilers reject x86 register
  constraints even in unused functions).
- `layout_diff.py` lists struct layouts that differ between the 32 and
  64-bit builds; a build with `-Wconversion` shows pointers passed into
  narrower ints (`conversion from 'intptr_t' ... to 'int'`).

## Game library (since 2026-11-23)

- The game is `src/libswarscore.a` with the interface `src/swcore.h`
  (see `docs/swcore.md`). `syndwarsfx` is `src/swmain.c` plus the whole
  library; `swcore_headless` is a test front end (builds without asm).
  New game sources go into `libswarscore_a_SOURCES` in `src/Makefile.am`.
- The direction: one C core, front ends per target (SDL desktop, VR,
  mobile), solo play only. Keep `swcore.h` free of game headers
  (`<stdint.h>` types only) and add to it, don't change it
  (`SWCORE_API_VERSION`).
- `SWARS_TRACE_EXE=swcore_headless python
  tools/decomp-agent/statetrace_win_par.py harness <outdir>` checks that
  a library front end plays the same game as the reference (traces and
  the hashes read through the interface); add `SWARS_TRACE_EXE_ARGS=--no-draw`
  for the run without drawing the world, which must be equal too.
- Drawing writes game state in places (face flags of vehicles that line
  of sight reads, the thing under the pointer, the panels' target): see
  `docs/swcore.md` before moving code out of the drawing path. Fields only
  drawing uses go in `state_hash_render_fields` (game.c).
- Trace runs use SDL's dummy drivers (set by `statetrace.sh`; `msys.sh`
  passes `SWARS_*` and `SDL_*` variables only). A real window let the
  desktop's mouse pointer change runs.
- Never touch the `assets/tripo` branch, its worktree
  (`worktrees/assets-tripo`) or `tools/asset3d/`: another session works
  there.

## Calling convention of the asm (Watcom register)

- Arguments in `eax`, `edx`, `ebx`, `ecx`, in that order; more arguments
  are pushed on the stack right to left and the callee pops them
  (`ret $N`). Return value in `eax` (`ax`/`al` for short/char).
- `conf/wrappers_game.conf` line `name W <ret> <args>`: `v` void, `i` int,
  `p` pointer, `s`/`c` short/char. `util/mkwrappers` generates `ac_name`,
  which asm calls to reach the C function.
- Inline asm for more than 4 args: put stack args in an `int stkargs[]`
  array and push via the `"S"` register, see `bul_path_end` in
  `src/weapon.c`. Never push "g" operands directly.

## How a function is remade (stage "routed": C still calls ASM_x)

1. `python tools/decomp-agent/show_fn.py <fn>`. Read the asm; map every
   offset to a struct field with `offsets.sh`. Shared globals appear as
   `EXPORT_SYMBOL(name)` and are declared `extern` in our headers.
2. Turn the existing C wrapper into a reference function, and write the C
   body in the real function:

   ```c
   static DIFFTEST_REF_FUNC short get_new_sthing_asm(void)
   {
       short ret;
       DIFFTEST_ASM ("call ASM_get_new_sthing\n"
           : "=a" (ret) : );
       return ret;
   }

   short get_new_sthing(void)
   {
       ThingIdx new_sthing;

       DIFFTEST_SHADOW_RET(get_new_sthing, short, get_new_sthing_asm());
       if (sthings_used >= STHINGS_LIMIT - 4)
           DIFFTEST_RETURN((short)0);
       ...
       DIFFTEST_RETURN(new_sthing);
   }
   ```

   - void functions: `DIFFTEST_SHADOW(name, name_asm(args));` and plain
     `return;`.
   - Non-void: `DIFFTEST_SHADOW_RET(name, type, name_asm(args));` and
     **every** return in the body becomes `DIFFTEST_RETURN(value);`. The
     value's type must equal `type` (cast literals, e.g. `(short)0`).
   - Use `"=a"` for return values in the reference function.
   - `#include "difftest.h"` in the .c file (alphabetical order).
3. Keep the asm body in `.sx` untouched: it is the reference. Do not edit
   `.sx` files or `wrappers_game.conf` unless the plan step requires it.
   - Wrappers marked "Routed asm (internal label)" are reached from asm
     through a generated `ac_` wrapper that pushes eax, edx, ebx, ecx
     (then stack args) as 32-bit cdecl arguments. Give the C function real
     parameters in those positions: typed pointers (never a pointer in an
     int), `int`/`s32` for numbers, named after their meaning; trailing
     registers the asm does not read may be dropped, unread middle ones
     stay as `int unused_aN`. The conf line stays as is. Only the
     `*_asm` reference keeps the raw `int a1..a4` signature; the shadow
     call passes it `(intptr_t)p_thing` and 0 for dropped registers.
     A parameter the body changes (`p_route++`) needs a local copy: at
     -O0 gcc writes it back into the caller's argument slot, which the
     difftest compares (MISMATCH in region `stack`).
     (Done for all such functions on 2026-11-23, see `find_best_guardian`.)
   - Asm data exported with `GLOBAL(name)` in swars.sx (also `data_xxxx`
     labels) can be used from C: declare `extern` with the right type next
     to the function (or in its header), e.g. `extern short heap[];`. Keep
     the data in the asm; other asm code still uses it.
4. Pure functions (no globals, only pointer args) also get a random-input
   case in `src/difftest_fuzz.c`, see `fuzz_vec_cross_prod`.
5. Build with `build.sh --difftest`, then run `difftest_run.sh <fn>`.
   MISMATCH output lists differing byte ranges (region, address, symbol for
   static data, asm vs c bytes); RETURN VALUE MISMATCH shows both values.

## Translation rules

- Keep exact widths and signedness from the asm: `movswl` = short,
  `movzwl` = ushort, `movsbl` = char/sbyte, `movzbl` = ubyte; `jl/jg/jle/jge`
  signed compares, `jb/ja/jbe/jae` unsigned. Use `s32`/`u32`, `short`/
  `ushort`, `sbyte`/`ubyte` accordingly.
- Translate every store, in the same order when pointers could alias.
  The difftest only proves equivalence on states the game reached, so a
  store that looks redundant must still be kept.
- Use existing struct fields and helpers; name unknown fields `unkn_*`
  following the existing style. Match the surrounding code style (4-space
  indent, `p_` prefix for pointers, braces on own line for functions).
- Do not edit unrelated functions. Do not change the behaviour, even if it
  looks like an original bug — add a `// TODO` comment instead.
- If the asm calls an internal label that is not a C function (no `ac_`
  prefix), the function is blocked: report it rather than routing more.
  Exception: a label that is only a thunk to a C function (its body just
  saves registers and calls `ac_x`, e.g. `is_that_a_fluid_texture` ->
  `floor_texture_is_fluid()`) is not a blocker; call the C function.
- If the asm jumps to labels outside its own body (shared tails), report
  blocked unless the shared code is trivial to replicate.
