/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file bfscrsurf.h
 *     Header file for sscrsurf.c.
 * @par Purpose:
 *     Graphics surfaces support.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     10 Feb 2010 - 30 Sep 2010
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef BFLIBRARY_BFSCRSURF_H_
#define BFLIBRARY_BFSCRSURF_H_

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct TbRect;

enum TbSSurfaceBlitFlags {
    SSBlt_NONE = 0x00,
    SSBlt_FLAG1 = 0x01,
    SSBlt_FLAG2 = 0x02,
    SSBlt_TRANSPRN = 0x04,	/*< Blit minding transparency colour. */
    SSBlt_TO_SCREEN = 0x08,	/*< Blit to screen; leave unset to blit from screen. */
    SSBlt_FLAG10 = 0x10,
};

typedef void * OSSurfaceHandle;

struct SSurface {
    OSSurfaceHandle surf_data;
    u32 locks_count;
    s32 pitch;
};

/******************************************************************************/

/** Internal screen surface structure. */
extern OSSurfaceHandle lbScreenSurface;

/** Internal drawing surface structure.
 *  Sometimes may be same as screen surface. */
extern OSSurfaceHandle lbDrawSurface;

/** Initialize screen sufrace struct before first use.
 */
void LbScreenSurfaceInit(struct SSurface *surf);

/** Create a screen surface of given dimensions.
 *
 * Screen surface is a graphics surface which should represent part of the screen.
 * It is created in the same format as current working screen, and its dimensions
 * should not exceed the size of that screen. It can only be created after working
 * screen has been initialized.
 *
 * Screen surfaces are a way for optimized compositing when the application screen
 * consists of several independently generated parts.
 */
TbResult LbScreenSurfaceCreate(struct SSurface *surf, u32 w, u32 h);

TbResult LbScreenSurfaceRelease(struct SSurface *surf);

/** Blits given surface on rectangular area of the working screen, or from it.
 *
 *  Copies pixels 1:1, without any scaling.
 *  Performs validation and clipping before copying image data.
 *  The blit function should not be called on a locked surface.
 *
 * @param surf Surface to blit from (or to blit onto, depending on flags).
 * @param x Screen X coordintate to start blitting to (or from, see flags).
 * @param y Screen Y coordintate to start blitting to (or from, see flags).
 * @param rect Coordinates of rectangular area within the given surface.
 * @param blflags Flags selecting type and direction of blitting.
 *
 * @return Gives Lb_SUCCESS if blitting was performed correctly.
 */
TbResult LbScreenSurfaceBlit(struct SSurface *surf, u32 x, u32 y,
    struct TbRect *rect, u32 blflags);

/** Blits between rectangular area of screen and of given surface.
 *
 *  Scales the pixel data to match destination area.
 *  Performs validation and clipping before copying image data.
 *  The blit function should not be called on a locked surface.
 *
 * @param surf Surface to blit from (or to blit onto, depending on flags).
 * @param scrn_rect Coordinates of rectangular screen area to start blitting to (or from, see flags).
 * @param surf_rect Coordinates of rectangular area within the given surface.
 * @param blflags Flags selecting type and direction of blitting.
 *
 * @return Gives Lb_SUCCESS if blitting was performed correctly.
 */
TbResult LbScreenSurfaceBlitScaled(struct SSurface *surf, struct TbRect *scrn_rect,
    struct TbRect *surf_rect, u32 blflags);

/** Locks the surface, allowing direct access to its pixel data buffer.
 */
void *LbScreenSurfaceLock(struct SSurface *surf);

/** Unlocks the surface, invalidating pixel data buffer.
 *  After this operation, the pointer acquired through locking should no longer
 *  be accessed.
 */
TbResult LbScreenSurfaceUnlock(struct SSurface *surf);

#ifdef __cplusplus
}
#endif
#endif // BFLIBRARY_BFSCRSURF_H_
/******************************************************************************/
