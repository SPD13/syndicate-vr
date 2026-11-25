/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file engindrwbuf.c
 *     The renderer's buffers and the state the game sets for it.
 * @par Purpose:
 *     Defines what the game core sets up or writes for the renderer: the buffers the game allocates
 *     with its world (game_data.c), the lights option, the light of a nuclear explosion.
 * @par Comment:
 *     In libswworld, so that the game core links without the drawing code.
 * @author   Tomasz Lis, SyndicateVR
 * @date     24 Nov 2026 - 25 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "engindrwlstx.h"

#include "bftypes.h"
#include "enginbckt.h"
#include "enginshadws.h"
/******************************************************************************/

/* the buffers, in the game's memory */
struct SpecialPoint *game_screen_point_pool = NULL;
struct SingleObjectFace3 *game_special_obj_faces3 = NULL;
struct SingleObjectFace4 *game_special_obj_faces4 = NULL;
struct SortLine *game_sort_lines = NULL;
struct DrawItem *game_draw_list = NULL;
struct SortSprite *game_sort_sprites = NULL;

/* the renderer's state the game sets */
ubyte engine_render_lights = 0;
s32 engn_nuclear_shade_timer = 0;
s32 engn_nuclear_shade_x = 0;
s32 engn_nuclear_shade_y = 0;
s32 engn_nuclear_shade_z = 0;

/******************************************************************************/

void set_nuclear_shade_point(s32 x, s32 y, s32 z)
{
    engn_nuclear_shade_x = x;
    engn_nuclear_shade_y = y;
    engn_nuclear_shade_z = z;
    engn_nuclear_shade_timer = 0;
}

void set_nuclear_shade_timer(u32 tmval)
{
    engn_nuclear_shade_timer = tmval;
}

/******************************************************************************/
