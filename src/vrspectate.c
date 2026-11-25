/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vrspectate.c
 *     Publishing the game world for the external VR spectator viewer.
 * @par Purpose:
 *     Every game turn, publishes the world (swview_update(), libswarsview)
 *     in shared memory, where the VR viewer (vr/viewer) reads it, and takes
 *     the viewer's commands. See vrshare.h for the layout.
 * @par Comment:
 *     Windows only. The test aids (SWARS_VR_TEST_*) change the game.
 * @author   SyndicateVR
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "vrspectate.h"

#include <stdlib.h>
#include <string.h>
#include "bigmap.h"
#include "bmbang.h"
#include "building.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "packet.h"
#include "player.h"
#include "thing.h"
#include "thing_fire.h"
#include "weapon.h"
#include "swview.h"
#include "vrshare.h"
#include "swlog.h"
#if defined(WIN32) || defined(_WIN32)
# include <windows.h>
#endif
/******************************************************************************/

// the block of libswarsview is the shared memory's layout
_Static_assert(SWV_TOTAL_SIZE == VRS_TOTAL_SIZE, "swview block layout");
_Static_assert(SWV_OFF_HUD == VRS_OFF_HUD, "swview block layout");

static TbBool vr_enabled;
static TbBool vr_env_checked;
static ubyte *vr_block;
static struct VrsCmdQueue *vr_cmds;

void vr_spectator_enable(TbBool enable)
{
    vr_enabled = enable;
    vr_env_checked = true;
}

static TbBool vr_open_block(void)
{
#if defined(WIN32) || defined(_WIN32)
    HANDLE hmap;
    struct VrsHeader *p_hdr;

    hmap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      0, VRS_TOTAL_SIZE, VRS_MAPPING_NAME);
    if (hmap == NULL) {
        LOGERR("cannot create shared memory, error %lu", (ulong)GetLastError());
        return false;
    }
    vr_block = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, VRS_TOTAL_SIZE);
    if (vr_block == NULL) {
        LOGERR("cannot map shared memory, error %lu", (ulong)GetLastError());
        return false;
    }
    p_hdr = (struct VrsHeader *)vr_block;
    memset(p_hdr, 0, sizeof(*p_hdr));
    p_hdr->magic = VRS_MAGIC;
    p_hdr->version = VRS_VERSION;
    p_hdr->total_size = VRS_TOTAL_SIZE;
    p_hdr->off_static = VRS_OFF_STATIC;
    p_hdr->off_dyn = VRS_OFF_DYN;
    p_hdr->off_things = VRS_OFF_THINGS;
    p_hdr->off_tmap = VRS_OFF_TMAP;
    p_hdr->off_palette = VRS_OFF_PALETTE;
    LOGSYNC_F("VR spectator shared memory '%s' ready, %u bytes",
      VRS_MAPPING_NAME, (uint)VRS_TOTAL_SIZE);

    hmap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      0, sizeof(struct VrsCmdQueue), VRS_CMD_MAPPING_NAME);
    if (hmap != NULL)
        vr_cmds = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct VrsCmdQueue));
    if (vr_cmds != NULL) {
        memset(vr_cmds, 0, sizeof(*vr_cmds));
        vr_cmds->magic = VRS_CMD_MAGIC;
    } else {
        LOGWARN("no command channel, the viewer can only watch");
    }
    return true;
#else
    return false;
#endif
}

/******************************************************************************/

/** Test aid: with SWARS_VR_TEST_BANG=1, makes an explosion and a fire near
 * the first player agent every 48 turns, and the other agents fire a laser,
 * an electric laser and a laser beam, to check the effects export without
 * playing.
 */
static void vr_test_bangs(void)
{
    static int test_bang = -1;
    struct Thing *p_agent;
    s32 prc_x, prc_y, prc_z;

    if (test_bang < 0) {
        const char *s = getenv("SWARS_VR_TEST_BANG");
        test_bang = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!test_bang)
        return;
    if ((gameturn % 48) == 20)
    {
        int i;
        // without a target, only the selected agent shoots, straight ahead
        for (i = 1; i < 4; i++)
        {
            u32 flag_bkp;
            p_agent = players[local_player_no].MyAgent[i];
            if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
                continue;
            flag_bkp = p_agent->Flag;
            p_agent->Flag |= TngF_SelectedAgent;
            if (i == 1)
                init_laser(p_agent, 0);
            else if (i == 2)
                init_laser_elec(p_agent, 0);
            else
                init_laser_beam(p_agent, 0, 0);
            p_agent->Flag = (p_agent->Flag & ~TngF_SelectedAgent) | (flag_bkp & TngF_SelectedAgent);
        }
    }
    if ((gameturn % 48) != 40)
        return;
    p_agent = players[local_player_no].MyAgent[0];
    if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
        return;
    prc_x = p_agent->X + MAPCOORD_TO_PRCCOORD(700, 0);
    prc_z = p_agent->Z + MAPCOORD_TO_PRCCOORD(300, 0);
    prc_y = p_agent->Y;
    bang_new4(prc_x, prc_y, prc_z, 20);
    FIRE_new(prc_x - MAPCOORD_TO_PRCCOORD(500, 0), PRCCOORD_TO_YCOORD(prc_y), prc_z, 3);
}

/** Test aid: with SWARS_VR_TEST_COLLAPSE=1, destroys the building nearest
 * to the first player agent once, at turn 100, to check how destruction
 * shows.
 */
static void vr_test_collapse(void)
{
    static int test_collapse = -1;
    struct Thing *p_agent, *p_best;
    ThingIdx thing;
    s32 best_dist;

    if (test_collapse < 0) {
        const char *s = getenv("SWARS_VR_TEST_COLLAPSE");
        test_collapse = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!test_collapse || (gameturn != 100))
        return;
    p_agent = players[local_player_no].MyAgent[0];
    if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
        return;
    p_best = NULL;
    best_dist = 0;
    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing = &things[thing];
        s32 dx, dz, dist;
        if ((p_thing->Type != TT_BUILDING) || (p_thing->U.UObject.NumbObjects == 0))
            continue;
        if ((p_thing->SubType == SubTT_BLD_MOVN_ROTOR) || (p_thing->SubType == SubTT_BLD_MGUN))
            continue;
        dx = (p_thing->X - p_agent->X) >> 8;
        dz = (p_thing->Z - p_agent->Z) >> 8;
        dist = dx * dx + dz * dz;
        if ((p_best == NULL) || (dist < best_dist)) {
            p_best = p_thing;
            best_dist = dist;
        }
    }
    if (p_best != NULL) {
        LOGSYNC_F("VR test: collapsing building %d", (int)p_best->ThingOffset);
        collapse_building(p_best->X >> 8, p_best->Y >> 8, p_best->Z >> 8, p_best);
    }
}

/** Executes the next command from the viewer, as the packet a mouse click
 * would build (see process_mouse_inputs()); one per turn, as the game
 * takes one packet per turn from the local player. Called from
 * load_packet() when the player gave no input of their own.
 * Returns whether a packet was built.
 */
TbBool vr_spectator_input(void)
{
    struct VrsCmd cmd;
    struct Packet *p_pckt;
    PlayerInfo *p_locplayer;
    ThingIdx dcthing;
    int map_y;

    if ((vr_cmds == NULL) || (vr_cmds->read == vr_cmds->write))
        return false;
    if (vr_cmds->write - vr_cmds->read > VRS_CMDS_MAX) // viewer ran ahead; drop
        vr_cmds->read = vr_cmds->write - VRS_CMDS_MAX;
    p_pckt = &packets[local_player_no];
    cmd = vr_cmds->cmds[vr_cmds->read % VRS_CMDS_MAX];
    __sync_synchronize();
    vr_cmds->read++;

    p_locplayer = &players[local_player_no];
    dcthing = p_locplayer->DirectControl[0];
    if ((dcthing <= 0) || (things[dcthing].Type != TT_PERSON))
        return false;
    p_pckt->Action = PAct_NONE;
    switch (cmd.type)
    {
    case VrsCmd_SELECT_AGENT:
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
    case VrsCmd_GOTO:
        if ((cmd.x <= 0) || (cmd.x >= MAP_COORD_WIDTH) || (cmd.z <= 0) || (cmd.z >= MAP_COORD_HEIGHT))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS, dcthing, cmd.x, map_y, cmd.z);
        break;
    case VrsCmd_SHOOT_THING:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type != TT_NONE))
            my_build_packet(p_pckt, PAct_SHOOT_AT_THING, dcthing, cmd.thing, 0, 0);
        break;
    case VrsCmd_SHOOT_GROUND:
        if ((cmd.x <= 0) || (cmd.x >= MAP_COORD_WIDTH) || (cmd.z <= 0) || (cmd.z >= MAP_COORD_HEIGHT))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT, dcthing, cmd.x, map_y, cmd.z);
        break;
    case VrsCmd_WEAPON_NEXT:
        my_build_packet(p_pckt, PAct_SELECT_NEXT_WEAPON, dcthing, 0, 0, 0);
        break;
    case VrsCmd_WEAPON_PREV:
        my_build_packet(p_pckt, PAct_SELECT_PREV_WEAPON, dcthing, 0, 0, 0);
        break;
    case VrsCmd_WEAPON_SELECT:
        if ((cmd.arg > 0) && (cmd.arg < WEP_TYPES_COUNT))
            my_build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, dcthing, cmd.arg, WepSel_TOGGLE, 0);
        break;
    case VrsCmd_ENTER_VEHICLE:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type == TT_VEHICLE))
            my_build_packet(p_pckt, PAct_GO_ENTER_VEHICLE, dcthing, cmd.thing, 0, 0);
        break;
    default:
        break;
    }
    return (p_pckt->Action != PAct_NONE);
}

void vr_spectator_update(void)
{
    struct VrsHeader *p_hdr;

    if (!vr_env_checked) {
        const char *s = getenv("SWARS_VR_SPECTATOR");
        vr_env_checked = true;
        vr_enabled = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!vr_enabled)
        return;
    if (vr_block == NULL) {
        if (!vr_open_block()) {
            vr_enabled = false;
            return;
        }
    }
    p_hdr = (struct VrsHeader *)vr_block;
    if ((ingame.DisplayMode == DpM_ENGINEPLY) && (game_my_big_map != NULL))
    {
        vr_test_bangs();
        vr_test_collapse();
    }
    p_hdr->seq++; // odd - writing
    __sync_synchronize();
    swview_update(vr_block);
    __sync_synchronize();
    p_hdr->seq++; // even - consistent
}

/******************************************************************************/
