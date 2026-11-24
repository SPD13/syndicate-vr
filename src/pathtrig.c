/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file pathtrig.c
 *     Pathfinding triangulation support required by the 3D engine.
 * @par Purpose:
 *     Implement functions for handling pathfinding triangulation.
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
#include "pathtrig.h"

#include "bfkeybd.h"
#include "bfmath.h"
#include "bfmemory.h"
#include <assert.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "drawshape.h"
#include "engincolour.h"

#include "bigmap.h"
#include "building.h"
#include "enginsngobjs.h"
#include "game.h"
#include "game_data.h"
#include "hud_draw.h"
#include "lvwalk.h"
#include "thing.h"
#include "tngcolisn.h"
#include "triangls.h"
#include "tringops.h"
#include "trpoints.h"
#include "trstate.h"
#include "trfringe.h"
#include "delaunay.h"
#include "difftest.h"
#include "swlog.h"
/******************************************************************************/
s32 Ex[100];
s32 Ey[100];
s32 ixE = 0;
s32 thin_wall_x1, thin_wall_y1;
s32 thin_wall_x2, thin_wall_y2;
s32 tree_dad[8000];
s32 tree_val[8000];
ubyte routeTags[8000];
s32 tree_Ax8;
s32 tree_Ay8;
s32 tree_Bx8;
s32 tree_By8;
s32 tree_altA;
s32 tree_altB;
s32 tree_triA;
s32 tree_triB;
s32 tree_CentreDest;
s32 tree_routelen;
s32 tree_routecost;
s32 tree_route[3000];
s32 route_bak[3000];


extern short link__MapColListEmptyHead;
extern short link__MapColVectEmptyHead;

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
short link__MapColListEmptyHead;
short link__MapColVectEmptyHead;
struct Path g_fat_data;
#endif /* typed asm data */

/** Map of the ground surface for triangluation purposes, 2x2 per tile.
 */
ubyte *ground_map = NULL;

enum ThinWallType {
    THIN_BLOCK = 0,
    THIN_PASS = 1,
};

extern const int MOD3[];

TbBool point_find(TrCoord pt_x, TrCoord pt_y, int *r_tri, int *r_cor);

void unkn_path_func_001(struct Thing *p_thing, ubyte a2)
{
    int cor_beg_x, cor_beg_z;
    ushort pt, pt_max;
    ushort path;

    if (a2 == 1)
        return;

    path = p_thing->U.UPerson.PathIndex;
    pt = p_thing->PathOffset;
    pt_max = my_paths[path].Flag;
    cor_beg_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
    cor_beg_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    while (path > 0)
    {
        int cor_end_x, cor_end_y, cor_end_z;
        int cor_beg_y;

        if (pt >= pt_max)
            break;
        cor_end_x = my_paths[path].X[pt & 3];
        cor_end_z = my_paths[path].Z[pt & 3];
        cor_beg_y = alt_at_point(cor_beg_x, cor_beg_z) >> 5;
        cor_end_y = alt_at_point(cor_end_x, cor_end_z) >> 5;

        if (lbShift == KMod_SHIFT) {
            draw_mapcoord_line_directed_to_target(cor_beg_x, cor_beg_y, cor_beg_z,
              cor_end_x, cor_end_y, cor_end_z, colour_lookup[2]);
        } else {
            draw_mapcoord_line_col(cor_beg_x, cor_beg_y, cor_beg_z,
              cor_end_x, cor_end_y, cor_end_z, colour_lookup[2]);
        }
        cor_beg_x = cor_end_x;
        cor_beg_z = cor_end_z;

        pt++;
        if ((pt & 3) == 0)
            path = my_paths[path].Next;
    }
}

//TODO temp copy of static func
static sbyte path_compare_multiplications(s32 mul1a, s32 mul1b, s32 mul2a, s32 mul2b)
{
    long long mul1,mul2;
    mul1 = (long long)mul1a * (long long)mul1b;
    mul2 = (long long)mul2a * (long long)mul2b;
    if (mul1 > mul2)
        return 1;
    if (mul1 < mul2)
        return -1;
    return 0;
}

static DIFFTEST_REF_FUNC TbBool triangle_contains8_asm(int tri, int x, int y)
{
    TbBool ret;
    DIFFTEST_ASM("call ASM_triangle_contains8\n"
        : "=a" (ret) : "a" (tri), "d" (x), "b" (y));
    return ret;
}

TbBool triangle_contains8(int tri, int x, int y)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    TrFineCoord pt0_x, pt0_y;
    TrFineCoord pt1_x, pt1_y;
    TrFineCoord pt2_x, pt2_y;

    DIFFTEST_SHADOW_RET(triangle_contains8, TbBool, triangle_contains8_asm(tri, x, y));
    p_tri = &triangulation[0].Triangles[tri];
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[0]];
    pt0_x = p_pt->x << 8;
    pt0_y = p_pt->y << 8;
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[1]];
    pt1_x = p_pt->x << 8;
    pt1_y = p_pt->y << 8;
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[2]];
    pt2_x = p_pt->x << 8;
    pt2_y = p_pt->y << 8;

    if (path_compare_multiplications(pt1_x - pt0_x, y - pt1_y, x - pt1_x, pt1_y - pt0_y) > 0)
        DIFFTEST_RETURN((TbBool)false);
    if (path_compare_multiplications(pt2_x - pt1_x, y - pt2_y, x - pt2_x, pt2_y - pt1_y) > 0)
        DIFFTEST_RETURN((TbBool)false);
    if (path_compare_multiplications(pt0_x - pt2_x, y - pt0_y, x - pt0_x, pt0_y - pt2_y) > 0)
        DIFFTEST_RETURN((TbBool)false);
    DIFFTEST_RETURN((TbBool)true);
}

static DIFFTEST_REF_FUNC int pointed_at8_asm(TrFineCoord pt_x, TrFineCoord pt_y, int *r_tri, int *r_cor)
{
    int ret;
    DIFFTEST_ASM("call ASM_pointed_at8\n"
        : "=a" (ret) : "a" (pt_x), "d" (pt_y), "b" (r_tri), "c" (r_cor));
    return ret;
}

int pointed_at8(TrFineCoord pt_x, TrFineCoord pt_y, int *r_tri, int *r_cor)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    int tri, cor;
    int lnk_tri, lnk_cor;
    TrFineCoord ptA_x, ptA_y;
    TrFineCoord ptB_x, ptB_y;
    TrFineCoord ptC_x, ptC_y;
    TbBool eqC, eqB;

    DIFFTEST_SHADOW_RET(pointed_at8, int, pointed_at8_asm(pt_x, pt_y, r_tri, r_cor));
    tri = *r_tri;
    cor = *r_cor;
    p_tri = &triangulation[0].Triangles[tri];
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[cor]];
    ptA_x = p_pt->x << 8;
    ptA_y = p_pt->y << 8;

    while (1)
    {
        p_tri = &triangulation[0].Triangles[tri];
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+1]]];
        ptB_x = p_pt->x << 8;
        ptB_y = p_pt->y << 8;
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+2]]];
        ptC_x = p_pt->x << 8;
        ptC_y = p_pt->y << 8;

        eqC = path_compare_multiplications(ptC_x - ptA_x, pt_y - ptC_y,
          ptC_y - ptA_y, pt_x - ptC_x) > 0;
        eqB = path_compare_multiplications(ptB_x - ptA_x, pt_y - ptB_y,
          ptB_y - ptA_y, pt_x - ptB_x) > 0;
        if (eqC && !eqB)
        {
            *r_tri = tri;
            *r_cor = cor;
            DIFFTEST_RETURN((int)MOD3[cor+1]);
        }

        lnk_tri = p_tri->tri[cor];
        if (lnk_tri == -1)
            DIFFTEST_RETURN(lnk_tri);
        // TODO no guard against an endless loop, same as the original
        p_tri = &triangulation[0].Triangles[lnk_tri];
        if (p_tri->tri[0] == tri)
            lnk_cor = 0;
        else if (p_tri->tri[1] == tri)
            lnk_cor = 1;
        else if (p_tri->tri[2] == tri)
            lnk_cor = 2;
        else
            lnk_cor = -1;
        cor = MOD3[lnk_cor+1];
        tri = lnk_tri;
    }
}

int triangle_find8(TrFineCoord pt_x, TrFineCoord pt_y)
{
    int tri; //TODO switch type to TrTriangId
    int remain;

    tri = triangulation[0].last_tri;

    if (!tri_is_allocated(tri))
    {
        for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
        {
            if (tri_is_allocated(tri) && triangle_contains8(tri, pt_x, pt_y))
                break;
        }
        if (tri >= triangulation[0].ix_Triangles) {
            tri = -1;
        }
        triangulation[0].last_tri = tri;
        return tri;
    }

    remain = 2000;
    while (tri >= 0)
    {
        struct TrTriangle *p_tri;
        TrFineCoord pt0_x, pt0_y;
        TrFineCoord pt1_x, pt1_y;
        TrFineCoord pt2_x, pt2_y;
        TbBool eqA, eqB, eqC;

        remain--;
        if (remain <= 0) {
            tri = -1;
            LOGERR("Cannot find (%d,%d)", pt_x, pt_y);
            break;
        }
        p_tri = &triangulation[0].Triangles[tri];
        {
            struct TrPoint *p_pt;
            p_pt = &triangulation[0].Points[p_tri->point[0]];
            pt0_x = p_pt->x << 8;
            pt0_y = p_pt->y << 8;
            p_pt = &triangulation[0].Points[p_tri->point[1]];
            pt1_x = p_pt->x << 8;
            pt1_y = p_pt->y << 8;
            p_pt = &triangulation[0].Points[p_tri->point[2]];
            pt2_x = p_pt->x << 8;
            pt2_y = p_pt->y << 8;
        }
        eqA = path_compare_multiplications(pt1_x - pt0_x, pt_y - pt1_y, pt_x - pt1_x, pt1_y - pt0_y) > 0;
        eqB = path_compare_multiplications(pt2_x - pt1_x, pt_y - pt2_y, pt_x - pt2_x, pt2_y - pt1_y) > 0;
        eqC = path_compare_multiplications(pt0_x - pt2_x, pt_y - pt0_y, pt_x - pt0_x, pt0_y - pt2_y) > 0;
        if (!eqA && !eqB && !eqC)
            break;

        if (eqA && !eqB && !eqC && p_tri->tri[0] >= 0) {
            tri = p_tri->tri[0];
        } else if (eqB && !eqC && !eqA && p_tri->tri[1] >= 0) {
            tri = p_tri->tri[1];
        } else if (eqC && !eqA && !eqB && p_tri->tri[2] >= 0) {
            tri = p_tri->tri[2];
        } else if (eqA && eqB) {
            int cor, rcor;
            cor = 1;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        } else if (eqB && eqC) {
            int cor, rcor;
            cor = 2;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        } else if (eqC && eqA) {
            int cor, rcor;
            cor = 0;
            rcor = pointed_at8(pt_x, pt_y, &tri, &cor);
            if (rcor < 0) tri = -1;
        }
    }
    if (tri >= 0 && !triangle_contains8(tri, pt_x, pt_y)) {
        tri = -1;
    }
    triangulation[0].last_tri = tri;
    return tri;
}

static DIFFTEST_REF_FUNC int triangle_findSE8_asm(int x, int y)
{
    int ret;
    DIFFTEST_ASM("call ASM_triangle_findSE8\n"
        : "=a" (ret) : "a" (x), "d" (y));
    return ret;
}

int triangle_findSE8(int x, int y)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    int tri, cor;

    DIFFTEST_SHADOW_RET(triangle_findSE8, int, triangle_findSE8_asm(x, y));
    tri = triangle_find8(x, y);
    if (tri < 0)
        DIFFTEST_RETURN(tri);

    // If the point is exactly at a triangle tip, rotate around that tip
    for (cor = 0; cor < 3; cor++)
    {
        p_tri = &triangulation[0].Triangles[tri];
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[cor]];
        if ((p_pt->x << 8) == x && (p_pt->y << 8) == y)
        {
            pointed_at8(0x10100, 0x10100, &tri, &cor);
            DIFFTEST_RETURN(tri);
        }
    }

    // If the point lies on an edge, select the neighbour on the SE side
    for (cor = 0; cor < 3; cor++)
    {
        TrFineCoord pt1_x, pt1_y;
        TrFineCoord pt2_x, pt2_y;

        p_tri = &triangulation[0].Triangles[tri];
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[cor]];
        pt1_x = p_pt->x << 8;
        pt1_y = p_pt->y << 8;
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+1]]];
        pt2_x = p_pt->x << 8;
        pt2_y = p_pt->y << 8;
        if (path_compare_multiplications(pt2_x - pt1_x, y - pt2_y,
          pt2_y - pt1_y, x - pt2_x) != 0)
            continue;
        if (path_compare_multiplications(pt2_x - pt1_x, 0x10100 - pt2_y,
          pt2_y - pt1_y, 0x10100 - pt2_x) <= 0)
            continue;
        DIFFTEST_RETURN((int)p_tri->tri[cor]);
    }
    DIFFTEST_RETURN(tri);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void fill_concave_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_fill_concave\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Find the tip of triangle `tri` whose neighbour is `tri_nb`, or -1.
 */
static s32 fill_concave_tip_to(s32 tri, s32 tri_nb)
{
    struct TrTriangle *p_tri;

    p_tri = &triangulation[0].Triangles[tri];
    if (p_tri->tri[0] == tri_nb)
        return 0;
    if (p_tri->tri[1] == tri_nb)
        return 1;
    if (p_tri->tri[2] == tri_nb)
        return 2;
    return -1;
}

/** Rotates edges around the far point of edge `cor_base` of triangle
 * `tri_base`, walking the fan from the neighbour across that edge until
 * triangle `tri_stop` is reached, repeating while any rotation succeeded.
 * Parameters come from eax, edx, ebx; the asm does not read ecx.
 * The asm returns 0/1 in eax, but the wrapper is void.
 */
void fill_concave(s32 tri_base, s32 cor_base, s32 tri_stop)
{
    s32 tri, cor, tri_nxt;
    s32 steps, rotated;

    DIFFTEST_SHADOW(fill_concave, fill_concave_asm(tri_base, cor_base, tri_stop, 0));
    do
    {
        tri = triangulation[0].Triangles[tri_base].tri[cor_base];
        if (tri == -1)
            return;
        cor = (fill_concave_tip_to(tri, tri_base) + 1) % 3;
        steps = 0;
        rotated = 0;
        while (triangulation[0].Triangles[tri].tri[cor] != tri_stop)
        {
            if (edge_rotateAC(tri, cor)) {
                rotated++;
                continue;
            }
            tri_nxt = triangulation[0].Triangles[tri].tri[cor];
            if (tri_nxt == -1 || tri_nxt == tri_base)
                return;
            cor = (fill_concave_tip_to(tri_nxt, tri) + 1) % 3;
            steps++;
            tri = tri_nxt;
        }
    } while (steps != 0 && rotated != 0);
}


static DIFFTEST_REF_FUNC void make_edge_asm(int x1, int y1, int x2, int y2)
{
    DIFFTEST_ASM(
      "call ASM_make_edge\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2));
}

/** Forces an edge from (x1,y1) to (x2,y2) into the triangulation, walking
 * the triangles crossed by the segment and rotating edges out of the way.
 * If the segment passes exactly through a point, splits it there.
 */
void make_edge(int x1, int y1, int x2, int y2)
{
    struct TrPoint *p_pt;
    int tri, cor;
    int tri2, cor2;
    s32 dx, dy;
    s32 tri_cur, cor_cur;
    s32 tri_nb, cor_nb;
    s32 tri_nt, cor_nt;
    s32 tri_nt2, cor_nt2;
    s32 pt_x, pt_y;
    s32 ptA_x, ptA_y;
    s32 ptB_x, ptB_y;
    s32 ptC_x, ptC_y;

    DIFFTEST_SHADOW(make_edge, make_edge_asm(x1, y1, x2, y2));
    if (x1 == x2 && y1 == y2)
        return;

    dy = y2 - y1;
    dx = x2 - x1;
    while (1)
    {
        if (!point_find(x1, y1, &tri, &cor))
            return;
        if (!point_find(x2, y2, &tri2, &cor2))
            return;
        cor_cur = pointed_at8(x2 << 8, y2 << 8, &tri, &cor);
        tri_cur = tri;

        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_cur].point[MOD3[cor+1]]];
        if (p_pt->x == x2 && p_pt->y == y2)
            return;
        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri].point[MOD3[cor+2]]];
        if (p_pt->x == x2 && p_pt->y == y2)
            return;

        tri_nb = triangulation[0].Triangles[tri].tri[cor];
        cor_nb = fill_concave_tip_to(tri_nb, tri);
        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_nb].point[cor_nb]];
        pt_x = p_pt->x;
        pt_y = p_pt->y;
        if (path_compare_multiplications(dx, pt_y - y2, dy, pt_x - x2) == 0)
        {
            // The segment passes through a point; split the edge there
            make_edge(x1, y1, pt_x, pt_y);
            make_edge(x2, y2, pt_x, pt_y);
            return;
        }

        do
        {
            tri_nt = triangulation[0].Triangles[tri_cur].tri[cor_cur];
            cor_nt = fill_concave_tip_to(tri_nt, tri_cur);
            p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_nt].point[MOD3[cor_nt+2]]];
            pt_x = p_pt->x;
            pt_y = p_pt->y;
            cor_cur = MOD3[cor_nt+1];
            tri_cur = tri_nt;
            if (path_compare_multiplications(dx, pt_y - y2, dy, pt_x - x2) >= 0)
            {
                tri_nt2 = triangulation[0].Triangles[tri_nt].tri[cor_cur];
                cor_nt2 = fill_concave_tip_to(tri_nt2, tri_nt);
                p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_nt2].point[cor_nt2]];
                ptA_x = p_pt->x;
                ptA_y = p_pt->y;
                p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_nb].point[cor_nb]];
                ptB_x = p_pt->x;
                ptB_y = p_pt->y;
                p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_nb].point[MOD3[cor_nb+1]]];
                ptC_x = p_pt->x;
                ptC_y = p_pt->y;
                if (path_compare_multiplications(ptB_x - ptC_x, ptA_y - ptB_y,
                  ptB_y - ptC_y, ptA_x - ptB_x) < 0)
                {
                    fill_concave(tri_nb, cor_nb, tri_nt2);
                    break;
                }
                tri_nb = tri_nt2;
                cor_nb = cor_nt2;
                cor_cur = MOD3[cor_nt+2];
            }
        }
        while (pt_x != x2 || pt_y != y2);
    }
}

static DIFFTEST_REF_FUNC int edge_find_asm(int x1, int y1, int x2, int y2, int *ntri1, int *ncor1)
{
    int ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)ntri1;
    stkargs[1] = (int)(intptr_t)ncor1;

    DIFFTEST_ASM(
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_edge_find\n"
        : "=a" (ret)
        : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

static int edge_find_sign(s32 val)
{
    if (val < 0)
        return -1;
    return (val != 0);
}

int edge_find(int x1, int y1, int x2, int y2, int *ntri1, int *ncor1)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt;
    int tri_start, cor_start;
    int tri, cor;
    int lnk_tri, lnk_cor;
    s32 dx, dy;
    s32 pt_x, pt_y;

    DIFFTEST_SHADOW_RET(edge_find, int,
      edge_find_asm(x1, y1, x2, y2, ntri1, ncor1));
    if (!point_find(x1, y1, &tri_start, &cor_start))
        DIFFTEST_RETURN((int)0);

    dy = y2 - y1;
    dx = x2 - x1;
    tri = tri_start;
    cor = cor_start;
    do
    {
        p_tri = &triangulation[0].Triangles[tri];
        p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+1]]];
        pt_x = p_pt->x;
        pt_y = p_pt->y;

        if ((path_compare_multiplications(pt_x - x1, y2 - pt_y,
          pt_y - y1, x2 - pt_x) == 0) &&
          (edge_find_sign(pt_x - x1) == edge_find_sign(dx)) &&
          (edge_find_sign(pt_y - y1) == edge_find_sign(dy)))
        {
            *ntri1 = tri;
            *ncor1 = cor;
            DIFFTEST_RETURN((int)1);
        }

        lnk_tri = p_tri->tri[cor];
        if (lnk_tri == -1)
            DIFFTEST_RETURN((int)0);
        p_tri = &triangulation[0].Triangles[lnk_tri];
        if (p_tri->tri[0] == tri)
            lnk_cor = 0;
        else if (p_tri->tri[1] == tri)
            lnk_cor = 1;
        else if (p_tri->tri[2] == tri)
            lnk_cor = 2;
        else
            lnk_cor = -1;
        cor = (lnk_cor + 1) % 3;
        tri = lnk_tri;
    }
    while (tri != tri_start);

    DIFFTEST_RETURN((int)0);
}

static DIFFTEST_REF_FUNC TbBool two4_line_intersection_asm(int x1, int y1,
  int x2, int y2, int x3, int y3, int x4, int y4)
{
    TbBool ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];

    stkargs[0] = (int)(intptr_t)x3;
    stkargs[1] = (int)(intptr_t)y3;
    stkargs[2] = (int)(intptr_t)x4;
    stkargs[3] = (int)(intptr_t)y4;

    DIFFTEST_ASM(
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_two4_line_intersection\n"
        : "=a" (ret)
        : "a" (x1), "d" (y1), "b" (x2), "c" (y2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

TbBool two4_line_intersection(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
{
    s32 dx12, dx43, dy12, dy43;
    s32 dx31, dy31;
    s32 num_a, num_b, denom;
    short lo, hi;

    DIFFTEST_SHADOW_RET(two4_line_intersection, TbBool,
      two4_line_intersection_asm(x1, y1, x2, y2, x3, y3, x4, y4));

    // Bounding box rejection; the asm compares only the low 16 bits
    dx12 = x2 - x1;
    dx43 = x3 - x4;
    if (dx12 < 0) {
        lo = x2;
        hi = x1;
    } else {
        lo = x1;
        hi = x2;
    }
    if (dx43 > 0) {
        if (hi < (short)x4 || (short)x3 < lo)
            DIFFTEST_RETURN((TbBool)0);
    } else {
        if (hi < (short)x3 || (short)x4 < lo)
            DIFFTEST_RETURN((TbBool)0);
    }

    dy12 = y2 - y1;
    dy43 = y3 - y4;
    if (dy12 < 0) {
        lo = y2;
        hi = y1;
    } else {
        lo = y1;
        hi = y2;
    }
    if (dy43 > 0) {
        if (hi < (short)y4 || lo > (short)y3)
            DIFFTEST_RETURN((TbBool)0);
    } else {
        if (hi < (short)y3 || lo > (short)y4)
            DIFFTEST_RETURN((TbBool)0);
    }

    // Segment parameters along both lines must lie within [0, denom]
    dx31 = x1 - x3;
    dy31 = y1 - y3;
    num_a = dy43 * dx31 - dx43 * dy31;
    denom = dy12 * dx43 - dy43 * dx12;
    if (denom > 0) {
        if (num_a < 0 || num_a > denom)
            DIFFTEST_RETURN((TbBool)0);
    } else {
        if (num_a > 0 || num_a < denom)
            DIFFTEST_RETURN((TbBool)0);
    }

    num_b = dy31 * dx12 - dy12 * dx31;
    if (denom > 0) {
        if (num_b < 0 || num_b > denom)
            DIFFTEST_RETURN((TbBool)0);
    } else {
        if (num_b > 0 || num_b < denom)
            DIFFTEST_RETURN((TbBool)0);
    }

    // Parallel (collinear) segments return 1, proper crossing 2
    if (denom == 0)
        DIFFTEST_RETURN((TbBool)1);
    DIFFTEST_RETURN((TbBool)2);
}

/**
 * Finds point with given coords in list of triangles.
 * @param pt_x
 * @param pt_y
 * @param r_tri Output pointer for triangle index.
 * @param r_cor Output corner/tip index within the triangle.
 * @return Gives true if a point was found and output pointers set.
 */
TbBool point_find(TrCoord pt_x, TrCoord pt_y, int *r_tri, int *r_cor)
// TODO change types to TrTriangId *r_tri, TrTipId *r_cor
{
    TrTriangId tri;
    TrTipId cor;

    tri = triangle_find8(pt_x << 8, pt_y << 8);
    if (tri == -1) {
        LOGERR("triangle not found at (%d,%d)", (int)pt_x, (int)pt_y);
        return false;
    }

    for (cor = 0; cor < 3; cor++)
    {
        if (triangle_tip_equals(tri, cor, pt_x, pt_y))
        {
            *r_tri = tri;
            *r_cor = cor;
            return true;
        }
    }
    return false;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int point_quad_status_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_point_quad_status\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/**
 * Locates a point within the quad formed by triangle `tri` and its
 * neighbour across edge `cor`. Parameters come from eax, edx, ebx, ecx.
 * @return 0..3 if the point equals a quad corner, 4..7 if it lies on
 *  a quad edge line, -2 if outside, -1 if strictly inside.
 */
int point_quad_status(int tri_idx, int cor_idx, s32 x, s32 y)
{
    TrTriangId tri = tri_idx;
    TrTipId cor = cor_idx;
    struct TrTriangle *p_tri;
    struct TrTriangle *p_lnk;
    struct TrPoint *p_pt;
    TrTriangId lnk_tri;
    int lnk_cor;
    s32 ptA_x, ptA_y, ptB_x, ptB_y, ptC_x, ptC_y, ptD_x, ptD_y;
    sbyte cmp;

    DIFFTEST_SHADOW_RET(point_quad_status, int, point_quad_status_asm(tri_idx, cor_idx, x, y));
    p_tri = &triangulation[0].Triangles[tri];
    lnk_tri = p_tri->tri[cor];
    p_lnk = &triangulation[0].Triangles[lnk_tri];
    if (p_lnk->tri[0] == tri)
        lnk_cor = 0;
    else if (p_lnk->tri[1] == tri)
        lnk_cor = 1;
    else if (p_lnk->tri[2] == tri)
        lnk_cor = 2;
    else
        lnk_cor = -1;

    p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+2]]];
    ptA_x = p_pt->x;
    ptA_y = p_pt->y;
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor]]];
    ptB_x = p_pt->x;
    ptB_y = p_pt->y;
    p_pt = &triangulation[0].Points[(ushort)p_lnk->point[MOD3[lnk_cor+2]]];
    ptC_x = p_pt->x;
    ptC_y = p_pt->y;
    p_pt = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+1]]];
    ptD_x = p_pt->x;
    ptD_y = p_pt->y;

    if (x == ptA_x && y == ptA_y)
        DIFFTEST_RETURN((int)0);
    if (x == ptB_x && y == ptB_y)
        DIFFTEST_RETURN((int)1);
    if (x == ptC_x && y == ptC_y)
        DIFFTEST_RETURN((int)3);
    if (x == ptD_x && y == ptD_y)
        DIFFTEST_RETURN((int)2);

    cmp = path_compare_multiplications(ptB_x - ptA_x, y - ptB_y,
      ptB_y - ptA_y, x - ptB_x);
    if (cmp > 0)
        DIFFTEST_RETURN((int)-2);
    if (cmp == 0)
        DIFFTEST_RETURN((int)4);

    cmp = path_compare_multiplications(ptC_x - ptB_x, y - ptC_y,
      ptC_y - ptB_y, x - ptC_x);
    if (cmp > 0)
        DIFFTEST_RETURN((int)-2);
    if (cmp == 0)
        DIFFTEST_RETURN((int)6);

    cmp = path_compare_multiplications(ptD_x - ptC_x, y - ptD_y,
      ptD_y - ptC_y, x - ptD_x);
    if (cmp > 0)
        DIFFTEST_RETURN((int)-2);
    if (cmp == 0)
        DIFFTEST_RETURN((int)7);

    cmp = path_compare_multiplications(ptA_x - ptD_x, y - ptA_y,
      ptA_y - ptD_y, x - ptA_x);
    if (cmp > 0)
        DIFFTEST_RETURN((int)-2);
    if (cmp == 0)
        DIFFTEST_RETURN((int)5);

    DIFFTEST_RETURN((int)-1);
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void func_81db4_asm(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[2];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    DIFFTEST_ASM("push 4(%4)\n""push 0(%4)\n""call ASM_func_81db4\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Snaps the point (x8,y8), in 8-bit fixed point, into the quad of
 * triangle `tri_idx` and its neighbour across edge `cor_idx`: splits the
 * edge there if the point is strictly inside, otherwise takes the nearer
 * end point of the edge. Stores the result to *p_x, *p_y.
 * Parameters come from eax, edx, ebx, ecx and two stack args.
 */
void func_81db4(int tri_idx, int cor_idx, int x8, int y8, s32 *p_x, s32 *p_y)
{
    TrTriangId tri = tri_idx;
    TrTipId cor = cor_idx;
    s32 pt_x, pt_y, y;
    struct TrTriangle *p_tri;
    struct TrPoint *p_pt1;
    struct TrPoint *p_pt2;
    s32 dist1, dist2;

    DIFFTEST_SHADOW(func_81db4, func_81db4_asm(tri_idx, cor_idx, x8, y8,
      (intptr_t)p_x, (intptr_t)p_y));
    pt_x = x8 >> 8;
    y = y8 >> 8;
    *p_x = pt_x;
    *p_y = y;
    if (point_quad_status(tri, cor, *p_x, y) == -1)
    {
        edge_split(tri, cor, *p_x, *p_y);
        return;
    }
    pt_x = *p_x;
    pt_y = *p_y;
    p_tri = &triangulation[0].Triangles[tri];
    p_pt1 = &triangulation[0].Points[(ushort)p_tri->point[cor]];
    p_pt2 = &triangulation[0].Points[(ushort)p_tri->point[MOD3[cor+1]]];
    dist2 = (p_pt2->x - pt_x) * (p_pt2->x - pt_x)
      + (p_pt2->y - pt_y) * (p_pt2->y - pt_y);
    dist1 = (p_pt1->x - pt_x) * (p_pt1->x - pt_x)
      + (p_pt1->y - pt_y) * (p_pt1->y - pt_y);
    if (dist1 > dist2)
    {
        *p_x = p_pt2->x;
        *p_y = p_pt2->y;
        return;
    }
    *p_x = p_pt1->x;
    *p_y = p_pt1->y;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void line_intersect8_asm(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[6];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    stkargs[2] = a7;
    stkargs[3] = a8;
    stkargs[4] = a9;
    stkargs[5] = a10;
    DIFFTEST_ASM("push 20(%4)\n""push 16(%4)\n""push 12(%4)\n""push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_line_intersect8\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Intersection point of the line through (x1,y1)-(x2,y2) with the line
 * through (x3,y3)-(x4,y4), in 8-bit fixed point, stored to *p_out_x and
 * *p_out_y. For parallel lines, stores the first point instead.
 * Parameters come from eax, edx, ebx, ecx and six stack args.
 */
void line_intersect8(s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3,
  s32 x4, s32 y4, s32 *p_out_x, s32 *p_out_y)
{
    s32 tmp;
    s32 dx12, dy12, dx34, dy34, num, den;

    DIFFTEST_SHADOW(line_intersect8, line_intersect8_asm(x1, y1, x2, y2,
      x3, y3, x4, y4, (intptr_t)p_out_x, (intptr_t)p_out_y));
    if (x1 > x2) {
        tmp = x1; x1 = x2; x2 = tmp;
        tmp = y1; y1 = y2; y2 = tmp;
    }
    if (x3 > x4) {
        tmp = x3; x3 = x4; x4 = tmp;
        tmp = y3; y3 = y4; y4 = tmp;
    }
    // Make the second line relative to the first point
    x3 = (s32)((u32)x3 - (u32)x1);
    y3 = (s32)((u32)y3 - (u32)y1);
    x4 = (s32)((u32)x4 - (u32)x1);
    y4 = (s32)((u32)y4 - (u32)y1);
    dx34 = (s32)((u32)x4 - (u32)x3);
    dy34 = (s32)((u32)y4 - (u32)y3);
    dx12 = (s32)((u32)x2 - (u32)x1);
    dy12 = (s32)((u32)y2 - (u32)y1);
    num = (s32)((u32)dx34 * (u32)y3 - (u32)x3 * (u32)dy34);
    den = (s32)((u32)dx34 * (u32)dy12 - (u32)dy34 * (u32)dx12);
    if (den == 0) {
        *p_out_x = (s32)((u32)x1 << 8);
        *p_out_y = (s32)((u32)y1 << 8);
        return;
    }
    *p_out_x = (s32)(((u32)x1 << 8) +
      (u32)((s64)(s32)((u32)dx12 << 8) * num / den));
    *p_out_y = (s32)(((u32)y1 << 8) +
      (u32)((s64)(s32)((u32)dy12 << 8) * num / den));
}

static DIFFTEST_REF_FUNC void make_clip_list_asm(int x1, int y1, int x2, int y2)
{
    DIFFTEST_ASM(
      "call ASM_make_clip_list\n"
        : : "a" (x1), "d" (y1), "b" (x2), "c" (y2));
}

/** Walks the triangulation along the segment (x1,y1)-(x2,y2), and stores
 * in Ex[]/Ey[] the list of points where the segment should be split:
 * the start, every point at which it crosses a blocking triangle edge
 * (snapped to an existing point or inserted into the edge), and the end.
 */
void make_clip_list(int x1, int y1, int x2, int y2)
{
    struct TrTriangle *p_tri;
    struct TrTriangle *p_old;
    struct TrPoint *p_pt;
    struct TrPoint *p_ptC;
    struct TrPoint *p_ptD;
    int tri, cor;
    s32 pt_x, pt_y;
    s32 out_x, out_y;
    s32 dx, dy;
    s32 tri_old, cor_cur, lnk_cor;
    int blocked;
    sbyte cmp;

    DIFFTEST_SHADOW(make_clip_list, make_clip_list_asm(x1, y1, x2, y2));
    pt_x = x1;
    pt_y = y1;
    point_find(x1, y1, &tri, &cor);
    dy = y2 - y1;
    dx = x2 - x1;
    while (1)
    {
        Ex[ixE] = pt_x;
        Ey[ixE] = pt_y;
        ixE++;
        if (pt_x == x2 && pt_y == y2)
            return;

        cor_cur = pointed_at8(x2 << 8, y2 << 8, &tri, &cor);
        cor = cor_cur;
        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri].point[cor_cur]];
        pt_x = p_pt->x;
        pt_y = p_pt->y;
        if (path_compare_multiplications(dx, pt_y - y2, dy, pt_x - x2) == 0)
            continue;

        cor = MOD3[cor_cur+1];
        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri].point[cor]];
        pt_x = p_pt->x;
        pt_y = p_pt->y;
        if (path_compare_multiplications(dx, pt_y - y2, dy, pt_x - x2) == 0)
            continue;

        while (1)
        {
            tri_old = tri;
            p_old = &triangulation[0].Triangles[tri_old];
            tri = p_old->tri[cor_cur];
            p_tri = &triangulation[0].Triangles[tri];
            if (p_tri->tri[0] == tri_old)
                lnk_cor = 0;
            else if (p_tri->tri[1] == tri_old)
                lnk_cor = 1;
            else if (p_tri->tri[2] == tri_old)
                lnk_cor = 2;
            else
                lnk_cor = -1;
            cor = MOD3[lnk_cor+2];
            p_pt = &triangulation[0].Points[(ushort)p_tri->point[cor]];
            pt_x = p_pt->x;
            pt_y = p_pt->y;

            blocked = 1;
            if (p_old->solid == p_tri->solid &&
              (p_old->enter & (1 << (cor_cur & 0x1F))) != 0 &&
              (p_tri->enter & (1 << (lnk_cor & 0x1F))) != 0)
                blocked = 0;
            if (blocked)
                break;

            cmp = path_compare_multiplications(dx, pt_y - y2, dy, pt_x - x2);
            if (cmp > 0)
                cor_cur = MOD3[lnk_cor+2];
            else if (cmp < 0)
                cor_cur = MOD3[lnk_cor+1];
            else
                break;
        }
        if (!blocked)
            continue;

        // The segment crosses a blocking edge of the previous triangle
        p_old = &triangulation[0].Triangles[tri_old];
        p_ptC = &triangulation[0].Points[(ushort)p_old->point[cor_cur]];
        p_ptD = &triangulation[0].Points[(ushort)p_old->point[MOD3[cor_cur+1]]];
        line_intersect8(x1, y1, x2, y2, p_ptC->x, p_ptC->y, p_ptD->x, p_ptD->y,
          &out_x, &out_y);
        pt_x = (out_x + 0x80) >> 8;
        pt_y = (out_y + 0x80) >> 8;

        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri_old].point[cor_cur]];
        if (pt_x == p_pt->x && pt_y == p_pt->y)
        {
            tri = tri_old;
            cor = cor_cur;
            continue;
        }
        p_pt = &triangulation[0].Points[(ushort)triangulation[0].Triangles[tri].point[lnk_cor]];
        if (pt_x == p_pt->x && pt_y == p_pt->y)
        {
            cor = lnk_cor;
            continue;
        }
        func_81db4(tri_old, cor_cur, out_x, out_y,
          &pt_x, &pt_y);
        point_find(pt_x, pt_y, &tri, &cor);
    }
}

static DIFFTEST_REF_FUNC void make_clipped_edges_asm(int en1, int en2)
{
    DIFFTEST_ASM(
      "call ASM_make_clipped_edges\n"
        : : "a" (en1), "d" (en2));
}

void make_clipped_edges(int en1, int en2)
{
    s32 i;

    DIFFTEST_SHADOW(make_clipped_edges, make_clipped_edges_asm(en1, en2));
    for (i = 0; i < ixE - 1; i++)
    {
        struct TrTriangle *p_tri;
        struct TrTriangle *p_tri2;
        int tri, cor;
        s32 tri2, cor2;

        make_edge(Ex[i], Ey[i], Ex[i+1], Ey[i+1]);
        edge_find(Ex[i], Ey[i], Ex[i+1], Ey[i+1], &tri, &cor);
        p_tri = &triangulation[0].Triangles[tri];
        tri2 = p_tri->tri[cor];
        p_tri2 = &triangulation[0].Triangles[tri2];
        if (p_tri2->tri[0] == tri)
            cor2 = 0;
        else if (p_tri2->tri[1] == tri)
            cor2 = 1;
        else if (p_tri2->tri[2] == tri)
            cor2 = 2;
        else
            cor2 = -1;
        // Shift counts are masked to 5 bits as by x86 8-bit shifts;
        // with cor2 == -1 the second update leaves the flags as they were.
        p_tri->enter = (ubyte)((p_tri->enter & (ubyte)~(ubyte)(1u << (cor & 0x1F)))
          | (ubyte)((u32)en1 << (cor & 0x1F)));
        p_tri2->enter = (ubyte)((p_tri2->enter & (ubyte)~(ubyte)(1u << (cor2 & 0x1F)))
          | (ubyte)((u32)en2 << (cor2 & 0x1F)));
    }
}

void thin_wall(int x1, int y1, int x2, int y2, ubyte en1, ubyte en2)
{
    thin_wall_x1 = x1;
    thin_wall_y1 = y1;
    thin_wall_x2 = x2;
    thin_wall_y2 = y2;
    if (!insert_point(x1, y1)) {
        LOGERR("point1 (%d,%d) insertion failed", x1, y1);
        return;
    }
    if (!insert_point(x2, y2)) {
        LOGERR("point2 (%d,%d) insertion failed", x2, y2);
        return;
    }
    if (x1 != x2 || y1 != y2)
    {
        ixE = 0;
        make_clip_list(x1, y1, x2, y2);
        make_clipped_edges(en1, en2);
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int triangle_route_do_unkn7_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_triangle_route_do_unkn7\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

extern ubyte tag_current;
/** Binary heap of triangles, ordered by tree_val[]; index 0 unused,
 * heap[1] (data_1c30c0 in asm) is the top. */
extern s32 heap[];
extern s32 heap_end;

/** Adds a triangle to the route heap and sifts it up.
 */
static void triangle_route_unkn7_heap_add(s32 tri)
{
    s32 i, par, sib, chosen, tmp, val;

    if (heap_end < 0x100)
        heap_end++;
    i = heap_end;
    heap[i] = tri;
    if (i == 1)
        return;
    if ((i & 1) == 0)
    {
        par = i >> 1;
        if (tree_val[heap[i]] > tree_val[heap[par]])
            return;
        tmp = heap[par];
        heap[par] = heap[i];
        heap[i] = tmp;
        i = par;
    }
    while (i != 1)
    {
        par = i >> 1;
        sib = i ^ 1;
        val = tree_val[heap[i]];
        if (val < tree_val[heap[sib]])
        {
            if (val < tree_val[heap[par]])
                chosen = i;
            else
                chosen = par;
        }
        else
        {
            if (tree_val[heap[sib]] < tree_val[heap[par]])
                chosen = sib;
            else
                chosen = par;
        }
        if (chosen == par)
            break;
        tmp = heap[par];
        heap[par] = heap[chosen];
        heap[chosen] = tmp;
        i = par;
    }
}

/** Removes the top triangle from the route heap and sifts down.
 */
static void triangle_route_unkn7_heap_pop(void)
{
    s32 i, n, lt, rt, chosen, tmp, val;

    n = heap_end;
    heap[1] = heap[n];
    heap_end = n - 1;
    heap[n] = -1;
    if (n - 1 == 0)
        return;
    i = 1;
    while (1)
    {
        lt = 2 * i;
        rt = lt ^ 1;
        if (rt > heap_end)
        {
            n = heap_end;
            if (i == n)
                return;
            if (tree_val[heap[i]] < tree_val[heap[n]])
                return;
            tmp = heap[i];
            heap[i] = heap[n];
            heap[n] = tmp;
            return;
        }
        val = tree_val[heap[i]];
        if (val < tree_val[heap[lt]])
        {
            if (val >= tree_val[heap[rt]])
                chosen = rt;
            else
                chosen = i;
        }
        else
        {
            if (tree_val[heap[lt]] >= tree_val[heap[rt]])
                chosen = rt;
            else
                chosen = lt;
        }
        if (chosen == i)
            return;
        tmp = heap[i];
        heap[i] = heap[chosen];
        heap[chosen] = tmp;
        i = chosen;
    }
}

/** Distance estimate from (ax, ay) to given triangulation point.
 */
static s32 triangle_route_unkn7_pt_dist(s32 ax, s32 ay, ushort pt)
{
    struct TrPoint *p_point;
    s32 dx, dy;

    p_point = &triangulation[0].Points[pt];
    dx = abs(ax - p_point->x);
    dy = abs(ay - p_point->y);
    if (dx > dy)
        return dx + (dy >> 1);
    else
        return (dx >> 1) + dy;
}

/** Best-first search over triangulation from triangle tri_beg towards tri_dst,
 * using distance to (tree_Ax8, tree_Ay8) as heap key.
 * Expands like triangle_route_do_unkn5() (3 siblings and the jump
 * triangle, as enabled by bits 0-3 of `enter`), one triangle per step.
 * When the destination is first reached, its tag is changed so it can
 * be reached again, and the search goes on for up to 29 more steps,
 * letting the destination get a different parent; if it is not popped
 * again in that time, its original parent is restored.
 * Parameters (from eax, edx, ebx; ecx is unused): destination tri, start
 * tri, route output (from tri_dst back to tri_beg).
 * Returns number of route steps, or -1 if not found.
 */
int triangle_route_do_unkn7(s32 tri_dst, s32 tri_beg, s32 *p_route)
{
    struct TrTriangle *p_tri;
    s32 tri_cur, tri_nxt;
    s32 ax, ay, dist, d;
    s32 extra_steps, dst_dad;
    int k, n;

    DIFFTEST_SHADOW_RET(triangle_route_do_unkn7, int,
      triangle_route_do_unkn7_asm(tri_dst, tri_beg, (intptr_t)p_route, 0));

    extra_steps = 0;
    dst_dad = 0;

    tag_current++;
    heap[1] = -1;
    heap_end = 0;
    if (tag_current == 0)
    {
        memset(routeTags, 0, 8000);
        tag_current = 1;
    }

    tree_val[tri_beg] = 1;
    tree_dad[tri_beg] = tri_beg;
    routeTags[tri_beg] = tag_current;
    triangle_route_unkn7_heap_add(tri_beg);

    while (1)
    {
        tri_cur = heap[1];
        if (tri_cur == -1)
            break;
        if (tri_cur == tri_dst)
        {
            if (extra_steps != 0)
                break;
            dst_dad = tree_dad[tri_dst];
            routeTags[tri_dst] = (ubyte)(tag_current + 1);
            extra_steps = 30;
            triangle_route_unkn7_heap_pop();
            continue;
        }
        if (extra_steps > 1)
        {
            extra_steps--;
            if (extra_steps == 1)
            {
                tree_dad[tri_dst] = dst_dad;
                break;
            }
        }
        triangle_route_unkn7_heap_pop();

        for (k = 0; k < 4; k++)
        {
            p_tri = &triangulation[0].Triangles[tri_cur];
            if ((p_tri->enter & (1 << k)) == 0)
                continue;
            // k == 3 reads the jump field, which follows tri[]
            if (k < 3)
                tri_nxt = p_tri->tri[k];
            else
                tri_nxt = p_tri->jump;
            if (tri_nxt == -1)
                continue;
            if (routeTags[tri_nxt] == tag_current)
                continue;
            p_tri = &triangulation[0].Triangles[tri_nxt];
            ax = tree_Ax8 >> 8;
            ay = tree_Ay8 >> 8;
            dist = triangle_route_unkn7_pt_dist(ax, ay, (ushort)p_tri->point[0]);
            d = triangle_route_unkn7_pt_dist(ax, ay, (ushort)p_tri->point[1]);
            if (d < dist)
                dist = d;
            d = triangle_route_unkn7_pt_dist(ax, ay, (ushort)p_tri->point[2]);
            if (d < dist)
                dist = d;

            routeTags[tri_nxt] = tag_current;
            tree_val[tri_nxt] = dist;
            tree_dad[tri_nxt] = tri_cur;
            triangle_route_unkn7_heap_add(tri_nxt);
        }
    }

    if (heap[1] == -1)
        DIFFTEST_RETURN((int)-1);

    tri_cur = tri_dst;
    n = 0;
    while (1)
    {
        *p_route = tri_cur;
        if (tri_cur == tri_beg)
            break;
        p_route++;
        n++;
        tri_cur = tree_dad[tri_cur];
    }
    DIFFTEST_RETURN((int)n);
}


static DIFFTEST_REF_FUNC s32 ma_triangle_route_4_asm(int triA, int triB, s32 *p_treert)
{
    s32 ret;
    int rd = triB, rb = (int)(intptr_t)p_treert, rc = 0;
    DIFFTEST_ASM("call ASM_ma_triangle_route_4\n"
        : "=a" (ret), "+d" (rd), "+b" (rb), "+c" (rc) : "a" (triA) : "cc", "memory");
    return ret;
}

extern struct Path fwd_path;

s32 route_to_path(int ax8, int ay8, int bx8, int by8, s32 *p_treert,
  s32 treert_len, struct Path *p_path, int *p_len2);

/** Finds a triangle route from triA to triB by searching backwards from
 * triB with triangle_route_do_unkn7() (A and B points swapped meanwhile),
 * checks it converts to a path, and stores it reversed in p_treert.
 * Returns the route length, or -1 (then the points stay swapped).
 */
s32 ma_triangle_route_4(int triA, int triB, s32 *p_treert)
{
    s32 *p_dst;
    s32 ret;
    s32 tmp;
    int len;
    s32 i;

    DIFFTEST_SHADOW_RET(ma_triangle_route_4, s32,
      ma_triangle_route_4_asm(triA, triB, p_treert));

    tmp = tree_Ax8;
    tree_Ax8 = tree_Bx8;
    tree_Bx8 = tmp;
    tmp = tree_Ay8;
    tree_Ay8 = tree_By8;
    tree_By8 = tmp;

    // TODO the asm passes its incoming ecx as a4; unused by the callee
    ret = triangle_route_do_unkn7(triB, triA, route_bak);
    if (ret == -1)
        DIFFTEST_RETURN((s32)-1);

    if (route_to_path(tree_Ax8, tree_Ay8, tree_Bx8, tree_By8, route_bak,
      ret, &fwd_path, &len) == -1)
        DIFFTEST_RETURN((s32)-1);

    tmp = tree_Ax8;
    tree_Ax8 = tree_Bx8;
    tree_Bx8 = tmp;
    tmp = tree_Ay8;
    tree_Ay8 = tree_By8;
    tree_By8 = tmp;

    // Store the backward route reversed
    p_dst = p_treert;
    for (i = ret; i >= 0; i--)
        *p_dst++ = route_bak[i];

    DIFFTEST_RETURN(ret);
}
static DIFFTEST_REF_FUNC int fov_region_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_fov_region\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Sign of (m1 * m2 - m3 * m4), with full 64-bit products.
 */
static int fov_cross_sign(s32 m1, s32 m2, s32 m3, s32 m4)
{
    s64 prod1, prod2;

    prod1 = (s64)m1 * (s64)m2;
    prod2 = (s64)m3 * (s64)m4;
    if (prod1 > prod2)
        return 1;
    if (prod1 < prod2)
        return -1;
    return 0;
}

/** Routed asm (internal label). Takes the point (x,y) (eax, edx) and a
 * pointer p_fov (ebx) to 6 coordinates: apex (p_fov[0],p_fov[1]) and two
 * ray end points (p_fov[2],p_fov[3]), (p_fov[4],p_fov[5]). Returns -1, 0
 * or 1 depending on which side of the rays the point lies. Register ecx
 * is not used.
 */
int fov_region(s32 x, s32 y, s32 *p_fov)
{
    int sgn;

    DIFFTEST_SHADOW_RET(fov_region, int, fov_region_asm(x, y, (intptr_t)p_fov, 0));
    // Differences wrap at 32 bits, products are full 64-bit
    sgn = fov_cross_sign((s32)((u32)p_fov[2] - (u32)p_fov[0]), (s32)((u32)y - (u32)p_fov[3]),
      (s32)((u32)p_fov[3] - (u32)p_fov[1]), (s32)((u32)x - (u32)p_fov[2]));
    if (sgn < 0)
        DIFFTEST_RETURN((int)-1);
    sgn = fov_cross_sign((s32)((u32)p_fov[4] - (u32)p_fov[0]), (s32)((u32)y - (u32)p_fov[5]),
      (s32)((u32)p_fov[5] - (u32)p_fov[1]), (s32)((u32)x - (u32)p_fov[4]));
    if (sgn > 0)
        DIFFTEST_RETURN((int)1);
    DIFFTEST_RETURN((int)0);
}
static DIFFTEST_REF_FUNC int edge_points8_asm(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[2];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    DIFFTEST_ASM("push 4(%4)\n""push 0(%4)\n""call ASM_edge_points8\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Finds the edge of triangle tri shared with
 * sibling triangle tri_sibl (directly, or through the jump link if enabled
 * in enter flags), and stores the coordinates of its two end points, scaled
 * by 256, at p_x1,p_y1 (first point) and p_x2,p_y2 (second point).
 * Parameters come from eax, edx, ebx, ecx and two stack args.
 * Returns 0, or -42 if the triangles are not connected.
 */
int edge_points8(int tri, int tri_sibl, s32 *p_x1, s32 *p_y1, s32 *p_x2, s32 *p_y2)
{
    struct TrTriangle *p_tri;
    struct TrPoint *p_points;
    int cor1, cor2;

    DIFFTEST_SHADOW_RET(edge_points8, int, edge_points8_asm(tri, tri_sibl,
      (intptr_t)p_x1, (intptr_t)p_y1, (intptr_t)p_x2, (intptr_t)p_y2));
    p_tri = &triangulation[0].Triangles[tri];
    if (p_tri->tri[0] == tri_sibl || (p_tri->jump == tri_sibl && (p_tri->enter & 0x10) != 0))
    {
        cor1 = 1;
        cor2 = 0;
    }
    else if (p_tri->tri[1] == tri_sibl || (p_tri->jump == tri_sibl && (p_tri->enter & 0x20) != 0))
    {
        cor1 = 2;
        cor2 = 1;
    }
    else if (p_tri->tri[2] == tri_sibl || (p_tri->jump == tri_sibl && (p_tri->enter & 0x40) != 0))
    {
        cor1 = 0;
        cor2 = 2;
    }
    else
    {
        DIFFTEST_RETURN((int)-42);
    }
    p_points = triangulation[0].Points;
    *p_x1 = (s32)((u32)p_points[(ushort)p_tri->point[cor1]].x << 8);
    *p_y1 = (s32)((u32)p_points[(ushort)p_tri->point[cor1]].y << 8);
    *p_x2 = (s32)((u32)p_points[(ushort)p_tri->point[cor2]].x << 8);
    *p_y2 = (s32)((u32)p_points[(ushort)p_tri->point[cor2]].y << 8);
    DIFFTEST_RETURN((int)0);
}



static DIFFTEST_REF_FUNC s32 route_to_path_asm(int ax8, int ay8, int bx8, int by8,
  s32 *p_treert, s32 treert_len, struct Path *p_path, int *p_len2)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];
    s32 ret;

    stkargs[0] = (int)(intptr_t)p_treert;
    stkargs[1] = (int)(intptr_t)treert_len;
    stkargs[2] = (int)(intptr_t)p_path;
    stkargs[3] = (int)(intptr_t)p_len2;

    DIFFTEST_ASM(
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_route_to_path\n"
        : "=a" (ret) : "a" (ax8), "d" (ay8), "b" (bx8), "c" (by8), "S" (stkargs)
        : "cc", "memory");

    return ret;
}

/** Converts a route through triangles into a list of way points, by
 * funnelling the view from the current point through the edges shared
 * by consecutive triangles of the route. Fills p_path and adds the
 * path length to *p_len2. Returns the number of way points, or -1.
 */
s32 route_to_path(int ax8, int ay8, int bx8, int by8, s32 *p_treert,
  s32 treert_len, struct Path *p_path, int *p_len2)
{
    // Current point, left and right edge points; layout used by fov_region()
    s32 fov[6];
    s32 ex1, ey1, ex2, ey2;
    s32 lt_idx, rt_idx;
    s32 i, n;
    int reg1, reg2;

    DIFFTEST_SHADOW_RET(route_to_path, s32, route_to_path_asm(ax8, ay8,
      bx8, by8, p_treert, treert_len, p_path, p_len2));

    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->bx8 = bx8;
    p_path->by8 = by8;
    n = 0;
    *p_len2 = 0;
    fov[0] = ax8;
    fov[1] = ay8;

    if (treert_len == 0)
    {
        *p_len2 += LbSqrL((by8 - ay8) * (by8 - ay8) + (bx8 - ax8) * (bx8 - ax8));
        p_path->PathLength = 1;
        p_path->WayPoints[0].x8 = bx8;
        p_path->WayPoints[0].y8 = by8;
        DIFFTEST_RETURN((s32)1);
    }

    rt_idx = 0;
    lt_idx = 0;
    if (edge_points8(p_treert[0], p_treert[1], &fov[2],
      &fov[3], &fov[4], &fov[5]) == -42)
        DIFFTEST_RETURN((s32)-1);

    for (i = 1; ; i++)
    {
        if (i < treert_len)
        {
            if (edge_points8(p_treert[i], p_treert[i + 1], &ex1,
              &ey1, &ex2, &ey2) == -42)
                DIFFTEST_RETURN((s32)-1);
            reg1 = fov_region(ex1, ey1, fov);
            reg2 = fov_region(ex2, ey2, fov);
        }
        else
        {
            reg1 = fov_region(bx8, by8, fov);
            reg2 = reg1;
            if (reg1 == 0)
            {
                // Destination is visible: final way point
                if (n == 256)
                    DIFFTEST_RETURN((s32)-1);
                *p_len2 += LbSqrL((by8 - fov[1]) * (by8 - fov[1])
                  + (bx8 - fov[0]) * (bx8 - fov[0]));
                p_path->WayPoints[n].x8 = bx8;
                p_path->WayPoints[n].y8 = by8;
                n++;
                p_path->PathLength = n;
                DIFFTEST_RETURN(n);
            }
            // Oscillating between two points
            if ((n >= 3) && (p_path->WayPoints[n - 1].x8 == p_path->WayPoints[n - 3].x8)
              && (p_path->WayPoints[n - 1].y8 == p_path->WayPoints[n - 3].y8))
                DIFFTEST_RETURN((s32)-1);
        }

        if (reg1 == 0)
        {
            fov[2] = ex1;
            fov[3] = ey1;
            lt_idx = i;
        }
        if (reg2 == 0)
        {
            fov[4] = ex2;
            fov[5] = ey2;
            rt_idx = i;
        }

        if (reg2 == -1)
        {
            if (n == 256)
                DIFFTEST_RETURN((s32)-1);
            *p_len2 += LbSqrL((fov[3] - fov[1]) * (fov[3] - fov[1])
              + (fov[2] - fov[0]) * (fov[2] - fov[0]));
            p_path->WayPoints[n].x8 = fov[2];
            p_path->WayPoints[n].y8 = fov[3];
            fov[0] = fov[2];
            fov[1] = fov[3];
            i = lt_idx;
            n++;
            if (edge_points8(p_treert[i], p_treert[i + 1], &fov[2],
              &fov[3], &fov[4], &fov[5]) == -42)
                DIFFTEST_RETURN((s32)-1);
        }
        else if (reg1 == 1)
        {
            if (n == 256)
                DIFFTEST_RETURN((s32)-1);
            *p_len2 += LbSqrL((fov[5] - fov[1]) * (fov[5] - fov[1])
              + (fov[4] - fov[0]) * (fov[4] - fov[0]));
            p_path->WayPoints[n].x8 = fov[4];
            p_path->WayPoints[n].y8 = fov[5];
            fov[0] = fov[4];
            fov[1] = fov[5];
            i = rt_idx;
            n++;
            if (edge_points8(p_treert[i], p_treert[i + 1], &fov[2],
              &fov[3], &fov[4], &fov[5]) == -42)
                DIFFTEST_RETURN((s32)-1);
        }
    }
}

void path_init8_unkn3(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6)
{
    navi_onscreen_debug(0);

    p_path->PathLength = 0;
    p_path->ax8 = ax8;
    p_path->ay8 = ay8;
    p_path->by8 = by8;
    p_path->bx8 = bx8;

    tree_Ax8 = ax8;
    tree_Ay8 = ay8;
    tree_Bx8 = bx8;
    tree_By8 = by8;
    tree_routelen = -1;

    tree_triA = triangle_findSE8(ax8, ay8);
    if (tree_triA != -1) {
        struct TrTriangle *p_tri;

        p_tri = &triangulation[0].Triangles[tree_triA];
        tree_altA = p_tri->solid;
    } else {
        tree_altA = 0;
    }

    tree_triB = triangle_findSE8(bx8, by8);
    if (tree_triB != -1) {
        struct TrTriangle *p_tri;

        p_tri = &triangulation[0].Triangles[tree_triB];
        tree_altB = p_tri->solid;
    } else {
        tree_altB = 0;
    }

    if ((tree_triA != -1) && (tree_triB != -1))
    {
        s32 len2;

        tree_routelen = ma_triangle_route_4(tree_triA, tree_triB, tree_route);
        if (tree_routelen != -1) {
            p_path->PathLength = route_to_path(ax8, ay8, bx8, by8,
              tree_route, tree_routelen, p_path, &len2);
        }
    }
}

static DIFFTEST_REF_FUNC int unused_func_115_asm(int X1, int Z1, int X2, int Z2)
{
    int ret;
    DIFFTEST_ASM("call ASM_unused_func_115\n"
        : "=a" (ret) : "a" (X1), "d" (Z1), "b" (X2), "c" (Z2));
    return ret;
}

/** Checks whether the line from (X1,Z1) to (X2,Z2) leaves the tiles marked
 * with MEF2_Unkn01 or MEF2_Unkn04. Returns 1 if any sampled tile (including
 * both ends) lacks these flags, or on map 11; 0 if all samples have them.
 */
int unused_func_115(int X1, int Z1, int X2, int Z2)
{
    struct MyMapElement *p_mapel;
    s32 dx, dz, adx, adz, dist, steps, step_x, step_z, x, z;

    DIFFTEST_SHADOW_RET(unused_func_115, int, unused_func_115_asm(X1, Z1, X2, Z2));
    if (current_map == 11)
        DIFFTEST_RETURN((int)1);

    p_mapel = &game_my_big_map[((Z1 >> 8) << 7) + (X1 >> 8)];
    if ((p_mapel->Flags2 & (MEF2_Unkn01|MEF2_Unkn04)) == 0)
        DIFFTEST_RETURN((int)1);
    p_mapel = &game_my_big_map[((Z2 >> 8) << 7) + (X2 >> 8)];
    if ((p_mapel->Flags2 & (MEF2_Unkn01|MEF2_Unkn04)) == 0)
        DIFFTEST_RETURN((int)1);

    dx = X2 - X1;
    dz = Z2 - Z1;
    if ((dx | dz) == 0)
        DIFFTEST_RETURN((int)0);

    // Approximate length of the vector
    adz = abs(dz);
    adx = abs(dx);
    if (adx < adz)
        dist = adz - (adz >> 5) - (adz >> 7)
          + (adx >> 2) + (adx >> 3) + (adx >> 6) + (adx >> 7);
    else
        dist = adx - (adx >> 5) - (adx >> 7)
          + (adz >> 2) + (adz >> 3) + (adz >> 6) + (adz >> 7);

    steps = dist >> 4;
    if (steps == 0)
        DIFFTEST_RETURN((int)0);
    step_x = dx / steps;
    step_z = dz / steps;

    x = X1;
    z = Z1;
    for (; steps >= 0; steps--)
    {
        p_mapel = &game_my_big_map[((z >> 8) << 7) + (x >> 8)];
        if ((p_mapel->Flags2 & (MEF2_Unkn01|MEF2_Unkn04)) == 0)
            DIFFTEST_RETURN((int)1);
        x += step_x;
        z += step_z;
    }
    DIFFTEST_RETURN((int)0);
}
static DIFFTEST_REF_FUNC int func_5cff0_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_func_5cff0\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Snaps the point (*p_x,*p_z) (eax, edx) to
 * one of two reference points if it lies closer than distance 32 to it.
 * p_vect (ebx) points to shorts (a struct ColVect); the first point is
 * (p_vect[0],p_vect[2]), the second (p_vect[3],p_vect[5]) - the asm reads
 * most of them as high halves of unaligned 32-bit loads.
 * Returns 1 if the point was snapped, 0 otherwise. Register ecx is not used.
 */
int func_5cff0(short *p_x, short *p_z, short *p_vect)
{
    s32 x, z, dx, dz, dist2;

    DIFFTEST_SHADOW_RET(func_5cff0, int, func_5cff0_asm((intptr_t)p_x,
      (intptr_t)p_z, (intptr_t)p_vect, 0));
    x = *p_x;
    z = *p_z;
    // Products wrap at 32 bits like imul
    dx = (s32)((u32)x - (u32)p_vect[0]);
    dz = (s32)((u32)z - (u32)p_vect[2]);
    dist2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (dist2 < 0x400) {
        *p_x -= (short)dx;
        *p_z -= (short)dz;
        DIFFTEST_RETURN((int)1);
    }
    dx = (s32)((u32)x - (u32)p_vect[3]);
    dz = (s32)((u32)z - (u32)p_vect[5]);
    dist2 = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (dist2 < 0x400) {
        *p_x -= (short)dx;
        *p_z -= (short)dz;
        DIFFTEST_RETURN((int)1);
    }
    DIFFTEST_RETURN((int)0);
}


static DIFFTEST_REF_FUNC short collide_coords_asm(short *p_X, short *p_Y, short *p_Z)
{
    int ra = (int)(intptr_t)p_X, rd = (int)(intptr_t)p_Y, rb = (int)(intptr_t)p_Z;
    DIFFTEST_ASM("call ASM_collide_coords\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
    return (short)ra;
}

/** Walks a collision vector chain starting at *p_ax, snapping the point
 * with func_5cff0() to vector ends. Returns true on the first snap.
 * Leaves in *p_ax the low word of eax the asm loop ends with.
 */
static TbBool collide_coords_col_list(short *p_X, short *p_Z, ushort *p_ax)
{
    ushort cvlist;
    ushort n;
    int ret;

    cvlist = *p_ax;
    if (cvlist == 0)
        return false;
    for (n = 0; n < 200; n++)
    {
        struct ColVect *p_colvect;

        p_colvect = &game_col_vects[game_col_vects_list[cvlist].Vect];
        ret = func_5cff0(p_X, p_Z, (short *)p_colvect);
        if ((short)ret != 0) {
            *p_ax = (ushort)ret;
            return true;
        }
        cvlist = game_col_vects_list[cvlist].NextColList & 0x7FFF;
        *p_ax = cvlist;
        if (cvlist == 0)
            break;
    }
    return false;
}

/** Gets the map element at index idx with the asm bounds check.
 * Returns NULL if out of bounds, and then sets *p_ax to the low word the
 * asm leaves in eax: the map base (or end) if base_in_ax, else the
 * element address.
 */
static struct MyMapElement *collide_coords_mapel(s32 idx, TbBool base_in_ax, ushort *p_ax)
{
    uintptr_t base, addr;

    base = (uintptr_t)game_my_big_map;
    addr = base + (uintptr_t)((intptr_t)idx * (intptr_t)sizeof(struct MyMapElement));
    if (addr <= base) {
        *p_ax = base_in_ax ? (ushort)base : (ushort)addr;
        return NULL;
    }
    base += 0x48000;
    if (addr >= base) {
        *p_ax = base_in_ax ? (ushort)base : (ushort)addr;
        return NULL;
    }
    return (struct MyMapElement *)addr;
}

/** Snaps the point (*p_X,*p_Z) to near collision vector ends in its map
 * tile and in the neighbouring tiles it lies close to. p_Y is not used.
 * The return value is the low word of eax at asm exit; only non-zero
 * after a snap is meaningful (callers ignore it).
 */
short collide_coords(short *p_X, short *p_Y, short *p_Z)
{
    struct MyMapElement *p_mapel;
    s32 dx, dz;
    s32 tile_x, tile_z;
    ushort ax;

    DIFFTEST_SHADOW_RET(collide_coords, short, collide_coords_asm(p_X, p_Y, p_Z));
    dx = 0;
    dz = 0;
    ax = *p_X & 0xFF;
    if ((short)ax < 0x40)
        dx = -1;
    else if ((short)ax > 0xC0)
        dx = 1;
    ax = *p_Z & 0xFF;
    if ((short)ax < 0x40)
        dz = -1;
    else if ((short)ax > 0xC0)
        dz = 1;

    tile_x = *p_X >> 8;
    if (tile_x >= 0 && tile_x < 128)
    {
        tile_z = *p_Z >> 8;
        ax = (ushort)tile_z;
        if (tile_z >= 0 && tile_z < 128)
        {
            p_mapel = &game_my_big_map[(tile_z << 7) + tile_x];
            ax = p_mapel->ColHead;
            if (collide_coords_col_list(p_X, p_Z, &ax))
                DIFFTEST_RETURN((short)ax);
        }
    }

    if (dx != 0)
    {
        p_mapel = collide_coords_mapel(dx + (*p_X >> 8) + ((*p_Z >> 8) << 7), true, &ax);
        if (p_mapel != NULL)
        {
            ax = p_mapel->ColHead;
            if (collide_coords_col_list(p_X, p_Z, &ax))
                DIFFTEST_RETURN((short)ax);
        }
    }

    if (dz != 0)
    {
        p_mapel = collide_coords_mapel((((*p_Z >> 8) + dz) << 7) + (*p_X >> 8), true, &ax);
        if (p_mapel != NULL)
        {
            ax = p_mapel->ColHead;
            if (collide_coords_col_list(p_X, p_Z, &ax))
                DIFFTEST_RETURN((short)ax);
        }
    }

    if (dz == 0 || dx == 0)
        DIFFTEST_RETURN((short)ax);
    p_mapel = collide_coords_mapel((((*p_Z >> 8) + dz) << 7) + (*p_X >> 8) + dx, false, &ax);
    if (p_mapel != NULL)
    {
        ax = p_mapel->ColHead;
        collide_coords_col_list(p_X, p_Z, &ax);
    }
    DIFFTEST_RETURN((short)ax);
}

void thin_wall_at_line(int X1, int Y1, int Z1, int X2, int Y2, int Z2, short face, ushort colt, ubyte pass)
{
    short cX1, cY1, cZ1;
    short cX2, cY2, cZ2;

    if (!unused_func_115(X1, Z1, X2, Z2))
        return;

    if (X1 < 0 || X1 > 32768)
        return;
    if (X2 < 0 || X2 > 32768)
        return;
    if (Z1 < 0 || Z1 > 32768)
        return;
    if (Z2 < 0 || Z2 > 32768)
        return;

    cX1 = X1;
    cY1 = Y1;
    cZ1 = Z1;

    cX2 = X2;
    cY2 = Y2;
    cZ2 = Z2;

    if (colt != 2)
    {
        collide_coords(&cX1, &cY1, &cZ1);
        collide_coords(&cX2, &cY2, &cZ2);
    }
    thin_wall(cX1, cZ1, cX2, cZ2, pass, pass);
}

/* Needed only for ASM - remove pending.
*/
void thin_wall_at_line_rm(int X1, int Y1, int Z1, int X2, int Y2, int Z2, short face, ushort colt)
{
    thin_wall_at_line(X1, Y1, Z1, X2, Y2, Z2, face, colt, THIN_PASS);
}

static ubyte face_is_blocking(ushort obj, short face, ushort colt)
{
    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        if ((p_face->GFlags & FGFlg_Unkn10) != 0)
            return THIN_PASS;
        return THIN_BLOCK;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        if ((p_face->GFlags & FGFlg_Unkn10) != 0)
            return THIN_PASS;
        return THIN_BLOCK;
    }
    return THIN_PASS;
}

#define TOLERANCE 150

void thin_wall_around_face3(short obj_x, short obj_y, short obj_z, short face, ushort colt, ubyte pass)
{
    struct SingleObjectFace3 *p_face;
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    short cor;

    p_face = &game_object_faces3[face];

    for (cor = 0; cor < 3; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y;
        z_cor[cor] = obj_z + p_pt->Z;
    }
    for (cor = 0; cor < 3; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = PRCCOORD_TO_YCOORD(alt);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], face, colt, pass);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[2], y_cor[2], z_cor[2], face, colt, pass);
    }
    if (alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[1], y_cor[1], z_cor[1],
          x_cor[2], y_cor[2], z_cor[2], face, colt, pass);
    }
}

void thin_wall_around_face4(short obj_x, short obj_y, short obj_z, short face, ushort colt, ubyte pass)
{
    struct SingleObjectFace4 *p_face;
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    short cor;

    p_face = &game_object_faces4[face];

    for (cor = 0; cor < 4; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y;
        z_cor[cor] = obj_z + p_pt->Z;
    }
    for (cor = 0; cor < 4; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = PRCCOORD_TO_YCOORD(alt);
    }
    if (alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]
      && alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]) {
        thin_wall_at_line(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], -face, colt, pass);
    }
    if (alt_cor[1] - TOLERANCE < y_cor[1] && alt_cor[1] + TOLERANCE > y_cor[1]
      && alt_cor[3] - TOLERANCE < y_cor[3] && alt_cor[3] + TOLERANCE > y_cor[3]) {
        thin_wall_at_line(x_cor[1], y_cor[1], z_cor[1],
          x_cor[3], y_cor[3], z_cor[3], -face, colt, pass);
    }
    if (alt_cor[3] - TOLERANCE < y_cor[3] && alt_cor[3] + TOLERANCE > y_cor[3]
      && alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]) {
        thin_wall_at_line(x_cor[3], y_cor[3], z_cor[3],
          x_cor[2], y_cor[2], z_cor[2], -face, colt, pass);
    }
    if (alt_cor[2] - TOLERANCE < y_cor[2] && alt_cor[2] + TOLERANCE > y_cor[2]
      && alt_cor[0] - TOLERANCE < y_cor[0] && alt_cor[0] + TOLERANCE > y_cor[0]) {
        thin_wall_at_line(x_cor[2], y_cor[2], z_cor[2],
          x_cor[0], y_cor[0], z_cor[0], -face, colt, pass);
    }
}

#undef TOLERANCE

void thin_wall_around_object(ushort obj, ushort colt)
{
    short obj_x, obj_y, obj_z;
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_y = p_obj->OffsetY;
        obj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        ubyte pass;
        pass = face_is_blocking(obj, face, colt);
        thin_wall_around_face3(obj_x, obj_y, obj_z, face, colt, pass);
    }
    for (face = startface4; face < endface4; face++)
    {
        ubyte pass;
        pass = face_is_blocking(obj, -face, colt);
        thin_wall_around_face4(obj_x, obj_y, obj_z, face, colt, pass);
    }
}

void thin_wall_around_object_rm(ushort obj, ushort colt)
{
    short obj_x, obj_y, obj_z;
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_y = p_obj->OffsetY;
        obj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        thin_wall_around_face3(obj_x, obj_y, obj_z, face, colt, THIN_PASS);
    }
    for (face = startface4; face < endface4; face++)
    {
        thin_wall_around_face4(obj_x, obj_y, obj_z, face, colt, THIN_PASS);
    }
}

void brute_fill_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
    int tri;

    for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
    {
        struct TrTriangle *p_tri;

        if (!tri_is_allocated(tri))
            continue;

        if (!triangle_contained_within_rect_coords(tri, x1, y1, x2, y2))
            continue;

        p_tri = &triangulation[0].Triangles[tri];
        p_tri->solid = solid;
    }
}

void fill_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
    int tri1, tri2, tri3, tri4, ucor;
    int area_r, area_t;

    area_r = (y2 - y1) * 2 * (x2 - x1);
    area_t = 0;

    edge_find(x1, y1, x1, y2, &tri1, &ucor);
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri1];
        p_tri->solid = solid;
        area_t += triangle_area1(tri1);
    }
    if (area_t == area_r)
        return;

    edge_find(x2, y2, x2, y1, &tri2, &ucor);
    if (tri2 != tri1)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri2];
        p_tri->solid = solid;
        area_t += triangle_area1(tri2);
    }
    if (area_t == area_r)
        return;

    edge_find(x2, y1, x1, y1, &tri3, &ucor);
    if (tri3 != tri1 && tri3 != tri2)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri3];
        p_tri->solid = solid;
        area_t += triangle_area1(tri3);
    }
    if (area_t == area_r)
        return;

    edge_find(x1, y2, x2, y2, &tri4, &ucor);
    if (tri4 != tri1 && tri4 != tri2 && tri4 != tri3)
    {
        struct TrTriangle *p_tri;
        p_tri = &triangulation[0].Triangles[tri4];
        p_tri->solid = solid;
        area_t += triangle_area1(tri4);
    }
    if (area_t == area_r)
        return;

    brute_fill_rectangle(x1, y1, x2, y2, solid);
}

void tri_set_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
    int sx1, sy1, sx2, sy2;

    sx1 = x1;
    sy1 = y1;
    sx2 = x2;
    sy2 = y2;
    if (x1 > x2) {
        sx1 = x2;
        sx2 = x1;
    }
    if (y1 > y2) {
        sy1 = y2;
        sy2 = y1;
    }
    insert_point(sx1, sy1);
    insert_point(sx2, sy1);
    make_edge(sx1, sy1, sx2, sy1);
    insert_point(sx2, sy2);
    make_edge(sx2, sy1, sx2, sy2);
    insert_point(sx1, sy2);
    make_edge(sx2, sy2, sx1, sy2);
    make_edge(sx1, sy2, sx1, sy1);
    fill_rectangle(sx1, sy1, sx2, sy2, solid);
}

void triangulate_set_rectangle(int x1, int y1, int x2, int y2, ubyte solid)
{
    tri_set_rectangle(x1, y1, x2, y2, solid);
}

void triangulation_initxy(int x1, int y1, int x2, int y2)
{
    int tri, pt;
    struct TrTriangle *p_tri;
    struct TrPoint *p_point;

    triangulation[0].tri_initialised = 1;

    for (tri = 0; tri < triangulation[0].max_Triangles; tri++)
    {
        tri_clear(tri);
    }
    triangulation[0].free_Triangles = -1;

    for (pt = 0; pt < triangulation[0].max_Points; pt++)
    {
        point_clear(pt);
    }
    triangulation[0].free_Points = -1;

    p_point = &triangulation[0].Points[0];
    p_point->x = x1;
    p_point->y = y1;
    p_point = &triangulation[0].Points[1];
    p_point->x = x2;
    p_point->y = y1;
    p_point = &triangulation[0].Points[2];
    p_point->x = x2;
    p_point->y = y2;
    p_point = &triangulation[0].Points[3];
    p_point->x = x1;
    p_point->y = y2;
    triangulation[0].ix_Points = 4;
    triangulation[0].count_Points = 4;

    p_tri = &triangulation[0].Triangles[0];
    p_tri->point[0] = 3;
    p_tri->point[1] = 1;
    p_tri->point[2] = 0;
    p_tri->tri[0] = 1;
    p_tri->tri[1] = -1;
    p_tri->tri[2] = -1;
    p_tri->solid = 0;
    p_tri->enter = 7;
    p_tri = &triangulation[0].Triangles[1];
    p_tri->point[0] = 1;
    p_tri->point[1] = 3;
    p_tri->point[2] = 2;
    p_tri->tri[0] = 0;
    p_tri->tri[1] = -1;
    p_tri->tri[2] = -1;
    p_tri->solid = 0;
    p_tri->enter = 7;
    triangulation[0].ix_Triangles = 2;
    triangulation[0].count_Triangles = 2;
}

void triangulation_init(void)
{
    int dim_lo, dim_hi;
    dim_lo = -4096;
    dim_hi = 32768+4096+512;

    triangulation[0].triangle_top = triangulation[0].max_Triangles;
    triangulation[0].point_top = triangulation[0].max_Points;
    triangulation_initxy(dim_lo, dim_lo, dim_hi, dim_hi);
    triangulation[0].last_tri = -1;
}

void triangulation_init_edges(void)
{
    int dim_lo, dim_hi;
    dim_lo = -4096+256;
    dim_hi = 32768+4096+256;

    insert_point(dim_lo, dim_lo);
    insert_point(dim_hi, dim_lo);
    insert_point(dim_lo, dim_hi);
    insert_point(dim_hi, dim_hi);
    make_edge(dim_lo, dim_lo, dim_hi, dim_lo);
    make_edge(dim_hi, dim_lo, dim_hi, dim_hi);
    make_edge(dim_hi, dim_hi, dim_lo, dim_hi);
    make_edge(dim_lo, dim_hi, dim_lo, dim_lo);
}

static DIFFTEST_REF_FUNC void triangulation_allocate_asm(int tgnNo, int maxTrigs)
{
    DIFFTEST_ASM(
      "call ASM_triangulation_allocate\n"
        : : "a" (tgnNo), "d" (maxTrigs));
}

void triangulation_allocate(int tgnNo, int maxTrigs)
{
    struct Triangulation *p_trgl;
    int max_pts;

    DIFFTEST_SHADOW(triangulation_allocate, triangulation_allocate_asm(tgnNo, maxTrigs));
    if (!triangulation_initied)
        triangulation_initialize();

    p_trgl = &triangulation[tgnNo];
    max_pts = maxTrigs / 2;
    p_trgl->tri_allocated = 1;
    p_trgl->max_Triangles = maxTrigs;
    p_trgl->triangle_top = maxTrigs;
    p_trgl->max_Points = max_pts;
    p_trgl->point_top = max_pts;
    p_trgl->Triangles = LbMemoryAlloc(maxTrigs << 4);
    p_trgl->Points = LbMemoryAlloc(max_pts << 3);
    memset(p_trgl->Triangles, 0, maxTrigs << 4);
    memset(p_trgl->Points, 0, max_pts << 3);
}

void triangulate_area(ubyte *p_map, int x1, int y1, int x2, int y2)
{
    ubyte solid;

    if (!triangulation[0].tri_initialised)
    {
        triangulation[0].tri_initialised = 1;
        triangulation_init();
    }
    tri_set_rectangle(x1 << 7, y1 << 7, x2 << 7, y2 << 7, 0);
    fringe_init(p_map, x1, y1, x2, y2);

    while (fringe_get_rectangle(&x1, &y1, &x2, &y2, &solid))
    {
        if (solid == 0)
            continue;
        tri_set_rectangle(x1 << 7, y1 << 7, x2 << 7, y2 << 7, solid);
    }
}

void triangulate_map(ubyte *p_map)
{
    triangulate_area(p_map, 0, 0, 256, 256);
}

void triangulation_clear(void)
{
    triangulation_init();
    triangulation_init_edges();
}

void init_col_vects_linked_list(void)
{
    int i, limit;

    link__MapColListEmptyHead = 0;
    link__MapColVectEmptyHead = 0;

    // TODO why this amount?
    limit = 1000;//get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    for (i = 0; i < limit; i++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[next_vects_list + i];
        p_cvlist->NextColList = link__MapColListEmptyHead;
        link__MapColListEmptyHead = i + next_vects_list;
    }

    // TODO why this amount?
    limit = 600;//get_memory_ptr_allocated_count((void **)&game_col_vects);
    for (i = 0; i < limit; i++)
    {
        struct ColVect *p_colvect;

        p_colvect = &game_col_vects[next_col_vect + i];
        p_colvect->Face = link__MapColVectEmptyHead;
        link__MapColVectEmptyHead = i + next_col_vect;
    }
}

void init_collision_vects(void)
{
    ushort tile_x, tile_z;
    int i, limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    for (i = 0; i < limit; i++) {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[i];
        p_cvlist->Vect = 0;
    }
    next_col_vect = 1;
    next_vects_list = 1;
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            p_mapel->ColHead = 0;
        }
    }
}

void reset_things_col_vect_range(void)
{
    struct Thing *p_thing;
    ThingIdx thing;
    ushort vl;
    short i;
    ushort count;

    thing = get_thing_same_type_head(TT_BUILDING, -1);
    for (i = 0; thing > 0; thing = p_thing->LinkSame, i++)
    {
        if (i >= THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
        p_thing = &things[thing];
        p_thing->U.UObject.BuildStartVect = 0;
        p_thing->U.UObject.BuildNumbVect = 0;
    }

    thing = 0;
    count = 0;
    for (vl = 0; vl < next_vects_list; vl++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[vl];
        if (p_cvlist->Object == thing)
        {
            count++;
            continue;
        }
        if (thing > 0)
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            if (p_thing->Type == TT_BUILDING)
            {
                if (p_thing->SubType == SubTT_BLD_DOME) {
                    // Dome has collision vectors spread evenly around; we want
                    // to make toggleable only a little above half of them,
                    // as only half of the dome is being opened
                    count = count * 5 / 9;
                } else {
                    // Other building types have no toggleable collision vectors
                    count = 0;
                }
                p_thing->U.UObject.BuildStartVect = vl - count;
                p_thing->U.UObject.BuildNumbVect = count;
            }
        }
        thing = p_cvlist->Object;
        count = 1;
    }
    { // Setting properties in the last thing after we've reeached end of vects_list
        if (thing > 0)
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            if (p_thing->Type == TT_BUILDING) {
                p_thing->U.UObject.BuildStartVect = vl - count;
                p_thing->U.UObject.BuildNumbVect = count;
            }
        }
    }
}

/** Adds given walk face to a list of walk items of another face.
 *
 * Can only be called continously for one face, until another face gats it walk head created.
 */
TbBool face_has_walk_item(short face, short walk_face)
{
    ushort wlkhead;

    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        wlkhead = p_face->WalkHeader;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        wlkhead = p_face->WalkHeader;
    } else
    {
        return false;
    }

    return walk_face_is_in_list(wlkhead, walk_face);
}

/** Adds given walk face to a list of walk items of another face.
 *
 * Can only be called continously for one face, until another face gets its walk head created.
 */
void add_face_walk_item(short face, short walk_face)
{
    ushort wlkhead;

    if (face_has_walk_item(face, walk_face)) {
        return;
    }

    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        wlkhead = p_face->WalkHeader;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        wlkhead = p_face->WalkHeader;
    } else
    {
        LOGERR("no walk head assigned to face %d", (int)face);
        return;
    }

    add_walk_face_to_list(wlkhead, walk_face);
}

int face_to_object_position(short face, short *x, short *y, short *z)
{
    if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        struct SingleObject *p_obj;

        p_face = &game_object_faces4[-face];
        p_obj = &game_objects[p_face->Object];
        *x = p_obj->MapX;
        *y = p_obj->OffsetY;
        *z = p_obj->MapZ;
        return 4;
    }
    else if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        struct SingleObject *p_obj;

        p_face = &game_object_faces3[face];
        p_obj = &game_objects[p_face->Object];
        *x = p_obj->MapX;
        *y = p_obj->OffsetY;
        *z = p_obj->MapZ;
        return 3;
    }
    return 0;
}

int add_walk_items_for_face_object(short face, short obj)
{
    short fcobj_x, fcobj_y, fcobj_z;
    short gvobj_x, gvobj_y, gvobj_z;
    short num_points;
    short cface;
    short startface3, endface3;
    short startface4, endface4;
    short cor;

    num_points = face_to_object_position(face, &fcobj_x, &fcobj_y, &fcobj_z);
    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        gvobj_x = p_obj->MapX;
        gvobj_y = p_obj->OffsetY;
        gvobj_z = p_obj->MapZ;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }

    for (cor = 0; cor < num_points; cor++)
    {
        short fcpt_x, fcpt_y, fcpt_z;

        if (face < 0)
        {
            struct SingleObjectFace4 *p_face;
            struct SinglePoint *p_pt;

            p_face = &game_object_faces4[-face];
            p_pt = &game_object_points[p_face->PointNo[cor]];
            fcpt_x = fcobj_x + p_pt->X;
            fcpt_y = fcobj_y + p_pt->Y;
            fcpt_z = fcobj_z + p_pt->Z;
        }
        else if (face > 0)
        {
            struct SingleObjectFace3 *p_face;
            struct SinglePoint *p_pt;

            p_face = &game_object_faces3[face];
            p_pt = &game_object_points[p_face->PointNo[cor]];
            fcpt_x = fcobj_x + p_pt->X;
            fcpt_y = fcobj_y + p_pt->Y;
            fcpt_z = fcobj_z + p_pt->Z;
        }
        else
        {
            LOGERR("walk on face zero");
            fcpt_x = fcobj_x;
            fcpt_y = fcobj_y;
            fcpt_z = fcobj_z;
        }

        for (cface = startface3; cface < endface3; cface++)
        {
            struct SingleObjectFace3 *p_face;
            short ccor;
            p_face = &game_object_faces3[cface];
            if ((p_face->GFlags & FGFlg_Unkn04) == 0) {
                continue;
            }
            if (cface == face) {
                continue;
            }
            if (game_normals[p_face->FaceNormal].NY == 0) {
                continue;
            }
            for (ccor = 0; ccor < 3; ccor++)
            {
                struct SinglePoint *p_pt;
                int delta_x, delta_y, delta_z;

                p_pt = &game_object_points[p_face->PointNo[ccor]];
                delta_x = (gvobj_x + p_pt->X) - fcpt_x;
                delta_y = (gvobj_y + p_pt->Y) - fcpt_y;
                delta_z = (gvobj_z + p_pt->Z) - fcpt_z;

                if (delta_z * delta_z + delta_y * delta_y + delta_x * delta_x < 1900)
                {
                    add_face_walk_item(face, cface);
                    break;
                }
            }
        }

        for (cface = startface4; cface < endface4; cface++)
        {
            struct SingleObjectFace4 *p_face;
            short ccor;

            if (-cface == face) {
                continue;
            }
            p_face = &game_object_faces4[cface];
            if ((p_face->GFlags & FGFlg_Unkn04) == 0) {
                continue;
            }
            if (game_normals[p_face->FaceNormal].NY == 0) {
                continue;
            }
            for (ccor = 0; ccor < 4; ccor++)
            {
                struct SinglePoint *p_pt;
                int delta_x, delta_y, delta_z;

                p_pt = &game_object_points[p_face->PointNo[ccor]];
                delta_x = (gvobj_x + p_pt->X) - fcpt_x;
                delta_y = (gvobj_y + p_pt->Y) - fcpt_y;
                delta_z = (gvobj_z + p_pt->Z) - fcpt_z;

                if (delta_z * delta_z + delta_y * delta_y + delta_x * delta_x < 1900)
                {
                    add_face_walk_item(face, -cface);
                    break;
                }
            }
        }
    }
    return 0;
}

void add_walk_items_for_face_thing_objects(short face, struct Thing *p_thing)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        add_walk_items_for_face_object(face, obj);
    }
}

void add_walk_items_for_face_things_near(short x, short y, short z, short radius, short face)
{
    short shift_x, shift_z;
    short range;

    range = MAPCOORD_TO_TILE(radius + 127);

    for (shift_x = -range; shift_x <= range; shift_x++)
    {
        for (shift_z = -range; shift_z <= range; shift_z++)
        {
            int tile_x, tile_z;
            struct MyMapElement *p_mapel;
            ThingIdx thing;
            int i;

            tile_x = MAPCOORD_TO_TILE(x) + shift_x;
            tile_z = MAPCOORD_TO_TILE(z) + shift_z;
            if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH)
                continue;
            if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT)
                continue;
            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thing = p_mapel->Child;
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        add_walk_items_for_face_thing_objects(face, p_thing);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

/** Reserves WalkHeader and assigns it to given face.
 *
 * The WalkHeader is initialized to have 0 items starting at given item index.
 */
TbBool prepare_face_for_having_walk_items(short face)
{
    ushort wlkhead;

    wlkhead = create_walk_list();

    if (face > 0)
    {
        struct SingleObjectFace3 *p_face;
        p_face = &game_object_faces3[face];
        p_face->WalkHeader = wlkhead;
    }
    else if (face < 0)
    {
        struct SingleObjectFace4 *p_face;
        p_face = &game_object_faces4[-face];
        p_face->WalkHeader = wlkhead;
    } else
    {
        destroy_walk_list(wlkhead);
        return false;
    }
    return true;
}

void add_walk_items_for_face(short face)
{
    short obj_x, obj_y, obj_z;

    prepare_face_for_having_walk_items(face);
    face_to_object_position(face, &obj_x, &obj_y, &obj_z);
    add_walk_items_for_face_things_near(obj_x, obj_y, obj_z, 31 << 8, face);
}

void generate_walk_items(void)
{
    short face;

    reset_all_walk_lists();

    for (face = 1; face < next_object_face3; face++)
    {
        if ((game_object_faces3[face].GFlags & FGFlg_Unkn04) != 0)
            add_walk_items_for_face(face);
    }
    for (face = 1; face < next_object_face4; face++)
    {
        if ((game_object_faces4[face].GFlags & FGFlg_Unkn04) != 0)
            add_walk_items_for_face(-face);
    }
}

#define FACE_SWEEP_STEPS 256

/** Function to find the divisor of N closest to the target
 */
int divisor_of_a_number_not_greater_than_given_target(int N, int target)
{
    int closest = -1;
    int diff = INT_MAX;

    // Iterate till square root of N
    for (int i = 1; i <= LbSqrL(N); i++)
    {
        if (N % i == 0)
        {
            // Check if divisors are equal
            if (N / i == i) {
                // Check if i is the closest
                if (abs(target - i) < diff) {
                    diff = abs(target - i);
                    closest = i;
                }
            } else {
                // Check if i is the closest
                if (abs(target - i) < diff) {
                    diff = abs(target - i);
                    closest = i;
                }
                // Check if n / i is the closest
                if (abs(target - N / i) < diff) {
                    diff = abs(target - N / i);
                    closest = N / i;
                }
            }
        }
    }
    return closest;
}

void update_mapel_collision_columns_around_triangle(short fcobj_x, short fcobj_y, short fcobj_z,
  struct SinglePoint *p_pt0, struct SinglePoint *p_pt1, struct SinglePoint *p_pt2, ushort flags)
{
    int dist_B, dist_A;
    int incr_B, incr_A;
    int sh_A, sh_B;
    int basept_x, basept_y, basept_z;
    int delta1_x, delta1_y, delta1_z;
    int delta2_x, delta2_y, delta2_z;

    basept_x = fcobj_x + p_pt0->X;
    basept_y = fcobj_y + p_pt0->Y;
    basept_z = fcobj_z + p_pt0->Z;
    delta1_x = p_pt1->X - p_pt0->X;
    delta1_y = p_pt1->Y - p_pt0->Y;
    delta1_z = p_pt1->Z - p_pt0->Z;
    delta2_x = p_pt2->X - p_pt0->X;
    delta2_y = p_pt2->Y - p_pt0->Y;
    delta2_z = p_pt2->Z - p_pt0->Z;
    // Distance between pt0 and pt1, in 1/4 of a tile
    dist_A = LbSqrL(delta1_x * delta1_x + delta1_y * delta1_y + delta1_z * delta1_z) >> 6;
    if (dist_A < 1)
        dist_A = 1;
    incr_A = divisor_of_a_number_not_greater_than_given_target(FACE_SWEEP_STEPS, FACE_SWEEP_STEPS / dist_A);
    // Distance between pt0 and pt2, in 1/4 of a tile
    dist_B = LbSqrL(delta2_x * delta2_x + delta2_y * delta2_y + delta2_z * delta2_z) >> 6;
    if (dist_B < 1)
        dist_B = 1;
    incr_B = divisor_of_a_number_not_greater_than_given_target(FACE_SWEEP_STEPS, FACE_SWEEP_STEPS / dist_B);
    if ((incr_A <= 0) || (incr_B <= 0)) {
        LOGERR("bad increment");
        return;
    }

    for (sh_A = 0; sh_A <= FACE_SWEEP_STEPS; sh_A += incr_A)
    {
        for (sh_B = 0; sh_B <= FACE_SWEEP_STEPS; sh_B += incr_B)
        {
            struct MyMapElement *p_mapel;
            int ccx, ccy, ccz;
            short tile_x, tile_z;
            ushort qb;

            ccx = basept_x + (delta2_x * sh_B + delta1_x * sh_A) / FACE_SWEEP_STEPS;
            ccy = basept_y + (delta2_y * sh_B + delta1_y * sh_A) / FACE_SWEEP_STEPS;
            ccz = basept_z + (delta2_z * sh_B + delta1_z * sh_A) / FACE_SWEEP_STEPS;
            tile_x = MAPCOORD_TO_TILE(ccx);
            tile_z = MAPCOORD_TO_TILE(ccz);
            if (tile_x < 1 || tile_x >= MAP_TILE_WIDTH)
                continue;
            if (tile_z < 1 || tile_z >= MAP_TILE_HEIGHT)
                continue;

            p_mapel = &game_my_big_map[tile_x + tile_z * MAP_TILE_WIDTH];
            if ((p_mapel->Alt - 20 <= ccy) && flags) {
                p_mapel->Texture |= 0x8000;
            }
            qb = map_coord_to_collision_qbit_index(ccx, ccz);
            set_mapel_col_columns(p_mapel, (ccy - p_mapel->Alt) >> 7, qb);
        }
    }
}

void update_mapel_collision_columns_around_face(short face, ushort flags)
{
    struct SinglePoint *p_pt0;
    struct SinglePoint *p_pt1;
    struct SinglePoint *p_pt2;
    short fcobj_x, fcobj_y, fcobj_z;
    short num_points;
    short tri;

    num_points = face_to_object_position(face, &fcobj_x, &fcobj_y, &fcobj_z);

    // Do it once for triangular, twice for quad surface
    for (tri = 0; tri < num_points - 2; tri++)
    {
        if (face <= 0)
        {
            struct SingleObjectFace4 *p_face;

            p_face = &game_object_faces4[-face];
            p_pt0 = &game_object_points[p_face->PointNo[0]];
            p_pt1 = &game_object_points[p_face->PointNo[tri+1]];
            p_pt2 = &game_object_points[p_face->PointNo[tri+2]];
        }
        else
        {
            struct SingleObjectFace3 *p_face;

            p_face = &game_object_faces3[face];
            p_pt0 = &game_object_points[p_face->PointNo[0]];
            p_pt1 = &game_object_points[p_face->PointNo[1]];
            p_pt2 = &game_object_points[p_face->PointNo[2]];
        }

        update_mapel_collision_columns_around_triangle(fcobj_x, fcobj_y, fcobj_z,
          p_pt0, p_pt1, p_pt2, flags);
    }
}

void update_mapel_collision_columns_around_object(ushort obj, ushort flags)
{
    short startface3, endface3;
    short startface4, endface4;
    short face;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++) {
        update_mapel_collision_columns_around_face(face, flags);
    }
    for (face = startface4; face < endface4; face++) {
        update_mapel_collision_columns_around_face(-face, flags);
    }
}

void update_mapel_collision_columns_around_thing_objects(struct Thing *p_thing, ushort flags)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        update_mapel_collision_columns_around_object(obj, flags);
    }
}

void clear_mapel_collision_columns(void)
{
    ushort tile_x, tile_z;

    next_col_column = 1;
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            p_mapel->ColumnHead = 0;
        }
    }
}

void update_mapel_collision_columns(void)
{
    ushort tile_x, tile_z;

    clear_mapel_collision_columns();

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;
            int i;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thing = p_mapel->Child;
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        update_mapel_collision_columns_around_thing_objects(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

void add_next_col_vect_to_vects_list(short x, short z, ThingIdx thing, short face, ushort vect, ubyte flags)
{
    short tile_x, tile_z;
    struct MyMapElement *p_mapel;
    struct ColVectList *p_cvlist;

    tile_x = MAPCOORD_TO_TILE(x);
    tile_z = MAPCOORD_TO_TILE(z);
    if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH) {
        return;
    }
    if (tile_z < 0 || tile_z >= MAP_TILE_HEIGHT) {
        return;
    }

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
    p_cvlist = &game_col_vects_list[p_mapel->ColHead];

    if (vect != p_cvlist->Vect)
    {
        ushort next_vl;

        next_vl = next_vects_list;
        next_vects_list++;
        p_cvlist = &game_col_vects_list[next_vl];
        p_cvlist->Vect = vect;
        p_cvlist->Object = thing;
        p_cvlist->NextColList = p_mapel->ColHead;
        p_mapel->ColHead = next_vl;
        if (flags & 0x01)
        {
            if (face != 0)
                p_mapel->Texture |= 0x8000;
        }
    }
}

int new_col_vect(short x1, short y1, short z1, short x2, short y2, short z2, short face)
{
    struct ColVect *p_colvect;
    int vect;
    int limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects);
    if (next_col_vect >= limit) {
        LOGERR("Reached limit of col_vect items");
        return 0;
    }

    vect = next_col_vect;
    next_col_vect++;
    p_colvect = &game_col_vects[vect];

    p_colvect->X1 = x1;
    p_colvect->Y1 = y1;
    p_colvect->Z1 = z1;
    p_colvect->X2 = x2;
    p_colvect->Y2 = y2;
    p_colvect->Z2 = z2;
    p_colvect->Face = face;

    return vect;
}

void add_obj_face_to_col_vect(short x1, short y1, short z1, short x2, short y2, short z2, ThingIdx thing, short face, ushort flags)
{
    int vect, limit;

    limit = get_memory_ptr_allocated_count((void **)&game_col_vects_list);
    if (next_vects_list >= limit) {
        return;
    }

    //TODO why generating thin walls here? we have a separate higher level call for that
    //thin_wall(x1 >> 7, z1 >> 7, x2 >> 7, z2 >> 7, 1, 1);
    vect = new_col_vect(x1, y1, z1, x2, y2, z2, face);
    if (vect <= 0)
        return;

    int delta_x, delta_z, dist;
    int x, z, step_x, step_z;

    delta_x = x2 - x1;
    delta_z = z2 - z1;
    dist = LbSqrL(delta_x * delta_x + delta_z * delta_z);
    if (dist < 1)
        dist = 1;

    x = x1 << 10;
    z = z1 << 10;
    step_x = (delta_x << 10) / dist;
    step_z = (delta_z << 10) / dist;
    for (; dist >= 0; dist--)
    {
        add_next_col_vect_to_vects_list(x >> 10, z >> 10, thing, face, vect, flags);
        x += step_x;
        z += step_z;
    }
}

#define TOLERANCE 10

/** Adds a face to col_vect lists in nearby MapElements, if the face has two sibling points close to the ground.
 */
void add_object_face3_to_col_vect(short obj_x, short obj_y, short obj_z, ThingIdx thing, short face, ushort a2)
{
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    struct SingleObjectFace3 *p_face;
    int cor;

    // Fill arrays with face coordinates
    p_face = &game_object_faces3[face];
    for (cor = 0; cor < 3; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = (obj_y + p_pt->Y) >> 3; // TODO why divide by 8?
        z_cor[cor] = obj_z + p_pt->Z;
    }
    // Fill array with altitudes
    for (cor = 0; cor < 3; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = alt >> 8;
    }
    // Add only if coords from two sibling points are very close to ground
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], thing, face, a2);
    }
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[2], y_cor[2], z_cor[2], thing, face, a2);
    }
    if (y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[1], y_cor[1], z_cor[1],
          x_cor[2], y_cor[2], z_cor[2], thing, face, a2);
    }
}

/** Adds a face to col_vect lists in nearby MapElements, if the face has two sibling points close to the ground.
 */
void add_object_face4_to_col_vect(short obj_x, short obj_y, short obj_z, ThingIdx thing, short face, ushort a2)
{
    int alt_cor[4];
    int x_cor[4];
    int y_cor[4];
    int z_cor[4];
    struct SingleObjectFace4 *p_face;
    int cor;

    // Fill arrays with face coordinates
    p_face = &game_object_faces4[face];
    for (cor = 0; cor < 4; cor++) {
        struct SinglePoint *p_pt;
        p_pt = &game_object_points[p_face->PointNo[cor]];
        x_cor[cor] = obj_x + p_pt->X;
        y_cor[cor] = obj_y + p_pt->Y; // TODO why not divide by 8?
        z_cor[cor] = obj_z + p_pt->Z;
    }
    // Fill array with altitudes
    for (cor = 0; cor < 4; cor++) {
        int alt;
        alt = alt_at_point(x_cor[cor], z_cor[cor]);
        alt_cor[cor] = alt >> 8;
    }
    // Add only if coords from two sibling points are very close to ground
    if (y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE
      && y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[0], y_cor[0], z_cor[0],
          x_cor[1], y_cor[1], z_cor[1], thing, -face, a2);
    }
    if (y_cor[1] > alt_cor[1] - TOLERANCE && y_cor[1] < alt_cor[1] + TOLERANCE
      && y_cor[3] > alt_cor[3] - TOLERANCE && y_cor[3] < alt_cor[3] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[1], y_cor[1], z_cor[1],
          x_cor[3], y_cor[3], z_cor[3], thing, -face, a2);
    }
    if (y_cor[3] > alt_cor[3] - TOLERANCE && y_cor[3] < alt_cor[3] + TOLERANCE
      && y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[3], y_cor[3], z_cor[3],
          x_cor[2], y_cor[2], z_cor[2], thing, -face, a2);
    }
    if (y_cor[2] > alt_cor[2] - TOLERANCE && y_cor[2] < alt_cor[2] + TOLERANCE
      && y_cor[0] > alt_cor[0] - TOLERANCE && y_cor[0] < alt_cor[0] + TOLERANCE) {
        add_obj_face_to_col_vect(x_cor[2], y_cor[2], z_cor[2],
          x_cor[0], y_cor[0], z_cor[0], thing, -face, a2);
    }
}

#undef TOLERANCE

void add_all_object_faces_to_col_vect(ThingIdx thing, ushort obj, ushort a2)
{
    short face;
    short startface3, endface3;
    short startface4, endface4;
    short obj_x, obj_y, obj_z;

    {
        struct SingleObject *p_obj;

        p_obj = &game_objects[obj];
        obj_x = p_obj->MapX;
        obj_z = p_obj->MapZ;
        obj_y = p_obj->OffsetY;
        startface3 = p_obj->StartFace;
        endface3 = startface3 + p_obj->NumbFaces;
        startface4 = p_obj->StartFace4;
        endface4 = startface4 + p_obj->NumbFaces4;
    }
    for (face = startface3; face < endface3; face++)
    {
        add_object_face3_to_col_vect(obj_x, obj_y, obj_z, thing, face, a2);
    }
    for (face = startface4; face < endface4; face++)
    {
        add_object_face4_to_col_vect(obj_x, obj_y, obj_z, thing, face, a2);
    }
}

void add_all_thing_objects_faces_to_col_vect(struct Thing *p_thing, ushort flags)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        add_all_object_faces_to_col_vect(p_thing->ThingOffset, obj, flags);
    }
}

void generate_collision_vects(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        add_all_thing_objects_faces_to_col_vect(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

void thin_wall_around_thing_objects(struct Thing *p_thing, ubyte colt)
{
    ushort beg_obj, end_obj;
    ushort obj;

    beg_obj = p_thing->U.UObject.Object;
    end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
    for (obj = beg_obj; obj < end_obj; obj++) {
        thin_wall_around_object(obj, colt);
    }
}

void generate_thin_walls(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        thin_wall_around_thing_objects(p_thing, 0);
                    thing = p_thing->Next;
                }
            }
        }
    }
}

/** Divides triangles adding line segment where entrances to thin paths ought to be.
 */
void thin_paths_entrance_on_vectlist(ushort vl_head)
{
    struct ColVectList *p_cvlist;
    ushort vl;

    for (vl = vl_head; vl != 0; vl = p_cvlist->NextColList & 0x7FFF)
    {
        struct ColVect *p_colvect;
        int sx1, sx2, sy1, sy2;

        p_cvlist = &game_col_vects_list[vl];
        p_colvect = &game_col_vects[p_cvlist->Vect];

        sx1 = p_colvect->X1;
        sy1 = p_colvect->Z1;
        sx2 = p_colvect->X2;
        sy2 = p_colvect->Z2;

        insert_point(sx1, sy1);
        insert_point(sx2, sy2);
    }
}

void generate_thin_paths_entrance(void)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thin_paths_entrance_on_vectlist(p_mapel->ColHead);
        }
    }
}

void thin_paths_on_vectlist(ushort vl_head,
  TrTriangId *faces3_added, TrTriangId *faces4_added)
{
    struct ColVectList *p_cvlist;
    ushort vl;

    for (vl = vl_head; vl != 0; vl = p_cvlist->NextColList & 0x7FFF)
    {
        struct ColVect *p_colvect;
        int sx1, sx2, sy1, sy2;

        p_cvlist = &game_col_vects_list[vl];
        p_colvect = &game_col_vects[p_cvlist->Vect];

        int face = p_colvect->Face;

        if (face_is_blocking_walk(face))
            continue;

        sx1 = p_colvect->X1;
        sy1 = p_colvect->Z1;
        sx2 = p_colvect->X2;
        sy2 = p_colvect->Z2;

        make_edge(sx1, sy1, sx2, sy2);

        // The lower edge is done, now we need to make the
        // remaining point and edges

        //TODO finish implementation
    }
}

void thin_path_on_thing_objects(struct Thing *p_thing,
  TrTriangId *faces3_added, TrTriangId *faces4_added)
{
    ushort vl;
    for (vl = 0; vl < next_vects_list; vl++)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[vl];
        if (p_cvlist->Object == p_thing->ThingOffset)
        {
            //TODO finish implementation
        }
    }
}

void generate_thin_paths(void)
{
    ushort tile_x, tile_z;
    // Array for mapping each SingleObjectFace3 into corresponding TrTriangle
    TrTriangId *faces3_added;
    // Array for mapping each SingleObjectFace4 into two TrTriangles
    TrTriangId *faces4_added;

    faces3_added = LbMemoryAlloc(next_object_face3 * sizeof(TrTriangId));
    faces4_added = LbMemoryAlloc(next_object_face4 * sizeof(TrTriangId) * 2);

    // Add faces which directly touch the ground
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thin_paths_on_vectlist(p_mapel->ColHead, faces3_added, faces4_added);
        }
    }

    // Add the rest of faces for each object
    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ThingIdx thing;
            int i;

            thing = get_mapwho_thing_index(tile_x, tile_z);
            for (i = 0; thing != 0 && i < MAX_THINGS_ON_TILE; i++)
            {
                if (thing <= 0) {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                } else {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        thin_path_on_thing_objects(p_thing, faces3_added, faces4_added);
                    thing = p_thing->Next;
                }
            }
        }
    }

    LbMemoryFree(faces3_added);
    LbMemoryFree(faces4_added);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int func_5fcf4_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_func_5fcf4\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Checks whether no collision vector at the tile of point (x1,z1)
 * matches the segment (x1,z1)-(x2,z2), in either direction, within
 * a tolerance of 32. Returns 0 if a matching vector exists, 1 otherwise.
 * Parameters come from eax, edx, ebx, ecx.
 */
int func_5fcf4(s32 x1, s32 z1, s32 x2, s32 z2)
{
    s32 tile_x, tile_z;
    ushort colvl;
    int count;

    DIFFTEST_SHADOW_RET(func_5fcf4, int, func_5fcf4_asm(x1, z1, x2, z2));
    tile_x = x1 >> 8;
    tile_z = z1 >> 8;
    if ((tile_x < 0) || (tile_x >= 128) || (tile_z < 0) || (tile_z >= 128))
        DIFFTEST_RETURN(0);

    colvl = game_my_big_map[(tile_z << 7) + tile_x].ColHead;
    count = 0;
    while (colvl != 0)
    {
        struct ColVect *p_colvect;

        if (count++ >= 200)
            break;
        p_colvect = &game_col_vects[game_col_vects_list[colvl].Vect];
        if ((abs(p_colvect->X1 - x1) < 32) && (abs(p_colvect->X2 - x2) < 32)
          && (abs(p_colvect->Z2 - z2) < 32) && (abs(p_colvect->Z1 - z1) < 32))
            DIFFTEST_RETURN(0);
        if ((abs(p_colvect->X1 - x2) < 32) && (abs(p_colvect->X2 - x1) < 32)
          && (abs(p_colvect->Z2 - z1) < 32) && (abs(p_colvect->Z1 - z2) < 32))
            DIFFTEST_RETURN(0);
        colvl = game_col_vects_list[colvl].NextColList & 0x7FFF;
    }
    DIFFTEST_RETURN(1);
}


static DIFFTEST_REF_FUNC void triangulation_unkn_func_002_asm(int x1, int z1, int x2, int z2)
{
    DIFFTEST_ASM(
      "call ASM_triangulation_unkn_func_002\n"
        : : "a" (x1), "d" (z1), "b" (x2), "c" (z2));
}

/** Marks edges of non-solid triangles within the given tile rectangle
 * as enterable, when no collision vector lies along the edge and the
 * sibling triangle across the edge is not solid; the sibling also gets
 * its matching edge marked.
 */
void triangulation_unkn_func_002(int x1, int z1, int x2, int z2)
{
    s32 xmin, zmin, xmax, zmax;
    int tri;

    DIFFTEST_SHADOW(triangulation_unkn_func_002,
      triangulation_unkn_func_002_asm(x1, z1, x2, z2));
    xmin = x1 << 8;
    zmin = z1 << 8;
    xmax = (x2 + 1) << 8;
    zmax = (z2 + 1) << 8;

    for (tri = 0; tri < triangulation[0].ix_Triangles; tri++)
    {
        struct TrTriangle *p_tri;
        struct TrTriangle *p_sibl;
        struct TrPoint *p_pt[3];
        ubyte inside;
        int cor, i;

        p_tri = &triangulation[0].Triangles[tri];
        if ((p_tri->solid & 0x06) != 0)
            continue;

        inside = 0;
        for (cor = 0; cor < 3; cor++)
        {
            struct TrPoint *p_pt_cor;

            p_pt_cor = &triangulation[0].Points[(ushort)p_tri->point[cor]];
            p_pt[cor] = p_pt_cor;
            if ((p_pt_cor->x >= xmin) && (p_pt_cor->x <= xmax)
              && (p_pt_cor->y >= zmin) && (p_pt_cor->y <= zmax))
                inside |= (1 << cor);
            // Third point is only checked if any of the first two is inside
            if ((cor == 1) && (inside == 0))
                break;
        }
        if (inside == 0)
            continue;

        for (cor = 0; cor < 3; cor++)
        {
            int cor2;
            ubyte mask;

            cor2 = MOD3[cor + 1];
            mask = (1 << cor) | (1 << cor2);
            if ((inside & mask) != mask)
                continue;
            if ((p_tri->enter & (1 << cor)) != 0)
                continue;
            if (func_5fcf4(p_pt[cor]->x, p_pt[cor]->y,
              p_pt[cor2]->x, p_pt[cor2]->y) == 0)
                continue;
            p_sibl = &triangulation[0].Triangles[p_tri->tri[cor]];
            if ((p_sibl->solid & 0x06) != 0)
                continue;
            p_tri->enter |= (1 << cor);
            for (i = 0; i < 3; i++)
            {
                if (p_sibl->tri[i] == tri) {
                    p_sibl->enter |= (1 << i);
                    break;
                }
            }
        }
    }
}

int fringe_at_tile(short tile_x, short tile_z)
{
    struct MyMapElement *p_mapel;

    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH))
        return 0;
    if ((tile_z < 0) || (tile_z >= MAP_TILE_HEIGHT))
        return 0;

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];

    return (p_mapel->Flags2 & MEF2_Unkn04);
}

void fill_ground_map(ubyte *p_map)
{
    ushort tile_x, tile_z;

    for (tile_x = 0; tile_x < MAP_TILE_WIDTH; tile_x++)
    {
        for (tile_z = 0; tile_z < MAP_TILE_HEIGHT; tile_z++)
        {
            ubyte *p_solid;
            int n;

            p_solid = &p_map[2*MAP_TILE_WIDTH * 2*tile_z + 2*tile_x];
            n = fringe_at_tile(tile_x, tile_z);
            p_solid[0] = n;
            p_solid[1] = n;
            p_solid += 2*MAP_TILE_WIDTH;
            p_solid[0] = n;
            p_solid[1] = n;
        }
    }
}

void generate_ground_map(void)
{
    if (ground_map == NULL) {
        ground_map = LbMemoryAlloc(MAP_TILE_WIDTH * MAP_TILE_HEIGHT);
    }
    fill_ground_map(ground_map);
    triangulate_map(ground_map);
}

void generate_map_triangulation(void)
{
#if 0 // This is done earlier, in triangulation_clear()
    triangulation_init();
    // TODO should this be replaced by triangulation_init_edges()?
    thin_wall(0, 0, 255, 0, 1, 1);
    thin_wall(255, 0, 255, 255, 1, 1);
    thin_wall(255, 255, 0, 255, 1, 1);
    thin_wall(0, 255, 0, 0, 1, 1);
#endif
    init_collision_vects();
    generate_walk_items();
    update_mapel_collision_columns();
    generate_collision_vects();
    generate_ground_map();
    generate_thin_walls();
    // Add points where jumps to thin_paths will be. These should be
    // added here to allow best results from Delaunay optimization
    generate_thin_paths_entrance();
    switch (selected_triangulation_no)
    {
    case 1:
        triangulation_clear_enter_into_solid_gnd();
        break;
    case 2:
        triangulation_clear_enter_into_solid_air();
        break;
    }
    delaunay_step();
    generate_thin_paths();
}

/******************************************************************************/
