/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file app_sprite.h
 *     Header file for app_spr_init.c, ap_gspr_map.c, ap_spr_smap.c.
 * @par Purpose:
 *     Unknown.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef APP_SPRITE_H_
#define APP_SPRITE_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(1)

struct TbSprite;

#pragma pack()

extern s32 low_trans_grey_brightness;
extern ubyte low_trans_grey_pal_bright[];
extern ubyte low_trans_grey_bright_limit[];

/** Init a table storing brightness of each colour in the palette.
 */
void LowTransGrey_InitPaletteBright(void);

/** Init a table used to limit colour value to 6-bit area.
 */
void LowTransGrey_InitBrightLimitTable(void);

/** Sprite drawing routine with modified transparency support.
 * Instead of standard transparency, it converts input and background to
 * grayscale and recolors using given color map.
 */
TbResult ApSpriteDrawLowTransGreyRemap(s32 x, s32 y,
  const struct TbSprite *spr, const ubyte *transmap);

/** Scaled sprite drawing routine with modified transparency support.
 * Instead of standard transparency, it converts input and background to
 * grayscale and recolors using given color map.
 */
TbResult ApSpriteDrawScaledLowTransGreyRemap(s32 xpos, s32 ypos,
  const struct TbSprite *sprite, s32 dest_width, s32 dest_height,
  const ubyte *transmap);

static inline TbPixel LbBlendPixelLowTrans4Remap(const TbPixel *transmap,
  ubyte sh1, ubyte sh2, TbPixel col1, TbPixel col2)
{
    uint pxmap;
    short k0, k1;
    ubyte brig;

    k0 = (low_trans_grey_pal_bright[col1] >> sh1);
    k1 = (low_trans_grey_pal_bright[col2] >> sh2);
    brig = low_trans_grey_brightness + k0 + k1;
    brig = low_trans_grey_bright_limit[brig];
    pxmap = (brig << 8) | col1;
    return transmap[pxmap];
}

static inline TbPixel LbBlendPixelLowTrans8Remap(const TbPixel *transmap,
  ubyte sh1, ubyte sh2, TbPixel col1, TbPixel col2)
{
    uint pxmap;
    short k0, k1;
    ubyte brig;

    k0 = (low_trans_grey_pal_bright[col1] >> sh1);
    k1 = (low_trans_grey_pal_bright[col2] >> sh2);
    brig = low_trans_grey_brightness + k0 + k1;
    brig = low_trans_grey_bright_limit[brig];
    pxmap = brig | (col1 << 8);
    return transmap[pxmap];
}

#ifdef __cplusplus
};
#endif

#endif // APP_SPRITE_H_
/******************************************************************************/
