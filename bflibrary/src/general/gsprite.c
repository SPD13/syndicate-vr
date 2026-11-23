/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file gsprite.c
 *     Graphics sprites support library.
 * @par Purpose:
 *     Functions for reading/writing, decoding/encodeing of sprites.
 * @par Comment:
 *     Part of 8-bit graphics canvas drawing library.
 *     Used for drawing sprites on screen.
 * @author   Tomasz Lis
 * @date     12 Nov 2008 - 05 Nov 2021
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfsprite.h"

#include <stdint.h>
#include <string.h>

#include "privbflog.h"

ubyte * lbSpriteReMapPtr;

long LbSpriteTabExpand(void *buf, long file_len)
{
    ubyte *p_file = buf;
    struct TbSprite *p_spr = buf;
    long n, i;

    n = file_len / LB_SPRITE_FILE_SIZE;
    if (sizeof(struct TbSprite) == LB_SPRITE_FILE_SIZE)
        return file_len;
    // From the last entry, as the entries grow in place
    for (i = n - 1; i >= 0; i--)
    {
        const ubyte *rec = p_file + i * LB_SPRITE_FILE_SIZE;
        u32 offs;
        struct TbSprite spr;

        memcpy(&offs, rec, 4);
        memcpy(&spr.SWidth, rec + 4, sizeof(spr.SWidth));
        memcpy(&spr.SHeight, rec + 4 + sizeof(spr.SWidth), sizeof(spr.SHeight));
        spr.Data = (TbSpriteData)(uintptr_t)offs;
        memcpy(&p_spr[i], &spr, sizeof(spr));
    }
    return n * sizeof(struct TbSprite);
}

TbResult LbSpriteSetup(TbSprite *start, const TbSprite *end, const unsigned char *data)
{
    struct TbSprite *sprt;
    int n;

    n = 0;
    sprt = start;
    while (sprt < end)
    {
      if ((uintptr_t)sprt->Data < (uintptr_t)data)
      {
        sprt->Data = (TbSpriteData)data + (uintptr_t)sprt->Data;
        n++;
      }
      sprt++;
    }
    LOGDBG("initiated %d of %d sprites", n, (sprt-start));
    return Lb_SUCCESS;
}

TbResult LbSpriteSetupAll(TbSetupSprite t_setup[])
{
    struct TbSetupSprite *stp_sprite;
    int idx;
    idx=0;
    stp_sprite = &t_setup[idx];
    while (stp_sprite->Data != NULL)
    {
      if ((stp_sprite->Start != NULL) && (stp_sprite->End != NULL))
        LbSpriteSetup(*(stp_sprite->Start), *(stp_sprite->End), (unsigned char *)*(stp_sprite->Data));
      idx++;
      stp_sprite = &t_setup[idx];
    }
    LOGSYNC("initiated %d SetupSprite lists", idx);
    return Lb_SUCCESS;
}

TbResult LbSpriteReset(TbSprite *start, const TbSprite *end, const unsigned char *data)
{
    struct TbSprite *sprt;
    int n;

    n = 0;
    sprt = start;
    while (sprt < end)
    {
      if ((uintptr_t)sprt->Data >= (uintptr_t)data)
      {
        sprt->Data = (TbSpriteData)(uintptr_t)(sprt->Data - data);
        n++;
      }
      sprt++;
    }
    LOGDBG("reset %d of %d sprites", n, (sprt-start));
    return Lb_SUCCESS;
}

TbResult LbSpriteClearAll(TbSetupSprite t_setup[])
{
    struct TbSetupSprite *stp_sprite;
    int idx;
    idx = 0;
    stp_sprite=&t_setup[idx];
    while (stp_sprite->Data != NULL)
    {
        if ((stp_sprite->Start != NULL) && (stp_sprite->End != NULL))
        {
            *(stp_sprite->Start) = NULL;
            *(stp_sprite->End) = NULL;
            *(stp_sprite->Data) = 0;
        }
        idx++;
        stp_sprite=&t_setup[idx];
    }
    LOGSYNC("cleaned %d SetupSprite lists", idx);
    return Lb_SUCCESS;
}
/******************************************************************************/
