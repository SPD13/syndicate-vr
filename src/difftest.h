/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest.h
 *     Header file for difftest.c.
 * @par Purpose:
 *     Differential testing of remade C functions against original asm.
 * @par Comment:
 *     When built with --enable-difftest, a remade function can execute its
 *     original asm reference first, then the C code from the same state,
 *     and compare the whole writable game state afterwards.
 *
 *     Usage in a remade function:
 *
 *       static DIFFTEST_REF_FUNC void foo_asm(int a, int b)
 *       {
 *           asm volatile ("call ASM_foo\n"
 *               : : "a" (a), "d" (b));
 *       }
 *
 *       void foo(int a, int b)
 *       {
 *           DIFFTEST_SHADOW(foo, foo_asm(a, b));
 *           ...C code...
 *       }
 *
 *     For a function returning a value, use DIFFTEST_SHADOW_RET() and
 *     replace every `return x;` in the C code with `DIFFTEST_RETURN(x);`:
 *
 *       short bar(int a)
 *       {
 *           DIFFTEST_SHADOW_RET(bar, short, bar_asm(a));
 *           ...
 *           DIFFTEST_RETURN(val);
 *       }
 *
 *     Without --enable-difftest, the macros compile to nothing (or plain
 *     return) and the reference function is unused.
 *
 *     Runtime control (environment variables):
 *       SWARS_DIFFTEST_FN     name of the function to shadow ("*" for all)
 *       SWARS_DIFFTEST_MAX    max number of compared calls (default 300)
 *       SWARS_DIFFTEST_MIN    min compared calls for a pass (default 1)
 *       SWARS_DIFFTEST_TURNS  quit after this many game turns
 *       SWARS_DIFFTEST_FAST   if set, do not wait between game turns
 *       SWARS_DIFFTEST_IGNORE comma separated "hexaddr:len" ranges to ignore
 *       SWARS_DIFFTEST_LOG    log file name (default "difftest.log")
 * @author   SyndicateVR decomp agent
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef DIFFTEST_H
#define DIFFTEST_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stddef.h>
#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Exit codes of a difftest run. */
enum DiffTestExitCodes {
    DtExit_PASS = 0,
    DtExit_MISMATCH = 3,
    DtExit_UNVERIFIED = 4,
};

/** Attributes for an asm reference function; never inlined, so that any
 * register clobbering by the asm stays within the usual call convention.
 * Without difftest nothing calls it, and as an unused static inline
 * function it is not emitted at all, so the build needs no asm of the
 * remade function (see the ENABLE_DIFFTEST sources in src/Makefile.am).
 */
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST
#define DIFFTEST_REF_FUNC __attribute__((noinline,unused))
#else
#define DIFFTEST_REF_FUNC inline __attribute__((unused))
#endif

#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

int difftest_shadow_begin(const char *name, void *frame);
void difftest_shadow_mid(const void *asm_ret, size_t ret_len);
void difftest_shadow_ret(const void *c_ret, size_t ret_len);
void difftest_shadow_end(int *p_active);

#define DIFFTEST_SHADOW(name, asm_call) \
    int difftest_active __attribute__((cleanup(difftest_shadow_end))) = \
        difftest_shadow_begin(#name, __builtin_frame_address(0)); \
    if (difftest_active) { \
        asm_call; \
        difftest_shadow_mid(NULL, 0); \
    }

#define DIFFTEST_SHADOW_RET(name, ret_type, asm_call) \
    int difftest_active __attribute__((cleanup(difftest_shadow_end))) = \
        difftest_shadow_begin(#name, __builtin_frame_address(0)); \
    if (difftest_active) { \
        ret_type difftest_asm_ret = asm_call; \
        difftest_shadow_mid(&difftest_asm_ret, sizeof(difftest_asm_ret)); \
    }

#define DIFFTEST_RETURN(val) do { \
        __typeof__(val) difftest_c_ret = (val); \
        if (difftest_active) \
            difftest_shadow_ret(&difftest_c_ret, sizeof(difftest_c_ret)); \
        return difftest_c_ret; \
    } while (0)

#else

#define DIFFTEST_SHADOW(name, asm_call)
#define DIFFTEST_SHADOW_RET(name, ret_type, asm_call)
#define DIFFTEST_RETURN(val) return (val)

#endif

/** Registers an additional memory region to be included in state snapshots.
 */
void difftest_add_region(const char *name, void *ptr, size_t len);

/** To be called at end of every game turn; finishes the run when the turn
 * limit is reached.
 */
void difftest_game_turn(void);

/** Returns true if waiting between game turns should be skipped.
 */
TbBool difftest_skip_turn_wait(void);

/** Builds a scripted player packet now and then, when $SWARS_DIFFTEST_BOT
 * is set; called after user input was turned into a packet.
 */
void difftest_bot_turn(void);

/** Returns true if the scenario suffix ($SWARS_DIFFTEST_SUFFIX, e.g.
 * "+bot+train") contains the token "+name" as a whole token.
 */
TbBool difftest_suffix_has(const char *name);

/** Hook groups called every turn from difftest_bot_turn(); each acts only
 * in scenarios whose suffix names one of its hooks.
 */
void difftest_hook_misc_turn(void);
void difftest_hook_people_turn(void);
void difftest_hook_transport_turn(void);

/** Runs random-input tests of pure functions, then exits.
 */
void difftest_run_fuzz(void);

/** Random-input test case; calls the remade function once with inputs
 * generated from given seed. Cases are listed in difftest_fuzz.c.
 */
struct DiffTestFuzzCase {
    const char *name;
    void (*func)(ulong *p_seed);
};

extern const struct DiffTestFuzzCase difftest_fuzz_cases[];

/** Xorshift pseudo-random generator for fuzz inputs.
 */
ulong difftest_fuzz_rand(ulong *p_seed);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
