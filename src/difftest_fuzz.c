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
#include "game_save.h"
#include "matrix.h"
#include "mydraw.h"
#include "pathtrig.h"
/******************************************************************************/
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

/** Random value with random magnitude, to cover small and large numbers. */
static s32 fuzz_int(u32 *p_seed)
{
    u32 v = difftest_fuzz_rand(p_seed);
    return (s32)(v >> (difftest_fuzz_rand(p_seed) % 32));
}

static void fuzz_vec_cross_prod(u32 *p_seed)
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

static void fuzz_arctan(u32 *p_seed)
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

static void fuzz_object_vec_normalisation(u32 *p_seed)
{
    struct M33 mat;
    int row, col;

    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            mat.R[row][col] = fuzz_int(p_seed);
    object_vec_normalisation(&mat, difftest_fuzz_rand(p_seed) % 3);
}

static void fuzz_rotate_object_axis(u32 *p_seed)
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

static void fuzz_check_person_within(u32 *p_seed)
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

static void fuzz_check_big_point_triangle(u32 *p_seed)
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

static void fuzz_two4_line_intersection(u32 *p_seed)
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

static void fuzz_fov_region(u32 *p_seed)
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
    fov_region(x, y, p);
}

void VNAV_qwertyuiop(intptr_t a1, intptr_t a2, int a3, int a4, int a5, int a6, int a7, int a8);

static void fuzz_VNAV_qwertyuiop(u32 *p_seed)
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

void matrix_transpose_cp(intptr_t a1, intptr_t a2, intptr_t a3);

static void fuzz_matrix_transpose_cp(u32 *p_seed)
{
    s32 dst[9], src[9];
    int i;

    for (i = 0; i < 9; i++) {
        dst[i] = fuzz_int(p_seed);
        src[i] = fuzz_int(p_seed);
    }
    // Sometimes check in-place transpose
    if (difftest_fuzz_rand(p_seed) % 8 == 0)
        matrix_transpose_cp((int)src, (int)src, 0);
    else
        matrix_transpose_cp((int)dst, (int)src, 0);
}

void mech_unkn_func_20(struct M31 *p_from, struct M31 *p_to, s32 div,
  struct M31 *p_out);

static void fuzz_mech_unkn_func_20(u32 *p_seed)
{
    struct M31 from, to, out;
    s32 div;
    int i;

    // Keep values small so the deltas cannot overflow
    for (i = 0; i < 3; i++) {
        from.R[i] = (s32)(difftest_fuzz_rand(p_seed) % 0x2000) - 0x1000;
        to.R[i] = (s32)(difftest_fuzz_rand(p_seed) % 0x2000) - 0x1000;
        out.R[i] = fuzz_int(p_seed);
    }
    div = (s32)(difftest_fuzz_rand(p_seed) % 17) - 8;
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        mech_unkn_func_20(&from, &to, div, &from);
        break;
    case 1:
        mech_unkn_func_20(&from, &to, div, &to);
        break;
    default:
        mech_unkn_func_20(&from, &to, div, &out);
        break;
    }
}

int func_5cff0(short *p_x, short *p_z, short *p_vect);

static void fuzz_func_5cff0(u32 *p_seed)
{
    short x, z;
    s32 pts[4];
    int i;

    // Points near (x, z) most of the time, so both distance checks matter
    x = (short)difftest_fuzz_rand(p_seed);
    z = (short)difftest_fuzz_rand(p_seed);
    for (i = 0; i < 4; i++) {
        if (difftest_fuzz_rand(p_seed) % 4 != 0)
            pts[i] = ((s32)(((i & 1) ? z : x) +
              (short)(difftest_fuzz_rand(p_seed) % 64) - 32) << 16) |
              (difftest_fuzz_rand(p_seed) & 0xFFFF);
        else
            pts[i] = difftest_fuzz_rand(p_seed);
    }
    // The asm reads the first point's x as the low short of pts[0]
    if (difftest_fuzz_rand(p_seed) % 2 == 0)
        pts[0] = (pts[0] & 0xFFFF0000) | (ushort)(x +
          (short)(difftest_fuzz_rand(p_seed) % 64) - 32);
    // Keeps the random sequence of the former (unused) ecx argument
    difftest_fuzz_rand(p_seed);
    func_5cff0(&x, &z, (short *)pts);
}

void line_intersect8(s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3,
  s32 x4, s32 y4, s32 *p_out_x, s32 *p_out_y);

static void fuzz_line_intersect8(u32 *p_seed)
{
    int v[8];
    s32 out_x, out_z;
    int base_x, base_z, i;

    // Deltas within +-32 keep the idiv quotients within 32 bits
    base_x = (short)difftest_fuzz_rand(p_seed) % 4096;
    base_z = (short)difftest_fuzz_rand(p_seed) % 4096;
    for (i = 0; i < 8; i++)
        v[i] = ((i & 1) ? base_z : base_x) +
          (int)(difftest_fuzz_rand(p_seed) % 65) - 32;
    // Sometimes parallel or shared-point lines
    switch (difftest_fuzz_rand(p_seed) % 8)
    {
    case 0:
        v[6] = v[4] + (v[2] - v[0]);
        v[7] = v[5] + (v[3] - v[1]);
        break;
    case 1:
        v[4] = v[0];
        v[5] = v[1];
        break;
    default:
        break;
    }
    out_x = fuzz_int(p_seed);
    out_z = fuzz_int(p_seed);
    line_intersect8(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
      &out_x, &out_z);
}

void save_crypto_make_hashtable(ubyte simple_salt);
u32 save_crypto_transform1(ubyte a1);
u32 save_crypto_transform2(ubyte a1);
u32 save_crypto_transform3(ubyte a1);

/** Makes the save crypto tables from a random salt, then draws keys as
 * save_crypto_docrypt() does. Covers all four save_crypto functions.
 */
static void fuzz_save_crypto(u32 *p_seed)
{
    int i;

    save_mortal_salt = difftest_fuzz_rand(p_seed);
    save_crypto_make_hashtable(difftest_fuzz_rand(p_seed) % 4 == 0);
    for (i = 0; i < 64; i++)
    {
        ubyte a1 = difftest_fuzz_rand(p_seed) & 1;
        switch (difftest_fuzz_rand(p_seed) % 3)
        {
        case 0:
            save_crypto_transform1(a1);
            break;
        case 1:
            save_crypto_transform2(a1);
            break;
        default:
            save_crypto_transform3(a1);
            break;
        }
    }
}

/** Random text made of the characters my_preprocess_text() treats
 * specially (backslash escapes, line ends, spaces), with a random login
 * name, which the escapes insert.
 */
static void fuzz_my_preprocess_text(u32 *p_seed)
{
    // No 'l'/'L': the asm finds the login name end through the address of
    // net_host_player_no, which no longer precedes login_name in the port
    static const char chars[] = "ab \\hncHNC\r\n";
    char text[1024];
    int i, len;

    len = difftest_fuzz_rand(p_seed) % 16;
    for (i = 0; i < len; i++)
        login_name[i] = 'A' + difftest_fuzz_rand(p_seed) % 26;
    login_name[len] = '\0';
    // Short text: insertions grow it by up to 15 chars per escape
    len = difftest_fuzz_rand(p_seed) % 48;
    for (i = 0; i < len; i++)
        text[i] = chars[difftest_fuzz_rand(p_seed) % (sizeof(chars) - 1)];
    memset(text + len, 0, sizeof(text) - len);
    my_preprocess_text(text);
}

void nullsub_7(void);

/** An empty function; the case only makes the difftest call it. */
static void fuzz_nullsub_7(u32 *p_seed)
{
    nullsub_7();
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
    {"matrix_transpose_cp", fuzz_matrix_transpose_cp},
    {"mech_unkn_func_20", fuzz_mech_unkn_func_20},
    {"func_5cff0", fuzz_func_5cff0},
    {"line_intersect8", fuzz_line_intersect8},
    {"my_preprocess_text", fuzz_my_preprocess_text},
    {"nullsub_7", fuzz_nullsub_7},
    {"save_crypto_make_hashtable", fuzz_save_crypto},
    {"save_crypto_transform1", fuzz_save_crypto},
    {"save_crypto_transform2", fuzz_save_crypto},
    {"save_crypto_transform3", fuzz_save_crypto},
    {NULL, NULL},
};

#endif
/******************************************************************************/
