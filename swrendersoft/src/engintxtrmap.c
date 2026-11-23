/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file engintxtrmap.c
 *     Engine texture map atlas storage.
 * @par Purpose:
 *     Implement functions for allocating and maintaining texture atlas.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "engintxtrmap.h"

#include "bffile.h"
#include "bfmemory.h"
#include "bfmemut.h"

#include "privrdlog.h"
/******************************************************************************/
ubyte *vec_tmap[VEC_TMAP_LIMIT] = {0};

ubyte *tmaps_alloc_start = NULL;
ubyte *tmaps_extra_buf = NULL;
u32 tmaps_extra_len = 0;

/******************************************************************************/

TbResult alloc_texturemaps(uint tmap_count)
{
    ubyte *p_buf;
    uint i;

    p_buf = NULL;
    if (tmap_count > 0)
    {
        p_buf = LbMemoryAlloc((tmap_count + 2) * (VEC_TMAP_WIDTH * VEC_TMAP_HEIGHT));
    }
    if (p_buf == NULL)
    {
        return Lb_FAIL;
    }
    tmaps_alloc_start = p_buf;
    tmaps_extra_buf = p_buf;
    p_buf = (ubyte *)(((uintptr_t)p_buf + 0xffff) & ~0xffff);
    tmaps_extra_len = p_buf - tmaps_extra_buf;
    for (i = 0; i < tmap_count; i++)
    {
        vec_tmap[i] = p_buf;
        p_buf += VEC_TMAP_WIDTH * VEC_TMAP_HEIGHT;
    }
    return Lb_SUCCESS;
}

void free_texturemaps(void)
{
    LbMemoryFree(tmaps_alloc_start);
}
/******************************************************************************/
