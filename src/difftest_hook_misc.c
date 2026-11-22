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
 *     Tokens: m_fissure, m_bat, m_cryo, m_typing, and m_pause (the sound
 *     side of opening the pause screen, every 50 turns).
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

#if defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST

#include <string.h>
#include "bfkeybd.h"
#include "bftext.h"
#include "bfsprite.h"
#include "bmbang.h"
#include "femain.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "guiboxes.h"
#include "keyboard.h"
#include "player.h"
#include "research.h"
#include "sound.h"
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
void BAT_unknsub_28(int a1, int a2);
#define HK_BAT_COLUMNS 12
#define HK_BAT_ROWS 10
#define HK_BATSt_Gameplay 2

/* Campaign roster */
extern struct ScreenTextBox cryo_agent_list_box;
void add_agent(ulong weapons, ushort mods);
void remove_agent(ubyte cryo_no);

/* Buffered keyboard input (keyboard.c) */
#define HK_KEYBOARD_BUFFER_SIZE 16
extern ulong buffered_keys[HK_KEYBOARD_BUFFER_SIZE];
extern ulong buffered_keys_read_index;
extern ulong buffered_keys_write_index;

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

/** Suffix m_cryo: every 16 turns a copy of a player agent is added to the
 * cryo roster (as for a persuaded agent after a mission), then one roster
 * entry is removed (as for a dead agent). The roster and everything the
 * two calls touch are restored afterwards.
 */
static void hook_cryo_turn(void)
{
    static ushort n = 0;
    static struct AgentInfo saved_cryo;
    static struct ScreenTextBox saved_box;
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    ulong saved_wep_allowed;
    ubyte saved_mission_agents;
    ubyte cryo_no;

    if ((gameturn & 15) != 5)
        return;
    p_locplayer = &players[local_player_no];
    p_agent = p_locplayer->MyAgent[n % 4];
    if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
        return;
    if (cryo_agents.NumAgents >= 32)
        return;

    memcpy(&saved_cryo, &cryo_agents, sizeof(struct AgentInfo));
    memcpy(&saved_box, &cryo_agent_list_box, sizeof(struct ScreenTextBox));
    saved_wep_allowed = research.WeaponsAllowed;
    saved_mission_agents = p_locplayer->MissionAgents;

    add_agent(p_agent->U.UPerson.WeaponsCarried, p_agent->U.UPerson.UMod.Mods);
    // Remove alternately the new agent and an existing one
    if ((n & 1) == 0)
        cryo_no = cryo_agents.NumAgents - 1;
    else
        cryo_no = (n >> 1) % cryo_agents.NumAgents;
    remove_agent(cryo_no);
    n++;

    memcpy(&cryo_agents, &saved_cryo, sizeof(struct AgentInfo));
    memcpy(&cryo_agent_list_box, &saved_box, sizeof(struct ScreenTextBox));
    research.WeaponsAllowed = saved_wep_allowed;
    p_locplayer->MissionAgents = saved_mission_agents;
}

/** Suffix m_typing: types a fixed key sequence into a text field the way
 * the front-end does (save slot name, net entries): a key is buffered every
 * 4 turns and user_read_value() polls the buffer every second turn. The
 * field type changes after each Enter.
 */
static void hook_typing_turn(void)
{
    static const ubyte keys[] = {
        KC_S, KC_Y, KC_N, KC_D, KC_1, KC_SPACE, KC_F, KC_9, KC_MINUS,
        KC_BACK, KC_W, KC_A, KC_R, KC_S, KC_C, KC_2, KC_E, KC_RETURN,
    };
    static const ubyte max_len[] = {23, 15, 4, 14};
    static char buf[32];
    static ushort n = 0;
    static ubyte type = 0;
    ubyte key, saved_shift;

    if ((gameturn & 1) != 0)
        return;
    if ((show_alert != 0) || (lbFontPtr == NULL))
        return;
    key = 0;
    if ((gameturn & 3) == 0)
    {
        ulong new_write;

        key = keys[n % sizeof(keys)];
        n++;
        // As add_key_to_buffer()
        buffered_keys[buffered_keys_write_index] = key;
        new_write = (buffered_keys_write_index + 1) % HK_KEYBOARD_BUFFER_SIZE;
        if (new_write != buffered_keys_read_index)
            buffered_keys_write_index = new_write;
        if (key == KC_BACK)
            lbKeyOn[KC_BACK] = 1;
    }
    saved_shift = lbShift;
    // Every third pass through the sequence types with shift held
    if (((n / sizeof(keys)) % 3) == 2)
        lbShift |= 0x01;
    user_read_value(buf, max_len[type], type);
    lbShift = saved_shift;
    lbKeyOn[KC_BACK] = 0;
    if (key == KC_RETURN)
    {
        memset(buf, 0, sizeof(buf));
        type = (type + 1) % 4;
    }
}

/** What start_pause_screen() does to the sound: playing samples are muted
 * (snd_unkn1_volume_all_samples()). Only sound state changes; the game
 * goes on unpaused.
 */
static void hook_pause_turn(void)
{
    if ((gameturn % 50) == 25)
        snd_unkn1_volume_all_samples();
}

void difftest_hook_misc_turn(void)
{
    static int on_fissure = -1, on_bat, on_cryo, on_typing, on_pause;

    if (on_fissure < 0) {
        on_fissure = difftest_suffix_has("m_fissure");
        on_bat = difftest_suffix_has("m_bat");
        on_cryo = difftest_suffix_has("m_cryo");
        on_typing = difftest_suffix_has("m_typing");
        on_pause = difftest_suffix_has("m_pause");
    }
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if (on_fissure)
        hook_fissure_turn();
    if (on_bat)
        hook_bat_turn();
    if (on_cryo)
        hook_cryo_turn();
    if (on_typing)
        hook_typing_turn();
    if (on_pause)
        hook_pause_turn();
}

#else

void difftest_hook_misc_turn(void)
{
}

#endif
/******************************************************************************/
