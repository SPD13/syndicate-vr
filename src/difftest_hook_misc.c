/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_hook_misc.c
 *     Difftest hooks reaching other rarely used code.
 * @par Purpose:
 *     Calls functions no mission reaches by itself, so the difftest can
 *     compare their C remake against the asm. Active only in scenarios
 *     whose suffix names the hook (see difftest_suffix_has()).
 * @par Comment:
 *     Called every game turn from difftest_bot_turn().
 *     Tokens: m_fissure, m_bat; the hooks of front-end code (m_cryo,
 *     m_typing, m_pause) are in difftest_hook_fe.c, called through
 *     difftest_hook_fe_turn, which the desktop front end sets.
 * @author   SyndicateVR decomp agent
 * @date     16 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#if DIFFTEST_HARNESS

#include <string.h>
#include "bfsprite.h"
#include "bmbang.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "player.h"
#include "thing.h"

/******************************************************************************/

/* Crater walkers ("fissures"): unused_func_025() starts one at a tile,
 * unused_func_026() advances walker i one step (stamping a crater and
 * sometimes splitting off a new walker via unused_func_192()).
 * Entries are 10 bytes, starting at map_cratunkn2 (0x1C4704); index 0
 * is the list terminator. The list head is data_1c476b, the next free
 * index is map_crater_last (both bytes).
 */
#pragma pack(1)
struct CraterWalker {
    short X;
    short Y;
    short DX;
    short DY;
    ubyte Next;
    ubyte Prev;
};
#pragma pack()

extern ubyte map_cratunkn2[];
extern ubyte data_1c476b;
extern ubyte map_crater_last;

/* Breakout minigame (bat.c) */
extern int BAT_state;
extern ubyte BAT_bricks[];
void BAT_unknsub_28(int pos_x, int pos_y);
#define HK_BAT_COLUMNS 12
#define HK_BAT_ROWS 10
#define HK_BATSt_Gameplay 2

/******************************************************************************/

/** Suffix m_fissure: every 64 turns a crater walker is started at a fixed
 * tile; every turn each live walker is advanced, as the walker list
 * would be processed by its (lost) caller.
 */
static void hook_fissure_turn(void)
{
    static ushort starts = 0;
    struct CraterWalker *p_walk;
    ubyte i, next;
    int guard;

    p_walk = (struct CraterWalker *)map_cratunkn2;
    if ((gameturn & 63) == 1)
    {
        short x, y, speed;
        // Tiles 32..95, speed 20..35 (step 2 or 3 tiles, crater depth = speed)
        x = 32 + (starts * 37) % 64;
        y = 32 + (starts * 23) % 64;
        speed = 20 + (starts % 4) * 5;
        starts++;
        unused_func_025(x, y, speed);
        return;
    }
    i = data_1c476b;
    for (guard = 0; (i != 0) && (guard < 10); guard++)
    {
        if (i >= 9)
            break;
        next = p_walk[i].Next;
        unused_func_026(i);
        i = next;
    }
}

/** Suffix m_bat: while the Breakout minigame is in play, every turn
 * a bonus may spawn at the centre of a brick still standing, as when the
 * ball destroys it (BAT_unknsub_22()).
 */
static void hook_bat_turn(void)
{
    static ushort n = 0;
    int k, idx;

    if (BAT_state != HK_BATSt_Gameplay)
        return;
    n++;
    for (k = 0; k < HK_BAT_COLUMNS * HK_BAT_ROWS; k++)
    {
        int col, row, left, right, top, bottom;

        idx = (n * 37 + k) % (HK_BAT_COLUMNS * HK_BAT_ROWS);
        if ((BAT_bricks[idx] & 0x80) != 0)
            continue;
        col = idx % HK_BAT_COLUMNS;
        row = idx / HK_BAT_COLUMNS;
        top = (row << 10) + 0x700;
        bottom = (row << 10) + 0xb00;
        left = col << 11;
        right = left + 0x800;
        BAT_unknsub_28((left + right) >> 1, (top + bottom) >> 1);
        return;
    }
}

void difftest_hook_misc_turn(void)
{
    static int on_fissure = -1, on_bat;

    if (on_fissure < 0) {
        on_fissure = difftest_suffix_has("m_fissure");
        on_bat = difftest_suffix_has("m_bat");
    }
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if (on_fissure)
        hook_fissure_turn();
    if (on_bat)
        hook_bat_turn();
    // the hooks of front-end code (m_cryo, m_typing, m_pause)
    if (difftest_hook_fe_turn != NULL)
        difftest_hook_fe_turn();
}

void (*difftest_hook_fe_turn)(void) = NULL;

#else

void difftest_hook_misc_turn(void)
{
}

void (*difftest_hook_fe_turn)(void) = NULL;

#endif
/******************************************************************************/
