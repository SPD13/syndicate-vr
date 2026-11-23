/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file engintxtrmap.h
 *     Header file for engintxtrmap.c.
 * @par Purpose:
 *     Engine texture map atlas storage.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef ENGINTXTRMAP_H
#define ENGINTXTRMAP_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

#define VEC_TMAP_LIMIT 18
#define VEC_TMAP_WIDTH 256
#define VEC_TMAP_HEIGHT 256

#pragma pack()
/******************************************************************************/

/** Engine texture atlas vector.
 */
extern ubyte *vec_tmap[VEC_TMAP_LIMIT];

/** Reference to the start of tmaps allocation.
 */
extern ubyte *tmaps_alloc_start;

/** Reference to extra buffer at start of tmaps allocation.
 * Can be used externally - this memory is not used within tmaps atlas vector.
 */
extern ubyte *tmaps_extra_buf;
extern u32 tmaps_extra_len;

TbResult alloc_texturemaps(uint tmap_count);
void free_texturemaps(void);
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
