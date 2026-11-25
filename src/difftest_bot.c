/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_bot.c
 *     Scripted player input for difftest mission runs.
 * @par Purpose:
 *     Plays the local player's agents during a difftest run, so that code
 *     reached only through player commands (picking items, vehicles,
 *     persuading, weapons use, protect/follow, moods) gets exercised.
 * @par Comment:
 *     Enabled by $SWARS_DIFFTEST_BOT, whose value seeds the bot's own random
 *     generator; the game's lbSeed is not touched. Decisions depend only on
 *     the game state and that generator, so two builds which behave the same
 *     receive the same packets.
 *     Every few turns one packet is built, in place of user input.
 *     $SWARS_DIFFTEST_BOT_LOG names a file to list the packets in.
 * @author   SyndicateVR decomp agent
 * @date     11 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bigmap.h"
#include "bmcrater.h"
#include "building.h"
#include "delaunay.h"
#include "enginsngobjs.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "packet.h"
#include "pathtrig.h"
#include "people.h"
#include "player.h"
#include "plyr_usrinp.h"
#include "thing.h"
#include "triangls.h"
#include "trstate.h"
#include "vehicle.h"
#include "weapon.h"
/******************************************************************************/

#if DIFFTEST_HARNESS

/** Game turns between bot packets. */
#define BOT_TURN_INTERVAL 6
/** Search range for targets around an agent, in map coords. */
#define BOT_RANGE 4096

static TbBool bot_tried = false;
static TbBool bot_on = false;
static u32 bot_seed;
static FILE *bot_log = NULL;
/** BAT billboard area (map tiles) and minigame state, asm data from bat.c */
extern int bat_unknvalA_min;
extern int bat_unknvalB_min;
extern int bat_unknvalA_max;
extern int bat_unknvalB_max;
extern int BAT_state;
/** Agent teleported next to a vehicle, and that vehicle. */
static ThingIdx bot_tp_agent = 0;
static ThingIdx bot_tp_vehicle = 0;
/** Agent sent to the BAT billboard area. */
static ThingIdx bot_bat_agent = 0;

static u32 bot_rand(u32 range)
{
    return difftest_fuzz_rand(&bot_seed) % range;
}

static TbBool bot_on_mapwho(struct Thing *p_thing);

static TbBool bot_agent_usable(struct Thing *p_agent)
{
    if (p_agent == NULL || p_agent->Type != TT_PERSON)
        return false;
    if ((p_agent->Flag & TngF_Destroyed) != 0)
        return false;
    if (p_agent->State == PerSt_DEAD)
        return false;
    // While a mission script drives the agent, ComCur is a command index
    // and not the player slot, which packet handlers expect
    if (person_is_executing_commands(p_agent->ThingOffset))
        return false;
    // In some missions (0,84) ComCur is not the slot even without a script
    if ((p_agent->U.UPerson.ComCur >> 2) != local_player_no)
        return false;
    // Entering and leaving a shuttle pod in quick succession leaves the
    // agent flagged in a vehicle (Vehicle 0) yet linked on the map, as in
    // the asm; a self destruct then links it twice and scare_civs() loops
    // forever (3,91+bot)
    if (((p_agent->Flag & TngF_InVehicle) != 0) && bot_on_mapwho(p_agent))
        return false;
    return true;
}

/** Cheat packets act on all agents; agents driven by a mission script
 * cannot take them (see bot_agent_usable()).
 */
static TbBool bot_cheat_allowed(void)
{
    PlayerInfo *p_locplayer;
    int i;

    p_locplayer = &players[local_player_no];
    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent = p_locplayer->MyAgent[i];
        if (p_agent == NULL || p_agent->Type != TT_PERSON)
            continue;
        if (person_is_executing_commands(p_agent->ThingOffset))
            return false;
    }
    return true;
}

static int bot_dist(struct Thing *p_agent, s32 x, s32 z)
{
    return abs(PRCCOORD_TO_MAPCOORD(p_agent->X - x)) +
      abs(PRCCOORD_TO_MAPCOORD(p_agent->Z - z));
}

/** Picks a random thing of given type within range; for TT_PERSON, other
 * than the agent itself and alive; with no_own, also not one of the
 * player's agents (the UI does not offer them as targets).
 */
static ThingIdx bot_pick_thing(struct Thing *p_agent, ubyte ttype, TbBool no_own)
{
    ThingIdx thing, found[32];
    int n;

    n = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing = &things[thing];
        if (p_thing->Type != ttype || p_thing == p_agent)
            continue;
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            continue;
        if (ttype == TT_PERSON && p_thing->State == PerSt_DEAD)
            continue;
        if (no_own && ttype == TT_PERSON &&
          person_slot_as_player_agent(p_thing, local_player_no) >= 0)
            continue;
        if (bot_dist(p_agent, p_thing->X, p_thing->Z) > BOT_RANGE)
            continue;
        found[n++] = thing;
        if (n >= 32)
            break;
    }
    return (n > 0) ? found[bot_rand(n)] : 0;
}

/** Whether the thing is linked on the map tile of its position, as
 * on_mapwho() checks (a copy, so that the bot adds no calls to the
 * remade function). The teleport cheat relinks only such things; for
 * others it would link the thing twice, making a cycle (hang in
 * scare_civs() on 3,91+bot).
 */
static TbBool bot_on_mapwho(struct Thing *p_thing)
{
    short thing;

    thing = game_my_big_map[(p_thing->Z >> 16) * MAP_TILE_WIDTH +
      (p_thing->X >> 16)].Child;
    while (thing != 0)
    {
        if (thing == p_thing->ThingOffset)
            return true;
        if (thing > 0)
            thing = things[thing].Next;
        else
            thing = sthings[thing].Next;
    }
    return false;
}

/** Picks a vehicle anywhere on the map, or 0. Some (the mech in 1,102)
 * stand too far from the agents to reach them within a scenario. Picks a
 * vehicle subtype first, so that a lone mech or ship is as likely as the
 * many cars.
 */
static ThingIdx bot_pick_far_vehicle(void)
{
    ThingIdx thing, found[64];
    ubyte subtypes[64];
    int n, nsub, i;
    ubyte subtype;

    nsub = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing = &things[thing];
        if (p_thing->Type != TT_VEHICLE)
            continue;
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            continue;
        for (i = 0; i < nsub; i++) {
            if (subtypes[i] == p_thing->SubType)
                break;
        }
        if ((i == nsub) && (nsub < 64))
            subtypes[nsub++] = p_thing->SubType;
    }
    if (nsub == 0)
        return 0;
    subtype = subtypes[bot_rand(nsub)];

    n = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing = &things[thing];
        if (p_thing->Type != TT_VEHICLE || p_thing->SubType != subtype)
            continue;
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            continue;
        found[n++] = thing;
        if (n >= 64)
            break;
    }
    return (n > 0) ? found[bot_rand(n)] : 0;
}

/** Picks a dropped item within range, or 0. */
static ThingIdx bot_pick_item(struct Thing *p_agent)
{
    ThingIdx thing, found[16];
    int n;

    n = 0;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing = &sthings[thing];
        if (p_sthing->Type != SmTT_DROPPED_ITEM)
            continue;
        if (bot_dist(p_agent, p_sthing->X, p_sthing->Z) > BOT_RANGE)
            continue;
        found[n++] = thing;
        if (n >= 16)
            break;
    }
    return (n > 0) ? found[bot_rand(n)] : 0;
}

/** Picks a weapon the agent carries, or 0. */
static WeaponType bot_pick_weapon(struct Thing *p_agent)
{
    WeaponType wtypes[WEP_TYPES_COUNT];
    WeaponType wtype;
    int n;

    n = 0;
    for (wtype = WEP_NULL + 1; wtype < WEP_TYPES_COUNT; wtype++)
    {
        if (person_carries_weapon(p_agent, wtype))
            wtypes[n++] = wtype;
    }
    return (n > 0) ? wtypes[bot_rand(n)] : WEP_NULL;
}

/** Centre of a walkable face near the agent, in map coords; false if the
 * face is not walkable or out of range.
 */
static TbBool bot_face_centre(short face, struct Thing *p_agent, short *p_x, short *p_y, short *p_z)
{
    struct SingleObject *p_sobj;
    short pts[4];
    int i, npts;
    s32 x, y, z;

    if (face < 0) {
        struct SingleObjectFace4 *p_face4 = &game_object_faces4[-face];
        if ((p_face4->GFlags & FGFlg_Unkn04) == 0)
            return false;
        p_sobj = &game_objects[p_face4->Object];
        for (i = 0; i < 4; i++)
            pts[i] = p_face4->PointNo[i];
        npts = 4;
    } else {
        struct SingleObjectFace3 *p_face3 = &game_object_faces3[face];
        if ((p_face3->GFlags & FGFlg_Unkn04) == 0)
            return false;
        p_sobj = &game_objects[p_face3->Object];
        for (i = 0; i < 3; i++)
            pts[i] = p_face3->PointNo[i];
        npts = 3;
    }
    x = y = z = 0;
    for (i = 0; i < npts; i++)
    {
        struct SinglePoint *p_pt = &game_object_points[(ushort)pts[i]];
        x += p_pt->X;
        y += p_pt->Y;
        z += p_pt->Z;
    }
    x = p_sobj->MapX + x / npts;
    z = p_sobj->MapZ + z / npts;
    if (x < 256 || x > MAP_COORD_WIDTH - 256)
        return false;
    if (z < 256 || z > MAP_COORD_HEIGHT - 256)
        return false;
    if (abs(PRCCOORD_TO_MAPCOORD(p_agent->X) - x) +
      abs(PRCCOORD_TO_MAPCOORD(p_agent->Z) - z) > BOT_RANGE)
        return false;
    *p_x = x;
    *p_y = y / npts;
    *p_z = z;
    return true;
}

/** Picks a walkable object face (building roofs, walkways) near the
 * agent, as the mouse would point at it. Returns the face index as the
 * game stores it in Target/OnFace (negative for quads), or 0; sets the
 * centre point of the face in map coords. Scans from a random face and
 * picks among the first few found.
 */
static short bot_pick_face(struct Thing *p_agent, short *p_x, short *p_y, short *p_z)
{
    short found[16];
    short fx[16], fy[16], fz[16];
    int n, k, nfaces, start;

    nfaces = (next_object_face3 - 1) + (next_object_face4 - 1);
    if (nfaces <= 0)
        return 0;
    start = bot_rand(nfaces);
    n = 0;
    for (k = 0; k < nfaces && n < 16; k++)
    {
        int idx = (start + k) % nfaces;
        short face;

        if (idx < next_object_face3 - 1)
            face = 1 + idx;
        else
            face = -(1 + idx - (next_object_face3 - 1));
        if (bot_face_centre(face, p_agent, &fx[n], &fy[n], &fz[n]))
            found[n++] = face;
    }
    if (n == 0)
        return 0;
    k = bot_rand(n);
    *p_x = fx[k];
    *p_y = fy[k];
    *p_z = fz[k];
    return found[k];
}

/** The walkable quad face of a bezier road (raised curvy road) nearest to
 * the agent within bot range, or 0. Draws no random numbers.
 */
static short bot_bezier_face(struct Thing *p_agent, short *p_x, short *p_y, short *p_z)
{
    short best;
    s32 best_dist;
    int i;

    best = 0;
    best_dist = 0;
    for (i = 1; i < next_object_face4; i++)
    {
        struct SingleObjectFace4 *p_face4;
        struct Thing *p_building;
        short x, y, z;
        s32 dist;

        p_face4 = &game_object_faces4[i];
        p_building = &things[(ushort)game_objects[p_face4->Object].ThingNo];
        if ((p_building->Type != TT_BUILDING) ||
          (p_building->SubType != SubTT_BLD_BEZIER_ROAD))
            continue;
        if (!bot_face_centre(-i, p_agent, &x, &y, &z))
            continue;
        dist = abs(PRCCOORD_TO_MAPCOORD(p_agent->X) - x) +
          abs(PRCCOORD_TO_MAPCOORD(p_agent->Z) - z);
        if ((best == 0) || (dist < best_dist)) {
            best = -i;
            best_dist = dist;
            *p_x = x;
            *p_y = y;
            *p_z = z;
        }
    }
    return best;
}

/** A ground point near the agent, in map coords. */
static void bot_near_point(struct Thing *p_agent, short *p_x, short *p_y, short *p_z, int spread)
{
    int x, z;

    x = PRCCOORD_TO_MAPCOORD(p_agent->X) + (int)bot_rand(2 * spread) - spread;
    z = PRCCOORD_TO_MAPCOORD(p_agent->Z) + (int)bot_rand(2 * spread) - spread;
    if (x < 256) x = 256;
    if (x > MAP_COORD_WIDTH - 256) x = MAP_COORD_WIDTH - 256;
    if (z < 256) z = 256;
    if (z > MAP_COORD_HEIGHT - 256) z = MAP_COORD_HEIGHT - 256;
    *p_x = x;
    *p_z = z;
    *p_y = (alt_at_point(x, z) >> 8) + 20;
}

static void bot_build(struct Thing *p_agent, struct Packet *p_pckt)
{
    ThingIdx agent, target;
    short x, y, z;
    WeaponType wtype;
    short face;

    agent = p_agent->ThingOffset;
    // Within a vehicle only drive or leave; weapons fired from there take
    // the vehicle as the shot start, which most vehicles cannot be
    if (person_is_in_a_vehicle(p_agent))
    {
        if (bot_rand(3) == 0) {
            build_packet(p_pckt, PAct_LEAVE_VEHICLE, agent, 0, 0, 0);
        } else {
            bot_near_point(p_agent, &x, &y, &z, 4096);
            // Every other drive turn onto a bezier road nearby, if any, as
            // clicking on it does (find_bezier_nav_point); no random draw
            face = 0;
            if (((gameturn / BOT_TURN_INTERVAL) % 2) == 0)
                face = bot_bezier_face(p_agent, &x, &y, &z);
            if (face != 0)
                build_packet(p_pckt, PAct_AGENT_GOTO_FACE_PT_ABS, agent, x, face, z);
            else
                build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS, agent, x, y, z);
        }
        return;
    }
    // On a map with a BAT billboard area (map 30), now and then send one
    // agent there to hold the Persuadertron: it starts the Breakout
    // minigame on the billboard (BAT_unknsub_27()). That agent then
    // teleports, selects the weapon and waits until the game starts.
    // Other maps draw no extra random numbers.
    if ((bat_unknvalA_max > 0) && (BAT_state == 0))
    {
        if ((bot_bat_agent == 0) &&
          person_carries_weapon(p_agent, WEP_PERSUADRTRN) &&
          (bot_rand(8) == 0))
            bot_bat_agent = agent;
        if (agent == bot_bat_agent)
        {
            int cx, cz;

            cx = (bat_unknvalA_min + bat_unknvalA_max + 1) * 128;
            cz = (bat_unknvalB_min + bat_unknvalB_max + 1) * 128;
            if (bot_dist(p_agent, MAPCOORD_TO_PRCCOORD(cx, 0),
              MAPCOORD_TO_PRCCOORD(cz, 0)) > 512) {
                if (!bot_on_mapwho(p_agent) || !bot_cheat_allowed()) {
                    bot_bat_agent = 0;
                    return;
                }
                build_packet(p_pckt, PAct_CHEAT_AGENT_TELEPORT, agent,
                  cx, 0, cz);
                return;
            }
            if (p_agent->U.UPerson.CurrentWeapon != WEP_PERSUADRTRN)
                build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, agent,
                  WEP_PERSUADRTRN, WepSel_TOGGLE, 0);
            return;
        }
    }
    else if (BAT_state != 0)
    {
        bot_bat_agent = 0;
    }
    // Right after a teleport next to a vehicle, get into it
    if ((agent == bot_tp_agent) && (bot_tp_vehicle != 0))
    {
        // Weapon away first (see case 7), keeping the plan
        if (p_agent->U.UPerson.CurrentWeapon != WEP_NULL) {
            build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, agent,
              p_agent->U.UPerson.CurrentWeapon, WepSel_HIDE, 0);
            return;
        }
        target = bot_tp_vehicle;
        bot_tp_agent = 0;
        bot_tp_vehicle = 0;
        if ((things[target].Type == TT_VEHICLE) &&
          ((things[target].Flag & TngF_Destroyed) == 0)) {
            build_packet(p_pckt, PAct_GO_ENTER_VEHICLE, agent, target, 0, 0);
            return;
        }
    }
    switch (bot_rand(26))
    {
    case 0:
    case 1:
        bot_near_point(p_agent, &x, &y, &z, 2048);
        build_packet(p_pckt, bot_rand(2) ? PAct_AGENT_GOTO_GND_PT_ABS :
          PAct_AGENT_GOTO_GND_PT_ABS_FF, agent, x, y, z);
        break;
    case 2:
        x = (int)bot_rand(2048) - 1024;
        z = (int)bot_rand(2048) - 1024;
        build_packet(p_pckt, bot_rand(2) ? PAct_AGENT_GOTO_GND_PT_REL :
          PAct_AGENT_GOTO_GND_PT_REL_FF, agent, x, 0, z);
        break;
    case 3:
        // Walk onto a roof or walkway face, as clicking on it does
        face = bot_pick_face(p_agent, &x, &y, &z);
        if (face == 0)
            break;
        build_packet(p_pckt, bot_rand(2) ? PAct_AGENT_GOTO_FACE_PT_ABS :
          PAct_AGENT_GOTO_FACE_PT_ABS_FF, agent, x, face, z);
        break;
    case 4:
    case 5:
        target = bot_pick_item(p_agent);
        if (target == 0)
            break;
        build_packet(p_pckt, bot_rand(2) ? PAct_GET_ITEM : PAct_GET_ITEM_FAST,
          agent, target, 0, 0);
        break;
    case 6:
        if (p_agent->U.UPerson.StandOnThing >= 0)
            break;
        build_packet(p_pckt, PAct_PICKUP, agent,
          p_agent->U.UPerson.StandOnThing, 0, 0);
        break;
    case 7:
    case 8:
        // Put the weapon away first; a shot still going on once the agent
        // is inside starts from the vehicle and asserts
        if (p_agent->U.UPerson.CurrentWeapon != WEP_NULL) {
            build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, agent,
              p_agent->U.UPerson.CurrentWeapon, WepSel_HIDE, 0);
            break;
        }
        target = bot_pick_thing(p_agent, TT_VEHICLE, false);
        if ((target == 0) || (bot_rand(4) == 0)) {
            // None near, or now and then: teleport next to one anywhere
            // (cheat); the agent's next action enters it
            if (!bot_on_mapwho(p_agent))
                break;
            target = bot_pick_far_vehicle();
            if (target == 0 || !bot_cheat_allowed())
                break;
            build_packet(p_pckt, PAct_CHEAT_AGENT_TELEPORT, agent,
              PRCCOORD_TO_MAPCOORD(things[target].X) + 256, 0,
              PRCCOORD_TO_MAPCOORD(things[target].Z) + 256);
            bot_tp_agent = agent;
            bot_tp_vehicle = target;
            break;
        }
        if (bot_dist(p_agent, things[target].X, things[target].Z) < 512)
            build_packet(p_pckt, PAct_ENTER_VEHICLE, agent, target, 0, 0);
        else
            build_packet(p_pckt, bot_rand(2) ? PAct_GO_ENTER_VEHICLE :
              PAct_GO_ENTER_VEHICLE_FF, agent, target, 0, 0);
        break;
    case 9:
    case 10:
        wtype = bot_pick_weapon(p_agent);
        if (wtype == WEP_NULL)
            break;
        if (bot_rand(4) == 0)
            build_packet(p_pckt, PAct_SELECT_GRP_SPEC_WEAPON, agent, wtype,
              WepSel_SELECT, 0);
        else
            // WepSel_SELECT is above WepSel_HIDE, which the handler
            // rejects, so this does nothing; kept, since agents that really
            // switch weapons carry them into vehicles, and shots from there
            // assert in thing_fire_shot_start_position()
            build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, agent, wtype,
              WepSel_SELECT, 0);
        break;
    case 11:
        build_packet(p_pckt, bot_rand(2) ? PAct_SELECT_NEXT_WEAPON :
          PAct_SELECT_PREV_WEAPON, agent, 0, 0, 0);
        break;
    case 12:
    case 13:
    case 14:
        target = bot_pick_thing(p_agent, bot_rand(4) ? TT_PERSON : TT_VEHICLE, true);
        if (target == 0)
            break;
        build_packet(p_pckt, PAct_SHOOT_AT_THING, agent,
          target, 0, 0);
        break;
    case 15:
        wtype = p_agent->U.UPerson.CurrentWeapon;
        // Half the time aim at a face, as when the mouse is over a roof
        if (bot_rand(2) && (face = bot_pick_face(p_agent, &x, &y, &z)) != 0)
        {
            if (weapon_is_for_planting(wtype))
                build_packet(p_pckt, bot_rand(2) ? PAct_PLANT_MINE_AT_FACE_PT :
                  PAct_PLANT_MINE_AT_FACE_PT_FF, agent, x, face, z);
            else
                build_packet(p_pckt, bot_rand(2) ? PAct_SHOOT_AT_FACE_POINT :
                  PAct_SHOOT_AT_FACE_POINT_FF, agent, x, face, z);
            break;
        }
        bot_near_point(p_agent, &x, &y, &z, 1024);
        if (weapon_is_for_planting(wtype))
            build_packet(p_pckt, bot_rand(2) ? PAct_PLANT_MINE_AT_GND_PT :
              PAct_PLANT_MINE_AT_GND_PT_FF, agent, x, y, z);
        else
            build_packet(p_pckt, bot_rand(2) ? PAct_SHOOT_AT_GND_POINT :
              PAct_SHOOT_AT_GND_POINT_FF, agent, x, y, z);
        break;
    case 16:
        build_packet(p_pckt, bot_rand(2) ? PAct_PROTECT_INC :
          PAct_PROTECT_TOGGLE, agent, 0, 0, 0);
        break;
    case 17:
        target = bot_pick_thing(p_agent, TT_PERSON, false);
        if (target == 0)
            break;
        build_packet(p_pckt, PAct_FOLLOW_PERSON, agent, target, 0, 0);
        break;
    case 18:
    {
        // Two random draws: keep them in separate statements, as argument
        // evaluation order differs between targets (this is the order of
        // the x86 builds which made the reference traces)
        int mood = (int)bot_rand(201) - 100;
        ushort action = bot_rand(2) ? PAct_AGENT_SET_MOOD : PAct_GROUP_SET_MOOD;
        build_packet(p_pckt, action, agent, mood, 0, 0);
        break;
    }
    case 19:
        build_packet(p_pckt, PAct_PEEPS_SCATTER, agent,
          PRCCOORD_TO_MAPCOORD(p_agent->X), 0, PRCCOORD_TO_MAPCOORD(p_agent->Z));
        break;
    case 20:
        switch (bot_rand(4))
        {
        case 0:
            build_packet(p_pckt, PAct_SHIELD_TOGGLE, agent, 0, 0, 0);
            break;
        case 1:
            build_packet(p_pckt, PAct_AGENT_USE_MEDIKIT, agent, 0, 0, 0);
            break;
        case 2:
            build_packet(p_pckt, PAct_THERMAL_TOGGLE, agent, 0, 0, 0);
            break;
        default:
            build_packet(p_pckt, PAct_CHAT_MESSAGE_KEY, 'a' + bot_rand(26), 0, 0, 0);
            break;
        }
        break;
    case 21:
        wtype = bot_pick_weapon(p_agent);
        if (wtype == WEP_NULL)
            break;
        build_packet(p_pckt, PAct_DROP_HELD_WEAPON_SECR, agent, wtype, 0, 0);
        break;
    case 22:
        // Half the time the next agent, as the agent cycling key picks it
        build_packet(p_pckt, PAct_SELECT_AGENT, agent, bot_rand(2) ?
          get_next_player_agent(local_player_no) : agent, 0, 0);
        break;
    case 24:
    case 25:
        // Persuade a nearby person: first hold the Persuadertron, then
        // target someone with it
        if (person_carries_weapon(p_agent, WEP_PERSUADER2))
            wtype = WEP_PERSUADER2;
        else if (person_carries_weapon(p_agent, WEP_PERSUADRTRN))
            wtype = WEP_PERSUADRTRN;
        else
            break;
        if (p_agent->U.UPerson.CurrentWeapon != wtype) {
            // WepSel_SELECT is above WepSel_HIDE, which the handler
            // rejects, so this does nothing; kept, since agents that really
            // switch weapons carry them into vehicles, and shots from there
            // assert in thing_fire_shot_start_position()
            build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, agent, wtype,
              WepSel_SELECT, 0);
            break;
        }
        target = bot_pick_thing(p_agent, TT_PERSON, true);
        if (target == 0)
            break;
        build_packet(p_pckt, PAct_SHOOT_AT_THING, agent, target, 0, 0);
        break;
    default:
        // Rearm and heal all agents now and then; the drop action and
        // fights would leave them with nothing to do
        if (!bot_cheat_allowed())
            break;
        build_packet(p_pckt, PAct_CHEAT_ALL_AGENTS, bot_rand(2) ?
          PCheatAA_RESURRECT_AND_WEPAPN : PCheatAA_BEEFUP_AND_MODS, 0, 0, 0);
        break;
    }
}

static void bot_decide(struct Packet *p_pckt)
{
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    int i, n;

    p_locplayer = &players[local_player_no];
    n = 0;
    for (i = 0; i < playable_agents; i++)
    {
        if (bot_agent_usable(p_locplayer->MyAgent[i]))
            n++;
    }
    if (n == 0) {
        if (bot_cheat_allowed())
            build_packet(p_pckt, PAct_CHEAT_ALL_AGENTS,
              PCheatAA_RESURRECT_AND_WEPAPN, 0, 0, 0);
        return;
    }
    // Save and restore agent groups (moods, weapons, protect targets) on
    // fixed turns, so the random sequence of the other actions stays. The
    // restore commands every agent, so not while any is scripted.
    if (bot_cheat_allowed()) {
        if ((gameturn % 300) == 150) {
            build_packet(p_pckt, PAct_AGENT_UNKGROUP_ADD, (gameturn / 300) % 5, 0, 0, 0);
            return;
        }
        if ((gameturn % 300) == 294) {
            build_packet(p_pckt, PAct_AGENT_UNKGROUP_PROT, (gameturn / 300) % 5, 0, 0, 0);
            return;
        }
    }
    n = bot_rand(n);
    for (i = 0; i < playable_agents; i++)
    {
        p_agent = p_locplayer->MyAgent[i];
        if (!bot_agent_usable(p_agent))
            continue;
        if (n-- == 0)
            break;
    }
    // Self destruct rarely and late, it ends the agent's part (until the
    // next resurrect cheat)
    if ((gameturn > 600) && (bot_rand(60) == 0)) {
        build_packet(p_pckt, PAct_AGENT_SELF_DESTRUCT, p_agent->ThingOffset, 0, 0, 0);
        return;
    }
    bot_build(p_agent, p_pckt);
}

// Triangulation builder steps of pathtrig.c, not declared in a header
void generate_thin_walls(void);
void generate_thin_paths_entrance(void);
void generate_thin_paths(void);

/** Rebuild the selected triangulation as generate_map_triangulation() does,
 * minus the collision steps: those add to the collision columns and vects
 * loaded from the level file and overflow them.
 */
static void retri_rebuild(int tgnNo)
{
    triangulation_select(tgnNo);
    triangulation_clear();
    generate_ground_map();
    generate_thin_walls();
    generate_thin_paths_entrance();
    if (tgnNo == 1)
        triangulation_clear_enter_into_solid_gnd();
    else
        triangulation_clear_enter_into_solid_air();
    delaunay_step();
    generate_thin_paths();
}

/** With env SWARS_DIFFTEST_RETRI set, rebuild the map triangulations once
 * at the start of the mission, as the original game did on map load. The
 * port loads them from the level files, so the triangulation builder
 * (pathtrig asm) never runs otherwise.
 */
static void difftest_retri_turn(void)
{
    static TbBool retri_tried = false;

    if (retri_tried)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    retri_tried = true;
    if (getenv("SWARS_DIFFTEST_RETRI") == NULL)
        return;
    // New storage as prepare_map_dat_to_play() does: the arrays loaded from
    // the level file are too small to rebuild in. With no slot selected,
    // the working copy is not written back over the new arrays.
    selected_triangulation_no = -1;
    triangulation_allocate(1, 9000);
    triangulation_allocate(2, 9000);
    retri_rebuild(2);
    retri_rebuild(1);
}

/** With env SWARS_DIFFTEST_KBD set, the local player uses keyboard control
 * from the first engine turn. Target finding for directly controlled agents
 * (find_a_target) and the camera tracking (track_player) only run when the
 * control mode is not mouse.
 */
static void difftest_kbd_turn(void)
{
    static TbBool kbd_tried = false;

    if (kbd_tried)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    kbd_tried = true;
    if (getenv("SWARS_DIFFTEST_KBD") == NULL)
        return;
    user_input_control_mode_set(local_player_no, 0, UInpCtr_Keyboard);
}

/** With env SWARS_DIFFTEST_CRATER set, a crater is started every 64 turns
 * near a player agent. The port's explosions stamp craters at once
 * (quick_crater()), so the animated craters (process_map_crater) never
 * run otherwise.
 */
static void difftest_crater_turn(void)
{
    static int crater_on = -1;
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    int n;

    if (crater_on < 0)
        crater_on = (getenv("SWARS_DIFFTEST_CRATER") != NULL);
    if (!crater_on)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if ((gameturn % 64) != 32)
        return;
    n = gameturn / 64;
    p_locplayer = &players[local_player_no];
    p_agent = p_locplayer->MyAgent[n % playable_agents];
    if (p_agent == NULL || p_agent->Type != TT_PERSON)
        return;
    create_crater(MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(p_agent->X)) + (n % 5) - 2,
      MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(p_agent->Z)) + (n % 3) - 1, 1 + (n % 4));
}

static TbBool wreck_target_ok(struct Thing *p_thing)
{
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return false;
    if (p_thing->Type == TT_VEHICLE)
    {
        ThingIdx passngr;
        int i;

        // Not with player agents inside: once their vehicle is wrecked,
        // a dying agent's ComCur can be no player slot (set_person_dead)
        passngr = p_thing->U.UVehicle.PassengerHead;
        for (i = 0; (passngr > 0) && (i < 32); i++)
        {
            if ((things[passngr].Flag & TngF_PlayerAgent) != 0)
                return false;
            passngr = things[passngr].U.UPerson.LinkPassenger;
        }
        return true;
    }
    return (p_thing->Type == TT_BUILDING) && (p_thing->SubType == SubTT_BLD_MGUN);
}

/** With env SWARS_DIFFTEST_WRECK set, every 32 turns a vehicle or mounted
 * gun takes a heavy hit (as from a rocket of the first player agent), the
 * same one until it is destroyed. The bot's weapons rarely destroy mechs
 * and mounted guns, so their explosions (init_mech_explode,
 * init_mgun_explode) never run otherwise.
 */
static void difftest_wreck_turn(void)
{
    static int wreck_on = -1;
    static ThingIdx wreck_thing = 0;
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    ThingIdx thing;

    if (wreck_on < 0)
        wreck_on = (getenv("SWARS_DIFFTEST_WRECK") != NULL);
    if (!wreck_on)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if ((gameturn % 32) != 16)
        return;
    if ((wreck_thing == 0) || !wreck_target_ok(&things[wreck_thing]))
    {
        // Next target after the previous one, mechs and guns first
        ThingIdx start = wreck_thing;
        wreck_thing = 0;
        for (thing = 1; thing < THINGS_LIMIT; thing++)
        {
            ThingIdx t = (start + thing) % THINGS_LIMIT;
            struct Thing *p_thing = &things[t];
            if (t == 0 || !wreck_target_ok(p_thing))
                continue;
            if ((p_thing->Type == TT_BUILDING) || (p_thing->SubType == SubTT_VEH_MECH)) {
                wreck_thing = t;
                break;
            }
            if (wreck_thing == 0)
                wreck_thing = t;
        }
        if (wreck_thing == 0)
            return;
    }
    p_locplayer = &players[local_player_no];
    p_agent = p_locplayer->MyAgent[0];
    // Mechs take an eighth of it
    thing_hit_by_bullet(&things[wreck_thing],
      (things[wreck_thing].SubType == SubTT_VEH_MECH) ? 30000 : 2000,
      0, 0, 0, p_agent, DMG_RAP);
}

TbBool difftest_suffix_has(const char *name)
{
    static const char *suffix = NULL;
    static TbBool tried = false;
    const char *s;
    size_t len;

    if (!tried) {
        suffix = getenv("SWARS_DIFFTEST_SUFFIX");
        tried = true;
    }
    if (suffix == NULL)
        return false;
    len = strlen(name);
    for (s = strchr(suffix, '+'); s != NULL; s = strchr(s + 1, '+'))
    {
        if ((strncmp(s + 1, name, len) == 0) &&
          ((s[1 + len] == '+') || (s[1 + len] == '\0')))
            return true;
    }
    return false;
}

void difftest_bot_turn(void)
{
    struct Packet *p_pckt;

    difftest_retri_turn();
    difftest_kbd_turn();
    difftest_crater_turn();
    difftest_wreck_turn();
    difftest_hook_misc_turn();
    difftest_hook_people_turn();
    difftest_hook_transport_turn();
    if (!bot_tried) {
        const char *s = getenv("SWARS_DIFFTEST_BOT");
        bot_tried = true;
        if (s != NULL) {
            bot_on = true;
            bot_seed = strtoul(s, NULL, 0) | 1;
        }
        s = getenv("SWARS_DIFFTEST_BOT_LOG");
        if (bot_on && (s != NULL))
            bot_log = fopen(s, "w");
    }
    if (!bot_on)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    p_pckt = &packets[local_player_no];
    if ((p_pckt->Action & ~PActF_All) != PAct_NONE)
        return;
    if ((gameturn % BOT_TURN_INTERVAL) != 0)
        return;

    bot_decide(p_pckt);
    if ((bot_log != NULL) && ((p_pckt->Action & ~PActF_All) != PAct_NONE)) {
        fprintf(bot_log, "turn %u: %s (0x%02x) data=%d x=%d y=%d z=%d\n",
          (u32)gameturn, get_packet_action_name(p_pckt->Action & ~PActF_All),
          (uint)(p_pckt->Action & ~PActF_All), (int)p_pckt->Data, (int)p_pckt->X, (int)p_pckt->Y, (int)p_pckt->Z);
        fflush(bot_log);
    }
}

#else

void difftest_bot_turn(void)
{
}

TbBool difftest_suffix_has(const char *name)
{
    return false;
}

#endif
/******************************************************************************/
