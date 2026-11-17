/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_hook_people.c
 *     Difftest hooks reaching people and thing processing.
 * @par Purpose:
 *     Calls functions no mission reaches by itself, so the difftest can
 *     compare their C remake against the asm. Active only in scenarios
 *     whose suffix names the hook (see difftest_suffix_has()).
 * @par Comment:
 *     Called every game turn from difftest_bot_turn().
 * @author   SyndicateVR decomp agent
 * @date     16 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

/******************************************************************************/

#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

void difftest_hook_people_turn(void)
{
}

#else

void difftest_hook_people_turn(void)
{
}

#endif
/******************************************************************************/
