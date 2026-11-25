/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_state.c
 *     Global state of the game core.
 * @par Purpose:
 *     Definitions of global variables the game core uses, which were
 *     defined in files of the front end (shell, menus, panels, renderer).
 * @par Comment:
 *     Moved unchanged; each group names the file it came from.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfconfig.h"
#include "bftypes.h"
#include "bfscreen.h"

#include "febrief.h"
#include "femail.h"
#include "game.h"
#include "game_options.h"
#include "guitext.h"
#include "hud_panel.h"
#include "player.h"
#include "feequip.h"
#include "hud_target.h"
#include "scandraw.h"
/******************************************************************************/

/* from game_options.c */
struct InGame ingame;
ubyte net_game_play_flags = 0;

/* from game.c */
char session_name[20] = "SWARA";
char user_name[50] = "TEMP";
ushort word_1531E0 = 1;
ubyte mouser = 0;
int mouse_map_x = 0x3200;
int mouse_map_y = 0;
int mouse_map_z = 0x3200;
struct LoginControl login_control[4];
struct LevelDef level_def;
sbyte mission_result;
ubyte net_player_teams[8];
ubyte group_factions[8];
s32 navi2_unkn_counter = 0;
s32 navi2_unkn_counter_max = 0;
ubyte exit_game = 0;
ubyte in_network_game = 0;
ubyte is_single_game = 0;
ubyte cmdln_colour_tables = 0;
ubyte cmdln_param_bcg = 0;

/* from game_save.c */
char login_name[16] = "";

/* from femail.c */
struct EmailItem brief_store[BRIEF_STORE_COUNT];

/* from febrief.c */
short next_brief = 0;
short open_brief = 0;

/* from hud_panel.c */
ubyte byte_153198 = 1;
char player_message_text[PLAYERS_LIMIT][128];
ubyte player_message_timer[PLAYERS_LIMIT];

/* from lvdraw3d.c */
TbBool nuclear_overexposure = false;

/* from tngobjdrw.c */
ubyte byte_1C83E4 = 0;

/* from scandraw.c */
#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* typed asm data; difftest builds take it from swars_data.c */
TbPixel SCANNER_people_colours[15];
ubyte SCANNER_colour[5];
#endif
ubyte net_player_colours[8];

/* from guitext.c */
char *gui_strings[STRINGS_MAX];

/* from display.c */
ubyte game_high_resolution = 0;

/* from hud_target.c */
TbBool hud_show_target_health = false;

/* from feequip.c */
ubyte refresh_equip_list = 0;

/* from specblit.c: a table of libswrender (app_sprite.h), which the
 * core's links pull in with the HUD drawing lists; builds with the asm
 * take it from the asm data (swars_data.c) */
#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
ubyte low_trans_grey_bright_limit[256];
#endif

/******************************************************************************/
