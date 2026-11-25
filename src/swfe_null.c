/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swfe_null.c
 *     A front end that shows and plays nothing.
 * @par Purpose:
 *     Implements the services of swfe.h as nothing, for programs that link
 *     the game core without the desktop front end: tests, and front ends
 *     that read the world through swcore.h and draw it themselves.
 * @par Comment:
 *     Not part of libswarscore: the desktop front end has its own versions
 *     of these functions. No sample is ever playing, no key is pressed.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swfe.h"

#include <stddef.h>
/******************************************************************************/

/* Sound */
void play_dist_sample(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte type)
{
}

int play_dist_speech(struct Thing *p_thing, ushort samp, ushort vol,
  ushort pan, int pitch, int loop, ubyte type)
{
    return 0;
}

void play_dist_ssample(struct SimpleThing *p_sthing, ushort smptbl_id,
  ushort vol, ushort pan, int pitch, int loop, ubyte type)
{
}

void play_disk_sample(short id, ushort sample, short vol, short pan,
  int pitch, int loop, int type)
{
}

struct SampleInfo *play_sample_using_heap(u32 bank_id, short smptbl_id,
  u32 volume, u32 pan, u32 pitch, sbyte loop_count, ubyte stype)
{
    return NULL;
}

void stop_sample_using_heap(s32 source_id, u32 sample_number)
{
}

void monitor_all_samples(void)
{
}

TbBool swfe_sample_is_playing(ThingIdx source_id, ushort sample_number)
{
    return false;
}

void swfe_sample_release_looped(ThingIdx source_id, short sample_number)
{
}

void swfe_sample_set_volume(ThingIdx source_id, short sample_number, short volume)
{
}

void swfe_sample_set_pitch(ThingIdx source_id, short sample_number, short pitch)
{
}

void swfe_samples_stop_all(void)
{
}

void swfe_cd_play_track(ushort track)
{
}

void swfe_cd_stop(void)
{
}

void swfe_music_start(int song, ubyte volume)
{
}

void swfe_music_set_volume(int msec, ubyte volume)
{
}

void update_danger_music(ubyte a1)
{
}

/* Messages and panels */
void show_message(const char *text)
{
}

void player_message_fmt(PlayerIdx plyr, const char *fmt, ...)
{
}

void player_chat_clear(void)
{
}

void show_goto_point(u32 flag)
{
}

void init_scanner(void)
{
}

TbBool panel_any_visible(void)
{
    return false;
}

void swfe_cryo_agents_changed(void)
{
}

void clear_research_screen(void)
{
}

ushort my_count_lines(const char *text)
{
    return 1;
}

/* Replays */
void packet_write_whole_player_init(void)
{
}

/* Input */
ubyte is_key_pressed(TbKeyCode key, TbKeyMods kmodif)
{
    return 0;
}

void clear_key_pressed(TbKeyCode key)
{
}

ubyte is_gamekey_pressed(GameKey gkey)
{
    return 0;
}

void clear_gamekey_pressed(GameKey gkey)
{
}

void init_user_input_local_controls(void)
{
}

/* Display */
void swap_wscreen(void)
{
}

void change_brightness(short val)
{
}

void set_user_selected_brightness(void)
{
}

void apply_super_quick_light(short lx, short lz, ushort b)
{
}

/* Debug drawing */
void draw_map_flat_circle(short cor_x, short cor_y_m8, short cor_z,
  short radius, TbPixel colour)
{
}

void draw_map_flat_rect(short cor_x, short cor_y_m8, short cor_z,
  short size_x, short size_z, TbPixel colour)
{
}

void draw_objective_point(s32 x, s32 y, ThingIdx thing, short a4,
  ubyte colour)
{
}

void draw_mapcoord_line_at_ground(int cor1_x, int cor1_z,
  int cor2_x, int cor2_z, TbPixel colour)
{
}

void draw_mapcoord_simple_text_at_ground(int cor_x, int cor_z,
  const char *text)
{
}

void draw_mapcoord_simple_text_number_at_ground(int cor_x, int cor_z,
  sbyte ln, int num)
{
}

void draw_mapcoord_line_directed_to_target(int probe_x, int probe_y,
  int probe_z, int target_x, int target_y, int target_z, TbPixel colour)
{
}

TbBool enlist_hud_draw_line_from_thing_to_mapcoord(ThingIdx thing,
  short cor2_x, short cor2_y_m8, short cor2_z, ushort drwflags, ubyte colkp)
{
    return false;
}

TbBool enlist_hud_draw_line_from_thing_ptr_to_mapcoord(struct Thing *p_thing,
  short cor2_x, short cor2_y_m8, short cor2_z, ushort drwflags, ubyte colkp)
{
    return false;
}

void draw_mark_over_tile_with_number(short tl_x, short tl_z, sbyte ln,
  int num)
{
}

void count_fnavs(TbBool now_draw)
{
}

void navi_onscreen_debug(TbBool a1)
{
}

/* Drawing of the renderer library */
void draw_mapcoord_line_col(int cor1_x, int cor1_y_m8, int cor1_z,
  int cor2_x, int cor2_y_m8, int cor2_z, TbPixel colour)
{
}

void draw_mapcoord_to_scrcoord_line(short scr_x, short scr_y,
  int cor_x, int cor_y_m8, int cor_z, TbPixel colour)
{
}

void draw_simple_text(short x, short y, const char *text, ubyte colour)
{
}

void draw_text_transformed(int cor_x, int cor_y_m8, int cor_z, const char *text)
{
}

TbBool enlist_hud_draw_mapcoord_flat_circle(short cor_x,
  short cor_y_m8, short cor_z, short radius,
  ushort drwflags, ubyte thickness, TbPixel colour)
{
    return false;
}

/******************************************************************************/
