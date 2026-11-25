/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_misend.h
 *     Header file for game_misend.c.
 * @par Purpose:
 *     The end of a mission: the fade-out and the campaign's outcome.
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
#ifndef GAME_MISEND_H
#define GAME_MISEND_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

extern const char *miss_end_sta_names[];

/** Once the mission is decided: starts the fade-out if the mission has
 * one (WaitToFade) and none runs yet. Without one, the mission ends when
 * the player confirms (MissionEndFade = 1).
 */
void game_core_mission_end_fade_start(void);

/** One turn of the fade-out. Returns true on the turn it ends, the turn the
 * mission is over (then a single mission game exits).
 */
TbBool game_core_mission_end_fade_turn(void);

/** The steps of mission_over(), with the front end's between them:
 * start (the agents' weapons hidden, the world stops), outcome (rewards,
 * mission state, missions opened; returns an OMiSta_* value), end (game
 * over or not).
 */
void game_core_mission_over_start(void);
ubyte game_core_mission_over_outcome(void);
void game_core_mission_over_end(void);

/** All steps of mission_over() for a front end without videos; returns the
 * outcome (OMiSta_*).
 */
ubyte game_core_mission_over(void);

void compound_mission_immediate_start_next(void);
ushort open_new_mission(short missi);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
