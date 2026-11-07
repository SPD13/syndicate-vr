/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_fuzz.c
 *     Random-input differential tests of pure functions.
 * @par Purpose:
 *     Each case calls one remade function with random inputs; the shadow
 *     execution in that function compares C against the asm reference.
 * @par Comment:
 *     Run with `syndwarsfx -t`, optionally with SWARS_DIFFTEST_FN=<name>.
 *     Only cases of functions using DIFFTEST_SHADOW() make sense here.
 * @author   SyndicateVR decomp agent
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#include "matrix.h"
/******************************************************************************/
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

/** Random value with random magnitude, to cover small and large numbers. */
static long fuzz_int(ulong *p_seed)
{
    ulong v = difftest_fuzz_rand(p_seed);
    return (long)(v >> (difftest_fuzz_rand(p_seed) % 32));
}

static void fuzz_vec_cross_prod(ulong *p_seed)
{
    struct M31 vecr, vec1, vec2;
    int i;

    for (i = 0; i < 3; i++) {
        vecr.R[i] = fuzz_int(p_seed);
        vec1.R[i] = fuzz_int(p_seed);
        vec2.R[i] = fuzz_int(p_seed);
    }
    // Sometimes check aliasing of result with inputs
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        vec_cross_prod(&vec1, &vec1, &vec2);
        break;
    case 1:
        vec_cross_prod(&vec2, &vec1, &vec2);
        break;
    default:
        vec_cross_prod(&vecr, &vec1, &vec2);
        break;
    }
}

static void fuzz_object_vec_normalisation(ulong *p_seed)
{
    struct M33 mat;
    int row, col;

    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            mat.R[row][col] = fuzz_int(p_seed);
    object_vec_normalisation(&mat, difftest_fuzz_rand(p_seed) % 3);
}

/******************************************************************************/

const struct DiffTestFuzzCase difftest_fuzz_cases[] = {
    {"vec_cross_prod", fuzz_vec_cross_prod},
    {"object_vec_normalisation", fuzz_object_vec_normalisation},
    {NULL, NULL},
};

#endif
/******************************************************************************/
