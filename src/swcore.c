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

#include "game.h"
#include "game_speed.h"
#include "people.h"
/******************************************************************************/

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

uint32_t swcore_game_turn(void)
{
    return (uint32_t)gameturn;
}

uint32_t swcore_state_hash(void)
{
    return game_state_hash();
}

/******************************************************************************/
