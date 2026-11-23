/******************************************************************************/
// Bullfrog Sound Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file bfmusic.h
 *     Header file for ???.
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
#ifndef BFSOUNDLIB_BFMUSIC_H_
#define BFSOUNDLIB_BFMUSIC_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

struct MusicBankSizes {
    uint32_t mbs0;
    uint32_t mbs1;
    uint32_t mbs2;
    uint32_t mbs3;
    uint32_t mbs4;
    uint32_t mbs5;
    uint32_t mbs6;
    uint32_t mbs7;
};

/** Song entry of a music bank, as in the file (32 bytes).
 * DataBeg and DataEnd are offsets in the bank data (BfMusicData); they
 * stay offsets in memory, so the entry has the file layout on every target.
 */
struct BfMusicInfo {
  char FileName[12];
  s32 mifld_12;
  short mifld_16;
  u32 DataBeg;
  s32 mifld_22;
  u32 DataEnd;
  short mifld_30;
};

#pragma pack()
/******************************************************************************/

void InitMusic(void);
void FreeMusic(void);

int LoadMusic(ushort bankNo);

void StartMusic(int songNo, ubyte volume);

void StopMusicIfActive(void);
void StopMusic(void);

void SetMusicVolume(int msec, ubyte volume);

void SetMusicTempo(int tempo, int msec);
void SetMusicTempoNormal(void);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif // BFSOUNDLIB_BFMUSIC_H_
