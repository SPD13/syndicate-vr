/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file lvdraw3d.h
 *     Header file for lvdraw3d.c.
 * @par Purpose:
 *     Routines for level and map drawing using 3D rendering.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     24 Dec 2023 - 10 Nov 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef LVDRAW3D_H
#define LVDRAW3D_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)


#pragma pack()
/******************************************************************************/
extern TbBool nuclear_overexposure;
extern ubyte byte_1C8444;

void clear_super_quick_lights(void);
void apply_super_quick_light(short lx, short lz, ushort b);

void lvdraw_do_floor(void);
void engine_draw_whole_screen_flyby(void);

void engine_draw_whole_screen_top_down(void);

void prepare_drawlist(void);

/** Called once the world is drawn into the frame, before the overlays and
 * the panels: the frame as it is then (vec_screen), for a front end that
 * draws the world its own way and keeps the game's 2D layer on top. It only
 * reads the frame. */
typedef void (*LvWorldDrawnHook)(const ubyte *pixels, s32 pitch, s32 width, s32 height);
extern LvWorldDrawnHook lvdraw_world_drawn_hook;
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
