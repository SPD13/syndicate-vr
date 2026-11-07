---
description: Remake one routed asm function in C and verify it with difftest
argument-hint: <function-name>
---

Remake the asm function `$ARGUMENTS` in C, following CLAUDE.md exactly.
You work alone in a git worktree; nobody will answer questions.

1. Run `python tools/decomp-agent/show_fn.py $ARGUMENTS` and read the asm.
   Map all struct offsets with `tools/decomp-agent/offsets.sh`. Look at
   neighbouring already-remade functions in the same .c file for style.
2. If the function is blocked (calls internal non-`ac_` asm labels, jumps
   into shared code outside its body, or is not a C wrapper calling
   `ASM_$ARGUMENTS`), stop and write RESULT.json with status "blocked".
3. Convert the C wrapper into a `DIFFTEST_REF_FUNC` reference function and
   write the C body with `DIFFTEST_SHADOW` / `DIFFTEST_SHADOW_RET` +
   `DIFFTEST_RETURN`. If the function is pure, add a fuzz case to
   `src/difftest_fuzz.c`.
4. Build: `tools/decomp-agent/build.sh --difftest`. Fix compile errors and
   warnings in your code.
5. Verify: `tools/decomp-agent/difftest_run.sh $ARGUMENTS`.
   - MISMATCH: compare the asm again instruction by instruction against
     the differing fields; fix and repeat steps 4-5, at most 3 fix rounds.
   - UNVERIFIED (never called in the scenarios): if the function is pure,
     add a fuzz case; otherwise keep the code and report "unverified".
6. Run `tools/decomp-agent/build.sh --check` (plain build + unit tests).
7. Write `RESULT.json` in the repository root:
   `{"fn": "$ARGUMENTS", "status": "pass|mismatch|unverified|blocked|error",
   "compared": <number from the verdict line>, "notes": "<one paragraph:
   what the function does, anything uncertain>"}`

Do not commit; do not edit `.sx` files or `conf/`; do not touch other
functions. Keep the diff minimal.
