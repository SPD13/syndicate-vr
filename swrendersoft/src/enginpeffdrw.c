/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file enginpeffdrw.c
 *     Drawing the scene effects: falling rain and snow, stars.
 * @par Purpose:
 *     Draws the effects of enginpeff.c (world state: the effect, its sizes, the rain on the floor).
 * @par Comment:
 *     Split from enginpeff.c.
 * @author   Tomasz Lis, SyndicateVR
 * @date     24 Nov 2026 - 25 Nov 2026
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

/** Applies one of the effect size limits, a limit of zero meaning none. */
static ushort size_limited(ushort size, ushort limit)
{
    if ((limit != 0) && (size > limit))
        return limit;
    return size;
}

static void draw_droplet(TbPixel *o, short scanln, short w, short h, TbPixel *coltb)
{
    short x, y;

    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++) {
            o[y * scanln + x] = coltb[o[y * scanln + x]];
        }
    }
}

void draw_falling_rain(int bckt)
{
    u32 icol;
    short limit_y;
    ushort rnd, m;
    u32 shift_y;
    TbPixel *o;
    TbPixel *coltb;
    u32 seed_bkp;
    short x, y;
    short scanln, w, h;

    seed_bkp = lbSeed;
    scanln = lbDisplay.GraphicsScreenWidth;

    icol = (BUCKETS_COUNT - bckt) / 416 << 7;
    shift_y = (render_anim_turn >> RENDER_ANIM_TURN_SHIFT) * (BUCKETS_COUNT - bckt);
    limit_y = 236 - (bckt >> 5);
    if (limit_y < 20)
        return;

    m = lbDisplay.GraphicsScreenHeight / 200;
    if (m == 0) m++;

    lbSeed = bckt;
    rnd = LbRandomPosShort();
    x = (rnd + (engn_xc >> 4) + (engn_cam_yaw >> 7)) % scanln;
    rnd = LbRandomPosShort();
    y = m * ((rnd + (shift_y >> 10)) % limit_y);
    lbDisplay.DrawFlags = Lb_SPRITE_TRANSPAR4;
    o = &lbDisplay.WScreen[scanln * y + x];
    w = size_limited(m, rain_drop_max_width);
    h = m;
    if (bckt < 4000) h += m;
    if (bckt < 3000) h += m;
    if (bckt < 1000) h += m;
    coltb = &pixmap.ghost_table[256 * pixmap.fade_table[15*PALETTE_8b_COLORS + 63 + icol]];
    draw_droplet(o, scanln, w, h, coltb);

    lbDisplay.DrawFlags = 0;
    lbSeed = seed_bkp;
}

static void draw_static_dot(short x, short y, short w, short h, short ftpos)
{
    short dx, dy;

    for (dy = 0; dy < h; dy++)
    {
        for (dx = 0; dx < w; dx++) {
            LbDrawPixelClip(x + dx, y + dy, pixmap.fade_table[20*PALETTE_8b_COLORS + ftpos]);
        }
    }
}

void draw_falling_snow(int bckt)
{
    int height;
    uint seed_bkp;
    ushort m, scanln;

    scanln = lbDisplay.GraphicsScreenWidth;
    m = lbDisplay.GraphicsScreenHeight / 200;
    if (m == 0) m++;
    height = 240 - (bckt >> 5);
    seed_bkp = lbSeed;
    if (height >= 20)
    {
        int shift1;
        uint shift2;
        uint x, y;
        ushort speed;
        ushort angXZs, angXZc;
        ushort dm;

        angXZs = ((engn_cam_yaw >> 5)) & 0x7FF;
        angXZc = ((engn_cam_yaw >> 5) + LbFPMath_PI/2) & 0x7FF;

        lbSeed = bckt;
        speed = (bckt >> 5) & 0x3;
        shift1 = waft_table[(bckt + (render_anim_turn >> RENDER_ANIM_TURN_SHIFT)) & 0x1F];
        // Moving with full background speed would be >> 19, dividing by half to make rotation look beter
        x = (shift1 >> (speed + 1)) + ((engn_zc * lbSinTable[angXZs]) >> 20) - ((engn_xc * lbSinTable[angXZc]) >> 20) + LbRandomAnyShort();
        shift2 = (BUCKETS_COUNT - bckt) * (render_anim_turn >> RENDER_ANIM_TURN_SHIFT);
        y = (shift2 >> (12 - speed/2)) + ((engn_xc * lbSinTable[angXZs]) >> 20) + ((engn_zc * lbSinTable[angXZc]) >> 20) + LbRandomAnyShort();
        lbDisplay.DrawFlags = Lb_SPRITE_TRANSPAR4;
        dm = size_limited(m, snow_flake_max_size);
        draw_static_dot((x * m) % scanln, (y % height) * m, dm, dm, 128 * ((BUCKETS_COUNT - bckt) / 416) + colour_lookup[ColLU_WHITE]);
        lbSeed = seed_bkp;
        lbDisplay.DrawFlags = 0;
    }
}

void scene_post_effect_for_bucket(short bckt)
{
    uint every;

    switch (gamep_scene_effect_type)
    {
    case ScEff_RAIN:
        every = 8 * 1000 / gamep_scene_effect_intensity;
        if ((bckt % every) == 0) {
            draw_falling_rain(bckt);
        }
        break;
    case ScEff_SNOW:
        every = 8 * 1000 / gamep_scene_effect_intensity;
        if ((bckt % every) == 0) {
            draw_falling_snow(bckt);
        }
        break;
    default:
        break;
    }
}

static void draw_distant_stars(short x, short y, short w, short h, TbPixel color)
{
    short dx, dy;

    for (dy = 0; dy < h; dy++)
    {
        for (dx = 0; dx < w; dx++) {
            LbDrawPixelClip(x + dx, y + dy, color);
        }
    }
}

void draw_background_stars(void)
{
    u32 seed_bkp;
    int i, limit;
    int scr_x0, scr_y0;
    ushort m, dm;

    m = lbDisplay.GraphicsScreenHeight / 300;
    if (m == 0) m++;
    dm = size_limited(m, star_max_size);
    scr_x0 = lbDisplay.GraphicsScreenWidth / 2;
    scr_y0 = lbDisplay.GraphicsScreenHeight / 2;

    seed_bkp = lbSeed;
    lbSeed = 0x8D747;
    limit = 1223 * gamep_scene_effect_intensity / 1000;
    for (i = 0; i < limit; i++)
    {
        int gt, plane, speed;
        int simp_x, simp_y;
        int scr_x, scr_y;

        gt = (render_anim_turn >> RENDER_ANIM_TURN_SHIFT) & 0x7FF;
        plane = (i >> 4) + 1;
#if 0 // some testing code which remained for no reason, remove later
        if (lbShift == KMod_SHIFT)
            speed = 32 * gt;
        else
#endif
        speed = plane * gt;
        simp_x = (LbRandomAnyShort() - (speed >> 6)) % 800 - 400;
        simp_y = LbRandomAnyShort() % 800 - 400;

        scr_x = scr_x0 + ((simp_x * m * transf_vec_yaw_z - simp_y * m * transf_vec_yaw_x) >> 16);
        scr_y = scr_y0 - ((simp_x * m * transf_vec_yaw_x + simp_y * m * transf_vec_yaw_z) >> 16);

        draw_distant_stars(scr_x, scr_y, dm, dm, 79 - (plane >> 1));
    }
    lbSeed = seed_bkp;
}

/******************************************************************************/
