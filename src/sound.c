/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file sound.c
 *     Sound related routines.
 * @par Purpose:
 *     Implements routines which help adjust functions from bfsoundlib to
 *     the games needs.
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

#include "sound.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bfwindows.h"
#include "bffile.h"
#include "bfmemory.h"
#include "bfmemut.h"
#include "bfmusic.h"
#include "bfsound.h"
#include "bfaudio.h"
#include "bfscd.h"
#include "ailss.h"
#include "aila.h"
#include "sb16.h"
#include "awe32.h"
#include "streamfx.h"
#include "sndtimer.h"
#include "bfsvaribl.h"
#include "snderr.h"
#include "ssampply.h"
#include "ssamplst.h"

#include "difftest.h"
#include "display.h"
#include "engincam.h"
#include "engintrns.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "thing.h"
#include "util.h"
#include "swlog.h"

/******************************************************************************/

#pragma pack(1)

struct HeapMgrHeader { // sizeof=0x24
    void *field_0;
    void *field_4;
    int field_8;
    int samples_count;
    int field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
};

#pragma pack()

extern long sound_heap_size;
extern struct SampleTable *sound_heap_memory;
extern TbFileHandle sound_file; // = INVALID_FILE;
extern struct HeapMgrHeader *hmhead;
extern long samples_in_bank;
extern struct SampleTable *sample_table;

short startscr_samplevol;
short startscr_midivol;
short startscr_cdvolume;

/******************************************************************************/

void set_default_sfx_settings(void)
{
    startscr_samplevol = STARTSCR_VOLUME_MAX;
    startscr_midivol = STARTSCR_VOLUME_MAX;
    startscr_cdvolume = STARTSCR_VOLUME_MAX * 71 / 100;
}

void sfx_apply_samplevol(void)
{
    if (startscr_samplevol < 0)
        startscr_samplevol = 0;
    if (startscr_samplevol > STARTSCR_VOLUME_MAX)
        startscr_samplevol = STARTSCR_VOLUME_MAX;
    SetSoundMasterVolume(127 * startscr_samplevol / STARTSCR_VOLUME_MAX);
}

void sfx_apply_midivol(void)
{
    if (startscr_midivol < 0)
        startscr_midivol = 0;
    if (startscr_midivol > STARTSCR_VOLUME_MAX)
        startscr_midivol = STARTSCR_VOLUME_MAX;
    SetMusicMasterVolume(127 * startscr_midivol / STARTSCR_VOLUME_MAX);
}

void sfx_apply_cdvolume(void)
{
    if (startscr_cdvolume < 0)
        startscr_cdvolume = 0;
    if (startscr_cdvolume > STARTSCR_VOLUME_MAX)
        startscr_cdvolume = STARTSCR_VOLUME_MAX;
    SetCDVolume(70 * (127 * startscr_cdvolume / STARTSCR_VOLUME_MAX) / 100);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int heapmgr_add_item_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_heapmgr_add_item\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Allocates a heap item of the given size.
 * a1 is the heap header, a2 the size in bytes. Takes the first unused
 * handle (ushort flags at 0x08 zero) from the array at 0x24, places it in
 * the first gap large enough in the address-ordered list (first at 0x18,
 * links 0x0C/0x10), and appends it as the newest in the age list.
 * Returns the item, or 0 if there is no room or no free handle.
 */
int heapmgr_add_item(int a1, int a2, int a3, int a4)
{
    struct HeapMgrHeader *p_hmhead = (struct HeapMgrHeader *)a1;
    s32 size = a2;
    ubyte *p_item;
    ubyte *p_prev;
    ubyte *p_next;
    s32 gap_beg, gap_end;

    DIFFTEST_SHADOW_RET(heapmgr_add_item, int, heapmgr_add_item_asm(a1, a2, a3, a4));
    if (size > p_hmhead->field_8 - p_hmhead->field_14)
        DIFFTEST_RETURN((int)0);
    if (p_hmhead->field_10 >= p_hmhead->samples_count)
        DIFFTEST_RETURN((int)0);
    p_item = (ubyte *)p_hmhead + 0x24;
    while (*(ushort *)(p_item + 0x08) != 0)
        p_item += 0x1C;

    if (p_hmhead->field_18 == 0)
    {
        p_hmhead->field_18 = (s32)p_item;
        *(s32 *)(p_item + 0x00) = (s32)p_hmhead->field_0;
    }
    else
    {
        p_prev = (ubyte *)p_hmhead->field_18;
        for (;;)
        {
            p_next = (ubyte *)*(s32 *)(p_prev + 0x10);
            gap_beg = *(s32 *)(p_prev + 0x00) + *(s32 *)(p_prev + 0x04);
            if (p_next != NULL)
                gap_end = *(s32 *)(p_next + 0x00);
            else
                gap_end = (s32)p_hmhead->field_4;
            if (gap_end - gap_beg >= size)
                break;
            if (p_next == NULL)
                DIFFTEST_RETURN((int)0);
            p_prev = p_next;
        }
        *(s32 *)(p_item + 0x0C) = (s32)p_prev;
        *(s32 *)(p_item + 0x00) = *(s32 *)(p_prev + 0x00) + *(s32 *)(p_prev + 0x04);
        p_next = (ubyte *)*(s32 *)(p_prev + 0x10);
        *(s32 *)(p_item + 0x10) = (s32)p_next;
        if (p_next != NULL)
            *(s32 *)(p_next + 0x0C) = (s32)p_item;
        *(s32 *)(p_prev + 0x10) = (s32)p_item;
    }
    *(ushort *)(p_item + 0x08) = 1;
    *(s32 *)(p_item + 0x04) = size;
    p_hmhead->field_14 += size;
    p_hmhead->field_10++;
    if (p_hmhead->field_20 != 0)
    {
        *(s32 *)((ubyte *)p_hmhead->field_20 + 0x18) = (s32)p_item;
        *(s32 *)(p_item + 0x14) = p_hmhead->field_20;
    }
    else
    {
        p_hmhead->field_1C = (s32)p_item;
    }
    p_hmhead->field_20 = (s32)p_item;
    DIFFTEST_RETURN((int)p_item);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void heapmgr_free_handle(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_heapmgr_free_handle\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int heapmgr_free_oldest_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_heapmgr_free_oldest\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
    return ra;
}

/** Frees the oldest heap item which is not locked (flag 0x02 at 0x08).
 * a1 is the heap header; walks the age list from the oldest (0x1C) through
 * the newer links (0x18). Returns -1 if no item could be freed, otherwise
 * the sample index of the freed item (ushort at 0x0A).
 */
int heapmgr_free_oldest(int a1, int a2, int a3, int a4)
{
    struct HeapMgrHeader *p_hmhead = (struct HeapMgrHeader *)a1;
    ubyte *p_item;
    int smp_id;

    DIFFTEST_SHADOW_RET(heapmgr_free_oldest, int, heapmgr_free_oldest_asm(a1, a2, a3, a4));
    p_item = (ubyte *)p_hmhead->field_1C;
    if (p_item == NULL)
        DIFFTEST_RETURN((int)-1);
    while ((*(p_item + 0x08) & 0x02) != 0)
    {
        p_item = (ubyte *)*(s32 *)(p_item + 0x18);
        if (p_item == NULL)
            DIFFTEST_RETURN((int)-1);
    }
    // The asm returns whatever eax the free_handle call leaves; the asm
    // free_handle returns the item's ushort at 0x0A, read before it clears
    // the item. Read it here, as heapmgr_free_handle() is declared void.
    smp_id = *(ushort *)(p_item + 0x0A);
    heapmgr_free_handle(a1, (int)p_item, a1, a4);
    DIFFTEST_RETURN(smp_id);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void heapmgr_make_newest_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_heapmgr_make_newest\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Moves a heap item to the newest end of the age list.
 * a1 is the heap header, a2 the item; the item keeps the older neighbour
 * at 0x14 and the newer one at 0x18.
 */
void heapmgr_make_newest(int a1, int a2, int a3, int a4)
{
    struct HeapMgrHeader *p_hmhead = (struct HeapMgrHeader *)a1;
    ubyte *p_item = (ubyte *)a2;
    ubyte *p_newer;
    ubyte *p_older;
    s32 newest;

    DIFFTEST_SHADOW(heapmgr_make_newest, heapmgr_make_newest_asm(a1, a2, a3, a4));
    p_newer = (ubyte *)*(s32 *)(p_item + 0x18);
    if (p_newer == NULL)
        return;
    p_older = (ubyte *)*(s32 *)(p_item + 0x14);
    *(s32 *)(p_newer + 0x14) = (s32)p_older;
    if (p_older != NULL)
        *(s32 *)(p_older + 0x18) = *(s32 *)(p_item + 0x18);
    else
        p_hmhead->field_1C = *(s32 *)(p_item + 0x18);
    *(s32 *)((ubyte *)p_hmhead->field_20 + 0x18) = (s32)p_item;
    newest = p_hmhead->field_20;
    *(s32 *)(p_item + 0x18) = 0;
    *(s32 *)(p_item + 0x14) = newest;
    p_hmhead->field_20 = (s32)p_item;
}




static DIFFTEST_REF_FUNC struct SampleInfo *play_sample_using_heap_asm(ulong bank_id,
  short smptbl_id, ulong volume, ulong pan, ulong pitch, sbyte loop_count, ubyte stype)
{
    int ra = (int)bank_id, rd = (int)smptbl_id, rb = (int)volume, rc = (int)pan;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop_count;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_play_sample_using_heap\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc)
        : "S" (stkargs)
        : "cc", "memory");
    return (struct SampleInfo *)ra;
}

/** Plays a sample from the sound bank, loading it into the sound heap first
 * if it is not there yet. To get room, frees the oldest heap items (up to 50
 * tries). Marks the played sample (UserFlag 0x01) and locks its heap handle
 * (flags 0x06 at 0x08).
 */
struct SampleInfo *play_sample_using_heap(ulong bank_id, short smptbl_id,
  ulong volume, ulong pan, ulong pitch, sbyte loop_count, ubyte stype)
{
    struct SampleTable *p_smptbl;
    struct SampleInfo *p_sinfo;
    ubyte *p_hmhandle;
    s32 smp_offs;
    s32 smp_size;
    int smp_id;
    int freed_id;
    int i;

    DIFFTEST_SHADOW_RET(play_sample_using_heap, struct SampleInfo *,
      play_sample_using_heap_asm(bank_id, smptbl_id, volume, pan, pitch, loop_count, stype));
    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        DIFFTEST_RETURN((struct SampleInfo *)NULL);
    if (sound_file == INVALID_FILE)
        DIFFTEST_RETURN((struct SampleInfo *)NULL);
    smp_id = smptbl_id;
    if ((smp_id <= 0) || (smp_id >= samples_in_bank))
        DIFFTEST_RETURN((struct SampleInfo *)NULL);

    p_smptbl = &sample_table[smp_id];
    if (p_smptbl->hmhandle == NULL)
    {
        smp_size = p_smptbl->field_4;
        smp_offs = p_smptbl->field_0;
        p_hmhandle = NULL;
        for (i = 0; i < 50; i++)
        {
            p_hmhandle = (ubyte *)heapmgr_add_item((int)hmhead, smp_size, i, smp_size);
            if (p_hmhandle != NULL)
                break;
            freed_id = heapmgr_free_oldest((int)hmhead, smp_size, i, smp_size);
            if (freed_id < 0)
                break;
            // Store of the (null) handle into the freed sample entry
            sample_table[freed_id].hmhandle = (struct HeapMgrHandle *)p_hmhandle;
        }
        if (p_hmhandle != NULL)
        {
            *(ushort *)(p_hmhandle + 0x0A) = smp_id;
            LbFileSeek(sound_file, smp_offs, Lb_FILE_SEEK_BEGINNING);
            LbFileRead(sound_file, *(void **)(p_hmhandle + 0x00), smp_size);
        }
        p_smptbl->hmhandle = (struct HeapMgrHandle *)p_hmhandle;
    }
    p_hmhandle = (ubyte *)p_smptbl->hmhandle;
    if (p_hmhandle == NULL)
        DIFFTEST_RETURN((struct SampleInfo *)NULL);
    heapmgr_make_newest((int)hmhead, (int)p_hmhandle, (int)volume, (int)pan);
    p_sinfo = PlaySampleFromAddress(bank_id, smptbl_id, volume, pan, pitch,
      loop_count, stype, *(void **)p_smptbl->hmhandle);
    if (p_sinfo != NULL)
    {
        p_sinfo->UserFlag |= 0x01;
        p_smptbl->hmhandle->field_0[8] |= 0x06;
    }
    DIFFTEST_RETURN(p_sinfo);
}

static DIFFTEST_REF_FUNC void stop_sample_using_heap_asm(long source_id, ulong sample_number)
{
    asm volatile (
      "call ASM_stop_sample_using_heap\n"
        : "+a" (source_id), "+d" (sample_number) : : "cc", "memory");
}

/** Stops a sample from the heap-backed sample table. If no other source
 * still plays the sample, clears the lock flags (0x06 at 0x08) of its heap
 * handle.
 */
void stop_sample_using_heap(long source_id, ulong sample_number)
{
    struct SampleInfo *p_last;
    struct SampleInfo *p_sinfo;
    struct HeapMgrHandle *p_hmhandle;
    short smp_id;

    DIFFTEST_SHADOW(stop_sample_using_heap, stop_sample_using_heap_asm(source_id, sample_number));
    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    smp_id = (short)sample_number;
    StopSample(source_id, smp_id);
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo < p_last; p_sinfo++)
    {
        if (p_sinfo->SampleNumber != smp_id)
            continue;
        if (p_sinfo->SampleHandle == NULL)
            continue;
        if ((p_sinfo->UserFlag & 0x01) == 0)
            continue;
        if (IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            break;
    }
    if (p_sinfo < p_last)
        return;
    p_hmhandle = sample_table[smp_id].hmhandle;
    if (p_hmhandle == NULL)
        return;
    p_hmhandle->field_0[8] &= ~0x06;
}

int play_dist_speech(struct Thing *p_thing, ushort speech_no, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    if ((p_thing <= &things[0]) || (p_thing >= &things[THINGS_LIMIT])) {
        LOGERR("Speech %hu playback requested on invalid thing", speech_no);
        return -1;
    }
    play_dist_sample(p_thing, 129 + speech_no,  vol, pan, pitch, loop, stype);
    return 0;
}

void play_disk_sample(short id, ushort sample, short vol,
  short pan, int pitch, int loop, int stype)
{
    play_sample_using_heap(9999, 129 + sample, vol, pan, pitch, loop, stype);
}
static DIFFTEST_REF_FUNC int snd_unkn3_find_stop_sample_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_snd_unkn3_find_stop_sample\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx); only eax (sample
 * number) and edx (volume) are read.
 *
 * Stops the quietest playing instance of given sample, if it is quieter
 * than given volume. Returns 1 if a sample was stopped.
 */
int snd_unkn3_find_stop_sample(int a1, int a2, int a3, int a4)
{
    struct SampleInfo *p_sinfo;
    struct SampleInfo *p_first;
    struct SampleInfo *p_last;
    ushort smp_id;
    ushort vol;
    short best_source;
    short best_vol;

    DIFFTEST_SHADOW_RET(snd_unkn3_find_stop_sample, int,
      snd_unkn3_find_stop_sample_asm(a1, a2, a3, a4));
    smp_id = (ushort)a1;
    vol = (ushort)a2;
    best_source = 9999;
    best_vol = 9999;
    p_first = GetFirstSampleInfoStructure();
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = p_first; p_sinfo <= p_last; p_sinfo++)
    {
        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle))
            continue;
        if (p_sinfo->SampleNumber != smp_id)
            continue;
        if ((s32)vol <= p_sinfo->SampleVolume)
            continue;
        if ((s32)best_vol <= p_sinfo->SampleVolume)
            continue;
        best_source = (short)p_sinfo->SourceID;
        best_vol = (short)p_sinfo->SampleVolume;
    }
    if (best_source == 9999)
        DIFFTEST_RETURN((int)0);
    stop_sample_using_heap(best_source, (short)smp_id);
    DIFFTEST_RETURN((int)1);
}


static DIFFTEST_REF_FUNC void play_dist_sample_asm(struct Thing *p_thing,
  ushort smptbl_id, ushort vol, ushort pan, int pitch, int loop, ubyte stype)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_play_dist_sample\n"
        : : "a" (p_thing), "d" (smptbl_id), "b" (vol), "c" (pan), "S" (stkargs)
        : "cc", "memory");
}

void play_dist_sample(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    ubyte *p_smp_type;
    s32 dx, dy, dz;
    s32 adx, adz;
    s32 dist;
    s32 sc_pan;
    s32 sc_vol;

    DIFFTEST_SHADOW(play_dist_sample, play_dist_sample_asm(p_thing, smptbl_id,
      vol, pan, pitch, loop, stype));
    // Per-sample type table (data_1dded8) is not exported from the asm;
    // it directly follows the 4-byte sound_heap_memory.
    p_smp_type = (ubyte *)&sound_heap_memory + 4;

    if (((ubyte *)p_thing < (ubyte *)things) ||
      ((ubyte *)p_thing > (ubyte *)&things[THINGS_LIMIT]))
        return;

    // The asm does not use the pan argument; pan is computed from position
    dx = (p_thing->X >> 8) - engn_xc;
    dz = (p_thing->Z >> 8) - engn_zc;
    {
        int rx, ry, rz;
        rx = dx;
        rz = dz;
        ry = 0; // uninitialised in the asm; unused by local_to_worldr()
        local_to_worldr(&rx, &ry, &rz);
        dy = rx >> 3;
    }
    if (dy > 0x39c)
        sc_pan = 0x6f;
    else if (dy < -0x39c)
        sc_pan = 0x10;
    else
        sc_pan = (dy >> 4) + 0x4a;
    if (sc_pan < 0x10)
        sc_pan = 0x10;
    else if (sc_pan > 0x6f)
        sc_pan = 0x6f;

    adx = abs(dx);
    adz = abs(dz);
    if (adz < adx)
        dist = adx + (adz >> 1);
    else
        dist = adz + (adx >> 1);

    if (dist < 0xa00)
        sc_vol = ((s32)vol * 127) >> 7;
    else if (dist >= 0x1a00)
        sc_vol = 0;
    else
        sc_vol = ((127 - ((dist - 0xa00) >> 5)) * (s32)vol) >> 7;

    if ((sc_vol <= 5) && (vol != 0))
        return;

    if (p_smp_type[smptbl_id] > 2)
    {
        if ((short)snd_unkn3_find_stop_sample(smptbl_id, (ushort)sc_vol,
          sc_vol, sc_pan) == 0)
            return;
    }
    play_sample_using_heap(p_thing->ThingOffset, (short)smptbl_id, sc_vol,
      sc_pan, (ushort)pitch, (sbyte)loop, stype);
}

static DIFFTEST_REF_FUNC void play_dist_ssample_asm(struct SimpleThing *p_sthing,
  ushort smptbl_id, ushort vol, ushort pan, int pitch, int loop, ubyte stype)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)pitch;
    stkargs[1] = (int)(intptr_t)loop;
    stkargs[2] = (int)(intptr_t)stype;

    asm volatile (
      "push 8(%4)\n"
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_play_dist_ssample\n"
        : : "a" (p_sthing), "d" (smptbl_id), "b" (vol), "c" (pan), "S" (stkargs)
        : "cc", "memory");
}

void play_dist_ssample(struct SimpleThing *p_sthing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte stype)
{
    ubyte *p_smp_type;
    s32 dx, dy, dz;
    s32 adx, adz;
    s32 dist;
    s32 sc_pan;
    s32 sc_vol;

    DIFFTEST_SHADOW(play_dist_ssample, play_dist_ssample_asm(p_sthing,
      smptbl_id, vol, pan, pitch, loop, stype));
    // Per-sample type table (data_1dded8) is not exported from the asm;
    // it directly follows the 4-byte sound_heap_memory.
    p_smp_type = (ubyte *)&sound_heap_memory + 4;

    // Range check as in the asm: sthings[-STHINGS_LIMIT] .. sthings[0]
    if (((ubyte *)p_sthing < (ubyte *)&sthings[-STHINGS_LIMIT]) ||
      ((ubyte *)p_sthing > (ubyte *)sthings))
        return;

    // The asm does not use the pan argument; pan is computed from position
    dx = (p_sthing->X >> 8) - engn_xc;
    dz = (p_sthing->Z >> 8) - engn_zc;
    {
        int rx, ry, rz;
        rx = dx;
        rz = dz;
        ry = 0; // uninitialised in the asm; unused by local_to_worldr()
        local_to_worldr(&rx, &ry, &rz);
        dy = rx >> 3;
    }
    if (dy > 0x39c)
        sc_pan = 0x6f;
    else if (dy < -0x39c)
        sc_pan = 0x10;
    else
        sc_pan = (dy >> 4) + 0x4a;
    if (sc_pan < 0x10)
        sc_pan = 0x10;
    else if (sc_pan > 0x6f)
        sc_pan = 0x6f;

    adx = abs(dx);
    adz = abs(dz);
    if (adz < adx)
        dist = adx + (adz >> 1);
    else
        dist = adz + (adx >> 1);

    if (dist < 0xa00)
        sc_vol = ((s32)vol * 127) >> 7;
    else if (dist >= 0x1a00)
        sc_vol = 0;
    else
        sc_vol = ((127 - ((dist - 0xa00) >> 5)) * (s32)vol) >> 7;

    if ((sc_vol <= 5) && (vol != 0))
        return;

    if (p_smp_type[smptbl_id] > 2)
    {
        if ((short)snd_unkn3_find_stop_sample(smptbl_id, (ushort)sc_vol,
          sc_vol, sc_pan) == 0)
            return;
    }
    play_sample_using_heap(p_sthing->ThingOffset, (short)smptbl_id, sc_vol,
      sc_pan, (ushort)pitch, (sbyte)loop, stype);
}

static DIFFTEST_REF_FUNC void stop_looped_weapon_sample_asm(struct Thing *p_person, short weapon)
{
    asm volatile ("call ASM_stop_looped_weapon_sample\n"
        : : "a" (p_person), "d" (weapon) : "cc", "memory");
}

void stop_looped_weapon_sample(struct Thing *p_person, short weapon)
{
    ushort plagent;

    DIFFTEST_SHADOW(stop_looped_weapon_sample,
      stop_looped_weapon_sample_asm(p_person, weapon));
    if ((p_person->Flag2 & TgF2_Unkn0200) == 0)
        return;

    switch ((ushort)weapon)
    {
    case 1:
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            plagent = p_person->U.UPerson.ComCur & 3;
        else
            plagent = 0;
        ReleaseLoopedSample(p_person->ThingOffset, (short)(0x50 + plagent));
        p_person->Flag2 &= ~TgF2_Unkn0200;
        break;
    case 2:
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            plagent = p_person->U.UPerson.ComCur & 3;
        else
            plagent = 0;
        ReleaseLoopedSample(p_person->ThingOffset, (short)(0x54 + plagent));
        play_dist_sample(p_person, 0x58 + (p_person->U.UPerson.ComCur & 3),
          0x7f, 0x40, 100, 0, 1);
        p_person->Flag2 &= ~TgF2_Unkn0200;
        break;
    case 8:
        ReleaseLoopedSample(p_person->ThingOffset, 0x0e);
        p_person->Flag2 &= ~TgF2_Unkn0200;
        break;
    default:
        break;
    }
}

//TODO better name?
static DIFFTEST_REF_FUNC void fill_ail_sample_ids_asm(void)
{
    asm volatile ("call ASM_fill_ail_sample_ids\n"
        :  :  : "eax", "cc", "memory" );
}

extern u32 ail_sample_ids[];

void fill_ail_sample_ids(void)
{
    struct SampleInfo *p_smpinfo;
    struct SampleInfo *p_last;
    u32 i;

    DIFFTEST_SHADOW(fill_ail_sample_ids, fill_ail_sample_ids_asm());
    if (!GetSoundInstalled())
        return;
    p_smpinfo = GetFirstSampleInfoStructure();
    p_last = GetLastSampleInfoStructure();
    i = 0;
    for (; p_smpinfo <= p_last; p_smpinfo++)
    {
        ail_sample_ids[(ushort)i] = *(u32 *)p_smpinfo;
        i++;
    }
}

static DIFFTEST_REF_FUNC void monitor_all_samples_asm(void)
{
    asm volatile ("call ASM_monitor_all_samples\n"
        :  :  : "eax", "cc", "memory" );
}

void monitor_all_samples(void)
{
    struct SampleInfo *p_smpinfo;
    struct SampleInfo *p_last;
    struct Thing *p_thing;
    ubyte *p_smp_type;
    u32 i;
    s32 sc_vol;

    DIFFTEST_SHADOW(monitor_all_samples, monitor_all_samples_asm());
    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    // Per-sample type table (data_1dded8) is not exported from the asm;
    // it directly follows the 4-byte sound_heap_memory.
    p_smp_type = (ubyte *)&sound_heap_memory + 4;
    memset(p_smp_type, 0, 0x18f);

    p_thing = NULL;
    sc_vol = 0;
    p_smpinfo = GetFirstSampleInfoStructure();
    p_last = GetLastSampleInfoStructure();
    for (i = 0; p_smpinfo <= p_last; p_smpinfo++, i++)
    {
        short source_id;
        s32 smp_id;
        s32 dx, dz;
        s32 adx, adz;
        s32 dist;
        s32 sc_pan;

        if (*(u32 *)p_smpinfo != ail_sample_ids[(ushort)i])
            continue;
        if (!IsSamplePlaying(0, 0, p_smpinfo->SampleHandle))
            continue;
        source_id = (short)p_smpinfo->SourceID;
        if (source_id > 0)
            p_thing = &things[source_id];
        else if (source_id < 0)
            p_thing = (struct Thing *)&sthings[source_id];

        smp_id = p_smpinfo->SampleNumber;
        if (smp_id != 9999)
            p_smp_type[smp_id]++;
        // The asm also sprintf()s a debug line into a local buffer here,
        // which is never used; omitted.

        if (source_id == 0 || source_id == 9999)
            continue;

        dx = (p_thing->X >> 8) - engn_xc;
        dz = (p_thing->Z >> 8) - engn_zc;
        {
            int rx, ry, rz;
            s32 dy;
            rx = dx;
            rz = dz;
            ry = 0; // uninitialised in the asm; unused by local_to_worldr()
            local_to_worldr(&rx, &ry, &rz);
            dy = rx >> 3;
            if (dy > 0x39c)
                sc_pan = 0x6f;
            else if (dy < -0x39c)
                sc_pan = 0x10;
            else
                sc_pan = (dy >> 4) + 0x4a;
            if (sc_pan < 0x10)
                sc_pan = 0x10;
            else if (sc_pan > 0x6f)
                sc_pan = 0x6f;
        }

        adx = abs(dx);
        adz = abs(dz);
        if (adx > adz)
            dist = adx + (adz >> 1);
        else
            dist = adz + (adx >> 1);

        if (dist < 0xa00)
            sc_vol = (0x7f * 0x7f) >> 7;
        else if (dist >= 0x1a00)
            sc_vol = 0;
        else
            sc_vol = ((0x7f - ((dist - 0xa00) >> 5)) * 0x7f) >> 7;

        if ((p_thing->Flag & TngF_StationrSht) != 0)
        {
            if (smp_id == 0x34 || smp_id == 7)
                sc_vol = 0;
        }

        if ((sc_vol < 5) || ((p_smp_type[smp_id] > 3) &&
          ((p_thing->Flag & TngF_PlayerAgent) == 0)))
        {
            if (smp_id == 0x2c || smp_id == 0x38)
            {
                if (p_smp_type[smp_id] <= 3)
                    continue;
            }
            stop_sample_using_heap(source_id, smp_id);
            continue;
        }

        if (sc_vol > 0x7f)
            sc_vol = 0x7f;
        if (p_thing->Type == TT_VEHICLE)
        {
            SetSamplePitch(source_id, smp_id,
              (short)((p_thing->Speed >> 6) + 100));
            SetSampleVolume(source_id, smp_id, (short)(sc_vol >> 1));
        }
        else
        {
            SetSampleVolume(source_id, smp_id, (short)sc_vol);
        }
        SetSamplePan(source_id, smp_id, (ushort)sc_pan);
    }
}

static DIFFTEST_REF_FUNC void snd_unkn1_volume_all_samples_asm(void)
{
    asm volatile ("call ASM_snd_unkn1_volume_all_samples\n"
        :  :  : "eax", "cc", "memory" );
}

void snd_unkn1_volume_all_samples(void)
{
    struct SampleInfo *p_smpinfo;
    struct SampleInfo *p_last;

    DIFFTEST_SHADOW(snd_unkn1_volume_all_samples, snd_unkn1_volume_all_samples_asm());
    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    update_danger_music(2);
    p_smpinfo = GetFirstSampleInfoStructure();
    p_last = GetLastSampleInfoStructure();
    for (; p_smpinfo <= p_last; p_smpinfo++)
    {
        short source_id;
        short smp_id;

        source_id = (short)p_smpinfo->SourceID;
        if (!IsSamplePlaying(0, 0, p_smpinfo->SampleHandle))
            continue;
        smp_id = p_smpinfo->SampleNumber;
        if (smp_id == 8 || smp_id == 77 || smp_id == 78)
            continue;
        SetSampleVolume(source_id, smp_id, 0);
    }
}

void wait_for_sound_sample_finish(ushort smpl_id)
{
    while (IsSamplePlaying(0, smpl_id, NULL)) {
        swap_wscreen();
        game_update();
    }
}

struct HeapMgrHeader *heapmgr_init(void *p_buf, int buf_size, int n_samples)
{
#if 0
    struct HeapMgrHeader *ret;
    asm volatile ("call ASM_heapmgr_init\n"
        : "=r" (ret) : "a" (p_buf), "d" (buf_size), "b" (n_samples));
    return ret;
#endif
    struct HeapMgrHeader *p_hmhead;
    uint offs_after_samples;
    int offs3;

    p_hmhead = (struct HeapMgrHeader *)p_buf;
    offs_after_samples = 0x1C * n_samples + sizeof(struct HeapMgrHeader);
    offs3 = buf_size - offs_after_samples;
    if (offs3 <= 0)
        return NULL;

    p_hmhead->field_10 = 0;
    p_hmhead->field_14 = 0;
    p_hmhead->field_18 = 0;
    p_hmhead->field_1C = 0;
    p_hmhead->field_20 = 0;
    p_hmhead->field_8 = offs3;
    p_hmhead->samples_count = n_samples;
    p_hmhead->field_0 = (ubyte *)p_hmhead + offs_after_samples;
    p_hmhead->field_4 = (ubyte *)p_hmhead + buf_size;
    return p_hmhead;
}

int setup_heap_manager(struct SampleTable *smptable, size_t smptb_len,
  const char *fname, ushort sndtype)
{
    TbFileHandle fh;
    ubyte tpno;
    long totlen;
    long pos;
    struct SampleTable *smptb;
    size_t tab_smptb_len;
    struct BfSoundBankHead *sbh;
    int i;
    long smptb_len_diff;
    ubyte *p_smptb_end;
    struct BfSoundBankHead sbharr[9];
    struct BfSfxInfo sfxi;
    ubyte unkhead1[18];

    LbMemorySet(smptable, 0, smptb_len);
    fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    sound_file = fh;
    if (fh == INVALID_FILE) {
        reset_heaps();
        return 0;
    }
    tpno = GetSoundTpNo(sndtype);
    if (tpno >= 255) {
        reset_heaps();
        return 0;
    }

    totlen = LbFileLengthHandle(fh);
    LbFileSeek(fh, totlen - 4, Lb_FILE_SEEK_BEGINNING);
    LbFileRead(fh, &pos, 4);
    LbFileSeek(fh, pos, Lb_FILE_SEEK_BEGINNING);
    LbFileRead(fh, &unkhead1, 18);
    LbFileRead(fh, &sbharr, sizeof(struct BfSoundBankHead) * 9);
    sbh = &sbharr[tpno];
    if ((sbh->TabPos == 0) || (sbh->TabSize == 0)) {
        reset_heaps();
        return 0;
    }
    samples_in_bank = sbh->TabSize / sizeof(struct BfSfxInfo);
    tab_smptb_len = sizeof(struct SampleTable) * samples_in_bank;
    if (tab_smptb_len >= smptb_len) {
        reset_heaps();
        return 0;
    }
    LbFileSeek(fh, sbh->TabPos, Lb_FILE_SEEK_BEGINNING);
    smptb = smptable;
    for (i = 0; i < samples_in_bank; i++)
    {
        LbFileRead(fh, &sfxi, sizeof(struct BfSfxInfo));
        smptb->field_0 = sbh->DatPos + (long)sfxi.DataBeg;
        smptb->hmhandle = 0;
        smptb->field_4 = (long)sfxi.DataEnd;
        smptb++;
    }
    if (samples_in_bank <= 0) {
        reset_heaps();
        return 0;
    }
    sample_table = smptable;
    smptb_len_diff = smptb_len - tab_smptb_len;
    p_smptb_end = (ubyte *)smptable + tab_smptb_len;
    if (smptb_len_diff <= 0)
        return 0;
    hmhead = heapmgr_init(p_smptb_end, smptb_len_diff, samples_in_bank);
    if (hmhead == NULL) {
        reset_heaps();
        return 0;
    }
    reset_heaps();
    return 1;
}

void reset_heaps(void)
{
}

TbResult sound_banks_fname(char *fname, const char *dir, const char *lang)
{
    sprintf(fname, "%s/sound_%s.dat", dir, lang);
    if (LbFileExists(fname)) {
        return Lb_OK;
    }
    // If no language-specific version found, use the english as default
    sprintf(fname, "%s/sound_%s.dat", dir, "eng");
    if (LbFileExists(fname)) {
        return Lb_OK;
    }

    // As last effort, try the old file name from original game
    sprintf(fname, "%s/sound.dat", dir);
    if (LbFileExists(fname)) {
        return Lb_OK;
    }

    return Lb_FAIL;
}

void setup_heaps(short setup_cmd, const char *lang)
{
    char locstr[DISKPATH_SIZE];
    PathInfo *pinfo;
    long sz;
    unsigned int n;

    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    pinfo = &game_dirs[DirPlace_Sound];

    switch (setup_cmd)
    {
    case SHSC_CreditsSnd:
        sprintf(locstr, "%s/syncreds.dat", pinfo->directory);
        sz = sound_heap_size;
        setup_heap_manager(sound_heap_memory, sz, locstr, 1622);
        break;
    case 99:
        break;
    case SHSC_GameSndAutoQ:
        sz = 1500000;
        while ((sound_heap_memory == NULL) && (sz > 100))
        {
            sound_heap_memory = LbMemoryAlloc(sz);
            if (sound_heap_memory == NULL)
                sz -= 30000;
        }
        if ((sound_heap_memory == NULL) || sz < 120000) {
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (sz <= 500000)
            n = 822;
        else
            n = 1622;
        if (sound_banks_fname(locstr, pinfo->directory, lang) == Lb_FAIL) {
            LOGERR("Matching sound banks file not found.");
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (!setup_heap_manager(sound_heap_memory, sz, locstr, n))
            ingame.Flags &= ~GamF_Unkn00020000;
        sound_heap_size = sz;
        break;
    case SHSC_GameSndBestQ:
        sz = 1500000;
        if (sound_banks_fname(locstr, pinfo->directory, lang) == Lb_FAIL) {
            LOGERR("Matching sound banks file not found.");
            ingame.Flags &= ~GamF_Unkn00020000;
            break;
        }
        if (!setup_heap_manager(sound_heap_memory, sz, locstr, 1622))
            ingame.Flags &= ~GamF_Unkn00020000;
        sound_heap_size = sz;
        break;
    case SHSC_ResetGameSnd:
    default:
        sz = 1500000;
        reset_heaps();
        LbMemoryFree(sound_heap_memory);
        sound_heap_size = sz;
        break;
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void heapmgr_defrag_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_heapmgr_defrag\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Closes one gap in the heap, by moving a single item down.
 * a1 is the heap header; walks the address-ordered item list from 0x18
 * through the next links (0x10). The first item whose data (pointer at
 * 0x00) starts above the end of the previous item (0x00 + size at 0x04),
 * and which is not pinned (flag 0x04 at 0x08), has its data moved to
 * close the gap; then stops.
 */
void heapmgr_defrag(int a1, int a2, int a3, int a4)
{
    struct HeapMgrHeader *p_hmhead = (struct HeapMgrHeader *)a1;
    ubyte *p_item;
    ubyte *p_next;
    u32 prev_end;

    DIFFTEST_SHADOW(heapmgr_defrag, heapmgr_defrag_asm(a1, a2, a3, a4));
    if (p_hmhead == NULL)
        return;
    if (p_hmhead->field_10 == 0)
        return;
    p_item = (ubyte *)p_hmhead->field_18;
    if (p_item == NULL)
        return;
    while (1)
    {
        p_next = (ubyte *)*(s32 *)(p_item + 0x10);
        if (p_next == NULL)
            return;
        prev_end = *(u32 *)(p_item + 0x00) + *(u32 *)(p_item + 0x04);
        if ((prev_end < *(u32 *)(p_next + 0x00)) &&
          ((*(p_next + 0x08) & 0x04) == 0))
        {
            memmove((void *)(*(u32 *)(p_item + 0x00) + *(u32 *)(p_item + 0x04)),
              (void *)*(u32 *)(p_next + 0x00), *(u32 *)(p_next + 0x04));
            *(u32 *)(p_next + 0x00) = *(u32 *)(p_item + 0x00) + *(u32 *)(p_item + 0x04);
            return;
        }
        p_item = p_next;
    }
}


static DIFFTEST_REF_FUNC void process_sound_heap_asm(void)
{
    asm volatile ("call ASM_process_sound_heap\n"
        :  :  : "eax", "cc", "memory" );
}

/** Refreshes the lock flags (0x06 at 0x08) of heap handles: clears them for
 * all samples, then sets them again for samples still playing. Samples which
 * stopped get their UserFlag bit 0x01 cleared. Then defragments the heap.
 */
void process_sound_heap(void)
{
    struct SampleInfo *p_sinfo;
    struct SampleInfo *p_last;
    struct HeapMgrHandle *p_hmhandle;
    s32 i;

    DIFFTEST_SHADOW(process_sound_heap, process_sound_heap_asm());
    if ((ingame.Flags & GamF_Unkn00020000) == 0)
        return;
    for (i = 0; i < samples_in_bank; i++)
    {
        p_hmhandle = sample_table[i].hmhandle;
        if (p_hmhandle != NULL)
            p_hmhandle->field_0[8] &= ~0x06;
    }
    p_last = GetLastSampleInfoStructure();
    for (p_sinfo = GetFirstSampleInfoStructure(); p_sinfo <= p_last; p_sinfo++)
    {
        if (p_sinfo->SampleHandle == NULL)
            continue;
        if ((p_sinfo->UserFlag & 0x01) == 0)
            continue;
        if (!IsSamplePlaying(0, 0, p_sinfo->SampleHandle)) {
            p_sinfo->UserFlag &= ~0x01;
            continue;
        }
        p_hmhandle = sample_table[p_sinfo->SampleNumber].hmhandle;
        if (p_hmhandle != NULL)
            p_hmhandle->field_0[8] |= 0x06;
    }
    heapmgr_defrag((int)hmhead, 0, 0, 0);
}

/******************************************************************************/
