/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_turn.c
 *     One game turn of a mission, in the game core.
 * @par Purpose:
 *     The parts of a game turn that change the game, which the desktop main
 *     loop (game_process_turn() in game.c) calls between reading the input
 *     and drawing, and game_core_turn(), a whole turn without the front
 *     end's input and drawing.
 * @par Comment:
 *     game_core_turn() must play as game_process_turn() does when the
 *     player gives no input and the world is not drawn (swcore_headless
 *     --no-draw), with the line of sight fix on.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_turn.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "bfutility.h"

#include "engincam.h"
#include "enginpeff.h"

#include "bat.h"
#include "bmbang.h"
#include "command.h"
#include "difftest.h"
#include "game.h"
#include "game_options.h"
#include "game_camera.h"
#include "game_misend.h"
#include "game_speed.h"
#include "game_world.h"
#include "lvobjctv.h"
#include "packet.h"
#include "people.h"
#include "player.h"
#include "plyr_packet.h"
#include "plyr_usrinp.h"
#include "swcore_game.h"
#include "swfe.h"
#include "thing.h"
#include "thing_expld.h"
#include "thing_view.h"
#include "vehicle.h"
#include "swlog.h"
/******************************************************************************/

/** The camera of one turn: zoom for the agent's weapon, rotation speed. */
void game_turn_camera(void)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    process_view_inputs(p_locplayer->DirectControl[0]);
    camera_apply_velocity();
}

/** What a turn changes in the world before the world is drawn: the mechs,
 * the craters, and what drawing the things in view changed.
 */
void game_turn_world_view(void)
{
    mech_gameturn_reinit();
    process_map_craters();
    game_view_pass();
}

/** The end of the view part of a turn: the BAT game, navigation stats. */
void game_turn_view_end(void)
{
    BAT_play();

    if (execute_commands)
    {
        s32 tmp;
        gamep_unknval_16 = nav_stats__ThisTurn;
        nav_stats__ThisTurn = 0;
        gamep_unknval_12++;
        gamep_unknval_10 += gamep_unknval_16;
        gamep_unknval_15 = gamep_unknval_14;
        tmp = gamep_unknval_14 + gamep_unknval_11;
        gamep_unknval_14 = 0;
        gamep_unknval_11 = tmp;
    }
}

/** The local player's turn without input (do_user_interface() when no key
 * is pressed and the pointer is not used). Returns whether a packet was
 * built.
 */
static TbBool game_turn_player_idle(void)
{
    PlayerInfo *p_locplayer;
    struct SpecialUserInput *p_usrinp;

    p_locplayer = &players[local_player_no];
    update_danger_music(0);
    game_camera_update(0, 0);
    game_camera_input(0, 0, 0);

    if (ingame.TrackThing)
    {
        build_packet(&packets[local_player_no], 0, p_locplayer->DirectControl[0], 0, 0, 0);
        return true;
    }
    if ((ingame.Flags & GamF_Unkn0100) != 0)
    {
        build_packet(&packets[local_player_no], 0, p_locplayer->DirectControl[0], 0, 0, 0);
        return true;
    }
    if (p_locplayer->DoubleMode)
        return false;
    p_usrinp = &p_locplayer->UserInput[0];
    do_user_input_bits_control_clear_all(p_usrinp);
    do_user_input_bits_direction_clear(p_usrinp);
    return false;
}

/** The packet of each directly controlled agent when the player gave no
 * command: no action, the trigger released (input_user_control_agent()
 * without input). Processing it stops the agent's shooting.
 */
static void game_turn_player_agents_idle(void)
{
    PlayerInfo *p_locplayer;
    struct Packet *p_pckt;
    short dmuser;

    p_locplayer = &players[local_player_no];
    p_pckt = &packets[local_player_no];
    for (dmuser = 0; dmuser < p_locplayer->DoubleMode + 1; dmuser++)
    {
        ThingIdx dcthing;

        if (p_locplayer->DoubleMode != 0) {
            ushort ctlmode;
            ctlmode = user_input_control_mode_get(local_player_no, dmuser);
            if (ctlmode == UInpCtr_Mouse)
                continue;
        }
        if (ingame.TrackThing != 0)
            continue;

        dcthing = p_locplayer->DirectControl[dmuser];
        if (dmuser == 0)
            build_packet(p_pckt, PAct_NONE, dcthing, 0, 0, 0);
        else if (dmuser == 1)
            build_packet2(p_pckt, PAct_NONE, dcthing, 0, 0, 0);
        else
            build_packet4(p_pckt, PAct_NONE, dcthing, 0, 0, 0);
    }
}

#if DIFFTEST_HARNESS
/** Writes game_state_hash() and the random seed each turn to the file
 * named by $SWARS_STATETRACE, so that two builds can be compared turn by
 * turn (e.g. after routing asm through C wrappers).
 */
void game_turn_statetrace(void)
{
    static FILE *fp = NULL;
    static TbBool tried = false;
    const u32 *beg, *end;

    if (!tried) {
        const char *fname = getenv("SWARS_STATETRACE");
        tried = true;
        if (fname != NULL)
            fp = fopen(fname, "wb"); // LF on every OS, for diffing
    }
    if (fp == NULL)
        return;
    fprintf(fp, "%u %08x %08x\n", (u32)gameturn, (uint)game_state_hash(), (u32)lbSeed);
    fflush(fp);
    if ((getenv("SWARS_STATETRACE_DUMP") != NULL) &&
      (strtoul(getenv("SWARS_STATETRACE_DUMP"), NULL, 0) == gameturn))
    {
        char fname[256];
        FILE *dfp;
        beg = (const u32 *)((ubyte *)sthings - STHINGS_LIMIT * sizeof(struct SimpleThing));
        end = (const u32 *)((ubyte *)things + THINGS_LIMIT * sizeof(struct Thing));
        snprintf(fname, sizeof(fname), "%s.dump", getenv("SWARS_STATETRACE"));
        dfp = fopen(fname, "wb");
        if (dfp != NULL) {
            u32 lo = (u32)(uintptr_t)beg;
            fwrite(&lo, sizeof(lo), 1, dfp);
            fwrite(beg, 1, (ubyte *)end - (ubyte *)beg, dfp);
            fclose(dfp);
        }
    }
}
#else
void game_turn_statetrace(void)
{
}
#endif

void (*game_turn_test_hook)(void) = NULL;
TbBool game_core_mission_end_request = false;

TbBool game_core_turn(void)
{
    struct Packet *p_pckt;

    if (exit_game)
        return false;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return false;

    navi2_unkn_counter -= 2;
    if (navi2_unkn_counter < 0)
        navi2_unkn_counter = 0;
    if (navi2_unkn_counter > navi2_unkn_counter_max)
        navi2_unkn_counter_max = navi2_unkn_counter;
    update_tick_time();
    if ((ingame.Flags & TngF_ProgressAction) != 0)
        process_explode();

    // what showing the game screen changes in the game
    if (!skip_redraw_this_turn())
    {
        game_turn_camera();
        game_turn_world_view();
        // what drawing the panels changed: the item under the agent
        update_dropped_item_under_agent_exists(
          direct_control_thing_for_player(local_player_no));
        game_turn_view_end();
    }

    // the player's turn: commands of the front end instead of input
    p_pckt = &packets[local_player_no];
    if (!PacketRecord_IsPlayback())
    {
        TbBool did_pckt;

        did_pckt = game_turn_player_idle();
        if (!did_pckt)
            did_pckt = swcore_input_packet(p_pckt);
        ingame.MissionStatus = test_missions(0);
        if ((ingame.MissionStatus != ObvStatu_UNDECIDED) && !in_network_game)
        {
            // the player confirmed the conclusion (input_mission_concluded())
            if (game_core_mission_end_request)
                ingame.MissionEndFade = 1;
            game_core_mission_end_fade_start();
        }
        game_core_mission_end_request = false;
        if (game_core_mission_end_fade_turn())
            game_core_mission_over();
        if (!did_pckt || (players[local_player_no].DoubleMode != 0))
            game_turn_player_agents_idle();
    }
    if (game_turn_test_hook != NULL)
        game_turn_test_hook();

    // the world stops once the mission is over (mission_over())
    if ((ingame.DisplayMode == DpM_ENGINEPLY)
      || (ingame.DisplayMode == DpM_UNKN_1)
      || (ingame.DisplayMode == DpM_UNKN_3B))
        process_things();
    if (ingame.DisplayMode != DpM_PURPLEMNU)
        process_packets();
    game_process_orbital_station_explode();
    gameturn++;
    game_turn_statetrace();
    // The rain on the floor textures draws from the game's random numbers
    // every turn, so it is part of the game state
    scene_post_effect_prepare();
    return !exit_game && (ingame.DisplayMode == DpM_ENGINEPLY);
}

TbBool skip_redraw_this_turn(void)
{
    if ((ingame.Flags & GamF_Unkn0020) != 0)
    {
        return ((gameturn & 0xF) != 0);
    }
    return false;
}

/******************************************************************************/
