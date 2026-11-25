/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_coreinit.h
 *     Header file for game_coreinit.c.
 * @par Purpose:
 *     Setting up the game core and a mission, without the front end.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef GAME_COREINIT_H
#define GAME_COREINIT_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Set explosion detail parameters; detail=0 high detail, detail=1 low detail.
 */
void bang_set_detail(int detail);

void load_prim_quad(void);

/** Number of texture atlas pages in the game data (data/texNN.dat). */
int existing_texturemaps(void);

/** Whether the game data has the texture atlas page; the levels' textures
 * on other pages are moved to page 0 when a level is loaded. Before, this
 * asked whether the software renderer had loaded the page, which it does
 * for every page of the game data.
 */
TbBool texturemap_page_exists(ushort page);

/** The game setup steps of the core, in the order game_setup() (game.c)
 * calls them between its front-end steps; a front end without screens
 * calls them in a row (game_core_setup()).
 *
 * The first: the engine memory, the animation data, the rules and config
 * files, explosions and fire.
 */
TbBool game_core_setup_data(void);

/** The second: the game's memory (things, map, objectives). */
TbBool game_core_setup_memory(void);

/** The third: things, people stats, campaigns, vehicle objects and the
 * starting camera.
 */
void game_core_setup_world(void);

/** All core setup steps in a row, for a front end without screens. */
TbBool game_core_setup(void);

/** Frees what game_core_setup_data() loaded. */
void game_core_free_data(void);

/** Sets the game's view from the screen size (lbDisplay.GraphicsScreen*):
 * the camera scale (overall_scale) and the area of map tiles around the
 * camera that is viewed (render_area_a/b). Game state: the view pass sets
 * the face flags shots and line of sight read for the vehicles in view.
 */
void game_core_view_setup(void);

/** Loads the missions of the campaign (background_type) and their texts;
 * ingame.CurrentMission is the mission to play.
 */
void game_core_mission_load(void);

/** Sets up the mission's map, level and player, and plays the turns the
 * mission is preprocessed for. A replay reads its player setup between
 * game_core_mission_load() and this.
 */
void game_core_mission_init(void);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
