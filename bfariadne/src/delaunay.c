/******************************************************************************/
// Bullfrog Ariadne Pathfinding Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file delaunay.c
 *     Triangulation optimization support using Delaunay algorithm.
 * @par Purpose:
 *     Implement the Delaunay mesh optimization for triangulation purposes.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "delaunay.h"

#include <limits.h>
#include <stdlib.h>
#include "bfmemut.h"
#include "difftest.h"
#include "triangls.h"
#include "tringops.h"
#include "trstate.h"
#include "trlog.h"
/******************************************************************************/

extern ubyte tag_current; // = 255;
extern ubyte routeTags[8000];

extern const int MOD3[];

static DIFFTEST_REF_FUNC int optimise_heuristic_asm(int tri, int cor)
{
    int ret;
    DIFFTEST_ASM("call ASM_optimise_heuristic\n"
        : "=a" (ret) : "a" (tri), "d" (cor));
    return ret;
}

/** Sign of (a1*b1 - a2*b2), computed with 64-bit products.
 */
static int sign_of_cross_diff(s32 a1, s32 b1, s32 a2, s32 b2)
{
    s64 v1, v2;

    v1 = (s64)a1 * b1;
    v2 = (s64)a2 * b2;
    if (v1 > v2)
        return 1;
    if (v1 < v2)
        return -1;
    return 0;
}

/** Checks whether flipping the edge at tip `cor` of triangle `tri` makes
 * the mesh closer to Delaunay (the opposite point lies inside the
 * quadrilateral, and the new diagonal is shorter).
 */
int optimise_heuristic(int tri, int cor)
{
    struct TrTriangle *p_tri;
    struct TrTriangle *p_ntri;
    struct TrPoint *p_pt0;
    struct TrPoint *p_pt1;
    struct TrPoint *p_pt2;
    struct TrPoint *p_ptn;
    int ntri, ncor;
    u32 dx, dy;
    s32 dist_n2, dist_01;

    DIFFTEST_SHADOW_RET(optimise_heuristic, int, optimise_heuristic_asm(tri, cor));

    p_tri = &triangulation[0].Triangles[tri];
    ntri = p_tri->tri[cor];
    if (ntri == -1)
        DIFFTEST_RETURN(0);
    p_ntri = &triangulation[0].Triangles[ntri];
    if (p_ntri->tri[0] == tri)
        ncor = 0;
    else if (p_ntri->tri[1] == tri)
        ncor = 1;
    else if (p_ntri->tri[2] == tri)
        ncor = 2;
    else
        ncor = -1;

    if (p_tri->solid != p_ntri->solid)
        DIFFTEST_RETURN(0);
    if ((p_tri->enter & (1 << cor)) == 0)
        DIFFTEST_RETURN(0);
    // The asm shifts 1 by (ncor & 31); for ncor == -1 the bit is outside the byte
    if (ncor < 0 || (p_ntri->enter & (1 << ncor)) == 0)
        DIFFTEST_RETURN(0);

    p_ptn = &triangulation[0].Points[(ushort)p_ntri->point[MOD3[ncor + 2]]];
    p_pt2 = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor + 2]]];
    p_pt1 = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor + 1]]];
    p_pt0 = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor + 0]]];

    if (sign_of_cross_diff((s32)((u32)p_pt0->x - (u32)p_pt2->x),
      (s32)((u32)p_ptn->y - (u32)p_pt0->y),
      (s32)((u32)p_pt0->y - (u32)p_pt2->y),
      (s32)((u32)p_ptn->x - (u32)p_pt0->x)) >= 0)
        DIFFTEST_RETURN(0);

    if (sign_of_cross_diff((s32)((u32)p_pt1->x - (u32)p_pt2->x),
      (s32)((u32)p_ptn->y - (u32)p_pt1->y),
      (s32)((u32)p_pt1->y - (u32)p_pt2->y),
      (s32)((u32)p_ptn->x - (u32)p_pt1->x)) <= 0)
        DIFFTEST_RETURN(0);

    dx = (u32)p_pt2->x - (u32)p_ptn->x;
    dy = (u32)p_pt2->y - (u32)p_ptn->y;
    dist_n2 = (s32)(dx * dx + dy * dy);
    dx = (u32)p_pt0->x - (u32)p_pt1->x;
    dy = (u32)p_pt0->y - (u32)p_pt1->y;
    dist_01 = (s32)(dx * dx + dy * dy);
    if (dist_n2 >= dist_01)
        DIFFTEST_RETURN(0);

    DIFFTEST_RETURN(1);
}

void delaunay_step(void)
{
    uint n_optimised;
    int tri;
    int cor;

    tag_current++;
    if (tag_current == 0) {
        LbMemorySet(routeTags, 0, 8000);
        tag_current = 1;
    }

    do
    {
        n_optimised = 0;
        for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
        {
            struct TrTriangle *p_tri;

            p_tri = &triangulation[0].Triangles[tri];
            if (p_tri->solid == 255)
                continue;
            for (cor = 0; cor < 3; cor++)
            {
                if (optimise_heuristic(tri, cor)) {
                    break;
                }
            }
            if (cor >= 3)
                continue;
            edge_rotateAC(tri, cor);
            ++n_optimised;
            routeTags[tri] = tag_current;
        }
    }
    while (n_optimised > 0);
}

/******************************************************************************/
