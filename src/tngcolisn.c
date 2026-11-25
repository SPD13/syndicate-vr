/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file tngcolisn.c
 *     Thing collisions support.
 * @par Purpose:
 *     Defines structures and routibes for detecting and handling collisions.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     04 Sep 2024 - 11 Nov 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "tngcolisn.h"

#include <stdio.h>
#include "bfline.h"

#include "engincolour.h"
#include "enginzoom.h"
#include "huddrwlstm.h"

#include "bigmap.h"
#include "bfmath.h"
#include "drawtext_wrp.h"
#include "thing.h"
/******************************************************************************/

extern short link__MapColListEmptyHead;
extern short link__MapColVectEmptyHead;

struct ColVect *game_col_vects = NULL;
ushort next_col_vect = 1;

struct ColVectList *game_col_vects_list = NULL;
ushort next_vects_list = 1;

ubyte debug_hud_collision = 0;

/******************************************************************************/

void set_dome_col(struct Thing *p_building, ubyte flag)
{
    struct ColVectList *p_cvlist;
    ushort vl_beg, vl_end;
    ushort  vl;

    // The thing data contains properties with range of ColVectList
    // which can switch the passability
    vl_beg = p_building->U.UObject.BuildStartVect;
    vl_end = vl_beg + p_building->U.UObject.BuildNumbVect;
    if (flag)
    {
        for (vl = vl_beg; vl < vl_end; vl++)
        {
            p_cvlist = &game_col_vects_list[vl];
            p_cvlist->NextColList |= 0x8000;
        }
    }
    else
    {
        for (vl = vl_beg; vl < vl_end; vl++)
        {
            p_cvlist = &game_col_vects_list[vl];
            p_cvlist->NextColList &= ~0x8000;
        }
    }
}

/** Links given collision vector into the ColVectList chain of the map
 * element located at given precise coordinates.
 *
 * Does nothing if the map element already has that vector linked, or if
 * there is no free ColVectList entry to use.
 */
static void dynamic_add_collision_to_single_mapwho(short x, short z,
  short owner, ushort vect_idx)
{
    struct MyMapElement *p_mapel;
    struct ColVectList *p_cvlist;
    ushort new_idx;

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * MAPCOORD_TO_TILE(z)
      + MAPCOORD_TO_TILE(x)];

    if (game_col_vects_list[p_mapel->ColHead].Vect == vect_idx)
        return;

    new_idx = (ushort)link__MapColListEmptyHead;
    if (new_idx == 0)
        return;

    p_cvlist = &game_col_vects_list[new_idx];
    link__MapColListEmptyHead = p_cvlist->NextColList;
    p_cvlist->Vect = vect_idx;
    p_cvlist->NextColList = p_mapel->ColHead;
    p_cvlist->Object = owner;
    p_mapel->ColHead = new_idx;
}

ushort dynamic_insert_vect(s32 x1, s32 y1, s32 z1, s32 x2,
  s32 y2, s32 z2, short owner, ubyte vtype)
{
    struct ColVect *p_colvect;
    ushort vect_idx;
    int dx, dz;
    s32 length;
    s32 step_x, step_z;
    s32 swp_x, swp_z;
    int step, sweep_steps;

    vect_idx = link__MapColVectEmptyHead;
    if (vect_idx == 0)
        return 0;
    p_colvect = &game_col_vects[vect_idx];
    // The freelist link is kept in the Face field while a ColVect is free;
    // it gets overwritten with the owner once the vector becomes used.
    link__MapColVectEmptyHead = p_colvect->Face;

    if ((x1 < 0) || (x1 > MAP_COORD_WIDTH))
        return 0;
    if ((x2 < 0) || (x2 > MAP_COORD_WIDTH))
        return 0;
    if ((z1 < 0) || (z1 > MAP_COORD_HEIGHT))
        return 0;
    if ((z2 < 0) || (z2 > MAP_COORD_HEIGHT))
        return 0;

    p_colvect->X1 = x1;
    p_colvect->Y1 = y1;
    p_colvect->Z1 = z1;
    p_colvect->X2 = x2;
    p_colvect->Y2 = y2;
    p_colvect->Z2 = z2;
    p_colvect->Face = owner;

    dx = x2 - x1;
    dz = z2 - z1;
    length = LbSqrL(dx * dx + dz * dz);

    sweep_steps = length >> 5;
    if (sweep_steps == 0)
        sweep_steps = 1;

    step_x = (dx << 10) / sweep_steps;
    step_z = (dz << 10) / sweep_steps;

    swp_x = x1 << 10;
    swp_z = z1 << 10;

    // The segment is sampled at `sweep_steps`+1 points (both endpoints included).
    for (step = sweep_steps; step >= 0; step--)
    {
        dynamic_add_collision_to_single_mapwho(swp_x >> 10, swp_z >> 10,
          owner, vect_idx);

        swp_x += step_x;
        swp_z += step_z;
    }

    return vect_idx;
}

/******************************************************************************/
