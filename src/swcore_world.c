/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swcore_world.c
 *     World state and player commands of the game library.
 * @par Purpose:
 *     The part of swcore.h that lets a front end read the world (things,
 *     agents, mission) and give the player's commands, without knowing the
 *     game's internal structures.
 * @par Comment:
 *     Commands are executed as the packets a mouse click builds; taken from
 *     the VR viewer, which runs the core in its process.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swcore.h"
#include "swcore_game.h"

#include <string.h>

#include "engincam.h"

#include "bigmap.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvobjctv.h"
#include "packet.h"
#include "people.h"
#include "player.h"
#include "thing.h"
#include "weapon.h"
/******************************************************************************/

static struct SwcCommand swc_cmds[SWCORE_COMMANDS_MAX];
static u32 swc_cmd_read, swc_cmd_write;

static TbBool swc_in_mission(void)
{
    return (ingame.DisplayMode == DpM_ENGINEPLY) && (game_my_big_map != NULL);
}

void swcore_world_info(struct SwcWorldInfo *p_info)
{
    PlayerInfo *p_locplayer;

    memset(p_info, 0, sizeof(*p_info));
    p_info->gameturn = (uint32_t)gameturn;
    p_info->in_mission = swc_in_mission();
    if (!p_info->in_mission)
        p_info->mission_status = SwcMS_NONE;
    else if (ingame.MissionStatus == ObvStatu_COMPLETED)
        p_info->mission_status = SwcMS_WON;
    else if (ingame.MissionStatus != ObvStatu_UNDECIDED)
        p_info->mission_status = SwcMS_LOST;
    else
        p_info->mission_status = SwcMS_PLAYING;
    p_info->map_no = current_map;
    p_info->level_no = current_level;
    p_locplayer = &players[local_player_no];
    p_info->selected = p_locplayer->DirectControl[0];
    // the camera is kept in map coordinates
    p_info->cam_x = MAPCOORD_TO_PRCCOORD(engn_xc, 0);
    p_info->cam_z = MAPCOORD_TO_PRCCOORD(engn_zc, 0);
    // bflib angle units times 32
    p_info->cam_angle = (engn_cam_yaw / 32) & 0x7FF;
    p_info->objective = scrollinfo_text;
}

static TbBool swc_is_player_agent(const struct Thing *p_thing)
{
    PlayerInfo *p_locplayer;
    int i;

    p_locplayer = &players[local_player_no];
    for (i = 0; i < AGENTS_SQUAD_MAX_COUNT; i++) {
        if (p_locplayer->MyAgent[i] == p_thing)
            return true;
    }
    return false;
}

static ubyte swc_thing_kind(const struct Thing *p_thing)
{
    switch (p_thing->Type)
    {
    case TT_PERSON:
        return SwcTT_PERSON;
    case TT_VEHICLE:
        return SwcTT_VEHICLE;
    case TT_BUILDING:
        return SwcTT_BUILDING;
    case TT_ROCKET:
    case TT_LASER11:
    case TT_LASER_GUIDED:
    case TT_MINE:
    case TT_GRENADE:
    case TT_LASER_ELEC:
    case TT_LASER29:
    case TT_AIR_STRIKE:
    case TT_LASER38:
        return SwcTT_EFFECT;
    default:
        return SwcTT_OTHER;
    }
}

uint32_t swcore_things(struct SwcThing *p_things, uint32_t max)
{
    ThingIdx thing;
    ThingIdx selected;
    uint32_t n;

    n = 0;
    selected = players[local_player_no].DirectControl[0];
    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing = &things[thing];
        struct SwcThing *p_out;

        if (p_thing->Type == TT_NONE)
            continue;
        n++;
        if (n > max)
            continue;
        p_out = &p_things[n - 1];
        memset(p_out, 0, sizeof(*p_out));
        p_out->index = thing;
        p_out->kind = swc_thing_kind(p_thing);
        p_out->game_type = p_thing->Type;
        p_out->game_subtype = p_thing->SubType;
        p_out->x = p_thing->X;
        p_out->y = p_thing->Y;
        p_out->z = p_thing->Z;
        p_out->health = p_thing->Health;
        p_out->state = p_thing->State;
        if (p_thing->Type == TT_PERSON)
        {
            p_out->group = p_thing->U.UPerson.Group;
            // people face one of 8 directions
            p_out->angle = (p_thing->U.UPerson.Angle & 7) * 256;
            p_out->max_health = p_thing->U.UPerson.MaxHealth;
            if (p_thing->State == PerSt_DEAD)
                p_out->flags |= SwcTF_DEAD;
            if (swc_is_player_agent(p_thing))
                p_out->flags |= SwcTF_PLAYER_AGENT;
        }
        else if (p_thing->Type == TT_VEHICLE)
        {
            p_out->group = p_thing->U.UVehicle.Group;
            p_out->angle = p_thing->U.UVehicle.AngleY & 0x7FF;
            p_out->max_health = p_thing->U.UVehicle.MaxHealth;
        }
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            p_out->flags |= SwcTF_DEAD;
        if (thing == selected)
            p_out->flags |= SwcTF_SELECTED;
    }
    return n;
}

void swcore_agents(struct SwcAgent *p_agents)
{
    PlayerInfo *p_locplayer;
    int i;

    p_locplayer = &players[local_player_no];
    for (i = 0; i < SWCORE_AGENTS_MAX; i++)
    {
        struct SwcAgent *p_sa = &p_agents[i];
        struct Thing *p_agent;

        p_agent = (i < AGENTS_SQUAD_MAX_COUNT) ? p_locplayer->MyAgent[i] : NULL;
        memset(p_sa, 0, sizeof(*p_sa));
        if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
            continue;
        p_sa->thing = p_agent->ThingOffset;
        p_sa->current_weapon = p_agent->U.UPerson.CurrentWeapon;
        p_sa->weapons = p_agent->U.UPerson.WeaponsCarried;
        p_sa->health = p_agent->Health;
        p_sa->max_health = p_agent->U.UPerson.MaxHealth;
        if (p_agent->ThingOffset == (ThingIdx)p_locplayer->DirectControl[0])
            p_sa->flags |= SwcTF_SELECTED;
        if (((p_agent->Flag & TngF_Destroyed) != 0) || (p_agent->State == PerSt_DEAD))
            p_sa->flags |= SwcTF_DEAD;
    }
}

const char *swcore_weapon_name(uint32_t weapon_type)
{
    const char *name;

    if ((weapon_type == 0) || (weapon_type >= WEP_TYPES_COUNT))
        return "";
    name = weapon_codename(weapon_type);
    return (name != NULL) ? name : "";
}

int swcore_command(const struct SwcCommand *p_cmd)
{
    if (swc_cmd_write - swc_cmd_read >= SWCORE_COMMANDS_MAX)
        return 1;
    swc_cmds[swc_cmd_write % SWCORE_COMMANDS_MAX] = *p_cmd;
    swc_cmd_write++;
    return 0;
}

static TbBool swc_map_point_valid(s32 x, s32 z)
{
    return (x > 0) && (x < MAP_COORD_WIDTH) && (z > 0) && (z < MAP_COORD_HEIGHT);
}

TbBool swcore_input_packet(struct Packet *p_pckt)
{
    struct SwcCommand cmd;
    PlayerInfo *p_locplayer;
    ThingIdx dcthing;
    int map_y;

    if (swc_cmd_read == swc_cmd_write)
        return false;
    cmd = swc_cmds[swc_cmd_read % SWCORE_COMMANDS_MAX];
    swc_cmd_read++;

    p_locplayer = &players[local_player_no];
    dcthing = p_locplayer->DirectControl[0];
    if ((dcthing <= 0) || (things[dcthing].Type != TT_PERSON))
        return false;
    p_pckt->Action = PAct_NONE;
    switch (cmd.type)
    {
    case SwcCmd_SELECT_AGENT:
    {
        int i;
        for (i = 0; i < AGENTS_SQUAD_MAX_COUNT; i++) {
            struct Thing *p_agent = p_locplayer->MyAgent[i];
            if ((p_agent != NULL) && (p_agent->ThingOffset == cmd.thing)
              && ((p_agent->Flag & TngF_Destroyed) == 0))
                my_build_packet(p_pckt, PAct_SELECT_AGENT, dcthing, cmd.thing, 0, 0);
        }
        break;
    }
    case SwcCmd_GOTO:
        if (!swc_map_point_valid(cmd.x, cmd.z))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS, dcthing, cmd.x, map_y, cmd.z);
        break;
    case SwcCmd_SHOOT_THING:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type != TT_NONE))
            my_build_packet(p_pckt, PAct_SHOOT_AT_THING, dcthing, cmd.thing, 0, 0);
        break;
    case SwcCmd_SHOOT_GROUND:
        if (!swc_map_point_valid(cmd.x, cmd.z))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT, dcthing, cmd.x, map_y, cmd.z);
        break;
    case SwcCmd_WEAPON_NEXT:
        my_build_packet(p_pckt, PAct_SELECT_NEXT_WEAPON, dcthing, 0, 0, 0);
        break;
    case SwcCmd_WEAPON_PREV:
        my_build_packet(p_pckt, PAct_SELECT_PREV_WEAPON, dcthing, 0, 0, 0);
        break;
    case SwcCmd_WEAPON_SELECT:
        if ((cmd.arg > 0) && (cmd.arg < WEP_TYPES_COUNT))
            my_build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, dcthing, cmd.arg, WepSel_TOGGLE, 0);
        break;
    case SwcCmd_ENTER_VEHICLE:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type == TT_VEHICLE))
            my_build_packet(p_pckt, PAct_GO_ENTER_VEHICLE, dcthing, cmd.thing, 0, 0);
        break;
    default:
        break;
    }
    return (p_pckt->Action != PAct_NONE);
}
/******************************************************************************/
