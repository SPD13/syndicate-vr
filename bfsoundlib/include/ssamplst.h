/******************************************************************************/
// Bullfrog Sound Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file ssamplst.h
 *     Header file for ssamplst.c.
 * @par Purpose:
 *     Sound Sample List support routines.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     12 Jun 2022 - 05 Sep 2022
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFSOUNDLIB_SSAMPLST_H_
#define BFSOUNDLIB_SSAMPLST_H_

#include <stdint.h>
#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct BfSoundBankHead {
  s32 TabPos;
  s32 DatPos;
  s32 TabSize;
  s32 DatSize;
};

/** Sample entry of a sound bank, as in the file (32 bytes).
 * DataBeg and DataEnd are offsets in the bank data (SfxData); they stay
 * offsets in memory, so the entry has the file layout on every target.
 */
struct BfSfxInfo { // sizeof=32
  char FileName[12];
  s32 sffld_12;
  short sffld_16;
  u32 DataBeg;
  s32 sffld_22;
  u32 DataEnd;
  short sffld_30;
};

#pragma pack()
/******************************************************************************/

void PlaySampleList(int sfx1, int sfx2, int sfx3, int sfx4,
  int sfx5, int sfx6, int sfx7, int sfx8);

void StopSampleQueueList(void);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif // BFSOUNDLIB_SSAMPLST_H_
