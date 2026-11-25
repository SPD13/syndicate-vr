/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file enginpeff.c
 *     Engine scene pre-/post-processing effects.
 * @par Purpose:
 *     Implement functions for adding effects while rendering a scene.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Oct 2023 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "enginpeff.h"

#include "bfgentab.h"
#include "bfmath.h"
#include "bfpixel.h"
#include "bfscreen.h"
#include "bfutility.h"

#include "enginbckt.h"
#include "engincam.h"
#include "engincolour.h"
#include "enginprops.h"
#include "engintrns.h"
#include "engintxtrmap.h"
#include "privrdlog.h"
/******************************************************************************/
const short waft_table2[] = {
  -28, -27, -25, -22, -18, -13, -7, 0,  7,  13,  18,  22,  25,  27,  28,  29,
   28,  27,       22,  18,  13,  7, 0, -7, -13, -18, -22, -25, -27, -28, -29, -28,
};

const short waft_table[] = {
  -28, -24, -21, -17, -14, -10, -7, -3, 0,  4,  7,  11,  14,  18,  21,  24,
   28,  24,  21,  18,  14,  11,  7,  4, 0, -3, -7, -10, -14, -17, -21, -24, -28,
};

ushort gamep_scene_effect_intensity = 1000;
short gamep_scene_effect_change = -1;
ushort gamep_scene_effect_type = ScEff_NONE;

ushort rain_drop_max_width = 0;
ushort snow_flake_max_size = 0;
ushort star_max_size = 0;

ushort word_1A7314;
ushort word_1A7330[1000];
ubyte byte_1A7B00[1000];
ubyte byte_1A7EE8[9004];

void scene_post_effect_rain_init(void)
{
    uint i;
    ushort idx3;

    for (i = 0; i < 999; i++)
    {
        idx3 = (i / 333) % 3;
        if (idx3 == 0)
        {
            uint rnd;
            ushort idx1, idx2;
            ubyte *tmap;

            rnd = (render_anim_turn >> (RENDER_ANIM_TURN_SHIFT + 2)) + LbRandomAnyShort();
            idx1 = (rnd >> 3) & 0x1FFF;
            if ((byte_1A7EE8[idx1] & (1 << (idx1 & 7))) == 0)
            {
                idx2 = rnd & 0xFFFF;
                word_1A7330[i] = idx2;
                tmap = vec_tmap[0];
                // the random numbers are drawn with no texture loaded too
                // (a front end without this renderer)
                byte_1A7B00[i] = (tmap != NULL) ? tmap[idx2] : 0;
                if (tmap != NULL)
                    tmap[idx2] = 63;
                byte_1A7EE8[idx1] |= (1 << (idx1 & 7));
            }
        }
    }
}

void water_droplets_on_floor(void)
{
    ushort i, k;

    for (i = 0; i < 999; i++)
    {
        ubyte *tmap;
        ushort n, step;

        step = (i / 333 + word_1A7314);
        switch (step % 3)
        {
        case 0:
            k = word_1A7330[i];
            tmap = vec_tmap[0];
            if ((k != 0) && (tmap != NULL)) {
                tmap[k] = byte_1A7B00[i];
            }
            n = k >> 3;
            byte_1A7EE8[n] &= ~(1 << (n & 7));
            k = (render_anim_turn >> (RENDER_ANIM_TURN_SHIFT + 2)) + LbRandomAnyShort();
            n = k >> 3;
            if (((1 << (n & 7)) & byte_1A7EE8[n]) == 0)
            {
                ubyte colA;

                word_1A7330[i] = k;
                // the game turns draw the random numbers with no texture
                // loaded too (a front end without this renderer)
                colA = (tmap != NULL) ? tmap[k] : 0;
                byte_1A7B00[i] = colA;
                if (tmap != NULL)
                    tmap[k] = pixmap.fade_table[0x3200 + colA];
                byte_1A7EE8[n] |= 1 << (n & 7);
            }
            break;
        case 1:
            k = word_1A7330[i];
            tmap = vec_tmap[0];
            if ((k != 0) && (tmap != NULL)) {
                tmap[k] = lbDisplay.GlassMap[256 * byte_1A7B00[i] + tmap[k]];
            }
            break;
        case 2:
            k = word_1A7330[i];
            tmap = vec_tmap[0];
            if ((k != 0) && (tmap != NULL)) {
                tmap[k] = lbDisplay.GlassMap[256 * byte_1A7B00[i] + tmap[k]];
            }
            break;
        }
    }
    word_1A7314++;
}

void scene_post_effect_texture_with_snow(void)
{
    int i;

    for (i = 0; i < 10; i++) {
        ushort pos;
        ubyte *ptr;
        pos = LbRandomAnyShort() + (render_anim_turn >> (RENDER_ANIM_TURN_SHIFT + 2));
        if (vec_tmap[0] == NULL)
            continue;
        ptr = vec_tmap[0] + pos;
        *ptr = pixmap.fade_table[40*PALETTE_8b_COLORS + *ptr];
    }
}

void scene_post_effect_prepare(void)
{
#if 0 // experimental function to alter effect intensity
    if ((LbRandomAnyShort() & 0xfff) > 0xf80)
        gamep_scene_effect_change = -gamep_scene_effect_change;
    gamep_scene_effect_intensity += gamep_scene_effect_change;
    if (gamep_scene_effect_intensity < 250)
        gamep_scene_effect_intensity = 250;
    else if (gamep_scene_effect_intensity > 2000)
        gamep_scene_effect_intensity = 2000;
#endif
    switch (gamep_scene_effect_type)
    {
    case ScEff_RAIN:
        water_droplets_on_floor();
        break;
    case ScEff_SNOW:
        scene_post_effect_texture_with_snow();
        break;
    }
}

/******************************************************************************/
