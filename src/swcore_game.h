/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swcore_game.h
 *     Header file for swcore_world.c, the parts the game itself calls.
 * @par Purpose:
 *     Hooks of the game library in the game loop.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SWCORE_GAME_H
#define SWCORE_GAME_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct Packet;

/** Builds the local player's packet from the next queued front end
 * command (swcore_command()), if any; called from load_packet() in the
 * turns the player gave no input of their own. Returns whether a packet
 * was built.
 */
TbBool swcore_input_packet(struct Packet *p_pckt);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
/******************************************************************************/
