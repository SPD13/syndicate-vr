/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_camera.h
 *     Header file for game_camera.c.
 * @par Purpose:
 *     The game's camera, which follows the player's agents.
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
#ifndef GAME_CAMERA_H
#define GAME_CAMERA_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** How much and how fast to rotate, tilt and zoom the camera. */
#define CAMERA_TILT_MIN -192
#define CAMERA_TILT_MAX -152
#define CAMERA_ZOOM_MIN 120
#define CAMERA_ZOOM_MAX 256
#define CAMERA_ROTATION_INPUT_MULTIPLIER 256
#define CAMERA_TILT_INPUT_MULTIPLIER 4

extern s32 dword_153194;

void move_camera(int x, int y, int z);
void track_player(int thing);
void track_angle(void);
void track_y(int y);
void scroll_map_update_alt(void);
void process_overall_scale(int zoom);
void process_view_inputs(int thing);

TbBool game_camera_scroll_allowed(void);
void game_camera_update(int scroll_dx, int scroll_dy);
void game_camera_input(short rotate_input, short zoom_input, short tilt_input);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
