/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swmain.c
 *     Entry point of the syndwarsfx executable.
 * @par Purpose:
 *     The SDL desktop front end: runs the game library turn by turn.
 * @par Comment:
 *     All of the game is in libswarscore, see swcore.h.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swcore.h"

int main(int argc, char **argv)
{
    if (swcore_init(argc, argv) != 0)
        return 1;
    while (swcore_step())
        ;
    swcore_shutdown();
    return 0;
}
/******************************************************************************/
