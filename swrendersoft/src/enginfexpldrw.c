/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file enginfexpldrw.c
 *     Drawing the faces thrown by explosions.
 * @par Purpose:
 *     Enlists the explode faces (enginfexpl.c, world data) for drawing.
 * @par Comment:
 *     Split from enginfexpl.c: the faces are world state, their drawing is the renderer's.
 * @author   Tomasz Lis, SyndicateVR
 * @date     24 Nov 2026 - 25 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "enginfexpl.h"

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include "bfmemut.h"
#include "bfutility.h"

#include "enginbckt.h"
#include "engincam.h"
#include "engindrwlstx.h"
#include "enginprops.h"
#include "enginshrapn.h"
#include "engintrns.h"
#include "frame_sprani.h"
#include "privrdlog.h"
/******************************************************************************/

void enlist_draw_explode_type1(ushort exface, ushort npoints)
{
    struct ShEnginePoint sp1, sp2, sp3, sp4;
    struct ExplodeFace *p_exface;
    struct SpecialPoint *p_specpt;
    int cor_dx, cor_dy, cor_dz;
    ushort flags_all;
    short depth_max;
    ushort pt;

    p_exface = &ex_faces[exface];

    pt = next_screen_point;
    p_exface->PointOffset = pt;
    next_screen_point += npoints;

    cor_dx = p_exface->X + p_exface->X0 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z0 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y0 - engn_yc;
    transform_shpoint(&sp1, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 0];
    p_specpt->X = sp1.X;
    p_specpt->Y = sp1.Y;

    cor_dx = p_exface->X + p_exface->X1 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z1 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y1 - engn_yc;
    transform_shpoint(&sp2, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 1];
    p_specpt->X = sp2.X;
    p_specpt->Y = sp2.Y;

    cor_dx = p_exface->X + p_exface->X2 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z2 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y2 - engn_yc;
    transform_shpoint(&sp3, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 2];
    p_specpt->X = sp3.X;
    p_specpt->Y = sp3.Y;

    depth_max = SHRT_MIN;
    if (depth_max < sp1.Depth)
        depth_max = sp1.Depth;
    if (depth_max < sp2.Depth)
        depth_max = sp2.Depth;
    if (depth_max < sp3.Depth)
        depth_max = sp3.Depth;

    flags_all = sp3.Flags & sp2.Flags & sp1.Flags;

    if (npoints >= 4)
    {
        cor_dx = p_exface->X + p_exface->X3 - engn_xc;
        cor_dz = p_exface->Z + p_exface->Z3 - engn_zc;
        cor_dy = p_exface->Y + p_exface->Y3 - engn_yc;
        transform_shpoint(&sp4, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

        p_specpt = &game_screen_point_pool[pt + 3];
        p_specpt->X = sp4.X;
        p_specpt->Y = sp4.Y;

        flags_all &= sp4.Flags;
        if (depth_max < sp4.Depth)
            depth_max = sp4.Depth;
    }

    if ((flags_all & 0xF) != 0)
        return;

    stat_drawlist_faces++;
    draw_item_add(DrIT_Unkn5, exface, BUCKET_MID + depth_max);
}

void enlist_draw_explode_type3(ushort exface, ushort npoints)
{
    struct ShEnginePoint sp1, sp2, sp3, sp4;
    struct ExplodeFace *p_exface;
    struct SpecialPoint *p_specpt;
    int cor_dx, cor_dy, cor_dz;
    ushort flags_all;
    short depth_max;
    ushort pt;

    p_exface = &ex_faces[exface];

    pt = next_screen_point;
    p_exface->PointOffset = pt;
    next_screen_point += npoints;

    cor_dx = p_exface->X0 - engn_xc;
    cor_dz = p_exface->Z0 - engn_zc;
    cor_dy = p_exface->Y0 - engn_yc;
    transform_shpoint(&sp1, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 0];
    p_specpt->X = sp1.X;
    p_specpt->Y = sp1.Y;

    cor_dx = p_exface->X1 - engn_xc;
    cor_dz = p_exface->Z1 - engn_zc;
    cor_dy = p_exface->Y1 - engn_yc;
    transform_shpoint(&sp2, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 1];
    p_specpt->X = sp2.X;
    p_specpt->Y = sp2.Y;

    cor_dx = p_exface->X2 - engn_xc;
    cor_dz = p_exface->Z2 - engn_zc;
    cor_dy = p_exface->Y2 - engn_yc;
    transform_shpoint(&sp3, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 2];
    p_specpt->X = sp3.X;
    p_specpt->Y = sp3.Y;

    depth_max = SHRT_MIN;
    if (depth_max < sp1.Depth)
        depth_max = sp1.Depth;
    if (depth_max < sp2.Depth)
        depth_max = sp2.Depth;
    if (depth_max < sp3.Depth)
        depth_max = sp3.Depth;

    flags_all = sp3.Flags & sp2.Flags & sp1.Flags;

    if (npoints >= 4)
    {
        cor_dx = p_exface->X3 - engn_xc;
        cor_dz = p_exface->Z3 - engn_zc;
        cor_dy = p_exface->Y3 - engn_yc;
        transform_shpoint(&sp4, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

        p_specpt = &game_screen_point_pool[pt + 3];
        p_specpt->X = sp4.X;
        p_specpt->Y = sp4.Y;

        flags_all &= sp4.Flags;
        if (depth_max < sp4.Depth)
            depth_max = sp4.Depth;
    }

    if ((flags_all & 0xF) != 0)
        return;

    stat_drawlist_faces++;
    draw_item_add(DrIT_Unkn5, exface, BUCKET_MID + depth_max);
}

void enlist_draw_explode_type5(ushort exface, ushort npoints)
{
    struct ShEnginePoint sp1, sp2, sp3, sp4;
    struct ExplodeFace *p_exface;
    struct SpecialPoint *p_specpt;
    int cor_dx, cor_dy, cor_dz;
    ushort flags_all;
    short depth_max;
    ushort pt;

    p_exface = &ex_faces[exface];

    pt = next_screen_point;
    p_exface->PointOffset = pt;
    next_screen_point += npoints;

    cor_dx = p_exface->X + p_exface->X0 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z0 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y0 - engn_yc;
    transform_shpoint(&sp1, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 0];
    p_specpt->X = sp1.X;
    p_specpt->Y = sp1.Y;

    cor_dx = p_exface->X + p_exface->X1 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z1 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y1 - engn_yc;
    transform_shpoint(&sp2, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 1];
    p_specpt->X = sp2.X;
    p_specpt->Y = sp2.Y;

    cor_dx = p_exface->X + p_exface->X2 - engn_xc;
    cor_dz = p_exface->Z + p_exface->Z2 - engn_zc;
    cor_dy = p_exface->Y + p_exface->Y2 - engn_yc;
    transform_shpoint(&sp3, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

    p_specpt = &game_screen_point_pool[pt + 2];
    p_specpt->X = sp3.X;
    p_specpt->Y = sp3.Y;

    depth_max = SHRT_MIN;
    if (depth_max < sp1.Depth)
        depth_max = sp1.Depth;
    if (depth_max < sp2.Depth)
        depth_max = sp2.Depth;
    if (depth_max < sp3.Depth)
        depth_max = sp3.Depth;

    flags_all = sp3.Flags & sp2.Flags & sp1.Flags;

    if (npoints >= 4)
    {
        cor_dx = p_exface->X + p_exface->X3 - engn_xc;
        cor_dz = p_exface->Z + p_exface->Z3 - engn_zc;
        cor_dy = p_exface->Y + p_exface->Y3 - engn_yc;
        transform_shpoint(&sp4, cor_dx, cor_dy - 8 * engn_yc, cor_dz);

        p_specpt = &game_screen_point_pool[pt + 3];
        p_specpt->X = sp4.X;
        p_specpt->Y = sp4.Y;

        flags_all &= sp4.Flags;
        if (depth_max < sp4.Depth)
            depth_max = sp4.Depth;
    }

    if ((flags_all & 0xF) != 0)
        return;

    stat_drawlist_faces++;
    draw_item_add(DrIT_Unkn5, exface, BUCKET_MID + depth_max);
}

/******************************************************************************/
