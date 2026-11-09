/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing.c
 *     Thing structure support.
 * @par Purpose:
 *     Implement general functions for handling Thing instances.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Apr 2022 - 27 May 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing.h"

#include "bfbox.h"
#include "bfutility.h"
#include "bfmath.h"
#include "bfmemut.h"
#include "bfscreen.h"
#include "ssampply.h"
#include <assert.h>
#include <stdlib.h>

#include "bigmap.h"
#include "building.h"
#include "bmbang.h"
#include "bmcrater.h"
#include "command.h"
#include "difftest.h"
#include "display.h"
#include "engindrwlstx.h"
#include "engincam.h"
#include "enginlights.h"
#include "enginshrapn.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "enginzoom.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvdraw3d.h"
#include "matrix.h"
#include "packet.h"
#include "pepgroup.h"
#include "player.h"
#include "sound.h"
#include "thing_expld.h"
#include "thing_fire.h"
#include "thing_search.h"
#include "vehicle.h"
#include "weapon.h"
#include "game.h"
#include "swlog.h"
/******************************************************************************/
#pragma pack(1)

struct UnkFLight { // sizeof=0x0A
    short unfulgt_0;
    short unfulgt_2;
    short unfulgt_4;
    short unfulgt_6;
    ubyte unfulgt_8;
    ubyte unfulgt_9;
};

struct AnimPage { // sizeof=0x0A
	ubyte PageNo;
	ubyte X;
	ubyte Y;
	ubyte Width;
	ubyte Height;
	ubyte No;
	u32 apunknfld_6;
};

#pragma pack()
/******************************************************************************/

struct UnkFLight unkn_full_lights[50];
ushort next_unkn_full_light = 1;

u32 things_init_times = 0;

ubyte debug_log_things = 0;

/** Radiuses of Things of type STATIC.
 */
short static_radii[] = {
  40, 50, 80, 15,  0, 40,  0,  0,  0, 40,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0, 70, 70, 70, 70,
 130, 40, 80, 40, 40, 40, 50,100, 40, 40,
   0,  0,  0,  0,  0,  0,  0,  0, 40, 70,
  40, 40, 80,100,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

struct AnimPage door_anim_page[] = {
  {2,    0, 0x20, 0x20, 0x20, 5, 0},
  {2, 0x60, 0x60, 0x10, 0x20, 5, 0},
  {2, 0x60, 0x60, 0x10, 0x20, 5, 0},
};

const char *thing_type_names[] = {
  "NONE",
  "UNKN1",
  "VEHICLE",
  "PERSON",
  "UNKN4",
  "STATIC",
  "ROCKET",
  "UNKN7",
  "UNKN8",
  "BUILDING",
  "UNKN10",
  "LASER11",
  "LASER_GUIDED",
  "UNKN13",
  "UNKN14",
  "MINE",
  "GRENADE",
  "SPARK",
  "UNKN18",
  "LASER_ELEC",
  "INTELLIG_DOOR",
  "SCALE_EFFECT",
  "NUCLEAR_BOMB",
  "UNKN23",
  "SMOKE_GENERTR",
  "DROPPED_ITEM",
  "CARRIED_ITEM",
  "ELECTR_STRAND",
  "RAZOR_WIRE",
  "LASER29",
  "TIME_POD",
  "AIR_STRIKE",
  "CANISTER",
  "VEH_TURRET",
  "UNKN34",
  "UNKN35",
  "STASIS_POD",
  "SOUL",
  "LASER38",
  "UNKN39",
  "UNKN40",
  "UNKN41",
  "UNKN42",
  "UNKN43",
  "UNKN44",
  "UNKN45",
  "UNKN46",
  "UNKN47",
  "UNKN48",
  "UNKN49",
  "BANG",
  "UNKN51",
  "FIRE",
  "SFX",
  "TEMP_LIGHT",
  "UNKN55",
  "UNKN56",
};

const char *state_change_result_names[] = {
    "accepted",
    "already done",
    "change denied",
    "goal unattainable",
};

struct Thing *things = NULL;
ushort things_used;
ThingIdx things_used_head;
ThingIdx things_empty_head;

struct SimpleThing *sthings = NULL;
ushort sthings_used;
ThingIdx sthings_used_head;
ThingIdx sthings_empty_head;

ThingIdx same_type_head[256 + PEOPLE_GROUPS_LIMIT + 1] = {0};

/******************************************************************************/

TbBool thing_type_is_simple(short ttype)
{
    return (ttype == SmTT_STATIC) ||
     (ttype == SmTT_SPARK) ||
     (ttype == SmTT_INTELLIG_DOOR) ||
     (ttype == SmTT_SCALE_EFFECT) ||
     (ttype == SmTT_NUCLEAR_BOMB) ||
     (ttype == SmTT_SMOKE_GENERATOR) ||
     (ttype == SmTT_DROPPED_ITEM) ||
     (ttype == SmTT_CARRIED_ITEM) ||
     (ttype == SmTT_ELECTRIC_STRAND) ||
     (ttype == SmTT_TIME_POD) ||
     (ttype == SmTT_CANISTER) ||
     (ttype == SmTT_STASIS_POD) ||
     (ttype == SmTT_SOUL) ||
     (ttype == SmTT_BANG) ||
     (ttype == SmTT_FIRE) ||
     (ttype == SmTT_SFX) ||
     (ttype == SmTT_TEMP_LIGHT);
}

TbBool thing_type_is_pickup_item(short ttype)
{
    return (ttype == SmTT_DROPPED_ITEM) ||
      (ttype == SmTT_CARRIED_ITEM);
}

struct Thing *get_thing_safe(ThingIdx thing, ubyte ttype)
{
    struct Thing *p_thing;

    if (thing_type_is_simple(ttype))
    {
        if ((thing > -1) || (thing < STHINGS_LIMIT))
            return INVALID_THING;
    }
    else
    {
        if ((thing < 1) || (thing > THINGS_LIMIT))
            return INVALID_THING;
    }
    p_thing = &things[thing];
    if (p_thing->Type != ttype)
            return INVALID_THING;
    return p_thing;
}

TbBool thing_position_uses_y_mul_8(struct Thing *p_thing)
{
    switch (p_thing->Type)
    {
    case TT_BUILDING:
        return (p_thing->SubType == SubTT_BLD_MGUN);
    case TT_VEHICLE:
    case TT_PERSON:
    default:
        return true;
    }
}

void get_thing_position_mapcoords(short *x, short *y, short *z, ThingIdx thing)
{
    short cor_x, cor_y, cor_z;

    if (thing >= 0) {
        struct Thing *p_thing;
        p_thing = &things[thing];
        cor_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
        if (thing_position_uses_y_mul_8(p_thing))
            cor_y = PRCCOORD_TO_YCOORD(p_thing->Y);
        else
            cor_y = PRCCOORD_TO_MAPCOORD(p_thing->Y);
        cor_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    } else {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        cor_x = PRCCOORD_TO_MAPCOORD(p_sthing->X);
        cor_y = PRCCOORD_TO_YCOORD(p_sthing->Y);
        cor_z = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
    }
    if (x != NULL)
        *x = cor_x;
    if (y != NULL)
        *y = cor_y;
    if (z != NULL)
        *z = cor_z;
}

u32 get_things_distance_mapcoords_precise(ThingIdx tng1, ThingIdx tng2)
{
    short x1, y1, z1;
    short x2, y2, z2;
    get_thing_position_mapcoords(&x1, &y1, &z1, tng1);
    get_thing_position_mapcoords(&x2, &y2, &z2, tng2);
    return map_distance_deltas_precise(x2 - x1, y2 - y1, z2 - z1);
}

u32 get_things_distance_mapcoords_fast(ThingIdx tng1, ThingIdx tng2)
{
    short x1, y1, z1;
    short x2, y2, z2;
    get_thing_position_mapcoords(&x1, &y1, &z1, tng1);
    get_thing_position_mapcoords(&x2, &y2, &z2, tng2);
    return map_distance_deltas_fast(x2 - x1, y2 - y1, z2 - z1);
}

const char *state_change_result_name(StateChRes res)
{
    return state_change_result_names[res];
}

struct Thing *effective_owner_of_thing(struct Thing *p_thing)
{
    struct Thing *p_owntng;

    if (p_thing == NULL) {
        return NULL;
    }

    // Consider player agent an owner even if it got persuaded
    if ((p_thing->Flag & TngF_PlayerAgent) != 0) {
        return p_thing;
    }

    p_owntng = &things[p_thing->Owner];

    if (((p_thing->Flag & TngF_Persuaded) != 0) && (p_owntng->Flag & TngF_PlayerAgent) != 0) {
        return p_owntng;
    }

    if (p_thing->Type == TT_MINE && p_thing->SubType == 48) {
        return p_owntng;
    }

    return p_thing;
}

ubyte on_mapwho(struct Thing *p_thing)
{
    ubyte ret;
    asm volatile (
      "call ASM_on_mapwho\n"
        : "=r" (ret) : "a" (p_thing));
    return ret;
}

static DIFFTEST_REF_FUNC void move_mapwho_asm(struct Thing *p_thing, int x, int y, int z)
{
    asm volatile (
      "call ASM_move_mapwho\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z));
}

void move_mapwho(struct Thing *p_thing, int x, int y, int z)
{
    ushort tile_x, tile_z;
    ushort mapel_idx;
    short nxt;
    short tng_offs;

    DIFFTEST_SHADOW(move_mapwho, move_mapwho_asm(p_thing, x, y, z));
    if ((x >> 8) >= 0x8000 || (z >> 8) >= 0x8000)
        return;
    if ((x >> 8) < 0 || (z >> 8) < 0)
        return;

    if ((x >> 16) >= 0x80 || (z >> 16) >= 0x80 ||
      (x >> 16) < 0 || (z >> 16) < 0)
    {
        // Wrap tile coords into the map; this modifies the stored coords too
        x = (x & 0xFFFF) | (((x >> 16) & 0x7F) << 16);
        z = (z & 0xFFFF) | (((z >> 16) & 0x7F) << 16);
    }
    tile_x = (ushort)(x >> 16);
    tile_z = (ushort)(z >> 16);

    if ((p_thing->Flag2 & TgF2_ExistsOffMap) == 0 &&
      (tile_x != (ushort)(p_thing->X >> 16) || tile_z != (ushort)(p_thing->Z >> 16)))
    {
        // The asm calls an empty function (nullsub_3) here for TT_PERSON
        mapel_idx = (ushort)((tile_z << 7) + tile_x);
        delete_node(p_thing);
        nxt = game_my_big_map[mapel_idx].Child;
        p_thing->Next = nxt;
        tng_offs = p_thing->ThingOffset;
        if (nxt > 0)
            things[nxt].Parent = tng_offs;
        else if (nxt < 0)
            sthings[nxt].Parent = tng_offs;
        p_thing->Parent = 0;
        game_my_big_map[mapel_idx].Child = tng_offs;
    }
    p_thing->Y = y;
    p_thing->X = x;
    p_thing->Z = z;
}

static DIFFTEST_REF_FUNC void init_just_things_asm(void)
{
    asm volatile ("call ASM_init_just_things\n"
        :  :  : "eax" );
}

void init_just_things(void)
{
    short i;

    DIFFTEST_SHADOW(init_just_things, init_just_things_asm());

    // Simple things use negative indices, linked as an empty list
    for (i = 1; i < STHINGS_LIMIT; i++)
    {
        struct SimpleThing *p_sthing;

        p_sthing = &sthings[-i];
        memset(p_sthing, 0, sizeof(struct SimpleThing));
        p_sthing->LinkParent = -(i - 1);
        p_sthing->U.UEffect.VZ = 0;
        p_sthing->ThingOffset = -i;
        p_sthing->LinkChild = -(i + 1);
    }
    sthings_empty_head = -1;
    sthings_used_head = 0;

    for (i = 1; i < THINGS_LIMIT; i++)
    {
        struct Thing *p_thing;

        p_thing = &things[i];
        memset(p_thing, 0, sizeof(struct Thing));
        p_thing->LinkParent = i - 1;
        p_thing->ThingOffset = i;
        p_thing->LinkChild = i + 1;
    }
    things_empty_head = 1;
    things_used_head = 0;
    things_used = 0;
    sthings_used = 0;
    // TODO the original writes past the last valid thing, maybe a bug
    things[THINGS_LIMIT].ThingOffset = THINGS_LIMIT;
}

void init_things(void)
{
#if 0
    asm volatile ("call ASM_init_things\n"
        :  :  : "eax" );
#endif
    things_init_times++;
    gameturn = 0;

    init_things_memory_with_user_heap();
    init_just_things();
    init_commands();
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void quick_light_unkn_func_07_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_quick_light_unkn_func_07\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Unlinks from a face point QuickLight chain all entries referencing
 * given light; visits at most 201 entries of the chain.
 */
static void quick_light_unlink_from_chain(struct QuickLight *p_qlights,
  ushort *p_head, ushort light, ushort *p_count)
{
    ushort qlight, prev_qlight;
    ushort n;

    prev_qlight = 0;
    n = 0;
    qlight = *p_head;
    while (qlight != 0)
    {
        struct QuickLight *p_qlight;

        if (n++ >= 201)
            break;
        p_qlight = &p_qlights[qlight];
        if (p_qlight->Light == light)
        {
            (*p_count)--;
            if (prev_qlight == 0)
                *p_head = p_qlight->NextQuick;
            else
                p_qlights[prev_qlight].NextQuick = p_qlight->NextQuick;
            p_qlight->Light = 0;
        }
        else
        {
            prev_qlight = qlight;
        }
        qlight = p_qlights[qlight].NextQuick;
    }
}

/** Unlinks from the QuickLight chains of all face points of given object
 * all entries referencing given light.
 */
void quick_light_unkn_func_07(int a1, int a2, int a3, int a4)
{
    struct SingleObject *p_sobj;
    struct QuickLight *p_qlights;
    ushort light = (ushort)a2;
    ushort count;
    ushort start_face, numb_faces, i;

    DIFFTEST_SHADOW(quick_light_unkn_func_07, quick_light_unkn_func_07_asm(a1, a2, a3, a4));

    p_qlights = game_quick_lights;
    count = next_quick_light;
    p_sobj = &game_objects[(ushort)a1];

    start_face = p_sobj->StartFace;
    numb_faces = p_sobj->NumbFaces;
    for (i = 0; i < numb_faces; i++)
    {
        struct SingleObjectFace3 *p_face;

        p_face = &game_object_faces3[(s32)start_face + i];
        quick_light_unlink_from_chain(p_qlights, &p_face->Light0, light, &count);
        quick_light_unlink_from_chain(p_qlights, &p_face->Light1, light, &count);
        quick_light_unlink_from_chain(p_qlights, &p_face->Light2, light, &count);
    }

    start_face = p_sobj->StartFace4;
    numb_faces = p_sobj->NumbFaces4;
    for (i = 0; i < numb_faces; i++)
    {
        struct SingleObjectFace4 *p_face;

        p_face = &game_object_faces4[(s32)start_face + i];
        quick_light_unlink_from_chain(p_qlights, (ushort *)&p_face->Light0, light, &count);
        quick_light_unlink_from_chain(p_qlights, (ushort *)&p_face->Light1, light, &count);
        quick_light_unlink_from_chain(p_qlights, (ushort *)&p_face->Light2, light, &count);
        quick_light_unlink_from_chain(p_qlights, (ushort *)&p_face->Light3, light, &count);
    }

    next_quick_light = count;
    // The asm stores the pointer back unchanged
    game_quick_lights = p_qlights;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void quick_light_unkn_func_09_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_quick_light_unkn_func_09\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Unlinks from the map element's QuickLight chain all entries
 * referencing given light.
 */
void quick_light_unkn_func_09(int a1, int a2, int a3, int a4)
{
    struct MyMapElement *p_mapel = (struct MyMapElement *)a1;
    ushort light = (ushort)a2;
    struct QuickLight *p_qlights;
    ushort qlight, prev_qlight;

    DIFFTEST_SHADOW(quick_light_unkn_func_09, quick_light_unkn_func_09_asm(a1, a2, a3, a4));

    p_qlights = game_quick_lights;
    prev_qlight = 0;
    qlight = p_mapel->Shade;
    while (qlight != 0)
    {
        struct QuickLight *p_qlight;

        p_qlight = &p_qlights[qlight];
        if (p_qlight->Light == light)
        {
            next_quick_light--;
            if (prev_qlight == 0)
                p_mapel->Shade = p_qlight->NextQuick;
            else
                p_qlights[prev_qlight].NextQuick = p_qlight->NextQuick;
            p_qlight->Light = 0;
        }
        else
        {
            prev_qlight = qlight;
        }
        qlight = p_qlights[qlight].NextQuick;
    }
    // The asm stores the pointer back unchanged
    game_quick_lights = p_qlights;
}



static DIFFTEST_REF_FUNC void quick_light_unkn_func_04_asm(short a1, int a2, short a3, short a4)
{
    asm volatile (
      "call ASM_quick_light_unkn_func_04\n"
        : : "a" (a1), "d" (a2), "b" (a3), "c" (a4));
    return;
}

/** Removes given light from map elements and building objects within
 * 7 tiles of given position, and decrements full lights count.
 */
void quick_light_unkn_func_04(short a1, int a2, short a3, short a4)
{
    ushort light = (ushort)a4;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(quick_light_unkn_func_04, quick_light_unkn_func_04_asm(a1, a2, a3, a4));

    // Parameter a2 is not used by the asm
    tile_x = a1 >> 8;
    tile_z = a3 >> 8;
    for (dx = -7; dx < 7; dx++)
    {
        for (dz = -7; dz < 7; dz++)
        {
            struct MyMapElement *p_mapel;
            s32 mx, mz;
            ThingIdx thing;

            mx = tile_x + dx;
            if (mx <= 0 || mx >= 128)
                continue;
            mz = tile_z + dz;
            if (mz <= 0 || mz >= 128)
                continue;
            p_mapel = &game_my_big_map[(mz << 7) + mx];
            if (p_mapel->Shade != 0)
                quick_light_unkn_func_09((int)p_mapel, light, 0, 0);

            thing = p_mapel->Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if ((p_thing->Type == TT_BUILDING || p_thing->Type == TT_UNKN55)
                      && p_thing->SubType != SubTT_BLD_MGUN)
                    {
                        short i;

                        for (i = 0; i < p_thing->U.UObject.NumbObjects; i++)
                        {
                            quick_light_unkn_func_07(
                              (ushort)(p_thing->U.UObject.Object + i),
                              light, thing, i);
                        }
                    }
                    thing = things[thing].Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
    next_full_light--;
}

void unkn_full_update_lights(void)
{
    ushort i;

    for (i = 1; i < next_unkn_full_light; i++)
    {
        struct UnkFLight *p_ufl;
        p_ufl = &unkn_full_lights[i];
        quick_light_unkn_func_04(p_ufl->unfulgt_2, p_ufl->unfulgt_4, p_ufl->unfulgt_6, p_ufl->unfulgt_0);
    }
    next_unkn_full_light = 1;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void quick_light_unkn_func_06_asm(int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[3];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    stkargs[2] = a7;
    asm volatile ("push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_quick_light_unkn_func_06\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

extern ushort obj_face3_create_normal(short face);
extern ushort obj_face4_create_normal(short face);

/** Adds a quick light to the chain of given face point, if the light
 * is bright enough at that point. Helper for quick_light_unkn_func_06().
 */
static void quick_light_unkn_func_06_point(ushort *p_qlgt, ushort *p_chain,
  ushort point, s32 ratio, s32 obj_x, s32 obj_y, s32 obj_z, s32 x, s32 y, s32 z,
  ushort light)
{
    struct SinglePoint *p_pt;
    struct QuickLight *p_qlight;
    s32 px, py, pz, dist, lratio, intens;

    p_pt = &game_object_points[point];
    px = p_pt->X + obj_x - x;
    py = p_pt->Y + obj_y - y;
    pz = p_pt->Z + obj_z - z;
    dist = (px * px + py * py + pz * pz) >> 8;
    if (dist == 0)
        dist = 1;
    lratio = (ratio << 8) / dist;
    intens = (game_full_lights[light].Intensity * lratio) >> 8;
    if (intens <= 3)
        return;
    p_qlight = &game_quick_lights[*p_qlgt];
    p_qlight->NextQuick = *p_chain;
    *p_chain = *p_qlgt;
    p_qlight->Light = light;
    p_qlight->Ratio = lratio;
    (*p_qlgt)++;
}

void quick_light_unkn_func_06(int a1, int a2, int a3, int a4, int a5, int a6, int a7)
{
    struct SingleObject *p_sobj;
    struct SingleObjectFace3 *p_face3;
    struct SingleObjectFace4 *p_face4;
    struct Normal *p_nrml;
    ushort start_face, numb_faces, start_face4, numb_faces4;
    ushort qlgt, light, i;
    s32 x, y, z;
    s32 obj_x, obj_y, obj_z;
    s32 dx, dy, dz, dist;
    s32 vx, vy, vz, ratio;

    DIFFTEST_SHADOW(quick_light_unkn_func_06,
      quick_light_unkn_func_06_asm(a1, a2, a3, a4, a5, a6, a7));
    // a5 and a6 are not used by the asm; a7 is the full light index
    x = a2;
    y = a3;
    z = a4;
    light = (ushort)a7;
    p_sobj = &game_objects[(ushort)a1];
    start_face4 = p_sobj->StartFace4;
    numb_faces4 = p_sobj->NumbFaces4;
    start_face = p_sobj->StartFace;
    numb_faces = p_sobj->NumbFaces;
    obj_x = p_sobj->MapX;
    obj_y = p_sobj->OffsetY;
    obj_z = p_sobj->MapZ;

    dx = x - obj_x;
    dy = y - obj_y;
    dz = z - obj_z;
    dist = LbSqrL(dy * dy + dx * dx + dz * dz);
    if (dist == 0)
        dist = 1;
    vx = (-dx << 8) / dist;
    vy = (-dy << 8) / dist;
    vz = (-dz << 8) / dist;

    qlgt = next_quick_light;
    for (i = 0; i < numb_faces; i++)
    {
        ushort nrml;

        p_face3 = &game_object_faces3[start_face + i];
        nrml = p_face3->FaceNormal;
        next_quick_light = qlgt;
        if (nrml < 1 || nrml >= 10000)
            p_face3->FaceNormal = obj_face3_create_normal((ushort)(start_face + i));
        qlgt = next_quick_light;
        game_normals[p_face3->FaceNormal].LightRatio = -1;
    }

    for (i = 0; i < numb_faces4; i++)
    {
        ushort nrml;

        p_face4 = &game_object_faces4[start_face4 + i];
        nrml = p_face4->FaceNormal;
        next_quick_light = qlgt;
        if (nrml < 1 || nrml >= 10000)
            p_face4->FaceNormal = obj_face4_create_normal((ushort)(start_face4 + i));
        qlgt = next_quick_light;
        game_normals[p_face4->FaceNormal].LightRatio = -1;
    }

    for (i = 0; i < numb_faces; i++)
    {
        if (qlgt >= 64000)
        {
            next_quick_light = qlgt;
            return;
        }
        p_face3 = &game_object_faces3[start_face + i];
        p_nrml = &game_normals[p_face3->FaceNormal];
        ratio = p_nrml->LightRatio;
        if (ratio == -1)
        {
            ratio = (p_nrml->NX * vx + p_nrml->NY * vy + p_nrml->NZ * vz) >> 8;
            if (ratio < 0)
                continue;
            p_nrml->LightRatio = ratio;
        }
        quick_light_unkn_func_06_point(&qlgt, &p_face3->Light0,
          (ushort)p_face3->PointNo[0], ratio, obj_x, obj_y, obj_z, x, y, z, light);
        quick_light_unkn_func_06_point(&qlgt, &p_face3->Light1,
          (ushort)p_face3->PointNo[1], ratio, obj_x, obj_y, obj_z, x, y, z, light);
        quick_light_unkn_func_06_point(&qlgt, &p_face3->Light2,
          (ushort)p_face3->PointNo[2], ratio, obj_x, obj_y, obj_z, x, y, z, light);
    }

    for (i = 0; i < numb_faces4; i++)
    {
        if (qlgt >= 64000)
            break;
        p_face4 = &game_object_faces4[start_face4 + i];
        p_nrml = &game_normals[p_face4->FaceNormal];
        ratio = p_nrml->LightRatio;
        if (ratio == -1)
        {
            ratio = (p_nrml->NX * vx + p_nrml->NY * vy + p_nrml->NZ * vz) >> 8;
            if (ratio < 0)
                continue;
            p_nrml->LightRatio = ratio;
        }
        quick_light_unkn_func_06_point(&qlgt, (ushort *)&p_face4->Light0,
          (ushort)p_face4->PointNo[0], ratio, obj_x, obj_y, obj_z, x, y, z, light);
        quick_light_unkn_func_06_point(&qlgt, (ushort *)&p_face4->Light1,
          (ushort)p_face4->PointNo[1], ratio, obj_x, obj_y, obj_z, x, y, z, light);
        quick_light_unkn_func_06_point(&qlgt, (ushort *)&p_face4->Light2,
          (ushort)p_face4->PointNo[2], ratio, obj_x, obj_y, obj_z, x, y, z, light);
        quick_light_unkn_func_06_point(&qlgt, (ushort *)&p_face4->Light3,
          (ushort)p_face4->PointNo[3], ratio, obj_x, obj_y, obj_z, x, y, z, light);
    }
    next_quick_light = qlgt;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void quick_light_unkn_func_08_asm(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[2];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    asm volatile ("push 4(%4)\n""push 0(%4)\n""call ASM_quick_light_unkn_func_08\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

extern s32 dword_176D6C;

void quick_light_unkn_func_08(int a1, int a2, int a3, int a4, int a5, int a6)
{
    struct MyMapElement *p_mapel;
    struct QuickLight *p_qlight;
    ushort tile_x, tile_z, lgt;
    s32 dx, dy, dz, dist_sq, dist, ratio, div;
    s32 intens;

    DIFFTEST_SHADOW(quick_light_unkn_func_08,
      quick_light_unkn_func_08_asm(a1, a2, a3, a4, a5, a6));
    tile_x = (ushort)a1;
    tile_z = (ushort)a2;
    p_mapel = &game_my_big_map[(u32)tile_z * 128 + tile_x];

    dx = (s32)((u32)a3 - ((u32)tile_x << 8));
    dy = (s32)(((u32)a4 - ((u32)(s32)p_mapel->Alt << 3)) << 4);
    dz = (s32)((u32)a5 - ((u32)tile_z << 8));
    dist_sq = (s32)((u32)dy * (u32)dy + (u32)dx * (u32)dx + (u32)dz * (u32)dz);
    if (dist_sq > 0x4640000)
    {
        dword_176D6C++;
        return;
    }
    dist = LbSqrL(dist_sq);
    if (dist == 0)
        dist = 1;
    ratio = (s32)((u32)dy << 8) / dist;

    if (next_quick_light >= 0xfa00)
        return;

    div = (s32)((u32)dx * (u32)dx + (u32)dz * (u32)dz + (u32)(dy >> 3) * (u32)(dy >> 3));
    div >>= 8;
    if (div == 0)
        div = 1;
    ratio = (s32)((u32)ratio << 8) / div;
    if (ratio > 0x100)
        ratio = 0x100;

    lgt = (ushort)a6;
    intens = (s32)((u32)(s32)game_full_lights[lgt].Intensity * (u32)ratio) >> 8;
    if (intens <= 3)
        return;

    p_qlight = &game_quick_lights[next_quick_light];
    p_qlight->NextQuick = p_mapel->Shade;
    p_mapel->Shade = next_quick_light;
    p_qlight->Light = (ushort)a6;
    next_quick_light++;
    p_qlight->Ratio = (ushort)ratio;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void quick_light_unkn_func_05_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_quick_light_unkn_func_05\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Adds a new full light at given position (a1=X, a2=Y, a3=Z, a4=intensity),
 * and applies it to map elements and building objects within 7 tiles.
 */
void quick_light_unkn_func_05(int a1, int a2, int a3, int a4)
{
    struct FullLight *p_fulight;
    short x, y, z;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(quick_light_unkn_func_05,
      quick_light_unkn_func_05_asm(a1, a2, a3, a4));
    x = (short)a1;
    y = (short)a2;
    z = (short)a3;
    p_fulight = &game_full_lights[next_full_light];
    p_fulight->lgtfld_10 = 0;
    p_fulight->lgtfld_12 = 0;
    p_fulight->Intensity = (short)a4;
    p_fulight->TrueIntensity = (short)a4;
    p_fulight->lgtfld_E = (short)a4;
    p_fulight->Z = z;
    p_fulight->X = x;
    p_fulight->Y = y;

    tile_x = x >> 8;
    tile_z = z >> 8;
    for (dx = -7; dx < 7; dx++)
    {
        for (dz = -7; dz < 7; dz++)
        {
            s32 mx, mz;
            ThingIdx thing;

            mx = tile_x + dx;
            if (mx <= 0 || mx >= 128)
                continue;
            mz = tile_z + dz;
            if (mz <= 0 || mz >= 128)
                continue;
            quick_light_unkn_func_08((ushort)mx, (ushort)mz, x, y, z,
              next_full_light);

            thing = game_my_big_map[(mz << 7) + mx].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING
                      && p_thing->SubType != SubTT_BLD_MGUN)
                    {
                        short i;

                        for (i = 0; i < p_thing->U.UObject.NumbObjects; i++)
                        {
                            quick_light_unkn_func_06(
                              (ushort)(p_thing->U.UObject.Object + i),
                              x, y, z, mx << 8, mz << 8, next_full_light);
                        }
                    }
                    thing = things[thing].Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
    next_full_light++;
}


static DIFFTEST_REF_FUNC void apply_full_light_asm(short lx, short lz, ushort b, ushort intens, ushort lid)
{
    int ra = lx, rd = lz, rb = b, rc = intens;
    int stkargs[1];
    int *p_stk = stkargs;
    stkargs[0] = lid;
    asm volatile ("push 0(%4)\n""call ASM_apply_full_light\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

extern ushort data_17fa52;

void apply_full_light(short lx, short lz, ushort b, ushort intens, ushort lid)
{
    struct UnkFLight *p_ufl;
    ushort ufl;
    s32 x, z, range;

    DIFFTEST_SHADOW(apply_full_light,
      apply_full_light_asm(lx, lz, b, intens, lid));
    ufl = next_unkn_full_light;
    if (lid != 0)
    {
        s32 i;
        for (i = 0; i < (s32)ufl; i++)
        {
            // the asm compares a 16-bit value at offset 8
            if (*(ushort *)&unkn_full_lights[i].unfulgt_8 == lid)
            {
                next_unkn_full_light = ufl;
                return;
            }
        }
    }
    x = lx;
    z = b;
    range = (s32)render_area_a << 7;
    if (engn_xc - range >= x)
        goto out;
    if (engn_xc + range <= x)
        goto out;
    if (engn_zc - ((s32)render_area_b << 7) >= z)
        goto out;
    // TODO the asm uses render_area_a for the upper Z bound, maybe an original bug
    if (engn_zc + range <= z)
        goto out;
    if ((s32)ufl >= 10)
        goto out;
    if (data_17fa52 != 0)
        goto out;
    if ((ingame.Flags & GamF_AdvLights) == 0)
        goto out;
    if (in_network_game != 0)
        goto out;

    next_unkn_full_light = ufl;
    quick_light_unkn_func_05(lx, lz, (short)b, intens);
    ufl = next_unkn_full_light;
    p_ufl = &unkn_full_lights[ufl];
    p_ufl->unfulgt_2 = lx;
    p_ufl->unfulgt_4 = lz;
    p_ufl->unfulgt_6 = b;
    *(ushort *)&p_ufl->unfulgt_8 = lid;
    p_ufl->unfulgt_0 = next_full_light - 1;
    ufl++;
out:
    next_unkn_full_light = ufl;
}

int process_things_mines_explode(int affected_max)
{
    int ret;
    asm volatile ("call ASM_process_things_mines_explode\n"
        : "=r" (ret) : "a" (affected_max));
    return ret;
}

void process_things_unkn_sub2(int tech_lv, int got_many)
{
    asm volatile (
      "call ASM_process_things_unkn_sub2\n"
        : : "a" (tech_lv), "d" (got_many));
}

int process_things_bang(int affected_max)
{
    int ret;
    asm volatile ("call ASM_process_things_bang\n"
        : "=r" (ret) : "a" (affected_max));
    return ret;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void light_cmd_unkn_func_06_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_light_cmd_unkn_func_06\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void light_cmd_unkn_func_06(int a1, int a2, int a3, int a4)
{
    struct LightCommand *p_lcmd = (struct LightCommand *)a1;
    ubyte *p_fld = p_lcmd->lcfld_0;
    s32 tile_x, tile_z;
    s32 row_beg, row_end, offs;
    TbBool found;

    DIFFTEST_SHADOW(light_cmd_unkn_func_06,
      light_cmd_unkn_func_06_asm(a1, a2, a3, a4));
    found = false;
    // The asm tests (flags | 0x20) for zero, which is never true
    p_fld[0x14]--;
    if ((sbyte)p_fld[0x14] <= 0)
        p_fld[0] &= ~0x20;
    if ((sbyte)p_fld[0x14] > 0)
        return;

    for (tile_x = p_fld[0xc]; tile_x < p_fld[0xe]; tile_x++)
    {
        row_beg = tile_x * (s32)sizeof(struct MyMapElement);
        row_end = row_beg + 0x48000;
        offs = p_fld[0xd] * 0x900 + row_beg;
        for (tile_z = p_fld[0xd]; tile_z < p_fld[0xf]; tile_z++, offs += 0x900)
        {
            short thing;

            if (tile_x < 0 || tile_x >= 128)
                return;
            if (offs < row_beg || offs >= row_end)
                return;
            thing = ((struct MyMapElement *)((ubyte *)game_my_big_map + offs))->Child;
            if (thing <= 0)
                continue;
            if (things[thing].Type == TT_PERSON)
            {
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    if (found)
    {
        p_fld[0x14] = 20;
        p_fld[0x9] = 0;
        p_fld[0] &= ~0x20;
    }
    else
    {
        p_fld[0x14] = 4;
        p_fld[0x9] = 0;
        p_fld[0] |= 0x20;
    }
}


void unkn_update_lights(void)
{
    asm volatile ("call ASM_unkn_update_lights\n"
        :  :  : "eax" );
    return;
}

int recalc_rocket_direction(struct Thing *p_rocket)
{
#if 0
    int ret;
    asm volatile ("call ASM_recalc_rocket_direction\n"
        : "=r" (ret) : "a" (p_rocket));
    return ret;
#endif
    struct Thing *p_target;
    int cor_dx, cor_dy, cor_dz;
    int new_vx, new_vy, new_vz;
    int dist;

    p_target = p_rocket->PTarget;
    cor_dx = (p_target->X - p_rocket->X) >> 8;
    cor_dz = (p_target->Z - p_rocket->Z) >> 8;
    cor_dy = (p_target->Y - p_rocket->Y) >> 8;
    dist = LbSqrL(cor_dx * cor_dx + cor_dz * cor_dz);
    if (dist == 0)
        dist = 1;
    new_vx = (cor_dx << 8) / dist;
    new_vy = (cor_dy << 8) / dist;
    new_vz = (cor_dz << 8) / dist;
    if (new_vx * p_rocket->VX + new_vy * p_rocket->VY + new_vz * p_rocket->VZ < 0)
        return 0;

    p_rocket->VX = new_vx;
    p_rocket->VY = new_vy;
    p_rocket->VZ = new_vz;
    return 1;
}

void process_rocket(struct Thing *p_rocket)
{
    //TODO modify to make the rocket disappear when falling into sludge
    asm volatile (
      "call ASM_process_rocket\n"
        : : "a" (p_rocket));
}

void process_laser_guided(struct Thing *p_laser)
{
    asm volatile (
      "call ASM_process_laser_guided\n"
        : : "a" (p_laser));
}

void process_c_iff(struct Thing *p_mine)
{
    asm volatile (
      "call ASM_process_c_iff\n"
        : : "a" (p_mine));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void init_electricity_bomb(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_init_electricity_bomb\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z);

static DIFFTEST_REF_FUNC void process_mine_asm(struct SimpleThing *p_mine)
{
    asm volatile (
      "call ASM_process_mine\n"
        : : "a" (p_mine));
}

void process_mine(struct SimpleThing *p_mine)
{
    DIFFTEST_SHADOW(process_mine, process_mine_asm(p_mine));

    if (p_mine->SubType == 13)
    {
        struct SimpleThing *p_bomb;

        if (p_mine->U.UWeapon.Ammo != 0)
            p_mine->U.UWeapon.Ammo--;
        if (p_mine->Timer1 == 999)
            return;
        if (p_mine->Timer1 == 60)
            play_dist_ssample(p_mine, 0x26, 0x7F, 0x40, 100, -1, 1);
        if ((p_mine->Flag & TngF_Unkn00040000) && (p_mine->Timer1 > 7)
          && (p_mine->U.UWeapon.Ammo == 0))
            p_mine->Timer1 = 7;
        p_mine->Timer1--;
        if (p_mine->Timer1 > 0)
            return;
        stop_sample_using_heap(p_mine->ThingOffset, 0x26);
        p_bomb = init_nuclear_bomb(PRCCOORD_TO_MAPCOORD(p_mine->X),
          PRCCOORD_TO_MAPCOORD(p_mine->Y), PRCCOORD_TO_MAPCOORD(p_mine->Z));
        if (p_bomb != NULL)
        {
            play_dist_ssample(p_bomb, 1, 0x7F, 0x40, 100, 0, 3);
            p_bomb->Timer1 = 20;
            p_bomb->Owner2 = p_mine->Owner2;
            p_bomb->Radius = 0;
            p_bomb->Flag &= ~(TngF_InVehicle|TngF_Persuaded);
        }
        remove_sthing(p_mine->ThingOffset);
        ingame.fld_unkCB7--;
        delete_snode(p_mine);
        return;
    }
    else if (p_mine->SubType == 48)
    {
        process_c_iff((struct Thing *)p_mine);
        return;
    }
    else if (p_mine->SubType == 12)
    {
        if (p_mine->U.UWeapon.Ammo != 0)
            p_mine->U.UWeapon.Ammo--;
        if (p_mine->Timer1 == 999)
            return;
        if (p_mine->Timer1 > 0)
        {
            struct Thing *p_owner;

            p_owner = &things[(ushort)p_mine->Owner2];
            if (check_for_radius_hit_person(p_mine->X, p_mine->Y, p_mine->Z,
              0x200, p_owner, 1, 0) != NULL)
                p_mine->Timer1 = 0;
        }
        if (p_mine->Timer1 == 60)
            play_dist_ssample(p_mine, 0x26, 0x7F, 0x40, 100, -1, 1);
        if ((p_mine->Flag & TngF_Unkn00040000) && (p_mine->Timer1 > 7)
          && (p_mine->U.UWeapon.Ammo == 0))
            p_mine->Timer1 = 7;
        if (p_mine->Timer1 > 0)
            return;
        p_mine->Timer1--;
        stop_sample_using_heap(p_mine->ThingOffset, 0x26);
        if ((p_mine->Timer1 == 0) || (p_mine->Timer1 == -6) || (p_mine->Timer1 == -11))
        {
            init_electricity_bomb(PRCCOORD_TO_MAPCOORD(p_mine->X),
              PRCCOORD_TO_MAPCOORD(p_mine->Y), PRCCOORD_TO_MAPCOORD(p_mine->Z),
              (ushort)p_mine->Owner2);
        }
        if (p_mine->Timer1 != -15)
            return;
        remove_sthing(p_mine->ThingOffset);
        ingame.fld_unkCB7--;
        delete_snode(p_mine);
        return;
    }

    if (in_network_game)
        p_mine->U.UWeapon.Ammo++;
}

struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z)
{
#if 0
    struct SimpleThing *ret;
    asm volatile (
      "call ASM_init_nuclear_bomb\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z));
    return ret;
#endif
    struct SimpleThing *p_sthing;
    ThingIdx thing;

    if (x < 0)
        return NULL;
    if (z < 0)
        return NULL;
    if (sthings_used > STHINGS_LIMIT - 5) {
        return NULL;
    }
    thing = get_new_sthing();
    if (thing == 0) {
        return NULL;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return NULL;
    }

    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_NUCLEAR_BOMB;
    p_sthing->Radius = 64;
    p_sthing->X = MAPCOORD_TO_PRCCOORD(x, 0);
    p_sthing->Z = MAPCOORD_TO_PRCCOORD(z, 0);
    p_sthing->Y = MAPCOORD_TO_PRCCOORD(y, 0);
    p_sthing->Timer1 = 0;
    p_sthing->Flag = (TngF_InVehicle|TngF_Persuaded|TngF_Unkn0004);
    add_node_sthing(thing);
    set_nuclear_shade_point(x, y, z);

    return p_sthing;
}

void process_grenade(struct Thing *p_grenade)
{
#if 0
    asm volatile (
      "call ASM_process_grenade\n"
        : : "a" (p_grenade));
#endif
    if (p_grenade->Timer1 == 0)
    {
        MapCoord cor_x, cor_y, cor_z;

        cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
        cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
        cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));

        if (p_grenade->SubType == 3)
        {
            struct SimpleThing *p_bomb;
            p_bomb = init_nuclear_bomb(cor_x, cor_y, cor_z);
            if (p_bomb != NULL) {
                p_bomb->Owner2 = p_grenade->Owner;
                play_dist_ssample(p_bomb, 0x22u, 0x7Fu, 0x40u, 100, 0, 2);
                play_dist_ssample(p_bomb, 1u, 0x7Fu, 0x40u, 100, 0, 3);
            }
        }
        else if ((p_grenade->SubType == 4) || (p_grenade->SubType == 5))
        {
            struct SimpleThing *p_bomb;
            p_bomb = init_nuclear_bomb(cor_x, cor_y, cor_z);
            if (p_bomb != NULL) {
                p_bomb->Type = SmTT_CANISTER;
                p_bomb->SubType = p_grenade->SubType;
                p_bomb->Owner2 = p_grenade->Owner;
                play_dist_ssample(p_bomb, 0x22u, 0x7Fu, 0x40u, 100, 0, 2);
                if (p_bomb->SubType == 4)
                    play_dist_ssample(p_bomb, 0x20u, 0, 0x40u, 100, -1, 3);
                else
                    play_dist_ssample(p_bomb, 0x1Fu, 0, 0x40u, 100, -1, 3);
            }
        }

        remove_thing(p_grenade->ThingOffset);
        delete_node(p_grenade);
        return;
    }

    short qbit;
    int prc_x, prc_y, prc_z;

    prc_x = p_grenade->X + MAPCOORD_TO_PRCCOORD(p_grenade->VX,0);
    prc_y = p_grenade->Y + p_grenade->VY;
    prc_z = p_grenade->Z + MAPCOORD_TO_PRCCOORD(p_grenade->VZ,0);
    p_grenade->VY -= 500;
    qbit = check_col_collision(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_y), PRCCOORD_TO_MAPCOORD(prc_z));
    if (qbit == -1)
    {
        ushort txtr;
        txtr = floor_texture_at_point(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_z));
        if (sea_texture(txtr))
        {
            struct SimpleThing *p_effect;
            MapCoord cor_x, cor_y, cor_z;

            cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
            cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
            cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));
            p_effect = create_scale_effect(cor_x, cor_y, cor_z, 1087, 8);
            if (p_effect != NULL) {
                p_effect->SubType = 60;
                p_effect->Object = 62 + (LbRandomAnyShort() & 0x7F);
                create_sound_effect(cor_x, 0, cor_z, 3, 0, 0);
            }
            remove_thing(p_grenade->ThingOffset);
            delete_node(p_grenade);
            return;
        }
        if (map_floor_is_sludge(PRCCOORD_TO_MAPCOORD(prc_x), PRCCOORD_TO_MAPCOORD(prc_z)))
        {
            struct SimpleThing *p_effect;
            MapCoord cor_x, cor_y, cor_z;

            // Create sludge bulge effect
            cor_x = PRCCOORD_TO_MAPCOORD(p_grenade->X);
            cor_z = PRCCOORD_TO_MAPCOORD(p_grenade->Z);
            cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z));
            p_effect = create_scale_effect(cor_x, cor_y, cor_z, 1091, 8);
            if (p_effect != NULL) {
                p_effect->Object = 62 + (LbRandomAnyShort() & 0x7F);
                // Generic type which just plays animation until end
                p_effect->SubType = 58;
                create_sound_effect(cor_x, cor_y, cor_z, 25, 0, 0);
            }
            remove_thing(p_grenade->ThingOffset);
            delete_node(p_grenade);
            return;
        }
    }
    if (qbit != 0) {
        p_grenade->Timer1 = 0;
    }
    if ((PRCCOORD_TO_MAPCOORD(prc_x) < MAP_COORD_WIDTH) &&
      (PRCCOORD_TO_MAPCOORD(prc_z) < MAP_COORD_HEIGHT)) {
        move_mapwho(p_grenade, prc_x, prc_y, prc_z);
    }
}

void process_laser_elec(struct Thing *p_elec)
{
    asm volatile (
      "call ASM_process_laser_elec\n"
        : : "a" (p_elec));
}

void process_razor_wire(struct Thing *p_thing)
{
    ;
}

void process_air_strike(struct Thing *p_thing)
{
#if 0
    asm volatile (
      "call ASM_process_air_strike\n"
        : : "a" (p_thing));
#endif
    struct SimpleThing *p_sthing;

    p_thing->Timer1--;
    if (p_thing->Timer1 < 0)
    {
        remove_thing(p_thing->ThingOffset);
        return;
    }

    if (p_thing->Timer1 == WEP_AIRSTRIKE_BOMB_GTURNS * WEP_AIRSTRIKE_BOMBS_NUM + WEP_AIRSTRIKE_DELAY_GTURNS)
    {
        play_dist_speech(p_thing, 4, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }

    if ((p_thing->Timer1 < WEP_AIRSTRIKE_BOMB_GTURNS * WEP_AIRSTRIKE_BOMBS_NUM) &&
      (p_thing->Timer1 % WEP_AIRSTRIKE_BOMB_GTURNS) == 0)
    {
        MapCoord cor_x, cor_y, cor_z;

        cor_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
        cor_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
        cor_x += (LbRandomAnyShort() % 3000) - 1500;
        cor_z += (LbRandomAnyShort() % 3000) - 1500;
        cor_y = PRCCOORD_TO_MAPCOORD(alt_at_point(cor_x, cor_z)) + 10;
        bang_new4(MAPCOORD_TO_PRCCOORD(cor_x,0), MAPCOORD_TO_PRCCOORD(cor_y,0),
          MAPCOORD_TO_PRCCOORD(cor_z,0), 50);

        p_sthing = init_nuclear_bomb(cor_x, cor_y, cor_z);
        if (p_sthing != NULL)
        {
            play_dist_ssample(p_sthing, 1, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_sthing->Timer1 = WEP_AIRSTRIKE_IMPACT_GTURNS;
            p_sthing->Owner2 = p_thing->Owner;
            p_sthing->Radius = 0;
            p_sthing->Flag &= ~TngF_Persuaded;
        }
    }
}

void process_unkn35_crashing(struct Thing *p_thing)
{
#if 0
    asm volatile (
      "call ASM_process_unkn35_crashing\n"
        : : "a" (p_thing));
#endif
    ushort rnd;
    int alt_under;

    LOGNO("Thing (%d,%d,%d)", (int)p_thing->X, (int)p_thing->Y, (int)p_thing->Z);
    move_mapwho(p_thing, p_thing->X + p_thing->VX, p_thing->Y + p_thing->VY, p_thing->Z + p_thing->VZ);

    rnd = LbRandomAnyShort();
    p_thing->VX -= p_thing->VX >> 4;
    p_thing->VZ -= p_thing->VZ >> 4;
    p_thing->VY -= ((rnd & 0x7F) + 0x200);

    alt_under = alt_at_point(PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_MAPCOORD(p_thing->Z));
    if (alt_under > p_thing->Y)
    {
        bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 80);
        remove_thing(p_thing->ThingOffset);
        delete_node(p_thing);
    }
}

void process_unkn35(struct Thing *p_thing)
{
    if (p_thing->State == 13) {
        process_unkn35_crashing(p_thing);
        return;
    }
    //TODO implement the rest of the state update
}

static DIFFTEST_REF_FUNC void process_laser_asm(struct Thing *p_laser)
{
    asm volatile (
      "call ASM_process_laser\n"
        : : "a" (p_laser));
}

void process_laser(struct Thing *p_laser)
{
    DIFFTEST_SHADOW(process_laser, process_laser_asm(p_laser));
    p_laser->Timer1--;
    if (p_laser->Timer1 == 0)
    {
        remove_thing((ushort)(p_laser - things));
        delete_node(p_laser);
        return;
    }
    // Beam end point is stored in velocity fields
    init_spark(p_laser->VX, p_laser->VY, p_laser->VZ);
}

struct SimpleThing *create_temp_light(int prc_x, int prc_y, int prc_z,
  short min_bright, short duration, short range_bright)
{
    short thing;
    struct SimpleThing *p_sthing;

    thing = get_new_sthing();
    if (thing == 0)
        return NULL;
    if (thing <= -(STHINGS_LIMIT + 1))
        return NULL;

    p_sthing = &sthings[thing];

    p_sthing->Type = SmTT_TEMP_LIGHT;
    //TODO switch from precise coords to map coords when all callers are remade
    p_sthing->X = prc_x; // MAPCOORD_TO_PRCCOORD(cor_x,127);
    p_sthing->Y = prc_y; // MAPCOORD_TO_PRCCOORD(cor_y,127);
    p_sthing->Z = prc_z; // MAPCOORD_TO_PRCCOORD(cor_z,127);
    p_sthing->Timer1 = duration;
    p_sthing->U.ULight.MinBright = min_bright;
    p_sthing->U.ULight.RangeBright = range_bright;

    return p_sthing;
}

struct SimpleThing *create_growing_e_strand(int x, int y, int z,
  struct Thing *p_thing, int health, int owner)
{
#if 1
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)health;
    stkargs[1] = (int)(intptr_t)owner;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_growing_e_strand\n"
        : "=r" (ret)
        : "a" ((s32)x), "d" ((s32)y), "b" ((s32)z), "c" (p_thing), "S" (stkargs)
        : "cc", "memory");
    return ret;
#endif
}

void create_another_strand(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_create_another_strand\n"
        : : "a" (p_sthing));
}

void process_electric_strand(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_electric_strand\n"
        : : "a" (p_sthing));
}

void process_canister(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_canister\n"
        : : "a" (p_sthing));
}

void process_stasis_pod(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_stasis_pod\n"
        : : "a" (p_sthing));
}

void find_and_stasis(struct SimpleThing *p_stasis, int radius)
{
    asm volatile ("call ASM_find_and_stasis\n"
        :  : "a" (p_stasis), "d" (radius));
}

void find_and_time_travel(struct SimpleThing *p_sthing)
{
#if 0
    asm volatile ("call ASM_find_and_time_travel\n"
        :  : "a" (p_sthing));
#endif
    struct Thing *p_hittng;

    p_hittng = check_for_radius_hit_person(
      p_sthing->X, p_sthing->Y, p_sthing->Z,
      p_sthing->Radius, 0, 1, 0);
    if (p_hittng == NULL) {
        return;
    }

    if (p_hittng->Type == TT_VEHICLE)
    {
        struct Thing *p_person;
        ThingIdx person;

        for (person = p_hittng->U.UVehicle.PassengerHead;
          person != 0; person = p_person->U.UPerson.LinkPassenger)
        {
            p_person = &things[person];
            p_person->Flag2 |= 0x0002;
        }
    }
    if ((p_hittng->Type != SmTT_STATIC) &&
      (p_hittng->Type != TT_BUILDING))
    {
        delete_node(p_hittng);
        p_hittng->Flag2 |= 0x0002;
        p_hittng->U.UPerson.SpecialTimer = 100;
        play_dist_sample(p_hittng, 0x33u, 0x7Fu, 0x40u, 100, 0, 3);
    }
    if (p_hittng->Type == TT_PERSON)
    {
        stop_looped_weapon_sample(p_hittng,
          p_hittng->U.UPerson.CurrentWeapon);
        if ((p_hittng->Flag2 & 0x0001) != 0) {
            finalise_razor_wire(p_hittng);
        }
    }
}

void process_time_pod(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_time_pod\n"
        : : "a" (p_sthing));
}

void process_soul(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_soul\n"
        : : "a" (p_sthing));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int get_some_phwoar_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_get_some_phwoar\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
    return ra;
}

int get_some_phwoar(int a1, int a2, int a3)
{
    ushort ph_idx;

    DIFFTEST_SHADOW_RET(get_some_phwoar, int, get_some_phwoar_asm(a1, a2, a3));
    ph_idx = phwoar_free;
    if (ph_idx == 0)
        DIFFTEST_RETURN((int)0);
    phwoar_free = phwoar[ph_idx].child;
    phwoar_num_used++;
    phwoar_num_free--;
    // The asm only sets the low word of edx; upper half of eax comes from a2
    DIFFTEST_RETURN((int)(((u32)a2 & 0xFFFF0000) | ph_idx));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void give_some_phwoar_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_give_some_phwoar\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
}

void give_some_phwoar(int a1, int a2, int a3)
{
    ushort ph_idx;
    ushort old_free;

    DIFFTEST_SHADOW(give_some_phwoar, give_some_phwoar_asm(a1, a2, a3));
    ph_idx = (ushort)a1;
    old_free = phwoar_free;
    phwoar_free = ph_idx;
    phwoar[ph_idx].child = old_free;
    phwoar_num_used--;
    phwoar_num_free++;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void give_some_shrapnel_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_give_some_shrapnel\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
}

void give_some_shrapnel(int a1, int a2, int a3)
{
    ushort shr = (ushort)a1;

    DIFFTEST_SHADOW(give_some_shrapnel, give_some_shrapnel_asm(a1, a2, a3));
    shrapnel[shr].child = shrapnel_free;
    shrapnel_free = shr;
}




static DIFFTEST_REF_FUNC void process_bang_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_bang\n"
        : : "a" (p_sthing));
}

/** Takes a free phwoar and links it at the head of given list.
 */
static struct Phwoar *bang_phwoar_child_new(ushort *p_head)
{
    struct Phwoar *p_child;
    ushort child;

    child = (ushort)get_some_phwoar(0, 0, 0);
    if (child == 0)
        return NULL;
    p_child = &phwoar[child];
    p_child->child = *p_head;
    *p_head = child;
    return p_child;
}

/** Fills type, position and velocity modifiers of a phwoar spawned by parent.
 */
static void bang_phwoar_child_inherit(struct Phwoar *p_child,
  struct Phwoar *p_parent, struct SimpleThing *p_sthing)
{
    int shift;

    p_child->type = p_parent->type - 1;
    fillin_phwoar_type_info(p_child);
    if ((p_parent->rabbit & 0x80) != 0)
    {
        p_child->x = p_parent->x;
        p_child->y = p_parent->y;
        p_child->z = p_parent->z;
    }
    else
    {
        p_child->x = p_sthing->X;
        p_child->y = p_sthing->Y;
        p_child->z = p_sthing->Z;
    }
    shift = (p_parent->rabbit & 0x0C) >> 2;
    p_child->vx = p_child->vx >> shift;
    shift = (p_parent->rabbit & 0x0C) >> 2;
    p_child->vy = p_child->vy >> shift;
    shift = (p_parent->rabbit & 0x0C) >> 2;
    p_child->vz = p_child->vz >> shift;
    if ((p_parent->rabbit & 0x40) != 0)
    {
        p_child->vx += p_parent->vx;
        p_child->vy += p_parent->vy;
        p_child->vz += p_parent->vz;
    }
}

void process_bang(struct SimpleThing *p_sthing)
{
    ushort *p_head;
    ushort phw;
    ushort *p_shead;
    ushort shr;
    ushort state;

    DIFFTEST_SHADOW(process_bang, process_bang_asm(p_sthing));

    state = p_sthing->State;
    if (state >= 100)
    {
        state++;
        p_sthing->State = state;
        if (state >= 105)
            p_sthing->State = 0;
    }
    else if (state != 0)
    {
        p_sthing->State = state - 1;
    }

    p_head = (ushort *)&p_sthing->U.UBang.phwoar;
    phw = *p_head;
    while (phw != 0)
    {
        struct Phwoar *p_phwoar;
        struct Phwoar *p_child;
        ushort child;
        s32 i, n, rings, base, dangley, dang, angle, angle_y;
        int ay;

        p_phwoar = &phwoar[phw];
        if ((p_phwoar->gestation & 0x3F) == 0)
        {
            switch ((p_phwoar->rabbit & 0x30) >> 4)
            {
            case 0:
                n = p_phwoar->rabbit & 0x03;
                dang = bangdetail.circle[n].dangle;
                n = bangdetail.circle[n].babynumber;
                angle = 0;
                for (i = 0; i < n; i++)
                {
                    int a;

                    p_child = bang_phwoar_child_new(p_head);
                    if (p_child == NULL)
                        continue;
                    a = angle & 0x7FF;
                    p_child->vx = lbSinTable[a] >> 10;
                    p_child->vy = 0;
                    p_child->vz = lbSinTable[a + 512] >> 10;
                    p_child->vx += (LbRandomAnyShort() & 7) - 3;
                    p_child->vz += (LbRandomAnyShort() & 7) - 3;
                    angle += dang;
                    bang_phwoar_child_inherit(p_child, p_phwoar, p_sthing);
                }
                break;
            case 1:
                n = p_phwoar->rabbit & 0x03;
                base = bangdetail.sphere[n].babynumber;
                rings = bangdetail.sphere[n].babyshift;
                dangley = bangdetail.sphere[n].dangley;
                for (angle_y = 0; rings != 0; rings--, angle_y += dangley)
                {
                    ay = angle_y & 0x7FF;
                    n = ((base * lbSinTable[ay + 512]) >> 16) + 1;
                    dang = 0x800 / n;
                    angle = 0;
                    for (i = 0; i < n; i++, angle += dang)
                    {
                        int a;

                        p_child = bang_phwoar_child_new(p_head);
                        if (p_child != NULL)
                        {
                            a = angle & 0x7FF;
                            p_child->vx = ((lbSinTable[a] >> 10) * lbSinTable[ay + 512]) >> 16;
                            p_child->vz = ((lbSinTable[a + 512] >> 10) * lbSinTable[ay + 512]) >> 16;
                            p_child->vy = lbSinTable[ay] >> 10;
                            p_child->vx += (LbRandomAnyShort() & 0xF) - 7;
                            p_child->vz += (LbRandomAnyShort() & 0xF) - 7;
                            bang_phwoar_child_inherit(p_child, p_phwoar, p_sthing);
                        }
                        if (angle_y == 0)
                            continue;
                        p_child = bang_phwoar_child_new(p_head);
                        if (p_child == NULL)
                            continue;
                        a = angle & 0x7FF;
                        p_child->vx = ((lbSinTable[a] >> 10) * lbSinTable[ay + 512]) >> 16;
                        p_child->vz = ((lbSinTable[a + 512] >> 10) * lbSinTable[ay + 512]) >> 16;
                        p_child->vy = (-lbSinTable[ay]) >> 10;
                        p_child->vx += (LbRandomAnyShort() & 0xF) - 7;
                        p_child->vz += (LbRandomAnyShort() & 0xF) - 7;
                        bang_phwoar_child_inherit(p_child, p_phwoar, p_sthing);
                    }
                }
                break;
            case 2:
                n = p_phwoar->rabbit & 0x03;
                base = bangdetail.sphere[n].babynumber;
                rings = bangdetail.sphere[n].babyshift;
                dangley = bangdetail.sphere[n].dangley;
                for (angle_y = 0; rings != 0; rings--, angle_y += dangley)
                {
                    ay = angle_y & 0x7FF;
                    n = ((base * lbSinTable[ay + 512]) >> 16) + 1;
                    dang = 0x800 / n;
                    angle = 0;
                    for (i = 0; i < n; i++)
                    {
                        int a;

                        p_child = bang_phwoar_child_new(p_head);
                        if (p_child == NULL)
                            continue;
                        a = angle & 0x7FF;
                        p_child->vx = ((lbSinTable[a] >> 10) * lbSinTable[ay + 512]) >> 16;
                        p_child->vz = ((lbSinTable[a + 512] >> 10) * lbSinTable[ay + 512]) >> 16;
                        p_child->vy = lbSinTable[ay] >> 10;
                        p_child->vx += (LbRandomAnyShort() & 0xF) - 7;
                        p_child->vz += (LbRandomAnyShort() & 0xF) - 7;
                        angle += dang;
                        bang_phwoar_child_inherit(p_child, p_phwoar, p_sthing);
                    }
                }
                break;
            case 3:
                n = (p_phwoar->rabbit & 0x03) * 2 + 1;
                for (i = 0; i < n; i++)
                {
                    int a;

                    p_child = bang_phwoar_child_new(p_head);
                    if (p_child == NULL)
                        continue;
                    a = LbRandomAnyShort() & 0x7FF;
                    ay = LbRandomAnyShort() & 0x3F;
                    p_child->vx = ((lbSinTable[a] >> 10) * lbSinTable[ay + 512]) >> 16;
                    p_child->vz = ((lbSinTable[a + 512] >> 10) * lbSinTable[ay + 512]) >> 16;
                    p_child->vy = lbSinTable[ay] >> 10;
                    bang_phwoar_child_inherit(p_child, p_phwoar, p_sthing);
                }
                break;
            }
            p_phwoar->gestation = 0xFF;
        }

        child = p_phwoar->child;
        if (p_phwoar->die <= 0)
        {
            *p_head = child;
            give_some_phwoar(phw, 0, 0);
            phw = child;
            continue;
        }
        p_phwoar->gestation--;
        p_phwoar->fc += p_phwoar->vf & 0x0F;
        while (p_phwoar->fc >= 8)
        {
            p_phwoar->die--;
            p_phwoar->fc -= 8;
            if (p_phwoar->die > 0)
                p_phwoar->f = frame[p_phwoar->f].Next;
        }
        p_phwoar->x += p_phwoar->vx * 1024;
        p_phwoar->y += p_phwoar->vy * 128;
        p_phwoar->z += p_phwoar->vz * 1024;
        if ((p_phwoar->vf & 0x80) != 0)
            p_phwoar->vy -= 3;
        if ((p_phwoar->vf & 0x40) != 0)
        {
            p_phwoar->vx = p_phwoar->vx - (p_phwoar->vx >> 1);
            p_phwoar->vy = p_phwoar->vy - (p_phwoar->vy >> 1);
            p_phwoar->vz = p_phwoar->vz - (p_phwoar->vz >> 1);
        }
        if ((p_phwoar->vf & 0x20) != 0)
        {
            p_phwoar->vx = p_phwoar->vx - (p_phwoar->vx >> 2);
            p_phwoar->vy = p_phwoar->vy - (p_phwoar->vy >> 2);
            p_phwoar->vz = p_phwoar->vz - (p_phwoar->vz >> 2);
        }
        if ((p_phwoar->vf & 0x10) != 0)
        {
            p_phwoar->vx = p_phwoar->vx - (p_phwoar->vx >> 3);
            p_phwoar->vy = p_phwoar->vy - (p_phwoar->vy >> 3);
            p_phwoar->vz = p_phwoar->vz - (p_phwoar->vz >> 3);
        }
        phw = child;
        p_head = &p_phwoar->child;
    }

    p_shead = (ushort *)&p_sthing->U.UBang.shrapnel;
    shr = *p_shead;
    while ((shr != 0) && (shr < SHRAPNEL_COUNT))
    {
        struct Shrapnel *p_shrapnel;
        ushort next;
        int alt, tile_x, tile_z;
        ushort sample;

        p_shrapnel = &shrapnel[shr];
        next = p_shrapnel->child;
        p_shrapnel->x += p_shrapnel->vx * 256;
        p_shrapnel->y += p_shrapnel->vy * 32;
        p_shrapnel->vy -= 3;
        p_shrapnel->z += p_shrapnel->vz * 256;
        p_shrapnel->yaw += p_shrapnel->vyaw;
        p_shrapnel->pitch += p_shrapnel->vpitch;

        if (p_shrapnel->type == 2)
        {
            if ((gameturn & bangdetail.mask) == 0)
                bang_new4(p_shrapnel->x, p_shrapnel->y, p_shrapnel->z, 40);
        }
        else if (p_shrapnel->type == 4)
        {
            bang_new4(p_shrapnel->x, p_shrapnel->y, p_shrapnel->z, 40);
            p_shrapnel->y += p_shrapnel->vy * 32;
            p_shrapnel->vy = p_shrapnel->vy - (p_shrapnel->vy >> 2);
        }
        else if (p_shrapnel->type == 3)
        {
            if ((gameturn & bangdetail.mask) == 0)
                bang_new4(p_shrapnel->x, p_shrapnel->y, p_shrapnel->z, 30);
        }

        alt = alt_at_point(p_shrapnel->x >> 8, p_shrapnel->z >> 8);
        tile_x = p_shrapnel->x >> 16;
        tile_z = p_shrapnel->z >> 16;
        if ((tile_x < 0) || (tile_x >= 128) || (tile_z < 0) || (tile_z >= 128))
        {
            *p_shead = p_shrapnel->child;
            give_some_shrapnel(shr, 0, 0);
            shr = next;
            continue;
        }

        if ((alt > p_shrapnel->y) || ((p_shrapnel->type != 4) &&
          (check_col_collision(p_shrapnel->x >> 8, p_shrapnel->y >> 8,
            p_shrapnel->z >> 8) != 0)))
        {
            if ((alt > p_shrapnel->y) &&
              sea_texture((short)(game_my_big_map[(tile_z << 7) + tile_x].Texture & 0x3FFF)))
            {
                struct SimpleThing *p_effect;

                sample = 3;
                p_effect = create_scale_effect(p_shrapnel->x >> 8, alt >> 8,
                  p_shrapnel->z >> 8, 0x43F, 8);
                if (p_effect != NULL)
                {
                    ushort rnd;

                    rnd = LbRandomAnyShort();
                    p_effect->SubType = 60;
                    p_effect->Object = (rnd & 0x7F) + 62;
                }
            }
            else
            {
                bang_new4(p_shrapnel->x, p_shrapnel->y, p_shrapnel->z, 35);
                if (p_shrapnel->type == 2)
                    do_shockwave(p_shrapnel->x >> 8, 0, p_shrapnel->z >> 8,
                      512, 800, NULL);
                switch (gameturn % 3)
                {
                case 0:
                    sample = 60;
                    break;
                case 1:
                    sample = 62;
                    break;
                default:
                    sample = 63;
                    break;
                }
            }
            create_sound_effect(p_shrapnel->x >> 8, 0, p_shrapnel->z >> 8,
              sample, 0, 0);
            *p_shead = p_shrapnel->child;
            give_some_shrapnel(shr, 0, 0);
            shr = next;
            continue;
        }

        if ((p_shrapnel->type == 4) && (p_shrapnel->vy <= 20))
        {
            *p_shead = p_shrapnel->child;
            give_some_shrapnel(shr, 0, 0);
            bang_new4(p_shrapnel->x, p_shrapnel->y, p_shrapnel->z, 20);
            shr = next;
            continue;
        }
        shr = next;
        p_shead = &p_shrapnel->child;
    }
}

void finish_bang(struct SimpleThing *p_sthing, ThingIdx thing)
{
    remove_sthing(thing);
    delete_snode(p_sthing);
}

void FIRE_process_flame(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_FIRE_process_flame\n"
        : : "a" (p_sthing));
}

void FIRE_finish_flame(struct SimpleThing *p_sthing, ThingIdx thing)
{
    ReleaseLoopedSample(thing, 16);
    play_dist_ssample(p_sthing, 0x17, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    remove_sthing(thing);
    delete_snode(p_sthing);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int move_smapwho(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_move_smapwho\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


void process_spark(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_spark\n"
        : : "a" (p_sthing));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void apply_texture_to_face_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_apply_texture_to_face\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
}

void apply_texture_to_face(int a1, int a2, int a3, int a4)
{
    ushort frame = (ushort)a1;
    ushort anim = (ushort)a2;
    ushort face = (ushort)a3;
    struct SingleObjectFace4 *p_face4;
    struct SingleFloorTexture *p_fltextr;
    struct AnimPage *p_anpg;
    ubyte x, y, x2, y2;

    DIFFTEST_SHADOW(apply_texture_to_face, apply_texture_to_face_asm(a1, a2, a3, a4));
    p_face4 = &game_object_faces4[face];
    p_fltextr = &game_textures[p_face4->Texture];
    p_anpg = &door_anim_page[anim];

    x = p_anpg->X + frame * p_anpg->Width;
    y = p_anpg->Y;
    p_fltextr->Page = p_anpg->PageNo;
    p_fltextr->TMapX2 = x;
    p_fltextr->TMapY2 = y;
    p_fltextr->TMapY4 = y;
    x2 = p_anpg->Width + x;
    p_fltextr->TMapX4 = x2;
    p_fltextr->TMapX3 = x2;
    p_fltextr->TMapX1 = x;
    y2 = p_anpg->Height + y;
    p_fltextr->TMapY3 = y2;
    p_fltextr->TMapY1 = y2;
}


static DIFFTEST_REF_FUNC void process_intelligent_door_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_intelligent_door\n"
        : : "a" (p_sthing));
}

void process_intelligent_door(struct SimpleThing *p_sthing)
{
    ushort frame;
    ushort anim;

    DIFFTEST_SHADOW(process_intelligent_door, process_intelligent_door_asm(p_sthing));
    if ((p_sthing->Flag & 0x01) != 0)
    {
        if (p_sthing->State != 1 && p_sthing->State != 3)
        {
            p_sthing->State = 1;
            p_sthing->Flag &= ~0x01;
        }
    }

    switch ((ushort)p_sthing->State)
    {
    case 1:
        p_sthing->Flag &= ~0x01;
        anim = p_sthing->StartFrame;
        frame = p_sthing->Frame + 1;
        p_sthing->Frame = frame;
        if ((s32)frame < (s32)door_anim_page[anim].No)
        {
            apply_texture_to_face(frame, anim, (ushort)p_sthing->Parent, frame);
        }
        else
        {
            p_sthing->State = 3;
            p_sthing->Timer1 = 7;
            p_sthing->Frame = frame - 1;
            p_sthing->Flag |= 0x02;
        }
        break;
    case 2:
        anim = p_sthing->StartFrame;
        frame = p_sthing->Frame - 1;
        p_sthing->Frame = frame;
        if ((s32)frame <= (s32)door_anim_page[anim].No)
        {
            apply_texture_to_face(frame, anim, (ushort)p_sthing->Parent, 0);
        }
        else
        {
            game_object_faces4[(short)p_sthing->Parent].WalkHeader = 0;
            remove_sthing(p_sthing->ThingOffset);
        }
        break;
    case 3:
        p_sthing->Flag &= ~0x01;
        if ((short)p_sthing->Timer1 != 0)
        {
            p_sthing->Timer1--;
        }
        else if (check_for_people(p_sthing->X, p_sthing->Y, p_sthing->Z, 100) == NULL)
        {
            p_sthing->State = 2;
            play_dist_ssample(p_sthing, 0x35, 0x7F, 0x40, 100, 0, 1);
        }
        break;
    default:
        break;
    }
}

void process_unkn58(struct SimpleThing *p_sthing)
{
    ushort frm;

    frm = frame[p_sthing->Frame].Next;

    if ((frame[frm].Flags & 0x01) != 0) {
        remove_sthing(p_sthing->ThingOffset);
        delete_snode(p_sthing);
        return;
    }

    p_sthing->Frame = frm;
}

ubyte napalm_person(struct Thing *p_person, ushort multi)
{
    ubyte ret;
    asm volatile ("call ASM_napalm_person\n"
        : "=r" (ret) : "a" (p_person), "d" (multi));
    return ret;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void person_go_insane(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_go_insane\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
}


void process_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_mushroom(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_mushroom\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_special_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_special_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_napalm_flame(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_napalm_flame\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_flame1(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_flame1\n"
        : : "a" (p_sthing));
}

void process_splash(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_splash\n"
        : : "a" (p_sthing));
}

void process_scale_effect(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
#if 0
    asm volatile (
      "call ASM_process_scale_effect\n"
        : : "a" (p_sthing), "d" (thing_item));
#endif
    switch (p_sthing->SubType)
    {
    case 30:
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 31:
        process_mushroom(p_sthing, thing_item);
        break;
    case 44:
        process_special_drift_smoke(p_sthing, thing_item);
        break;
    case 45:
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 46:
        process_napalm_flame(p_sthing, thing_item);
        break;
    case 49:
        if (p_sthing->Timer1 == 4)
            p_sthing->Frame = nstart_ani[1104];
        process_drift_smoke(p_sthing, thing_item);
        break;
    case 52:
        process_flame1(p_sthing);
        break;
    case 56:
    case 57:
        process_flame1(p_sthing);
        break;
    case 58:
        process_unkn58(p_sthing);
        break;
    case 60:
        process_splash(p_sthing);
        break;
    }
}

void test_stuff(int x, int y, int z, ushort owner)
{
    asm volatile (
      "call ASM_test_stuff\n"
        : : "a" (x), "d" (y), "b" (z), "c" ((int)owner));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void do_explode(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_do_explode\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


static DIFFTEST_REF_FUNC void process_nuclear_bomb_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    asm volatile (
      "call ASM_process_nuclear_bomb\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_nuclear_bomb(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    s32 x, y, z;
    s32 timer;
    s32 turn;
    short timer1;

    DIFFTEST_SHADOW(process_nuclear_bomb, process_nuclear_bomb_asm(p_sthing, thing_item));

    turn = gameturn;
    x = p_sthing->X >> 8;
    y = p_sthing->Y >> 8;
    z = p_sthing->Z >> 8;
    timer = p_sthing->Timer1;
    set_nuclear_shade_timer(timer);

    if (timer < 12)
    {
        s32 radius = (ushort)p_sthing->Radius;
        p_sthing->Radius = radius + ((radius >> 3) + 4);
    }
    else if (timer < 25)
    {
        s32 radius = (ushort)p_sthing->Radius;
        p_sthing->Radius = radius - ((radius >> 2) + 4);
    }
    else
    {
        p_sthing->Radius = 0;
    }

    timer = p_sthing->Timer1;
    if (timer == 20)
    {
        struct Thing *p_owner;
        ushort angle;
        int intensity;

        do_explode((short)x, (short)y, (short)z, 0);
        quick_crater(x >> 8, z >> 8, 2);
        p_owner = &things[(ushort)p_sthing->Owner2];
        if ((p_sthing->Flag & TngF_Persuaded) != 0)
            intensity = 10000;
        else
            intensity = 5000;
        do_shockwave(x, y, z, 0x400, intensity, p_owner);
        stop_sample_using_heap(p_sthing->ThingOffset, 1);
        play_dist_sample(&things[(ushort)p_sthing->Owner2], 0x16, 0x7F, 0x40, 100, 0, 3);
        bang_new4(x << 8, y << 8, z << 8, 45);

        for (angle = 0; angle < 0x800; angle += 20, turn++)
        {
            struct SimpleThing *p_effect;
            s32 cx, cz;
            s32 m;

            cx = lbSinTable[angle + 512] >> 8;
            cz = lbSinTable[angle] >> 8;
            cx += (LbRandomAnyShort() & 0x1F) - 15;
            cz += (LbRandomAnyShort() & 0x1F) - 15;
            p_effect = create_scale_effect(x + cx, y, z + cz, 900, 20);
            if (p_effect == NULL)
                continue;
            m = turn & 1;
            p_effect->SubType = 45;
            p_effect->Object = 0x100;
            p_effect->StartFrame = 0;
            p_effect->U.UEffect.VY = 0;
            p_effect->U.UEffect.OY = 0;
            p_effect->U.UEffect.VX = (cx << 4) - m * cx;
            p_effect->U.UEffect.VZ = (cz << 4) - m * cz;
            p_effect->U.UEffect.OZ = (-cz) >> 4;
            p_effect->U.UEffect.OX = (-cx) >> 4;
            p_effect->Owner2 = p_sthing->Owner2;
        }
    }
    else if (timer > 20)
    {
        if ((p_sthing->Flag & TngF_Persuaded) != 0)
            test_stuff(x, y, z, 0);
    }

    timer1 = p_sthing->Timer1;
    p_sthing->Timer1 = timer1 + 1;
    if (timer1 > 28)
    {
        remove_sthing(thing_item);
        delete_snode(p_sthing);
        // TODO the asm stores a 32-bit zero over a 1-byte TbBool, clobbering
        // the bytes which follow it; kept to match the original
        *(u32 *)&nuclear_overexposure = 0;
    }
}

static DIFFTEST_REF_FUNC void process_smoke_generator_asm(struct SimpleThing *p_sthing)
{
    asm volatile (
      "call ASM_process_smoke_generator\n"
        : : "a" (p_sthing));
}

void process_smoke_generator(struct SimpleThing *p_sthing)
{
    struct SimpleThing *p_smoke;
    s32 x, y, z;
    s32 tm_range;
    short timer;
    short scale;
    u32 angle;

    DIFFTEST_SHADOW(process_smoke_generator, process_smoke_generator_asm(p_sthing));
    x = p_sthing->X >> 8;
    y = p_sthing->Y >> 8;
    z = p_sthing->Z >> 8;
    timer = p_sthing->Timer1;
    p_sthing->Timer1 = timer - 1;
    if (timer >= 0)
        return;

    tm_range = p_sthing->U.UEffect.OZ;
    p_sthing->Timer1 = (s32)LbRandomAnyShort() % tm_range + tm_range;
    scale = (LbRandomAnyShort() & 0xF) + 0x50;
    p_smoke = create_scale_effect(x, y, z, 0x38b, scale);
    if (p_smoke == NULL)
        return;

    p_smoke->SubType = 44;
    p_smoke->Object = 0x80;
    p_smoke->StartFrame = 2;
    p_smoke->U.UEffect.VX = p_sthing->U.UEffect.VX;
    p_smoke->U.UEffect.VY = p_sthing->U.UEffect.VY;
    p_smoke->U.UEffect.VZ = p_sthing->U.UEffect.VZ;
    angle = (gameturn << 7) & 0x7FF;
    p_smoke->U.UEffect.VX = p_smoke->U.UEffect.VX
      + ((lbSinTable[angle + LbFPMath_PI/2] >> 10) - 32);
    p_smoke->U.UEffect.VY = p_smoke->U.UEffect.VY
      + ((lbSinTable[angle] >> 10) - 32);
    p_smoke->U.UEffect.OX = p_sthing->U.UEffect.OX;
    p_smoke->U.UEffect.OY = p_sthing->U.UEffect.OY;
    p_smoke->U.UEffect.OZ = p_sthing->U.UEffect.OZ;
    p_smoke->StartTimer1 = p_sthing->StartTimer1;
    p_smoke->State = 0;
    p_smoke->Owner2 = p_sthing->Owner2;
}

void process_thing_checksum(struct Thing *p_thing, ThingIdx thing)
{
    ingame.fld_unkC4B += thing;
    ingame.fld_unkC4B += p_thing->Y + p_thing->X + p_thing->Type + p_thing->State;
}

TbBool process_thing_unkflag02000000(struct Thing *p_thing, ThingIdx thing)
{
    if (((p_thing->Flag2 & TgF2_Unkn02000000) != 0) && ((gameturn + thing) & 3) != 0) {
        return true;
    }
    p_thing->Flag2 &= ~TgF2_Unkn02000000;
    return false;
}

TbBool process_thing_unkflag0002(struct Thing *p_thing)
{
    if ((p_thing->Flag2 & TgF2_Unkn0002) != 0)
    {
        if ((gameturn & 3) != 0)
            return true;
        p_thing->U.UPerson.SpecialTimer--;
        if (p_thing->U.UPerson.SpecialTimer >= 0)
            return true;
        if ((p_thing->Flag & TngF_InVehicle) == 0)
            add_node_thing(p_thing->ThingOffset);
        p_thing->Flag2 &= ~TgF2_Unkn0002;
        return true;
    }
    return false;
}

void process_thing(struct Thing *p_thing, ThingIdx thing)
{
    switch (p_thing->Type)
    {
    case TT_VEHICLE:
        process_vehicle(p_thing);
        break;
    case TT_PERSON:
    case TT_UNKN4:
        process_shield(p_thing);
        process_person(p_thing);
        if ((p_thing->Flag & TngF_TriggerUse) != 0)
            p_thing->Flag2 |= TgF2_Unkn0400;
        else
            p_thing->Flag2 &= ~TgF2_Unkn0400;
        break;
    case TT_ROCKET:
        process_rocket(p_thing);
        break;
    case TT_BUILDING:
        process_building(p_thing);
        break;
    case TT_LASER_GUIDED:
        process_laser_guided(p_thing);
        break;
    case TT_MINE:
        process_mine((struct SimpleThing *)p_thing);
        break;
    case TT_GRENADE:
        process_grenade(p_thing);
        break;
    case TT_LASER_ELEC:
        process_laser_elec(p_thing);
        break;
    case TT_RAZOR_WIRE: // exploding wire it a subtype of razor wire
        process_razor_wire(p_thing);
        break;
    case TT_AIR_STRIKE:
        process_air_strike(p_thing);
        break;
    case TT_UNKN35:
        process_unkn35(p_thing);
        break;
    case TT_LASER11:
    case TT_LASER29:
    case TT_LASER38: // on PSX, laser type=38 has additional light effect
        process_laser(p_thing);
        break;
    default:
        break;
    }
}

void process_sthing_checksum(struct SimpleThing *p_sthing, ThingIdx thing)
{
    ingame.fld_unkC4B += p_sthing->Y + p_sthing->X + p_sthing->Type + p_sthing->State;
}

void process_carried_item(struct SimpleThing *p_item)
{
    ThingIdx item;
    struct Thing *p_owner;

    p_owner = &things[p_item->U.UWeapon.Owner];
    p_item->X = p_owner->X;
    p_item->Y = p_owner->Y;
    p_item->Z = p_owner->Z;
    if (p_owner->State == PerSt_DEAD) {
        item = p_item->ThingOffset;
        p_item->Type = SmTT_DROPPED_ITEM;
        add_node_sthing(item);
    }
}

void process_temp_light(struct SimpleThing *p_sthing)
{
    int brig, rng;
    short lgtng_x, lgtng_y, lgtng_z;

    p_sthing->Timer1--;
    if (p_sthing->Timer1 < 0) {
        remove_sthing(p_sthing->ThingOffset);
        return;
    }
    brig = p_sthing->U.ULight.MinBright;
    if (p_sthing->U.ULight.RangeBright != 0) {
        rng = p_sthing->U.ULight.RangeBright;
        brig += (ushort)LbRandomAnyShort() % rng;
    }
    get_thing_position_mapcoords(&lgtng_x, &lgtng_y, &lgtng_z, p_sthing->ThingOffset);
    apply_full_light(lgtng_x, lgtng_y >> 3, lgtng_z, brig, 0);
}

void set_thing_frame(struct Thing *p_thing, ushort anim_start)
{
    assert(p_thing->Type != TT_PERSON);
    p_thing->StartFrame = anim_start;
    p_thing->Frame = nstart_ani[p_thing->StartFrame];
}

void reset_thing_frame(struct Thing *p_thing)
{
    assert(p_thing->Type != TT_PERSON);
    p_thing->Frame = nstart_ani[p_thing->StartFrame];
}

void reset_sthing_frame(struct SimpleThing *p_sthing)
{
    p_sthing->Frame = nstart_ani[p_sthing->StartFrame];
}

void process_burning_static(struct SimpleThing *p_sthing)
{
    p_sthing->Timer1--;
    if (p_sthing->Timer1 != 0)
        return;
    p_sthing->State = 13;
    reset_sthing_frame(p_sthing);
    stop_sample_using_heap(p_sthing->ThingOffset, 29);
}

void process_static(struct SimpleThing *p_sthing)
{
    switch (p_sthing->State)
    {
    case 36:
        play_dist_ssample(p_sthing, 29, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
        process_burning_static(p_sthing);
        break;
    case 12:
        process_burning_static(p_sthing);
        break;
    }
}

void process_sfx(struct SimpleThing *p_sthing)
{
    switch (p_sthing->State)
    {
    case 1:
        if (!dont_bother_with_explode_faces)
            break;
        stop_sample_using_heap(p_sthing->ThingOffset, 0x2E);
        play_dist_ssample(p_sthing, 0x2F, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        p_sthing->State = 2;
        p_sthing->Timer1 = 100;
        break;
    default:
        p_sthing->Timer1--;
        if (p_sthing->Timer1 >= 0)
            break;
        remove_sthing(p_sthing->ThingOffset);
        if (p_sthing->State != 2)
            break;
        ingame.SoundThing = 0;
        break;
    }
}

void process_sthing(struct SimpleThing *p_sthing, ThingIdx thing)
{
    switch (p_sthing->Type)
    {
    case SmTT_CARRIED_ITEM:
        process_carried_item(p_sthing);
        break;
    case SmTT_ELECTRIC_STRAND:
        process_electric_strand(p_sthing);
        break;
    case SmTT_CANISTER:
        process_canister(p_sthing);
        break;
    case SmTT_STASIS_POD:
        process_stasis_pod(p_sthing);
        break;
    case SmTT_TIME_POD:
        process_time_pod(p_sthing);
        break;
    case SmTT_SOUL:
        process_soul(p_sthing);
        break;
    case SmTT_BANG:
        if ((p_sthing->U.UEffect.VY != 0) || (p_sthing->U.UEffect.VX != 0)) {
            process_bang(p_sthing);
        } else {
            finish_bang(p_sthing, thing);
        }
        break;
    case SmTT_FIRE:
        if (p_sthing->U.UFire.flame != 0) {
            FIRE_process_flame(p_sthing);
        } else {
            FIRE_finish_flame(p_sthing, thing);
        }
        break;
    case SmTT_SFX:
        process_sfx(p_sthing);
        break;
    case SmTT_TEMP_LIGHT:
        process_temp_light(p_sthing);
        break;
    case SmTT_SPARK:
        process_spark(p_sthing);
        break;
    case SmTT_STATIC:
        process_static(p_sthing);
        break;
    case SmTT_INTELLIG_DOOR:
        process_intelligent_door(p_sthing);
        break;
    case SmTT_SCALE_EFFECT:
        process_scale_effect(p_sthing, thing);
        break;
    case SmTT_NUCLEAR_BOMB:
        process_nuclear_bomb(p_sthing, thing);
        break;
    case SmTT_SMOKE_GENERATOR:
        process_smoke_generator(p_sthing);
        break;
    case SmTT_DROPPED_ITEM:
        if (p_sthing->SubType == WEP_EXPLMINE || p_sthing->SubType == WEP_ELEMINE) {
            process_mine(p_sthing);
        } else {
            if (!in_network_game || p_sthing->U.UWeapon.WeaponType == 31)
                break;
            p_sthing->U.UWeapon.Ammo++;
        }
        break;
    default:
        break;
    }
}

void process_things(void)
{
    int i;
    ushort plyr;

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        PlayerInfo *p_player;
        struct Thing *p_dcthing;
        ThingIdx dcthing;

        p_player = &players[plyr];
        dcthing = p_player->DirectControl[0];
        p_dcthing = &things[dcthing];
        if (((1 << plyr) & ingame.InNetGame_UNSURE) != 0
          && (p_dcthing->Flag & TngF_SelectedAgent) == 0)
        {
#if 0
            for (i = 0; i < playable_agents; i++)
              ;
#endif
            p_dcthing->Flag |= TngF_SelectedAgent;
        }
    }

    if (in_network_game)
    {
        if (ingame.fld_unkCB7 > 150)
            process_things_mines_explode((rand() & 0x1F) + 2);
        if ((net_game_play_flags & NGPF_Unkn10) != 0 && (gameturn & 0xF) == 0)
            process_things_unkn_sub2(login_control[0].TechLevel, ingame.fld_unkCB7 > 100);
        if (things_used > 900)
            process_things_bang(16);
        else if ( things_used > 700 || ((gameturn & 0xF) == 0))
            process_things_bang(1);
    }

    if ((gameturn & 0x1F) == 0)
    {
        for (i = 0; i < PEOPLE_GROUPS_LIMIT+1; i++) {
            group_actions[i].Storming &= ~GrpAF_UnderAttack;
        }
    }

    shield_frames_cycle();

    player_update_thermal(local_player_no);

#if 0
    merged_noop_unkn1(gameturn);
#endif
    build_same_type_headers();
    ingame.fld_unkC4B = 0;
    unkn_update_lights();
    unkn_full_update_lights();

    if (!execute_commands)
        return;
    if ((ingame.Flags & TngF_ProgressAction) == 0)
        return;
    monitor_all_samples();

    if (!in_network_game && (pktrec_mode == PktR_NONE)
      && ((ingame.Flags & GamF_Unkn0004) != 0) && ((gameturn & 0xF) != 0))
        return;

    if (execute_commands)
    {
        struct Thing *p_thing;
        int remain;
        ThingIdx thing, nxthing;

        remain = things_used;
        for (thing = things_used_head; thing > 0; thing = nxthing)
        {
            if (--remain == -1) {
                break;
            }
            p_thing = &things[thing];
            nxthing = p_thing->LinkChild;

            process_thing_checksum(p_thing, thing);

            if (process_thing_unkflag02000000(p_thing, thing))
                continue;

            if (process_thing_unkflag0002(p_thing))
                continue;

            process_thing(p_thing, thing);
        }
    }

    if (execute_commands)
    {
        struct SimpleThing *p_sthing;
        int remain;
        ThingIdx thing, nxthing;

        remain = sthings_used;
        for (thing = sthings_used_head; thing < 0; thing = nxthing)
        {
            if (--remain == -1) {
                break;
            }
            p_sthing = &sthings[thing];
            nxthing = p_sthing->LinkChild;

            process_sthing_checksum(p_sthing, thing);

            process_sthing(p_sthing, thing);
        }
    }

    if (execute_commands)
    {
        people_intel(0);
    }
    navi_onscreen_debug(1);
}

const char *thing_type_name(ubyte tngtype, ubyte subtype)
{
    if (tngtype >= sizeof(thing_type_names)/sizeof(thing_type_names[1]))
        return "OUTRANGED";
    if (tngtype == TT_PERSON)
        return person_type_name(subtype);
    if (tngtype == TT_VEHICLE)
        return vehicle_type_name(subtype);
    if (tngtype == TT_BUILDING)
        return building_type_name(subtype);
    // TODO support some sub-types
    return thing_type_names[tngtype];
}

void snprint_thing(char *buf, ulong buflen, struct Thing *p_thing)
{
    char *s;
    ubyte nparams;

    s = buf;

    sprintf(s, "T%02d = %s( ", (int)p_thing->ThingOffset, thing_type_name(p_thing->Type, p_thing->SubType));
    s += strlen(s);
    nparams = 0;

    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Coord(%d,%d,%d)", (int)PRCCOORD_TO_MAPCOORD(p_thing->X),
          (int)PRCCOORD_TO_MAPCOORD(p_thing->Y), (int)PRCCOORD_TO_MAPCOORD(p_thing->Z));
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Health != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Health(%hd)", p_thing->Health);
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Flag != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Flag(0x%lx)", p_thing->Flag);
        s += strlen(s);
        nparams++;
    }
    if (p_thing->Type == TT_PERSON) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        snprint_person_state(s, buflen - (s-buf), p_thing);
        s += strlen(s);
        nparams++;
    }
    // TODO support more properties

    snprintf(s, buflen - (s-buf), " )");
}

void snprint_sthing(char *buf, ulong buflen, struct SimpleThing *p_sthing)
{
    char *s;
    ubyte nparams;

    s = buf;

    sprintf(s, "T%02d = %s( ", (int)p_sthing->ThingOffset, thing_type_name(p_sthing->Type, p_sthing->SubType));
    s += strlen(s);
    nparams = 0;

    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Coord(%d,%d,%d)", (int)PRCCOORD_TO_MAPCOORD(p_sthing->X),
          (int)PRCCOORD_TO_MAPCOORD(p_sthing->Y), (int)PRCCOORD_TO_MAPCOORD(p_sthing->Z));
        s += strlen(s);
        nparams++;
    }
    if (p_sthing->Flag != 0) {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Flag(0x%lx)", p_sthing->Flag);
        s += strlen(s);
        nparams++;
    }
    // TODO support more properties

    snprintf(s, buflen - (s-buf), " )");
}

TbBool thing_is_destroyed(ThingIdx thing)
{
    if (thing == 0) {
        return true;
    } else if (thing > 0) {
        struct Thing *p_thing;
        p_thing = &things[thing];
        return ((p_thing->Flag & TngF_Destroyed) != 0);
    } else {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        return ((p_sthing->Flag & TngF_Destroyed) != 0);
    }
}

TbBool thing_is_pickup_item(ThingIdx thing)
{
    struct SimpleThing *p_sthing;

    if (thing >= 0)
        return false;

    p_sthing = &sthings[thing];
    return thing_type_is_pickup_item(p_sthing->Type);
}

static DIFFTEST_REF_FUNC TbResult delete_node_asm(struct Thing *p_thing)
{
    TbResult ret;
    asm volatile ("call ASM_delete_node\n"
        : "=a" (ret) : "a" (p_thing));
    return ret;
}

TbResult delete_node(struct Thing *p_thing)
{
    s32 tile_x, tile_z;
    short parent, nxt;

    DIFFTEST_SHADOW_RET(delete_node, TbResult, delete_node_asm(p_thing));
    tile_x = p_thing->X >> 8;
    if (tile_x < 0 || tile_x >= 0x8000)
        DIFFTEST_RETURN((TbResult)0);
    tile_z = p_thing->Z >> 8;
    if (tile_z < 0 || tile_z >= 0x8000)
        DIFFTEST_RETURN((TbResult)0);

    parent = p_thing->Parent;
    if (parent > 0)
        things[parent].Next = p_thing->Next;
    else if (parent < 0)
        sthings[parent].Next = p_thing->Next;
    else
        game_my_big_map[(p_thing->Z >> 16) * 128 + (p_thing->X >> 16)].Child = p_thing->Next;

    nxt = p_thing->Next;
    if (nxt > 0)
        things[nxt].Parent = p_thing->Parent;
    else if (nxt < 0)
        sthings[nxt].Parent = p_thing->Parent;

    p_thing->Parent = 0;
    p_thing->Next = 0;
    DIFFTEST_RETURN((TbResult)0);
}

static DIFFTEST_REF_FUNC TbResult delete_snode_asm(struct SimpleThing *p_sthing)
{
    TbResult ret;
    asm volatile ("call ASM_delete_snode\n"
        : "=a" (ret) : "a" (p_sthing));
    return ret;
}

TbResult delete_snode(struct SimpleThing *p_sthing)
{
    short parent, nxt;

    DIFFTEST_SHADOW_RET(delete_snode, TbResult, delete_snode_asm(p_sthing));
    parent = p_sthing->Parent;
    if (parent > 0)
        things[parent].Next = p_sthing->Next;
    else if (parent < 0)
        sthings[parent].Next = p_sthing->Next;
    else
        game_my_big_map[(p_sthing->Z >> 16) * 128 + (p_sthing->X >> 16)].Child = p_sthing->Next;

    nxt = p_sthing->Next;
    if (nxt > 0)
        things[nxt].Parent = p_sthing->Parent;
    else if (nxt < 0)
        sthings[nxt].Parent = p_sthing->Parent;
    DIFFTEST_RETURN((TbResult)0);
}

static DIFFTEST_REF_FUNC void add_node_thing_asm(ThingIdx new_thing)
{
    asm volatile ("call ASM_add_node_thing\n"
        : : "a" (new_thing));
}

void add_node_thing(ThingIdx new_thing)
{
    struct Thing *p_thing;
    struct MyMapElement *p_mapel;
    s32 tile_x, tile_z;
    short nxt;

    DIFFTEST_SHADOW(add_node_thing, add_node_thing_asm(new_thing));
    p_thing = &things[(ushort)new_thing];
    tile_x = p_thing->X >> 8;
    p_thing->Parent = 0;
    if (tile_x < 0 || tile_x >= 0x8000)
        return;
    tile_z = p_thing->Z >> 8;
    if (tile_z < 0 || tile_z >= 0x8000)
        return;

    p_mapel = &game_my_big_map[(p_thing->Z >> 16) * 128 + (p_thing->X >> 16)];
    p_thing->Next = p_mapel->Child;
    nxt = p_thing->Next;
    if (nxt > 0)
        things[nxt].Parent = new_thing;
    else if (nxt < 0)
        sthings[nxt].Parent = new_thing;
    p_mapel->Child = new_thing;
}

static DIFFTEST_REF_FUNC short get_new_thing_asm(void)
{
    short ret;
    asm volatile ("call ASM_get_new_thing\n"
        : "=a" (ret) : );
    return ret;
}

/** Takes the given thing index off the free list, if it is free.
 * Returns non-zero when the index was claimed; only the low 16 bits matter.
 */
static DIFFTEST_REF_FUNC int get_new_thing_with_index_asm(ThingIdx thing)
{
    int ret;
    asm volatile ("call ASM_get_new_thing_with_index\n"
        : "=a" (ret) : "a" (thing) : "cc", "memory");
    return ret;
}

int get_new_thing_with_index(ThingIdx thing)
{
    struct Thing *p_thing;
    ThingIdx used_head, thing_cur;

    DIFFTEST_SHADOW_RET(get_new_thing_with_index, int,
      get_new_thing_with_index_asm(thing));
    if ((ushort)thing > 1000)
        DIFFTEST_RETURN((int)0);

    p_thing = &things[(ushort)thing];
    used_head = things_used_head;
    // Already in use - fail. The asm returns a value with only the low
    // 16 bits being zero; callers check only ax.
    for (thing_cur = used_head; thing_cur != 0;
      thing_cur = things[(ushort)thing_cur].LinkChild)
    {
        if (thing_cur == thing)
            DIFFTEST_RETURN((int)0);
    }

    // Unlink from the empty list; the asm assumes it is there
    if (things_empty_head == thing)
    {
        things_empty_head = p_thing->LinkChild;
        things[p_thing->LinkChild].LinkParent = 0;
    }
    else
    {
        if (p_thing->LinkParent != 0)
            things[p_thing->LinkParent].LinkChild = p_thing->LinkChild;
        if (p_thing->LinkChild != 0)
            things[p_thing->LinkChild].LinkParent = p_thing->LinkParent;
    }

    // Put at head of the used list
    things[(ushort)used_head].LinkParent = thing;
    p_thing->LinkParent = 0;
    p_thing->LinkChild = used_head;
    p_thing->Flag = 0;
    p_thing->Flag2 = 0;
    things_used++;
    things_used_head = thing;
    DIFFTEST_RETURN((int)1);
}

short get_new_thing(void)
{
    ThingIdx new_thing, used_head, next_empty;

    DIFFTEST_SHADOW_RET(get_new_thing, short, get_new_thing_asm());
    // Few slots are always left unused
    if (things_used >= THINGS_LIMIT - 4)
        DIFFTEST_RETURN((short)0);

    new_thing = things_empty_head;
    used_head = things_used_head;

    things[(ushort)used_head].LinkParent = new_thing;
    things[(ushort)new_thing].LinkParent = 0;
    things[(ushort)new_thing].Flag = 0;
    next_empty = things[(ushort)new_thing].LinkChild;
    things[(ushort)new_thing].Flag2 = 0;
    things[(ushort)new_thing].U.UPerson.Flag3 = 0;
    things[(ushort)new_thing].LinkChild = used_head;
    things[(ushort)next_empty].LinkParent = 0;
    things_used++;

    things_used_head = new_thing;
    things_empty_head = next_empty;
    DIFFTEST_RETURN(new_thing);
}

static DIFFTEST_REF_FUNC void remove_thing_asm(short tngno)
{
    asm volatile ("call ASM_remove_thing\n"
        : : "a" (tngno));
}

void remove_thing(short tngno)
{
    struct Thing *p_thing;
    ushort link_parent, link_child, empty_head;

    DIFFTEST_SHADOW(remove_thing, remove_thing_asm(tngno));
    p_thing = &things[(ushort)tngno];
    link_child = p_thing->LinkChild;
    link_parent = p_thing->LinkParent;
    if (link_child != 0)
        things[link_child].LinkParent = link_parent;

    if (link_parent != 0)
        things[link_parent].LinkChild = link_child;
    else
        things_used_head = link_child;

    p_thing->LinkChild = things_empty_head;
    empty_head = things_empty_head;
    p_thing->LinkParent = 0;
    things_empty_head = tngno;
    things[empty_head].LinkParent = tngno;
    things_used--;
}

static DIFFTEST_REF_FUNC void add_node_sthing_asm(ThingIdx new_thing)
{
    asm volatile ("call ASM_add_node_sthing\n"
        : : "a" (new_thing));
}

void add_node_sthing(ThingIdx new_thing)
{
    struct SimpleThing *p_sthing;
    struct MyMapElement *p_mapel;
    s32 tile_x, tile_z;
    short nxt;

    DIFFTEST_SHADOW(add_node_sthing, add_node_sthing_asm(new_thing));
    p_sthing = &sthings[new_thing];
    p_sthing->Parent = 0;
    tile_x = p_sthing->X >> 8;
    tile_z = p_sthing->Z >> 8;
    if (tile_x < 0 || tile_x >= 0x8000 || tile_z < 0 || tile_z >= 0x8000)
        return;

    p_mapel = &game_my_big_map[(p_sthing->Z >> 16) * 128 + (p_sthing->X >> 16)];
    p_sthing->Next = p_mapel->Child;
    nxt = p_sthing->Next;
    if (nxt > 0)
        things[nxt].Parent = new_thing;
    else if (nxt < 0)
        sthings[nxt].Parent = new_thing;
    p_mapel->Child = new_thing;
}

static DIFFTEST_REF_FUNC short get_new_sthing_asm(void)
{
    short ret;
    asm volatile ("call ASM_get_new_sthing\n"
        : "=a" (ret) : );
    return ret;
}

short get_new_sthing(void)
{
    ThingIdx new_sthing, used_head, next_empty;

    DIFFTEST_SHADOW_RET(get_new_sthing, short, get_new_sthing_asm());
    // Few slots are always left unused
    if (sthings_used >= STHINGS_LIMIT - 4)
        DIFFTEST_RETURN((short)0);

    new_sthing = sthings_empty_head;
    used_head = sthings_used_head;

    sthings[used_head].LinkParent = new_sthing;
    sthings[new_sthing].LinkParent = 0;
    next_empty = sthings[new_sthing].LinkChild;
    sthings[new_sthing].Flag = 0;
    sthings[new_sthing].LinkChild = used_head;
    sthings[next_empty].LinkParent = 0;
    sthings_used++;

    sthings_used_head = new_sthing;
    sthings_empty_head = next_empty;
    DIFFTEST_RETURN(new_sthing);
}

static DIFFTEST_REF_FUNC void remove_sthing_asm(short tngno)
{
    asm volatile ("call ASM_remove_sthing\n"
        : : "a" (tngno));
}

void remove_sthing(short tngno)
{
    struct SimpleThing *p_sthing;
    ThingIdx link_parent, link_child, empty_head, nxt_empty;

    DIFFTEST_SHADOW(remove_sthing, remove_sthing_asm(tngno));
    p_sthing = &sthings[tngno];
    link_child = p_sthing->LinkChild;
    p_sthing->Type = 0;
    link_parent = p_sthing->LinkParent;
    if (link_child < 0)
        sthings[link_child].LinkParent = link_parent;

    if (sthings_used_head == tngno)
        sthings_used_head = link_child;
    else
        sthings[link_parent].LinkChild = link_child;

    empty_head = sthings_empty_head;
    // TODO the asm reads the upper 16 bits of a 32-bit load from `sthings_used`,
    // ie. the short placed right after it in memory; originally that was most
    // likely the previous `sthings_empty_head`. Replicated exactly.
    nxt_empty = *(((short *)&sthings_used) + 1);
    p_sthing->LinkParent = 0;
    p_sthing->LinkChild = empty_head;
    sthings_empty_head = tngno;
    sthings_used--;
    sthings[nxt_empty].LinkParent = tngno;
}

short add_static(int x, int y, int z, ushort frame, int timer)
{
#if 0
    short ret;
    asm volatile (
      "push %5\n"
      "call ASM_add_static\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (frame), "g" (timer));
    return ret;
#else
    ThingIdx thing;
    struct SimpleThing *p_sthing;

    if (map_coords_limit(NULL, NULL, NULL, x, y, z))
        return 0;
    if (sthings_used > STHINGS_LIMIT - 5) {
        return 0;
    }

    thing = get_new_sthing();
    if (thing == 0) {
        return 0;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return 0;
    }
    p_sthing = &sthings[thing];
    p_sthing->Z = MAPCOORD_TO_PRCCOORD(z,127);
    p_sthing->X = MAPCOORD_TO_PRCCOORD(x,127);
    p_sthing->Y = y;
    p_sthing->Parent = 0;
    p_sthing->StartFrame = frame - 1;
    //TODO verify and/or unify to allow use of reset_sthing_frame(p_sthing);
    p_sthing->Frame = nstart_ani[p_sthing->StartFrame + 1];
    add_node_sthing(thing);
    p_sthing->Type = SmTT_STATIC;
    p_sthing->Radius = 64;
    p_sthing->Flag = 4;
    p_sthing->Timer1 = timer;
    return thing;
#endif
}

TbBool thing_is_within_circle(ThingIdx thing, short X, short Z, ushort R)
{
    s32 dtX, dtZ, r2;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
    }
    r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < r2);
}

TbBool thing_intersects_circle(ThingIdx thing, short X, short Z, ushort R)
{
    s32 dtX, dtZ, cyl_r2, tng_r;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
        tng_r = p_sthing->Radius;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
        tng_r = p_thing->Radius;
    }
    cyl_r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < cyl_r2 + tng_r * tng_r);
}

TbBool thing_intersects_cylinder(ThingIdx thing, short X, short Y, short Z, ushort R, ushort H)
{
    s32 dtX, dtY, dtZ, cyl_r2, tng_r, h_min, h_max;

    if (thing <= 0) {
        struct SimpleThing *p_sthing;
        p_sthing = &sthings[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_sthing->X) - X;
        dtY = PRCCOORD_TO_MAPCOORD(p_sthing->Y) - Y;
        dtZ = PRCCOORD_TO_MAPCOORD(p_sthing->Z) - Z;
        tng_r = p_sthing->Radius;
        h_min = H;
        h_max = H;
    } else {
        struct Thing *p_thing;
        p_thing = &things[thing];
        dtX = PRCCOORD_TO_MAPCOORD(p_thing->X) - X;
        dtY = PRCCOORD_TO_MAPCOORD(p_thing->Y) - Y;
        dtZ = PRCCOORD_TO_MAPCOORD(p_thing->Z) - Z;
        tng_r = p_thing->Radius;
        h_min = H;
        h_max = H;
        // Things represented by 3D objects require lower minimum
        if ((p_thing->Type == TT_VEHICLE) || (p_thing->Type == TT_BUILDING)) {
            h_min += 80;
        }
    }
    cyl_r2 = R * R;
    return ((dtZ * dtZ + dtX * dtX) < cyl_r2 + tng_r * tng_r)
        && ((dtY > -h_min) && (dtY < h_max));
}

void build_same_type_headers(void)
{
#if 0
    asm volatile ("call ASM_build_same_type_headers\n"
        :  :  : "eax" );
    return;
#endif
    struct Thing *p_thing;
    short i;
    ThingIdx thing;
    TbBool link_all_people;

    // On specific modes, people are linked into same-type lists even
    // while under a flag which would normally exclude them
    link_all_people = (word_1552F8 == 36) || (word_1552F8 == 18);

    for (i = 0; i < 256 + PEOPLE_GROUPS_LIMIT + 1; i++)
        same_type_head[i] = 0;

    for (thing = things_used_head; thing != 0; thing = p_thing->LinkChild)
    {
        p_thing = &things[thing];

        switch (p_thing->Type)
        {
        case TT_VEHICLE:
            if (p_thing->SubType == SubTT_VEH_SHIP) {
                p_thing->LinkSame = same_type_head[5];
                same_type_head[5] = thing;
            } else {
                p_thing->LinkSame = same_type_head[2];
                same_type_head[2] = thing;
            }
            break;
        case TT_PERSON:
        case TT_UNKN4:
            if ((p_thing->Flag2 & (TgF2_ExistsOffMap|TgF2_AlteredSubType)) == 0 || link_all_people)
            {
                ubyte group;

                p_thing->LinkSame = same_type_head[1];
                same_type_head[1] = thing;

                group = p_thing->U.UPerson.Group & PEOPLE_GROUPS_INDEX_MASK;
                p_thing->LinkSameGroup = same_type_head[256 + group];
                same_type_head[256 + group] = thing;
            }
            break;
        case TT_BUILDING:
            if (p_thing->SubType == SubTT_BLD_MGUN) {
                p_thing->LinkSame = same_type_head[7];
                same_type_head[7] = thing;
            } else {
                p_thing->LinkSame = same_type_head[3];
                same_type_head[3] = thing;
            }
            break;
        case TT_MINE:
            if (p_thing->SubType == 48) {
                p_thing->LinkSame = same_type_head[6];
                same_type_head[6] = thing;
            }
            break;
        default:
            break;
        }
    }
}

short get_thing_same_type_head(short ttype, short subtype)
{
    ThingIdx thing;

    switch (ttype)
    {
    case TT_PERSON:
    case TT_UNKN4:
        thing = same_type_head[1];
        break;
    case TT_VEHICLE:
        if (subtype == SubTT_VEH_SHIP)
            thing = same_type_head[5];
        else
            thing = same_type_head[2];
        break;
    case TT_BUILDING:
        if (subtype == SubTT_BLD_MGUN)
            thing = same_type_head[7];
        else
            thing = same_type_head[3];
        break;
    case TT_MINE:
        if (subtype == 48)
            thing = same_type_head[6];
        else
            thing = 0;
        break;
    default:
        thing = 0;
        break;
    }
    return thing;
}

void new_thing_traffic_clone(struct SimpleThing *p_clsthing)
{
    asm volatile ("call ASM_new_thing_traffic_clone\n"
        : : "a" (p_clsthing));
}

short new_thing_smoke_gen_clone(struct SimpleThing *p_clsthing)
{
    struct SimpleThing *p_sthing;
    ThingIdx thing;

    thing = add_static(PRCCOORD_TO_MAPCOORD(p_clsthing->X), p_clsthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clsthing->Z), 0, p_clsthing->Timer1);
    p_sthing = &sthings[thing];
    p_sthing->Type = SmTT_SMOKE_GENERATOR;
    p_sthing->U.UEffect.VX = p_clsthing->U.UEffect.VX;
    p_sthing->U.UEffect.VY = p_clsthing->U.UEffect.VY;
    p_sthing->U.UEffect.VZ = p_clsthing->U.UEffect.VZ;
    p_sthing->StartTimer1 = p_clsthing->StartTimer1;
    p_sthing->U.UEffect.OX = p_clsthing->U.UEffect.OX;
    p_sthing->U.UEffect.OY = p_clsthing->U.UEffect.OY;
    p_sthing->U.UEffect.OZ = p_clsthing->U.UEffect.OZ;
    return thing;
}

short new_thing_static_clone(struct SimpleThing *p_clsthing)
{
    struct SimpleThing *p_sthing;
    ThingIdx thing;
    ushort frame;

    thing = add_static(PRCCOORD_TO_MAPCOORD(p_clsthing->X), p_clsthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clsthing->Z), p_clsthing->StartFrame + 1, p_clsthing->Timer1);
    p_sthing = &sthings[thing];
    p_sthing->U.UEffect.VZ = p_clsthing->U.UEffect.VZ;
    frame = p_sthing->StartFrame;
    if (frame != 999 && frame != 1002 && frame != 1004 && frame != 1008 &&
      frame != 1032 && frame != 1037 && frame != 1038 && frame != 1050) {
        p_sthing->SubType = 2;
    } else {
        p_sthing->SubType = 1;
        p_sthing->Radius = 128;
    }
    return thing;
}

ThingIdx new_thing_building_clone(struct Thing *p_clthing, struct M33 *p_clmatx, short shut_h)
{
    struct Thing *p_thing;
    struct SingleObject *p_sobj;
    int i;

    p_thing = create_building_thing(PRCCOORD_TO_MAPCOORD(p_clthing->X), p_clthing->Y,
      PRCCOORD_TO_MAPCOORD(p_clthing->Z), p_clthing->U.UObject.Object,
      p_clthing->U.UObject.NumbObjects, p_clthing->ThingOffset);

    p_thing->ThingOffset = p_clthing->ThingOffset;
    p_thing->Flag = p_clthing->Flag;
    p_thing->VX = p_clthing->VX;
    p_thing->VY = p_clthing->VY;
    p_thing->VZ = p_clthing->VZ;
    p_thing->SubType = p_clthing->SubType;

    ubyte styp;
    styp = p_thing->SubType;

    if (styp == SubTT_BLD_MGUN)
    {
        p_thing->U.UMGun.PathIndex = p_clthing->U.UMGun.PathIndex;
        p_thing->U.UMGun.UniqueID = p_clthing->U.UMGun.UniqueID;
        p_thing->U.UMGun.NextThing = p_clthing->U.UMGun.NextThing;
        p_thing->U.UMGun.PrevThing = p_clthing->U.UMGun.PrevThing;
        p_thing->U.UMGun.Token = p_clthing->U.UMGun.Token;
        p_thing->U.UMGun.TokenDir = p_clthing->U.UMGun.TokenDir;
        p_thing->U.UMGun.ObjectNo = p_clthing->U.UMGun.ObjectNo;
        p_thing->U.UMGun.CurrentWeapon = p_clthing->U.UMGun.CurrentWeapon;
    }
    else
    {
        p_thing->U.UObject.Token = p_clthing->U.UObject.Token;
        p_thing->U.UObject.TokenDir = p_clthing->U.UObject.TokenDir;
        p_thing->U.UObject.NextThing = p_clthing->U.UObject.NextThing;
        p_thing->U.UObject.PrevThing = p_clthing->U.UObject.PrevThing;
        p_thing->U.UObject.OffX = p_clthing->U.UObject.OffX;
        p_thing->U.UObject.OffZ = p_clthing->U.UObject.OffZ;
        p_thing->U.UObject.BuildStartVect = p_clthing->U.UObject.BuildStartVect;
        p_thing->U.UObject.BuildNumbVect = p_clthing->U.UObject.BuildNumbVect;
        // Copy all fields from Turn down
        p_thing->U.UObject.Turn = p_clthing->U.UObject.Turn;
        p_thing->U.UObject.TurnPadOnPS = p_clthing->U.UObject.TurnPadOnPS;
        for (i = 0; i < 4; i++) {
            p_thing->U.UObject.tnode[i] = p_clthing->U.UObject.tnode[i];
        }
        p_thing->U.UObject.player_in_me = p_clthing->U.UObject.player_in_me;
        p_thing->U.UObject.unkn_4D = p_clthing->U.UObject.unkn_4D;
        p_thing->U.UObject.DrawTurn = p_clthing->U.UObject.DrawTurn;
        for (i = 0; i < 4; i++) {
            p_thing->U.UObject.tnode_50[i] = p_clthing->U.UObject.tnode_50[i];
        }
    }

    p_sobj = &game_objects[p_clthing->U.UObject.Object];
    p_thing->U.UObject.MinY[0] = p_sobj->OffsetY - 500;
    p_thing->U.UObject.MaxY[0] = p_sobj->OffsetY;
    
    if (styp == SubTT_BLD_SHUTLDR)
    {
        if (((p_thing->Flag & TngF_Unkn0001) == 0)
          && (p_thing->U.UObject.PrevThing == 0 || p_thing->U.UObject.NextThing == 0))
        {
            p_thing->VX = p_thing->X >> 16;
            p_thing->VZ = p_thing->Z >> 16;
            p_thing->Flag |=  TngF_Unkn0001;
        }
        if (shut_h < 15)
            p_thing->Y >>= 3;
    }
    else if (styp == SubTT_BLD_MGUN)
    {
        assert(next_local_mat < LOCAL_MATS_COUNT);
        p_thing->U.UMGun.MatrixIndex = next_local_mat++;
        if (p_clmatx != NULL)
            memcpy(&local_mats[p_thing->U.UMGun.MatrixIndex], p_clmatx, sizeof(struct M33));
        else
            matrix_identity_fill(&local_mats[p_thing->U.UMGun.MatrixIndex]);
        p_thing->U.UMGun.AngleX = 1024;
        p_thing->U.UMGun.AngleY = 0;
        p_thing->Radius = 256;
        p_thing->U.UMGun.RecoilTimer = 0;
        p_thing->U.UMGun.MaxHealth = 6000;
        p_thing->U.UMGun.RecoilTimer = 0;
        p_thing->Health = p_thing->U.UMGun.MaxHealth;
    }
    else if (styp >= SubTT_BLD_MOVN_ROTOR && styp <= SubTT_BLD_37)
    {
        assert(next_local_mat < LOCAL_MATS_COUNT);
        p_thing->U.UObject.MatrixIndex = next_local_mat++;
        if (p_clmatx != NULL)
            memcpy(&local_mats[p_thing->U.UObject.MatrixIndex], p_clmatx, sizeof(struct M33));
        else
            matrix_identity_fill(&local_mats[p_thing->U.UObject.MatrixIndex]);
        p_thing->Flag |= TngF_SelectedAgent;
    }
    p_thing->SubState = p_clthing->SubState;
    p_thing->Timer1 = p_clthing->Timer1;

    // Should be have a separate UGate struct?
    if (styp >= SubTT_BLD_GATE && styp <= SubTT_BLD_26
      && p_thing->U.UObject.MinY[0] == *(ushort *)&p_thing->U.UObject.Group )
    {
        p_thing->U.UObject.MinY[0] = -500;
        p_thing->U.UObject.MaxY[0] = 0;
    }
    p_thing->State = p_clthing->State;
    if (p_thing->State == 9)
        p_thing->State = 0;
    p_thing->Frame = p_clthing->Frame;

    return p_thing->ThingOffset;
}

/** Maps fields from old Thing struct to the current one.
 */
void refresh_old_thing_format(struct Thing *p_thing, struct ThingOldV9 *p_oldthing, u32 fmtver)
{
    ushort len;

    LbMemorySet(p_thing, 0, sizeof(struct Thing));

    p_thing->Parent = p_oldthing->Parent;
    p_thing->Next = p_oldthing->Next;
    p_thing->LinkParent = p_oldthing->LinkParent;
    p_thing->LinkChild = p_oldthing->LinkChild;
    p_thing->SubType = p_oldthing->SubType;
    p_thing->Type = p_oldthing->Type;
    p_thing->State = p_oldthing->State;
    p_thing->Flag = p_oldthing->Flag;
    p_thing->LinkSame = p_oldthing->LinkSame;
    p_thing->Radius = p_oldthing->Radius;
    p_thing->X = p_oldthing->X;
    p_thing->Y = p_oldthing->Y;
    p_thing->Z = p_oldthing->Z;
    p_thing->Frame = p_oldthing->Frame;
    p_thing->StartFrame = p_oldthing->StartFrame;
    p_thing->Timer1 = p_oldthing->Timer1;
    p_thing->StartTimer1 = p_oldthing->StartTimer1;
    p_thing->LinkSameGroup = p_oldthing->LinkSameGroup;
    p_thing->ThingOffset = p_oldthing->ThingOffset;
    p_thing->VX = p_oldthing->VX;
    p_thing->VY = p_oldthing->VY;
    p_thing->VZ = p_oldthing->VZ;
    p_thing->Speed = p_oldthing->Speed;
    p_thing->Health = p_oldthing->Health;
    p_thing->Owner = p_oldthing->Owner;
    p_thing->PathOffset = p_oldthing->PathOffset;
    // The target pointer will be cleared anyway
    p_thing->PTarget = NULL; // p_oldthing->PTarget;
    p_thing->GotoThingIndex = p_oldthing->GotoThingIndex;
    p_thing->SubState = p_oldthing->SubState;

    // Type-dependent fields
    if (p_thing->Type == TT_PERSON)
    {
        p_thing->U.UPerson.AnimMode = p_oldthing->PersonAnimMode;
        p_thing->U.UPerson.OldAnimMode = p_oldthing->PersonOldAnimMode;
        p_thing->U.UPerson.Timer2 = p_oldthing->Timer2;
        p_thing->U.UPerson.StartTimer2 = p_oldthing->StartTimer2;
        p_thing->U.UPerson.Angle = p_oldthing->PersonAngle;
        p_thing->U.UPerson.GotoX = p_oldthing->PersonGotoX;
        p_thing->U.UPerson.GotoZ = p_oldthing->PersonGotoZ;
        p_thing->U.UPerson.Group = p_oldthing->PersonGroup;
        p_thing->U.UPerson.EffectiveGroup = p_oldthing->PersonEffectiveGroup;
        p_thing->U.UPerson.WeaponsCarried = p_oldthing->PersonWeaponsCarried;
        p_thing->U.UPerson.CurrentWeapon = p_oldthing->PersonCurrentWeapon;
        p_thing->U.UPerson.ComHead = p_oldthing->PersonComHead;
        p_thing->U.UPerson.ComCur = p_oldthing->PersonComCur;
        p_thing->U.UPerson.ComTimer = p_oldthing->PersonComTimer;
        p_thing->U.UPerson.Brightness = p_oldthing->PersonBrightness;
        p_thing->U.UPerson.MaxShieldEnergy = p_oldthing->PersonMaxShieldEnergy;
        p_thing->U.UPerson.UniqueID = p_oldthing->PersonUniqueID;
        p_thing->U.UPerson.PathIndex = p_oldthing->PersonPathIndex;
        p_thing->U.UPerson.WeaponTimer = p_oldthing->PersonWeaponTimer;
        p_thing->U.UPerson.WeaponTurn = p_oldthing->PersonWeaponTurn;
        p_thing->U.UPerson.Energy = p_oldthing->PersonEnergy;
        p_thing->U.UPerson.MaxEnergy = p_oldthing->PersonMaxEnergy;
        // Uncertain fields
        p_thing->U.UPerson.ShieldEnergy = p_oldthing->PersonShieldEnergy;
        p_thing->U.UPerson.SpecialTimer = p_oldthing->PersonSpecialTimer;
        p_thing->U.UPerson.ComRange = p_oldthing->PersonComRange;
        p_thing->U.UPerson.BumpMode = p_oldthing->PersonBumpMode;
        p_thing->U.UPerson.BumpCount = p_oldthing->PersonBumpCount;
        p_thing->U.UPerson.Vehicle = p_oldthing->PersonVehicle;
        p_thing->U.UPerson.LinkPassenger = p_oldthing->PersonLinkPassenger;
        p_thing->U.UPerson.Within = p_oldthing->PersonWithin;
        p_thing->U.UPerson.LastDist = p_oldthing->PersonLastDist;
        p_thing->U.UPerson.OnFace = p_oldthing->PersonOnFace;
        // End of uncertain fields
        p_thing->U.UPerson.ComRange = p_oldthing->PersonComRange;
        p_thing->U.UPerson.Shadows[0] = p_oldthing->PersonShadows[0];
        p_thing->U.UPerson.Shadows[1] = p_oldthing->PersonShadows[1];
        p_thing->U.UPerson.Shadows[2] = p_oldthing->PersonShadows[2];
        p_thing->U.UPerson.Shadows[3] = p_oldthing->PersonShadows[3];
        p_thing->U.UPerson.RecoilTimer = p_oldthing->PersonRecoilTimer;
        p_thing->U.UPerson.MaxHealth = p_oldthing->PersonMaxHealth;
        p_thing->U.UPerson.RecoilDir = p_oldthing->PersonRecoilDir;
        // Having cybernetic mods is kind of important; but we do not have location of these
        //p_thing->U.UPerson.UMod.Mods = p_oldthing->?;
        if (fmtver >= 12) {
            p_thing->U.UPerson.Stamina = p_oldthing->PersonStamina;
            p_thing->U.UPerson.MaxStamina = p_oldthing->PersonMaxStamina;
        }
        // Field FrameId
        if (p_thing->SubType == SubTT_PERS_PUNK_F) {
            // Randomize hair color - 50% normal red(0), 25% blonde(1), 25% blue(2)
            // The ThingOffset should be random enough
            len = (p_thing->ThingOffset ^ p_thing->U.UPerson.UniqueID) & 0xF;
            if (len < 4)
                p_thing->U.UPerson.FrameId.Version[0] = 1;
            else if (len < 8)
                p_thing->U.UPerson.FrameId.Version[0] = 2;
        }
        if ((fmtver < 8) && (p_thing->U.UPerson.Group == 0)) {
            // Some very old formats may have effective group set and normal zeroed out
            p_thing->U.UPerson.Group = p_thing->U.UPerson.EffectiveGroup;
        }
        // The current weapon should always be in a list of carried wepons (issue mostly for fmtver < 5)
        if (p_thing->U.UPerson.CurrentWeapon > 0)
            p_thing->U.UPerson.WeaponsCarried |= (1 << (p_thing->U.UPerson.CurrentWeapon - 1));
    }
    else if (p_thing->Type == TT_VEHICLE)
    {
        p_thing->U.UVehicle.Object = p_oldthing->VehicleObject;
        p_thing->U.UVehicle.NumbObjects = p_oldthing->VehicleNumbObjects;
        p_thing->U.UVehicle.Timer2 = p_oldthing->Timer2;
        p_thing->U.UVehicle.AngleX = p_oldthing->VehicleAngleX;
        p_thing->U.UVehicle.AngleY = p_oldthing->VehicleAngleY;
        p_thing->U.UVehicle.AngleZ = p_oldthing->VehicleAngleZ;
        p_thing->U.UVehicle.GotoX = p_oldthing->VehicleGotoX;
        p_thing->U.UVehicle.GotoY = p_oldthing->VehicleGotoY;
        p_thing->U.UVehicle.GotoZ = p_oldthing->VehicleGotoZ;
        p_thing->U.UVehicle.MatrixIndex = p_oldthing->VehicleMatrixIndex;
        p_thing->U.UVehicle.UniqueID = p_oldthing->VehicleUniqueID;
        p_thing->U.UVehicle.MaxSpeed = p_oldthing->VehicleReqdSpeed;
        p_thing->U.UVehicle.ReqdSpeed = p_oldthing->VehicleReqdSpeed;
        p_thing->U.UVehicle.PassengerHead = p_oldthing->VehiclePassengerHead;
        p_thing->U.UVehicle.TNode = p_oldthing->VehicleTNode;
        p_thing->U.UVehicle.AngleDY = p_oldthing->VehicleAngleDY;
        p_thing->U.UVehicle.RecoilTimer = p_oldthing->VehicleRecoilTimer;
        // Only one group seem to exist in the old vehicles
        p_thing->U.UVehicle.Group = p_oldthing->VehicleEffectiveGroup;
        p_thing->U.UVehicle.EffectiveGroup = p_oldthing->VehicleEffectiveGroup;
        // In old format, MaxHealth is stored in additional vehicle block, not in the thing
        //p_thing->U.UVehicle.MaxHealth = ?;
    }
    else if (p_thing->Type == TT_BUILDING)
    {
        p_thing->U.UObject.Object = p_oldthing->ObjectObject;
        p_thing->U.UObject.NumbObjects = p_oldthing->ObjectNumbObjects;
        p_thing->U.UObject.Timer[0] = p_oldthing->ObjectTimer0;
        p_thing->U.UObject.Timer[1] = p_oldthing->ObjectTimer1;
        p_thing->U.UObject.Angle = p_oldthing->ObjectAngle;
        p_thing->U.UObject.TargetDX = p_oldthing->ObjectTargetDX;
        p_thing->U.UObject.TargetDY = p_oldthing->ObjectTargetDY;
        p_thing->U.UObject.TargetDZ = p_oldthing->ObjectTargetDZ;
        // Only one group seem to exist in the old Objects
        p_thing->U.UObject.Group = p_oldthing->ObjectEffectiveGroup;
        p_thing->U.UObject.EffectiveGroup = p_oldthing->ObjectEffectiveGroup;
        if (p_thing->SubType == SubTT_BLD_GATE) {
            p_thing->U.UObject.RaiseDY[0] = p_oldthing->ObjectRaiseDY[0];
            p_thing->U.UObject.RaiseDY[1] = p_oldthing->ObjectRaiseDY[1];
            p_thing->VX = 0;
            p_thing->VY = 0;
            p_thing->VZ = 0;
            p_thing->U.UObject.MinY[0] = p_oldthing->ObjectMinY0;
            p_thing->U.UObject.RaiseY[1] = p_oldthing->ObjectRaiseY1;
            p_thing->Frame = 0;
            p_thing->StartFrame = 0;
        }
        else if (p_thing->SubType == SubTT_BLD_MGUN) {
            p_thing->U.UMGun.Object = p_oldthing->MGunObject;
            p_thing->U.UMGun.NumbObjects = p_oldthing->MGunNumbObjects;
            p_thing->U.UMGun.AngleX = p_oldthing->MGunAngleX;
            p_thing->U.UMGun.AngleY = p_oldthing->MGunAngleY;
            p_thing->U.UMGun.AngleZ = p_oldthing->MGunAngleZ;
            p_thing->U.UMGun.GotoX = p_oldthing->MGunGotoX;
            p_thing->U.UMGun.GotoY = p_oldthing->MGunGotoY;
            p_thing->U.UMGun.GotoZ = p_oldthing->MGunGotoZ;
            p_thing->U.UMGun.UniqueID = p_oldthing->MGunUniqueID;
            // Only one group seem to exist in the old MGun
            p_thing->U.UMGun.Group = p_oldthing->MGunEffectiveGroup;
            p_thing->U.UMGun.EffectiveGroup = p_oldthing->MGunEffectiveGroup;
        }
    }
}

static DIFFTEST_REF_FUNC struct SimpleThing *create_item_asm(int x, int y,
  int z, ushort frame, ubyte subtype)
{
    struct SimpleThing *ret;
    // Pushed through a register holding it: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[1];
    int edx = y, ebx = z, ecx = frame;

    stkargs[0] = (u32)subtype;

    asm volatile (
      "push 0(%4)\n"
      "call ASM_create_item\n"
        : "=a" (ret), "+d" (edx), "+b" (ebx), "+c" (ecx)
        : "S" (stkargs), "0" (x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_item(int x, int y, int z, ushort frame, ubyte subtype)
{
    struct SimpleThing *p_sthing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_item, struct SimpleThing *,
      create_item_asm(x, y, z, frame, subtype));
    if ((u32)x >= 0x8000 || (u32)z >= 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    sthing = get_new_sthing();
    if (sthing > -1 || sthing < -1500)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_sthing = &sthings[sthing];
    p_sthing->StartTimer1 = 5;
    p_sthing->Timer1 = 5;
    p_sthing->X = x << 8;
    p_sthing->Z = z << 8;
    p_sthing->Y = y << 8;
    p_sthing->Type = SmTT_DROPPED_ITEM;
    p_sthing->U.UWeapon.OnFace = 0;
    p_sthing->U.UWeapon.Ammo = 0;
    p_sthing->Radius = 30;
    p_sthing->Frame = nstart_ani[frame];
    p_sthing->StartFrame = frame - 1;
    p_sthing->U.UWeapon.WeaponType = subtype;

    switch (subtype)
    {
    case 0:
        p_sthing->StartFrame = 1058;
        p_sthing->Frame = nstart_ani[1059];
        break;
    case 2:
    case 3:
    case 4:
    case 8:
    case 15:
    case 17:
    case 20:
        p_sthing->StartFrame = 1055;
        p_sthing->Frame = nstart_ani[1056];
        break;
    case 6:
    case 11:
    case 13:
        p_sthing->StartFrame = 1087;
        p_sthing->Frame = nstart_ani[1088];
        break;
    case 10:
    case 12:
        p_sthing->StartFrame = 1088;
        p_sthing->Frame = nstart_ani[1089];
        break;
    default:
        p_sthing->StartFrame = 1003;
        p_sthing->Frame = nstart_ani[1004];
        break;
    }

    ingame.fld_unkCB7++;
    add_node_sthing(sthing);
    p_sthing->Flag = 0x04;
    p_sthing->SubType = subtype;
    DIFFTEST_RETURN(p_sthing);
}

static DIFFTEST_REF_FUNC struct SimpleThing *create_scale_effect_asm(int x,
  int y, int z, ushort frame, short timer)
{
    struct SimpleThing *ret;
    // Pushed through a register holding it: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[1];
    int edx = y, ebx = z, ecx = frame;

    stkargs[0] = (s32)timer;

    asm volatile (
      "push 0(%4)\n"
      "call ASM_create_scale_effect\n"
        : "=a" (ret), "+d" (edx), "+b" (ebx), "+c" (ecx)
        : "S" (stkargs), "0" (x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_scale_effect(int x, int y, int z, ushort frame, short timer)
{
    struct SimpleThing *p_sthing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_scale_effect, struct SimpleThing *,
      create_scale_effect_asm(x, y, z, frame, timer));
    if (x < 0 || x >= 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (z < 0 || z >= 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if ((s32)sthings_used > 1495)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    sthing = get_new_sthing();
    if (sthing == 0)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (sthing > -1 || sthing < -1500)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_sthing = &sthings[sthing];
    p_sthing->Z = z << 8;
    p_sthing->X = x << 8;
    p_sthing->Y = y << 8;
    p_sthing->Frame = nstart_ani[frame];
    p_sthing->StartFrame = frame - 1;
    add_node_sthing(sthing);
    p_sthing->Type = SmTT_SCALE_EFFECT;
    p_sthing->SubType = 0;
    p_sthing->Radius = 64;
    p_sthing->Flag = 0x04;
    p_sthing->Timer1 = timer;
    DIFFTEST_RETURN(p_sthing);
}

static DIFFTEST_REF_FUNC struct SimpleThing *create_sound_effect_asm(int x,
  int y, int z, ushort sample, int vol, int loop)
{
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];
    int edx = y, ebx = z, ecx = sample;

    stkargs[0] = (int)(intptr_t)vol;
    stkargs[1] = (int)(intptr_t)loop;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_create_sound_effect\n"
        : "=a" (ret), "+d" (edx), "+b" (ebx), "+c" (ecx)
        : "S" (stkargs), "0" (x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_sound_effect(int x, int y, int z, ushort sample, int vol, int loop)
{
    struct SimpleThing *p_sthing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_sound_effect, struct SimpleThing *,
      create_sound_effect_asm(x, y, z, sample, vol, loop));
    if (x < 0 || x >= 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (z < 0 || z >= 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if ((s32)sthings_used > 1495)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    sthing = get_new_sthing();
    if (sthing == 0)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (sthing > -1 || sthing < -1500)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_sthing = &sthings[sthing];
    p_sthing->Type = SmTT_SFX;
    p_sthing->Timer1 = 100;
    p_sthing->Flag = 0x04;
    p_sthing->State = 0;
    p_sthing->Z = z << 8;
    p_sthing->Y = y << 8;
    p_sthing->SubType = (ubyte)sample;
    p_sthing->X = x << 8;
    // TODO: the vol argument is ignored, sample always plays at full volume
    play_dist_ssample(p_sthing, sample, 0x7F, 0x40, 100, (short)loop, 1);
    DIFFTEST_RETURN(p_sthing);
}

struct SimpleThing *create_stasis_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer, struct Thing *p_owner)
{
    struct SimpleThing *ret;
    asm volatile (
      "push %5\n"
      "call ASM_create_stasis_pod\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (timer), "g" (p_owner));
    return ret;
}

struct SimpleThing *create_time_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer)
{
#if 0
    struct SimpleThing *ret;
    asm volatile (
      "call ASM_create_time_pod\n"
        : "=r" (ret) : "a" (x), "d" (y), "b" (z), "c" (timer));
    return ret;
#endif
    struct SimpleThing *p_podtng;
    ThingIdx thing;

    if (sthings_used > STHINGS_LIMIT - 5) {
        return NULL;
    }
    if ( x < 0 || z < 0 )
        return NULL;
    thing = get_new_sthing();
    if (thing == 0) {
        return NULL;
    }
    if (thing <= -STHINGS_LIMIT-1) {
        return NULL;
    }
    p_podtng = &sthings[thing];
    p_podtng->Radius = 0;
    p_podtng->StartFrame = 0;
    p_podtng->Type = 30;
    p_podtng->State = 0;
    p_podtng->X = MAPCOORD_TO_PRCCOORD(x, 0);
    p_podtng->Z = MAPCOORD_TO_PRCCOORD(z, 0);
    p_podtng->Y = MAPCOORD_TO_PRCCOORD(y, 0);
    p_podtng->Timer1 = timer;
    add_node_sthing(thing);
    return p_podtng;
}

struct SimpleThing *create_electric_strand(MapCoord x, MapCoord y, MapCoord z,
  MapCoord x2, MapCoord y2, MapCoord z2, int sound)
{
#if 1
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)y2;
    stkargs[1] = (int)(intptr_t)z2;
    stkargs[2] = (int)(intptr_t)sound;

    asm volatile (
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_electric_strand\n"
        : "=r" (ret)
        : "a" ((s32)x), "d" ((s32)y), "b" ((s32)z), "c" ((s32)x2), "S" (stkargs)
        : "cc", "memory");
    return ret;
#endif
}

void mine_detonate(struct Thing *p_thing)
{
    play_dist_sample(p_thing, 37, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 20);
    p_thing->State = 13;
    p_thing->Flag |= TngF_Destroyed;
    set_thing_frame(p_thing, 1069);
}

int mine_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    if (p_thing->State == 13) {
        return 0;
    }
    p_thing->Health -= hp;
    if (p_thing->Health <= 0) {
        mine_detonate(p_thing);
        return p_thing->Health + hp;
    }
    return 0;
}

int static_hit_by_bullet(struct SimpleThing *p_sthing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    p_sthing->U.UScenery.Health -= hp;
    if (p_sthing->U.UScenery.Health <= 0) {
        set_static_on_fire(p_sthing);
    }
    return 100;
}

struct Thing *replace_thing_given_thing_idx(int x, int y, int z, short owner, ThingIdx pv_thing)
{
    struct Thing *p_pv_thing;
    struct Thing *p_nx_thing;

    p_pv_thing = &things[pv_thing];
    if (on_mapwho(p_pv_thing))
        delete_node(p_pv_thing);
    unkn01_thing_idx = pv_thing;
    p_nx_thing = new_sim_person(x, y, z, 101u);
    p_nx_thing->State = 0;
    p_nx_thing->Owner = owner;
    return p_nx_thing;
}

/******************************************************************************/
