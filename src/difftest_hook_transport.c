/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_hook_transport.c
 *     Difftest hooks reaching vehicle, train and ship code.
 * @par Purpose:
 *     Calls functions no mission reaches by itself, so the difftest can
 *     compare their C remake against the asm. Active only in scenarios
 *     whose suffix names the hook (see difftest_suffix_has()).
 * @par Comment:
 *     Called every game turn from difftest_bot_turn().
 * @author   SyndicateVR decomp agent
 * @date     16 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#include <string.h>
#include "building.h"
#include "command.h"
#include "engincam.h"
#include "enginzoom.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "people.h"
#include "thing.h"
#include "vehicle.h"
#include "vehtraffic.h"

/******************************************************************************/

#if DIFFTEST_HARNESS

/* Not declared in the headers. */
short find_station_platform(short x, short z);

#define HOOK_STATIONS_MAX 8

/** True if the vehicle has a player agent aboard; such vehicles are left
 * to the player (or the bot).
 */
static TbBool vehicle_has_player_agent(struct Thing *p_vehicle)
{
    ThingIdx passngr;
    int i;

    passngr = p_vehicle->U.UVehicle.PassengerHead;
    for (i = 0; (passngr > 0) && (i < 32); i++)
    {
        if ((things[passngr].Flag & TngF_PlayerAgent) != 0)
            return true;
        passngr = things[passngr].U.UPerson.LinkPassenger;
    }
    return false;
}

/** Map coordinate a little away from the given one, in a direction which
 * changes every few turns; kept inside the map.
 */
static void hook_target_near(short *p_x, short *p_z, s32 x, s32 z, ulong step)
{
    switch (step & 3)
    {
    case 0: x += 0x600; break;
    case 1: z += 0x600; break;
    case 2: x -= 0x600; break;
    default: z -= 0x600; break;
    }
    if (x < 0x400) x = 0x400;
    if (x > 0x7c00) x = 0x7c00;
    if (z < 0x400) z = 0x400;
    if (z > 0x7c00) z = 0x7c00;
    *p_x = x;
    *p_z = z;
}

/** Suffix t_train: wandering civilians get a CATCH_TRAIN command for a
 * station platform, as level scripts give it (no mission does). They walk
 * there (person_catch_train), the train takes them aboard
 * (person_clear_route_and_teleport_to_vehicle) and lets them out at the
 * next station (set_xz_for_unkn3).
 */
static void hook_train_turn(void)
{
    static ushort station_cmd[HOOK_STATIONS_MAX];
    static ThingIdx station_thing[HOOK_STATIONS_MAX];
    static int stations_count = -1;
    static int sent = 0;
    struct Thing *p_station;
    struct Thing *p_best;
    ThingIdx thing;
    s32 best_dist;
    int n;

    if (stations_count < 0)
    {
        stations_count = 0;
        for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
        {
            struct Thing *p_thing = &things[thing];
            short face;
            ushort cmd;

            if ((p_thing->Type != TT_BUILDING) || (p_thing->SubType != SubTT_BLD_STATION))
                continue;
            if (stations_count >= HOOK_STATIONS_MAX)
                break;
            face = find_station_platform(p_thing->X >> 8, p_thing->Z >> 8);
            if (face == 0)
                continue;
            cmd = get_new_command();
            game_commands[cmd].Type = PCmd_CATCH_TRAIN;
            game_commands[cmd].OtherThing = face;
            game_commands[cmd].X = p_thing->X >> 8;
            game_commands[cmd].Z = p_thing->Z >> 8;
            game_commands[cmd].Next = 0;
            station_cmd[stations_count] = cmd;
            station_thing[stations_count] = thing;
            stations_count++;
        }
    }
    if (stations_count == 0)
        return;
    if ((gameturn % 24) != 12)
        return;
    if (sent >= 40)
        return;

    // Nearest wandering civilian to the station, stations taken in turn
    n = (gameturn / 24) % stations_count;
    p_station = &things[station_thing[n]];
    if ((p_station->Flag & TngF_Destroyed) != 0)
        return;
    p_best = NULL;
    best_dist = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_person = &things[thing];
        s32 dx, dz, dist;

        if (p_person->Type != TT_PERSON)
            continue;
        if ((p_person->Flag & (TngF_Destroyed | TngF_PlayerAgent |
          TngF_InVehicle | TngF_Persuaded)) != 0)
            continue;
        if (p_person->State != PerSt_WANDER)
            continue;
        if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn80) != 0)
            continue;
        dx = (p_person->X - p_station->X) >> 8;
        dz = (p_person->Z - p_station->Z) >> 8;
        dist = dx * dx + dz * dz;
        if ((p_best == NULL) || (dist < best_dist)) {
            p_best = p_person;
            best_dist = dist;
        }
    }
    if (p_best == NULL)
        return;
    p_best->U.UPerson.ComHead = station_cmd[n];
    p_best->U.UPerson.ComCur = station_cmd[n];
    person_start_executing_commands(p_best);
    // Next process_person goes to person_find_next_state(), which inits the command
    p_best->State = PerSt_NONE;
    sent++;
}

/** Suffix t_ship: every 60 turns each ship is sent to a node of the ship
 * traffic network, as start_goto_vehicle() does when a player drives it
 * (send_ship_to).
 */
static void hook_ship_turn(void)
{
    ThingIdx thing;
    int nodes_count, i, k;

    if ((gameturn % 60) != 30)
        return;
    nodes_count = 0;
    for (i = 1; i < next_traffic_node; i++)
    {
        if ((game_traffic_nodes[i].Flags & 0x0200) != 0)
            nodes_count++;
    }
    if (nodes_count == 0)
        return;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_ship = &things[thing];

        if ((p_ship->Type != TT_VEHICLE) || (p_ship->SubType != SubTT_VEH_SHIP))
            continue;
        if ((p_ship->Flag & TngF_Destroyed) != 0)
            continue;
        if (vehicle_has_player_agent(p_ship))
            continue;
        k = (gameturn / 60 + thing) % nodes_count;
        for (i = 1; i < next_traffic_node; i++)
        {
            if ((game_traffic_nodes[i].Flags & 0x0200) == 0)
                continue;
            if (k-- == 0)
                break;
        }
        start_goto_vehicle(p_ship, game_traffic_nodes[i].X, game_traffic_nodes[i].Z);
    }
}

/** Suffix t_fly: parked flying vehicles take off again. Even ones use the
 * goto point state (0x27: liftoff, fly, land; nothing in the game sets it),
 * odd ones the normal flight of start_goto_vehicle() and, once hovering at
 * the target, the flying wander (0x43, continue_flying_wander).
 */
static void hook_fly_turn(void)
{
    ThingIdx thing;

    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_vehicle = &things[thing];
        short x, z;
        s32 cx, cz;

        if ((p_vehicle->Type != TT_VEHICLE) || (p_vehicle->SubType != SubTT_VEH_FLYING))
            continue;
        if ((p_vehicle->Flag & TngF_Destroyed) != 0)
            continue;
        if (vehicle_has_player_agent(p_vehicle))
            continue;
        cx = p_vehicle->X >> 8;
        cz = p_vehicle->Z >> 8;
        switch (p_vehicle->State)
        {
        case VehSt_NONE:
        case VehSt_PARKED_PARAL:
        case VehSt_PARKED_PERPN:
            if ((int)(gameturn % 40) != (thing % 40))
                break;
            hook_target_near(&x, &z, cx, cz, gameturn / 40 + thing);
            if ((thing & 1) == 0)
            {
                p_vehicle->U.UVehicle.GotoX = x;
                p_vehicle->U.UVehicle.GotoZ = z;
                p_vehicle->State = VehSt_UNKN_27;
                p_vehicle->SubState = 5;
            }
            else
            {
                start_goto_vehicle(p_vehicle, x, z);
            }
            break;
        case VehSt_UNKN_41:
            // Hovering at the target: wander on from here
            p_vehicle->U.UVehicle.GotoX = cx;
            p_vehicle->U.UVehicle.GotoZ = cz;
            p_vehicle->State = VehSt_UNKN_43;
            break;
        case VehSt_UNKN_43:
            // Heading for the map edge: wander on from here instead
            if ((p_vehicle->U.UVehicle.GotoX == 1) || (p_vehicle->U.UVehicle.GotoX == 0x7e00) ||
              (p_vehicle->U.UVehicle.GotoZ == 1) || (p_vehicle->U.UVehicle.GotoZ == 0x7e00))
            {
                if ((int)(gameturn % 40) != (thing % 40))
                    break;
                p_vehicle->U.UVehicle.GotoX = cx;
                p_vehicle->U.UVehicle.GotoZ = cz;
            }
            break;
        default:
            break;
        }
    }
}

/** Suffix t_traffic: the camera flyby along the traffic nodes of the credits
 * screen (traffic_unkn_func_01) runs every turn, with the game camera kept;
 * and every 40 turns a traffic node is cloned into a traffic simple thing,
 * as levels of old formats store them (new_thing_traffic_clone), then
 * removed again.
 */
static void hook_traffic_turn(void)
{
    static int cloned = 0;
    s32 xc, yc, zc, yaw, tilt;
    ushort area_a, area_b;

    xc = engn_xc;
    yc = engn_yc;
    zc = engn_zc;
    yaw = engn_cam_yaw;
    tilt = engn_cam_tilt;
    area_a = render_area_a;
    area_b = render_area_b;
    traffic_unkn_func_01();
    engn_xc = xc;
    engn_yc = yc;
    engn_zc = zc;
    engn_cam_yaw = yaw;
    engn_cam_tilt = tilt;
    render_area_a = area_a;
    render_area_b = area_b;

    if (((gameturn % 40) == 20) && (cloned < 30) && (next_traffic_node > 1))
    {
        struct SimpleThing loc_sthing;
        struct TrafficNode *p_tnode;
        ThingIdx sthing;

        p_tnode = &game_traffic_nodes[1 + (gameturn / 40) % (next_traffic_node - 1)];
        memset(&loc_sthing, 0, sizeof(loc_sthing));
        loc_sthing.Type = SmTT_TRAFFIC;
        loc_sthing.ThingOffset = sthings_empty_head;
        loc_sthing.X = p_tnode->X << 8;
        loc_sthing.Y = p_tnode->Y << 8;
        loc_sthing.Z = p_tnode->Z << 8;
        loc_sthing.U.UTraffic = p_tnode->UTraffic;
        if ((loc_sthing.X < 0) || (loc_sthing.X >= 0x800000) ||
          (loc_sthing.Z < 0) || (loc_sthing.Z >= 0x800000))
            return;
        new_thing_traffic_clone(&loc_sthing);
        sthing = sthings_used_head;
        if ((sthing < 0) && (sthings[sthing].Type == SmTT_TRAFFIC))
        {
            delete_snode(&sthings[sthing]);
            remove_sthing(sthing);
        }
        cloned++;
    }
}

void difftest_hook_transport_turn(void)
{
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if (difftest_suffix_has("t_train"))
        hook_train_turn();
    if (difftest_suffix_has("t_ship"))
        hook_ship_turn();
    if (difftest_suffix_has("t_fly"))
        hook_fly_turn();
    if (difftest_suffix_has("t_traffic"))
        hook_traffic_turn();
}

#else

void difftest_hook_transport_turn(void)
{
}

#endif
/******************************************************************************/
