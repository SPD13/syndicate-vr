/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_view.h
 *     Header file for thing_view.c.
 * @par Purpose:
 *     Game state the original updated while drawing the things in view.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef THING_VIEW_H
#define THING_VIEW_H

#include "bftypes.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct Thing;
struct SingleObject;

/** Updates the game state the original drawing updated for the things in
 * view of the camera, in the order the drawing visits them: face flags of
 * moving objects, which line of sight and walking read, and the positions
 * of mech parts. To be called once per frame, before drawing; the
 * renderer then only reads it.
 */
void game_view_pass(void);

/** Sets the flags of the faces of a moving object in view, as drawing it
 * did: the darkening near the map edge in GFlags bits 2-4 (bit 2 makes
 * line of sight ignore the face), and the hit flash in WalkHeader, after
 * which the thing's hit flag is cleared.
 * @param cor_x, cor_z Map coordinates of the object.
 */
void thing_view_object_faces(struct SingleObject *p_sobj, struct Thing *p_thing,
  short cor_x, short cor_z);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
/******************************************************************************/
