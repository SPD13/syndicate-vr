/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file bmbang.c
 *     On-map explosion effect implementation.
 * @par Purpose:
 *     Implement functions displaying an explosion within the game world.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Sep 2023 - 17 Mar 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bmbang.h"

#include <stdio.h>
#include <stdlib.h>

#include "enginshrapn.h"

#include "bfmath.h"
#include "bfutility.h"
#include "bigmap.h"
#include "building.h"
#include "cybmod.h"
#include "difftest.h"
#include "febrief.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_options.h"
#include "misstat.h"
#include "people.h"
#include "sound.h"
#include "thing.h"
#include "tngcolisn.h"
#include "vehicle.h"
#include "swlog.h"
/******************************************************************************/

void bang_init(void)
{
#if 0
    asm volatile ("call ASM_bang_init\n"
        :  :  : "eax" );
#endif
    shrapnel_init();
    phwoar_init();
}

/** Fill in the type-specific fields of a freshly allocated Phwoar entry.
 */
void fillin_phwoar_type_info(struct Phwoar *p_phwoar)
{
    asm volatile ("call ASM_fillin_phwoar_type_info\n"
        : : "a" (p_phwoar) : "cc", "memory");
}

void new_bang(int x, int y, int z, int type, int owner, int c)
{
#if 0
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)owner;
    stkargs[1] = (int)(intptr_t)c;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_new_bang\n"
        : : "a" (x), "d" (y), "b" (z), "c" (type), "S" (stkargs)
        : "cc", "memory");
    return;
#endif
    short thing;
    struct SimpleThing *p_sthing;
    int level; // "phwoar type"/level parameter, stored into the phwoar entry
    int ang0, ang1, ang2; // quadrant angle-spread sizes (also the shrapnel counts)
    int fixed_dir; // if set, shrapnel fly in one fixed (not random) direction
    int fixed_vy; // the fixed vy value used when fixed_dir is set
    int state_override; // -1 if State keeps its default (0) value
    int total, i;
    ThingIdx tng;

    if (x < 0)
        return;
    if ((x >> 8) >= MAP_COORD_WIDTH)
        return;
    if (z < 0)
        return;
    if ((z >> 8) >= MAP_COORD_HEIGHT)
        return;
    if (phwoar_free == 0)
        return;
    c++;
    if (shrapnel_free == 0)
        return;

    // Light-only types
    switch (type)
    {
    case 0xa:
        new_bang(x, y, z, 0xb, owner, c);
        new_bang(x, y, z, 0xc, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x80, 0x19, 0x80);
        return;
    case 0xf:
        new_bang(x, y, z, 0x10, owner, c);
        new_bang(x, y, z, 0x11, owner, c);
        new_bang(x, y, z, 0x12, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x50, 0x14, 0x50);
        return;
    case 0x14:
        new_bang(x, y, z, 0x15, owner, c);
        new_bang(x, y, z, 0x16, owner, c);
        create_temp_light(x, y + 0x6400, z, 0x32, 0x5, 0x32);
        return;
    case 0x19:
        new_bang(x, y, z, 0x1a, owner, c);
        new_bang(x, y, z, 0x1b, owner, c);
        new_bang(x, y, z, 0x1c, owner, c);
        create_temp_light(x, y + 0xc800, z, 0x100, 0x5, 0xa);
        return;
    case 0x2d:
        new_bang(x, y, z, 0x2e, owner, c);
        new_bang(x, y, z, 0x2f, owner, c);
        new_bang(x, y, z, 0x30, owner, c);
        create_temp_light(x, y + 0x9600, z, 0x80, 0x19, 0x80);
        return;
    default:
        break;
    }

    tng = get_new_sthing();
    if (tng == 0)
        return;

    thing = tng;
    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_BANG;
    p_sthing->State = 0;
    p_sthing->X = x;
    p_sthing->Y = y;
    p_sthing->Z = z;
    p_sthing->Owner2 = owner;
    p_sthing->U.UBang.shrapnel = 0;
    p_sthing->U.UBang.phwoar = 0;

    ang0 = 0;
    ang1 = 0;
    ang2 = 0;
    fixed_dir = 0;
    fixed_vy = 0;
    state_override = -1;

    switch (type)
    {
    case 0xb:
        ang0 = (LbRandomAnyShort() & 0x3) + 6;
        ang1 = (LbRandomAnyShort() & 0x3) + 6;
        ang2 = (LbRandomAnyShort() & 0x3) + 6;
        level = 0x35;
        break;
    case 0xc:
        level = 0x33;
        break;
    case 0x10:
        ang0 = (LbRandomAnyShort() & 0x3) + 4;
        ang1 = (LbRandomAnyShort() & 0x3) + 4;
        ang2 = (LbRandomAnyShort() & 0x3) + 4;
        level = 0x40;
        break;
    case 0x11:
    case 0x16:
        level = 0x3d;
        break;
    case 0x12:
        level = 0x5a;
        break;
    case 0x15:
        ang1 = (LbRandomAnyShort() & 0x7) + 4;
        level = 0x3f;
        break;
    case 0x1e:
        level = 0x64;
        break;
    case 0x1b:
        level = 0x56;
        break;
    case 0x1c:
        level = 0x58;
        break;
    case 0x1a:
        level = 0x54;
        break;
    case 0x23:
        level = 0x65;
        break;
    case 0x28:
        level = 0x52;
        break;
    case 0x2e:
        level = 0x63;
        break;
    case 0x2f:
        level = 0x5f;
        break;
    case 0x30:
        level = 0x61;
        break;
    case 0x32:
        state_override = 0x64;
        level = 0x68;
        break;
    case 0x35:
        level = 0x68;
        break;
    case 0x37:
        state_override = 0xf;
        level = 0x6a;
        break;
    case 0x3c:
        level = 0x6f;
        break;
    case 0x41:
        level = 0x72;
        break;
    case 0x4b:
        level = 0x96;
        break;
    case 0x50:
        level = 0x8c;
        break;
    case 0x55:
        level = 0x82;
        break;
    case 0x5a:
        level = 0x78;
        break;
    case 0x5f:
        level = 0xbe;
        break;
    case 0x64:
        level = 0xb4;
        break;
    case 0x69:
        level = 0xaa;
        break;
    case 0x6e:
        level = 0xa0;
        break;
    case 0x73:
        create_temp_light(x, y + 0x6400, z, 0x32, 0xf, 0x32);
        level = 0x96;
        fixed_dir = 1;
        fixed_vy = 0x7f;
        break;
    case 0x78:
        create_temp_light(x, y + 0x6400, z, 0x32, 0xa, 0x32);
        level = 0x96;
        fixed_dir = 1;
        fixed_vy = 0x40;
        break;
    default:
        LOGERR("bang at (0x%x,0x%x,0x%x) owner=%d type=%d c=%d, bad type",
            x, y, z, owner, type, c);
        return;
    }

    if (state_override >= 0)
        p_sthing->State = state_override;

    total = ang0 + ang1 + ang2 + (fixed_dir ? 1 : 0);
    for (i = 0; i < total; i++) {
        short shr_idx;
        struct Shrapnel *p_shrap;
        ubyte quadrant;

        shr_idx = shrapnel_free;
        if (shr_idx == 0)
            break;
        shrapnel_free = shrapnel[shr_idx].child;

        if (i < ang0)
            quadrant = 1;
        else if (i < ang0 + ang1)
            quadrant = 3;
        else if (i < ang0 + ang1 + ang2)
            quadrant = 2;
        else
            quadrant = 4;

        p_shrap = &shrapnel[shr_idx];
        p_shrap->type = quadrant;
        p_shrap->x = x;
        p_shrap->y = y;
        p_shrap->z = z;
        if (fixed_dir) {
            p_shrap->vx = 0;
            p_shrap->vz = 0;
            p_shrap->vy = fixed_vy;
        } else {
            int angle1, angle2;
            int vx, vy, vz;

            angle1 = LbRandomAnyShort() & 0x7ff;
            angle2 = LbRandomAnyShort() & 0x1ff;

            vx = (lbSinTable[angle1] >> 10) * lbSinTable[angle2 + LbFPMath_PI/2];
            vx >>= 16;
            vz = (lbSinTable[angle1 + LbFPMath_PI/2] >> 10) * lbSinTable[angle2 + LbFPMath_PI/2];
            vz >>= 16;
            vy = lbSinTable[angle2] >> 10;

            p_shrap->vx = (sbyte)vx;
            p_shrap->vz = (sbyte)vz;
            p_shrap->vy = (sbyte)vy;
        }
        p_shrap->vyaw = LbRandomAnyShort() & 0xf;
        p_shrap->vpitch = LbRandomAnyShort() & 0x1f;

        p_shrap->child = p_sthing->U.UBang.shrapnel;
        p_sthing->U.UBang.shrapnel = shr_idx;
    }

    if (level != 0) {
        short ph_idx;

        ph_idx = phwoar_free;
        if (ph_idx != 0) {
            phwoar_free = phwoar[ph_idx].child;
            phwoar_num_used++;
            phwoar_num_free--;
        }
        if (ph_idx != 0) {
            struct Phwoar *p_phwoar;

            p_sthing->U.UBang.phwoar = ph_idx;
            p_phwoar = &phwoar[ph_idx];
            p_phwoar->x = x;
            p_phwoar->y = y;
            p_phwoar->z = z;
            p_phwoar->vx = 0;
            p_phwoar->vz = 0;
            p_phwoar->vy = 0;
            p_phwoar->type = level;
            p_phwoar->child = 0;
            p_phwoar->fc = 0;
            fillin_phwoar_type_info(p_phwoar);
        }
    }

    add_node_sthing(thing);
}

void bang_new5(int x, int y, int z, int type, int owner)
{
    new_bang(x, y, z, type, owner, 0);
}

void bang_new4(int x, int y, int z, int type)
{
    new_bang(x, y, z, type, 0, 0);
}
/* Crater walker list: 10-byte entries starting at map_cratunkn2
 * (X, Y, DX, DY as shorts, then Next and Prev bytes).
 */
extern short map_cratunkn2[];
extern ubyte map_crater_last;
extern short data_1c476c;

static DIFFTEST_REF_FUNC void unused_func_192_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_unused_func_192\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
}

/** Routed asm (internal label). Arguments are the raw registers the
 * asm takes (eax, edx, ebx, ecx); only eax (X) and edx (Y) are used.
 *
 * Adds a crater walker at index map_crater_last, at the given position,
 * moving diagonally with a step of data_1c476c / 10 (at least 1) in
 * random directions, then advances map_crater_last.
 */
void unused_func_192(int a1, int a2, int a3, int a4)
{
    s32 idx;
    ubyte step;

    DIFFTEST_SHADOW(unused_func_192, unused_func_192_asm(a1, a2, a3, a4));
    step = (ubyte)(data_1c476c / 10);
    if (step == 0)
        step = 1;
    idx = (sbyte)map_crater_last * 5;
    map_cratunkn2[idx + 0] = (short)a1;
    map_cratunkn2[idx + 1] = (short)a2;
    map_cratunkn2[idx + 2] = step;
    map_cratunkn2[idx + 3] = step;
    if (LbRandomAnyShort() % 9 > 4) {
        idx = (sbyte)map_crater_last * 5;
        map_cratunkn2[idx + 2] = -(short)step;
    }
    if (LbRandomAnyShort() % 9 > 4) {
        idx = (sbyte)map_crater_last * 5;
        map_cratunkn2[idx + 3] = -(short)step;
    }
    map_crater_last++;
    idx = (sbyte)map_crater_last * 10;
    ((ubyte *)map_cratunkn2)[idx + 9] = 0;
    ((ubyte *)map_cratunkn2)[idx + 8] = 0;
}


ubyte unused_func_026(ubyte a1)
{
    ubyte ret;
    asm volatile (
      "call ASM_unused_func_026\n"
        : "=r" (ret) : "a" (a1));
    return ret;
}

void unused_func_025(short a1, short a2, short a3)
{
    asm volatile (
      "call ASM_unused_func_025\n"
        : : "a" (a1), "d" (a2), "b" (a3));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
extern short link__MapColListEmptyHead;

static DIFFTEST_REF_FUNC int del_thing_vectors_from_mapwho_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_del_thing_vectors_from_mapwho\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Remove the first ColVectList entry owned by given thing from the
 * collision list of a map element, returning it to the empty list.
 *
 * @param a1 Pointer to the MyMapElement.
 * @param a2 Index of the thing owning the vectors (low 16 bits used).
 * @return 1 if an entry was removed, 0 otherwise.
 */
int del_thing_vectors_from_mapwho(int a1, int a2, int a3, int a4)
{
    struct MyMapElement *p_mapel = (struct MyMapElement *)a1;
    struct ColVectList *p_list;
    short thing;
    ushort cvlist, prev_cvlist, next;

    DIFFTEST_SHADOW_RET(del_thing_vectors_from_mapwho, int,
      del_thing_vectors_from_mapwho_asm(a1, a2, a3, a4));

    p_list = game_col_vects_list;
    thing = (short)a2;
    prev_cvlist = 0;
    cvlist = p_mapel->ColHead;
    while (cvlist != 0)
    {
        next = p_list[cvlist].NextColList;
        if (p_list[cvlist].Object == thing)
        {
            if (prev_cvlist != 0)
                p_list[prev_cvlist].NextColList = next;
            else
                p_mapel->ColHead = next & 0x7FFF;
            p_list[cvlist].NextColList = link__MapColListEmptyHead;
            link__MapColListEmptyHead = cvlist;
            game_col_vects_list = p_list;
            DIFFTEST_RETURN((int)1);
        }
        prev_cvlist = cvlist;
        cvlist = next & 0x7FFF;
    }
    game_col_vects_list = p_list;
    DIFFTEST_RETURN((int)0);
}


static DIFFTEST_REF_FUNC void do_shockwave_asm(int x, int y, int z,
  int radius, int intensity, struct Thing *p_owner)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)intensity;
    stkargs[1] = (int)(intptr_t)p_owner;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_do_shockwave\n"
        : : "a" (x), "d" (y), "b" (z), "c" (radius), "S" (stkargs)
        : "cc", "memory");
}

/** Approximate distance from deltas: the larger one plus half the smaller.
 */
static int shockwave_dist(int dx, int dz)
{
    int adx, adz;

    adx = abs(dx);
    adz = abs(dz);
    if (adx > adz)
        adz >>= 1;
    else
        adx >>= 1;
    return adx + adz;
}

void do_shockwave(int x, int y, int z, int radius_arg, int intensity, struct Thing *p_owner)
{
    char locstr[100];
    ubyte negative;
    int radius;
    int range, tile_cx, tile_cz;
    int tile_x, tile_z;

    DIFFTEST_SHADOW(do_shockwave,
      do_shockwave_asm(x, y, z, radius_arg, intensity, p_owner));

    // Work on a copy; the asm keeps the argument slot unchanged
    radius = radius_arg;
    negative = 0;
    if (radius < 0)
    {
        negative = 1;
        radius = -radius;
    }
    range = radius >> 8;
    if (range < 1)
        range = 1;
    tile_cz = z >> 8;
    tile_cx = x >> 8;

    for (tile_x = tile_cx - range; tile_x <= tile_cx + range; tile_x++)
    {
        for (tile_z = tile_cz - range; tile_z <= tile_cz + range; tile_z++)
        {
            struct MyMapElement *p_mapel;
            short thing;

            if ((tile_x <= 0) || (tile_x >= 128))
                continue;
            if ((tile_z <= 0) || (tile_z >= 128))
                continue;

            p_mapel = &game_my_big_map[(tile_z << 7) + tile_x];

            if (!negative)
            {
                int obj;

                obj = game_col_vects_list[p_mapel->ColHead].Object;
                if (obj > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[obj];
                    if (p_thing->Type == TT_BUILDING)
                    {
                        do_shockwave_building(0x200, intensity, p_thing, p_owner);
                    }
                    else if (obj < 1000)
                    {
                        del_thing_vectors_from_mapwho((int)(intptr_t)p_mapel,
                          (short)obj, 0, 0);
                    }
                    else
                    {
                        sprintf(locstr, "Strange thing owns col-vect type %d state %d subtype %d",
                          p_thing->Type, p_thing->State, p_thing->SubType);
                    }
                }
            }

            thing = p_mapel->Child;
            while (thing != 0)
            {
                int dx, dy, dz, dist;

                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if (!negative)
                    {
                        if ((p_thing->Type == TT_PERSON) &&
                          (p_thing->State != PerSt_PERSON_BURNING) &&
                          ((p_thing->Flag & TngF_Destroyed) == 0))
                        {
                            dy = (p_thing->Y >> 8) - y;
                            if ((dy < 0x55) && (dy > -0x55))
                            {
                                dx = (p_thing->X >> 8) - x;
                                dz = (p_thing->Z >> 8) - z;
                                dist = shockwave_dist(dx, dz);
                                if (dist < (ushort)p_thing->Radius + radius)
                                    do_shockwave_person(dx, dz, dist, intensity,
                                      p_thing, p_owner);
                            }
                        }
                        if (((p_thing->Type == TT_VEHICLE) ||
                          ((p_thing->Type == TT_BUILDING) && (p_thing->SubType == SubTT_BLD_MGUN))) &&
                          (p_thing->State != PerSt_DEAD))
                        {
                            dy = (p_thing->Y >> 8) - y;
                            if ((dy < 0xB9) && (dy > -0xB9))
                            {
                                dx = (p_thing->X >> 8) - x;
                                dz = (p_thing->Z >> 8) - z;
                                dist = shockwave_dist(dx, dz);
                                if (dist < (ushort)p_thing->Radius + radius)
                                    do_shockwave_vehicle(dx, dz, dist, intensity,
                                      p_thing, p_owner);
                            }
                        }
                        if ((p_thing->Type == TT_MINE) && (p_thing->SubType == 0x30) &&
                          ((p_thing->Flag & TngF_Destroyed) == 0) &&
                          (p_thing->State != PerSt_DEAD))
                        {
                            dx = (p_thing->X >> 8) - x;
                            dz = (p_thing->Z >> 8) - z;
                            dist = shockwave_dist(dx, dz);
                            if ((radius - dist) * intensity / radius > 0)
                            {
                                p_thing->Health -= (short)intensity;
                                if (p_thing->Health <= 0)
                                {
                                    play_dist_sample(p_thing, 0x25, 0x7F, 0x40, 100, 0, 3);
                                    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 20);
                                    p_thing->State = PerSt_DEAD;
                                    p_thing->Flag |= TngF_Destroyed;
                                    p_thing->StartFrame = 1069;
                                    p_thing->Frame = nstart_ani[1069];
                                }
                            }
                        }
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;

                    p_sthing = &sthings[thing];
                    if ((p_sthing->Type == SmTT_SCALE_EFFECT) && !negative)
                    {
                        dy = (p_sthing->Y >> 8) - y;
                        if ((dy < 0xB9) && (dy > -0xB9))
                        {
                            dx = (p_sthing->X >> 8) - x;
                            dz = (p_sthing->Z >> 8) - z;
                            dist = shockwave_dist(dx, dz);
                            if (dist < (ushort)p_sthing->Radius + radius)
                                do_shockwave_scale_effect(dx, dz, dist, intensity,
                                  p_sthing, NULL);
                        }
                    }
                    if (((p_sthing->Type == SmTT_DROPPED_ITEM) && (p_sthing->SubType == 0xD))
                      || (p_sthing->SubType == 0xC))
                    {
                        dy = (p_sthing->Y >> 8) - y;
                        if ((dy < 0xB9) && (dy > -0xB9))
                        {
                            dx = (p_sthing->X >> 8) - x;
                            dz = (p_sthing->Z >> 8) - z;
                            dist = shockwave_dist(dx, dz);
                            if (dist < (ushort)p_sthing->Radius + radius)
                            {
                                p_sthing->Flag |= TngF_Unkn00040000;
                                if (p_sthing->Timer1 == 999)
                                    p_sthing->Timer1 = 140;
                            }
                        }
                    }
                    thing = sthings[thing].Next;
                }
            }
        }
    }
}


static DIFFTEST_REF_FUNC void do_shockwave_building_asm(int dist, int intensity,
  struct Thing *p_thing, struct Thing *p_owner)
{
    asm volatile (
      "call ASM_do_shockwave_building\n"
        : : "a" (dist), "d" (intensity), "b" (p_thing), "c" (p_owner));
}

void do_shockwave_building(int dist, int intensity, struct Thing *p_thing, struct Thing *p_owner)
{
    DIFFTEST_SHADOW(do_shockwave_building,
      do_shockwave_building_asm(dist, intensity, p_thing, p_owner));

    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;

    p_thing->Health -= (intensity >> 4);
    if (p_thing->Health < 0)
    {
        collapse_building(p_thing->X >> 8, p_thing->Y >> 8,
          p_thing->Z >> 8, p_thing);
    }
}

static DIFFTEST_REF_FUNC void do_shockwave_vehicle_asm(int dx, int dz,
  int dist, int intensity, struct Thing *p_vevicle, struct Thing *p_owner)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)p_vevicle;
    stkargs[1] = (int)(intptr_t)p_owner;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_do_shockwave_vehicle\n"
        : : "a" (dx), "d" (dz), "b" (dist), "c" (intensity), "S" (stkargs)
        : "cc", "memory");
}

void do_shockwave_vehicle(int dx, int dz, int dist, int intensity,
  struct Thing *p_vevicle, struct Thing *p_owner)
{
    s32 dist_sq;
    s32 damage;

    DIFFTEST_SHADOW(do_shockwave_vehicle,
      do_shockwave_vehicle_asm(dx, dz, dist, intensity, p_vevicle, p_owner));

    if ((p_vevicle->Flag & TngF_Destroyed) != 0)
        return;

    // Local copy: writing to the parameter would change the caller's
    // stack slot, which the asm does not do.
    damage = intensity;
    dist_sq = (dist >> 7) * (dist >> 7);
    if (dist_sq > 0)
        damage /= dist_sq;
    if (p_vevicle->SubType == SubTT_VEH_TANK)
        damage >>= 1;
    p_vevicle->Health -= damage;

    p_vevicle->U.UVehicle.RecoilTimer = 10;
    if (p_vevicle->Health > 0)
        return;
    if (p_vevicle->Type == TT_VEHICLE)
        start_crashing(p_vevicle);
    else
        init_mgun_explode(p_vevicle);
}

static DIFFTEST_REF_FUNC void do_shockwave_person_asm(int dx, int dz,
  int dist, int intensity, struct Thing *p_person, struct Thing *p_owner)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)p_person;
    stkargs[1] = (int)(intptr_t)p_owner;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_do_shockwave_person\n"
        : : "a" (dx), "d" (dz), "b" (dist), "c" (intensity), "S" (stkargs)
        : "cc", "memory");
}

/** Count a person killed by a shockwave in the mission statistics.
 */
static void shockwave_person_killed_add_to_stats(struct Thing *p_person,
  struct Thing *p_owner)
{
    struct MissionStatus *p_mistat;

    if (!in_network_game)
    {
        if (p_owner->U.UPerson.EffectiveGroup != ingame.MyGroup)
            return;
        p_mistat = &mission_status[open_brief];
        switch ((ubyte)(p_person->SubType - 1))
        {
        case SubTT_PERS_AGENT - 1:
        case SubTT_PERS_ZEALOT - 1:
        case SubTT_PERS_PUNK_F - 1:
        case SubTT_PERS_PUNK_M - 1:
        case SubTT_PERS_HIGH_PRIEST - 1:
            p_mistat->SP.EnemiesKilled++;
            break;
        case SubTT_PERS_BRIEFCASE_M - 1:
        case SubTT_PERS_WHITE_BRUN_F - 1:
        case SubTT_PERS_SCIENTIST - 1:
        case SubTT_PERS_SHADY_M - 1:
        case SubTT_PERS_WHIT_BLOND_F - 1:
        case SubTT_PERS_LETH_JACKT_M - 1:
            p_mistat->SP.CivsKilled++;
            break;
        case SubTT_PERS_MERCENARY - 1:
        case SubTT_PERS_MECH_SPIDER - 1:
        case SubTT_PERS_POLICE - 1:
            p_mistat->SP.SecurityKilled++;
            break;
        default:
            break;
        }
    }
    else
    {
        struct Thing *p_killer;

        if ((p_person->Flag & TngF_PlayerAgent) == 0)
            return;
        p_killer = NULL;
        if ((p_owner->Flag & TngF_PlayerAgent) != 0)
        {
            p_killer = p_owner;
        }
        else if ((p_owner->Flag & TngF_Persuaded) != 0)
        {
            struct Thing *p_ownown;

            p_ownown = &things[(ushort)p_owner->Owner];
            if ((p_ownown->Flag & TngF_PlayerAgent) != 0)
                p_killer = p_ownown;
        }
        if (p_killer == NULL)
            return;
        if ((p_killer->Flag & TngF_PlayerAgent) == 0)
            return;
        // TODO no range checks on the player indexes, same as the original
        p_mistat = &mission_status[p_killer->U.UPerson.ComCur >> 2];
        p_mistat->MP.AgentsKilled[p_person->U.UPerson.ComCur >> 2]++;
    }
}

void do_shockwave_person(int dx, int dz, int dist, int intensity,
  struct Thing *p_person, struct Thing *p_owner)
{
    int dist_sq, damage;
    int recoil_x, recoil_z;
    short health;

    DIFFTEST_SHADOW(do_shockwave_person,
      do_shockwave_person_asm(dx, dz, dist, intensity, p_person, p_owner));

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    dist_sq = (dist >> 7) * (dist >> 7);
    damage = intensity;
    if (dist_sq > 0)
        damage = intensity / dist_sq;

    if (cybmod_chest_level(&p_person->U.UPerson.UMod) < 4)
        p_person->Health -= (short)damage;

    if ((p_person->Type == TT_MINE) && (p_person->Health <= 0))
    {
        play_dist_ssample((struct SimpleThing *)p_person, 0x25, 0x7F, 0x40,
          100, 0, 3);
        // TODO only Y is converted from fine coords, same as the original
        bang_new4(p_person->X, p_person->Y >> 8, p_person->Z, 20);
        remove_thing(p_person->ThingOffset);
        delete_node(p_person);
        return;
    }

    health = p_person->Health;
    p_person->U.UPerson.ShieldEnergy = 0;
    if (health < 0)
    {
        if (p_owner != NULL)
            shockwave_person_killed_add_to_stats(p_person, p_owner);
        set_person_dead(p_person, 11);
        return;
    }

    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
        return;

    recoil_x = dx;
    recoil_z = dz;
    if ((recoil_x == 0) && (recoil_z == 0))
    {
        recoil_x = (LbRandomAnyShort() & 0x1FF) - 0x100;
        recoil_z = (LbRandomAnyShort() & 0x1FF) - 0x100;
    }
    init_recoil(p_person, (short)recoil_x, 0, (short)recoil_z, 0);
    p_person->U.UPerson.RecoilTimer = 10;
}

static DIFFTEST_REF_FUNC void do_shockwave_scale_effect_asm(int dx, int dz,
  int dist, int intensity, struct SimpleThing *p_sthing, struct Thing *p_owner)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)p_sthing;
    stkargs[1] = (int)(intptr_t)p_owner;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_do_shockwave_scale_effect\n"
        : : "a" (dx), "d" (dz), "b" (dist), "c" (intensity), "S" (stkargs)
        : "cc", "memory");
}

void do_shockwave_scale_effect(int dx, int dz, int dist, int intensity,
  struct SimpleThing *p_sthing, struct Thing *p_owner)
{
    s32 dist_sq, force;

    DIFFTEST_SHADOW(do_shockwave_scale_effect,
      do_shockwave_scale_effect_asm(dx, dz, dist, intensity, p_sthing, p_owner));

    force = intensity;
    if (force > 2000)
        force = 2000;
    dist_sq = (dist >> 5) * (dist >> 5);
    if (dist_sq > 0)
        force /= dist_sq;

    p_sthing->U.UEffect.VX += (dx * force) >> 3;
    p_sthing->U.UEffect.VZ += (dz * force) >> 3;
}
/******************************************************************************/
