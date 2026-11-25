/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vrspectate.h
 *     Header file for vrspectate.c.
 * @par Purpose:
 *     Publishing the game world for the external VR spectator viewer.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   SyndicateVR
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef VRSPECTATE_H
#define VRSPECTATE_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Enables publishing the world through shared memory.
 * Also enabled by environment variable SWARS_VR_SPECTATOR=1.
 */
void vr_spectator_enable(TbBool enable);

/** Publishes the current world state; to be called once per game turn.
 */
void vr_spectator_update(void);
/** Builds the local player packet from the next VR viewer command, if any. */
TbBool vr_spectator_input(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
