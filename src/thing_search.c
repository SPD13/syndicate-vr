/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_search.c
 *     Support for finding a matching Thing instance.
 * @par Purpose:
 *     Implement searching through various lists of things.
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
#include "thing_search.h"

#include <stdlib.h>
#include "bfutility.h"

#include "bigmap.h"
#include "building.h"
#include "difftest.h"
#include "enginsngobjs.h"
#include "game.h"
#include "game_options.h"
#include "people.h"
#include "pepgroup.h"
#include "swlog.h"
#include "thing.h"
#include "thing_fire.h"
#include "vehicle.h"
#include "weapon.h"

/******************************************************************************/
/** Problematic tile for mapwho debug.
 */
short bad_mapwho_tl_x = -1;
short bad_mapwho_tl_z = -1;


/** Searches for a thing of given type on the specific mapwho tile.
 * Besides being given a title, the thing must also meet circular range condition.
 */
ThingIdx find_thing_on_mapwho_tile_within_circle_with_bfilter(short tile_x, short tile_z, short X, short Z, ushort R,
  short ttype, short subtype, ThingBoolFilter filter, ThingFilterParams *params)
{
    ThingIdx thing;
    u32 k;

    k = 0;
    thing = get_mapwho_thing_index(tile_x, tile_z);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    // Our search radius could have exceeded expected one a bit
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
            thing = p_sthing->Next;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    // Our search radius could have exceeded expected one a bit
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
            thing = p_thing->Next;
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in mapwho (%d,%d) things list",
              (int)tile_x, (int)tile_z);
            bad_mapwho_tl_x = tile_x;
            bad_mapwho_tl_z = tile_z;
            break;
        }
    }
    return 0;
}

/** Searches for a thing of given type on the specific mapwho tile.
 * Besides being given a title, the thing must also meet circular range condition.
 */
ThingIdx find_thing_on_mapwho_tile_within_circle_with_mfilter(s32 *p_min_fval, short tile_x, short tile_z, short X, short Z, ushort R,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    s32 min_fval, fval;
    ThingIdx min_thing, thing;
    u32 k;

    k = 0;
    min_fval = INT32_MAX;
    min_thing = 0;
    thing = get_mapwho_thing_index(tile_x, tile_z);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    // Our search radius could have exceeded expected one a bit
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_sthing->Next;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    // Our search radius could have exceeded expected one a bit
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_thing->Next;
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in mapwho (%d,%d) things list",
              (int)tile_x, (int)tile_z);
            bad_mapwho_tl_x = tile_x;
            bad_mapwho_tl_z = tile_z;
            break;
        }
    }
    *p_min_fval = min_fval;
    return min_thing;
}

/**
 * Searches for thing of given type and subtype around tile under given coords, with bool filter.
 * Uses `spiral` checking of surrounding `mapwho` tiles, up to given number of tiles.
 *
 * @return Gives thing index, or 0 if not found.
 */
static ThingIdx find_thing_type_on_spiral_near_tile_with_bfilter(short X, short Z, ushort R, s32 spiral_len,
  short ttype, short subtype, ThingBoolFilter filter, ThingFilterParams *params)
{
    short tile_x, tile_z;
    int around;

    tile_x = MAPCOORD_TO_TILE(X);
    tile_z = MAPCOORD_TO_TILE(Z);
    for (around = 0; around < spiral_len; around++)
    {
        struct MapOffset *sstep;
        ThingIdx thing;
        s32 sX, sZ;

        sstep = &spiral_step[around];
        sX = tile_x + sstep->h;
        sZ = tile_z + sstep->v;
        thing = find_thing_on_mapwho_tile_within_circle_with_bfilter(sX, sZ, X, Z, R,
          ttype, subtype, filter, params);
        if (thing != 0)
            return thing;
    }
    return 0;
}

s32 mfilter_nearest(ThingIdx thing, short X, short Z, ThingFilterParams *params)
{
    short dtX, dtZ;

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
    return (dtZ * dtZ + dtX * dtX);
}

s32 mfilter_farthest(ThingIdx thing, short X, short Z, ThingFilterParams *params)
{
    short dtX, dtZ;

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
    return (MAP_COORD_WIDTH * MAP_COORD_HEIGHT) - (dtZ * dtZ + dtX * dtX);
}

/**
 * Searches for thing of given type and subtype around tile under given coords, with minimizing
 * filter.
 * Uses `spiral` checking of surrounding `mapwho` tiles, up to given number of tiles.
 *
 * @return Gives thing index, or 0 if not found.
 */
static ThingIdx find_thing_type_on_spiral_near_tile_with_mfilter(short X, short Z, ushort R, s32 spiral_len,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    s32 min_fval;
    ThingIdx min_thing;
    int around;
    short tile_x, tile_z;

    min_fval = INT32_MAX;
    min_thing = 0;
    tile_x = MAPCOORD_TO_TILE(X);
    tile_z = MAPCOORD_TO_TILE(Z);
    for (around = 0; around < spiral_len; around++)
    {
        struct MapOffset *sstep;
        s32 fval;
        ThingIdx thing;
        s32 sX, sZ;

        sstep = &spiral_step[around];
        sX = tile_x + sstep->h;
        sZ = tile_z + sstep->v;
        thing = find_thing_on_mapwho_tile_within_circle_with_mfilter(&fval, sX, sZ, X, Z, R,
          ttype, subtype, filter, params);
        if (fval < min_fval) {
            min_fval = fval;
            min_thing = thing;
            // If the minimizing factor is distance, then finding any match narrows the spiral
            // length to check (because we expect lower values further away)
            if (filter == mfilter_nearest)
                spiral_len = min(spiral_len, around * 2 + 4); // good enough estimate of where to finish
        }
    }
    return min_thing;
}

static ThingIdx find_thing_type_on_same_type_list_within_circle_with_bfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingBoolFilter filter, ThingFilterParams *params)
{
    ThingIdx thing;
    u32 k;

    k = 0;
    thing = get_thing_same_type_head(ttype, subtype);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
            thing = p_sthing->LinkSame;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
            thing = p_thing->LinkSame;
        }
        // If searching for all subtypes, make sure we really catch them all; switch to
        // second linked list if first one did not gave results
        if ((thing == 0) && (subtype == -1) && (ttype == TT_VEHICLE)) {
            subtype = SubTT_VEH_SHIP;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        if ((thing == 0) && (subtype == -1) && (ttype == TT_BUILDING)) {
            subtype = SubTT_BLD_MGUN;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
    }
    return 0;
}

static ThingIdx find_thing_type_on_same_type_list_within_circle_with_mfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    s32 min_fval, fval;
    ThingIdx min_thing, thing;
    u32 k;

    k = 0;
    min_fval = INT32_MAX;
    min_thing = 0;
    thing = get_thing_same_type_head(ttype, subtype);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_sthing->LinkSame;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_thing->LinkSame;
        }
        // If searching for all subtypes, make sure we really catch them all; switch to
        // second linked list if first one did not gave results
        if ((thing == 0) && (subtype == -1) && (ttype == TT_VEHICLE)) {
            subtype = SubTT_VEH_SHIP;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        if ((thing == 0) && (subtype == -1) && (ttype == TT_BUILDING)) {
            subtype = SubTT_BLD_MGUN;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
    }
    return min_thing;
}

static ThingIdx find_thing_type_on_same_type_list_outside_circle_with_mfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    s32 min_fval, fval;
    ThingIdx min_thing, thing;
    u32 k;

    k = 0;
    min_fval = INT32_MAX;
    min_thing = 0;
    thing = get_thing_same_type_head(ttype, subtype);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (!thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_sthing->LinkSame;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (!thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
            thing = p_thing->LinkSame;
        }
        // If searching for all subtypes, make sure we really catch them all; switch to
        // second linked list if first one did not gave results
        if ((thing == 0) && (subtype == -1) && (ttype == TT_VEHICLE)) {
            subtype = SubTT_VEH_SHIP;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        if ((thing == 0) && (subtype == -1) && (ttype == TT_BUILDING)) {
            subtype = SubTT_BLD_MGUN;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
    }
    return min_thing;
}

static short find_thing_type_on_same_type_list_with_bfilter(short ttype, short subtype,
  ThingBoolFilter filter, ThingFilterParams *params)
{
    ThingIdx thing;
    u32 k;

    k = 0;
    thing = get_thing_same_type_head(ttype, subtype);
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            // Per thing code start
            if (p_sthing->Type == ttype) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (filter(thing, params))
                        return thing;
                }
            }
            // Per thing code end
            thing = p_sthing->LinkSame;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            // Per thing code start
            if (p_thing->Type == ttype) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (filter(thing, params))
                        return thing;
                }
            }
            // Per thing code end
            thing = p_thing->LinkSame;
        }
        // If searching for all subtypes, make sure we really catch them all; switch to
        // second linked list if first one did not gave results
        if ((thing == 0) && (subtype == -1) && (ttype == TT_VEHICLE)) {
            subtype = SubTT_VEH_SHIP;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        if ((thing == 0) && (subtype == -1) && (ttype == TT_BUILDING)) {
            subtype = SubTT_BLD_MGUN;
            thing = get_thing_same_type_head(ttype, subtype);
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
    }
    return 0;
}

/** Slowest but most comprehensive search for things within given circle and meeting given
 * bool filter.
 * Unlike most search functions, this one allows searching for multiple thing types.
 *
 * @param X Map coordinate in map units.
 * @param Z Map coordinate in map units.
 * @param R Circle radius in map units.
 * @param ttype Thing Type; to catch all, use -1.
 * @param subtype Thing SubType; to catch all, use -1.
 * @param filter Filter callback function.
 * @param param Parameters for filter callback function.
 */
static ThingIdx find_thing_type_on_used_list_within_circle_with_bfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingBoolFilter filter, ThingFilterParams *params)
{
    ThingIdx thing;

    if ((ttype == -1) || thing_type_is_simple(ttype))
    {
        struct SimpleThing *p_sthing;
        for (thing = sthings_used_head; thing < 0; thing = p_sthing->LinkChild)
        {
            p_sthing = &sthings[thing];
            // Per thing code start
            if ((p_sthing->Type == ttype) || (ttype == -1)) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
        }
    }
    if ((ttype == -1) || !thing_type_is_simple(ttype))
    {
        struct Thing *p_thing;
        for (thing = things_used_head; thing > 0; thing = p_thing->LinkChild)
        {
            p_thing = &things[thing];
            // Per thing code start
            if ((p_thing->Type == ttype) || (ttype == -1)) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        if (filter(thing, params))
                            return thing;
                    }
                }
            }
            // Per thing code end
        }
    }
    return 0;
}

/** Slowest but most comprehensive search for things within given circle and meeting given
 * minimizing filter.
 * Unlike most search functions, this one allows searching for multiple thing types.
 *
 * @param X Map coordinate in map units.
 * @param Z Map coordinate in map units.
 * @param R Circle radius in map units.
 * @param ttype Thing Type; to catch all, use -1.
 * @param subtype Thing SubType; to catch all, use -1.
 * @param filter Filter callback function.
 * @param param Parameters for filter callback function.
 */
static ThingIdx find_thing_type_on_used_list_within_circle_with_mfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    s32 min_fval, fval;
    ThingIdx min_thing, thing;

    min_fval = INT32_MAX;
    min_thing = 0;
    if ((ttype == -1) || thing_type_is_simple(ttype))
    {
        struct SimpleThing *p_sthing;
        for (thing = sthings_used_head; thing < 0; thing = p_sthing->LinkChild)
        {
            p_sthing = &sthings[thing];
            // Per thing code start
            if ((p_sthing->Type == ttype) || (ttype == -1)) {
                if ((p_sthing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
        }
    }
    if ((ttype == -1) || !thing_type_is_simple(ttype))
    {
        struct Thing *p_thing;
        for (thing = things_used_head; thing > 0; thing = p_thing->LinkChild)
        {
            p_thing = &things[thing];
            // Per thing code start
            if ((p_thing->Type == ttype) || (ttype == -1)) {
                if ((p_thing->SubType == subtype) || (subtype == -1)) {
                    if (thing_is_within_circle(thing, X, Z, R)) {
                        fval = filter(thing, X, Z, params);
                        if (fval < min_fval) {
                            min_fval = fval;
                            min_thing = thing;
                        }
                    }
                }
            }
            // Per thing code end
        }
    }
    return min_thing;
}

ThingIdx find_thing_type_within_circle_with_bfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingBoolFilter filter, ThingFilterParams *params)
{
    ushort tile_dist;
    ThingIdx thing;

    tile_dist = MAPCOORD_TO_TILE(R + 255);
    if ((tile_dist <= spiral_dist_tiles_limit) && (ttype != -1))
    {
        thing = find_thing_type_on_spiral_near_tile_with_bfilter(X, Z, R,
          dist_tiles_to_spiral_step[tile_dist], ttype, subtype, filter, params);
    }
    else if ((ttype == TT_PERSON) || (ttype == TT_UNKN4) || (ttype == TT_VEHICLE) || (ttype == TT_BUILDING))
    {
        thing = find_thing_type_on_same_type_list_within_circle_with_bfilter(
          X, Z, R, ttype, subtype, filter, params);
    }
    else
    {
        // do slow search on all existing things
        // if ttype == -1, then this is the only option possible
        thing = find_thing_type_on_used_list_within_circle_with_bfilter(
          X, Z, R, ttype, subtype, filter, params);
    }
    return thing;
}

ThingIdx find_thing_type_within_circle_with_mfilter(short X, short Z, ushort R,
  short ttype, short subtype, ThingMinFilter filter, ThingFilterParams *params)
{
    ushort tile_dist;
    ThingIdx thing;

    tile_dist = MAPCOORD_TO_TILE(R + 255);
    if ((tile_dist <= spiral_dist_tiles_limit) && (ttype != -1))
    {
        thing = find_thing_type_on_spiral_near_tile_with_mfilter(X, Z, R,
          dist_tiles_to_spiral_step[tile_dist], ttype, subtype, filter, params);
    }
    else if ((ttype == TT_PERSON) || (ttype == TT_UNKN4) || (ttype == TT_VEHICLE) || (ttype == TT_BUILDING))
    {
        thing = find_thing_type_on_same_type_list_within_circle_with_mfilter(
          X, Z, R, ttype, subtype, filter, params);
    }
    else
    {
        // do slow search on all existing things
        thing = find_thing_type_on_used_list_within_circle_with_mfilter(
          X, Z, R, ttype, subtype, filter, params);
    }
    return thing;
}

TbBool bfilter_match_all(ThingIdx thing, ThingFilterParams *params)
{
    return true;
}

TbBool bfilter_item_is_weapon(ThingIdx thing, ThingFilterParams *params)
{
    struct SimpleThing *p_sthing;
    short weapon;

    if (thing >= 0)
        return false;
    weapon = params->Arg1;

    p_sthing = &sthings[thing];
    if ((p_sthing->Type != SmTT_DROPPED_ITEM) && (p_sthing->Type != SmTT_CARRIED_ITEM))
        return false;

    return (weapon == -1) || (p_sthing->U.UWeapon.WeaponType == weapon);
}

TbBool bfilter_person_carries_weapon(ThingIdx thing, ThingFilterParams *params)
{
    struct Thing *p_thing;
    short weapon;

    if (thing <= 0)
        return false;
    weapon = params->Arg1;

    p_thing = &things[thing];
    if (p_thing->Type != TT_PERSON)
        return false;

    return (weapon == -1) || person_carries_weapon(p_thing, weapon);
}

TbBool bfilter_person_can_be_persuaded_now(ThingIdx thing, ThingFilterParams *params)
{
    struct Thing *p_thing;
    ThingIdx attacker;
    short weapon_range;
    ushort target_select;
    ushort energy_reqd;
    TbBool found;

    if (thing <= 0)
        return false;

    attacker = params->Arg1;
    weapon_range = params->Arg2;
    target_select = params->Arg3;
    energy_reqd = -1;

    p_thing = &things[thing];
    if (p_thing->Type != TT_PERSON)
        return false;

    found = person_can_be_persuaded_now(attacker, thing, weapon_range, target_select, &energy_reqd);
    if (found)
        params->Arg4 = energy_reqd;
    return found;
}

TbBool bfilter_waiting_stopped(ThingIdx thing, ThingFilterParams *params)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];

    if ((p_thing->U.UVehicle.LeisurePlace == 0) || (p_thing->Speed != 0))
        return false;

    return true;
}

short find_dropped_weapon_within_circle(short X, short Z, ushort R, short weapon)
{
    ThingIdx thing;
    ThingFilterParams params;

    params.Arg1 = weapon;
    thing = find_thing_type_within_circle_with_bfilter(X, Z, R, SmTT_DROPPED_ITEM, 0, bfilter_item_is_weapon, &params);

    return thing;
}

ThingIdx find_person_carrying_weapon_within_circle(short X, short Z, ushort R, short weapon)
{
    ThingIdx thing;
    ThingFilterParams params;

    params.Arg1 = weapon;
    thing = find_thing_type_within_circle_with_bfilter(X, Z, R, TT_PERSON, -1,
      bfilter_person_carries_weapon, &params);

    return thing;
}

ThingIdx find_person_which_can_be_persuaded_now(short X, short Z, ushort R,
  ThingIdx attacker, ushort target_select, ushort *energy_reqd)
{
    ThingIdx thing;
    ThingFilterParams params;

    params.Arg1 = attacker;
    params.Arg2 = R;
    params.Arg3 = target_select;
    params.Arg4 = -1;
    thing = find_thing_type_within_circle_with_bfilter(X, Z, R, TT_PERSON, -1,
      bfilter_person_can_be_persuaded_now, &params);
    *energy_reqd = params.Arg4;

    return thing;
}
static DIFFTEST_REF_FUNC int find_nearest_player_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_find_nearest_player\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments are the raw registers the asm
 * takes (eax, edx, ebx, ecx); only eax (the person) is used.
 * Returns, in eax, the nearest thing among the groups of all players in
 * the net game, or 0. The conf line says void, but the wrapper passes
 * eax through and the asm caller uses it, so we return it.
 */
int find_nearest_player(intptr_t a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_thing;
    u32 best_dist, dist;
    s32 nearest, dx, dz;
    ushort plyr;
    ThingIdx thing;

    DIFFTEST_SHADOW_RET(find_nearest_player, int,
      find_nearest_player_asm(a1, a2, a3, a4));

    best_dist = 0xFFFFFFFF;
    nearest = 0;
    for (plyr = 0; plyr < 8; plyr++)
    {
        if ((ingame.InNetGame_UNSURE & (1 << plyr)) == 0)
            continue;
        for (thing = same_type_head[256 + level_def.PlayableGroups[plyr]];
          thing != 0; thing = p_thing->LinkSameGroup)
        {
            p_thing = &things[thing];
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                continue;
            dx = p_person->X - p_thing->X;
            if (dx < 0)
                dx = -dx;
            dx >>= 8;
            dz = p_person->Z - p_thing->Z;
            if (dz < 0)
                dz = -dz;
            dz >>= 8;
            if (dx <= dz)
                dx >>= 1;
            else
                dz >>= 1;
            dist = dz + dx;
            if (dist < best_dist)
            {
                best_dist = dist;
                nearest = (ushort)thing;
            }
        }
    }
    DIFFTEST_RETURN(nearest);
}


static DIFFTEST_REF_FUNC short find_nearest_from_group_asm(struct Thing *p_person, ushort group, ubyte no_persuaded)
{
    short ret;
    DIFFTEST_ASM("call ASM_find_nearest_from_group\n"
        : "=a" (ret) : "a" (p_person), "d" (group), "b" (no_persuaded));
    return ret;
}

static s32 find_nearest_from_group_dist(struct Thing *p_person, struct Thing *p_thing)
{
    s32 dx, dz;

    dx = p_person->X - p_thing->X;
    if (dx < 0)
        dx = -dx;
    dz = p_person->Z - p_thing->Z;
    if (dz < 0)
        dz = -dz;
    dx >>= 8;
    dz >>= 8;
    if (dx > dz)
        dz >>= 1;
    else
        dx >>= 1;
    return dx + dz;
}

short find_nearest_from_group(struct Thing *p_person, ushort group, ubyte no_persuaded)
{
    struct Thing *p_thing;
    u32 best_dist, dist;
    s32 nearest;
    short thing;
    short i;

    DIFFTEST_SHADOW_RET(find_nearest_from_group, short,
      find_nearest_from_group_asm(p_person, group, no_persuaded));

    if (in_network_game && (group == 0))
    {
        short ret;
        ret = find_nearest_player((intptr_t)p_person, 0, 0, 0);
        DIFFTEST_RETURN(ret);
    }

    best_dist = 0xFFFFFFFF;
    nearest = 0;

    for (thing = same_type_head[256 + group]; thing != 0;
      thing = p_thing->LinkSameGroup)
    {
        p_thing = &things[thing];
        if (no_persuaded && (p_thing->Flag & TngF_Persuaded) != 0)
            continue;
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            continue;
        dist = find_nearest_from_group_dist(p_person, p_thing);
        if (dist < best_dist)
        {
            best_dist = dist;
            nearest = (ushort)thing;
        }
    }

    if (no_persuaded)
        DIFFTEST_RETURN((short)nearest);

    for (i = 1; i < (s32)word_1531DA; i++)
    {
        p_thing = &things[word_17FA58[i]];
        if (p_thing->U.UPerson.EffectiveGroup != group)
            continue;
        dist = find_nearest_from_group_dist(p_person, p_thing);
        if (dist < best_dist)
        {
            best_dist = dist;
            nearest = p_thing->ThingOffset;
        }
    }

    DIFFTEST_RETURN((short)nearest);
}

ThingIdx find_person_carrying_weapon(short weapon)
{
    ThingIdx thing;
    ThingFilterParams params;

    params.Arg1 = weapon;
    thing = find_thing_type_on_same_type_list_with_bfilter(TT_PERSON, -1, bfilter_person_carries_weapon, &params);

    return thing;
}

ThingIdx search_for_vehicle(short X, short Z)
{
    ThingIdx thing;
    ThingFilterParams params;

    // Try finding very close to target coords
    thing = find_thing_type_within_circle_with_bfilter(X, Z, 48, TT_VEHICLE, 0, bfilter_match_all, &params);
    // If very clos search failed, retry with bigger radius
    if (thing == 0)
        thing = find_thing_type_within_circle_with_bfilter(X, Z, 1024, TT_VEHICLE, 0, bfilter_match_all, &params);

    return thing;
}

ThingIdx search_things_for_index(short index)
{
    ThingIdx thing;
    if (index <= 0)
    {
        struct SimpleThing *p_sthing;
        for (thing = sthings_used_head; thing < 0; thing = p_sthing->LinkChild)
        {
            p_sthing = &sthings[thing];
            if (index == p_sthing->ThingOffset) {
                return thing;
            }
        }
    }
    else
    {
        struct Thing *p_thing;
        for (thing = things_used_head; thing > 0; thing = p_thing->LinkChild)
        {
            p_thing = &things[thing];
            if (index == p_thing->ThingOffset) {
                if (p_thing->Type != TT_VEH_TURRET)
                    return thing;
            }
        }
    }
    return 0;
}

ThingIdx search_things_for_any_including_offmap_nearest_within_circle(short X, short Z, ushort R)
{
    ThingIdx thing;
    ThingFilterParams params;

    thing = find_thing_type_within_circle_with_mfilter(X, Z, R, -1, -1, mfilter_nearest, &params);

    return thing;
}

ThingIdx search_things_for_type_farthest_from_xz(short X, short Z, short ttype, short subtype)
{
    ThingIdx thing;
    ThingFilterParams params;

    thing = find_thing_type_on_same_type_list_outside_circle_with_mfilter(X, Z,
      TILE_TO_MAPCOORD(1,0), ttype, subtype, mfilter_farthest, &params);

    return thing;
}

static DIFFTEST_REF_FUNC ThingIdx search_things_for_uniqueid_asm(short uniqid, ubyte flag)
{
    ThingIdx ret;
    DIFFTEST_ASM("call ASM_search_things_for_uniqueid\n"
        : "=a" (ret) : "a" (uniqid), "d" (flag));
    return ret;
}

ThingIdx search_things_for_uniqueid(short uniqid, ubyte flag)
{
    ThingIdx thing;
    ThingIdx found;

    DIFFTEST_SHADOW_RET(search_things_for_uniqueid, ThingIdx,
      search_things_for_uniqueid_asm(uniqid, flag));
    if (flag)
    {
        for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
        {
            struct Thing *p_thing = &things[thing];

            if (p_thing->Type == TT_BUILDING)
                continue;
            if ((s32)uniqid == (s32)p_thing->U.UPerson.UniqueID)
                DIFFTEST_RETURN(thing);
        }
        DIFFTEST_RETURN((ThingIdx)0);
    }
    // TODO the asm returns an uninitialized register (edi) when no simple
    // thing matches; we return 0 instead. It also keeps the last match.
    found = 0;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing = &sthings[thing];

        if ((s32)uniqid == (s32)p_sthing->UniqueID)
            found = thing;
    }
    DIFFTEST_RETURN(found);
}

static DIFFTEST_REF_FUNC ThingIdx find_nearest_object2_asm(short mx, short mz, ushort sub_type)
{
    ThingIdx ret;
    DIFFTEST_ASM("call ASM_find_nearest_object2\n"
        : "=a" (ret) : "a" (mx), "d" (mz), "b" (sub_type));
    return ret;
}

ThingIdx find_nearest_object2(short mx, short mz, ushort sub_type)
{
    s32 tile_x, tile_z;
    s32 x_beg, x_end, z_beg, z_end;
    u32 best_dist;
    ThingIdx best_thing;

    DIFFTEST_SHADOW_RET(find_nearest_object2, ThingIdx,
      find_nearest_object2_asm(mx, mz, sub_type));

    tile_x = mx >> 8;
    tile_z = mz >> 8;
    x_beg = tile_x - 20;
    x_end = tile_x + 20;
    z_beg = tile_z - 20;
    z_end = tile_z + 20;
    best_dist = 0x7fffffff;
    best_thing = 0;

    for (tile_x = x_beg; tile_x < x_end; tile_x++)
    {
        for (tile_z = z_beg; tile_z < z_end; tile_z++)
        {
            ThingIdx thing;

            if (tile_x >= 128 || tile_z >= 128 || tile_x <= 0 || tile_z <= 0)
                continue;

            thing = game_my_big_map[tile_z * 128 + tile_x].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;

                if (thing <= 0) {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                if (p_thing->Type == TT_BUILDING &&
                  (sub_type == 0 || p_thing->SubType == sub_type))
                {
                    struct SingleObject *p_sobj;
                    s32 dx, dz;
                    u32 dist;

                    p_sobj = &game_objects[p_thing->U.UObject.Object];
                    dx = p_sobj->MapX - mx;
                    dz = p_sobj->MapZ - mz;
                    dist = (u32)(dz * dz + dx * dx);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_thing = thing;
                    }
                }
                thing = p_thing->Next;
            }
        }
    }
    DIFFTEST_RETURN(best_thing);
}

static DIFFTEST_REF_FUNC short search_object_for_qface_asm(ushort object, ubyte gflag, ubyte flag, ushort after)
{
    short ret;
    DIFFTEST_ASM("call ASM_search_object_for_qface\n"
        : "=a" (ret) : "a" (object), "d" (gflag), "b" (flag), "c" (after));
    return ret;
}

short search_object_for_qface(ushort object, ubyte gflag, ubyte flag, ushort after)
{
    struct SingleObject *p_sobj;
    struct SingleObjectFace4 *p_face4;
    s32 end_face;
    short face;

    DIFFTEST_SHADOW_RET(search_object_for_qface, short,
      search_object_for_qface_asm(object, gflag, flag, after));
    p_sobj = &game_objects[object];
    if ((ushort)p_sobj->NumbFaces4 == 0)
        DIFFTEST_RETURN((short)0);

    p_face4 = &game_object_faces4[(ushort)p_sobj->StartFace4];
    face = (short)p_sobj->StartFace4;
    for (; ; p_face4++, face++)
    {
        end_face = (s32)(ushort)p_sobj->StartFace4 + (s32)(ushort)p_sobj->NumbFaces4;
        if ((s32)face >= end_face)
            break;
        if ((s32)face <= (s32)after)
            continue;
        if ((p_face4->GFlags & gflag) == gflag && p_face4->Flags == flag)
            DIFFTEST_RETURN(face);
    }
    DIFFTEST_RETURN((short)0);
}

ThingIdx search_for_station(short X, short Z)
{
    ThingIdx thing;
    ThingFilterParams params;

    thing = find_thing_type_within_circle_with_bfilter(X, Z, TILE_TO_MAPCOORD(15,0),
      TT_BUILDING, SubTT_BLD_STATION, bfilter_match_all, &params);

    return thing;
}

ThingIdx search_for_ferry(short X, short Y, short Z, ushort R)
{
    ThingIdx thing;
    ThingFilterParams params;

    params.Arg1 = Y;
    params.Arg2 = R;
    thing = find_thing_type_within_circle_with_bfilter(X, Z, R,
      TT_VEHICLE, SubTT_VEH_SHIP, bfilter_waiting_stopped, &params);

    return thing;
}

struct Thing *check_for_radius_hit_person(int prc_x, int prc_y, int prc_z,
        int radius, struct Thing *p_owner, int flag, int skip)
{
    ThingIdx thing;
    short dtx, dtz;
    int cor_x, cor_y, cor_z;

    cor_x = PRCCOORD_TO_MAPCOORD(prc_x);
    cor_y = PRCCOORD_TO_MAPCOORD(prc_y);
    cor_z = PRCCOORD_TO_MAPCOORD(prc_z);

    for (dtx = -1; dtx <= 1; dtx++)
    {
      for (dtz = -1; dtz <= 1; dtz++)
      {
        uint tile_x, tile_z;

        tile_x = MAPCOORD_TO_TILE(cor_x) + dtx;
        tile_z = MAPCOORD_TO_TILE(cor_z) + dtz;
        if ((tile_x >= MAP_TILE_WIDTH) || (tile_z >= MAP_TILE_HEIGHT))
            continue;
        thing = game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x].Child;
        while (thing != 0)
        {
            if (thing <= 0)
            {
              struct SimpleThing *p_sthing;

              p_sthing = &sthings[thing];
              if (p_sthing->Type == SmTT_STATIC && (p_sthing->Flag & TngF_Destroyed) == 0 && p_sthing->Frame != 1008)
              {
                  if (thing_intersects_cylinder(thing, cor_x, cor_y, cor_z, radius, 25)) {
                      if (--skip < 0) {
                          if ((p_sthing->StartFrame <= 1004) || (p_sthing->StartFrame >= 1008))
                              return (struct Thing *)p_sthing;
                          set_static_on_fire(p_sthing);
                      }
                  }
              }
              thing = p_sthing->Next;
            }
            else
            {
              struct Thing *p_thing;
              ushort group;

              p_thing = &things[thing];
              if (p_owner != NULL)
                  group = p_owner->U.UPerson.EffectiveGroup;
              else
                  group = 99;
              if ((p_thing->U.UPerson.EffectiveGroup != group) || (group >= 100))
              {
                if (!groups_have_truce(group, p_thing->U.UPerson.EffectiveGroup)
                  && (p_thing->Type == TT_PERSON) && (p_thing != p_owner))
                {
                    if ((p_thing->State != PerSt_DEAD) && (p_thing->State != PerSt_PERSON_BURNING) && (p_thing->Flag & TngF_Destroyed) == 0)
                    {
                        if (thing_intersects_cylinder(thing, cor_x, cor_y, cor_z, radius, 25)) {
                            if (--skip < 0)
                                return p_thing;
                        }
                    }
                }
                if (--skip < 0)
                {
                  if (flag)
                  {
                    if ((p_thing->Type == TT_VEHICLE) && (p_thing->State != VehSt_UNKN_D)
                      && (p_owner == NULL || p_thing->ThingOffset != p_owner->U.UPerson.Vehicle))
                    {
                        if (thing_intersects_cylinder(thing, cor_x, cor_y, cor_z, radius, 25)) {
                            if (--skip < 0)
                                return p_thing;
                        }
                    }
                  }
                }
                if (flag)
                {
                    if ((p_thing->Type == TT_BUILDING) && (p_thing->SubType == SubTT_BLD_MGUN) && ((p_thing->Flag & TngF_Destroyed) == 0))
                    {
                        if (thing_intersects_cylinder(thing, cor_x, cor_y, cor_z, radius, 25)) {
                            if (--skip < 0)
                                return p_thing;
                        }
                    }
                }
                if (flag)
                {
                    if ((p_thing->Type == TT_MINE) && (p_thing->SubType == 48) && (p_thing->Flag & TngF_Destroyed) == 0 && (p_thing->State != 13))
                    {
                        if (thing_intersects_cylinder(thing, cor_x, cor_y, cor_z, radius, 25)) {
                            if (--skip < 0)
                                return p_thing;
                        }
                    }
                }
              }
              thing = p_thing->Next;
            }
        }
      }
    }
    return NULL;
}

static DIFFTEST_REF_FUNC short find_nearest_person_min_asm(int x, int y, int z,
  int n_dist, int *a_dist, int angle1, u32 group_bits)
{
    short ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)a_dist;
    stkargs[1] = (int)(intptr_t)angle1;
    stkargs[2] = (int)(intptr_t)group_bits;

    DIFFTEST_ASM(
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_find_nearest_person_min\n"
        : "=a" (ret)
        : "a" (x), "d" (y), "b" (z), "c" (n_dist), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

short find_nearest_person_min(int x, int y, int z,
  int n_dist, int *a_dist, int angle1, u32 group_bits)
{
    ThingIdx thing;
    ThingIdx nearest;
    u32 best_dist;

    DIFFTEST_SHADOW_RET(find_nearest_person_min, short,
      find_nearest_person_min_asm(x, y, z, n_dist, a_dist, angle1, group_bits));
    best_dist = 0xFFFFFFFF;
    nearest = 0;
    for (thing = same_type_head[1]; thing != 0; thing = things[thing].LinkSame)
    {
        struct Thing *p_thing;
        s32 dt_x, dt_z;
        u32 dist;
        ubyte outside_angle;

        p_thing = &things[thing];
        if ((group_bits & (1u << (p_thing->U.UPerson.EffectiveGroup & 0x1F))) == 0)
            continue;
        if ((p_thing->Flag2 & TgF2_Unkn0020) != 0)
            continue;
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            continue;
        dt_x = (p_thing->X >> 8) - x;
        dt_z = (p_thing->Z >> 8) - z;
        outside_angle = 0;
        if ((short)angle1 >= 0)
        {
            short angdt;

            angdt = (short)((angle1 - arctan(dt_x, -dt_z) + 0x800) & 0x7FF);
            if ((angdt > 200) && (angdt < 0x737))
                outside_angle = 1;
        }
        if (outside_angle)
            continue;
        dist = (u32)dt_x * (u32)dt_x + (u32)dt_z * (u32)dt_z;
        if ((dist < best_dist) && (dist > (u32)n_dist))
        {
            best_dist = dist;
            nearest = thing;
        }
    }
    *a_dist = best_dist;
    DIFFTEST_RETURN((short)nearest);
}

static DIFFTEST_REF_FUNC struct Thing *check_for_people_asm(int x, int y, int z, ushort radius)
{
    struct Thing *ret;
    DIFFTEST_ASM(
      "call ASM_check_for_people\n"
        : "=a" (ret) : "a" (x), "d" (y), "b" (z), "c" (radius));
    return ret;
}

struct Thing *check_for_people(int x, int y, int z, ushort radius)
{
    short tile_x, tile_z;
    short dt_x, dt_z;

    DIFFTEST_SHADOW_RET(check_for_people, struct Thing *,
      check_for_people_asm(x, y, z, radius));
    tile_x = x >> 16;
    tile_z = z >> 16;
    for (dt_x = -1; dt_x < 2; dt_x++)
    {
        for (dt_z = -1; dt_z < 2; dt_z++)
        {
            ThingIdx thing;
            ushort k;
            s32 mx, mz;

            mx = tile_x + dt_x;
            if ((mx < 0) || (mx >= 128))
                continue;
            mz = tile_z + dt_z;
            if ((mz < 0) || (mz >= 128))
                continue;
            thing = game_my_big_map[(mz << 7) + mx].Child;
            k = 0;
            while (thing != 0)
            {
                struct Thing *p_thing;
                s32 dist_x, dist_y, dist_z, dist;

                if (k >= 400)
                    break;
                k++;
                if (thing <= 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                if ((p_thing->Type == TT_PERSON) && (p_thing->State != PerSt_DEAD))
                {
                    dist_y = (p_thing->Y - y) >> 8;
                    if ((dist_y < 25) && (dist_y > -25))
                    {
                        dist_x = abs((p_thing->X - x) >> 8);
                        dist_z = abs((p_thing->Z - z) >> 8);
                        if (dist_x <= dist_z)
                            dist = dist_z + (dist_x >> 1);
                        else
                            dist = dist_x + (dist_z >> 1);
                        if (dist < (s32)(short)radius + (s32)p_thing->Radius)
                            DIFFTEST_RETURN(p_thing);
                    }
                }
                thing = p_thing->Next;
            }
        }
    }
    DIFFTEST_RETURN((struct Thing *)NULL);
}

/******************************************************************************/
