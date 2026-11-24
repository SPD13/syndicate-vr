/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_view.c
 *     Game state the original updated while drawing the things in view.
 * @par Purpose:
 *     The original drawing of the things in view of the camera also
 *     changed the game: the flags of the faces of moving objects, which
 *     line of sight and walking read, and the positions of the mech parts.
 *     game_view_pass() does that without drawing, visiting the things the
 *     drawing (engine_draw_things()) visits, so that the game runs the
 *     same with any renderer, or none.
 * @par Comment:
 *     The view depends on the camera and the render area, as in the
 *     original. The thing under the pointer is still found by the drawing.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_view.h"

#include "bfutility.h"

#include "engincam.h"
#include "enginsngobjs.h"
#include "enginzoom.h"

#include "bigmap.h"
#include "game_speed.h"
#include "matrix.h"
#include "thing.h"
#include "vehicle.h"
/******************************************************************************/

/** Set only by the asm, to hide things existing off the map. */
extern ubyte byte_1C83E4;

void thing_view_object_faces(struct SingleObject *p_sobj, struct Thing *p_thing,
  short cor_x, short cor_z)
{
    ushort faceWH, faceGF;
    int i, face;

    faceGF = 0;
    if ((p_thing->Type != TT_UNKN35) && (p_thing->SubType != SubTT_VEH_TRAIN))
    {
        ushort darken;

        darken = 9;
        if (cor_x < TILE_TO_MAPCOORD(8, 0))
            darken = min(darken, cor_x >> 7);
        else if (cor_x > TILE_TO_MAPCOORD(MAP_TILE_WIDTH - 8, 0))
            darken = min(darken, (MAP_COORD_WIDTH - cor_x) >> 7);
        else if (cor_z < TILE_TO_MAPCOORD(8, 0))
            darken = min(darken, cor_z >> 7);
        else if (cor_z > TILE_TO_MAPCOORD(MAP_TILE_HEIGHT - 8, 0))
            darken = min(darken, (MAP_COORD_HEIGHT - cor_z) >> 7);

        if (darken < 9)
        {
            if (darken > 7)
                darken = 7;
            if (darken <= 0)
                darken = 1;
            faceGF |= (darken << 2);
        }
    }

    if ((p_thing->Flag & TngF_Unkn01000000) != 0)
    {
        p_thing->Flag &= ~TngF_Unkn01000000;
        faceWH = 11 - (gameturn & 3);
    }
    else
    {
        faceWH = 0;
    }

    // TODO the original stopped updating the faces of an object at the
    // first face the full draw list could not take; here all are updated
    face = p_sobj->StartFace;
    for (i = 0; i < p_sobj->NumbFaces; i++, face++)
    {
        struct SingleObjectFace3 *p_face;

        p_face = &game_object_faces3[face];
        p_face->GFlags &= ~(FGFlg_Unkn10|FGFlg_Unkn08|FGFlg_Unkn04);
        p_face->GFlags |= faceGF;
        p_face->WalkHeader = faceWH;
    }
    face = p_sobj->StartFace4;
    for (i = 0; i < p_sobj->NumbFaces4; i++, face++)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[face];
        p_face4->GFlags &= ~(FGFlg_Unkn10|FGFlg_Unkn08|FGFlg_Unkn04);
        p_face4->GFlags |= faceGF;
        p_face4->WalkHeader = faceWH;
    }
}

/** The mounted gun of a tank, as process_child_object() places it. */
static void view_vehicle_child_object(struct Thing *p_vehicle)
{
    struct Thing *p_mgun;
    struct M33 *p_matx;
    struct M31 vec1, vec2;
    short vhtng_x, vhtng_y, vhtng_z;

    get_thing_position_mapcoords(&vhtng_x, &vhtng_y, &vhtng_z, p_vehicle->ThingOffset);
    p_mgun = &things[p_vehicle->U.UVehicle.SubThing];
    // the mounted gun position is relative to the vehicle
    vec2.R[0] = PRCCOORD_TO_MAPCOORD(p_mgun->X);
    vec2.R[1] = p_mgun->Y >> 4;
    vec2.R[2] = PRCCOORD_TO_MAPCOORD(p_mgun->Z);
    p_matx = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    matrix_transform(&vec1, p_matx, &vec2);

    thing_view_object_faces(&game_objects[p_mgun->U.UMGun.Object], p_mgun,
      vhtng_x + (vec1.R[0] >> 15), vhtng_z + (vec1.R[2] >> 15));
}

/** What drawing a vehicle (build_vehicle()) changed in the game. */
static void view_vehicle(struct Thing *p_thing)
{
    if (((p_thing->Flag2 & TgF2_ExistsOffMap) != 0) && (byte_1C83E4 & 0x01) != 0)
        return;
    if (p_thing->SubType == SubTT_VEH_SHUTTLE_POD)
        return;

    if (p_thing->SubType == SubTT_VEH_MECH)
    {
        if ((p_thing->Flag & TngF_Destroyed) == 0)
            mech_view_update(p_thing);
    }
    else
    {
        thing_view_object_faces(&game_objects[p_thing->U.UVehicle.Object], p_thing,
          PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_MAPCOORD(p_thing->Z));
    }

    if ((p_thing->U.UVehicle.SubThing != 0) && (p_thing->SubType == SubTT_VEH_TANK))
        view_vehicle_child_object(p_thing);
}

void game_view_pass(void)
{
    int pos_beg_x, pos_beg_z, rend_beg_x, rend_beg_z, tlcount_x, tlcount_z;
    int view_end_x, view_beg_z, view_beg_x, view_end_z;
    int tlno_x, pos_x;

    camera_setup_view(&pos_beg_x, &pos_beg_z, &rend_beg_x, &rend_beg_z, &tlcount_x, &tlcount_z);

    view_end_x = rend_beg_x + 512;
    view_beg_z = rend_beg_z - 512;
    view_beg_x = rend_beg_x - ((render_area_a << 8) + 512);
    view_end_z = rend_beg_z + ((render_area_b << 8) + 512);

    // The tiles and things engine_draw_things() visits; only vehicles
    // changed the game when drawn (buildings outside the view did not)
    for (tlno_x = 0, pos_x = pos_beg_x; tlno_x < tlcount_x; tlno_x++, pos_x += -256)
    {
        int tlno_z, pos_z;

        for (tlno_z = 0, pos_z = pos_beg_z; tlno_z < tlcount_z; tlno_z++, pos_z += 256)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;

            if (pos_x <= TILE_TO_MAPCOORD(0,0) || pos_x >= MAP_COORD_WIDTH)
                continue;
            if (pos_z <= TILE_TO_MAPCOORD(0,0) || pos_z >= MAP_COORD_HEIGHT)
                continue;
            if (pos_x < view_beg_x || pos_x > view_end_x || pos_z < view_beg_z || pos_z > view_end_z)
                continue;

            p_mapel = &game_my_big_map[MAPCOORD_TO_TILE(pos_x) + MAPCOORD_TO_TILE(pos_z) * MAP_TILE_WIDTH];
            thing = p_mapel->Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if (p_thing->Type == TT_VEHICLE)
                        view_vehicle(p_thing);
                    thing = p_thing->Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
}
/******************************************************************************/
