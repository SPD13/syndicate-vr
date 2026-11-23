/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file sprfontut.c
 *     Sprite font utilities.
 * @par Purpose:
 *     Implement utility functions for handling sprite fonts.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Apr 2022 - 19 Dec 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "sprfontut.h"

#include "bfsprite.h"

/******************************************************************************/

struct TbSprite *AppFontCharSpriteRW(struct TbSprite *font,
  const u32 chr)
{
    if (font == NULL)
        return NULL;
    if ((chr >= 31) && (chr < 256))
        return &font[(chr-31)];
    return NULL;
}

ushort FontSpacingAlter(struct TbSprite *font, int units_per_px)
{
    struct TbSprite *p_spr;
    ushort space_bkp;

    p_spr = AppFontCharSpriteRW(font, ' ');
    if (p_spr == NULL)
        return 0;
    space_bkp = p_spr->SWidth;
    p_spr->SWidth = (space_bkp * units_per_px) / 16;
    return space_bkp;
}

void FontSpacingRestore(struct TbSprite *font, ushort space_bkp)
{
    struct TbSprite *p_spr;

    p_spr = AppFontCharSpriteRW(font, ' ');
    if (p_spr == NULL)
        return;
    p_spr->SWidth = space_bkp;
}

/******************************************************************************/
