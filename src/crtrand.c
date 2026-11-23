/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file crtrand.c
 *     The C runtime random generator the original code called, with own state.
 * @par Purpose:
 *     Gives the same random draws on every target and C library.
 * @par Comment:
 *     The algorithm and initial state of rand() in the Microsoft C runtime,
 *     which the verified 32-bit builds used.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "crtrand.h"

/******************************************************************************/
u32 game_rand_state = 1;

void game_srand(u32 seed)
{
    game_rand_state = seed;
}

int game_rand(void)
{
    game_rand_state = game_rand_state * 214013u + 2531011u;
    return (int)((game_rand_state >> 16) & GAME_RAND_MAX);
}

/******************************************************************************/
