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

#include <string.h>
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
#include "crtrand.h"
#include "cybmod.h"
#include "difftest.h"
#include "display.h"
#include "engindrwlstx.h"
#include "engincam.h"
#include "engincolour.h"
#include "enginfexpl.h"
#include "enginlights.h"
#include "enginshrapn.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "enginzoom.h"
#include "febrief.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvdraw3d.h"
#include "matrix.h"
#include "misstat.h"
#include "packet.h"
#include "people.h"
#include "pepgroup.h"
#include "player.h"
#include "sound.h"
#include "thing_expld.h"
#include "thing_fire.h"
#include "thing_search.h"
#include "tngcolisn.h"
#include "tngobjdrw.h"
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

static DIFFTEST_REF_FUNC ubyte on_mapwho_asm(struct Thing *p_thing)
{
    ubyte ret;
    DIFFTEST_ASM(
      "call ASM_on_mapwho\n"
        : "=a" (ret) : "a" (p_thing));
    return ret;
}

ubyte on_mapwho(struct Thing *p_thing)
{
    s32 mapel_idx;
    u32 tng_offs;
    short thing;

    DIFFTEST_SHADOW_RET(on_mapwho, ubyte, on_mapwho_asm(p_thing));
    mapel_idx = (p_thing->Z >> 16) * 128 + (p_thing->X >> 16);
    thing = game_my_big_map[mapel_idx].Child;
    // The asm compares sign-extended index with zero-extended ThingOffset
    tng_offs = (ushort)p_thing->ThingOffset;
    while (thing != 0)
    {
        if ((s32)thing == (s32)tng_offs)
            DIFFTEST_RETURN((ubyte)1);
        if (thing > 0)
            thing = things[thing].Next;
        else
            thing = sthings[thing].Next;
    }
    DIFFTEST_RETURN((ubyte)0);
}

static DIFFTEST_REF_FUNC void move_mapwho_asm(struct Thing *p_thing, int x, int y, int z)
{
    DIFFTEST_ASM(
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
    DIFFTEST_ASM("call ASM_init_just_things\n"
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
    DIFFTEST_ASM("call ASM_quick_light_unkn_func_07\n"
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
void quick_light_unkn_func_07(int object, int light_idx)
{
    struct SingleObject *p_sobj;
    struct QuickLight *p_qlights;
    ushort light = (ushort)light_idx;
    ushort count;
    ushort start_face, numb_faces, i;

    DIFFTEST_SHADOW(quick_light_unkn_func_07, quick_light_unkn_func_07_asm(object, light_idx, 0, 0));

    p_qlights = game_quick_lights;
    count = next_quick_light;
    p_sobj = &game_objects[(ushort)object];

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
    DIFFTEST_ASM("call ASM_quick_light_unkn_func_09\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Unlinks from the map element's QuickLight chain all entries
 * referencing given light.
 */
void quick_light_unkn_func_09(struct MyMapElement *p_mapel, int light_idx)
{
    ushort light = (ushort)light_idx;
    struct QuickLight *p_qlights;
    ushort qlight, prev_qlight;

    DIFFTEST_SHADOW(quick_light_unkn_func_09, quick_light_unkn_func_09_asm((intptr_t)p_mapel, light_idx, 0, 0));

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
    DIFFTEST_ASM(
      "call ASM_quick_light_unkn_func_04\n"
        : : "a" (a1), "d" (a2), "b" (a3), "c" (a4));
    return;
}

/** Removes given light from map elements and building objects within
 * 7 tiles of given position, and decrements full lights count.
 */
void quick_light_unkn_func_04(short pos_x, int unused_a2, short pos_z, short light_idx)
{
    ushort light = (ushort)light_idx;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(quick_light_unkn_func_04, quick_light_unkn_func_04_asm(pos_x, unused_a2, pos_z, light_idx));

    // Parameter unused_a2 is not used by the asm
    tile_x = pos_x >> 8;
    tile_z = pos_z >> 8;
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
                quick_light_unkn_func_09(p_mapel, light);

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
                              light);
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
    DIFFTEST_ASM("push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_quick_light_unkn_func_06\n"
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

void quick_light_unkn_func_06(int object, int pos_x, int pos_y, int pos_z,
  int unused_a5, int unused_a6, int light_idx)
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
      quick_light_unkn_func_06_asm(object, pos_x, pos_y, pos_z,
        unused_a5, unused_a6, light_idx));
    // unused_a5 and unused_a6 are not used by the asm
    x = pos_x;
    y = pos_y;
    z = pos_z;
    light = (ushort)light_idx;
    p_sobj = &game_objects[(ushort)object];
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
    DIFFTEST_ASM("push 4(%4)\n""push 0(%4)\n""call ASM_quick_light_unkn_func_08\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

extern s32 dword_176D6C;

void quick_light_unkn_func_08(int map_x, int map_z, int pos_x, int pos_y,
  int pos_z, int light_idx)
{
    struct MyMapElement *p_mapel;
    struct QuickLight *p_qlight;
    ushort tile_x, tile_z, lgt;
    s32 dx, dy, dz, dist_sq, dist, ratio, div;
    s32 intens;

    DIFFTEST_SHADOW(quick_light_unkn_func_08,
      quick_light_unkn_func_08_asm(map_x, map_z, pos_x, pos_y, pos_z, light_idx));
    tile_x = (ushort)map_x;
    tile_z = (ushort)map_z;
    p_mapel = &game_my_big_map[(u32)tile_z * 128 + tile_x];

    dx = (s32)((u32)pos_x - ((u32)tile_x << 8));
    dy = (s32)(((u32)pos_y - ((u32)(s32)p_mapel->Alt << 3)) << 4);
    dz = (s32)((u32)pos_z - ((u32)tile_z << 8));
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

    lgt = (ushort)light_idx;
    intens = (s32)((u32)(s32)game_full_lights[lgt].Intensity * (u32)ratio) >> 8;
    if (intens <= 3)
        return;

    p_qlight = &game_quick_lights[next_quick_light];
    p_qlight->NextQuick = p_mapel->Shade;
    p_mapel->Shade = next_quick_light;
    p_qlight->Light = (ushort)light_idx;
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
    DIFFTEST_ASM("call ASM_quick_light_unkn_func_05\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Adds a new full light at given position, with given intensity,
 * and applies it to map elements and building objects within 7 tiles.
 */
void quick_light_unkn_func_05(int pos_x, int pos_y, int pos_z, int intensity)
{
    struct FullLight *p_fulight;
    short x, y, z;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(quick_light_unkn_func_05,
      quick_light_unkn_func_05_asm(pos_x, pos_y, pos_z, intensity));
    x = (short)pos_x;
    y = (short)pos_y;
    z = (short)pos_z;
    p_fulight = &game_full_lights[next_full_light];
    p_fulight->lgtfld_10 = 0;
    p_fulight->lgtfld_12 = 0;
    p_fulight->Intensity = (short)intensity;
    p_fulight->TrueIntensity = (short)intensity;
    p_fulight->lgtfld_E = (short)intensity;
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
    DIFFTEST_ASM("push 0(%4)\n""call ASM_apply_full_light\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

extern ushort data_17fa52;

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
short word_1552F8 = 1;
s32 dword_176D6C;
ushort data_17fa52;
#endif /* typed asm data */

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

static DIFFTEST_REF_FUNC int process_things_mines_explode_asm(int affected_max)
{
    int ret;
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST
    /* The asm takes the list head from the upper half of the dword at
     * sthings_empty_head, where the original binary had sthings_used_head.
     * In the port that half is padding; fill it for the call so the asm
     * walks the real list, then restore it. Difftest builds only.
     */
    ushort *p_orig_used_head = (ushort *)&sthings_empty_head + 1;
    ushort saved = *p_orig_used_head;
    *p_orig_used_head = (ushort)sthings_used_head;
#endif
    DIFFTEST_ASM("call ASM_process_things_mines_explode\n"
        : "=a" (ret) : "a" (affected_max) : "cc", "memory");
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST
    *p_orig_used_head = saved;
#endif
    return ret;
}

int process_things_mines_explode(int affected_max)
{
    struct SimpleThing *p_sthing;
    s32 thing;
    s32 remain;
    s32 min_ammo;
    int affected_left;

    DIFFTEST_SHADOW_RET(process_things_mines_explode, int,
      process_things_mines_explode_asm(affected_max));
    affected_left = affected_max;
    min_ammo = 600 - (ushort)ingame.fld_unkCB7;
    if (min_ammo < 30)
        min_ammo = 30;
    // TODO: The asm reads a dword at sthings_empty_head and takes its upper
    // half, which was sthings_used_head in the original binary layout. In
    // the port the variables are separate (empty_head is followed by
    // padding), so the asm always sees head 0 and does nothing. The C
    // follows the original intent and deliberately differs from the asm.
    thing = sthings_used_head;
    remain = (ushort)sthings_used;
    while (thing < 0)
    {
        remain--;
        if (remain == -1)
            break;
        p_sthing = &sthings[thing];
        thing = p_sthing->LinkChild;
        if (p_sthing->Type != SmTT_DROPPED_ITEM)
            continue;
        if ((p_sthing->SubType >= WEP_MEDI1) && (p_sthing->SubType <= WEP_MEDI2))
        {
            if ((LbRandomAnyShort() & 0x1) != 0)
                continue;
        }
        if ((s32)(ushort)p_sthing->U.UWeapon.Ammo < min_ammo)
            continue;
        bang_new4(p_sthing->X, p_sthing->Y, p_sthing->Z, 0x37);
        delete_snode(p_sthing);
        remove_sthing(p_sthing->ThingOffset);
        ingame.fld_unkCB7--;
        affected_left--;
        if (affected_left <= 0)
            DIFFTEST_RETURN(1);
    }
    DIFFTEST_RETURN(0);
}

static DIFFTEST_REF_FUNC void process_things_unkn_sub2_asm(int tech_lv, int got_many)
{
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST
    /* Same list head layout issue as in process_things_mines_explode_asm(). */
    ushort *p_orig_used_head = (ushort *)&sthings_empty_head + 1;
    ushort saved = *p_orig_used_head;
    *p_orig_used_head = (ushort)sthings_used_head;
#endif
    DIFFTEST_ASM(
      "call ASM_process_things_unkn_sub2\n"
        : : "a" (tech_lv), "d" (got_many) : "cc", "memory");
#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST
    *p_orig_used_head = saved;
#endif
}

/** Weapon "worth" ranks, copy of data_155d54 in asm. */
static const ubyte drop_weapon_rank[32] = {
    0x00, 0x01, 0x02, 0x05, 0x06, 0x0e, 0x13, 0x04,
    0x08, 0x00, 0x0d, 0x14, 0x12, 0x11, 0x10, 0x0f,
    0x0b, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0e, 0x00,
    0x0a, 0x09, 0x15, 0x07, 0x0b, 0x11, 0x00, 0x00,
};

/** Dropped item StartFrame per weapon type (vtable_e063c in asm). */
static const ushort drop_weapon_start_frame[26] = {
    0x422, 0x3eb, 0x41f, 0x41f, 0x41f, 0x3eb, 0x43f, 0x3eb,
    0x41f, 0x3eb, 0x440, 0x43f, 0x440, 0x43f, 0x3eb, 0x41f,
    0x3eb, 0x41f, 0x3eb, 0x3eb, 0x41f, 0x3eb, 0x3eb, 0x3eb,
    0x3eb, 0x3eb,
};

void process_things_unkn_sub2(int tech_lv, int got_many)
{
    static const ubyte bucket_width[] = {
        2, 2, 3, 4, 6, 5, 4, 4, 6, 8, 7, 10, 13, 13, 15, 8,
        13, 6, 5, 8, 8, 8,
    };
    static const ubyte bucket_wtype[] = {
        0x02, 0x12, 0x1b, 0x07, 0x0b, 0x0a, 0x04, 0x03,
        0x08, 0x0d, 0x0c, 0x1a, 0x0f, 0x05, 0x06, 0x16,
        0x1c, 0x15, 0x19, 0x10, 0x1d, 0x11,
    };
    static const ubyte range_add[] = {4, 13, 13, 21, 36, 42, 21, 8};
    s32 range, rnd, wtype;
    int i;

    DIFFTEST_SHADOW(process_things_unkn_sub2,
      process_things_unkn_sub2_asm(tech_lv, got_many));

    if ((u32)(tech_lv - 1) > 7)
        return;
    // Fall-through switch: sum of increments from the tech level down to 1
    range = 0;
    for (i = tech_lv - 1; i >= 0; i--)
        range += range_add[i];
    if (range == 0)
        return;

    rnd = (s32)(ushort)LbRandomAnyShort() % range;
    if (rnd < 0) {
        wtype = 0x01;
    } else {
        wtype = -1;
        for (i = 0; i < (int)(sizeof(bucket_width)/sizeof(bucket_width[0])); i++)
        {
            if (rnd < bucket_width[i]) {
                wtype = bucket_wtype[i];
                break;
            }
            rnd -= bucket_width[i];
        }
        if (wtype == -1) {
            if (rnd >= 0)
                return;
            wtype = 0x14;
        }
    }

    if (got_many == 0)
    {
        struct SimpleThing *p_item;
        s32 prc_x, prc_z, cor_x, cor_y, cor_z;

        do
        {
            prc_x = (s32)(ushort)LbRandomAnyShort() % 480;
            prc_z = (s32)(ushort)LbRandomAnyShort() % 480;
            prc_x = (prc_x + 16) << 14;
            prc_z = (prc_z + 16) << 14;
        }
        while (!is_this_a_nice_place_for_a_picnic(prc_x, prc_z));
        cor_z = prc_z >> 8;
        cor_x = prc_x >> 8;
        cor_y = alt_at_point((short)cor_x, (short)cor_z) >> 8;
        p_item = create_item(cor_x, cor_y, cor_z, 1004, (ubyte)wtype);
        if (p_item == NULL)
            return;
        if ((wtype == WEP_ELEMINE) || (wtype == WEP_EXPLMINE))
        {
            p_item->Radius = 50;
            p_item->Timer1 = 999;
            p_item->StartTimer1 = 999;
        }
        return;
    }
    else if (got_many == 1)
    {
        struct SimpleThing *p_sthing;
        s32 thing, remain, cur_wtype;

        thing = sthings_used_head;
        remain = (ushort)sthings_used;
        // TODO: The asm walks the list only while the index is positive,
        // but simple thing indices are negative, so the loop never runs.
        while (thing > 0)
        {
            remain--;
            if (remain == -1)
                return;
            p_sthing = &sthings[thing];
            if ((p_sthing->Type == SmTT_DROPPED_ITEM) && ((LbRandomAnyShort() & 0x3) == 0))
            {
                cur_wtype = (ushort)p_sthing->U.UWeapon.WeaponType;
                if ((wtype >= 0) && (wtype <= 31) && (cur_wtype >= 0) && (cur_wtype <= 31)
                  && (drop_weapon_rank[wtype] > drop_weapon_rank[cur_wtype]))
                {
                    ushort sframe;

                    p_sthing->U.UWeapon.WeaponType = wtype;
                    if ((u32)wtype > 25)
                        sframe = 0x3eb;
                    else
                        sframe = drop_weapon_start_frame[wtype];
                    p_sthing->StartFrame = sframe;
                    p_sthing->Frame = nstart_ani[sframe + 1];
                    return;
                }
            }
            thing = p_sthing->LinkChild;
        }
    }
}

static DIFFTEST_REF_FUNC int process_things_bang_asm(int affected_max)
{
    int ret;
    DIFFTEST_ASM("call ASM_process_things_bang\n"
        : "=a" (ret) : "a" (affected_max));
    return ret;
}

int process_things_bang(int affected_max)
{
    struct Thing *p_thing;
    s32 thing;
    s32 remain;
    int affected_left;

    DIFFTEST_SHADOW_RET(process_things_bang, int, process_things_bang_asm(affected_max));
    affected_left = affected_max;
    remain = (ushort)things_used;
    thing = (ushort)things_used_head;
    if (thing <= 0)
        DIFFTEST_RETURN(0);
    do
    {
        remain--;
        if (remain == -1)
            break;
        p_thing = &things[thing];
        thing = p_thing->LinkChild;
        if ((p_thing->Flag & TngF_Destroyed) == 0)
            continue;
        if ((LbRandomAnyShort() & 0x3) != 0)
            continue;
        if (!((p_thing->Type == TT_PERSON)
            && ((p_thing->Flag & TngF_PlayerAgent) == 0)
            && ((ushort)p_thing->State == PerSt_DEAD))
          && !((p_thing->Type == TT_MINE) && (p_thing->SubType == 0x30)))
            continue;
        bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 0x37);
        delete_node(p_thing);
        affected_left--;
        remove_thing((ushort)p_thing->ThingOffset);
        if (affected_left <= 0)
            DIFFTEST_RETURN(1);
    }
    while (thing > 0);
    DIFFTEST_RETURN(0);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void light_cmd_unkn_func_06_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_light_cmd_unkn_func_06\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void light_cmd_unkn_func_06(struct LightCommand *p_lcmd)
{
    ubyte *p_fld = p_lcmd->lcfld_0;
    s32 tile_x, tile_z;
    s32 row_beg, row_end, offs;
    TbBool found;

    DIFFTEST_SHADOW(light_cmd_unkn_func_06,
      light_cmd_unkn_func_06_asm((intptr_t)p_lcmd, 0, 0, 0));
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
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int light_cmd_unkn_func_05_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_light_cmd_unkn_func_05\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Sets Intensity of a full light, or of every full light in the list of
 * a scenery sthing (negative index), to TrueIntensity * intens_mult / 256.
 * The return value is whatever the asm left in eax; no caller uses it.
 */
int light_cmd_unkn_func_05(int light_idx, int intens_mult)
{
    short light = (short)light_idx;
    ushort mult = (ushort)intens_mult;
    struct FullLight *p_fulight;
    u32 ret;

    DIFFTEST_SHADOW_RET(light_cmd_unkn_func_05, int,
      light_cmd_unkn_func_05_asm(light_idx, intens_mult, 0, 0));
    if (light >= 0)
    {
        p_fulight = &game_full_lights[light];
        p_fulight->Intensity = (short)(((s32)mult * p_fulight->TrueIntensity) >> 8);
        // the asm returns the pointer; no caller uses it
        DIFFTEST_RETURN((int)(intptr_t)p_fulight);
    }

    ret = (u32)((s32)light * 15) & 0xFFFF0000;
    light = sthings[light].U.UScenery.LightHead;
    ret |= (ushort)light;
    while (light != 0)
    {
        ret = (u32)(s32)light;
        // TODO the asm checks light > next_full_light, so index next_full_light is accepted
        if ((s32)light > (s32)(ushort)next_full_light)
            break;
        if (light < 0)
            break;
        p_fulight = &game_full_lights[light];
        p_fulight->Intensity = (short)(((s32)mult * p_fulight->TrueIntensity) >> 8);
        light = p_fulight->NextFull;
        ret = ((u32)(uintptr_t)p_fulight & 0xFFFF0000) | (ushort)light;
    }
    DIFFTEST_RETURN((int)ret);
}



static DIFFTEST_REF_FUNC void unkn_update_lights_asm(void)
{
    DIFFTEST_ASM("call ASM_unkn_update_lights\n"
        :  :  : "eax" );
    return;
}

/** Light command field layout (offsets in lcfld_0): 0 flags, 1 type,
 * 2 timer, 3 bright A, 4 bright B, 5 and 6 limits/steps, 7 and 8 last
 * timer values, 0xA current brightness, 0x10 anim tmap, 0x12 light index.
 */
#define LCMD_SHORT(p_fld, offs) (*(short *)&(p_fld)[offs])

static ubyte light_cmd_random_between(ubyte hi, ubyte lo)
{
    s32 range, rnd;

    range = (s32)hi - (s32)lo;
    rnd = (ushort)LbRandomAnyShort();
    return (ubyte)(rnd % range + lo);
}

static void light_cmd_anim_tmap_lookup(ubyte *p_fld)
{
    short light;
    short tile;
    ushort tex;
    short anim;

    light = LCMD_SHORT(p_fld, 0x12);
    if (light < 0)
    {
        struct SimpleThing *p_sthing = &sthings[light];
        tile = (short)((((ushort)(p_sthing->Z >> 16)) << 7)
          + (ushort)(p_sthing->X >> 16));
    }
    else
    {
        struct FullLight *p_fulight = &game_full_lights[light];
        tile = (short)(((p_fulight->Z >> 8) << 7) + (p_fulight->X >> 8));
    }
    tex = game_my_big_map[tile].Texture & 0x3FFF;
    anim = tex;
    if (tex != 0)
    {
        for (anim = 10; anim < (s32)next_anim_tmap; anim++)
        {
            if (game_anim_tmaps[anim].Texture == tex)
                break;
        }
        if (anim >= (s32)next_anim_tmap)
            anim = 0;
    }
    LCMD_SHORT(p_fld, 0x10) = anim;
    if (anim != 0)
        return;

    light = LCMD_SHORT(p_fld, 0x12);
    p_fld[1] = 8;
    if (light < 0)
        sthings[light].U.UScenery.LightHead = 0;
    else
        game_full_lights[light].Command = 0;
}

void unkn_update_lights(void)
{
    ushort cmd;

    DIFFTEST_SHADOW(unkn_update_lights, unkn_update_lights_asm());
    for (cmd = 1; cmd < next_light_command; cmd++)
    {
        ubyte *p_fld;
        short light;
        ubyte flags;

        if (cmd == 0)
            continue;
        p_fld = game_light_commands[cmd].lcfld_0;
        light = LCMD_SHORT(p_fld, 0x12);
        if ((light > 0) && (game_full_lights[light].Command != (short)cmd))
            goto tail;

        switch ((ubyte)(p_fld[1] - 1))
        {
        case 0:
            flags = p_fld[0];
            if ((flags & 0x04) != 0)
            {
                p_fld[2]--;
                if ((sbyte)p_fld[2] >= 0)
                    break;
                if (p_fld[5] == p_fld[6])
                    p_fld[8] = p_fld[6];
                else
                    p_fld[8] = light_cmd_random_between(p_fld[5], p_fld[6]);
                p_fld[2] = p_fld[8];
                light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[3]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x02;
            }
            else if ((flags & 0x02) != 0)
            {
                p_fld[2]--;
                if ((sbyte)p_fld[2] >= 0)
                    break;
                if (p_fld[5] == p_fld[6])
                    p_fld[7] = p_fld[6];
                else
                    p_fld[7] = light_cmd_random_between(p_fld[5], p_fld[6]);
                p_fld[2] = 0;
                light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[4]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x04;
            }
            break;
        case 1:
            if ((p_fld[0] & 0x08) != 0)
            {
                LCMD_SHORT(p_fld, 0xA) = (ushort)LCMD_SHORT(p_fld, 0xA) + p_fld[6];
                if (LCMD_SHORT(p_fld, 0xA) >= (s32)p_fld[3])
                {
                    flags = p_fld[0] & 0xF7;
                    LCMD_SHORT(p_fld, 0xA) = p_fld[3];
                    p_fld[0] = flags;
                }
            }
            else
            {
                LCMD_SHORT(p_fld, 0xA) = (ushort)LCMD_SHORT(p_fld, 0xA) - p_fld[5];
                if (LCMD_SHORT(p_fld, 0xA) <= (s32)p_fld[4])
                {
                    flags = p_fld[0] | 0x08;
                    LCMD_SHORT(p_fld, 0xA) = p_fld[4];
                    p_fld[0] = flags;
                }
            }
            light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12),
              (ushort)LCMD_SHORT(p_fld, 0xA));
            break;
        case 2:
        case 3:
        case 4:
        case 7:
            break;
        case 5:
            flags = p_fld[0];
            if ((flags & 0x04) != 0)
            {
                p_fld[2]--;
                if ((sbyte)p_fld[2] >= 0)
                    break;
                p_fld[8] = p_fld[6];
                p_fld[2] = p_fld[6];
                if ((sbyte)p_fld[6] > 0)
                    light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[3]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x02;
            }
            else if ((flags & 0x02) != 0)
            {
                p_fld[2]--;
                if ((sbyte)p_fld[2] >= 0)
                    break;
                p_fld[7] = p_fld[5];
                p_fld[2] = p_fld[5];
                if ((sbyte)p_fld[5] > 0)
                    light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[4]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x04;
            }
            break;
        case 6:
            if (LCMD_SHORT(p_fld, 0x10) >= (s32)next_anim_tmap)
                LCMD_SHORT(p_fld, 0x10) = 0;
            if (LCMD_SHORT(p_fld, 0x10) == 0)
            {
                light_cmd_anim_tmap_lookup(p_fld);
                break;
            }
            flags = p_fld[0];
            if ((flags & 0x04) != 0)
            {
                s32 frame;

                frame = game_anim_tmaps[LCMD_SHORT(p_fld, 0x10)].field_22 + 1;
                if ((frame != p_fld[6]) && (frame != p_fld[5]))
                    break;
                light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[3]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x02;
            }
            else if ((flags & 0x02) != 0)
            {
                s32 frame;

                frame = game_anim_tmaps[LCMD_SHORT(p_fld, 0x10)].field_22 + 1;
                if ((frame == p_fld[6]) || (frame == p_fld[5]))
                    break;
                light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), p_fld[4]);
                flags = p_fld[0] & 0xF9;
                p_fld[0] = flags;
                p_fld[0] = flags | 0x04;
            }
            break;
        default:
            light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), 0x100);
            break;
        }
tail:
        if ((p_fld[0] & 0x10) != 0)
            light_cmd_unkn_func_06(&game_light_commands[cmd]);
        if ((p_fld[0] & 0x20) != 0)
            light_cmd_unkn_func_05(LCMD_SHORT(p_fld, 0x12), 0);
    }
}

#undef LCMD_SHORT

int recalc_rocket_direction(struct Thing *p_rocket)
{
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

static DIFFTEST_REF_FUNC void process_rocket_asm(struct Thing *p_rocket)
{
    DIFFTEST_ASM(
      "call ASM_process_rocket\n"
        : : "a" (p_rocket));
}

void process_rocket(struct Thing *p_rocket)
{
    //TODO modify to make the rocket disappear when falling into sludge
    struct Thing *p_target;
    struct Thing *p_owner;
    struct SimpleThing *p_sthing;
    s32 new_x, new_y, new_z;
    s32 neg_vz, vx;
    s32 dx, dz, dist;
    s32 cs, ox, tmp, tz;
    int ground, elev;
    int angle, alt;
    int x, z;
    ushort frame, rnd;
    ushort col;
    ubyte check_sea;

    DIFFTEST_SHADOW(process_rocket, process_rocket_asm(p_rocket));

    check_sea = 0;
    ground = 0;
    p_rocket->Timer1--;
    if (p_rocket->Timer1 == 0)
    {
        stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
        goto finish;
    }

    p_target = p_rocket->PTarget;
    if ((p_target >= &things[0]) && (p_target < &things[1000]))
    {
        ubyte tgt_grp, own_grp;

        p_owner = &things[(ushort)p_rocket->Owner];
        tgt_grp = p_target->U.UPerson.EffectiveGroup;
        own_grp = p_owner->U.UPerson.EffectiveGroup;
        if (own_grp == tgt_grp)
            p_rocket->PTarget = NULL;
        // x86 shl masks the count to 5 bits
        else if ((war_flags[own_grp & 0x7f].Truce & (1u << ((tgt_grp & 0x7f) & 0x1f))) != 0)
            p_rocket->PTarget = NULL;
    }
    p_target = p_rocket->PTarget;
    if ((p_target >= &things[0]) && (p_target < &things[1000]))
    {
        if ((ushort)p_target->State == PerSt_PERSON_BURNING)
            p_rocket->PTarget = NULL;
    }

    if ((p_rocket->SubType == 1) && (p_rocket->PTarget != NULL) &&
      (((gameturn + (ushort)p_rocket->ThingOffset) & 1) == 0))
    {
        if (!recalc_rocket_direction(p_rocket))
        {
            stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
            goto finish;
        }
    }

    new_x = p_rocket->X + ((p_rocket->Speed * p_rocket->VX) >> 2);
    new_y = p_rocket->Y + ((p_rocket->Speed * p_rocket->VY) >> 2);
    new_z = p_rocket->Z + ((p_rocket->Speed * p_rocket->VZ) >> 2);

    if ((new_x >> 16) < 0 || (new_x >> 16) >= 0x80 ||
      (new_z >> 16) < 0 || (new_z >> 16) >= 0x80)
    {
        stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
        goto finish;
    }

    alt = alt_at_point((short)(new_x >> 8), (short)(new_z >> 8));
    ground = alt >> 8;
    if ((new_y >> 5) + 0x19 < ground * 8 + 0x50)
    {
        struct MyMapElement *p_mapel;

        p_mapel = &game_my_big_map[(new_z >> 16) * 128 + (new_x >> 16)];
        if ((p_mapel->Flags2 & 0x05) != 0)
        {
            check_sea = 1;
            stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
            goto finish;
        }
    }
    if ((new_y >> 5) < ground * 8 + 0x50)
        new_y = (ground << 8) + 0xa00;

    col = check_col_collision(new_x >> 8, (new_y >> 8) + 8, new_z >> 8);
    if ((col == 0) && (((p_rocket->Flag & TngF_ShootAtPos) != 0) || (p_rocket->SubType == 1)))
    {
        dx = abs((p_rocket->X >> 8) - p_rocket->U.UEffect.GotoX);
        dz = abs((p_rocket->Z >> 8) - p_rocket->U.UEffect.GotoZ);
        if (dx > dz)
            dist = dx + (dz >> 1);
        else
            dist = (dx >> 1) + dz;
        if ((dist < 0x80) && ((p_rocket->Flag & TngF_ShootAtPos) != 0))
        {
            col = 0xffff;
            check_sea = 1;
        }
    }
    if (col != 0)
    {
        stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
        goto finish;
    }

    if ((new_x >> 16) < 0 || (new_x >> 16) >= 0x80 ||
      (new_z >> 16) < 0 || (new_z >> 16) >= 0x80)
        return;

    move_mapwho(p_rocket, new_x, new_y, new_z);

    p_target = p_rocket->PTarget;
    if (p_target != NULL)
    {
        if ((p_target->Flag & TngF_Destroyed) == 0)
        {
            struct Thing *p_hit;

            p_owner = &things[(ushort)p_rocket->Owner];
            p_hit = check_for_radius_hit_person(p_rocket->X, p_rocket->Y,
              p_rocket->Z, (ushort)p_rocket->Radius, p_owner, 1, 0);
            if (p_hit != NULL)
            {
                stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
                p_owner = &things[(ushort)p_rocket->Owner];
                thing_hit_by_bullet(p_hit, (short)weapon_defs[WEP_RAP].HitDamage,
                  (short)p_rocket->VX, (short)p_rocket->VY, (short)p_rocket->VZ,
                  p_owner, 6);
                goto finish;
            }
        }
        else
        {
            s32 dy;

            dy = (p_target->Y - p_rocket->Y) >> 8;
            if ((dy < 25) && (dy > -25))
            {
                dx = abs((p_target->X - p_rocket->X) >> 8);
                dz = abs((p_target->Z - p_rocket->Z) >> 8);
                if (dx > dz)
                    dist = dx + (dz >> 1);
                else
                    dist = (dx >> 1) + dz;
                if (dist < (ushort)p_target->Radius + (ushort)p_rocket->Radius)
                {
                    stop_sample_using_heap((ushort)p_rocket->ThingOffset, 0x18);
                    p_owner = &things[(ushort)p_rocket->Owner];
                    thing_hit_by_bullet(p_rocket->PTarget, (short)weapon_defs[WEP_RAP].HitDamage,
                      (short)p_rocket->VX, (short)p_rocket->VY, (short)p_rocket->VZ,
                      p_owner, 6);
                    goto finish;
                }
            }
        }
    }

    // Rocket still flying - emit smoke trail
    neg_vz = -p_rocket->VZ;
    vx = p_rocket->VX;
    for (angle = 0x100; angle < 0x900; angle += 0x200)
    {
        x = p_rocket->X >> 8;
        elev = lbSinTable[angle & 0x7ff] >> 10;
        z = p_rocket->Z >> 8;
        if (angle <= 0x400)
            frame = 0x44d;
        else
            frame = 0x44f;

        p_sthing = create_scale_effect(x, p_rocket->Y >> 8, z, frame, 0x20);
        if (p_sthing != NULL)
        {
            cs = lbSinTable[(angle & 0x7ff) + 512];
            tz = neg_vz * cs;
            ox = (vx * cs) >> 15;
            rnd = LbRandomAnyShort();
            p_sthing->SubType = 0x31;
            p_sthing->Object = 0x100;
            p_sthing->StartFrame = 0;
            p_sthing->Timer1 = p_sthing->Timer1 + (s32)(rnd & 0x7) - 4;
            tz >>= 15;
            tmp = ((p_rocket->Speed * p_rocket->VX) >> 5) + tz;
            rnd = LbRandomAnyShort();
            p_sthing->U.UEffect.VX = ((rnd & 0x1f) << 4) - 0x100 + tmp;
            tmp = ((p_rocket->Speed * p_rocket->VY) >> 5) + elev;
            rnd = LbRandomAnyShort();
            p_sthing->U.UEffect.VY = (s32)(rnd & 0x1f) - 0x10 + tmp;
            tmp = ox + ((p_rocket->Speed * p_rocket->VZ) >> 5);
            rnd = LbRandomAnyShort();
            p_sthing->U.UEffect.OX = -(tz >> 3);
            p_sthing->U.UEffect.OY = -(elev >> 3);
            p_sthing->U.UEffect.VZ = ((rnd & 0x1f) << 4) - 0x100 + tmp;
            p_sthing->U.UEffect.OZ = -(ox >> 3);
            p_sthing->Owner2 = p_rocket->Owner;
        }

        p_sthing = create_scale_effect(x, p_rocket->Y >> 8, z, (ushort)(frame - 1), 0x20);
        if (p_sthing != NULL)
        {
            cs = lbSinTable[(angle & 0x7ff) + 512];
            tz = neg_vz * cs;
            ox = (vx * cs) >> 16;
            rnd = LbRandomAnyShort();
            p_sthing->SubType = 0x31;
            p_sthing->Object = 0x100;
            p_sthing->StartFrame = 0;
            p_sthing->Timer1 = p_sthing->Timer1 + (s32)(rnd & 0x7) - 4;
            tz >>= 16;
            tmp = ((p_rocket->Speed * p_rocket->VX) >> 5) + tz;
            rnd = LbRandomAnyShort();
            p_sthing->U.UEffect.VX = ((rnd & 0x1f) << 4) - 0x100 + tmp;
            tmp = p_rocket->Speed * p_rocket->VY;
            elev >>= 1;
            rnd = LbRandomAnyShort();
            tmp = (tmp >> 5) + elev;
            p_sthing->U.UEffect.VY = (s32)(rnd & 0x1f) - 0x10 + tmp;
            tmp = ox + ((p_rocket->Speed * p_rocket->VZ) >> 5);
            rnd = LbRandomAnyShort();
            p_sthing->U.UEffect.OX = -(tz >> 3);
            p_sthing->U.UEffect.OY = -(elev >> 3);
            p_sthing->U.UEffect.VZ = ((rnd & 0x1f) << 4) - 0x100 + tmp;
            p_sthing->U.UEffect.OZ = -(ox >> 3);
            p_sthing->Owner2 = p_rocket->Owner;
        }
    }
    return;

finish:
    if (check_sea)
    {
        struct MyMapElement *p_mapel;

        p_mapel = &game_my_big_map[(p_rocket->Z >> 16) * 128 + (p_rocket->X >> 16)];
        if (sea_texture((short)(p_mapel->Texture & 0x3fff)))
        {
            p_sthing = create_scale_effect(p_rocket->X >> 8, ground,
              p_rocket->Z >> 8, 0x43f, 8);
            if (p_sthing != NULL)
            {
                rnd = LbRandomAnyShort();
                p_sthing->SubType = 0x3c;
                p_sthing->Object = (rnd & 0x7f) + 0x3e;
                create_sound_effect(p_rocket->X >> 8, 0, p_rocket->Z >> 8, 3, 0, 0);
            }
            remove_thing((ushort)(p_rocket - things));
            delete_node(p_rocket);
            return;
        }
    }

    alt = alt_at_point((short)(p_rocket->X >> 8), (short)(p_rocket->Z >> 8));
    if (abs(p_rocket->Y - alt) < 20000)
    {
        alt = alt_at_point((short)(p_rocket->X >> 8), (short)(p_rocket->Z >> 8));
        FIRE_new(p_rocket->X, alt >> 5, p_rocket->Z, 1);
    }
    bang_new4(p_rocket->X, p_rocket->Y, p_rocket->Z, 0x5f);
    create_sound_effect(p_rocket->X >> 8, p_rocket->Y >> 8, p_rocket->Z >> 8, 0x25, 0x7f, 0);
    p_owner = &things[(ushort)p_rocket->Owner];
    do_shockwave(p_rocket->X >> 8, p_rocket->Y >> 8, p_rocket->Z >> 8, 0x300, 1000, p_owner);
    remove_thing((ushort)(p_rocket - things));
    delete_node(p_rocket);
}

static DIFFTEST_REF_FUNC void process_laser_guided_asm(struct Thing *p_laser)
{
    DIFFTEST_ASM(
      "call ASM_process_laser_guided\n"
        : : "a" (p_laser));
}

void process_laser_guided(struct Thing *p_laser)
{
    struct Thing *p_target;
    struct Thing *p_owner;
    s32 dx, dz, dist;
    s32 new_x, new_z;
    s32 sn;
    short angle, rate;
    short glow_size;

    DIFFTEST_SHADOW(process_laser_guided, process_laser_guided_asm(p_laser));

    glow_size = 0x40;
    p_target = p_laser->PTarget;
    if (p_target == NULL)
    {
        remove_thing((ushort)(p_laser - things));
        delete_node(p_laser);
        return;
    }

    dx = (p_laser->X - p_target->X) >> 8;
    dz = (p_laser->Z - p_target->Z) >> 8;
    dist = dx * dx + dz * dz;
    if (dist < 0x4000)
    {
        p_owner = &things[(ushort)p_laser->Owner];
        angle = p_laser->U.UEffect.AngleY;
        thing_hit_by_bullet(p_target, 1000,
          (short)(lbSinTable[angle] >> 5), 0,
          (short)(lbSinTable[angle + 512] >> 5), p_owner, 3);
    }

    p_laser->Timer1--;
    if ((p_laser->Timer1 == 0) || (dist < 0x4000))
    {
        remove_thing((ushort)(p_laser - things));
        delete_node(p_laser);
        return;
    }

    p_target = p_laser->PTarget;
    angle = arctan((p_target->X >> 8) - (p_laser->X >> 8),
      -((p_target->Z >> 8) - (p_laser->Z >> 8)));
    rate = calc_angle_rate(angle, p_laser->U.UEffect.AngleY, &glow_size);
    p_laser->U.UEffect.AngleY = (p_laser->U.UEffect.AngleY + rate) & 0x7ff;

    angle = p_laser->U.UEffect.AngleY;
    sn = lbSinTable[angle];
    new_x = p_laser->X + ((sn * glow_size) >> 8);
    new_z = p_laser->Z + ((lbSinTable[angle + 512] * glow_size) >> 8);
    move_mapwho(p_laser, new_x, p_laser->Y, new_z);
}

int mounted_los(int x1, int y1, int z1, int x2, int y2, int z2);
ThingIdx aquire_target(struct Thing *p_gun);

static DIFFTEST_REF_FUNC void process_c_iff_asm(struct Thing *p_mine)
{
    DIFFTEST_ASM(
      "call ASM_process_c_iff\n"
        : : "a" (p_mine));
}

void process_c_iff(struct Thing *p_mine)
{
    struct Thing *p_target;
    int dx, dz, dist;

    DIFFTEST_SHADOW(process_c_iff, process_c_iff_asm(p_mine));
    if ((p_mine->Flag & TngF_Destroyed) != 0)
        return;
    if (p_mine->U.UPerson.WeaponTurn > 3)
        p_mine->U.UPerson.WeaponTurn = 0;
    process_weapon(p_mine);

    p_target = p_mine->PTarget;
    if (p_target == NULL)
    {
        if (((gameturn + (ushort)p_mine->ThingOffset) & 0x07) == 0)
            aquire_target(p_mine);
        return;
    }

    dx = abs((p_mine->X >> 8) - (p_target->X >> 8));
    dz = abs((p_mine->Z >> 8) - (p_target->Z >> 8));
    if (dx > dz)
        dist = dx + (dz >> 1);
    else
        dist = dz + (dx >> 1);
    if (dist >= get_weapon_range(p_mine))
    {
        p_mine->PTarget = NULL;
        return;
    }

    if (!mounted_los(p_mine->X >> 8, (p_mine->Y >> 8) + 20, p_mine->Z >> 8,
      p_target->X >> 8, (p_target->Y >> 8) + 20, p_target->Z >> 8))
        return;
    p_mine->Flag |= TngF_TriggerUse;
    if ((p_mine->Flag & TngF_InVehicle) != 0)
        things[p_mine->U.UPerson.Vehicle].Flag |= TngF_Unkn01000000;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
struct SimpleThing *create_growing_e_strand(int x, int y, int z,
  struct Thing *p_thing, int health, int owner);

static DIFFTEST_REF_FUNC void init_electricity_bomb_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_init_electricity_bomb\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void init_electricity_bomb(int x, int y, int z, int owner_idx)
{
    ushort owner = (ushort)owner_idx;
    ThingIdx person;
    int dist;
    ushort i;

    DIFFTEST_SHADOW(init_electricity_bomb, init_electricity_bomb_asm(x, y, z, owner_idx));
    dist = 0;
    for (i = 0; i < 4; i++)
    {
        // The previous distance found is given as minimal distance
        person = find_nearest_person_min(x, y, z, dist, &dist, -1, -1);
        if (dist >= 0x2fa000)
            continue;
        if (person == 0)
            continue;
        create_growing_e_strand(x, y, z, &things[(ushort)person], 4, owner);
    }
    do_shockwave(x, y, z, -700, 1, NULL);
}


struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z);

static DIFFTEST_REF_FUNC void process_mine_asm(struct SimpleThing *p_mine)
{
    DIFFTEST_ASM(
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

static DIFFTEST_REF_FUNC void process_laser_elec_asm(struct Thing *p_elec)
{
    DIFFTEST_ASM(
      "call ASM_process_laser_elec\n"
        : : "a" (p_elec));
}

void process_laser_elec(struct Thing *p_elec)
{
    DIFFTEST_SHADOW(process_laser_elec, process_laser_elec_asm(p_elec));
    p_elec->Timer1--;
    if (p_elec->Timer1 == 0)
    {
        remove_thing((ushort)(p_elec - things));
        delete_node(p_elec);
        return;
    }
    if ((p_elec->State == 0) || ((gameturn & 7) == 0))
    {
        struct SimpleThing *p_spark;

        // Beam end point is stored in velocity fields
        p_spark = init_spark(p_elec->VX, p_elec->VY, p_elec->VZ);
        if (p_spark != NULL)
            p_spark->Object = colour_lookup[4];
    }
    if ((p_elec->State != 0) && (p_elec->Timer1 > 10))
    {
        p_elec->VX -= ((3 * p_elec->U.UEffect.GotoX
          * p_elec->U.UEffect.GotoY) >> 6) / 20;
        p_elec->VZ -= ((3 * p_elec->U.UEffect.GotoZ
          * p_elec->U.UEffect.GotoY) >> 6) / 20;
    }
}

void process_razor_wire(struct Thing *p_thing)
{
    ;
}

void process_air_strike(struct Thing *p_thing)
{
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
    DIFFTEST_ASM(
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

static DIFFTEST_REF_FUNC struct SimpleThing *create_growing_e_strand_asm(int x, int y, int z,
  struct Thing *p_thing, int health, int owner)
{
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];
    int rd = y, rb = z, rc = (int)(intptr_t)p_thing;

    stkargs[0] = (int)(intptr_t)health;
    stkargs[1] = (int)(intptr_t)owner;

    DIFFTEST_ASM(
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_create_growing_e_strand\n"
        : "=a" (ret), "+d" (rd), "+b" (rb), "+c" (rc)
        : "S" (stkargs), "0" ((s32)x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_growing_e_strand(int x, int y, int z,
  struct Thing *p_thing, int health, int owner)
{
    struct SimpleThing *p_sthing;
    ThingIdx sthing;
    int tx, ty, tz;
    int damage;

    DIFFTEST_SHADOW_RET(create_growing_e_strand, struct SimpleThing *,
      create_growing_e_strand_asm(x, y, z, p_thing, health, owner));
    sthing = get_new_sthing();
    tx = 0;
    ty = 0;
    tz = 0;
    if (sthing == 0)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_thing->Flag2 |= TgF2_Unkn0020;
    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        tx = LbRandomAnyShort() % 320 - 160;
        ty = LbRandomAnyShort() % 320 - 160;
        tz = LbRandomAnyShort() % 320 - 160;
        damage = 5000;
        // Only the low byte is modified by the original code
        things[p_thing->U.UPerson.Vehicle].U.UVehicle.WorkPlace |= 0x80;
    }
    else
    {
        damage = weapon_defs[WEP_ELEMINE].HitDamage;
    }
    tx += p_thing->X >> 8;
    // TODO the original uses a different shift for Y; possibly a bug
    ty += p_thing->Y >> 5;
    tz += p_thing->Z >> 8;

    p_sthing = &sthings[sthing];
    p_sthing->X = x << 8;
    p_sthing->Z = z << 8;
    p_sthing->Y = y << 8;
    p_sthing->U.UEffect.VX = tx;
    p_sthing->U.UEffect.VY = ty;
    p_sthing->U.UEffect.VZ = tz;
    add_node_sthing(p_sthing->ThingOffset);
    p_sthing->Timer1 = health;
    p_sthing->StartTimer1 = health;
    p_sthing->Type = SmTT_ELECTRIC_STRAND;
    p_sthing->SubType = 6;
    p_sthing->Object = arctan(tx - x, z - tz);
    p_sthing->Radius = p_thing->ThingOffset;
    p_sthing->Owner2 = owner;
    play_dist_ssample(p_sthing, 0x28, 0x7F, 0x40, 100, 0, 1);

    thing_hit_by_bullet(p_thing, (short)damage, (short)(x - tx),
      (short)(y - ty), (short)(z - tz), &things[(ushort)owner], 10);
    if ((p_thing->Flag & TngF_Persuaded) != 0)
        stop_being_persuaded(p_thing);

    DIFFTEST_RETURN(p_sthing);
}

static DIFFTEST_REF_FUNC void create_another_strand_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_create_another_strand\n"
        : : "a" (p_sthing));
}

void create_another_strand(struct SimpleThing *p_sthing)
{
    short cand_vert[100];
    short cand_face[100];
    struct SingleObject *p_sobj;
    struct SimpleThing *p_strand;
    short *p_pointno;
    struct SinglePoint *p_pt;
    short cur_face;
    short count;
    short face, vert;
    short i, k;
    ubyte sound;
    u32 rnd;
    int map_x, map_z, ofs_y;

    DIFFTEST_SHADOW(create_another_strand, create_another_strand_asm(p_sthing));
    cur_face = p_sthing->U.UEffect.OY;
    p_sobj = &game_objects[p_sthing->U.UEffect.OX];
    map_x = p_sobj->MapX;
    map_z = p_sobj->MapZ;
    ofs_y = p_sobj->OffsetY;
    count = 0;

    // Collect vertices following the current point on faces sharing it
    if (p_sobj->NumbFaces != 0)
    {
        struct SingleObjectFace3 *p_face3;

        p_face3 = &game_object_faces3[p_sobj->StartFace];
        for (i = p_sobj->StartFace;
          i < (int)p_sobj->StartFace + (int)p_sobj->NumbFaces;
          i++, p_face3++)
        {
            if (i == cur_face)
                continue;
            for (k = 0; k < 3; k++)
            {
                if ((ushort)p_face3->PointNo[k] != p_sthing->U.UEffect.OZ)
                    continue;
                cand_face[count] = i;
                cand_vert[count] = k + 1;
                count++;
                cand_face[count] = i;
                cand_vert[count] = k + 2;
                count++;
            }
        }
    }
    if (p_sobj->NumbFaces4 != 0)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[p_sobj->StartFace4];
        for (i = p_sobj->StartFace4;
          i < (int)p_sobj->StartFace4 + (int)p_sobj->NumbFaces4;
          i++, p_face4++)
        {
            if (i == -cur_face)
                continue;
            for (k = 0; k < 4; k++)
            {
                if ((ushort)p_face4->PointNo[k] != p_sthing->U.UEffect.OZ)
                    continue;
                cand_face[count] = -i;
                cand_vert[count] = k + 1;
                count++;
                cand_face[count] = -i;
                cand_vert[count] = k + 2;
                count++;
                cand_face[count] = -i;
                cand_vert[count] = k + 3;
                count++;
            }
        }
    }

    if (count <= 0)
        return;

    rnd = ((u32)(ushort)LbRandomAnyShort() + (u32)gameturn) % (u32)(s32)count;
    face = cand_face[(short)rnd];
    vert = cand_vert[(short)rnd];
    // TODO the wrap-around does not give vertex 0 (3 -> 1 on triangles,
    // 4..6 -> 1..3 on quads); kept as in the original
    if (face > 0)
    {
        if (vert > 2)
            vert -= 2;
        p_pointno = &game_object_faces3[face].PointNo[vert];
    }
    else
    {
        if (vert > 3)
            vert -= 3;
        p_pointno = &game_object_faces4[-face].PointNo[vert];
    }
    sound = gameturn & 1;
    p_pt = &game_object_points[(ushort)*p_pointno];
    p_strand = create_electric_strand(p_sthing->U.UEffect.VX,
      p_sthing->U.UEffect.VY, p_sthing->U.UEffect.VZ,
      p_pt->X + map_x, p_pt->Y + ofs_y, p_pt->Z + map_z, sound);
    if (p_strand == NULL)
        return;

    p_strand->U.UEffect.OX = p_sthing->U.UEffect.OX;
    p_strand->U.UEffect.OY = face;
    p_strand->U.UEffect.OZ = *p_pointno;
    p_strand->StartTimer1 = p_sthing->StartTimer1 + 1;
}

static DIFFTEST_REF_FUNC void process_electric_strand_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_electric_strand\n"
        : : "a" (p_sthing));
}

void process_electric_strand(struct SimpleThing *p_sthing)
{
    short timer;

    DIFFTEST_SHADOW(process_electric_strand, process_electric_strand_asm(p_sthing));
    if (p_sthing->SubType == 0)
    {
        timer = p_sthing->Timer1;
        p_sthing->Timer1 = timer - 1;
        if (timer == 0)
        {
            // TODO the asm keeps processing the sthing after deleting it
            delete_snode(p_sthing);
            remove_sthing(p_sthing->ThingOffset);
        }
        if (p_sthing->Timer1 == 4 && p_sthing->StartTimer1 < 15)
            create_another_strand(p_sthing);
    }
    else if (p_sthing->SubType == 6)
    {
        ThingIdx person;
        int dist;

        timer = p_sthing->Timer1;
        p_sthing->Timer1 = timer - 1;
        if (timer == 0)
        {
            things[(ushort)p_sthing->Radius].Flag2 &= ~TgF2_Unkn0020;
            delete_snode(p_sthing);
            remove_sthing(p_sthing->ThingOffset);
            return;
        }
        if (p_sthing->Timer1 != p_sthing->StartTimer1 - 1)
        {
            if ((LbRandomAnyShort() & 0xF) >= 7)
                return;
        }
        dist = 0;
        person = find_nearest_person_min(p_sthing->X >> 8, p_sthing->Y >> 8,
          p_sthing->Z >> 8, 0, &dist, p_sthing->Object, -1);
        if (person == 0)
            return;
        if (dist >= 0x200000)
            return;
        create_growing_e_strand(p_sthing->U.UEffect.VX, p_sthing->U.UEffect.VY,
          p_sthing->U.UEffect.VZ, &things[person],
          (ushort)p_sthing->Timer1, (ushort)p_sthing->Owner2);
    }
}

static DIFFTEST_REF_FUNC void process_canister_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_canister\n"
        : : "a" (p_sthing));
}

void set_dist_volume(struct SimpleThing *p_sthing,
  int smptbl_id, ushort vol);

void process_canister(struct SimpleThing *p_sthing)
{
    s32 x, y, z;
    s32 step;
    int smptbl_id;

    DIFFTEST_SHADOW(process_canister, process_canister_asm(p_sthing));
    x = p_sthing->X >> 8;
    y = p_sthing->Y >> 8;
    z = p_sthing->Z >> 8;
    p_sthing->Radius = 0;
    set_nuclear_shade_timer(p_sthing->Timer1);

    for (step = 0; step != 0x800; step += 0x200)
    {
        struct SimpleThing *p_effect;
        s32 angle, vx, vz, rnd_x, rnd_z;
        short timer;

        angle = ((p_sthing->Timer1 << 6) + step) & 0x7ff;
        vx = (lbSinTable[angle + 512] >> 4) - (lbSinTable[angle + 512] >> 6);
        vz = (lbSinTable[angle] >> 4) - (lbSinTable[angle] >> 6);
        rnd_x = (s32)LbRandomAnyShort() % 800;
        vx += rnd_x - 400;
        rnd_z = (s32)LbRandomAnyShort() % 800;
        timer = (LbRandomAnyShort() & 0xf) + 20;
        vz += rnd_z - 400;

        p_effect = create_scale_effect(x, y, z, 0x398, timer);
        if (p_effect == NULL)
            continue;

        p_effect->SubType = 0x1e;
        p_effect->Object = 0x20;
        p_effect->StartFrame = 0xc;
        p_effect->U.UEffect.VY = 8;
        p_effect->U.UEffect.OY = 0;
        p_effect->Frame = frame[frame[(ushort)p_effect->Frame].Next].Next;
        p_effect->U.UEffect.VX = vx;
        p_effect->U.UEffect.VZ = vz;
        p_effect->U.UEffect.OX = (-vx) >> 5;
        p_effect->U.UEffect.OZ = (-vz) >> 5;
        p_effect->Owner2 = p_sthing->Owner2;
        if (p_sthing->SubType == 4)
        {
            p_effect->Flag |= 0x10;
        }
        else if (p_sthing->SubType == 5)
        {
            p_effect->Flag |= 0x20;
            p_effect->Frame = frame[frame[nstart_ani[908]].Next].Next;
        }
    }

    p_sthing->Timer1++;
    smptbl_id = (p_sthing->SubType == 4) ? 0x20 : 0x1f;
    if (p_sthing->Timer1 < 10)
    {
        set_dist_volume(p_sthing, smptbl_id,
          (ushort)(p_sthing->Timer1 * 10));
    }
    if (p_sthing->Timer1 > 190)
    {
        set_dist_volume(p_sthing, smptbl_id,
          (ushort)((0x12d - p_sthing->Timer1) * 10));
    }
    if (p_sthing->Timer1 > 200)
    {
        stop_sample_using_heap(p_sthing->ThingOffset, smptbl_id);
        remove_sthing(p_sthing->ThingOffset);
        delete_snode(p_sthing);
        *(u32 *)&nuclear_overexposure = 0;
    }
}

static DIFFTEST_REF_FUNC void process_stasis_pod_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_stasis_pod\n"
        : : "a" (p_sthing));
}

void find_and_stasis(struct SimpleThing *p_stasis, int radius);

static DIFFTEST_REF_FUNC void set_dist_volume_asm(struct SimpleThing *p_sthing,
  int smptbl_id, ushort vol)
{
    int a1 = (int)(intptr_t)p_sthing, a2 = smptbl_id, a3 = vol;

    DIFFTEST_ASM("call ASM_set_dist_volume\n"
        : "+a" (a1), "+d" (a2), "+b" (a3) : : "memory", "cc");
}

/** Sets the volume of a sample played by a simple thing, attenuated
 * by the thing's approximate distance from the engine camera centre.
 */
void set_dist_volume(struct SimpleThing *p_sthing,
  int smptbl_id, ushort vol)
{
    s32 dx, dz, adx, adz, dist, volume;

    DIFFTEST_SHADOW(set_dist_volume,
      set_dist_volume_asm(p_sthing, smptbl_id, vol));
    dx = (p_sthing->X >> 8) - engn_xc;
    dz = (p_sthing->Z >> 8) - engn_zc;
    adx = abs(dx);
    adz = abs(dz);
    if (adz < adx)
        dist = adx + (adz >> 1);
    else
        dist = adz + (adx >> 1);

    if (dist < 0xa00)
        volume = ((s32)vol * 127) >> 7;
    else if (dist >= 0x1a00)
        volume = 0;
    else
        volume = ((127 - ((dist - 0xa00) >> 5)) * (s32)vol) >> 7;

    SetSampleVolume((short)p_sthing->ThingOffset, (short)smptbl_id,
      (short)volume);
}

void process_stasis_pod(struct SimpleThing *p_sthing)
{
    DIFFTEST_SHADOW(process_stasis_pod, process_stasis_pod_asm(p_sthing));
    p_sthing->Timer1--;
    switch ((ushort)p_sthing->State)
    {
    case 0:
        p_sthing->Radius += 0x10;
        if (!IsSamplePlaying(p_sthing->ThingOffset, 0x2c, NULL))
            play_dist_ssample(p_sthing, 0x2c,
              (ushort)((0x32 - (ushort)p_sthing->Timer1) * 2), 0x40, 100, -1, 1);
        else
            set_dist_volume(p_sthing, 0x2c,
              (ushort)((0x32 - p_sthing->Timer1) * 2));
        if (p_sthing->Timer1 < 0)
        {
            p_sthing->Timer1 = 100;
            p_sthing->State++;
        }
        find_and_stasis(p_sthing, (ushort)p_sthing->Radius);
        break;
    case 1:
        if (p_sthing->Timer1 < 0)
        {
            p_sthing->Timer1 = 0x17;
            p_sthing->State++;
        }
        find_and_stasis(p_sthing, (ushort)p_sthing->Radius);
        break;
    case 2:
        if (p_sthing->Timer1 > 0)
        {
            if (!IsSamplePlaying(p_sthing->ThingOffset, 0x2c, NULL))
                play_dist_ssample(p_sthing, 0x2c,
                  (ushort)((0x32 - (ushort)p_sthing->Timer1) * 2), 0x40, 100, -1, 1);
            else
                set_dist_volume(p_sthing, 0x2c,
                  (ushort)(p_sthing->Timer1 << 2));
        }
        p_sthing->Radius -= 0x20;
        if (p_sthing->Timer1 < 0)
        {
            p_sthing->Timer1 = 0x19;
            p_sthing->State++;
        }
        break;
    case 3:
        stop_sample_using_heap(p_sthing->ThingOffset, 0x2c);
        delete_snode(p_sthing);
        remove_sthing(p_sthing->ThingOffset);
        break;
    default:
        break;
    }
}

static DIFFTEST_REF_FUNC void find_and_stasis_asm(struct SimpleThing *p_stasis, int radius)
{
    DIFFTEST_ASM("call ASM_find_and_stasis\n"
        :  : "a" (p_stasis), "d" (radius));
}

void find_and_stasis(struct SimpleThing *p_stasis, int radius)
{
    s32 cor_x, cor_y, cor_z;
    s32 rng;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(find_and_stasis, find_and_stasis_asm(p_stasis, radius));
    cor_x = p_stasis->X >> 8;
    cor_y = p_stasis->Y >> 8;
    cor_z = p_stasis->Z >> 8;
    rng = (radius >> 8) & 0xFFFF;
    if (rng < 1)
        rng = 1;
    tile_x = cor_x >> 8;
    tile_z = cor_z >> 8;

    for (dx = -rng; dx <= (s32)(ushort)rng; dx++)
    {
        for (dz = -rng; dz <= (s32)(ushort)rng; dz++)
        {
            s32 mx, mz;
            short thing;

            mx = tile_x + dx;
            if ((mx <= 0) || (mx >= 128))
                continue;
            mz = tile_z + dz;
            if ((mz <= 0) || (mz >= 128))
                continue;
            thing = game_my_big_map[(mz << 7) + mx].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                s32 dy, adx, adz, dist;

                if (thing <= 0)
                {
                    // The asm also computes distances to simple things here,
                    // but never uses them.
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                thing = p_thing->Next;
                if ((p_thing->Type == TT_PERSON) && (p_thing->U.UPerson.Group ==
                  things[(ushort)p_stasis->Owner2].U.UPerson.Group))
                    continue;
                dy = (p_thing->Y >> 8) - cor_y;
                if ((dy >= 85) || (dy <= -85))
                    continue;
                adx = (p_thing->X >> 8) - cor_x;
                if (adx < 0)
                    adx = -adx;
                adz = (p_thing->Z >> 8) - cor_z;
                if (adz < 0)
                    adz = -adz;
                if (adx <= adz)
                    dist = adz + (adx >> 1);
                else
                    dist = adx + (adz >> 1);
                if (dist < (s32)(ushort)p_thing->Radius + radius)
                    p_thing->Flag2 |= TgF2_Unkn02000000;
            }
        }
    }
}

void find_and_time_travel(struct SimpleThing *p_sthing)
{
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

static DIFFTEST_REF_FUNC void process_time_pod_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_time_pod\n"
        : : "a" (p_sthing));
}

void process_time_pod(struct SimpleThing *p_sthing)
{
    DIFFTEST_SHADOW(process_time_pod, process_time_pod_asm(p_sthing));
    p_sthing->Timer1--;
    switch ((ushort)p_sthing->State)
    {
    case 0:
        p_sthing->Radius += 0x10;
        if (!IsSamplePlaying(p_sthing->ThingOffset, 0x38, NULL))
            play_dist_ssample(p_sthing, 0x38,
              (ushort)((0x32 - (ushort)p_sthing->Timer1) * 2), 0x40, 100, -1, 1);
        else
            set_dist_volume(p_sthing, 0x38,
              (ushort)((0x32 - p_sthing->Timer1) * 2));
        if (p_sthing->Timer1 < 0)
        {
            p_sthing->Timer1 = 0x17;
            p_sthing->State++;
        }
        find_and_time_travel(p_sthing);
        break;
    case 1:
        p_sthing->Radius -= 0x20;
        if (p_sthing->Timer1 > 0)
        {
            if (!IsSamplePlaying(p_sthing->ThingOffset, 0x38, NULL))
                play_dist_ssample(p_sthing, 0x38,
                  (ushort)(p_sthing->Timer1 << 2), 0x40, 100, -1, 1);
            else
                set_dist_volume(p_sthing, 0x38,
                  (ushort)(p_sthing->Timer1 << 2));
        }
        if (p_sthing->Timer1 < 0)
        {
            p_sthing->Timer1 = 0x19;
            p_sthing->State++;
        }
        break;
    case 2:
        stop_sample_using_heap(p_sthing->ThingOffset, 0x38);
        delete_snode(p_sthing);
        remove_sthing(p_sthing->ThingOffset);
        break;
    default:
        break;
    }
}

int move_smapwho(struct SimpleThing *p_sthing, s32 *p_x, s32 new_y, s32 *p_z);

static DIFFTEST_REF_FUNC void process_soul_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_soul\n"
        : : "a" (p_sthing));
}

void process_soul(struct SimpleThing *p_sthing)
{
    s32 new_x, new_y, new_z;

    DIFFTEST_SHADOW(process_soul, process_soul_asm(p_sthing));
    new_x = p_sthing->X;
    new_z = p_sthing->Z;
    new_y = p_sthing->Y;
    if (p_sthing->State == 0)
    {
        u32 angle0, angle1;
        short timer;

        new_y += p_sthing->U.UEffect.VY;
        angle0 = (u32)gameturn << 5;
        angle1 = (angle0 + 0x780) & 0x7FF;
        angle0 &= 0x7FF;
        new_x += (lbSinTable[angle0] - lbSinTable[angle1]) >> 1;
        new_z += (lbSinTable[angle0 + 512] - lbSinTable[angle1 + 512]) >> 1;
        p_sthing->U.UEffect.VY = (short)(p_sthing->U.UEffect.VY + 0x200);
        timer = p_sthing->Timer1;
        p_sthing->Timer1 = timer - 1;
        if (timer == 0)
        {
            p_sthing->Timer1 = 0;
            p_sthing->State++;
        }
    }
    else if (p_sthing->State == 1)
    {
        struct Thing *p_owner;
        s32 dx, dy, dz;
        s32 dist;
        short vx, vy, vz;

        p_owner = &things[(ushort)p_sthing->Owner2];
        p_sthing->Timer1++;
        dx = (p_owner->X - p_sthing->X) >> 8;
        dy = (p_owner->Y - p_sthing->Y) >> 8;
        dz = (p_owner->Z - p_sthing->Z) >> 8;
        dist = LbSqrL((u32)dx * (u32)dx + (u32)dz * (u32)dz
          + (u32)dy * (u32)dy);
        if (dist == 0)
            dist = 1;
        vx = (s32)((u32)dx << 8) / dist;
        vy = (s32)((u32)dy << 8) / dist;
        vz = (s32)((u32)dz << 8) / dist;
        p_sthing->U.UEffect.VY = vy;
        p_sthing->U.UEffect.VX = vx;
        p_sthing->U.UEffect.VZ = vz;
        if (dist < 0x80)
        {
            if ((p_owner->Flag2 & TgF2_KnockedOut) == 0)
                init_recoil(p_owner, vx, vy, vz, 1);
            // Increments the high byte of Health
            ((ubyte *)&p_owner->Health)[1]++;
            if (p_owner->Health > 2 * (s32)p_owner->U.UPerson.MaxHealth)
                p_owner->Health = 2 * p_owner->U.UPerson.MaxHealth;
            play_dist_sample(p_owner, 0x37, 0x7F, 0x40, 100, 0, 1);
            delete_snode(p_sthing);
            remove_sthing(p_sthing->ThingOffset);
            return;
        }
        new_x += p_sthing->U.UEffect.VX * p_sthing->Timer1 * 4;
        new_y += p_sthing->U.UEffect.VY * p_sthing->Timer1 * 4;
        new_z += p_sthing->U.UEffect.VZ * p_sthing->Timer1 * 4;
    }
    move_smapwho(p_sthing, &new_x, new_y, &new_z);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int get_some_phwoar_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    DIFFTEST_ASM("call ASM_get_some_phwoar\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
    return ra;
}

/** Takes a phwoar entry from the free list; returns its index, or 0.
 * Parameters are eax (not used) and edx, whose upper half ends up in
 * the upper half of the return value.
 */
int get_some_phwoar(int unused_a1, int prev_edx)
{
    ushort ph_idx;

    DIFFTEST_SHADOW_RET(get_some_phwoar, int, get_some_phwoar_asm(unused_a1, prev_edx, 0));
    ph_idx = phwoar_free;
    if (ph_idx == 0)
        DIFFTEST_RETURN((int)0);
    phwoar_free = phwoar[ph_idx].child;
    phwoar_num_used++;
    phwoar_num_free--;
    // The asm only sets the low word of edx; upper half of eax comes from prev_edx
    DIFFTEST_RETURN((int)(((u32)prev_edx & 0xFFFF0000) | ph_idx));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void give_some_phwoar_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    DIFFTEST_ASM("call ASM_give_some_phwoar\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
}

void give_some_phwoar(int phwoar_idx)
{
    ushort ph_idx;
    ushort old_free;

    DIFFTEST_SHADOW(give_some_phwoar, give_some_phwoar_asm(phwoar_idx, 0, 0));
    ph_idx = (ushort)phwoar_idx;
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
    DIFFTEST_ASM("call ASM_give_some_shrapnel\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
}

void give_some_shrapnel(int shrapnel_idx)
{
    ushort shr = (ushort)shrapnel_idx;

    DIFFTEST_SHADOW(give_some_shrapnel, give_some_shrapnel_asm(shrapnel_idx, 0, 0));
    shrapnel[shr].child = shrapnel_free;
    shrapnel_free = shr;
}




static DIFFTEST_REF_FUNC void process_bang_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_bang\n"
        : : "a" (p_sthing));
}

/** Takes a free phwoar and links it at the head of given list.
 */
static struct Phwoar *bang_phwoar_child_new(ushort *p_head)
{
    struct Phwoar *p_child;
    ushort child;

    child = (ushort)get_some_phwoar(0, 0);
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
            give_some_phwoar(phw);
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
            give_some_shrapnel(shr);
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
            give_some_shrapnel(shr);
            shr = next;
            continue;
        }

        if ((p_shrapnel->type == 4) && (p_shrapnel->vy <= 20))
        {
            *p_shead = p_shrapnel->child;
            give_some_shrapnel(shr);
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

extern s32 dword_1E08BC;
void snap_razor_wire(struct Thing *p_wire);

static DIFFTEST_REF_FUNC void FIRE_process_flame_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_FIRE_process_flame\n"
        : : "a" (p_sthing));
}

/** Takes a flame from the free list, fills it and links it at the head
 * of the fire flames list. Returns pointer to the new flame `next` field,
 * or NULL if no free flame was available.
 */
static ushort *FIRE_process_flame_spawn(struct SimpleThing *p_sthing,
  short x, short y, short z, ubyte type, sbyte big, sbyte dbig, sbyte ddbig,
  ushort anim, ubyte life, ubyte count, sbyte fvel)
{
    struct FireFlame *p_nflame;
    ushort nflame;

    nflame = word_1E08B8;
    if (nflame == 0)
        return NULL;
    word_1E08B8 = FIRE_flame[nflame].next;
    dword_1E08BC++;

    p_nflame = &FIRE_flame[nflame];
    p_nflame->x = x;
    p_nflame->z = z;
    p_nflame->y = y;
    p_nflame->type = type;
    p_nflame->big = big;
    p_nflame->dbig = dbig;
    p_nflame->ddbig = ddbig;
    p_nflame->frame = nstart_ani[anim];
    while ((LbRandomAnyShort() & 0x3) != 0)
        p_nflame->frame = frame[p_nflame->frame].Next;
    p_nflame->life = life;
    p_nflame->count = count;
    p_nflame->fvel = fvel;
    p_nflame->fcount = LbRandomAnyShort() & 0x7F;
    p_nflame->next = p_sthing->U.UFire.flame;
    p_sthing->U.UFire.flame = nflame;
    return &p_nflame->next;
}

void FIRE_process_flame(struct SimpleThing *p_sthing)
{
    struct MyMapElement *p_mapel;
    struct FireFlame *p_fflame;
    ushort *p_prev;
    ushort *p_nnext;
    int cor_x, cor_y, cor_z;
    int x, y, z;
    ThingIdx thing;
    ushort colvect;
    ushort fflame, nxflame;
    ubyte life, count;
    sbyte fvel;
    int i;

    DIFFTEST_SHADOW(FIRE_process_flame, FIRE_process_flame_asm(p_sthing));

    cor_x = p_sthing->X >> 8;
    cor_y = p_sthing->Y;
    cor_z = p_sthing->Z >> 8;
    apply_super_quick_light(cor_x, cor_z, LbRandomAnyShort() & 0xF);

    p_mapel = &game_my_big_map[((cor_z >> 8) << 7) + (cor_x >> 8)];

    // Burn things standing on the tile
    thing = p_mapel->Child;
    while (thing != 0)
    {
        if (thing > 0)
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if ((p_thing->Type == TT_PERSON)
              && ((p_thing->Flag & TngF_Destroyed) == 0)
              && (p_thing->Health >= 0))
            {
                int dmg;

                dmg = 0x20 - (cybmod_chest_level(&p_thing->U.UPerson.UMod) << 4);
                if (dmg > 0)
                {
                    p_thing->Health -= dmg;
                    if (p_thing->Health < 0)
                        set_person_dead(p_thing, 0x12);
                }
            }
            thing = p_thing->Next;
        }
        else
        {
            struct SimpleThing *p_static;

            p_static = &sthings[thing];
            if ((p_static->Type == SmTT_STATIC)
              && ((p_static->Flag & TngF_Destroyed) == 0))
                set_static_on_fire(p_static);
            thing = p_static->Next;
        }
    }

    // Burn objects with collision vectors on the tile
    colvect = p_mapel->ColHead;
    while (colvect != 0)
    {
        struct ColVectList *p_cvlist;
        short obj;

        p_cvlist = &game_col_vects_list[colvect];
        obj = p_cvlist->Object;
        colvect = p_cvlist->NextColList & 0x7FFF;
        if ((obj < 0) && ((LbRandomAnyShort() & 0x7) == 0))
        {
            struct Thing *p_thing;

            p_thing = &things[-obj];
            if (p_thing->SubType != 0)
            {
                bang_new4(p_sthing->X, p_sthing->Y, p_sthing->Z, 15);
                do_shockwave(p_sthing->X >> 8, p_sthing->Y >> 8, p_sthing->Z >> 8,
                  0x400, 1000, &things[(ushort)p_thing->Owner]);
            }
            snap_razor_wire(p_thing);
        }
    }

    // Update the flames
    p_prev = &p_sthing->U.UFire.flame;
    fflame = *p_prev;
    while (fflame != 0)
    {
        p_fflame = &FIRE_flame[fflame];
        nxflame = p_fflame->next;
        p_fflame->life--;
        if (p_fflame->life == 0)
        {
            p_fflame->next = word_1E08B8;
            word_1E08B8 = fflame;
            *p_prev = nxflame;
            dword_1E08BC--;
            fflame = nxflame;
            continue;
        }
        p_fflame->big += (p_fflame->dbig >> 3);
        if ((gameturn & 0x3) == 0)
            p_fflame->dbig += p_fflame->ddbig;
        p_fflame->fcount -= p_fflame->fvel;
        if (p_fflame->fcount <= 0)
        {
            p_fflame->fcount = 100;
            p_fflame->frame = frame[p_fflame->frame].Next;
        }
        p_fflame->count--;
        if (p_fflame->count != 0)
        {
            p_prev = &p_fflame->next;
            fflame = nxflame;
            continue;
        }

        switch (p_fflame->type)
        {
        case 1:
        case 5:
        case 20:
        case 24:
            p_fflame->count = 0x7F;
            p_prev = &p_fflame->next;
            break;
        case 4:
            if ((LbRandomAnyShort() & 0x1) == 0)
            {
                life = (LbRandomAnyShort() & 0xF) + 0x21;
                z = cor_z + (LbRandomAnyShort() & 0xFF) - 0x7F;
                x = cor_x + (LbRandomAnyShort() & 0xFF) - 0x7F;
                fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
                p_nnext = FIRE_process_flame_spawn(p_sthing, x, cor_y, z,
                  5, 0x1E, 0, -1, 0x39B, life, 100, fvel);

                life = (LbRandomAnyShort() & 0xF) + 0x21;
                z = cor_z + (LbRandomAnyShort() & 0x1FF) - 0xFF;
                x = cor_x + (LbRandomAnyShort() & 0x1FF) - 0xFF;
                fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
                FIRE_process_flame_spawn(p_sthing, x, cor_y, z,
                  5, 0x1E, 0, -1, 0x39C, life, 100, fvel);

                life = (LbRandomAnyShort() & 0xF) + 0x21;
                z = cor_z + (LbRandomAnyShort() & 0x1FF) - 0xFF;
                x = cor_x + (LbRandomAnyShort() & 0x1FF) - 0xFF;
                fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
                FIRE_process_flame_spawn(p_sthing, x, cor_y, z,
                  5, 0x1E, 0, -1, 0x39C, life, 100, fvel);

                // TODO comparing list pointer with free list head looks like an original bug
                if ((p_prev == &word_1E08B8) && (p_nnext != NULL))
                    p_prev = p_nnext;
                else
                    p_prev = &p_fflame->next;
            }
            else
            {
                p_prev = &p_fflame->next;
            }
            p_fflame->count = 0x7F;
            break;
        case 6:
        case 7:
        case 8:
        case 9:
            if (p_fflame->type <= 7) {
                count = 0x2B - (LbRandomAnyShort() & 0x3);
                life = (LbRandomAnyShort() & 0xF) + 0x2B;
            } else {
                count = 0x35 - (LbRandomAnyShort() & 0x3);
                life = (LbRandomAnyShort() & 0xF) + 0x35;
            }
            if (p_fflame->type == 6)
                z = cor_z + (LbRandomAnyShort() & 0x3F) - 0x1F;
            else if (p_fflame->type == 9)
                z = cor_z + (LbRandomAnyShort() & 0x1FF) - 0xFF;
            else
                z = cor_z + (LbRandomAnyShort() & 0xFF) - 0x7F;
            y = alt_at_point(cor_x, cor_z) >> 5;
            if (p_fflame->type == 6)
                x = cor_x + (LbRandomAnyShort() & 0x3F) - 0x1F;
            else if (p_fflame->type == 9)
                x = cor_x + (LbRandomAnyShort() & 0x1FF) - 0xFF;
            else
                x = cor_x + (LbRandomAnyShort() & 0xFF) - 0x7F;
            fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
            p_nnext = FIRE_process_flame_spawn(p_sthing, x, y, z,
              p_fflame->type,
              (p_fflame->type == 6 || p_fflame->type == 8) ? 100 : 0x1E,
              0, -1, (p_fflame->type <= 7) ? 0x39B : 0x39C, life, count, fvel);
            // TODO comparing list pointer with free list head looks like an original bug
            if ((p_prev == &word_1E08B8) && (p_nnext != NULL))
                p_prev = p_nnext;
            else
                p_prev = &p_fflame->next;
            break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:
        case 18:
        case 19:
            life = (LbRandomAnyShort() & 0xF) + 0x35;
            z = cor_z + (LbRandomAnyShort() & 0x3F) - 0x1F;
            y = p_fflame->y + 0x32;
            x = cor_x + (LbRandomAnyShort() & 0x3F) - 0x1F;
            fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
            p_nnext = FIRE_process_flame_spawn(p_sthing, x, y, z,
              p_fflame->type + 1, 0x1E, 0, -1, 0x39B, life, 3, fvel);
            if (p_fflame->type >= 15)
            {
                for (i = 0; i < 2; i++)
                {
                    int angle, dist;
                    int sn, cs;

                    angle = LbRandomAnyShort() & 0x7FF;
                    sn = lbSinTable[angle] >> 9;
                    cs = lbSinTable[angle + 512] >> 9;
                    dist = LbRandomAnyShort() & 0xFF;
                    sn *= dist;
                    cs *= dist;
                    count = (LbRandomAnyShort() & 0xF) + 0x12;
                    life = (LbRandomAnyShort() & 0xF) + 0x2B;
                    z = (cs >> 8) + p_fflame->z;
                    y = p_fflame->y;
                    x = (sn >> 8) + p_fflame->x;
                    fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
                    FIRE_process_flame_spawn(p_sthing, x, y, z,
                      5, 100, 0, -1, 0x39B, life, count, fvel);
                }
                for (i = 0; i < 2; i++)
                {
                    int angle, dist;
                    int sn, cs;

                    angle = LbRandomAnyShort() & 0x7FF;
                    sn = lbSinTable[angle] >> 8;
                    cs = lbSinTable[angle + 512] >> 8;
                    dist = LbRandomAnyShort() & 0xFF;
                    sn *= dist;
                    cs *= dist;
                    count = (LbRandomAnyShort() & 0xF) + 0x18;
                    life = (LbRandomAnyShort() & 0xF) + 0x35;
                    z = (cs >> 8) + p_fflame->z;
                    y = p_fflame->y;
                    x = (sn >> 8) + p_fflame->x;
                    fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
                    FIRE_process_flame_spawn(p_sthing, x, y, z,
                      5, 100, 0, -1, 0x39C, life, count, fvel);
                }
            }
            // TODO comparing list pointer with free list head looks like an original bug
            if ((p_prev == &word_1E08B8) && (p_nnext != NULL))
                p_prev = p_nnext;
            else
                p_prev = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        case 21:
            life = (LbRandomAnyShort() & 0xF) + 0x35;
            z = cor_z + (LbRandomAnyShort() & 0x7F) - 0x3F;
            y = p_fflame->y + 0x23;
            x = cor_x + (LbRandomAnyShort() & 0x7F) - 0x3F;
            fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
            p_nnext = FIRE_process_flame_spawn(p_sthing, x, y, z,
              p_fflame->type + 1, 0x1E, 0, -1, 0x39B, life, 8, fvel);
            // TODO comparing list pointer with free list head looks like an original bug
            if ((p_prev == &word_1E08B8) && (p_nnext != NULL))
                p_prev = p_nnext;
            else
                p_prev = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        case 22:
        case 23:
            life = (LbRandomAnyShort() & 0xF) + 0x35;
            if (p_fflame->type == 22)
                z = cor_z + (LbRandomAnyShort() & 0x3F) - 0x1F;
            else
                z = cor_z + (LbRandomAnyShort() & 0x1F) - 0xF;
            y = p_fflame->y + 0x37;
            if (p_fflame->type == 22)
                x = cor_x + (LbRandomAnyShort() & 0x3F) - 0x1F;
            else
                x = cor_x + (LbRandomAnyShort() & 0x1F) - 0xF;
            fvel = (LbRandomAnyShort() & 0x3F) + 0x32;
            p_nnext = FIRE_process_flame_spawn(p_sthing, x, y, z,
              p_fflame->type + 1, 0x1E, 0, -1, 0x39B, life, 8, fvel);
            // TODO comparing list pointer with free list head looks like an original bug
            if ((p_prev == &word_1E08B8) && (p_nnext != NULL))
                p_prev = p_nnext;
            else
                p_prev = &p_fflame->next;
            p_fflame->count = 0x7F;
            break;
        default: // types 2, 3 and unknown keep the list pointer
            break;
        }
        fflame = nxflame;
    }
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
static DIFFTEST_REF_FUNC int move_smapwho_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_move_smapwho\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int move_smapwho(struct SimpleThing *p_sthing, s32 *p_x, s32 new_y, s32 *p_z)
{
    ushort *p_tile_x = (ushort *)p_x + 1;
    ushort *p_tile_z = (ushort *)p_z + 1;

    DIFFTEST_SHADOW_RET(move_smapwho, int, move_smapwho_asm((intptr_t)p_sthing,
      (intptr_t)p_x, new_y, (intptr_t)p_z));
    if (((*p_x >> 16) >= 128) || ((*p_z >> 16) >= 128) ||
      ((*p_x >> 16) < 0) || ((*p_z >> 16) < 0))
    {
        *p_tile_x &= 0x7f;
        *p_tile_z &= 0x7f;
    }
    if ((*p_tile_x != ((ushort *)&p_sthing->X)[1]) ||
      (*p_tile_z != ((ushort *)&p_sthing->Z)[1]))
    {
        struct MyMapElement *p_mapel;
        ushort tile;
        short child;
        ThingIdx offset;

        delete_snode(p_sthing);
        tile = (ushort)((*p_tile_z << 7) + *p_tile_x);
        p_mapel = &game_my_big_map[tile];
        child = p_mapel->Child;
        offset = p_sthing->ThingOffset;
        p_sthing->Next = child;
        if (child > 0)
            things[child].Parent = offset;
        else if (child < 0)
            sthings[child].Parent = offset;
        p_sthing->Parent = 0;
        p_mapel->Child = offset;
    }
    p_sthing->X = *p_x;
    p_sthing->Z = *p_z;
    p_sthing->Y = new_y;
    DIFFTEST_RETURN((int)*p_z);
}


static DIFFTEST_REF_FUNC void process_spark_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_spark\n"
        : : "a" (p_sthing));
}

void process_spark(struct SimpleThing *p_sthing)
{
    s32 new_x, new_y, new_z;
    short timer;

    DIFFTEST_SHADOW(process_spark, process_spark_asm(p_sthing));
    timer = p_sthing->Timer1;
    p_sthing->Timer1 = timer - 1;
    if ((timer == 0) ||
      (alt_at_point(p_sthing->X >> 8, p_sthing->Z >> 8) > p_sthing->Y))
    {
        remove_sthing(p_sthing - sthings);
        delete_snode(p_sthing);
        return;
    }
    new_x = p_sthing->X + p_sthing->U.UEffect.VX;
    new_z = p_sthing->Z + p_sthing->U.UEffect.VZ;
    new_y = p_sthing->Y + p_sthing->U.UEffect.VY;
    p_sthing->U.UEffect.OX = p_sthing->X >> 8;
    p_sthing->U.UEffect.OY = p_sthing->Y >> 8;
    p_sthing->U.UEffect.VY -= 0x20;
    p_sthing->U.UEffect.OZ = p_sthing->Z >> 8;
    move_smapwho(p_sthing, &new_x, new_y, &new_z);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void apply_texture_to_face_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_apply_texture_to_face\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
}

void apply_texture_to_face(int frame_idx, int anim_idx, int face_idx)
{
    ushort frame = (ushort)frame_idx;
    ushort anim = (ushort)anim_idx;
    ushort face = (ushort)face_idx;
    struct SingleObjectFace4 *p_face4;
    struct SingleFloorTexture *p_fltextr;
    struct AnimPage *p_anpg;
    ubyte x, y, x2, y2;

    DIFFTEST_SHADOW(apply_texture_to_face, apply_texture_to_face_asm(frame_idx, anim_idx, face_idx, 0));
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
    DIFFTEST_ASM(
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
            apply_texture_to_face(frame, anim, (ushort)p_sthing->Parent);
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
            apply_texture_to_face(frame, anim, (ushort)p_sthing->Parent);
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

static DIFFTEST_REF_FUNC ubyte napalm_person_asm(struct Thing *p_person, ushort multi)
{
    ubyte ret;
    DIFFTEST_ASM("call ASM_napalm_person\n"
        : "=a" (ret) : "a" (p_person), "d" (multi));
    return ret;
}

ubyte napalm_person(struct Thing *p_person, ushort multi)
{
    u32 dmg;
    ushort frm;

    DIFFTEST_SHADOW_RET(napalm_person, ubyte, napalm_person_asm(p_person, multi));
    if ((ushort)p_person->State == 0x24)
        DIFFTEST_RETURN((ubyte)0);
    if ((p_person->Flag & TngF_Destroyed) != 0)
        DIFFTEST_RETURN((ubyte)0);

    if (p_person->Type == SmTT_STATIC)
    {
        struct SimpleThing *p_static = (struct SimpleThing *)p_person;

        p_static->U.UScenery.Health -= weapon_defs[WEP_FLAMER].HitDamage;
        if (p_static->U.UScenery.Health <= 0)
            set_static_on_fire(p_static);
        DIFFTEST_RETURN((ubyte)0);
    }

    if (p_person->SubType == 7)
        DIFFTEST_RETURN((ubyte)0);
    if (cybmod_chest_level(&p_person->U.UPerson.UMod) > 3)
        DIFFTEST_RETURN((ubyte)0);

    dmg = weapon_defs[WEP_FLAMER].HitDamage;
    // The asm takes all 4 upper bits here, not masked to 3 like cybmod_skin_level()
    if ((ushort)(p_person->U.UPerson.UMod.Mods >> 12) == 2)
        dmg >>= 2;
    p_person->Health -= (ushort)(dmg * multi);

    if (p_person->Health > 0)
    {
        if ((p_person->Flag2 & TgF2_KnockedOut) == 0)
        {
            short rcl;

            rcl = (short)lbSinTable[512 + (p_person->U.UPerson.RecoilDir << 3)];
            init_recoil(p_person, rcl, 0, rcl, 7);
        }
        DIFFTEST_RETURN((ubyte)0);
    }

    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
    {
        set_person_dead(p_person, 0x12);
        DIFFTEST_RETURN((ubyte)1);
    }
    if ((p_person->Flag & TngF_Persuaded) != 0)
        stop_being_persuaded(p_person);

    p_person->State = 0x24;
    p_person->U.UPerson.FrameId.Version[0] = 0;
    play_dist_sample(p_person, 0x1D, 0x7F, 0x40, 100, -1, 2);
    p_person->Flag |= TngF_Unkn40000000;
    p_person->U.UPerson.RecoilTimer = (LbRandomAnyShort() + 0x32) & 0x1F;
    p_person->Flag &= ~(TngF_Unkn0001 | TngF_PersKeepPost | TngF_WepRecoil | TngF_Persuaded);
    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;
    p_person->Timer1 = 10;
    p_person->StartTimer1 = 10;
    p_person->U.UPerson.AnimMode = 0x11;
    p_person->U.UPerson.OldAnimMode = 0x11;
    p_person->Flag2 &= ~(TgF2_Unkn0008 | TgF2_KnockedOut);
    frm = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->Frame = nstart_ani[frm + p_person->U.UPerson.Angle];
    p_person->U.UPerson.Target2 = 0;
    p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    DIFFTEST_RETURN((ubyte)1);
}
static DIFFTEST_REF_FUNC void person_go_insane_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_person_go_insane\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
}

/** Makes given person go insane.
 * Routed asm (internal label); the person comes in eax.
 */
void person_go_insane(struct Thing *p_person)
{
    DIFFTEST_SHADOW(person_go_insane, person_go_insane_asm((intptr_t)p_person, 0, 0, 0));
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;
    if ((p_person->Flag2 & TgF2_Unkn0008) != 0)
        return;
    p_person->Flag2 |= TgF2_Unkn0008;
    p_person->U.UPerson.EffectiveGroup += 0x80;
    choose_best_weapon_for_range(p_person, 3);
    p_person->Speed = 0x400;
    p_person->U.UPerson.Timer2 = 5;
    p_person->U.UPerson.Target2 = 0;
    p_person->U.UPerson.StartTimer2 = 5;
    p_person->U.UPerson.BumpCount = 150;
}


static void napalm_flame_person_killed_add_to_stats(struct Thing *p_person,
  struct Thing *p_owner);

static DIFFTEST_REF_FUNC void process_drift_smoke_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    DIFFTEST_ASM(
      "call ASM_process_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    struct Thing *p_person;

    DIFFTEST_SHADOW(process_drift_smoke,
      process_drift_smoke_asm(p_sthing, thing_item));

    if ((p_sthing->Timer1 < 12) && ((gameturn & 1) != 0))
        p_sthing->Frame = frame[(ushort)p_sthing->Frame].Next;

    if ((p_sthing->Flag & TngF_Unkn0008) == 0)
    {
        s32 new_x, new_y, new_z;

        new_x = p_sthing->X + 4 * p_sthing->U.UEffect.VX;
        new_y = p_sthing->Y + 4 * p_sthing->U.UEffect.VY;
        new_z = p_sthing->Z + 4 * p_sthing->U.UEffect.VZ;
        if (p_sthing->SubType != 30)
        {
            s32 alt;

            alt = alt_at_point((short)(new_x >> 8), (short)(new_z >> 8));
            if ((new_y < alt) && (alt - new_y < 0x800))
                new_y = alt + 0x40;
        }
        if ((new_x >> 16) < 0 || (new_x >> 16) >= 128 ||
          (new_z >> 16) < 0 || (new_z >> 16) >= 128)
        {
            p_sthing->Timer1 = -1;
        }
        else
        {
            if (check_col_collision(new_x >> 8, new_y >> 8, new_z >> 8) != 0)
            {
                p_sthing->Flag |= TngF_Unkn0008;
                if (p_sthing->Timer1 > 5)
                    p_sthing->Timer1 = 5;
            }
            else
            {
                move_smapwho(p_sthing, &new_x, new_y, &new_z);
            }
            p_sthing->U.UEffect.VX += p_sthing->U.UEffect.OX;
            p_sthing->U.UEffect.VY += p_sthing->U.UEffect.OY;
            p_sthing->U.UEffect.VZ += p_sthing->U.UEffect.OZ;
        }
    }

    p_sthing->Object += p_sthing->StartFrame;
    p_sthing->Timer1--;
    if (p_sthing->Timer1 <= 0)
    {
        remove_sthing(thing_item);
        delete_snode(p_sthing);
        return;
    }

    if (((p_sthing->Flag & (TngF_Unkn0010|TngF_Unkn0020)) != 0) &&
      ((gameturn & 3) == 0) && ((ushort)p_sthing->Object > 60) &&
      (things != NULL))
    {
        short skip;

        skip = 0;
        do
        {
            p_person = check_for_radius_hit_person(p_sthing->X, p_sthing->Y,
              p_sthing->Z, (ushort)p_sthing->Object, NULL, 0, skip);
            if ((p_person != NULL) && (p_person->Type != TT_VEHICLE) &&
              (p_person->Type != SmTT_STATIC) &&
              (p_person->ThingOffset != p_sthing->Owner2) &&
              (person_mod_chest_level(p_person) < 3) &&
              (p_person->U.UPerson.Group !=
                things[(ushort)p_sthing->Owner2].U.UPerson.Group))
            {
                if ((p_person->Flag2 & TgF2_Unkn0001) != 0)
                    finalise_razor_wire(p_person);
                if ((p_sthing->Flag & TngF_Unkn0010) != 0)
                    person_go_insane(p_person);
                else
                    person_go_sleep(p_person);
            }
            skip++;
        }
        while (p_person != NULL);
    }

    if (p_sthing->SubType != 45)
        return;

    p_person = check_for_radius_hit_person(p_sthing->X, p_sthing->Y,
      p_sthing->Z, (ushort)p_sthing->Object, NULL, 0, 0);
    if (p_person == NULL)
        return;
    if (p_person->Type == TT_VEHICLE)
        return;
    if (p_person->Type != SmTT_STATIC)
    {
        if ((ushort)p_person->State == PerSt_PERSON_BURNING)
            return;
        if ((ushort)p_person->State == PerSt_DIEING)
            return;
        if ((p_person->Flag & TngF_Destroyed) != 0)
            return;
    }
    if ((p_person->Type != SmTT_STATIC) &&
      ((p_person->Flag2 & TgF2_KnockedOut) == 0))
    {
        short angle;

        angle = arctan((p_sthing->X >> 8) - (p_person->X >> 8),
          -((p_sthing->Z >> 8) - (p_person->Z >> 8)));
        p_person->U.UPerson.RecoilDir = ((s32)angle + 0x80) >> 8;
        p_person->U.UPerson.RecoilDir = (p_person->U.UPerson.RecoilDir + 8) & 7;
        change_person_angle(p_person, (p_person->U.UPerson.RecoilDir + 4) & 7);
    }
    if (napalm_person(p_person, 2) != 0)
    {
        struct Thing *p_owner;

        p_owner = &things[(ushort)p_sthing->Owner2];
        napalm_flame_person_killed_add_to_stats(p_person, p_owner);
    }
}

static DIFFTEST_REF_FUNC void process_mushroom_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    DIFFTEST_ASM(
      "call ASM_process_mushroom\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_mushroom(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    s32 new_x, new_y, new_z;
    s32 height, dt;

    DIFFTEST_SHADOW(process_mushroom,
      process_mushroom_asm(p_sthing, thing_item));
    height = (p_sthing->Y >> 8) - p_sthing->U.UEffect.OY;
    if (height < 80)
    {
        dt = height >> 1;
        new_x = p_sthing->X - p_sthing->U.UEffect.VX * dt;
        new_z = p_sthing->Z - p_sthing->U.UEffect.VZ * dt;
    }
    else
    {
        dt = (height - 80) >> 2;
        new_x = p_sthing->X + p_sthing->U.UEffect.VX * dt;
        new_z = p_sthing->Z + p_sthing->U.UEffect.VZ * dt;
    }
    p_sthing->U.UEffect.VY += (short)((80 - height) << 3);
    if (p_sthing->U.UEffect.VY < -2000)
        p_sthing->U.UEffect.VY = -2000;
    new_y = p_sthing->Y + (p_sthing->U.UEffect.VY >> 1);
    move_smapwho(p_sthing, &new_x, new_y, &new_z);
    p_sthing->Object += 4;
    if (p_sthing->Timer1 > 0)
    {
        p_sthing->Timer1--;
        if (p_sthing->Timer1 == 0)
        {
            remove_sthing(thing_item);
            delete_snode(p_sthing);
        }
    }
}

static DIFFTEST_REF_FUNC void process_special_drift_smoke_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    DIFFTEST_ASM(
      "call ASM_process_special_drift_smoke\n"
        : : "a" (p_sthing), "d" (thing_item));
}

void process_special_drift_smoke(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    s32 new_x, new_y, new_z;
    s32 radius, angle;
    short dx, dz;

    DIFFTEST_SHADOW(process_special_drift_smoke,
      process_special_drift_smoke_asm(p_sthing, thing_item));
    p_sthing->State++;
    p_sthing->Frame = frame[(ushort)p_sthing->Frame].Next;
    new_x = p_sthing->X + 4 * p_sthing->U.UEffect.VX;
    new_y = p_sthing->Y + 4 * p_sthing->U.UEffect.VY;
    new_z = p_sthing->Z + 4 * p_sthing->U.UEffect.VZ;
    radius = (ushort)p_sthing->State;
    angle = (radius << 5) & 0x7FF;
    dx = (lbSinTable[angle + 512] * radius) >> 11;
    dz = (lbSinTable[angle] * radius) >> 11;
    new_x += dx;
    new_z += dz;
    move_smapwho(p_sthing, &new_x, new_y, &new_z);
    p_sthing->Object += p_sthing->StartFrame;
    if (p_sthing->StartTimer1 > 0)
    {
        short damp;

        p_sthing->StartTimer1--;
        damp = p_sthing->U.UEffect.OX;
        p_sthing->U.UEffect.VX = (p_sthing->U.UEffect.VX * damp) >> 8;
        p_sthing->U.UEffect.VY = (p_sthing->U.UEffect.VY * damp) >> 8;
        p_sthing->U.UEffect.VZ = (p_sthing->U.UEffect.VZ * damp) >> 8;
    }
    p_sthing->U.UEffect.VY += p_sthing->U.UEffect.OY;
    if (p_sthing->Timer1 > 0)
    {
        p_sthing->Timer1--;
        if (p_sthing->Timer1 == 0)
        {
            remove_sthing(thing_item);
            delete_snode(p_sthing);
        }
    }
}

static DIFFTEST_REF_FUNC void process_napalm_flame_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    DIFFTEST_ASM(
      "call ASM_process_napalm_flame\n"
        : : "a" (p_sthing), "d" (thing_item));
}

/** Count a person killed by a napalm flame in the mission statistics.
 */
static void napalm_flame_person_killed_add_to_stats(struct Thing *p_person,
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

void process_napalm_flame(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    struct Thing *p_person;
    s32 intens;

    DIFFTEST_SHADOW(process_napalm_flame,
      process_napalm_flame_asm(p_sthing, thing_item));

    if (((p_sthing->ThingOffset + gameturn) & 1) != 0)
        p_sthing->Frame = frame[(ushort)p_sthing->Frame].Next;

    if ((p_sthing->Flag & TngF_Unkn0008) == 0)
    {
        s32 new_x, new_y, new_z;

        new_x = p_sthing->X + 4 * p_sthing->U.UEffect.VX;
        new_y = p_sthing->Y + 4 * p_sthing->U.UEffect.VY;
        new_z = p_sthing->Z + 4 * p_sthing->U.UEffect.VZ;
        if ((new_x >> 16) < 0 || (new_x >> 16) >= 128 ||
          (new_z >> 16) < 0 || (new_z >> 16) >= 128)
        {
            p_sthing->Timer1 = -1;
        }
        else
        {
            if (check_col_collision(new_x >> 8, (new_y >> 8) + 8, new_z >> 8) != 0)
                p_sthing->Flag |= TngF_Unkn0008;
            else
                move_smapwho(p_sthing, &new_x, new_y, &new_z);
            p_sthing->U.UEffect.VX += p_sthing->U.UEffect.OX;
            p_sthing->U.UEffect.VY += p_sthing->U.UEffect.OY;
            p_sthing->U.UEffect.VZ += p_sthing->U.UEffect.OZ;
        }
    }

    if (p_sthing->Timer1 < 10)
        p_sthing->Object += p_sthing->StartFrame;
    else
        p_sthing->Object += 5;

    p_sthing->Timer1--;
    if (p_sthing->Timer1 <= 0)
    {
        remove_sthing(thing_item);
        delete_snode(p_sthing);
        ingame.FlameCount--;
        return;
    }

    p_person = check_for_radius_hit_person(p_sthing->X, p_sthing->Y,
      p_sthing->Z, (ushort)p_sthing->Object, NULL, 0, 0);
    if ((p_person != NULL) && (p_person->Type != TT_VEHICLE))
    {
        struct Thing *p_owner;
        TbBool hit;

        hit = true;
        if (p_person->Type != SmTT_STATIC)
        {
            p_owner = &things[(ushort)p_sthing->Owner2];
            if (p_person->ThingOffset == p_sthing->Owner2)
                hit = false;
            else if (p_person->U.UPerson.EffectiveGroup == p_owner->U.UPerson.EffectiveGroup)
                hit = false;
            else if ((ushort)p_person->State == PerSt_PERSON_BURNING)
                hit = false;
            else if ((ushort)p_person->State == PerSt_DIEING)
                hit = false;
            else if ((p_person->Flag & TngF_Destroyed) != 0)
                hit = false;
        }
        if (hit)
        {
            p_sthing->Flag |= TngF_Unkn0008;
            if ((p_person->Type != SmTT_STATIC) &&
              ((p_person->Flag2 & TgF2_KnockedOut) == 0))
            {
                short angle;

                angle = arctan((p_sthing->X >> 8) - (p_person->X >> 8),
                  -((p_sthing->Z >> 8) - (p_person->Z >> 8)));
                p_person->U.UPerson.RecoilDir = ((s32)angle + 0x80) >> 8;
                p_person->U.UPerson.RecoilDir = (p_person->U.UPerson.RecoilDir + 8) & 7;
                change_person_angle(p_person, (p_person->U.UPerson.RecoilDir + 4) & 7);
            }
            if (napalm_person(p_person, 1) != 0)
            {
                p_owner = &things[(ushort)p_sthing->Owner2];
                napalm_flame_person_killed_add_to_stats(p_person, p_owner);
            }
        }
    }

    intens = (LbRandomAnyShort() & 0x1F) + 0x50;
    if ((ushort)ingame.FlameCount < 20)
        intens = (intens * (ushort)ingame.FlameCount) / 20;
    apply_full_light(p_sthing->X >> 8, (p_sthing->Y >> 8) + 100, p_sthing->Z >> 8,
      (short)intens, p_sthing->U.UEffect.OZ);
}

static DIFFTEST_REF_FUNC void process_flame1_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_flame1\n"
        : : "a" (p_sthing));
}

void process_flame1(struct SimpleThing *p_sthing)
{
    struct Thing *p_person;

    DIFFTEST_SHADOW(process_flame1, process_flame1_asm(p_sthing));

    if (((p_sthing->ThingOffset + gameturn) & 1) != 0)
        p_sthing->Frame = frame[(ushort)p_sthing->Frame].Next;

    if (p_sthing->Timer1 > 0)
    {
        p_sthing->Timer1--;
        // TODO the original keeps processing the flame after deleting it
        if (p_sthing->Timer1 == 0)
        {
            remove_sthing(p_sthing->ThingOffset);
            delete_snode(p_sthing);
        }
    }

    p_person = check_for_radius_hit_person(p_sthing->X, p_sthing->Y,
      p_sthing->Z, (ushort)p_sthing->Object, NULL, 0, 0);
    if ((p_person == NULL) || (p_person->Type == TT_VEHICLE))
        return;

    if (p_person->Type != SmTT_STATIC)
    {
        struct Thing *p_owner;

        if (p_person->ThingOffset == p_sthing->Owner2)
            return;
        p_owner = &things[(ushort)p_sthing->Owner2];
        if (p_person->U.UPerson.EffectiveGroup == p_owner->U.UPerson.EffectiveGroup)
            return;
        if ((ushort)p_person->State == PerSt_PERSON_BURNING)
            return;
    }

    p_sthing->Flag |= TngF_Unkn0008;
    if ((p_person->Type != SmTT_STATIC) &&
      ((p_person->Flag2 & TgF2_KnockedOut) == 0))
    {
        short angle;

        angle = arctan((p_sthing->X >> 8) - (p_person->X >> 8),
          -((p_sthing->Z >> 8) - (p_person->Z >> 8)));
        p_person->U.UPerson.RecoilDir = ((s32)angle + 0x80) >> 8;
        p_person->U.UPerson.RecoilDir = (p_person->U.UPerson.RecoilDir + 8) & 7;
        change_person_angle(p_person, (p_person->U.UPerson.RecoilDir + 4) & 7);
    }
    if (napalm_person(p_person, 1) != 0)
    {
        struct Thing *p_owner;

        p_owner = &things[(ushort)p_sthing->Owner2];
        napalm_flame_person_killed_add_to_stats(p_person, p_owner);
    }
}

static DIFFTEST_REF_FUNC void process_splash_asm(struct SimpleThing *p_sthing)
{
    DIFFTEST_ASM(
      "call ASM_process_splash\n"
        : : "a" (p_sthing));
}

void process_splash(struct SimpleThing *p_sthing)
{
    ushort nxframe;

    DIFFTEST_SHADOW(process_splash, process_splash_asm(p_sthing));

    p_sthing->Timer1--;
    if ((p_sthing->Timer1 > 0) && ((p_sthing->Timer1 & 1) != 0))
    {
        struct SimpleThing *p_effect;
        s32 x, z;
        int alt;

        x = (p_sthing->X >> 8) + ((ushort)LbRandomAnyShort() & 0xFF) - 0x80;
        z = (p_sthing->Z >> 8) + ((ushort)LbRandomAnyShort() & 0xFF) - 0x80;
        alt = alt_at_point((short)x, (short)z);
        p_effect = create_scale_effect((ushort)x, alt >> 8, (ushort)z, 0x43F, 0);
        if (p_effect != NULL)
        {
            p_effect->SubType = 60;
            p_effect->Object = ((ushort)LbRandomAnyShort() & 0x7F) + 62;
        }
    }

    nxframe = frame[(ushort)p_sthing->Frame].Next;
    if ((frame[nxframe].Flags & 0x01) != 0)
    {
        remove_sthing(p_sthing->ThingOffset);
        delete_snode(p_sthing);
    }
    else
    {
        p_sthing->Frame = nxframe;
    }
}

void process_scale_effect(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
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

static DIFFTEST_REF_FUNC void test_stuff_asm(int x, int y, int z, ushort owner)
{
    DIFFTEST_ASM(
      "call ASM_test_stuff\n"
        : : "a" (x), "d" (y), "b" (z), "c" ((int)owner));
}

void test_stuff(int x, int y, int z, ushort owner)
{
    ushort angle;
    u32 n;

    DIFFTEST_SHADOW(test_stuff, test_stuff_asm(x, y, z, owner));
    angle = 0;
    if ((gameturn & 0x01) != 0)
        angle = 0x80;
    for (n = 0; angle < 0x800; n++, angle += 0x100)
    {
        struct SimpleThing *p_sthing;
        s32 dx, dz;
        short timer;

        dx = lbSinTable[angle + LbFPMath_PI/2];
        dz = lbSinTable[angle];
        dx = (dx >> 8) + (s32)LbRandomAnyShort() % 55 - 27;
        dz = (dz >> 8) + (s32)LbRandomAnyShort() % 55 - 27;
        if ((n & 0x01) != 0)
            timer = 40;
        else
            timer = (s32)LbRandomAnyShort() % 10 + 40;

        p_sthing = create_scale_effect(x + dx, y, z + dz, 907, timer);
        if (p_sthing == NULL)
            continue;
        p_sthing->SubType = 31;
        p_sthing->Object = 128;
        p_sthing->StartFrame = 4;
        p_sthing->U.UEffect.VY = 4000;
        p_sthing->U.UEffect.VX = dx;
        p_sthing->U.UEffect.VZ = dz;
        p_sthing->U.UEffect.OX = x;
        p_sthing->U.UEffect.OY = y;
        p_sthing->U.UEffect.OZ = z;
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void do_explode_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_do_explode\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void explode_thing_object(int a1, int a2, int a3, int a4);

void do_explode(int pos_x, int pos_y, int pos_z)
{
    short x, y, z;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW(do_explode, do_explode_asm(pos_x, pos_y, pos_z, 0));

    x = (short)pos_x;
    y = (short)pos_y;
    z = (short)pos_z;

    if (current_map == 11)
    {
        unkn01_pos_x = engn_xc >> 8;
        unkn01_downcount = 50;
        unkn01_pos_y = engn_zc >> 8;
        return;
    }

    tile_x = x >> 8;
    tile_z = z >> 8;
    for (dx = -4; dx < 4; dx++)
    {
        for (dz = -4; dz < 4; dz++)
        {
            s32 tx, tz;
            short thing;

            tx = tile_x + dx;
            if (tx <= 0 || tx >= 128)
                continue;
            tz = tile_z + dz;
            if (tz <= 0 || tz >= 128)
                continue;
            thing = game_my_big_map[(tz << 7) + tx].Child;

            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if ((p_thing->Type == TT_BUILDING)
                      && ((p_thing->Flag & TngF_Destroyed) == 0)
                      && (p_thing->SubType != SubTT_BLD_MGUN))
                    {
                        // TODO the same thing is checked again after collapse;
                        // relies on collapse_building() setting TngF_Destroyed
                        collapse_building(x, y, z, p_thing);
                        continue;
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;

                    p_sthing = &sthings[thing];
                    if (p_sthing->Type == TT_BUILDING)
                        explode_thing_object(thing, x, y, z);
                    thing = p_sthing->Next;
                }
            }
        }
    }
}


static DIFFTEST_REF_FUNC void process_nuclear_bomb_asm(struct SimpleThing *p_sthing, ThingIdx thing_item)
{
    DIFFTEST_ASM(
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

        do_explode((short)x, (short)y, (short)z);
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
    DIFFTEST_ASM(
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
            process_things_mines_explode((game_rand() & 0x1F) + 2);
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

void snprint_thing(char *buf, u32 buflen, struct Thing *p_thing)
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
        sprintf(s, "Flag(0x%x)", p_thing->Flag);
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

void snprint_sthing(char *buf, u32 buflen, struct SimpleThing *p_sthing)
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
        sprintf(s, "Flag(0x%x)", p_sthing->Flag);
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
    DIFFTEST_ASM("call ASM_delete_node\n"
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
    DIFFTEST_ASM("call ASM_delete_snode\n"
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
    DIFFTEST_ASM("call ASM_add_node_thing\n"
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
    DIFFTEST_ASM("call ASM_get_new_thing\n"
        : "=a" (ret) : );
    return ret;
}

/** Takes the given thing index off the free list, if it is free.
 * Returns non-zero when the index was claimed; only the low 16 bits matter.
 */
static DIFFTEST_REF_FUNC int get_new_thing_with_index_asm(ThingIdx thing)
{
    int ret;
    DIFFTEST_ASM("call ASM_get_new_thing_with_index\n"
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
    DIFFTEST_ASM("call ASM_remove_thing\n"
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
    DIFFTEST_ASM("call ASM_add_node_sthing\n"
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
    DIFFTEST_ASM("call ASM_get_new_sthing\n"
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
    DIFFTEST_ASM("call ASM_remove_sthing\n"
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

static DIFFTEST_REF_FUNC void new_thing_traffic_clone_asm(struct SimpleThing *p_clsthing)
{
    DIFFTEST_ASM("call ASM_new_thing_traffic_clone\n"
        : : "a" (p_clsthing));
}

void new_thing_traffic_clone(struct SimpleThing *p_clsthing)
{
    struct SimpleThing *p_sthing;
    ThingIdx sthing;
    ushort i;

    DIFFTEST_SHADOW(new_thing_traffic_clone, new_thing_traffic_clone_asm(p_clsthing));
    // TODO the asm does not check for get_new_sthing() returning 0
    sthing = get_new_sthing();
    p_sthing = &sthings[sthing];
    p_sthing->X = p_clsthing->X;
    p_sthing->Z = p_clsthing->Z;
    p_sthing->Type = SmTT_TRAFFIC;
    p_sthing->Y = p_clsthing->Y;
    p_sthing->SubType = p_clsthing->SubType;
    p_sthing->ThingOffset = p_clsthing->ThingOffset;
    for (i = 0; i < 4; i++)
    {
        p_sthing->U.UTraffic.Link[i] = p_clsthing->U.UTraffic.Link[i];
        p_sthing->U.UTraffic.Flags[i] = p_clsthing->U.UTraffic.Flags[i];
    }
    add_node_sthing(sthing);
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

    DIFFTEST_ASM(
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

    DIFFTEST_ASM(
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

    DIFFTEST_ASM(
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

static DIFFTEST_REF_FUNC struct SimpleThing *create_stasis_pod_asm(MapCoord x,
  MapCoord y, MapCoord z, ushort timer, struct Thing *p_owner)
{
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[1];
    int edx = y, ebx = z, ecx = timer;

    stkargs[0] = (int)(intptr_t)p_owner;

    DIFFTEST_ASM(
      "push 0(%4)\n"
      "call ASM_create_stasis_pod\n"
        : "=a" (ret), "+d" (edx), "+b" (ebx), "+c" (ecx)
        : "S" (stkargs), "0" ((int)x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_stasis_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer, struct Thing *p_owner)
{
    struct SimpleThing *p_sthing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_stasis_pod, struct SimpleThing *,
      create_stasis_pod_asm(x, y, z, timer, p_owner));
    if ((s32)sthings_used > 1495)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (x < 0 || x > 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (z < 0 || z > 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    sthing = get_new_sthing();
    if (sthing == 0)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if (sthing > -1 || sthing < -1500)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_sthing = &sthings[sthing];
    p_sthing->Radius = 0;
    p_sthing->StartFrame = 0;
    p_sthing->Type = SmTT_STASIS_POD;
    p_sthing->Z = (s32)z << 8;
    p_sthing->State = 0;
    p_sthing->X = (s32)x << 8;
    p_sthing->Y = (s32)y << 8;
    p_sthing->Timer1 = timer;
    p_sthing->Owner2 = p_owner->ThingOffset;
    add_node_sthing(sthing);
    DIFFTEST_RETURN(p_sthing);
}

struct SimpleThing *create_time_pod(MapCoord x, MapCoord y, MapCoord z,
  ushort timer)
{
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

static DIFFTEST_REF_FUNC struct SimpleThing *create_electric_strand_asm(
  MapCoord x, MapCoord y, MapCoord z,
  MapCoord x2, MapCoord y2, MapCoord z2, int sound)
{
    struct SimpleThing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];
    int edx = y, ebx = z, ecx = x2;

    stkargs[0] = (int)(intptr_t)y2;
    stkargs[1] = (int)(intptr_t)z2;
    stkargs[2] = (int)(intptr_t)sound;

    DIFFTEST_ASM(
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_create_electric_strand\n"
        : "=a" (ret), "+d" (edx), "+b" (ebx), "+c" (ecx)
        : "S" (stkargs), "0" ((s32)x)
        : "cc", "memory");
    return ret;
}

struct SimpleThing *create_electric_strand(MapCoord x, MapCoord y, MapCoord z,
  MapCoord x2, MapCoord y2, MapCoord z2, int sound)
{
    struct SimpleThing *p_sthing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_electric_strand, struct SimpleThing *,
      create_electric_strand_asm(x, y, z, x2, y2, z2, sound));
    if ((s32)x < 0 || (s32)x > 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);
    if ((s32)z < 0 || (s32)z > 0x8000)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    sthing = get_new_sthing();
    if (sthing == 0)
        DIFFTEST_RETURN((struct SimpleThing *)NULL);

    p_sthing = &sthings[sthing];
    p_sthing->X = (s32)x << 8;
    p_sthing->Z = (s32)z << 8;
    p_sthing->U.UEffect.VY = y2;
    p_sthing->U.UEffect.VZ = z2;
    p_sthing->Y = (s32)y << 8;
    p_sthing->U.UEffect.VX = x2;
    add_node_sthing(p_sthing->ThingOffset);
    p_sthing->Type = SmTT_ELECTRIC_STRAND;
    p_sthing->SubType = 0;
    p_sthing->Radius = 50;
    p_sthing->Timer1 = 5;
    if ((ubyte)sound != 0)
        play_dist_ssample(p_sthing, 0x28, 0x7F, 0x40, 100, 0, 1);
    DIFFTEST_RETURN(p_sthing);
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
