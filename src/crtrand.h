/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file crtrand.h
 *     Header file for crtrand.c.
 * @par Purpose:
 *     The C runtime random generator the original code called, with own state.
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
#ifndef CRTRAND_H
#define CRTRAND_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Largest value game_rand() returns. */
#define GAME_RAND_MAX 0x7FFF

/** State of game_rand(); the test harness saves and restores it. */
extern u32 game_rand_state;

/** Sets the seed of game_rand(), like srand(). */
void game_srand(u32 seed);

/** Next value in 0..GAME_RAND_MAX, like rand() of the Microsoft C runtime.
 *
 * The original code called the C library rand(). Its sequence and range
 * differ between C libraries (glibc: RAND_MAX 0x7FFFFFFF), so the game
 * uses this one, which gives the same draws on every target.
 */
int game_rand(void);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
/******************************************************************************/
