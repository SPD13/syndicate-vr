/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_turn.h
 *     Header file for game_turn.c.
 * @par Purpose:
 *     One game turn of a mission, in the game core.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef GAME_TURN_H
#define GAME_TURN_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Whether this turn skips showing the game (and what showing it changes). */
TbBool skip_redraw_this_turn(void);

void game_turn_camera(void);
void game_turn_world_view(void);
void game_turn_view_end(void);

/** Called by game_core_turn() after the player's turn and before the things
 * are processed, where the desktop turn calls the test hooks; NULL for none.
 * Set by the front end (swapp.c sets difftest_bot_turn), so the core does
 * not depend on the test code.
 */
extern void (*game_turn_test_hook)(void);

/** Set by the front end when the player confirmed the conclusion of a
 * decided mission; the next turn ends it (or starts its fade-out).
 */
extern TbBool game_core_mission_end_request;

/** With $SWARS_STATETRACE set (test builds), writes the state hash and the
 * random seed of the turn to that file; $SWARS_STATETRACE_DUMP=N also dumps
 * the things after turn N (statetrace.sh, trace_compare.py).
 */
void game_turn_statetrace(void);

/** One game turn of a mission without the front end's input and drawing.
 * Returns false once the game asked to exit, or when no mission is played.
 */
TbBool game_core_turn(void);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
