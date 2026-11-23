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
#include <stdlib.h>
#include "bfendian.h"
#include "bfscreen.h"
#include "poly.h"

#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "engintrns.h"

#include "bigmap.h"
#include "difftest.h"
#include "game.h"
#include "lvwalk.h"
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

    DIFFTEST_ASM(
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

/** The asm does not preserve ebx and edx.
 */
static DIFFTEST_REF_FUNC int inside_the_triangle_asm(struct SingleObjectFace3 *p_face,
  int x, int z)
{
    int ret;
    DIFFTEST_ASM("call ASM_inside_the_triangle\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "a" (p_face) : "cc", "memory");
    return ret;
}

/** Checks whether the point is inside the triangle face (precise coords).
 */
int inside_the_triangle(struct SingleObjectFace3 *p_face, int x, int z)
{
    struct SingleObject *p_sobj;
    struct SinglePoint *p_pt;
    s32 px, pz;
    s32 x0, z0, x1, z1, x2, z2;

    DIFFTEST_SHADOW_RET(inside_the_triangle, int,
      inside_the_triangle_asm(p_face, x, z));
    px = x >> 8;
    pz = z >> 8;
    p_sobj = &game_objects[p_face->Object];
    p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
    x0 = (short)p_sobj->MapX + p_pt->X;
    z0 = (short)p_sobj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
    x1 = (short)p_sobj->MapX + p_pt->X;
    z1 = (short)p_sobj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
    x2 = (short)p_sobj->MapX + p_pt->X;
    z2 = (short)p_sobj->MapZ + p_pt->Z;

    // Signed 64-bit products compared, like the imul/sbb pairs in asm
    if ((s64)(x1 - x0) * (pz - z1) < (s64)(z1 - z0) * (px - x1))
        DIFFTEST_RETURN((int)0);
    if ((s64)(x2 - x1) * (pz - z2) < (s64)(z2 - z1) * (px - x2))
        DIFFTEST_RETURN((int)0);
    if ((s64)(x0 - x2) * (pz - z0) < (s64)(z0 - z2) * (px - x0))
        DIFFTEST_RETURN((int)0);
    DIFFTEST_RETURN((int)1);
}

/** The asm does not preserve ebx, ecx and edx.
 */
static DIFFTEST_REF_FUNC int set_thing_height_on_face_tri64_asm(struct Thing *p_thing,
  int x, int z, int face)
{
    int ret;
    DIFFTEST_ASM("call ASM_set_thing_height_on_face_tri64\n"
        : "=a" (ret), "+d" (x), "+b" (z), "+c" (face) : "a" (p_thing) : "cc", "memory");
    return ret;
}

static s32 quad64_fmul(s32 a, s32 b);

/** Sets the thing height from the triangle face plane at the point.
 * Returns 0 if the point is too far or outside the triangle, 1 otherwise.
 */
int set_thing_height_on_face_tri64(struct Thing *p_thing, int x, int z, int face)
{
    struct SingleObjectFace3 *p_face;
    struct SingleObject *p_sobj;
    struct SinglePoint *p_pt;
    s32 map_x, map_z, ofs_y;
    s32 x0, y0, z0, x1, y1, z1, x2, y2, z2;
    s32 dx1, dy1, dz1, dx2, dy2, dz2;
    s32 px, pz, apx, apz, dist, num, den, a, b, y;

    DIFFTEST_SHADOW_RET(set_thing_height_on_face_tri64, int,
      set_thing_height_on_face_tri64_asm(p_thing, x, z, face));
    if ((ushort)face > next_object_face3)
        DIFFTEST_RETURN((int)0);

    p_face = &game_object_faces3[(ushort)face];
    p_sobj = &game_objects[p_face->Object];
    map_x = p_sobj->MapX;
    map_z = p_sobj->MapZ;
    ofs_y = p_sobj->OffsetY;

    p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
    x0 = map_x + p_pt->X;
    y0 = ofs_y + p_pt->Y;
    z0 = map_z + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
    x1 = map_x + p_pt->X;
    y1 = ofs_y + p_pt->Y;
    z1 = map_z + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
    x2 = map_x + p_pt->X;
    y2 = ofs_y + p_pt->Y;
    z2 = map_z + p_pt->Z;

    dx1 = (x1 - x0) << 8;
    dy1 = (y1 - y0) << 8;
    dz1 = (z1 - z0) << 8;
    dx2 = (x2 - x0) << 8;
    dy2 = (y2 - y0) << 8;
    dz2 = (z2 - z0) << 8;
    px = x - (x0 << 8);
    pz = z - (z0 << 8);

    // Approximate distance from the first point: max + min / 2
    apx = abs(px >> 8);
    apz = abs(pz >> 8);
    if (apz < apx)
        dist = apx + (apz >> 1);
    else
        dist = apz + (apx >> 1);
    if (dist > 0xa00)
        DIFFTEST_RETURN((int)0);

    num = quad64_fmul(px, dz2) - quad64_fmul(pz, dx2);
    den = quad64_fmul(dz2, dx1) - quad64_fmul(dx2, dz1);
    if (den == 0)
        den = 1;
    a = (s32)(((s64)num * 0x10000) / den);
    num = quad64_fmul(pz, dx1) - quad64_fmul(px, dz1);
    b = (s32)(((s64)num * 0x10000) / den);

    if (a < 0 || a > 0x10000 || b < 0 || b > 0x10000)
        DIFFTEST_RETURN((int)0);
    if (a + b > 0x10000)
        DIFFTEST_RETURN((int)0);

    y = (y0 << 8) + quad64_fmul(a, dy1) + quad64_fmul(b, dy2);
    p_thing->Y = y >> 3;
    DIFFTEST_RETURN((int)1);
}

static DIFFTEST_REF_FUNC ushort set_thing_height_on_face_tri_asm(struct Thing *p_thing,
  int x, int z, short face)
{
    ushort ret;
    DIFFTEST_ASM(
      "call ASM_set_thing_height_on_face_tri\n"
        : "=a" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

/** Sets the thing height on the triangle face if the point is inside it.
 * Returns the inside_the_triangle() result, or 0 if outside.
 */
ushort set_thing_height_on_face_tri(struct Thing *p_thing, int x, int z, short face)
{
    int inside;

    DIFFTEST_SHADOW_RET(set_thing_height_on_face_tri, ushort,
      set_thing_height_on_face_tri_asm(p_thing, x, z, face));
    inside = inside_the_triangle(&game_object_faces3[(ushort)face], x, z);
    if (inside == 0)
        DIFFTEST_RETURN((ushort)0);
    set_thing_height_on_face_tri64(p_thing, x, z, (ushort)face);
    DIFFTEST_RETURN((ushort)inside);
}

/** The asm does not preserve ebx and edx.
 */
static DIFFTEST_REF_FUNC int inside_the_quad_asm(struct SingleObjectFace4 *p_face,
  int x, int z)
{
    int ret;
    DIFFTEST_ASM("call ASM_inside_the_quad\n"
        : "=a" (ret), "+d" (x), "+b" (z) : "a" (p_face) : "cc", "memory");
    return ret;
}

/** Checks whether the point is inside the quad face (precise coords).
 * Returns 1 if within triangle of points 0,1,2, 2 if within triangle
 * of points 1,2,3, 0 if outside.
 */
int inside_the_quad(struct SingleObjectFace4 *p_face, int x, int z)
{
    struct SingleObject *p_sobj;
    struct SinglePoint *p_pt;
    s32 px, pz;
    s32 x0, z0, x1, z1, x2, z2, x3, z3;

    DIFFTEST_SHADOW_RET(inside_the_quad, int,
      inside_the_quad_asm(p_face, x, z));
    px = x >> 8;
    pz = z >> 8;
    p_sobj = &game_objects[p_face->Object];
    p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
    x0 = (short)p_sobj->MapX + p_pt->X;
    z0 = (short)p_sobj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
    x1 = (short)p_sobj->MapX + p_pt->X;
    z1 = (short)p_sobj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
    x2 = (short)p_sobj->MapX + p_pt->X;
    z2 = (short)p_sobj->MapZ + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[3]];
    x3 = (short)p_sobj->MapX + p_pt->X;
    z3 = (short)p_sobj->MapZ + p_pt->Z;

    // Signed 64-bit products compared, like the imul/sbb pairs in asm
    if ((s64)(x1 - x0) * (pz - z1) >= (s64)(z1 - z0) * (px - x1) &&
      (s64)(x2 - x1) * (pz - z2) >= (s64)(z2 - z1) * (px - x2) &&
      (s64)(x0 - x2) * (pz - z0) >= (s64)(z0 - z2) * (px - x0))
        DIFFTEST_RETURN((int)1);
    if ((s64)(x2 - x3) * (pz - z2) < (s64)(z2 - z3) * (px - x2))
        DIFFTEST_RETURN((int)0);
    if ((s64)(x1 - x2) * (pz - z1) < (s64)(z1 - z2) * (px - x1))
        DIFFTEST_RETURN((int)0);
    if ((s64)(x3 - x1) * (pz - z3) < (s64)(z3 - z1) * (px - x3))
        DIFFTEST_RETURN((int)0);
    DIFFTEST_RETURN((int)2);
}

/** The asm does not preserve ebx, ecx and edx.
 */
static DIFFTEST_REF_FUNC int set_thing_height_on_face_quad64_asm(struct Thing *p_thing,
  int x, int z, int face)
{
    int ret;
    DIFFTEST_ASM("call ASM_set_thing_height_on_face_quad64\n"
        : "=a" (ret), "+d" (x), "+b" (z), "+c" (face) : "a" (p_thing) : "cc", "memory");
    return ret;
}

/** Fixed point multiply, like imul followed by `mov %dx,%ax; rol $0x10,%eax`.
 */
static s32 quad64_fmul(s32 a, s32 b)
{
    return (s32)(((s64)a * b) >> 16);
}

/** Sets the thing height from the quad face plane at the point.
 * Uses the plane of the first three points of the quad. Returns 0 if
 * the point is outside, 1 if within the first triangle half, 2 otherwise.
 */
int set_thing_height_on_face_quad64(struct Thing *p_thing, int x, int z, int face)
{
    struct SingleObjectFace4 *p_face;
    struct SingleObject *p_sobj;
    struct SinglePoint *p_pt;
    s32 map_x, map_z, ofs_y;
    s32 x0, y0, z0, x1, y1, z1, x2, y2, z2;
    s32 dx1, dy1, dz1, dx2, dy2, dz2;
    s32 px, pz, num, den, a, b, sum, y;

    DIFFTEST_SHADOW_RET(set_thing_height_on_face_quad64, int,
      set_thing_height_on_face_quad64_asm(p_thing, x, z, face));
    if ((ushort)face > next_object_face4)
        DIFFTEST_RETURN((int)0);

    p_face = &game_object_faces4[(ushort)face];
    p_sobj = &game_objects[p_face->Object];
    map_x = p_sobj->MapX;
    map_z = p_sobj->MapZ;
    ofs_y = p_sobj->OffsetY;

    p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
    x0 = map_x + p_pt->X;
    y0 = ofs_y + p_pt->Y;
    z0 = map_z + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
    x1 = map_x + p_pt->X;
    y1 = ofs_y + p_pt->Y;
    z1 = map_z + p_pt->Z;
    p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
    x2 = map_x + p_pt->X;
    y2 = ofs_y + p_pt->Y;
    z2 = map_z + p_pt->Z;

    dx1 = (x1 - x0) << 8;
    dy1 = (y1 - y0) << 8;
    dz1 = (z1 - z0) << 8;
    dx2 = (x2 - x0) << 8;
    dy2 = (y2 - y0) << 8;
    dz2 = (z2 - z0) << 8;
    px = x - (x0 << 8);
    pz = z - (z0 << 8);

    den = quad64_fmul(dz2, dx1) - quad64_fmul(dx2, dz1);
    if (den == 0)
        den = 1;
    num = quad64_fmul(px, dz2) - quad64_fmul(pz, dx2);
    a = (s32)(((s64)num * 0x10000) / den);
    num = quad64_fmul(pz, dx1) - quad64_fmul(px, dz1);
    b = (s32)(((s64)num * 0x10000) / den);

    if (a < 0 || a > 0x10000 || b < 0 || b > 0x10000)
        DIFFTEST_RETURN((int)0);
    sum = a + b;
    if (sum > 0x20000)
        DIFFTEST_RETURN((int)0);

    y = (y0 << 8) + quad64_fmul(a, dy1) + quad64_fmul(b, dy2);
    p_thing->Y = y >> 3;
    if (sum > 0x10000)
        DIFFTEST_RETURN((int)2);
    DIFFTEST_RETURN((int)1);
}

static DIFFTEST_REF_FUNC ushort set_thing_height_on_face_quad_asm(struct Thing *p_thing,
  int x, int z, short face)
{
    ushort ret;
    DIFFTEST_ASM(
      "call ASM_set_thing_height_on_face_quad\n"
        : "=a" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

/** Sets the thing height on the quad face if the point is inside it.
 * Returns the inside_the_quad() result, or 0 if outside.
 */
ushort set_thing_height_on_face_quad(struct Thing *p_thing, int x, int z, short face)
{
    int inside;

    DIFFTEST_SHADOW_RET(set_thing_height_on_face_quad, ushort,
      set_thing_height_on_face_quad_asm(p_thing, x, z, face));
    inside = inside_the_quad(&game_object_faces4[(ushort)face], x, z);
    if (inside == 0)
        DIFFTEST_RETURN((ushort)0);
    set_thing_height_on_face_quad64(p_thing, x, z, (ushort)face);
    DIFFTEST_RETURN((ushort)inside);
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

static DIFFTEST_REF_FUNC short find_and_set_connected_face_asm(struct Thing *p_thing,
  int x, int z, short face)
{
    short ret;
    int iface = face;
    DIFFTEST_ASM(
      "call ASM_find_and_set_connected_face\n"
        : "=a" (ret), "+d" (x), "+b" (z), "+c" (iface) : "a" (p_thing)
        : "cc", "memory");
    return ret;
}

/** Finds a face connected to the thing's current face which contains
 * the given point, and moves the thing onto it.
 * The face parameter is ignored; the current face is read from the thing.
 */
short find_and_set_connected_face(struct Thing *p_thing, int x, int z, short face)
{
    struct WalkHeader *p_walk_head;
    struct SingleObjectFace3 *p_face3;
    struct SingleObjectFace4 *p_face4;
    struct SingleObject *p_sobj;
    short cur_face, item;
    ushort wi;
    s32 old_y, dt;
    short inside;

    DIFFTEST_SHADOW_RET(find_and_set_connected_face, short,
      find_and_set_connected_face_asm(p_thing, x, z, face));

    cur_face = p_thing->U.UPerson.OnFace;
    if (cur_face > 0)
        p_walk_head = &game_walk_headers[(short)game_object_faces3[cur_face].WalkHeader];
    else
        p_walk_head = &game_walk_headers[(short)game_object_faces4[-cur_face].WalkHeader];

    for (wi = p_walk_head->StartItem; ; wi++)
    {
        if ((s32)wi >= (s32)p_walk_head->Count + (s32)p_walk_head->StartItem)
            DIFFTEST_RETURN((short)0);
        item = game_walk_items[wi];
        if (item >= 0)
        {
            p_face3 = &game_object_faces3[item];
            if ((p_face3->GFlags & FGFlg_Unkn04) == 0)
                continue;
            if ((short)inside_the_triangle(p_face3, x, z) != 0)
                break;
        }
        else
        {
            p_face4 = &game_object_faces4[(ushort)(-item)];
            if ((p_face4->GFlags & FGFlg_Unkn04) == 0)
                continue;
            inside = inside_the_quad(p_face4, x, z);
            if (inside == 0)
                continue;
            if (inside == 2)
                p_thing->Flag |= TngF_Unkn0020;
            else
                p_thing->Flag &= ~TngF_Unkn0020;
            break;
        }
    }

    old_y = p_thing->Y;
    item = game_walk_items[wi];
    if (item > 0)
    {
        p_face3 = &game_object_faces3[(ushort)item];
        if (inside_the_triangle(p_face3, x, z) != 0)
            set_thing_height_on_face_tri64(p_thing, x, z, (ushort)item);
    }
    else if (item < 0)
    {
        s32 n;

        n = -item;
        p_face4 = &game_object_faces4[n];
        p_sobj = &game_objects[p_face4->Object];
        n -= p_sobj->StartFace4;
        if (n == 0x3e || n == 9 || n == 0x3f)
        {
            if (p_sobj->NumbFaces == 0x15 && p_sobj->NumbFaces4 == 0x40 &&
              (s32)p_sobj->EndPoint - (s32)p_sobj->StartPoint == 0x53)
                p_thing->Flag2 &= ~TgF2_InsideBuilding;
        }
        p_face4 = &game_object_faces4[(ushort)(-game_walk_items[wi])];
        if (inside_the_quad(p_face4, x, z) != 0)
            set_thing_height_on_face_quad64(p_thing, x, z,
              (ushort)(-game_walk_items[wi]));
    }
    else
    {
        DIFFTEST_RETURN(game_walk_items[wi]);
    }

    dt = old_y - p_thing->Y;
    if (dt < 0)
        dt = -dt;
    if (dt > 4000)
    {
        p_thing->Y = old_y;
        DIFFTEST_RETURN((short)0);
    }
    p_thing->U.UPerson.OnFace = game_walk_items[wi];
    DIFFTEST_RETURN(game_walk_items[wi]);
}

void check_mouse_over_face(struct PolyPoint *p_pt0, struct PolyPoint *p_pt1,
  struct PolyPoint *p_pt2, int face, int type)
{
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
