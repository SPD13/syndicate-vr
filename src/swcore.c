/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swcore.c
 *     The part of the game library interface that belongs to the game core.
 * @par Purpose:
 *     Fixes of original bugs, the game turn and the state hash. Starting
 *     and running the whole application (options, screens, menus) is in
 *     swapp.c, which belongs to the desktop front end.
 * @par Comment:
 *     None.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swcore.h"

#include "bflog.h"
#include "bfscreen.h"

#include "campaign.h"
#include "game.h"
#include "game_coreinit.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "game_turn.h"
#include "people.h"
#include "player.h"
#include "swlog.h"
/******************************************************************************/

int swcore_core_init(uint32_t campaign, uint32_t mission)
{
    // as swcore_init() (swapp.c) before reading the options
    if (LbErrorLogSetup(NULL, NULL, Lb_ERROR_LOG_NEW) != Lb_SUCCESS)
        return 1;
    ingame.GameMode = GamM_None;
    ingame.LowerMemoryUse = 0;
    ingame.Flags = 0;
    // all game data is in the data folder (the desktop reads where each
    // kind is from the install config, config.ini)
    {
        int i;
        for (i = 0; i < DirPlace_None; i++)
            game_dirs[i].use_cd = 0;
    }
    setup_file_names();
    // as the option "-m campaign,mission"
    is_single_game = 1;
    background_type = (ubyte)campaign;
    ingame.GameMode = GamM_Unkn2;
    ingame.Flags |= GamF_Unkn0008;
    ingame.CurrentMission = (ushort)mission;
    ingame.UserFlags |= UsrF_Cheats;
    LOGSYNC_F("Game core alone, campaign %u mission %u; fixes of original bugs: 0x%x",
      (unsigned)campaign, (unsigned)mission, (unsigned)swcore_get_fixes());
    adjust_memory_use();
    setup_language_file_names();
    // the default user settings of the game core (the desktop's are in
    // set_default_user_settings())
    set_default_player_control();
    ingame.PanelPermutation = -2;
    ingame.TrenchcoatPreference = OPT_TRENCHCOAT_PREF_MIN;
    ingame.DetailLevel = 1;
    ingame.UseMultiMedia = 0;

    if (!game_core_setup())
        return 1;
    game_core_mission_load();
    game_core_mission_init();
    // the game's view: as the desktop's in its game screen mode, 640x480
    // (a front end that draws its own view still plays the game of that)
    lbDisplay.GraphicsScreenWidth = 640;
    lbDisplay.GraphicsScreenHeight = 480;
    game_core_view_setup();
    return 0;
}

void swcore_core_shutdown(void)
{
    game_core_free_data();
    LbErrorLogReset();
}

void swcore_set_fixes(uint32_t fixes)
{
    game_fix_los_face_read = ((fixes & SWCORE_FIX_LOS_FACE_READ) != 0);
}

uint32_t swcore_get_fixes(void)
{
    uint32_t fixes = 0;

    if (game_fix_los_face_read)
        fixes |= SWCORE_FIX_LOS_FACE_READ;
    return fixes;
}

int swcore_core_step(void)
{
    return game_core_turn() ? 1 : 0;
}

uint32_t swcore_game_turn(void)
{
    return (uint32_t)gameturn;
}

uint32_t swcore_state_hash(void)
{
    return game_state_hash();
}

void swcore_mission_conclude(void)
{
    game_core_mission_end_request = true;
}

/******************************************************************************/
