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
- `python tools/decomp-agent/show_fn.py <fn>` — asm body, wrapper conf
  line, current C, callers.
- `tools/decomp-agent/offsets.sh <header.h> <Struct> [field...]` — sizeof
  and field offsets, to map `0x48(%ebx)` to a field.
- `python tools/decomp-agent/queue.py --print 20` — next targets.

Build only with `-O0` (the scripts do). Many existing inline-asm wrappers
lack clobber lists and break under optimisation.

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
       asm volatile ("call ASM_get_new_sthing\n"
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
   - Wrappers marked "Routed asm (internal label)" take raw registers
     (`int a1..a4` = eax, edx, ebx, ecx; often more than the asm really
     uses). Keep that signature, since the conf line must stay as is, and
     cast inside, e.g. `struct Thing *p_thing = (struct Thing *)a1;`. Only
     the registers the asm reads matter; the reference function passes
     all of them.
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
