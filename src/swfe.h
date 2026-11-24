/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swfe.h
 *     Services the game core needs from its front end.
 * @par Purpose:
 *     The game core (libswarscore) calls these functions; every front end
 *     provides them. The SDL desktop front end (libswarsfe) implements them
 *     with the original sound, panels and renderer; swfe_null.c implements
 *     them as nothing, for front ends that only read the world through
 *     swcore.h (swcore_headless, and later VR and mobile front ends).
 * @par Comment:
 *     None of these functions may change the game state: the game must play
 *     the same with any front end. They only show or play what happened.
 *     The functions keep the names they had in the original game, so that
 *     the asm reference of the differential tests still finds them.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SWFE_H
#define SWFE_H

#include "bftypes.h"
#include "game_bstype.h"
#include "bfkeybd.h"
#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct Thing;
struct SimpleThing;
struct SampleInfo;

/* Sound (sound.c) */
void play_dist_sample(struct Thing *p_thing, ushort smptbl_id, ushort vol,
  ushort pan, int pitch, int loop, ubyte type);
int play_dist_speech(struct Thing *p_thing, ushort samp, ushort vol,
  ushort pan, int pitch, int loop, ubyte type);
void play_dist_ssample(struct SimpleThing *p_sthing, ushort smptbl_id,
  ushort vol, ushort pan, int pitch, int loop, ubyte type);
void play_disk_sample(short id, ushort sample, short vol, short pan,
  int pitch, int loop, int type);
struct SampleInfo *play_sample_using_heap(u32 bank_id, short smptbl_id,
  u32 volume, u32 pan, u32 pitch, sbyte loop_count, ubyte stype);
void stop_sample_using_heap(s32 source_id, u32 sample_number);
void stop_looped_weapon_sample(struct Thing *p_person, short weapon);
/** Updates the volume and pan of playing samples from the positions of
 * their sources. */
void monitor_all_samples(void);
/** Whether the sample started by the given source is still playing. The
 * game only uses it to decide whether to start a sound again. */
TbBool swfe_sample_is_playing(ThingIdx source_id, ushort sample_number);
/** Music following the danger level of the mission. */
void update_danger_music(ubyte a1);

/* Messages and panels (hud_panel.c, hud_target.c, drawtext_wrp.c) */
void show_message(const char *text);
void player_message_fmt(PlayerIdx plyr, const char *fmt, ...);
void player_chat_clear(void);
void show_goto_point(u32 flag);
void init_scanner(void);
TbBool panel_any_visible(void);
/** The number of agents in cryo changed (cryo_agents.NumAgents). */
void swfe_cryo_agents_changed(void);
/** Research progressed; the research screen redraws its lists. */
void clear_research_screen(void);
/** Number of lines the text takes on screen (it depends on the font). */
ushort my_count_lines(const char *text);

/* Replays (packetfe.c) */
/** Writes the player setup at the start of a recorded replay. */
void packet_write_whole_player_init(void);

/* Input (keyboard.c, plyr_usrinp.c). The core still reads a few keys
 * directly: debug and cheat keys, the game speed keys and the restart
 * key. A front end without a keyboard reports none pressed. Planned to
 * become commands. */
ubyte is_key_pressed(TbKeyCode key, TbKeyMods kmodif);
void clear_key_pressed(TbKeyCode key);
ubyte is_gamekey_pressed(GameKey gkey);
void clear_gamekey_pressed(GameKey gkey);
/** Sets up which local user controls with the mouse. */
void init_user_input_local_controls(void);

/* Display (display.c) */
void change_brightness(short val);
void set_user_selected_brightness(void);
/** A light flash on the floor around the given map coordinates (flames). */
void apply_super_quick_light(short lx, short lz, ushort b);

/* Debug drawing (scandraw.c, drawtext_wrp.c, hud_draw.c); only called
 * when a debug option is on. */
void draw_map_flat_circle(short cor_x, short cor_y_m8, short cor_z,
  short radius, TbPixel colour);
void draw_map_flat_rect(short cor_x, short cor_y_m8, short cor_z,
  short size_x, short size_z, TbPixel colour);
void draw_objective_point(s32 x, s32 y, ThingIdx thing, short a4,
  ubyte colour);
void draw_mapcoord_line_at_ground(int cor1_x, int cor1_z,
  int cor2_x, int cor2_z, TbPixel colour);
void draw_mapcoord_simple_text_at_ground(int cor_x, int cor_z,
  const char *text);
void draw_mapcoord_simple_text_number_at_ground(int cor_x, int cor_z,
  sbyte ln, int num);
void draw_mapcoord_line_directed_to_target(int probe_x, int probe_y,
  int probe_z, int target_x, int target_y, int target_z, TbPixel colour);
TbBool enlist_hud_draw_line_from_thing_to_mapcoord(ThingIdx thing,
  short cor2_x, short cor2_y_m8, short cor2_z, ushort drwflags, ubyte colkp);
TbBool enlist_hud_draw_line_from_thing_ptr_to_mapcoord(struct Thing *p_thing,
  short cor2_x, short cor2_y_m8, short cor2_z, ushort drwflags, ubyte colkp);
void draw_mark_over_tile_with_number(short tl_x, short tl_z, sbyte ln,
  int num);
void count_fnavs(TbBool now_draw);
void navi_onscreen_debug(TbBool a1);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
