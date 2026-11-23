/******************************************************************************/
// Bullfrog Ariadne Pathfinding Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet, Genewars or Dungeon Keeper.
/******************************************************************************/
/** @file trstate.c
 *     Triangulation state declaration and support.
 * @par Purpose:
 *     Implement functions for handling whole triangulation states.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     29 Sep 2023 - 02 Jan 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "triangls.h"

#include <limits.h>
#include <stdlib.h>
#include "difftest.h"
#include "trstate.h"
#include "trlog.h"

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
int selected_triangulation_no = -1;
#endif /* typed asm data */
/******************************************************************************/

struct Triangulation triangulation[TRIANGULATIONS_COUNT];
int triangulation_initied = 0;

/******************************************************************************/

static void triangulation_init_single(int trglno)
{
    struct Triangulation *p_trgl;
    p_trgl = &triangulation[trglno];

    p_trgl->tri_allocated = 0;
    p_trgl->tri_initialised = 0;
    p_trgl->last_tri = -1;
    p_trgl->ix_Triangles = 0;
    p_trgl->count_Triangles = 0;
    p_trgl->free_Triangles = -1;
    p_trgl->triangle_top = 0;
    p_trgl->max_Triangles = 0;
    p_trgl->Triangles = 0;
    p_trgl->ix_Points = 0;
    p_trgl->count_Points = 0;
    p_trgl->free_Points = -1;
    p_trgl->point_top = 0;
    p_trgl->max_Points = 0;
    p_trgl->Points = 0;
}

void triangulation_initialize(void)
{
    int n;

    for (n = 0; n < TRIANGULATIONS_COUNT; n++)
    {
        triangulation_init_single(n);
    }

    triangulation_initied = 1;
}

static DIFFTEST_REF_FUNC void triangulation_select_asm(int trglno)
{
    DIFFTEST_ASM("call ASM_triangulation_select\n"
        : : "a" (trglno));
}

void triangulation_select(int trglno)
{
    DIFFTEST_SHADOW(triangulation_select, triangulation_select_asm(trglno));
    if (selected_triangulation_no == trglno)
        return;
    // The original also formats a debug message "tri id %d=%d triangulation_select"
    // with trglno and the first field of triangulation[trglno] into a local
    // buffer which is never used; skipped.
    // Store the working copy back into the previously selected slot
    if (selected_triangulation_no != -1)
        triangulation[selected_triangulation_no] = triangulation[0];
    selected_triangulation_no = trglno;
    // Load the newly selected triangulation into the working slot 0
    triangulation[0] = triangulation[trglno];
}

void triangulation_clear_enter_into_solid(void)
{
    ubyte seltr;

    seltr = selected_triangulation_no;

    triangulation_select(1);
    triangulation_clear_enter_into_solid_gnd();

    triangulation_select(2);
    triangulation_clear_enter_into_solid_air();

    triangulation_select(seltr);
}

/******************************************************************************/
