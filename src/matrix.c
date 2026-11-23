/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file matrix.c
 *     Routines implementing matrix support.
 * @par Purpose:
 *     Implement functions for handling multi-dimensional matrices.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     27 May 2022 - 12 Jun 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "matrix.h"

#include "bfmath.h"
#include "difftest.h"
#include "swlog.h"
/******************************************************************************/

struct M33 local_mats[LOCAL_MATS_COUNT];
ushort next_local_mat = 1;

/******************************************************************************/

void matrix_identity_fill(struct M33 *p_mat)
{
    p_mat->R[0][0] = 0x4000; p_mat->R[0][1] = 0; p_mat->R[0][2] = 0;
    p_mat->R[1][0] = 0; p_mat->R[1][1] = 0x4000; p_mat->R[1][2] = 0;
    p_mat->R[2][0] = 0; p_mat->R[2][1] = 0; p_mat->R[2][2] = 0x4000;
}

void matrix_transpose_copy(struct M33 *p_mat1, struct M33 *p_mat2)
{
    int row, col;

    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            p_mat2->R[col][row] = p_mat1->R[row][col];
}

void matrix_rotate_yaw_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int a, c;
        a = p_src->R[row][0];
        c = p_src->R[row][2];
        p_dst->R[row][0] = (int)((((s64)a * sin_v) >> 16)
          + (((s64)c * cos_v) >> 16));
        p_dst->R[row][1] = p_src->R[row][1];
        p_dst->R[row][2] = (int)((((s64)c * sin_v) >> 16)
          - (((s64)a * cos_v) >> 16));
    }
}

void matrix_rotate_pitch_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int a, b;
        a = p_src->R[row][0];
        b = p_src->R[row][1];
        p_dst->R[row][0] = (int)((((s64)a * cos_v) >> 16)
          - (((s64)b * sin_v) >> 16));
        p_dst->R[row][1] = (int)((((s64)b * cos_v) >> 16)
          + (((s64)a * sin_v) >> 16));
        p_dst->R[row][2] = p_src->R[row][2];
    }
}

void matrix_rotate_roll_copy(const struct M33 *p_src, struct M33 *p_dst,
  int cos_v, int sin_v)
{
    int row;
    for (row = 0; row < 3; row++)
    {
        int b, c;
        b = p_src->R[row][1];
        c = p_src->R[row][2];
        p_dst->R[row][0] = p_src->R[row][0];
        p_dst->R[row][1] = (int)((((s64)b * sin_v) >> 16)
          + (((s64)c * cos_v) >> 16));
        p_dst->R[row][2] = (int)((((s64)c * sin_v) >> 16)
          - (((s64)b * cos_v) >> 16));
    }
}

static DIFFTEST_REF_FUNC void rotate_object_axis_asm(struct M33 *p_base,
  short xangle, short yangle, short zangle)
{
    DIFFTEST_ASM("call ASM_rotate_object_axis\n"
        : : "a" (p_base), "d" (xangle), "b" (yangle), "c" (zangle));
}

void rotate_object_axis(struct M33 *p_base, short xangle, short yangle, short zangle)
{
    struct M31 vec1, vec2, vecr;
    int row;

    DIFFTEST_SHADOW(rotate_object_axis,
      rotate_object_axis_asm(p_base, xangle, yangle, zangle));
    if (xangle == 0 && yangle == 0 && zangle == 0)
        return;

    if (xangle != 0)
    {
        for (row = 0; row < 3; row++)
            p_base->R[row][2] += (p_base->R[row][0] - p_base->R[row][2])
              * (s32)xangle / 512;
    }
    if (yangle != 0)
    {
        for (row = 0; row < 3; row++)
            p_base->R[row][2] += (p_base->R[row][1] - p_base->R[row][2])
              * (s32)yangle / 512;
    }
    if (zangle != 0)
    {
        for (row = 0; row < 3; row++)
            p_base->R[row][0] += (p_base->R[row][1] - p_base->R[row][0])
              * (s32)zangle / 512;
    }

    object_vec_normalisation(p_base, 2);

    for (row = 0; row < 3; row++)
    {
        vec1.R[row] = p_base->R[row][2];
        vec2.R[row] = p_base->R[row][0];
    }
    vec_cross_prod(&vecr, &vec1, &vec2);
    for (row = 0; row < 3; row++)
        p_base->R[row][1] = vecr.R[row] >> 14;

    object_vec_normalisation(p_base, 1);

    for (row = 0; row < 3; row++)
    {
        vec1.R[row] = p_base->R[row][2];
        vec2.R[row] = p_base->R[row][1];
    }
    vec_cross_prod(&vecr, &vec2, &vec1);
    for (row = 0; row < 3; row++)
        p_base->R[row][0] = vecr.R[row] >> 14;

    object_vec_normalisation(p_base, 0);
}

static DIFFTEST_REF_FUNC void vec_cross_prod_asm(struct M31 *vecr,
  struct M31 *vec1, struct M31 *vec2)
{
    DIFFTEST_ASM("call ASM_vec_cross_prod\n"
        : : "a" (vecr), "d" (vec1), "b" (vec2));
}

void vec_cross_prod(struct M31 *vecr, struct M31 *vec1, struct M31 *vec2)
{
    DIFFTEST_SHADOW(vec_cross_prod, vec_cross_prod_asm(vecr, vec1, vec2));
    // Each component is stored before the next is computed, like in the
    // original - matters if the result vector is also one of the inputs
    vecr->R[0] = vec1->R[1] * vec2->R[2] - vec1->R[2] * vec2->R[1];
    vecr->R[1] = vec1->R[2] * vec2->R[0] - vec1->R[0] * vec2->R[2];
    vecr->R[2] = vec1->R[0] * vec2->R[1] - vec2->R[0] * vec1->R[1];
}

static DIFFTEST_REF_FUNC void object_vec_normalisation_asm(struct M33 *vec,
  ubyte col)
{
    DIFFTEST_ASM("call ASM_object_vec_normalisation\n"
        : : "a" (vec), "d" (col));
}

void object_vec_normalisation(struct M33 *vec, ubyte col)
{
    s32 x, y, z;
    s32 len;

    DIFFTEST_SHADOW(object_vec_normalisation,
      object_vec_normalisation_asm(vec, col));
    x = vec->R[0][col];
    y = vec->R[1][col];
    z = vec->R[2][col];
    // Unsigned arithmetic to get the same wrap-around as the original
    len = LbSqrL((u32)y * (u32)y + (u32)x * (u32)x + (u32)z * (u32)z) + 1;
    vec->R[0][col] = (s32)((u32)x << 14) / len;
    vec->R[1][col] = (s32)((u32)y << 14) / len;
    vec->R[2][col] = (s32)((u32)z << 14) / len;
}

void matrix_transform(struct M31 *p_result, struct M33 *p_trans, struct M31 *p_source)
{
    p_result->R[0] = p_trans->R[0][2] * p_source->R[2] + p_trans->R[0][0] * p_source->R[0] + p_source->R[1] * p_trans->R[0][1];
    p_result->R[1] = p_trans->R[1][2] * p_source->R[2] + p_trans->R[1][0] * p_source->R[0] + p_source->R[1] * p_trans->R[1][1];
    p_result->R[2] = p_trans->R[2][2] * p_source->R[2] + p_trans->R[2][1] * p_source->R[1] + p_source->R[0] * p_trans->R[2][0];
}

/******************************************************************************/
