/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_ovmous.c
 *     Support for checking if thing sprite overlaps with (is under) mouse.
 * @par Purpose:
 *     Check rendering bounds of a sprite vs mouse position.
 *     Use as callbacks during rendering, so that sprite position and scale
 *     calculated there can be reused.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     09 Oct 2025 - 22 Feb 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_ovmous.h"

#include <assert.h>
#include "bfscreen.h"

#include "frame_sprani.h"
#include "engincam.h"
#include "engindrwlstx.h"

#include "guiboxes.h"
#include "hud_draw.h"
#include "player.h"
#include "swlog.h"
#include "thing.h"

/******************************************************************************/

/******************************************************************************/

ubyte check_mouse_overlap(ushort sspr)
{
    struct ScreenBoxBase box;
    struct SortSprite *p_sspr;
    struct Frame *p_frm;

    p_sspr = &game_sort_sprites[sspr];
    box.X = p_sspr->X + ((anim_el_pos_min_x * overall_scale) >> 8);
    box.Y = p_sspr->Y + ((anim_el_pos_min_y * overall_scale) >> 8);

    p_frm = &frame[p_sspr->Frame];
    box.Width = (p_frm->SWidth * overall_scale) >> 9;
    box.Height = (p_frm->SHeight * overall_scale) >> 9;

    if (box.Width < 16)
    {
        box.X -= ((16 + 1 - box.Width) >> 1);
        box.Width = 16;
    }
    if (box.Height < 20) {
        box.Y -= ((20 + 1 - box.Height) >> 1);
        box.Height = 20;
    }

    if (in_box(lbDisplay.MMouseX, lbDisplay.MMouseY, box.X, box.Y, box.Width, box.Height))
    {
        PlayerInfo *p_locplayer;
        struct Thing *p_thing;
        p_thing = (struct Thing *)p_sspr->SrcItem;
        p_locplayer = &players[local_player_no];
        p_locplayer->Target = p_thing->ThingOffset;
        p_locplayer->TargetType = TrgTp_Unkn7;
        return 1;
    }
    return 0;
}

ubyte check_mouse_overlap_item(ushort sspr)
{
    struct ScreenBoxBase box;
    struct SortSprite *p_sspr;
    struct Frame *p_frm;
    PlayerInfo *p_locplayer;

    p_sspr = &game_sort_sprites[sspr];
    box.X = p_sspr->X + ((anim_el_pos_min_x * overall_scale) >> 8);
    box.Y = p_sspr->Y + ((anim_el_pos_min_y * overall_scale) >> 8);

    p_frm = &frame[p_sspr->Frame];
    box.Width = (p_frm->SWidth * overall_scale) >> 9;
    box.Height = (p_frm->SHeight * overall_scale) >> 9;

    p_locplayer = &players[local_player_no];
    if (p_locplayer->TargetType == TrgTp_DroppedTng)
    {
        struct Thing *p_thing;
        ushort VX;
        p_thing = (struct Thing *)p_sspr->SrcItem;
        VX = p_thing->VX;
        if ( VX )
        {
            if (VX < 12 || VX > 13)
                return 0;
            box.X -= (box.Width >> 1);
            box.Y -= (box.Height >> 1);
            box.Width *= 2;
            box.Height *= 2;
        }
    }
    if (in_box(lbDisplay.MMouseX, lbDisplay.MMouseY, box.X, box.Y, box.Width, box.Height))
    {
        struct Thing *p_thing;
        p_thing = (struct Thing *)p_sspr->SrcItem;
        p_locplayer->Target = p_thing->ThingOffset;
        p_locplayer->TargetType = TrgTp_DroppedTng;
        return 1;
    }
    return 0;
}

ubyte check_mouse_overlap_corpse(ushort sspr)
{
    struct ScreenBoxBase box;
    struct SortSprite *p_sspr;
    struct Frame *p_frm;
    PlayerInfo *p_locplayer;

    p_sspr = &game_sort_sprites[sspr];
    box.X = p_sspr->X + ((anim_el_pos_min_x * overall_scale) >> 8);
    box.Y = p_sspr->Y + ((anim_el_pos_min_y * overall_scale) >> 8);

    p_frm = &frame[p_sspr->Frame];
    box.Width = (p_frm->SWidth * overall_scale) >> 9;
    box.Height = (p_frm->SHeight * overall_scale) >> 9;

    p_locplayer = &players[local_player_no];
    if (box.Width < 16)
    {
        box.X -= ((17 - box.Width) >> 1);
        box.Width = 16;
    }
    if (box.Height < 20) {
        box.Y -= ((21 - box.Height) >> 1);
        box.Height = 20;
    }

    if (in_box(lbDisplay.MMouseX, lbDisplay.MMouseY, box.X, box.Y, box.Width, box.Height))
    {
        struct Thing *p_thing;
        p_thing = (struct Thing *)p_sspr->SrcItem;
        p_locplayer->Target = p_thing->ThingOffset;
        p_locplayer->TargetType = TrgTp_Unkn1;
        return 1;
    }
    return 0;
}

ubyte check_mouse_over_netgame_enemy(ushort sspr, struct Thing *p_thing)
{
    struct ScreenBoxBase box;
    struct SortSprite *p_sspr;
    struct Frame *p_frm;

    p_sspr = &game_sort_sprites[sspr];
    box.X = p_sspr->X + ((anim_el_pos_min_x * overall_scale) >> 8);
    box.Y = p_sspr->Y + ((anim_el_pos_min_y * overall_scale) >> 8);

    p_frm = &frame[p_sspr->Frame];
    box.Width = (p_frm->SWidth * overall_scale) >> 9;
    box.Height = (p_frm->SHeight * overall_scale) >> 9;

    if (box.Width < 16)
    {
        box.X -= ((17 - box.Width) >> 1);
        box.Width = 16;
    }
    if (box.Height < 20) {
        box.Y -= ((21 - box.Height) >> 1);
        box.Height = 20;
    }

    if (in_box(lbDisplay.MMouseX, lbDisplay.MMouseY, box.X, box.Y, box.Width, box.Height))
    {
        set_netplayer_name_text_over_thing(box.X + (box.Height >> 1), box.Y - 8, p_thing);
        return 1;
    }
    return 0;
}

/******************************************************************************/
