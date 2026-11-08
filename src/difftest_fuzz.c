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

#include <string.h>
#include "command.h"
#include "game.h"
#include "matrix.h"
#include "pathtrig.h"
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

static void fuzz_arctan(ulong *p_seed)
{
    int dx, dz;

    // Magnitudes below 2^31; INT_MIN would make the asm divide by zero
    dx = fuzz_int(p_seed) & 0x7FFFFFFF;
    dz = fuzz_int(p_seed) & 0x7FFFFFFF;
    if (difftest_fuzz_rand(p_seed) & 1)
        dx = -dx;
    if (difftest_fuzz_rand(p_seed) & 1)
        dz = -dz;
    if ((difftest_fuzz_rand(p_seed) % 16) == 0)
        dx = 0;
    if ((difftest_fuzz_rand(p_seed) % 16) == 0)
        dz = 0;
    arctan(dx, dz);
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

static void fuzz_rotate_object_axis(ulong *p_seed)
{
    struct M33 mat;
    short angle[3];
    int row, col;

    // Mostly matrix-like values, sometimes full range
    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++) {
            if (difftest_fuzz_rand(p_seed) % 4 != 0)
                mat.R[row][col] = (s32)(difftest_fuzz_rand(p_seed) % 0x8001) - 0x4000;
            else
                mat.R[row][col] = fuzz_int(p_seed);
        }
    for (row = 0; row < 3; row++) {
        switch (difftest_fuzz_rand(p_seed) % 3)
        {
        case 0:
            angle[row] = 0;
            break;
        case 1:
            angle[row] = (short)(difftest_fuzz_rand(p_seed) % 129) - 64;
            break;
        default:
            angle[row] = (short)difftest_fuzz_rand(p_seed);
            break;
        }
    }
    rotate_object_axis(&mat, angle[0], angle[1], angle[2]);
}

ubyte check_person_within(struct Command *p_cmd, int x, int z);

static void fuzz_check_person_within(ulong *p_seed)
{
    struct Command cmd;
    int x, z;

    memset(&cmd, 0, sizeof(cmd));
    cmd.X = difftest_fuzz_rand(p_seed);
    cmd.Z = difftest_fuzz_rand(p_seed);
    cmd.Arg1 = difftest_fuzz_rand(p_seed);
    cmd.Time = difftest_fuzz_rand(p_seed);
    cmd.Flags = difftest_fuzz_rand(p_seed);
    // Keep the point near the area most of the time
    if (difftest_fuzz_rand(p_seed) % 4 != 0) {
        cmd.Arg1 %= 256;
        x = cmd.X + (short)difftest_fuzz_rand(p_seed) % 512;
        z = cmd.Z + (short)difftest_fuzz_rand(p_seed) % 512;
    } else {
        x = fuzz_int(p_seed);
        z = fuzz_int(p_seed);
    }
    check_person_within(&cmd, x, z);
}

ubyte check_big_point_triangle(int x, int y, int ux, int uy, int vx, int vy,
  int wx, int wy);

static void fuzz_check_big_point_triangle(ulong *p_seed)
{
    int v[8];
    int i;

    // Map-like coordinates; huge values could trap on idiv overflow
    for (i = 0; i < 8; i++)
        v[i] = (short)difftest_fuzz_rand(p_seed) % 2048;
    // Sometimes axis-aligned or degenerate triangles
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        v[6] = v[2];
        break;
    case 1:
        v[7] = v[3];
        break;
    case 2:
        v[4] = v[2];
        v[5] = v[3];
        break;
    default:
        break;
    }
    check_big_point_triangle(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

static void fuzz_two4_line_intersection(ulong *p_seed)
{
    int v[8];
    int i;

    // Map-like coordinates, sometimes full-range values
    for (i = 0; i < 8; i++)
    {
        if (difftest_fuzz_rand(p_seed) % 4 == 0)
            v[i] = fuzz_int(p_seed);
        else
            v[i] = (short)difftest_fuzz_rand(p_seed) % 2048;
    }
    // Sometimes shared endpoints, parallel or degenerate segments
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        v[4] = v[0];
        v[5] = v[1];
        break;
    case 1:
        v[6] = v[4] + (v[2] - v[0]);
        v[7] = v[5] + (v[3] - v[1]);
        break;
    case 2:
        v[2] = v[0];
        v[3] = v[1];
        break;
    case 3:
        v[3] = v[1];
        v[7] = v[5];
        break;
    default:
        break;
    }
    two4_line_intersection(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

static void fuzz_fov_region(ulong *p_seed)
{
    s32 p[6];
    int x, y;
    int i;

    // Map-like coordinates, sometimes full-range values
    for (i = 0; i < 6; i++)
    {
        if (difftest_fuzz_rand(p_seed) % 4 == 0)
            p[i] = fuzz_int(p_seed);
        else
            p[i] = (short)difftest_fuzz_rand(p_seed) % 2048;
    }
    if (difftest_fuzz_rand(p_seed) % 4 == 0) {
        x = fuzz_int(p_seed);
        y = fuzz_int(p_seed);
    } else {
        x = (short)difftest_fuzz_rand(p_seed) % 2048;
        y = (short)difftest_fuzz_rand(p_seed) % 2048;
    }
    // Sometimes the point on the apex or a ray end, or degenerate rays
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        x = p[2];
        y = p[3];
        break;
    case 1:
        x = p[0];
        y = p[1];
        break;
    case 2:
        p[4] = p[2];
        p[5] = p[3];
        break;
    default:
        break;
    }
    fov_region(x, y, (int)p, 0);
}

void VNAV_qwertyuiop(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8);

static void fuzz_VNAV_qwertyuiop(ulong *p_seed)
{
    s32 dist, frac;
    int v[6];
    int i, dx, dy;

    // Map-like coordinates; full range would overflow into a division trap
    for (i = 0; i < 6; i++)
        v[i] = (short)difftest_fuzz_rand(p_seed) % 2048;
    // Sometimes the point on an endpoint, or a degenerate segment
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        v[4] = v[0];
        v[5] = v[1];
        break;
    case 1:
        v[4] = v[2];
        v[5] = v[3];
        break;
    case 2:
        v[2] = v[0];
        v[3] = v[1];
        break;
    default:
        break;
    }
    // The asm divides by (len^2 >> 8); avoid short non-zero segments
    dx = v[2] - v[0];
    dy = v[3] - v[1];
    if ((dx != 0 || dy != 0) && ((dx * dx + dy * dy) >> 8) == 0)
        v[2] = v[0] + 16;
    dist = frac = 0x5a5a5a5a;
    VNAV_qwertyuiop((int)&dist, (int)&frac, v[0], v[1], v[2], v[3], v[4], v[5]);
}

/******************************************************************************/

const struct DiffTestFuzzCase difftest_fuzz_cases[] = {
    {"vec_cross_prod", fuzz_vec_cross_prod},
    {"arctan", fuzz_arctan},
    {"object_vec_normalisation", fuzz_object_vec_normalisation},
    {"rotate_object_axis", fuzz_rotate_object_axis},
    {"check_person_within", fuzz_check_person_within},
    {"check_big_point_triangle", fuzz_check_big_point_triangle},
    {"two4_line_intersection", fuzz_two4_line_intersection},
    {"fov_region", fuzz_fov_region},
    {"VNAV_qwertyuiop", fuzz_VNAV_qwertyuiop},
    {NULL, NULL},
};

#endif
/******************************************************************************/
