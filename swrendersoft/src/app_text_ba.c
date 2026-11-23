/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_text_ba.c
 *     Functions for drawing text on graphical screen, per-application mod.
 * @par Purpose:
 *     Allows drawing brightness adjusted text.
 * @par Comment:
 *     This is a modification of `gtext.c` from bflibrary.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 02 Oct 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "bftext.h"

#include "app_text_ba.h"

#include "bfconfig.h"
#include "bfgentab.h"
#include "bfsprite.h"
#include "bfscreen.h"
#include "bffont.h"
#include "bfmath.h"
#include "bfmemory.h"
#include "bfanywnd.h"

#include "engincolour.h"
#include "privrdlog.h"
/******************************************************************************/

#if defined(LB_ENABLE_SHADOW_COLOUR)
#  define SHADOW_COLOUR lbDisplay.ShadowColour
#else
#  define SHADOW_COLOUR 0x00
#endif

TbBool LbIApplyControlCharToDrawSettings(const char **c);
TbBool LbIAlignMethodSet(ushort fdflags);
TbBool is_wide_charcode(u32 chr);

/** @internal
 * Puts brightness adjusted text sprites on screen.
 * @param sbuf
 * @param ebuf
 * @param x
 * @param y
 * @param space_len
 */
void put_down_brigadjtext_sprites(const char *sbuf, const char *ebuf,
  s32 x, s32 y, s32 space_len, short brig)
{
  const char *c;
  const struct TbSprite *p_spr;
  ubyte chr;
  s32 w,h;

  if (brig > PALETTE_FADE_LEVELS-1)
      brig = PALETTE_FADE_LEVELS-1;
  if (brig < 0)
      brig = 0;

  for (c=sbuf; c < ebuf; c++)
  {
    TbPixel colour, shadcol;

    colour = lbDisplay.DrawColour;
    colour = pixmap.fade_table[brig * PALETTE_8b_COLORS + colour];
    shadcol = SHADOW_COLOUR;
    chr = (ubyte)(*c);
    if (chr > 32)
    {
      p_spr = LbFontCharSprite(lbFontPtr, chr);
      if (p_spr != NULL)
      {
        // Draw shadow
        LbSpriteDrawOneColour(x + 1, y + 1, p_spr, shadcol);
        if ((lbDisplay.DrawFlags & Lb_TEXT_ONE_COLOR) != 0)
          LbSpriteDrawOneColour(x, y, p_spr, colour);
        else
          LbSpriteDraw(x, y, p_spr);
        w = p_spr->SWidth;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight();
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
      }
    } else
    if (chr == ' ')
    {
        w = space_len;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight();
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
    } else
    if (chr == '\t')
    {
        w = space_len*(s32)lbSpacesPerTab;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight();
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
    } else
    {
        LbIApplyControlCharToDrawSettings(&c);
    }
  }
}

/** @internal
 * Puts scaled brightness adjusted text sprites on screen.
 * @param sbuf
 * @param ebuf
 * @param x
 * @param y
 * @param space_len
 * @param units_per_px
 */
void put_down_brigadjtext_sprites_resized(const char *sbuf, const char *ebuf,
  s32 x, s32 y, s32 space_len, int units_per_px, short brig)
{
  const char *c;
  const struct TbSprite *p_spr;
  ubyte chr;
  s32 w,h;

  if (brig > PALETTE_FADE_LEVELS-1)
      brig = PALETTE_FADE_LEVELS-1;
  if (brig < 0)
      brig = 0;

  for (c=sbuf; c < ebuf; c++)
  {
    TbPixel colour, shadcol;

    colour = lbDisplay.DrawColour;
    colour = pixmap.fade_table[brig * PALETTE_8b_COLORS + colour];
    shadcol = SHADOW_COLOUR;
    chr = (ubyte)(*c);
    if (chr > 32)
    {
      p_spr = LbFontCharSprite(lbFontPtr,chr);
      if (p_spr != NULL)
      {
        // Draw shadow
        LbSpriteDrawResizedOneColour(x + units_per_px/12, y + units_per_px/12, units_per_px, p_spr, shadcol);
        if ((lbDisplay.DrawFlags & Lb_TEXT_ONE_COLOR) != 0) {
            LbSpriteDrawResizedOneColour(x, y, units_per_px, p_spr, colour);
        } else {
            LbSpriteDrawResized(x, y, units_per_px, p_spr);
        }
        w = p_spr->SWidth * units_per_px / 16;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight() * units_per_px / 16;
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
      }
    } else
    if (chr == ' ')
    {
        w = space_len;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight() * units_per_px / 16;
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
    } else
    if (chr == '\t')
    {
        w = space_len*(s32)lbSpacesPerTab;
        if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLINE) != 0)
        {
            h = LbTextLineHeight() * units_per_px / 16;
            LbDrawCharUnderline(x, y, w, h, colour, shadcol);
        }
        x += w;
    } else
    {
        LbIApplyControlCharToDrawSettings(&c);
    }
  }
}

void put_down_ba_sprites(const char *sbuf, const char *ebuf,
  s32 x, s32 y, s32 space_len, int units_per_px, short brig)
{
    if (units_per_px == 16)
    {
        put_down_brigadjtext_sprites(sbuf, ebuf, x, y, space_len, brig);
    } else
    {
        put_down_brigadjtext_sprites_resized(sbuf, ebuf, x, y, space_len, units_per_px, brig);
    }
}

TbBool AppTextDrawLineBrigAdjWthPartsResized(int posx, int posy,
  int units_per_px, short brig, const char *text)
{
    struct TbAnyWindow grwnd;
    const char *text_end;
    int len;

    if ((lbFontPtr == NULL) || (text == NULL))
        return true;
    LbScreenStoreGraphicsWindow(&grwnd);
    LbScreenLoadGraphicsWindow(&lbTextClipWindow);

    text_end = text + strlen(text);
    len = LbTextCharWidth(' ') * units_per_px / 16;
    put_down_ba_sprites(text, text_end, posx, posy, len, units_per_px, brig);
    LbScreenLoadGraphicsWindow(&grwnd);
    return true;
}

TbBool AppTextDrawLineBrigAdjWthParts(int posx, int posy,
  short brig, const char *text)
{
    // Using resized version - it will end up with version optimized for no resize anyway
    return AppTextDrawLineBrigAdjWthPartsResized(posx, posy, 16, brig, text);
}

/******************************************************************************/
