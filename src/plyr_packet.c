/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file plyr_packet.c
 *     Players User Input handling.
 * @par Purpose:
 *     Implement functions for storing user input and building packets
 *     which will later influence the game world.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     11 Dec 2024 - 01 Feb 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "plyr_packet.h"

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "bfkeybd.h"
#include "bfscd.h"
#include "bfmemut.h"
#include "bfmusic.h"
#include "bfutility.h"
#include "enginsngobjs.h"
#include "engintrns.h"
#include "frame_sprani.h"
#include "ssampply.h"

#include "bigmap.h"
#include "building.h"
#include "difftest.h"
#include "display.h"
#include "drawtext_wrp.h"
#include "game_bstype.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "guitext.h"
#include "hud_panel.h"
#include "hud_target.h"
#include "keyboard.h"
#include "network.h"
#include "packet.h"
#include "people.h"
#include "player.h"
#include "plyr_net.h"
#include "research.h"
#include "scandraw.h"
#include "sound.h"
#include "thing.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/
struct ShortPacket shpackets[8];

/******************************************************************************/

void net_player_leave(PlayerIdx plyr)
{
    kill_my_players(plyr);
    if ((plyr == net_host_player_no) || (plyr == local_player_no) || (nsvc.I.Type != NetSvc_IPX))
    {
        ingame.DisplayMode = DpM_PURPLEMNU;
        StopCD();
        StopAllSamples();
        SetMusicVolume(100, 0);
        LbNetworkSessionStop(local_player_no);
        if (nsvc.I.Type != NetSvc_IPX && net_serial_uses_modem)
            LbNetworkHangUp();
    }
    else
    {
        net_players_num--;
        player_message_fmt(plyr, "%s %s", net_player_names[plyr], gui_strings[GSTR_NET_LEFT_GAME]);
        LbNetworkSessionStop(plyr);
        ingame.InNetGame_UNSURE &= ~(1 << plyr);
    }
}

void player_agent_select(PlayerIdx plyr, ThingIdx person)
{
    if (person == (ThingIdx)players[plyr].DirectControl[mouser])
        return;
    if (plyr == local_player_no)
    {
        struct Thing *p_person;
        ushort smp;

        p_person = &things[person];
        if (p_person->SubType == SubTT_PERS_AGENT)
            smp = 44; // 'selected' speech
        else
            smp = 46;
        play_disk_sample(0, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
    if (person != (ThingIdx)players[plyr].DirectControl[0])
    {
        player_change_person(person, plyr);
    }
}

/** Alter relative position, limiting its vector length to 256.
 *
 * Relative position change needs to be capped, as packet creation function does
 * not care too much about exceeding the limit. Normalizing here, on packet receive
 * side, also makes sure cheating is not possible.
 */
void packet_rel_position_3d_normalize(short *p_x, short *p_y, short *p_z)
{
    int len;
    len = map_distance_deltas_fast(*p_x, *p_y, *p_z);
    if (len < 256) { // do not increase vector length if below max
        len = 256;
    }
    *p_x = (*p_x) * 256 / len;
    *p_y = (*p_y) * 256 / len;
    *p_z = (*p_z) * 256 / len;
}

static DIFFTEST_REF_FUNC void thing_goto_point_rel_asm(struct Thing *p_thing,
  short x, short y, short z)
{
    DIFFTEST_ASM(
      "call ASM_thing_goto_point_rel\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z));
}

void thing_goto_point_rel(struct Thing *p_thing, short x, short y, short z)
{
    ushort angle;
    ushort frame;

    // Make sure the normalization is kept when this function is remade
    packet_rel_position_3d_normalize(&x, &y, &z);
    DIFFTEST_SHADOW(thing_goto_point_rel, thing_goto_point_rel_asm(p_thing, x, y, z));

    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00020000 | TgF2_Unkn0040);
    if ((p_thing->Flag2 & TgF2_KnockedOut) != 0)
        return;
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;

    if ((p_thing->Flag & TngF_StationrSht) != 0)
    {
        angle = (((s32)angle_between_points(0, 0, x, z) + 0x80) >> 8) + 8;
        angle &= 0x7;
        if (angle == p_thing->U.UPerson.Angle)
            return;
        p_thing->U.UPerson.Angle = angle;
        frame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode];
        p_thing->Frame = nstart_ani[frame + (ubyte)angle];
        return;
    }

    if (((p_thing->Flag2 & TgF2_Unkn0001) != 0) && ((p_thing->Flag & TngF_TriggerUse) == 0))
        finalise_razor_wire(p_thing);

    if ((p_thing->Flag & TngF_InVehicle) != 0)
        return;

    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = 0;

    if (((p_thing->State & 0x7fff) != 1) && ((p_thing->State & 0x7fff) != 0x23)
      && ((ushort)p_thing->State != 5))
        return;

    p_thing->StartTimer1 = 0x30;
    p_thing->Flag2 &= ~TgF2_Unkn00020000;
    p_thing->State = (p_thing->State & 0x8000) | 0x23;
    p_thing->VX = x;
    p_thing->VZ = z;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);

    if ((p_thing->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if (p_thing->U.UPerson.AnimMode == 0x13)
            return;
        p_thing->U.UPerson.AnimMode = 0x13;
    }
    else
    {
        if (p_thing->U.UPerson.AnimMode != 0x13)
            return;
        p_thing->U.UPerson.AnimMode = gun_out_anim(p_thing, 0);
    }
    p_thing->StartFrame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode] - 1;
    frame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode];
    p_thing->Frame = nstart_ani[frame + p_thing->U.UPerson.Angle];
}

static DIFFTEST_REF_FUNC void thing_goto_point_rel_fast_asm(struct Thing *p_thing,
  short x, short y, short z, int plyr)
{
    int stkargs[] = {plyr};
    DIFFTEST_ASM(
      "push 0(%4)\n"
      "call ASM_thing_goto_point_rel_fast\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z), "S" (stkargs)
        : "cc", "memory");
}

void thing_goto_point_rel_fast(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    DIFFTEST_SHADOW(thing_goto_point_rel_fast,
      thing_goto_point_rel_fast_asm(p_thing, x, y, z, plyr));
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00080000 | TgF2_Unkn00020000 | TgF2_Unkn0040);
    p_thing->Flag2 |= TgF2_Unkn00080000;
    thing_goto_point_rel(p_thing, x, y, z);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}

void thing_goto_point(struct Thing *p_thing, short x, short y, short z);

static DIFFTEST_REF_FUNC void thing_goto_point_fast_asm(struct Thing *p_thing,
  short x, short y, short z, int plyr)
{
    int stkargs[] = {plyr};
    DIFFTEST_ASM(
      "push 0(%4)\n"
      "call ASM_thing_goto_point_fast\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z), "S" (stkargs)
        : "cc", "memory");
}

void thing_goto_point_fast(struct Thing *p_thing, short x, short y, short z, int plyr)
{
    DIFFTEST_SHADOW(thing_goto_point_fast,
      thing_goto_point_fast_asm(p_thing, x, y, z, plyr));
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00080000 | TgF2_Unkn00020000 | TgF2_Unkn0040);
    p_thing->Flag2 |= TgF2_Unkn00080000;
    thing_goto_point(p_thing, x, y, z);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void clear_player_protectors_paths_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_clear_player_protectors_paths\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Removes the paths of the player's agents which protect p_thing (eax);
 * the asm does not read edx, ebx, ecx.
 */
void clear_player_protectors_paths(struct Thing *p_thing)
{
    ushort plyr;
    ushort i;

    DIFFTEST_SHADOW(clear_player_protectors_paths,
      clear_player_protectors_paths_asm((intptr_t)p_thing, 0, 0, 0));
    plyr = p_thing->U.UPerson.ComCur >> 2;
    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[i];
        if (p_agent == NULL)
            continue;
        if ((ushort)p_agent->State != PerSt_PROTECT_PERSON)
            continue;
        if (p_agent->GotoThingIndex != (ushort)p_thing->ThingOffset)
            continue;
        remove_path(p_agent);
        players[plyr].MyAgent[i]->U.UPerson.ComTimer = -1;
    }
}


static DIFFTEST_REF_FUNC void thing_goto_point_asm(struct Thing *p_thing,
  short x, short y, short z)
{
    DIFFTEST_ASM(
      "call ASM_thing_goto_point\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z));
}

void thing_goto_point(struct Thing *p_thing, short x, short y, short z)
{
    ushort state;
    ushort frame;

    DIFFTEST_SHADOW(thing_goto_point, thing_goto_point_asm(p_thing, x, y, z));

    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_StationrSht | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 &= ~(TgF2_Unkn00020000 | TgF2_Unkn0040);
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;
    if ((ushort)p_thing->State == PerSt_DEAD)
        return;

    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_thing);

    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        if (start_goto_vehicle(&things[p_thing->U.UPerson.Vehicle], x, z) == 0)
        {
            p_thing->Flag2 |= TgF2_Unkn0040;
            return;
        }
        p_thing->U.UPerson.GotoX = x;
        p_thing->U.UPerson.GotoZ = z;
        return;
    }

    if ((p_thing->Flag & TngF_SelectedAgent) != 0)
        clear_player_protectors_paths(p_thing);

    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = 0;

    p_thing->Flag2 &= ~TgF2_Unkn00020000;
    state = p_thing->State & 0x7fff;
    switch (state)
    {
    case PerSt_NONE:
    case PerSt_GOTO_POINT:
    case PerSt_WAIT:
    case PerSt_GOTO_PERSON:
    case PerSt_GET_ITEM:
    case PerSt_USE_VEHICLE:
        break;
    case PerSt_DRIVING_VEHICLE:
        start_goto_vehicle(&things[p_thing->U.UPerson.Vehicle], x, z);
        return;
    default:
        return;
    }

    p_thing->U.UPerson.ComTimer = -1;
    p_thing->Timer1 = 0x30;
    p_thing->StartTimer1 = 0x30;
    p_thing->SubState = 0;
    p_thing->U.UPerson.Target2 = 0;
    p_thing->U.UPerson.GotoX = x;
    p_thing->U.UPerson.GotoZ = z;
    p_thing->State = (p_thing->State & 0x8000) | PerSt_GOTO_POINT;
    p_thing->U.UPerson.ComRange = 1;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);
    p_thing->U.UPerson.PathIndex = 0;
    build_navigate_path(p_thing, x, z, 0);

    if ((p_thing->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if (p_thing->U.UPerson.AnimMode == 0x13)
            return;
        p_thing->U.UPerson.AnimMode = 0x13;
    }
    else
    {
        if (p_thing->U.UPerson.AnimMode != 0x13)
            return;
        p_thing->U.UPerson.AnimMode = gun_out_anim(p_thing, 0);
    }
    p_thing->StartFrame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode] - 1;
    frame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode];
    p_thing->Frame = nstart_ani[frame + p_thing->U.UPerson.Angle];
}

void thing_goto_point_on_face(struct Thing *p_thing, short x, short z, short face);

static DIFFTEST_REF_FUNC void thing_goto_point_on_face_fast_asm(struct Thing *p_thing,
  short x, short z, short face, int plyr)
{
    int stkargs[] = {plyr};
    DIFFTEST_ASM(
      "push 0(%4)\n"
      "call ASM_thing_goto_point_on_face_fast\n"
        : : "a" (p_thing), "d" (x), "b" (z), "c" (face), "S" (stkargs)
        : "cc", "memory");
}

void thing_goto_point_on_face_fast(struct Thing *p_thing, short x, short z, short face, int plyr)
{
    DIFFTEST_SHADOW(thing_goto_point_on_face_fast,
      thing_goto_point_on_face_fast_asm(p_thing, x, z, face, plyr));
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    p_thing->Flag2 |= TgF2_Unkn00080000;
    thing_goto_point_on_face(p_thing, x, z, face);
    if ((p_thing->Flag2 & TgF2_Unkn0040) != 0)
        return;
    set_person_animmode_run(p_thing);
    set_player_peeps_to_run(p_thing, (ushort)plyr);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void find_bezier_nav_point_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_find_bezier_nav_point\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Picks the end of the bezier road owning face `face_no` (eax) farther
 * from p_vehicle (edx), and stores its X and Z coordinates >> 8 at
 * p_nav_x (ebx) and p_nav_z (ecx).
 */
void find_bezier_nav_point(int face_no, struct Thing *p_vehicle, short *p_nav_x, short *p_nav_z)
{
    short face = (short)face_no;
    struct Thing *p_first;
    struct Thing *p_last;
    ushort obj;
    s32 dx, dz, dist_first, dist_last;

    DIFFTEST_SHADOW(find_bezier_nav_point, find_bezier_nav_point_asm(face_no,
      (intptr_t)p_vehicle, (intptr_t)p_nav_x, (intptr_t)p_nav_z));
    if (face > 0)
        obj = game_object_faces3[face].Object;
    else
        obj = game_object_faces4[-face].Object;

    p_last = &things[(ushort)game_objects[obj].ThingNo];
    p_first = p_last;
    while (p_first->U.UObject.PrevThing > 0)
        p_first = &things[p_first->U.UObject.PrevThing];
    while (p_last->U.UObject.NextThing > 0)
        p_last = &things[p_last->U.UObject.NextThing];

    dx = (p_vehicle->X - p_first->X) >> 9;
    dz = (p_vehicle->Z - p_first->Z) >> 9;
    dist_first = dx * dx + dz * dz;
    dx = (p_vehicle->X - p_last->X) >> 9;
    dz = (p_vehicle->Z - p_last->Z) >> 9;
    dist_last = dx * dx + dz * dz;

    if (dist_first < dist_last)
    {
        *p_nav_x = p_last->X >> 8;
        *p_nav_z = p_last->Z >> 8;
    }
    else
    {
        *p_nav_x = p_first->X >> 8;
        *p_nav_z = p_first->Z >> 8;
    }
}


static DIFFTEST_REF_FUNC void thing_goto_point_on_face_asm(struct Thing *p_thing,
  short x, short z, short face)
{
    DIFFTEST_ASM(
      "call ASM_thing_goto_point_on_face\n"
        : : "a" (p_thing), "d" (x), "b" (z), "c" (face));
}

void thing_goto_point_on_face(struct Thing *p_thing, short x, short z, short face)
{
    ushort frame;

    DIFFTEST_SHADOW(thing_goto_point_on_face,
      thing_goto_point_on_face_asm(p_thing, x, z, face));
    if ((p_thing->Flag & (TngF_Unkn40000000 | TngF_Unkn0001)) != 0)
        return;
    if ((p_thing->Flag & (TngF_StationrSht | TngF_Destroyed)) != 0)
        return;
    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_thing);

    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;
        s32 nav_x, nav_z;

        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        if (face < 0)
        {
            struct SingleObjectFace4 *p_face;
            struct Thing *p_building;

            p_face = &game_object_faces4[-face];
            p_building = &things[(ushort)game_objects[(ushort)p_face->Object].ThingNo];
            if (p_building->SubType == SubTT_BLD_BEZIER_ROAD)
            {
                nav_x = 0;
                nav_z = 0;
                // Stores shorts into the low halves of the s32 variables
                find_bezier_nav_point(face, p_vehicle, (short *)&nav_x, (short *)&nav_z);
            }
            else
            {
                nav_z = z;
                nav_x = x;
            }
        }
        else if (face == 0)
        {
            return;
        }
        else
        {
            nav_z = z;
            nav_x = x;
        }
        p_vehicle->Flag &= ~TngF_PlayerAgent;
        start_goto_vehicle(p_vehicle, (short)nav_x, (short)nav_z);
        return;
    }

    p_thing->Flag2 &= ~TgF2_Unkn00020000;
    switch (p_thing->State & 0x7fff)
    {
    case PerSt_NONE:
    case PerSt_GOTO_POINT:
    case PerSt_WAIT:
    case PerSt_GOTO_PERSON:
    case PerSt_GET_ITEM:
    case PerSt_USE_VEHICLE:
        break;
    default:
        return;
    }

    p_thing->State = PerSt_GOTO_POINT;
    p_thing->U.UPerson.ComTimer = -1;
    p_thing->Timer1 = 0x30;
    p_thing->StartTimer1 = 0x30;
    p_thing->SubState = 0;
    p_thing->U.UPerson.ComRange = 1;
    p_thing->U.UPerson.GotoX = x;
    p_thing->U.UPerson.GotoZ = z;
    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        players[p_thing->U.UPerson.ComCur >> 2].GotoFace = face;
    if (p_thing->U.UPerson.PathIndex != 0)
        remove_path(p_thing);
    p_thing->U.UPerson.PathIndex = 0;
    build_navigate_path_to_face_xz(p_thing, -face, x, z);

    if ((p_thing->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if (p_thing->U.UPerson.AnimMode == 0x13)
            return;
        p_thing->U.UPerson.AnimMode = 0x13;
    }
    else
    {
        if (p_thing->U.UPerson.AnimMode != 0x13)
            return;
        p_thing->U.UPerson.AnimMode = gun_out_anim(p_thing, 0);
    }
    p_thing->StartFrame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode] - 1;
    frame = people_frames[p_thing->SubType][p_thing->U.UPerson.AnimMode];
    p_thing->Frame = nstart_ani[frame + p_thing->U.UPerson.Angle];
}

static DIFFTEST_REF_FUNC ubyte select_new_weapon_asm(ushort index, short dir)
{
    ubyte ret;
    DIFFTEST_ASM("call ASM_select_new_weapon\n"
        : "=a" (ret) : "a" (index), "d" (dir));
    return ret;
}

ubyte select_new_weapon(ushort index, short dir)
{
    struct Thing *p_person;
    u32 wepflags;
    short wtype;
    short count;

    DIFFTEST_SHADOW_RET(select_new_weapon, ubyte, select_new_weapon_asm(index, dir));

    p_person = &things[index];
    count = 0;
    if ((p_person->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_person);

    wtype = (short)(p_person->U.UPerson.CurrentWeapon + dir);
    if (wtype < 0)
        wtype = 32;
    if ((wtype < 1) || (wtype > 32))
        DIFFTEST_RETURN((ubyte)0);

    wepflags = p_person->U.UPerson.WeaponsCarried & ~(1 << (WEP_ENERGYSHLD - 1));
    while (((wepflags & (1u << (wtype - 1))) == 0) && (count < 32))
    {
        wtype += dir;
        if ((wtype < 1) || (wtype > 32))
            DIFFTEST_RETURN((ubyte)0);
        count++;
    }
    if (count >= 32)
        DIFFTEST_RETURN((ubyte)p_person->U.UPerson.CurrentWeapon);
    DIFFTEST_RETURN((ubyte)wtype);
}

extern struct UnkPlayerGroup unkn_player_groups[8][5];

static DIFFTEST_REF_FUNC void unkn_player_group_add_asm(sbyte a1, ubyte a2)
{
    DIFFTEST_ASM("call ASM_unkn_player_group_add\n"
        :  : "a" (a1), "d" (a2));
}

void unkn_player_group_add(sbyte a1, ubyte a2)
{
    struct UnkPlayerGroup *p_group;
    PlayerInfo *p_player;
    ubyte grp;
    u32 i;

    DIFFTEST_SHADOW(unkn_player_group_add, unkn_player_group_add_asm(a1, a2));

    // Flat index into the [8][5] array, computed in 8 bits like the asm
    grp = (ubyte)((ubyte)a1 + (ubyte)(a2 * 5));
    p_group = &unkn_player_groups[0][0] + grp;
    p_group->GroupActive = 1;
    p_player = &players[a2];
    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;

        p_agent = p_player->MyAgent[i];
        p_group->AgentWeapons[i] = p_agent->U.UPerson.CurrentWeapon;
        p_group->AgentMoods[i] = p_agent->U.UPerson.Mood;
        if ((ushort)p_agent->State == PerSt_PROTECT_PERSON)
            p_group->AgentOwners[i] = p_agent->Owner;
        else
            p_group->AgentOwners[i] = 0;
    }
}

static DIFFTEST_REF_FUNC void unkn_player_group_prot_asm(sbyte a1, ubyte a2)
{
    DIFFTEST_ASM("call ASM_unkn_player_group_prot\n"
        :  : "a" (a1), "d" (a2));
}

void call_protect_specific(struct Thing *p_person_client, struct Thing *p_protector);

void unkn_player_group_prot(sbyte a1, ubyte a2)
{
    struct UnkPlayerGroup *p_group;
    u32 i;

    DIFFTEST_SHADOW(unkn_player_group_prot, unkn_player_group_prot_asm(a1, a2));

    // Flat index into the [8][5] array, computed in 8 bits like the asm
    p_group = &unkn_player_groups[0][0] + (ubyte)((ubyte)a1 + (ubyte)(a2 * 5));
    if (p_group->GroupActive == 0)
        return;
    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        ubyte wtype;
        ushort owner;

        p_agent = players[a2].MyAgent[i];
        wtype = p_group->AgentWeapons[i];
        if (wtype != 0)
        {
            if ((p_agent->U.UPerson.WeaponsCarried & (1u << ((wtype - 1) & 0x1F))) != 0)
                thing_select_specific_weapon(p_agent, wtype, 2);
        }
        else
        {
            thing_select_specific_weapon(p_agent, p_agent->U.UPerson.CurrentWeapon, 2);
        }
        p_agent->U.UPerson.Mood = p_group->AgentMoods[i];
        call_unprotect(p_agent, a2, 2);
        owner = (ushort)p_group->AgentOwners[i];
        if (owner != 0)
            call_protect_specific(&things[owner], p_agent);
    }
}

static DIFFTEST_REF_FUNC void peep_change_weapon_asm(struct Thing *p_person)
{
    DIFFTEST_ASM(
      "call ASM_peep_change_weapon\n"
        : : "a" (p_person));
}

void peep_change_weapon(struct Thing *p_person)
{
    ushort plagent;

    DIFFTEST_SHADOW(peep_change_weapon, peep_change_weapon_asm(p_person));

    if ((p_person->Flag & TngF_WepCharging) != 0)
    {
        stop_sample_using_heap((ushort)p_person->ThingOffset, 7);
        stop_sample_using_heap((ushort)p_person->ThingOffset, 52);
    }
    if ((p_person->Flag2 & TgF2_Unkn0200) != 0)
    {
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            plagent = (ushort)p_person->U.UPerson.ComCur & 3;
        else
            plagent = 0;
        ReleaseLoopedSample(p_person->ThingOffset, (short)(plagent + 80));
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            plagent = (ushort)p_person->U.UPerson.ComCur & 3;
        else
            plagent = 0;
        ReleaseLoopedSample(p_person->ThingOffset, (short)(plagent + 84));
        ReleaseLoopedSample(p_person->ThingOffset, 14);
        p_person->Flag2 &= ~TgF2_Unkn0200;
    }
    if (((p_person->Flag & TngF_PlayerAgent) != 0)
      && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        ushort comcur = (ushort)p_person->U.UPerson.ComCur;
        players[comcur >> 2].PrevWeapon[comcur & 3] = p_person->U.UPerson.CurrentWeapon;
    }
}

void player_agent_weapon_switch(PlayerIdx plyr, ThingIdx person, short shift)
{
    struct Thing *p_person;

    p_person = &things[person];

    p_person->U.UPerson.CurrentWeapon = select_new_weapon(person, shift);
    peep_change_weapon(p_person);
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        ushort smp;
        smp = weapon_sound_name_speech_index(p_person->U.UPerson.CurrentWeapon);
        play_disk_sample(local_player_no, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
}

StateChRes player_agent_init_drop_item(PlayerIdx plyr, struct Thing *p_person, ThingIdx item)
{
    StateChRes res;
    if ((item == 0) || (item == p_person->U.UPerson.CurrentWeapon)) {
        set_person_anim_mode(p_person, ANIM_PERS_IDLE);
    }
    if (p_person->State == PerSt_PROTECT_PERSON)
        p_person->Flag2 |= TgF2_Unkn10000000;

    res = person_init_drop_item_where_standing(p_person, item);
    //TODO we've just initiated the drop, the weapon is not subtracted yet; a bit early for speed recalc?
    p_person->Speed = calc_person_speed(p_person);
    return res;
}

void person_grp_switch_to_specific_weapon(struct Thing *p_person, PlayerIdx plyr,
  WeaponType wtype, ubyte first_flag)
{
    struct Thing *p_owntng;
    ushort plagent;
    ubyte flag;

    p_owntng = p_person;
    if (p_person->State == PerSt_PROTECT_PERSON)
        p_owntng = &things[p_person->Owner];

    peep_change_weapon(p_person);
    flag = thing_select_specific_weapon(p_person, wtype, first_flag);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
        play_disk_sample(local_player_no, 44, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if ((p_agent->State != PerSt_PROTECT_PERSON) || (p_agent->Owner != p_owntng->ThingOffset)) {
            if (p_agent != p_owntng)
                continue;
        }
        if (p_agent == p_person)
            continue;

        if (!person_carries_weapon(p_agent, wtype) || (flag == WepSel_HIDE))
        {
            stop_looped_weapon_sample(p_agent, p_agent->U.UPerson.CurrentWeapon);
            if (flag == WepSel_HIDE)
            {
                thing_deselect_weapon(p_agent);
            }
            else if (p_agent->U.UPerson.TempWeapon != WEP_NULL)
            {
                thing_select_specific_weapon(p_agent, p_agent->U.UPerson.TempWeapon, flag);
            }
            else
            {
                thing_select_best_weapon_for_range(p_agent, 1280);
            }
        }
        else
        {
            peep_change_weapon(p_agent);
            thing_select_specific_weapon(p_agent, wtype, flag);
        }
        p_agent->U.UPerson.TempWeapon = p_agent->U.UPerson.CurrentWeapon;
    }
}

void cheat_teleport_agent_to_gnd(struct Thing *p_person, MapCoord cor_x, MapCoord cor_z)
{
    remove_path(p_person);
    if (on_mapwho(p_person))
        delete_node(p_person);
    p_person->U.UPerson.OnFace = 0; // Can only teleport to ground
    p_person->X = MAPCOORD_TO_PRCCOORD(cor_x, 0);
    p_person->Z = MAPCOORD_TO_PRCCOORD(cor_z, 0);
    p_person->Y = alt_at_point(cor_x, cor_z);
    add_node_thing(p_person->ThingOffset);
}

void person_give_all_weapons(struct Thing *p_person)
{
    WeaponType wtype;

    for (wtype = WEP_TYPES_COUNT-1; wtype > WEP_NULL; wtype--)
    {
        struct WeaponDef *wdef;
        u32 wepflg;

        wdef = &weapon_defs[wtype];

        if ((wdef->Flags & WEPDFLG_CanPurchease) == 0)
            continue;

        wepflg = 1 << (wtype-1);
        p_person->U.UPerson.WeaponsCarried |= wepflg;
    }
    player_agent_set_weapon_quantities_max(p_person);
    person_weapons_update_previous(p_person);
}

void mark_all_weapons_researched(void)
{
    WeaponType wtype;

    for (wtype = WEP_TYPES_COUNT-1; wtype > WEP_NULL; wtype--)
    {
        struct WeaponDef *wdef;

        wdef = &weapon_defs[wtype];

        if ((wdef->Flags & WEPDFLG_CanPurchease) == 0)
            continue;

        research_weapon_complete(wtype);
    }
}

void resurrect_any_dead_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;

        if ((p_agent->Flag & TngF_Destroyed) != 0)
            person_resurrect(p_agent);
        else if (p_agent->State == PerSt_PERSON_BURNING)
            person_burning_stifle_fire(p_agent);
    }
}

void give_all_weapons_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_give_all_weapons(p_agent);
    }
    mark_all_weapons_researched();
}

void give_best_mods_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_give_best_mods(p_agent);
    }
}

void set_max_stats_to_all_agents(PlayerIdx plyr)
{
    PlayerInfo *p_player;
    int i;

    p_player = &players[plyr];

    for (i = 0; i < playable_agents; i++)
    {
        struct Thing *p_agent;
        p_agent = p_player->MyAgent[i];
        if (p_agent->Type != TT_PERSON)
            continue;
        if (thing_is_destroyed(p_agent->ThingOffset))
            continue;
        person_set_helath_to_max_limit(p_agent);
        person_set_energy_to_max_limit(p_agent);
        person_set_persuade_power__to_allow_all(p_agent);
    }
}

extern struct TbIPXPlayer IPXPlayer;

/** Marks in a1[0..7] the IPX player slots in use; returns 1 when the
 * network service is IPX, -1 otherwise (a1 cleared in both cases).
 * C version of the asm (0x0E57B0): the slots are 0x1C bytes apart from
 * IPXPlayer+0x2D, a slot is in use when its word at +0x1A is non-zero.
 */
int net_unkn_func_12(void *a1)
{
    ubyte *p_out = (ubyte *)a1;
    ubyte *p_slot;
    int i;

    memset(p_out, 0, 8);
    if (NetworkServicePtr.I.Type != 1)
        return -1;
    p_slot = (ubyte *)&IPXPlayer + 0x2D;
    for (i = 0; i < 8; i++, p_slot += 0x1C)
    {
        if (*(ushort *)(p_slot + 0x1A) != 0)
            p_out[i] = 1;
    }
    return 1;
}

void net_unkn_check_1(void)
{
    char locstr[100];
    ushort i, m;
    s32 check_val;
    char recvd2[8];
    char recvd3[8];
    char recvd[8];

    LbMemorySet(recvd, 0, 8);
    if ((PacketRecord_IsPlayback()) && in_network_game)
    {
        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            if (((1 << i) & ingame.InNetGame_UNSURE) != 0) {
                PacketRecord_Read(&packets[i], players[i].DoubleMode);
            }
        }
    }
    else if (nsvc.I.Type == NetSvc_IPX)
    {
        struct Packet *p_pckt;
        int ret;

        p_pckt = &packets[local_player_no];
        p_pckt->D1Seed = lbSeed;
        p_pckt->D2Check = ingame.fld_unkC4B;
        net_unkn_func_12(recvd2);
        ret = LbNetworkExchange(packets, sizeof(struct Packet));
        if (gameturn == 5) {
            LbNetworkSetTimeoutSec(15);
        }
        if (ret == -1)
        {
            net_unkn_func_12(recvd3);
            for (i = 0; i < PLAYERS_LIMIT; i++)
            {
                if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
                    continue;
                if (recvd2[i] == recvd3[i])
                    continue;

                sprintf(locstr, " Player >%s< Has Timed Out", net_player_names[i]);
                show_message(locstr);
                ingame.InNetGame_UNSURE &= ~(1 << i);
                if (i == local_player_no)
                {
                  show_message("You have Timed out from the system");
                  ingame.DisplayMode = DpM_PURPLEMNU;
                  StopCD();
                  StopAllSamples();
                }
                net_players_num--;
            }
        }
        else if (ret == -8)
        {
            show_message(" Host Connection Lost");
            ingame.InNetGame_UNSURE = 0;
            ingame.DisplayMode = DpM_PURPLEMNU;
            StopCD();
            StopAllSamples();
        }
    }
    else
    {
        struct Packet *p_pckt;
        struct ShortPacket *p_shpckt;

        p_pckt = &packets[local_player_no];
        p_shpckt = &shpackets[local_player_no];
        p_shpckt->Action = p_pckt->Action;
        p_shpckt->Data = p_pckt->Data;
        p_shpckt->X = p_pckt->X;
        p_shpckt->Y = p_pckt->Y;
        p_shpckt->Z = p_pckt->Z;
        p_shpckt->BCheck = ingame.fld_unkC4B;

        LbNetworkExchange(shpackets, sizeof(struct ShortPacket));

        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            struct Packet *p_pckt;
            struct ShortPacket *p_shpckt;

            p_pckt = &packets[i];
            p_shpckt = &shpackets[i];
            p_pckt->Action = p_shpckt->Action;
            p_pckt->Action2 = 0;
            p_pckt->Action3 = 0;
            p_pckt->Action4 = 0;
            p_pckt->Data = p_shpckt->Data;
            p_pckt->X = p_shpckt->X;
            p_pckt->Y = p_shpckt->Y;
            p_pckt->Z = p_shpckt->Z;
            p_pckt->D1Seed = lbSeed;
            p_pckt->D2Check = p_shpckt->BCheck;
        }
    }

    if (nsvc.I.Type == NetSvc_IPX)
        check_val = ingame.fld_unkC4B;
    else
        check_val = (ubyte)ingame.fld_unkC4B;
    if (nsvc.I.Type == NetSvc_IPX)
    {
        for (i = 0; i < PLAYERS_LIMIT; i++)
        {
            if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
                continue;
            for (m = 0; m < PLAYERS_LIMIT; m++)
            {
                if ( ((1 << m) & ingame.InNetGame_UNSURE) == 0)
                    continue;
                if (packets[m].D2Check == packets[i].D2Check)
                    recvd[i]++;
            }
        }
    }

    for (i = 0; i < PLAYERS_LIMIT; i++)
    {
        if (((1 << i) & ingame.InNetGame_UNSURE) == 0)
            continue;

        if (PacketRecord_IsRecord() && in_network_game && (net_host_player_no == local_player_no))
            PacketRecord_Write(&packets[i], players[i].DoubleMode);

        if ((nsvc.I.Type == NetSvc_IPX) && (recvd[i] == 1) && (net_players_num > 2))
        {
            sprintf(locstr, " Player >%s< is out of sync", net_player_names[i]);
            show_message(locstr);
        }
        if (check_val != packets[i].D2Check)
        {
          show_message(" CHECK ERROR ");
          clear_gamekey_pressed(GKey_MISSN_RESTART);
          in_network_game = 2;
          StopCD();
          test_missions(1);
          init_level_3d(1);
          restart_back_into_mission(ingame.CurrentMission);
          in_network_game = 1;
          return;
        }
    }
}

extern char player_message_text[PLAYERS_LIMIT][128];
extern ubyte player_message_timer[PLAYERS_LIMIT];

static DIFFTEST_REF_FUNC void player_chat_message_add_key_asm(ushort a1, int a2)
{
    DIFFTEST_ASM("call ASM_player_chat_message_add_key\n"
        :  : "a" (a1), "d" (a2));
}

/** Adds a key to the chat message of player `plyr` (from ax); `keycode`
 * (edx) holds the key code, with shift in bit 0x100.
 */
void player_chat_message_add_key(ushort plyr, int keycode)
{
    u32 key;
    u32 shift;
    s32 len;
    int c;

    DIFFTEST_SHADOW(player_chat_message_add_key,
      player_chat_message_add_key_asm(plyr, keycode));
    if (player_message_timer[plyr] != 0)
    {
        player_message_timer[plyr] = 0;
        player_message_text[plyr][0] = '\0';
    }
    len = strlen(player_message_text[plyr]);
    shift = (ushort)keycode & 0x100;
    key = (ushort)keycode & 0xFEFF;

    if (key == 0x0E) // backspace
    {
        if (len > 0)
            player_message_text[plyr][len - 1] = '\0';
        return;
    }
    if (key == 0x1C) // enter
    {
        if ((shift == 0) ||
          (net_player_colours[plyr] == net_player_colours[(ubyte)local_player_no]))
            player_message_timer[plyr] = 150;
        if ((ubyte)local_player_no == plyr)
            players[(ubyte)local_player_no].PanelState[mouser] = 0;
        return;
    }

    if (shift != 0)
        c = (ubyte)lbInkeyToAsciiShift[key];
    else
        c = (ubyte)lbInkeyToAscii[key];
    c = toupper(c);

    if (((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) ||
      (c == '.') || (c == ',') || (c == '-') || (c == '=') ||
      (c == '!') || (c == '"') || (c == '?') || (c == '(') ||
      (c == ')') || (c == '*') || (c == '#') || (c == '~') ||
      (c == '+') || (c == ':') || (c == ';') || (c == '\'') ||
      (c == ' '))
    {
        if (len == 127)
            len--;
        player_message_text[plyr][len] = c;
        player_message_text[plyr][len + 1] = '\0';
    }
}

void plgroup_set_mood(PlayerIdx plyr, struct Thing *p_member, short mood)
{
    struct Thing *p_owntng;
    ushort plagent;

    p_owntng = p_member;
    if (p_member->State == PerSt_PROTECT_PERSON)
        p_owntng = &things[p_member->Owner];

    p_member->U.UPerson.Mood = limit_mood(p_member, mood);
    p_member->Speed = calc_person_speed(p_member);

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        struct Thing *p_agent;

        p_agent = players[plyr].MyAgent[plagent];
        if ((p_agent <= &things[0]) || (p_agent >= &things[THINGS_LIMIT]))
            continue;

        if ((p_agent->State != PerSt_PROTECT_PERSON) || (p_agent->Owner != p_owntng->ThingOffset)) {
            if (p_agent != p_owntng)
                continue;
        }
        if (p_agent == p_member) // already updated
            continue;

        p_agent->U.UPerson.Mood = limit_mood(p_agent, mood);
        p_agent->Speed = calc_person_speed(p_agent);
    }
}

void player_agent_init_goto_gnd_point_abs(PlayerIdx plyr, struct Thing *p_person,
  MapCoord cor_x, MapCoord cor_y, MapCoord cor_z)
{
    if (plyr == local_player_no)
        show_goto_point(1);
    p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
    thing_goto_point(p_person, cor_x, cor_y, cor_z);
}

void player_agent_select_specific_weapon(PlayerIdx plyr, struct Thing *p_person,
  WeaponType wtype, ubyte flag)
{
    thing_select_specific_weapon(p_person, wtype, flag);
    peep_change_weapon(p_person);
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);
    p_person->U.UPerson.TempWeapon = p_person->U.UPerson.CurrentWeapon;

    if ((plyr == local_player_no) && (p_person->U.UPerson.CurrentWeapon != 0))
    {
        ushort smp;
        smp = weapon_sound_name_speech_index(p_person->U.UPerson.CurrentWeapon);
        play_disk_sample(local_player_no, smp, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
}

void process_packet(PlayerIdx plyr, struct Packet *p_pckt, ushort i)
{
    struct Thing *p_thing;
    struct Thing *p_sectng;
    int n;
    short result;

    result = PARes_EBADRQC;
    switch (p_pckt->Action & ~PActF_All)
    {
    case PAct_MISSN_ABORT:
        if (in_network_game) {
            net_player_leave(plyr);
            result = PARes_DONE;
            break;
        }
        exit_game = 1;
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_ABS:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        player_agent_init_goto_gnd_point_abs(plyr, p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_REL:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_goto_point_rel(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_SELECT_NEXT_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_weapon_switch(plyr, p_pckt->Data, 1);
        result = PARes_DONE;
        break;
    case PAct_DROP_SELC_WEAPON_SECR:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        if (p_thing->U.UPerson.CurrentWeapon == 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_init_drop_item(plyr, p_thing, 0);
        result = PARes_DONE;
        break;
    case PAct_PICKUP:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_pickup(p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_ENTER_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_enter_vehicle(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_LEAVE_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_attempt_to_leave_vehicle(p_thing);
        result = PARes_DONE;
        break;
    case PAct_SELECT_AGENT:
        p_sectng = get_thing_safe(p_pckt->X, TT_PERSON);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_sectng, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        player_agent_select(plyr, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_REL_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_goto_point_rel_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, plyr);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_GND_POINT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SELECT_PREV_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_weapon_switch(plyr, p_pckt->Data, -1);
        result = PARes_DONE;
        break;
    case PAct_PROTECT_INC:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        call_protect(p_thing, plyr);
        n = count_protect(p_thing, plyr);
        if (plyr == local_player_no && n)
            play_sample_using_heap(0, 61, FULL_VOL, EQUL_PAN, 90 + 5 * n, LOOP_NO, 3);
        result = PARes_DONE;
        break;
    case PAct_PROTECT_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        call_unprotect(p_thing, plyr, 0);
        n = count_protect(p_thing, plyr);
        if (plyr == local_player_no && n)
            play_sample_using_heap(0, 61, FULL_VOL, EQUL_PAN, 90 + 5 * n, LOOP_NO, 3);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_THING:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_thing(p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_GET_ITEM:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_get_item(p_thing, p_pckt->X, plyr);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_GND_PT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SELECT_SPECIFIC_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (p_pckt->Y > WepSel_HIDE) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_select_specific_weapon(plyr, p_thing, p_pckt->X, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_DROP_HELD_WEAPON_SECR:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        player_agent_init_drop_item(plyr, p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_SET_MOOD:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        p_thing->U.UPerson.Mood = limit_mood(p_thing, p_pckt->X);
        p_thing->Speed = calc_person_speed(p_thing);
        result = PARes_DONE;
        break;
    case PAct_GO_ENTER_VEHICLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        person_go_enter_vehicle(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_FOLLOW_PERSON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_PERSON);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_follow_person(p_thing, p_sectng);
        result = PARes_DONE;
        break;
    case PAct_CONTROL_MODE:
        if ((p_pckt->Data & ~UInpCtr_AllFlagsMask) >= UInpCtr_MODES_COUNT) {
            result = PARes_EINVAL;
            break;
        }
        user_input_control_mode_set(plyr, 0, p_pckt->Data);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_FACE_PT_ABS:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_on_face(p_thing, p_pckt->X, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_GND_PT_ABS_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, plyr);
        result = PARes_DONE;
        break;
    case PAct_AGENT_GOTO_FACE_PT_ABS_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (plyr == local_player_no)
            show_goto_point(1);
        p_thing->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
        thing_goto_point_on_face_fast(p_thing, p_pckt->X, p_pckt->Z, p_pckt->Y, plyr);
        result = PARes_DONE;
        break;
    case PAct_GO_ENTER_VEHICLE_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        p_sectng = get_thing_safe(p_pckt->X, TT_VEHICLE);
        if (p_sectng == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_go_enter_vehicle_fast(p_thing, p_sectng, plyr);
        result = PARes_DONE;
        break;
    case PAct_GET_ITEM_FAST:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        person_init_get_item_fast(p_thing, p_pckt->X, plyr);
        result = PARes_DONE;
        break;
    case PAct_SHIELD_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!person_can_toggle_supershield(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_supershield_toggle(p_thing);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_GND_PT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine_fast(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 0);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_GND_POINT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 1);
        result = PARes_DONE;
        break;
    case PAct_PEEPS_SCATTER:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        make_peeps_scatter(p_thing, p_pckt->X, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_SELECT_GRP_SPEC_WEAPON:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (p_pckt->Y > WepSel_SKIP) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_TNGBADST;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_grp_switch_to_specific_weapon(p_thing, plyr, p_pckt->X, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_USE_MEDIKIT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!person_can_use_medikit(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_use_medikit(p_thing, plyr);
        result = PARes_DONE;
        break;
    case PAct_GROUP_SET_MOOD:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        plgroup_set_mood(plyr, p_thing, p_pckt->X);
        result = PARes_DONE;
        break;
    case PAct_AGENT_UNKGROUP_PROT: // Unfinished mess; remove pending
        unkn_player_group_prot(p_pckt->Data, plyr);
        result = PARes_DONE;
        break;
    case PAct_AGENT_UNKGROUP_ADD: // Unfinished mess; remove pending
        unkn_player_group_add(p_pckt->Data, plyr);
        result = PARes_DONE;
        break;
    case PAct_THERMAL_TOGGLE:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (!player_can_toggle_thermal(plyr)) {
            result = PARes_TNGBADST;
            break;
        }
        player_toggle_thermal(plyr);
        result = PARes_DONE;
        break;
    case PAct_CHAT_MESSAGE_KEY:
        player_chat_message_add_key(plyr, p_pckt->Data);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_FACE_POINT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 2);
        result = PARes_DONE;
        break;
    case PAct_SHOOT_AT_FACE_POINT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        thing_shoot_at_point(p_thing, p_pckt->X, p_pckt->Y, p_pckt->Z, 3);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_FACE_PT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine(p_thing, p_pckt->X, 0, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_PLANT_MINE_AT_FACE_PT_FF:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (p_thing->State == PerSt_DROP_ITEM) {
            result = PARes_EALREADY;
            break;
        }
        if ((p_thing->Flag2 & TgF2_KnockedOut) != 0) {
            result = PARes_TNGBADST;
            break;
        }
        person_init_plant_mine_fast(p_thing, p_pckt->X, 0, p_pckt->Z, p_pckt->Y);
        result = PARes_DONE;
        break;
    case PAct_AGENT_SELF_DESTRUCT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        if (person_is_executing_commands(p_thing->ThingOffset)) {
            result = PARes_TNGBADST;
            break;
        }
        person_self_destruct(p_thing);
        result = PARes_DONE;
        break;
    case PAct_CHEAT_AGENT_TELEPORT:
        p_thing = get_thing_safe(p_pckt->Data, TT_PERSON);
        if (p_thing == INVALID_THING) {
            result = PARes_EINVAL;
            break;
        }
        if (person_slot_as_player_agent(p_thing, plyr) < 0) {
            result = PARes_EBADSLT;
            break;
        }
        cheat_teleport_agent_to_gnd(p_thing, p_pckt->X, p_pckt->Z);
        result = PARes_DONE;
        break;
    case PAct_CHEAT_ALL_AGENTS:
        switch (p_pckt->Data)
        {
        case PCheatAA_RESURRECT_AND_WEPAPN:
            resurrect_any_dead_agents(plyr);
            give_all_weapons_to_all_agents(plyr);
            player_agents_clear_weapon_delays(plyr);
            result = PARes_DONE;
            break;
        case PCheatAA_BEEFUP_AND_MODS:
            result = PARes_DONE;
            give_best_mods_to_all_agents(plyr);
            set_max_stats_to_all_agents(plyr);
            break;
        default:
            result = PARes_EINVAL;
            break;
        }
        break;
    case PAct_NONE:
        result = PARes_DONE;
        break;
    }
    if ((debug_log_things & 0x02) != 0)
    {
        char locstr[192];

        snprint_packet(locstr, sizeof(locstr), p_pckt);

        if ((p_pckt->Action & ~PActF_All) == PAct_NONE)
            ; // no logging for empty packet
        else if (result <= PARes_SUCCESS)
            LOGSYNC_F("Player %d packet %s: %s", (int)plyr,
              locstr, get_packet_action_result_text(result));
        else
            LOGWARN("Player %d packet %s: %s", (int)plyr,
              locstr, get_packet_action_result_text(result));
    }
}

void process_packets(void)
{
    PlayerIdx plyr;

    if (in_network_game && (net_players_num > 1))
        net_unkn_check_1();

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        struct Packet *packet;
        ushort i;

        if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
            continue;
        packet = &packets[plyr];
        for (i = 0; i < LOCAL_USERS_MAX_COUNT; i++)
        {
            struct Thing *p_thing;

            p_thing = get_thing_safe(packet->Data, TT_PERSON);

            if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
                packet->Action = PAct_NONE;

            if (p_thing != INVALID_THING)
            {
                if ((packet->Action & PActF_TriggerUse) == 0)
                    p_thing->Flag &= ~TngF_TriggerUse;
                else
                    p_thing->Flag |= TngF_TriggerUse;
            }

            process_packet(plyr, packet, i);

            packet->Action = PAct_NONE;
            packet = (struct Packet *)((ubyte *)packet + 10);
        }
    }
}

/******************************************************************************/
