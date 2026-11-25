/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file sound.h
 *     Header file for sound.c.
 * @par Purpose:
 *     Sound related routines.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 02 Nov 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>
#include <stdint.h>

#include "bftypes.h"
#include "mssal.h"

/******************************************************************************/

#pragma pack(1)

enum SounsHeapSetupCmd {
    /** Load sounds from a bank selected based on available memory. */
    SHSC_GameSndAutoQ = 0,
    /** Unload sounds and free memory. */
    SHSC_ResetGameSnd,
    /** Load best quality sounds only */
    SHSC_GameSndBestQ,
    /** Load sounds for game credits screen. */
    SHSC_CreditsSnd = 100,
};

struct Thing;
struct SimpleThing;

/** Item of the sound heap, holding the data of one sample.
 * In 32-bit builds the layout is the one of the asm (0x1C bytes).
 */
struct HeapMgrHandle {
  ubyte *Data;                  /**< 0x00 sample data, in the heap area */
  s32 Size;                     /**< 0x04 */
  ushort Flags;                 /**< 0x08 0x01 used, 0x02 locked, 0x04 pinned */
  ushort SmpId;                 /**< 0x0A sample table index */
  struct HeapMgrHandle *Prev;   /**< 0x0C previous item by address */
  struct HeapMgrHandle *Next;   /**< 0x10 next item by address */
  struct HeapMgrHandle *Older;  /**< 0x14 older item in the age list */
  struct HeapMgrHandle *Newer;  /**< 0x18 newer item in the age list */
};

struct SampleTable {
  s32 field_0;
  s32 field_4;
  struct HeapMgrHandle *hmhandle;
};

#define STARTSCR_VOLUME_MAX 322

#pragma pack()

extern short startscr_samplevol;
extern short startscr_midivol;
extern short startscr_cdvolume;

/******************************************************************************/

void set_default_sfx_settings(void);
void sfx_apply_samplevol(void);
void sfx_apply_midivol(void);
void sfx_apply_cdvolume(void);
void fill_ail_sample_ids(void);
void monitor_all_samples(void);

struct SampleInfo *play_sample_using_heap(u32 bank_id, short smptbl_id,
  u32 volume, u32 pan, u32 pitch, sbyte loop_count, ubyte type);

void stop_sample_using_heap(s32 source_id, u32 sample_number);
void play_dist_sample(struct Thing *p_thing, ushort smptbl_id, ushort vol, ushort pan, int pitch, int loop, ubyte type);
int play_dist_speech(struct Thing *p_thing, ushort samp, ushort vol, ushort pan, int pitch, int loop, ubyte type);
void play_dist_ssample(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol, ushort pan, int pitch, int loop, ubyte type);
void play_disk_sample(short id, ushort sample, short vol, short pan, int pitch, int loop, int type);

void snd_unkn1_volume_all_samples(void);
void wait_for_sound_sample_finish(ushort smpl_id);

void setup_heaps(short setup_cmd, const char *lang);
void reset_heaps(void);
void process_sound_heap(void);

/******************************************************************************/
#endif
