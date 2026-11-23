/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file sprfontut.h
 *     Header file for sprfontut.c.
 * @par Purpose:
 *     Sprite font utilities.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     19 Apr 2022 - 19 Dec 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SPRFONTUT_H
#define SPRFONTUT_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct TbSprite;

#pragma pack()
/******************************************************************************/

/** Modifies spacing of given font, by altering width of space character.
 */
ushort FontSpacingAlter(struct TbSprite *font, int units_per_px);

void FontSpacingRestore(struct TbSprite *font, ushort space_bkp);

/** Altered version of LbFontCharSprite() which returns non-const reference.
 */
struct TbSprite *AppFontCharSpriteRW(struct TbSprite *font,
  const u32 chr);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
