/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_onface.c
 *     Support for checking if thing is on 3D face and putting it on a face.
 * @par Purpose:
 *     Check thing position to determine whether it is placed on a 3D face.
 *     Adjust thing properties to put it on the 3D face (triangle or quad).
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Aug 2025 - 17 Jan 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_onface.h"

#include <assert.h>
#include "bfendian.h"
#include "bfscreen.h"
#include "poly.h"

#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "engintrns.h"

#include "bigmap.h"
#include "difftest.h"
#include "game.h"
#include "packet.h"
#include "player.h"
#include "swlog.h"
#include "thing.h"
/******************************************************************************/

struct Thing thing_on_face;

/******************************************************************************/

static DIFFTEST_REF_FUNC ubyte check_big_point_triangle_asm(int x, int y,
  int ux, int uy, int vx, int vy, int wx, int wy)
{
    ubyte ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[4];

    stkargs[0] = (int)(intptr_t)vx;
    stkargs[1] = (int)(intptr_t)vy;
    stkargs[2] = (int)(intptr_t)wx;
    stkargs[3] = (int)(intptr_t)wy;

    asm volatile (
      "push 12(%5)\n"
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_check_big_point_triangle\n"
        : "=a" (ret)
        : "a" (x), "d" (y), "b" (ux), "c" (uy), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

ubyte check_big_point_triangle(int x, int y, int ux, int uy, int vx, int vy, int wx, int wy)
{
    s32 dwx, dwy, dvx, dvy;
    s32 num, den, a, b;

    DIFFTEST_SHADOW_RET(check_big_point_triangle, ubyte,
      check_big_point_triangle_asm(x, y, ux, uy, vx, vy, wx, wy));
    dwx = wx - ux;
    dwy = wy - uy;
    dvx = vx - ux;
    dvy = vy - uy;
    num = (y - uy) * dwx + (ux - x) * dwy;
    den = dvy * dwx - dvx * dwy;
    if (den == 0)
        DIFFTEST_RETURN((ubyte)0);
    a = (num << 8) / den;
    if (a < 0)
        DIFFTEST_RETURN((ubyte)0);
    if (dwx == 0)
        b = ((y << 8) - (uy << 8) - dvy * a) / dwy;
    else
        b = ((x << 8) - (ux << 8) - dvx * a) / dwx;
    if (b < 0)
        DIFFTEST_RETURN((ubyte)0);
    if (a + b >= 0x100)
        DIFFTEST_RETURN((ubyte)0);
    DIFFTEST_RETURN((ubyte)1);
}

/** Checks whether the point is inside the triangle face (precise coords).
 * The asm does not preserve ebx and edx.
 */
int inside_the_triangle(struct SingleObjectFace3 *p_face, int x, int z)
{
    int ret;
    asm volatile ("call ASM_inside_the_triangle\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "a" (p_face) : "cc", "memory");
    return ret;
}

/** Sets the thing height from the triangle face plane at the point.
 * The asm does not preserve ebx, ecx and edx.
 */
int set_thing_height_on_face_tri64(struct Thing *p_thing, int x, int z, int face)
{
    int ret;
    asm volatile ("call ASM_set_thing_height_on_face_tri64\n"
        : "=a" (ret), "+d" (x), "+b" (z), "+c" (face) : "a" (p_thing) : "cc", "memory");
    return ret;
}

ushort set_thing_height_on_face_tri(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_tri\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

/** Checks whether the point is inside the quad face (precise coords).
 * The asm does not preserve ebx and edx.
 */
int inside_the_quad(struct SingleObjectFace4 *p_face, int x, int z)
{
    int ret;
    asm volatile ("call ASM_inside_the_quad\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "a" (p_face) : "cc", "memory");
    return ret;
}

/** Sets the thing height from the quad face plane at the point.
 * The asm does not preserve ebx, ecx and edx.
 */
int set_thing_height_on_face_quad64(struct Thing *p_thing, int x, int z, int face)
{
    int ret;
    asm volatile ("call ASM_set_thing_height_on_face_quad64\n"
        : "=a" (ret), "+d" (x), "+b" (z), "+c" (face) : "a" (p_thing) : "cc", "memory");
    return ret;
}

ushort set_thing_height_on_face_quad(struct Thing *p_thing, int x, int z, short face)
{
    ushort ret;
    asm volatile (
      "call ASM_set_thing_height_on_face_quad\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

ushort set_thing_height_on_face(struct Thing *p_thing, int x, int z, short face)
{
    if (face > 0) {
        return set_thing_height_on_face_tri(p_thing, x, z, face);
    } else if (face < 0) {
        return set_thing_height_on_face_quad(p_thing, x, z, -face);
    }
    return 0;
}

short find_and_set_connected_face(struct Thing *p_thing, int x, int z, short face)
{
    short ret;
    asm volatile (
      "call ASM_find_and_set_connected_face\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

void check_mouse_over_face(struct PolyPoint *p_pt0, struct PolyPoint *p_pt1,
  struct PolyPoint *p_pt2, int face, int type)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_check_mouse_over_face\n"
        : : "a" (p_pt0), "d" (p_pt1), "b" (p_pt2), "c" (face), "g" (type));
    return;
#endif
    struct SingleObjectFace3 *p_face3;
    struct SingleObjectFace4 *p_face4;
    struct SingleObject *p_sobj;
    struct SinglePoint *p_point0;
    struct SinglePoint *p_point1;
    struct SinglePoint *p_point2;
    int ms_x, ms_y;

    if (pktrec_mode == PktR_PLAYBACK) {
        return;
    }

    ms_x = lbDisplay.MMouseX;
    ms_y = lbDisplay.MMouseY;

    if (!check_big_point_triangle(ms_x, ms_y, p_pt0->X, p_pt0->Y,
        p_pt1->X, p_pt1->Y, p_pt2->X, p_pt2->Y)) {
        return;
    }

    switch (type)
    {
    case 1:
        players[local_player_no].Target = face;
        p_face3 = &game_object_faces3[face];
        p_sobj = &game_objects[p_face3->Object];
        p_point2 = &game_object_points[p_face3->PointNo[0]];
        p_point0 = &game_object_points[p_face3->PointNo[2]];
        p_point1 = &game_object_points[p_face3->PointNo[1]];
        break;
    case 2:
        players[local_player_no].Target = -face;
        p_face4 = &game_object_faces4[face];
        p_sobj = &game_objects[p_face4->Object];
        p_point2 = &game_object_points[p_face4->PointNo[0]];
        p_point0 = &game_object_points[p_face4->PointNo[2]];
        p_point1 = &game_object_points[p_face4->PointNo[1]];
        break;
    case 3:
        players[local_player_no].Target = -face;
        p_face4 = &game_object_faces4[face];
        p_sobj = &game_objects[p_face4->Object];
        p_point2 = &game_object_points[p_face4->PointNo[3]];
        p_point0 = &game_object_points[p_face4->PointNo[1]];
        p_point1 = &game_object_points[p_face4->PointNo[2]];
        break;
    default:
        LOGERR("Invalid type=%d", (int)type);
        return;
    }

    players[local_player_no].TargetType = TrgTp_Unkn3;

    int ms_cor_x, ms_cor_z;
    ms_cor_x = p_sobj->MapX;
    ms_cor_z = p_sobj->MapZ;

    if (get_mapcoord_on_face_points(&ms_cor_x, &ms_cor_z, p_pt0, p_pt1, p_pt2,
          p_point0, p_point1, p_point2, ms_x, ms_y))
    {
        int prc_x, prc_z;

        prc_x = MAPCOORD_TO_PRCCOORD(ms_cor_x, 0);
        prc_z = MAPCOORD_TO_PRCCOORD(ms_cor_z, 0);
        switch (type)
        {
        case 1:
            set_thing_height_on_face_tri(&thing_on_face, prc_x, prc_z, face);
            break;
        case 2:
        case 3:
            set_thing_height_on_face_quad(&thing_on_face, prc_x, prc_z, face);
            break;
        }
        mouse_map_x = ms_cor_x;
        mouse_map_z = ms_cor_z;
    }
}

/******************************************************************************/
