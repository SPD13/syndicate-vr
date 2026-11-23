/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file building.c
 *     Buildings related functions.
 * @par Purpose:
 *     Implement functions for handling buildings.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     27 May 2022 - 12 Jun 2023
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "building.h"

#include <assert.h>
#include <stdlib.h>
#include "bfmath.h"
#include "bfmemory.h"
#include "bfutility.h"

#include "enginfexpl.h"
#include "enginprops.h"
#include "enginsngobjs.h"
#include "triangls.h"

#include "ssampply.h"
#include "bigmap.h"
#include "bmbang.h"
#include "bmcrater.h"
#include "difftest.h"
#include "game_options.h"
#include "game_speed.h"
#include "game.h"
#include "matrix.h"
#include "pathtrig.h"
#include "people.h"
#include "pepgroup.h"
#include "sound.h"
#include "thing.h"
#include "thing_expld.h"
#include "tngcolisn.h"
#include "vehicle.h"
#include "vehtraffic.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/
ubyte dome_open_speed = 4;

struct BuildingStat bldng_type_stats[] = {
  {"BLD_NONE",},
  {"BLD_1",},
  {"BLD_2",},
  {"BLD_3",},
  {"BLD_4",},
  {"BLD_5",},
  {"BLD_6",},
  {"BLD_7",},
  {"BLD_8",},
  {"BLD_9",},
  {"BLD_A",},
  {"BLD_B",},
  {"BLD_C",},
  {"BLD_D",},
  {"BLD_E",},
  {"BLD_F",},
  {"BLD_10",},
  {"BLD_11",},
  {"BLD_12",},
  {"BLD_13",},
  {"SHUTTL_LDR",},
  {"BEZIER_ROAD",},
  {"BLD_16",},
  {"BLD_17",},
  {"DOME",},
  {"BLD_19",},
  {"BLD_1A",},
  {"STANDRD BLD",},
  {"TRAIN_TRACK",},
  {"BLD_1D",},
  {"BLD_1E",},
  {"BLD_1F",},
  {"MOUNT_GUN",},
  {"BLD_21",},
  {"BLD_22",},
  {"BLD_23",},
  {"BLD_24",},
  {"BLD_GATE",},
  {"BLD_26",},
  {"BLD_27",},
  {"BLD_28",},
  {"BLD_29",},
  {"BLD_2A",},
  {"BLD_2B",},
  {"BLD_2C",},
  {"BLD_2D",},
  {"BLD_2E",},
  {"TRAIN_STATN",},
  {"BLD_30",},
  {"BLD_31",},
  {"BLD_32",},
  {"BLD_33",},
  {"BLD_34",},
  {"BLD_35",},
  {"MOVN_ROTOR",},
  {"BLD_37",},
  {"BLD_38",},
  {"BLD_39",},
  {"BLD_3A",},
  {"BLD_3B",},
  {"BLD_3C",},
  {"BILLBOARD",},
  {"BLD_3E",},
  {"BLD_3F",},
  {"BLD_40",},
  {"BLD_41",},
  {"BLD_42",},
  {"BLD_43",},
  {"BLD_44",},
};


const char *building_type_name(ushort btype)
{
    struct BuildingStat *p_bldstat;

    p_bldstat = &bldng_type_stats[btype];
    if (strlen(p_bldstat->Name) == 0)
        return "OUTRNG_BLDNG";
    return p_bldstat->Name;
}

TbBool building_can_transform_open(ThingIdx bldng)
{
    struct Thing *p_building;

    p_building = &things[bldng];
    if (p_building->Type != TT_BUILDING)
        return false;
    if ((p_building->Flag & TngF_Destroyed) != 0)
        return false;
    return (p_building->SubType == SubTT_BLD_DOME);
}

static DIFFTEST_REF_FUNC struct Thing *create_building_thing_asm(int x, int y,
  int z, ushort obj, ushort nobj, ushort a6)
{
    struct Thing *ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)nobj;
    stkargs[1] = (int)(intptr_t)a6;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_create_building_thing\n"
        : "=a" (ret)
        : "a" (x), "d" (y), "b" (z), "c" (obj), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

int get_new_thing_with_index(ThingIdx thing);

struct Thing *create_building_thing(int x, int y, int z, ushort obj, ushort nobj, ushort a6)
{
    struct Thing *p_thing;
    ushort thing;
    ushort i;

    DIFFTEST_SHADOW_RET(create_building_thing, struct Thing *,
      create_building_thing_asm(x, y, z, obj, nobj, a6));
    thing = a6;
    if (((short)a6 < 0) || ((ushort)get_new_thing_with_index(a6) == 0))
        thing = get_new_thing();

    p_thing = &things[thing];
    p_thing->Type = TT_BUILDING;
    p_thing->X = x << 8;
    p_thing->Z = z << 8;
    p_thing->U.UObject.Angle = 0;
    p_thing->Y = y;
    p_thing->U.UObject.Object = obj;
    if ((nobj == 0) || (nobj > 250))
        nobj = 1;
    p_thing->U.UObject.NumbObjects = nobj;
    p_thing->Health = 8024;
    for (i = 0; i < nobj; i++)
        game_objects[(ushort)obj + i].ThingNo = thing;

    add_node_thing(thing);
    DIFFTEST_RETURN(p_thing);
}

void do_dome_rotate1(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_do_dome_rotate1\n"
        : : "a" (p_building));
#endif
    ushort obj, i;
    ushort pt1_beg, pt1_num;

    obj = p_building->U.UObject.Object;
    pt1_beg = game_objects[obj].StartPoint;
    pt1_num = game_objects[obj].EndPoint - pt1_beg;
    for (i = obj + 1; i < obj + 8; i++)
    {
        int factrS, factrC;
        short angle;
        ushort pt2_beg;
        ushort dpt;

        angle = (i - obj) * p_building->SubState;
        pt2_beg = game_objects[i].StartPoint;
        factrC = lbSinTable[angle + LbFPMath_PI/2];
        factrS = lbSinTable[angle];
        for (dpt = 0; dpt <= pt1_num; dpt++)
        {
            struct SinglePoint *p_point1;
            struct SinglePoint *p_point2;
            p_point1 = &game_object_points[pt1_beg + dpt];
            p_point2 = &game_object_points[pt2_beg + dpt];
            p_point2->Y = (factrC * p_point1->Y - factrS * p_point1->Z) >> 16;
            p_point2->Z = (factrC * p_point1->Z + factrS * p_point1->Y) >> 16;
        }
    }
}

void process_dome1(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_dome1\n"
        : : "a" (p_building));
#endif
    uint timer0;

    switch (p_building->State)
    {
    case BldSt_TRA_OPENING:
        timer0 = p_building->SubState - dome_open_speed;
        p_building->SubState = timer0;
        if (timer0 <= 127)
        {
            if ((timer0 >= 111u) && (timer0 < 111u + dome_open_speed)) {
                set_dome_col(p_building, 1);
            }
            do_dome_rotate1(p_building);
        }
        else
        {
            play_dist_sample(p_building, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->SubState = 0;
            p_building->Timer1 = 100;
            p_building->State = BldSt_TRA_OPENED;
        }
        break;
    case BldSt_TRA_CLOSING:
        timer0 = p_building->SubState + dome_open_speed;
        p_building->SubState = timer0;
        if (timer0 <= 127)
        {
            do_dome_rotate1(p_building);
        }
        else
        {
            play_dist_sample(p_building, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->SubState = 127;
            p_building->Timer1 = 100;
            p_building->State = BldSt_TRA_CLOSED;
        }
        break;
    case BldSt_TRA_OPENED:
        if ((p_building->Flag & TngF_TransCloseRq) != 0) {
            play_dist_sample(p_building, 100, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->State = BldSt_TRA_CLOSING;
        }
        p_building->Flag &= ~(TngF_TransCloseRq|TngF_TransOpenRq);
        break;
    case BldSt_TRA_CLOSED:
        if ((p_building->Flag & TngF_TransOpenRq) != 0) {
            play_dist_sample(p_building, 100, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
            p_building->State = BldSt_TRA_OPENING;
        }
        p_building->Flag &= ~(TngF_TransCloseRq|TngF_TransOpenRq);
        break;
    default:
        p_building->SubState = 127;
        p_building->Timer1 = 100;
        p_building->State = BldSt_TRA_CLOSED;
        break;
    }
}
static DIFFTEST_REF_FUNC int sit_on_my_face_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_sit_on_my_face\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Checks whether given face belongs to one of the objects of given thing.
 * Negative face index refers to a quad face (SingleObjectFace4).
 *
 * Routed asm (internal label): a1 = face, a2 = thing pointer.
 */
int sit_on_my_face(int a1, intptr_t a2, int a3, int a4)
{
    int face = a1;
    struct Thing *p_thing = (struct Thing *)a2;
    struct SingleObject *p_objects;
    int i;

    DIFFTEST_SHADOW_RET(sit_on_my_face, int, sit_on_my_face_asm(a1, a2, a3, a4));
    p_objects = game_objects;
    for (i = 0; i < (int)p_thing->U.UObject.NumbObjects; i++)
    {
        struct SingleObject *p_sobj;
        int start, end;

        p_sobj = &p_objects[p_thing->U.UObject.Object + i];
        if (face < 0)
        {
            start = p_sobj->StartFace4;
            end = start + p_sobj->NumbFaces4;
            if (-face >= start && -face <= end) {
                game_objects = p_objects;
                DIFFTEST_RETURN(1);
            }
        }
        else
        {
            start = p_sobj->StartFace;
            end = start + p_sobj->NumbFaces;
            if (face >= start && face <= end) {
                game_objects = p_objects;
                DIFFTEST_RETURN(1);
            }
        }
    }
    game_objects = p_objects;
    DIFFTEST_RETURN(0);
}

static DIFFTEST_REF_FUNC void do_people_on_exploding_building_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_do_people_on_exploding_building\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Updates people standing on faces of an exploding building: drops them
 * to the ground, and kills them if the ground tile is marked MEF2_Unkn04.
 *
 * Routed asm (internal label): a1 = building thing index.
 */
void do_people_on_exploding_building(int a1, int a2, int a3, intptr_t a4)
{
    struct Thing *p_building;
    int thing;
    int remain;

    DIFFTEST_SHADOW(do_people_on_exploding_building,
      do_people_on_exploding_building_asm(a1, a2, a3, a4));
    thing = (ushort)things_used_head;
    remain = things_used;
    if (thing <= 0)
        return;
    do
    {
        struct Thing *p_thing;

        remain--;
        if (remain == -1)
            break;
        p_thing = &things[thing];
        thing = p_thing->LinkChild;
        if (p_thing->Type != TT_PERSON)
            continue;
        if ((p_thing->Flag & TngF_InVehicle) != 0)
            continue;
        if (p_thing->U.UPerson.OnFace == 0)
            continue;
        p_building = &things[(short)a1];
        if (sit_on_my_face(p_thing->U.UPerson.OnFace, (intptr_t)p_building, 0, 0) == 0)
            continue;
        {
            int alt;
            struct MyMapElement *p_mapel;

            alt = alt_at_point((short)(p_thing->X >> 8), (short)(p_thing->Z >> 8));
            p_thing->U.UPerson.OnFace = 0;
            p_thing->Y = alt;
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                continue;
            p_mapel = &game_my_big_map[((p_thing->Z >> 16) << 7) + (p_thing->X >> 16)];
            if ((p_mapel->Flags2 & MEF2_Unkn04) == 0)
                continue;
            p_thing->U.UPerson.WeaponsCarried = 0;
            set_person_dead(p_thing, 10);
        }
    } while (thing > 0);
}


int del_thing_vectors_from_mapwho(intptr_t a1, int a2, intptr_t a3, int a4);

/** Scanner areas to redraw; defined in asm, same layout as in scanner.c. */
extern ubyte redo_scanner[128][4];
extern s32 redo_scanner_last;
extern s32 redo_scanner_first;

static DIFFTEST_REF_FUNC ushort explode_thing_building_asm(short thing,
  short x, short y, short z)
{
    ushort ret;
    asm volatile (
      "call ASM_explode_thing_building\n"
        : "=a" (ret) : "a" (thing), "d" (x), "b" (y), "c" (z));
    return ret;
}

/** Turns faces of a building's objects into explode faces, clears the map
 * tiles under it (collapsing dependent buildings and mines on the way),
 * and damages people and vehicles around.
 *
 * @return NextObject of the last SingleObject of the building.
 */
ushort explode_thing_building(short thing, short x, short y, short z)
{
    struct Thing *p_building;
    struct SingleObject *p_sobj;
    int obj;
    ubyte i;
    int min_x, min_z, max_x, max_z;
    int intensity, cntr_x, cntr_z;
    int tile_x, tile_z;

    DIFFTEST_SHADOW_RET(explode_thing_building, ushort,
      explode_thing_building_asm(thing, x, y, z));

    dont_bother_with_explode_faces = 0;
    p_building = &things[thing];
    // TODO the original returns uninitialized data if there are no objects
    p_sobj = NULL;

    obj = p_building->U.UObject.Object;
    for (i = 0; i < p_building->U.UObject.NumbObjects; i++, obj++)
    {
        int ofs_x, ofs_y, ofs_z;
        int face, face_end;
        ushort start_face, numb_faces;

        p_sobj = &game_objects[obj];
        numb_faces = p_sobj->NumbFaces;
        start_face = p_sobj->StartFace;
        if (p_building->SubType == SubTT_BLD_MGUN)
        {
            ofs_x = p_building->X >> 8;
            ofs_y = p_building->Y >> 5;
            ofs_z = p_building->Z >> 8;
        }
        else
        {
            do_people_on_exploding_building(thing, numb_faces, start_face,
              (intptr_t)p_building);
            ofs_x = p_sobj->MapX;
            ofs_y = p_sobj->OffsetY;
            ofs_z = p_sobj->MapZ;
        }

        face_end = (int)start_face + numb_faces;
        for (face = start_face; face < face_end; face++)
        {
            struct SingleObjectFace3 *p_face;
            struct SinglePoint *p_pt;
            int x0, y0, z0, x1, y1, z1, x2, y2, z2;
            int avg_x, avg_z;
            ushort exface;

            p_face = &game_object_faces3[face];
            if ((p_face->GFlags & 0x04) != 0)
            {
                p_face->GFlags &= ~0x04;
                make_triangle_solid(p_face->UnknTringl);
            }
            p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
            x0 = ofs_x + p_pt->X;
            y0 = p_pt->Y + ofs_y;
            z0 = ofs_z + p_pt->Z;
            p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
            x1 = p_pt->X + ofs_x;
            y1 = p_pt->Y + ofs_y;
            z1 = ofs_z + p_pt->Z;
            p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
            x2 = p_pt->X + ofs_x;
            y2 = p_pt->Y + ofs_y;
            z2 = ofs_z + p_pt->Z;
            avg_x = (x0 + x1 + x2) / 3;
            avg_z = (z0 + z1 + z2) / 3;

            exface = explode_face_alloc();
            if (exface != 0)
            {
                struct ExplodeFace *p_exface;

                p_exface = &ex_faces[exface];
                p_exface->Type = EXPL_FACE_TRI_ABS;
                p_exface->Texture = p_face->Texture;
                p_exface->Flags = p_face->Flags;
                p_exface->Col = p_face->ExCol;
                p_exface->X0 = x0;
                p_exface->Y0 = y0;
                p_exface->Z0 = z0;
                p_exface->Y1 = y1;
                p_exface->Z1 = z1;
                p_exface->X2 = x2;
                p_exface->Y2 = y2;
                p_exface->Z2 = z2;
                p_exface->X1 = x1;
                p_exface->DX = (avg_x - ofs_x) >> 7;
                p_exface->DY = 1 - (LbRandomAnyShort() & 7);
                p_exface->Timer = 1;
                p_exface->DZ = (avg_z - ofs_z) >> 7;
            }
        }

        face_end = (int)p_sobj->StartFace4 + p_sobj->NumbFaces4;
        for (face = p_sobj->StartFace4; face < face_end; face++)
        {
            struct SingleObjectFace4 *p_face;
            struct SinglePoint *p_pt;
            int x0, y0, z0, x1, y1, z1, x2, y2, z2, x3, y3, z3;
            int avg_x, avg_z;
            ushort exface;

            p_face = &game_object_faces4[face];
            if ((p_face->GFlags & 0x04) != 0)
            {
                p_face->GFlags &= ~0x04;
                make_triangle_solid(p_face->UnknTringl2);
                make_triangle_solid(p_face->UnknTringl1);
            }
            p_pt = &game_object_points[(ushort)p_face->PointNo[0]];
            x0 = ofs_x + p_pt->X;
            y0 = p_pt->Y + ofs_y;
            z0 = ofs_z + p_pt->Z;
            p_pt = &game_object_points[(ushort)p_face->PointNo[1]];
            x1 = p_pt->X + ofs_x;
            y1 = p_pt->Y + ofs_y;
            z1 = ofs_z + p_pt->Z;
            p_pt = &game_object_points[(ushort)p_face->PointNo[2]];
            x2 = p_pt->X + ofs_x;
            y2 = p_pt->Y + ofs_y;
            z2 = ofs_z + p_pt->Z;
            p_pt = &game_object_points[(ushort)p_face->PointNo[3]];
            x3 = p_pt->X + ofs_x;
            y3 = p_pt->Y + ofs_y;
            z3 = ofs_z + p_pt->Z;
            avg_x = (x0 + x1 + x2 + x3) / 4;
            avg_z = (z0 + z1 + z2 + z3) / 4;

            exface = explode_face_alloc();
            if (exface != 0)
            {
                struct ExplodeFace *p_exface;

                p_exface = &ex_faces[exface];
                p_exface->Type = EXPL_FACE_QUAD_ABS;
                p_exface->Texture = p_face->Texture;
                p_exface->Flags = p_face->Flags;
                p_exface->Col = p_face->ExCol;
                p_exface->X0 = x0;
                p_exface->Y0 = y0;
                p_exface->Z0 = z0;
                p_exface->Y1 = y1;
                p_exface->Z1 = z1;
                p_exface->X2 = x2;
                p_exface->Y2 = y2;
                p_exface->Z2 = z2;
                p_exface->X3 = x3;
                p_exface->Y3 = y3;
                p_exface->Z3 = z3;
                p_exface->X1 = x1;
                p_exface->DX = (avg_x - ofs_x) >> 7;
                p_exface->Timer = 1;
                p_exface->DZ = (avg_z - ofs_z) >> 7;
                p_exface->DY = 1 - (LbRandomAnyShort() & 7);
            }
        }
    }

    // Get bounds of the building, in tiles
    max_x = (s32)0x80000000;
    max_z = (s32)0x80000000;
    min_x = 0x7FFFFFFF;
    min_z = 0x7FFFFFFF;
    obj = p_building->U.UObject.Object;
    for (i = 0; i < p_building->U.UObject.NumbObjects; i++, obj++)
    {
        int ofs_x, ofs_z;
        int pt;

        p_sobj = &game_objects[obj];
        ofs_x = p_sobj->MapX;
        ofs_z = p_sobj->MapZ;
        for (pt = p_sobj->StartPoint; pt <= (int)p_sobj->EndPoint; pt++)
        {
            struct SinglePoint *p_pt;
            int px, pz;

            p_pt = &game_object_points[pt];
            px = ofs_x + p_pt->X;
            pz = ofs_z + p_pt->Z;
            if (max_x <= px)
                max_x = px;
            if (min_x >= px)
                min_x = px;
            if (max_z <= pz)
                max_z = pz;
            if (min_z >= pz)
                min_z = pz;
        }
    }
    max_x = (max_x + 0x100) >> 8;
    min_x >>= 8;
    min_z >>= 8;
    max_z = (max_z + 0x100) >> 8;
    if (min_x < 0)
        min_x = 0;
    if (min_z < 0)
        min_z = 0;
    if (max_x > 127)
        max_x = 127;
    if (max_z > 127)
        max_z = 127;

    intensity = ((max_x - min_x) + (max_z - min_z)) << 9;
    if (intensity > 20000)
        intensity = 20000;
    cntr_x = (min_x + max_x) << 15;
    cntr_z = (min_z + max_z) << 15;

    // Clear map tiles under the building
    for (tile_x = min_x; tile_x <= max_x; tile_x++)
    {
        for (tile_z = min_z; tile_z <= max_z; tile_z++)
        {
            struct MyMapElement *p_mapel;
            ubyte flags2;
            short child;

            p_mapel = &game_my_big_map[tile_z * MAP_TILE_WIDTH + tile_x];
            p_mapel->Flags &= ~0x80;
            flags2 = p_mapel->Flags2;
            p_mapel->ColumnHead = 0;
            flags2 &= ~0x08;
            p_mapel->Flags2 = flags2;
            if (p_building->SubType == SubTT_BLD_GATE)
            {
                int tnode;

                p_mapel->Flags2 = flags2 | 0x08;
                for (tnode = 1; tnode < next_traffic_node; tnode++)
                {
                    struct TrafficNode *p_tnode;

                    p_tnode = &game_traffic_nodes[tnode];
                    if (p_tnode->GateLink == thing)
                    {
                        p_tnode->GateLink = 0;
                        p_tnode->Flags &= ~0x8000;
                    }
                }
            }

            while (del_thing_vectors_from_mapwho((intptr_t)p_mapel, thing,
              (intptr_t)p_mapel, 0) != 0)
                ;

            child = p_mapel->Child;
            switch (p_building->SubType)
            {
            case SubTT_BLD_SHUTLDR:
            case SubTT_BLD_1D:
            case SubTT_BLD_TRAINTRK:
            case SubTT_BLD_BEZIER_ROAD:
            case SubTT_BLD_MGUN:
                continue;
            }

            while (child != 0)
            {
                if (child > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[child];
                    child = p_thing->Next;
                    if (p_thing->Type == TT_BUILDING)
                    {
                        switch (p_thing->SubType)
                        {
                        case SubTT_BLD_SHUTLDR:
                        case SubTT_BLD_1D:
                        case SubTT_BLD_TRAINTRK:
                        case SubTT_BLD_BEZIER_ROAD:
                        case SubTT_BLD_MGUN:
                            if ((p_thing->Flag & TngF_Destroyed) != 0)
                                break;
                            if (p_thing->SubType == SubTT_BLD_MGUN)
                                init_mgun_explode(p_thing);
                            else
                                collapse_building(x, y, z, p_thing);
                            break;
                        }
                    }
                    else if (p_thing->Type == TT_MINE)
                    {
                        if (p_thing->SubType == 0x30)
                        {
                            bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 20);
                            p_thing->State = PerSt_DEAD;
                            p_thing->Flag |= TngF_Destroyed;
                            remove_thing(p_thing->ThingOffset);
                            delete_node(p_thing);
                        }
                    }
                }
                else
                {
                    struct SimpleThing *p_sthing;

                    p_sthing = &sthings[child];
                    child = p_sthing->Next;
                    if ((p_sthing->Type == SmTT_DROPPED_ITEM) &&
                      (p_sthing->U.UWeapon.OnFace != 0))
                    {
                        remove_sthing(p_sthing->ThingOffset);
                        delete_snode(p_sthing);
                    }
                }
            }
        }
    }

    triangulation_unkn_func_002(min_x, min_z, max_x, max_z);
    {
        s32 next;

        next = (redo_scanner_last + 1) & 0x7F;
        if (next != redo_scanner_first)
        {
            s32 last;

            last = redo_scanner_last;
            redo_scanner[last][0] = min_z * 2;
            redo_scanner[last][1] = min_x * 2;
            redo_scanner[last][2] = max_z * 2;
            redo_scanner_last = next;
            redo_scanner[last][3] = max_x * 2;
        }
    }

    // Shockwave around the building
    min_x -= 6;
    min_z -= 6;
    max_x += 6;
    max_z += 6;
    if (min_x < 0)
        min_x = 0;
    if (min_z < 0)
        min_z = 0;
    if (max_x > 127)
        max_x = 127;
    if (max_z > 127)
        max_z = 127;

    for (tile_x = min_x; tile_x <= max_x; tile_x++)
    {
        for (tile_z = min_z; tile_z <= max_z; tile_z++)
        {
            short child;

            child = game_my_big_map[tile_z * MAP_TILE_WIDTH + tile_x].Child;
            while (child != 0)
            {
                if (child > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[child];
                    if (p_thing->Type == TT_PERSON)
                        p_thing->Flag2 &= ~TgF2_InsideBuilding;
                    if ((p_thing->Type == TT_PERSON) &&
                      ((p_thing->Flag & TngF_Destroyed) == 0) &&
                      (p_thing->State != PerSt_PERSON_BURNING))
                    {
                        int dx, dz;
                        int dist_x, dist_z, dist;

                        dx = (p_thing->X - cntr_x) >> 8;
                        dz = (p_thing->Z - cntr_z) >> 8;
                        dist_x = abs((p_thing->X >> 16) - (min_x + 6));
                        dist = abs((p_thing->X >> 16) - (max_x - 6));
                        if (dist_x >= dist)
                            dist_x = dist;
                        dist_z = abs((p_thing->Z >> 16) - (max_z - 6));
                        dist = abs((p_thing->Z >> 16) - (min_z + 6));
                        if (dist < dist_z)
                            dist_z = dist;
                        if (dist_x < dist_z)
                            dist_z = dist_x;
                        do_shockwave_person(dx, dz, (dist_z << 7) + 0x100,
                          intensity, p_thing, NULL);
                    }
                    if (p_thing->Type == TT_VEHICLE)
                    {
                        int damage;

                        damage = 3500;
                        p_thing->U.UVehicle.WorkPlace |= VWPFlg_Unkn0080;
                        if (p_thing->SubType == SubTT_VEH_TANK)
                            damage = 1750;
                        if (p_thing->SubType == SubTT_VEH_MECH)
                            damage >>= 2;
                        p_thing->Health -= damage;
                        if (p_thing->Health <= 0)
                        {
                            if (p_thing->SubType == SubTT_VEH_MECH)
                            {
                                p_thing->Flag |= TngF_Destroyed;
                                init_mech_explode(p_thing);
                            }
                            else
                            {
                                p_thing->Flag |= TngF_Destroyed;
                                start_crashing(p_thing);
                                play_dist_sample(p_thing, 0x5F, 0x7F, 0x40, 100, 0, 1);
                            }
                        }
                    }
                    child = p_thing->Next;
                }
                else
                {
                    child = sthings[child].Next;
                }
            }
        }
    }

    if (p_sobj == NULL)
        DIFFTEST_RETURN((ushort)0);
    DIFFTEST_RETURN(p_sobj->NextObject);
}

void collapse_building_process_tnodes(struct Thing *p_building)
{
    short tnode1, tnode2;
    int i;

    for (i = 0; i < 4; i++)
    {
        short tnode;
        tnode = p_building->U.UObject.tnode_50[i];
        if (tnode != 0) {
            struct TrafficNode *p_tnode;
            p_tnode = &game_traffic_nodes[tnode];
            p_tnode->Flags |= (0x2000|0x1000);
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[0];
    tnode2 = p_building->U.UObject.tnode_50[1];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                break;
            }
            if (p_tnode->UTraffic.Link[i] == 0) {
                p_tnode->UTraffic.Link[i] = -tnode2;
                p_tnode->UTraffic.Flags[i] = 0x40;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[2];
    tnode2 = p_building->U.UObject.tnode_50[3];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                break;
            }
            if (p_tnode->UTraffic.Link[i] == 0) {
                p_tnode->UTraffic.Link[i] = -tnode2;
                p_tnode->UTraffic.Flags[i] = 0x40;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[0];
    tnode2 = p_building->U.UObject.tnode_50[3];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                p_tnode->UTraffic.Link[i] = 0;
                break;
            }
        }
    }

    tnode1 = p_building->U.UObject.tnode_50[2];
    tnode2 = p_building->U.UObject.tnode_50[1];
    if ((tnode1 != 0) && (tnode2 != 0))
    {
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[tnode1];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] == -tnode2) {
                p_tnode->UTraffic.Link[i] = 0;
                break;
            }
        }
    }
}

void collapse_building_shuttle_loader(short x, short y, short z, struct Thing *p_building)
{
    struct Thing *p_thing;
    short thing;

    p_thing = p_building;
    thing = p_building->ThingOffset;
    while ( 1 )
    {
        short pvthing;
        pvthing = thing;
        thing = p_thing->U.UObject.NextThing;
        if (thing == 0)
            break;
        if (thing <= 0)
        {
            if (p_building->SubType == SubTT_BLD_BEZIER_ROAD) {
                tnode_all_unlink_thing(pvthing);
            }
            break;
        }
        else
        {
            p_thing = &things[thing];
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                break;
            p_thing->Flag |= TngF_Destroyed;
            explode_thing_building(thing, x, y, z);
            p_thing->Type = TT_UNKN55;
        }
    }

    p_thing = p_building;
    thing = p_building->ThingOffset;
    while ( 1 )
    {
        short nxthing;
        nxthing = thing;
        thing = p_thing->U.UObject.PrevThing;
        if (thing == 0)
            break;
        if (thing <= 0)
        {
            if (p_building->SubType == SubTT_BLD_BEZIER_ROAD) {
                tnode_all_unlink_thing(nxthing);
            }
            break;
        }
        else
        {
            p_thing = &things[thing];
            if ((p_thing->Flag & TngF_Destroyed) != 0)
                break;
            p_thing->Flag |= TngF_Destroyed;
            explode_thing_building(thing, x, y, z);
            p_thing->Type = TT_UNKN55;
        }
    }
}

void collapse_building_station(struct Thing *p_building)
{
    MapCoord cntr_cor_x, cntr_cor_z;
    short cntr_x, cntr_z;
    short x, z;

    get_thing_position_mapcoords(&cntr_cor_x, NULL, &cntr_cor_z, p_building->ThingOffset);
    cntr_x = MAPCOORD_TO_TILE(cntr_cor_x);
    cntr_z = MAPCOORD_TO_TILE(cntr_cor_z);

    for (x = cntr_x - 8; x <= cntr_x + 8; x++)
    {
        if ((x < 0) || (x > MAP_TILE_WIDTH-1))
            continue;
        for (z = cntr_z - 8; z <= cntr_z + 8; z++)
        {
            struct MyMapElement *p_mapel;
            short thing;

            if ((z < 0) || (z > MAP_TILE_HEIGHT-1))
                continue;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * z + x];
            thing = p_mapel->Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    if ((p_thing->Type == TT_BUILDING)
                      && (p_thing->SubType == SubTT_BLD_TRAINTRK)
                      && ((p_thing->Flag & TngF_Destroyed) == 0))
                    {
                        collapse_building(TILE_TO_MAPCOORD(x,0), 0, TILE_TO_MAPCOORD(z,0), p_thing);
                        break; // do not expect more than one building on a tile
                    }
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                }
            }
        }
    }
}

static DIFFTEST_REF_FUNC void init_mgun_explode_asm(struct Thing *p_thing)
{
    asm volatile (
      "call ASM_init_mgun_explode\n"
        :  : "a" (p_thing));
    return;
}

void init_mgun_explode(struct Thing *p_thing)
{
    s32 cor_x, cor_z;

    DIFFTEST_SHADOW(init_mgun_explode, init_mgun_explode_asm(p_thing));
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;

    p_thing->Flag |= TngF_Destroyed;
    play_dist_sample(p_thing, 0x25, 0x7F, 0x40, 100, 0, 3);
    p_thing->State = 13;

    cor_x = p_thing->X;
    if (cor_x < 0 || (cor_x >> 8) >= 0x8000)
        return;
    cor_z = p_thing->Z >> 8;
    if (cor_z < 0 || cor_z >= 0x8000)
        return;

    bang_new4(p_thing->X, p_thing->Y, p_thing->Z, 15);
    delete_node(p_thing);
    remove_thing(p_thing->ThingOffset);
    do_shockwave(p_thing->X >> 8, p_thing->Y >> 8, p_thing->Z >> 8,
      ((ushort)p_thing->Radius) << 2, 5000, NULL);
}

void collapse_building(short x, short y, short z, struct Thing *p_building)
{
#if 0
    asm volatile (
      "call ASM_collapse_building\n"
        :  : "a" (x), "d" (y), "b" (z), "c" (p_thing));
    return;
#endif
    struct SimpleThing *p_sthing;
    short thing;
    int i;

    if (ingame.SoundThing != 0)
    {
        p_sthing = &sthings[ingame.SoundThing];
        p_sthing->X = p_building->X;
        p_sthing->Y = p_building->Y;
        p_sthing->Z = p_building->Z;
        p_sthing->State = 1;
    }
    else
    {
        short tng_x, tng_y, tng_z;
        get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, p_building->ThingOffset);
        play_dist_sample(p_building, 45, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        p_sthing = create_sound_effect(tng_x, tng_y, tng_z, 46, FULL_VOL, -1);
        if (p_sthing != NULL)
        {
            p_sthing->State = 1;
            ingame.SoundThing = p_sthing->ThingOffset;
        }
    }
    thing = p_building->ThingOffset;

    collapse_building_process_tnodes(p_building);

    switch (p_building->SubType)
    {
    case SubTT_BLD_SHUTLDR:
    case SubTT_BLD_1D:
    case SubTT_BLD_TRAINTRK:
    case SubTT_BLD_BEZIER_ROAD:
        collapse_building_shuttle_loader(x, y, z, p_building);
        break;
    case SubTT_BLD_STATION:
        collapse_building_station(p_building);
        break;
    }

    if ((p_building->Flag & TngF_Destroyed) == 0)
    {
        struct SingleObject *p_sobj;

        p_sobj = &game_objects[p_building->U.UObject.Object];
        if (((p_sobj->field_1C & 0x0100) == 0) || ((render_floor_flags & RendFlrF_WobblyTerrain) != 0))
        {
            short cra_cor_x, cra_cor_z;

            get_thing_position_mapcoords(&cra_cor_x, NULL, &cra_cor_z, p_building->ThingOffset);
            quick_crater(MAPCOORD_TO_TILE(cra_cor_x), MAPCOORD_TO_TILE(cra_cor_z), 3);
            for (i = 0; i < 32; i++)
            {
                int dx, dz;
                int bang_x, bang_z;
                dz = (LbRandomAnyShort() & 0xFF) - 127;
                dx = (LbRandomAnyShort() & 0xFF) - 127;
                bang_z = ((z + dz) << 8);
                bang_x = ((x + dx) << 8);
                bang_new4(bang_x, 0, bang_z, 65);
            }
        }
        p_building->Flag |= TngF_Destroyed;
        explode_thing_building(thing, x, y, z);
        p_building->Type = TT_UNKN55;
    }
}

static DIFFTEST_REF_FUNC short create_a_pod_asm(struct Thing *p_thing,
  struct Thing *p_loader, ushort pod_id)
{
    short ret;
    asm volatile (
      "call ASM_create_a_pod\n"
        : "=a" (ret) : "a" (p_thing), "d" (p_loader), "b" (pod_id));
    return ret;
}

short create_a_pod(struct Thing *p_thing, struct Thing *p_loader, ushort pod_id)
{
    struct Thing *p_pod;
    short next;

    DIFFTEST_SHADOW_RET(create_a_pod, short,
      create_a_pod_asm(p_thing, p_loader, pod_id));
    if (p_thing != NULL)
        delete_node(p_thing);

    if (pod_id == 0)
    {
        ThingIdx pod;

        pod = get_new_thing();
        if (pod == 0)
            DIFFTEST_RETURN((short)0);
        p_pod = &things[pod];
        p_pod->Type = TT_VEHICLE;
        p_pod->SubType = SubTT_VEH_SHUTTLE_POD;
        p_pod->U.UVehicle.TNode = p_loader->ThingOffset;
        if ((p_thing != NULL) && ((p_thing->Flag & TngF_PlayerAgent) != 0))
            play_dist_sample(p_pod, 9, 127, 64, 100, -1, 1);
    }
    else
    {
        // TODO the asm leaves the pod pointer uninitialised when pod_id
        // is non-zero; all callers pass 0, assume the id was meant
        p_pod = &things[pod_id];
    }

    if (p_thing == NULL)
    {
        p_pod->U.UVehicle.PassengerHead = 0;
        DIFFTEST_RETURN((short)0);
    }

    next = p_thing->Next;
    p_thing->Next = p_pod->U.UVehicle.PassengerHead;
    p_thing->U.UPerson.Vehicle = p_pod->ThingOffset;
    p_pod->U.UVehicle.PassengerHead = p_thing->ThingOffset;
    p_thing->U.UPerson.GotoX = ((p_thing->X >> 8) & ~0xFF) + 0x80;
    p_thing->Flag |= TngF_Unkn4000;
    p_thing->U.UPerson.GotoZ = ((p_thing->Z >> 8) & ~0xFF) + 0x80;
    DIFFTEST_RETURN(next);
}

static DIFFTEST_REF_FUNC void process_shuttle_loader_asm(struct Thing *p_building)
{
    asm volatile ("call ASM_process_shuttle_loader\n"
        : : "a" (p_building));
}

void process_shuttle_loader(struct Thing *p_building)
{
    s32 tile_x, tile_z;
    ThingIdx thing;

    DIFFTEST_SHADOW(process_shuttle_loader, process_shuttle_loader_asm(p_building));
    // The loader keeps its tile coordinates in the velocity fields
    tile_x = p_building->VX;
    tile_z = p_building->VZ;
    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH))
        return;
    if ((tile_z < 0) || (tile_z >= MAP_TILE_HEIGHT))
        return;

    thing = game_my_big_map[tile_z * MAP_TILE_WIDTH + tile_x].Child;
    while (thing != 0)
    {
        if (thing > 0)
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if ((p_thing->Type == TT_PERSON) && ((p_thing->Flag & TngF_Unkn0040) == 0))
                thing = create_a_pod(p_thing, p_building, 0);
            else
                thing = p_thing->Next;
        }
        else
        {
            thing = sthings[thing].Next;
        }
    }
}

static DIFFTEST_REF_FUNC void bul_hit_vector_asm(int x, int y, int z, short col, int hp, int type)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)hp;
    stkargs[1] = (int)(intptr_t)type;

    asm volatile (
      "push 4(%4)\n"
      "push 0(%4)\n"
      "call ASM_bul_hit_vector\n"
        : : "a" (x), "d" (y), "b" (z), "c" (col), "S" (stkargs)
        : "cc", "memory");
}

void bul_hit_vector(int x, int y, int z, short col, int hp, int type)
{
    struct ColVectList *p_cvlist;
    struct Thing *p_thing;
    s32 health;

    DIFFTEST_SHADOW(bul_hit_vector, bul_hit_vector_asm(x, y, z, col, hp, type));
    p_cvlist = &game_col_vects_list[col];
    if (game_col_vects[p_cvlist->Vect].Face == 0)
        return;

    p_thing = &things[p_cvlist->Object];
    if ((p_thing->Flag & TngF_Destroyed) != 0)
        return;

    play_dist_sample(p_thing, 0x41, 0x7f, 0x40, 100, 0, 1);
    health = p_thing->Health - ((short)hp >> 1);
    if (health < 0)
        collapse_building(p_thing->X >> 8, p_thing->Y >> 8, p_thing->Z >> 8, p_thing);
    p_thing->Health = health;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int rotate_obj_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_rotate_obj\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int rotate_obj(int a1, int a2, int a3, intptr_t a4)
{
    s32 *p_mat = (s32 *)a4;
    ushort ang_x, ang_y, ang_z;
    s32 sin_x, cos_x, sin_y, cos_y, sin_z, cos_z;
    s32 cxcy, czcy, czcx, cxsz, sxsz, sxcy;
    s32 v;

    DIFFTEST_SHADOW_RET(rotate_obj, int, rotate_obj_asm(a1, a2, a3, a4));
    ang_x = a1 & 0x7FF;
    ang_y = a2 & 0x7FF;
    ang_z = a3 & 0x7FF;

    sin_x = lbSinTable[ang_x] >> 1;
    cos_x = lbSinTable[ang_x + LbFPMath_PI/2] >> 1;
    sin_y = lbSinTable[ang_y] >> 1;
    cos_y = lbSinTable[ang_y + LbFPMath_PI/2] >> 1;
    sin_z = lbSinTable[ang_z] >> 1;
    cos_z = lbSinTable[ang_z + LbFPMath_PI/2] >> 1;

    cxcy = cos_x * cos_y;
    czcy = cos_z * cos_y;
    czcx = cos_z * cos_x;
    cxsz = cos_x * sin_z;
    sxsz = sin_x * sin_z;
    sxcy = sin_x * cos_y;

    p_mat[0] = czcy >> 16;
    p_mat[1] = (-(cos_y * sin_z)) >> 16;
    p_mat[2] = sin_y >> 1;
    v = (s32)((u32)cxsz + (u32)(((cos_z * sin_x) >> 15) * sin_y));
    p_mat[3] = v >> 16;
    v = (s32)((u32)czcx - (u32)((sxsz >> 15) * sin_y));
    p_mat[4] = v >> 16;
    p_mat[5] = (-sxcy) >> 16;
    v = (s32)((u32)sxsz - (u32)((czcx >> 15) * sin_y));
    p_mat[6] = v >> 16;
    v = (s32)((u32)(cos_z * sin_x) + (u32)(((cos_x * sin_y) >> 15) * sin_z));
    p_mat[7] = v >> 16;
    p_mat[8] = cxcy >> 16;
    DIFFTEST_RETURN((int)(cxcy >> 16));
}


static DIFFTEST_REF_FUNC void rotate_object_asm(struct Thing *p_object)
{
    asm volatile ("call ASM_rotate_object\n"
        : : "a" (p_object));
}

void rotate_object(struct Thing *p_object)
{
    struct M33 *p_mat;
    short angle;

    DIFFTEST_SHADOW(rotate_object, rotate_object_asm(p_object));
    angle = p_object->U.UMGun.AngleY + (p_object->U.UMGun.AngleDY >> 3);
    p_object->U.UMGun.AngleY = angle;
    p_object->U.UMGun.AngleY = angle & 0x7FF;
    p_mat = &local_mats[p_object->U.UMGun.MatrixIndex];
    if (p_object->U.UMGun.AngleX > 0x400)
        p_object->U.UMGun.AngleX -= 0x800;
    rotate_obj(0, (short)(-p_object->U.UMGun.AngleY), 0, (intptr_t)p_mat);
    rotate_object_axis(p_mat, 0, (short)(-p_object->U.UMGun.AngleX), 0);
}

static DIFFTEST_REF_FUNC int mounted_los_asm(int x1, int y1, int z1, int x2, int y2, int z2)
{
    int ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[2];

    stkargs[0] = (int)(intptr_t)y2;
    stkargs[1] = (int)(intptr_t)z2;

    asm volatile (
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_mounted_los\n"
        : "=a" (ret)
        : "a" (x1), "d" (y1), "b" (z1), "c" (x2), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

/** Line of sight check for mounted guns: steps along the line from
 * (x1,y1,z1) to (x2,y2,z2) in about 128-unit steps, checking collision
 * columns. Returns 0 if blocked or too far, 1 otherwise.
 */
int mounted_los(int x1, int y1, int z1, int x2, int y2, int z2)
{
    s32 dx, dy, dz;
    s32 adx, ady, adz;
    s32 dist, sum;
    s32 n, i;
    s32 x, y, z;

    DIFFTEST_SHADOW_RET(mounted_los, int, mounted_los_asm(x1, y1, z1, x2, y2, z2));
    dx = x2 - x1;
    dy = y2 - y1;
    dz = z2 - z1;
    adx = abs(dx);
    ady = abs(dy);
    adz = abs(dz);

    // Approximate distance
    if (adx > ady && adx > adz)
    {
        sum = ady + adz;
        dist = adx - (adx >> 4) + (sum >> 1) - (sum >> 3);
    }
    else if (adx <= ady && ady > adz)
    {
        sum = adx + adz;
        dist = ady - (ady >> 4) + (sum >> 1) - (sum >> 3);
    }
    else
    {
        sum = ady + adx;
        dist = adz - (adz >> 4) + (sum >> 1) - (sum >> 3);
    }

    if (dist > 0xC00)
        DIFFTEST_RETURN(0);

    n = dist >> 7;
    if (n == 0)
        DIFFTEST_RETURN(0);

    dx /= n;
    dy /= n;
    dz /= n;
    x = x1;
    y = y1;
    z = z1;
    for (i = 0; i < n; i++)
    {
        if (check_col_collision(x, y, z) != 0)
            DIFFTEST_RETURN(0);
        x += dx;
        y += dy;
        z += dz;
    }
    DIFFTEST_RETURN(1);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int kill_flags_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_kill_flags\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
    return ra;
}

int kill_flags(intptr_t a1, intptr_t a2, intptr_t a3, intptr_t a4)
{
    struct Thing *p_thing = (struct Thing *)a1;
    struct Thing *p_target = (struct Thing *)a2;
    ubyte group, tgt_group;

    DIFFTEST_SHADOW_RET(kill_flags, int, kill_flags_asm(a1, a2, a3, a4));
    if ((p_target->Type == TT_PERSON) && (p_target->Flag2 & TgF2_AlteredSubType))
        DIFFTEST_RETURN((int)0);

    if (p_thing->Type == TT_BUILDING)
        group = p_thing->U.UObject.Group;
    else
        group = p_thing->U.UObject.EffectiveGroup;

    if (p_target->Type == TT_BUILDING)
        tgt_group = p_target->U.UObject.Group;
    else
        tgt_group = p_target->U.UObject.EffectiveGroup;

    // The asm shifts by cl, so the count is taken modulo 32
    if ((war_flags[group].KillOnSight & (1u << (tgt_group & 0x1F))) == 0)
        DIFFTEST_RETURN((int)0);
    DIFFTEST_RETURN((int)1);
}


static DIFFTEST_REF_FUNC ThingIdx aquire_target_asm(struct Thing *p_gun)
{
    ThingIdx ret;
    asm volatile (
      "call ASM_aquire_target\n"
        : "=a" (ret) : "a" (p_gun));
    return ret;
}

ThingIdx aquire_target(struct Thing *p_gun)
{
    s32 best_dist;
    ThingIdx best_thing;
    short tile_x, tile_z;
    short dx, dz;

    DIFFTEST_SHADOW_RET(aquire_target, ThingIdx, aquire_target_asm(p_gun));
    best_dist = 0x7FFFFFFF;
    best_thing = 0;
    tile_x = (short)(p_gun->X >> 16);
    tile_z = (short)(p_gun->Z >> 16);

    for (dz = -8; dz < 8; dz++)
    {
        for (dx = -8; dx < 8; dx++)
        {
            s32 tx, tz;
            ThingIdx thing;

            tx = tile_x + dx;
            if (tx <= 0 || tx >= MAP_TILE_WIDTH)
                continue;
            tz = tile_z + dz;
            if (tz <= 0 || tz >= MAP_TILE_HEIGHT)
                continue;

            thing = game_my_big_map[tz * MAP_TILE_WIDTH + tx].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                s32 dist_x, dist_z, dist;

                if (thing <= 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                if ((short)kill_flags((intptr_t)p_gun, (intptr_t)p_thing, (intptr_t)p_thing, (intptr_t)p_gun) != 0
                  && p_thing->Type == TT_PERSON
                  && (p_thing->Flag & TngF_Destroyed) == 0)
                {
                    dist_x = abs((p_gun->X - p_thing->X) >> 8);
                    dist_z = abs((p_gun->Z - p_thing->Z) >> 8);
                    if (dist_x <= dist_z)
                        dist = dist_z + (dist_x >> 1);
                    else
                        dist = dist_x + (dist_z >> 1);
                    if (dist < best_dist)
                    {
                        best_dist = dist;
                        best_thing = thing;
                    }
                }
                thing = p_thing->Next;
            }
        }
    }

    p_gun->PTarget = &things[best_thing];
    DIFFTEST_RETURN(best_thing);
}

static DIFFTEST_REF_FUNC ubyte track_target_asm(struct Thing *p_mgun)
{
    ubyte ret;
    asm volatile (
      "call ASM_track_target\n"
        : "=a" (ret) : "a" (p_mgun));
    return ret;
}

ubyte track_target(struct Thing *p_mgun)
{
    struct Thing *p_target;
    s32 dt_x, dt_z, dt_y;
    s32 tgt_y;
    short angle, dangle_y, dangle_x;
    ubyte ret;

    DIFFTEST_SHADOW_RET(track_target, ubyte, track_target_asm(p_mgun));
    ret = 0;
    if (p_mgun->U.UMGun.WeaponTurn != 0)
        p_mgun->U.UMGun.WeaponTurn--;

    p_target = p_mgun->PTarget;
    dt_x = (p_target->X >> 8) - (p_mgun->X >> 8);
    dt_z = (p_target->Z >> 8) - (p_mgun->Z >> 8);
    tgt_y = p_target->Y;
    angle = arctan(dt_x, dt_z);
    dangle_y = (short)(p_mgun->U.UMGun.AngleY - angle);
    if (dangle_y < -0x400)
        dangle_y = (short)(dangle_y + 0x800);
    else if (dangle_y > 0x400)
        dangle_y = (short)(dangle_y - 0x800);

    if (dangle_y < 0)
    {
        if (dangle_y > -20)
            p_mgun->U.UMGun.AngleDY = 0;
        else
            p_mgun->U.UMGun.AngleDY = 0x100;
    }
    else if (dangle_y > 0)
    {
        if (dangle_y < 20)
            p_mgun->U.UMGun.AngleDY = 0;
        else
            p_mgun->U.UMGun.AngleDY = (short)0xFF00;
    }

    if (dangle_y <= -200 || dangle_y >= 200)
        DIFFTEST_RETURN(ret);

    dt_y = (tgt_y >> 4) - (p_mgun->Y >> 5);
    angle = arctan(dt_y * dt_y, dt_x * dt_x + dt_z * dt_z);
    angle = (short)(angle - 0x400);
    dangle_x = (short)(p_mgun->U.UMGun.AngleX - angle);
    if (dangle_x < -0x400)
        dangle_x = (short)(dangle_x + 0x800);
    else if (dangle_x > 0x400)
        dangle_x = (short)(dangle_x - 0x800);

    p_mgun->U.UMGun.AngleX = (short)(p_mgun->U.UMGun.AngleX - (dangle_x >> 1));
    p_mgun->U.UMGun.AngleX = (short)((p_mgun->U.UMGun.AngleX + 0x800) & 0x7FF);
    if (abs(dangle_x) < 200)
        ret = 1;
    DIFFTEST_RETURN(ret);
}

ubyte mgun_shoot_at_target(struct Thing *p_mgun)
{
    ubyte turn;

    if (p_mgun->U.UMGun.WeaponTurn != 0) {
        return 0;
    }
    turn = p_mgun->U.UMGun.ShotTurn;
    p_mgun->U.UMGun.WeaponTurn = 8;
    p_mgun->U.UMGun.ShotTurn = (turn == 0);
    switch (p_mgun->U.UMGun.CurrentWeapon)
    {
    default:
        LOGWARN("Mounted gun weapon %d not supported, using laser",
          (int)p_mgun->U.UMGun.CurrentWeapon);
        // fall through
    case WEP_LASER:
        init_mgun_laser(p_mgun, 7);
        break;
    }
    return 0;
}

void process_mounted_gun(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_mounted_gun\n"
        : : "a" (p_building));
#endif
    struct Thing *p_target;

    if (p_building->U.UMGun.RecoilTimer > 0)
        p_building->U.UMGun.RecoilTimer--;
    if (p_building->PTarget == NULL)
        p_building->State = 10;

    switch(p_building->State)
    {
    case 10:
        if (((gameturn + p_building->ThingOffset) & 0x07) == 0 && aquire_target(p_building) > 0)
            p_building->State = 11;
        break;
    case 11:
        if (((gameturn + p_building->ThingOffset) & 0x1F) == 0 && aquire_target(p_building) == 0)
            p_building->State = 10;
        if (!track_target(p_building))
        {
            rotate_object(p_building);
            break;
        }
        p_target = p_building->PTarget;
        if (p_target != NULL) // Invalid pointer correction
        {
            if ((p_target <= &things[0]) || (p_target >= &things[THINGS_LIMIT])) {
                LOGERR("Tried to shoot at invalid target 0x%p", p_target);
                p_building->PTarget = p_target = NULL;
            }
        }
        if (p_target != NULL)
        {
            MapCoord tg_cor_x, tg_cor_y, tg_cor_z; // Target/victim coords
            MapCoord at_cor_x, at_cor_y, at_cor_z; // Attacker coords

            tg_cor_x = PRCCOORD_TO_MAPCOORD(p_target->X);
            tg_cor_y = PRCCOORD_TO_MAPCOORD(p_target->Y);
            tg_cor_z = PRCCOORD_TO_MAPCOORD(p_target->Z);
            if (p_target->Type == TT_PERSON)
                tg_cor_y += 20;
            at_cor_x = PRCCOORD_TO_MAPCOORD(p_building->X);
            at_cor_y = PRCCOORD_TO_MAPCOORD(p_building->Y) + 40;
            at_cor_z = PRCCOORD_TO_MAPCOORD(p_building->Z);
            if (mounted_los(at_cor_x, at_cor_y, at_cor_z, tg_cor_x, tg_cor_y, tg_cor_z))
            {
                mgun_shoot_at_target(p_building);
            }
            rotate_object(p_building);
        }
        break;
    }
}
static DIFFTEST_REF_FUNC int apply_object_morph_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_apply_object_morph\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments are the raw registers the asm
 * takes: a1 = object index, a2 = X shift, a3 = Z shift, a4 = which set
 * of points to move. The return value is the leftover eax of the asm.
 */
int apply_object_morph(int a1, int a2, int a3, int a4)
{
    static const ubyte pts_set1[] = {0, 3, 4, 7, 9};
    static const ubyte pts_set0[] = {1, 2, 5, 6, 8};
    struct SingleObject *p_sobj;
    const ubyte *pts;
    short dx, dz;
    u32 pt;
    int i;

    DIFFTEST_SHADOW_RET(apply_object_morph, int,
      apply_object_morph_asm(a1, a2, a3, a4));
    dx = (short)a2;
    dz = (short)a3;
    p_sobj = &game_objects[(ushort)a1];
    pts = ((short)a4 != 0) ? pts_set1 : pts_set0;

    // Only one axis is shifted; X takes precedence over Z
    if (dx != 0)
    {
        for (i = 0; i < 5; i++)
        {
            pt = p_sobj->StartPoint + pts[i];
            game_object_points[pt].X += dx;
        }
    }
    else if (dz != 0)
    {
        for (i = 0; i < 5; i++)
        {
            pt = p_sobj->StartPoint + pts[i];
            game_object_points[pt].Z += dz;
        }
    }
    else
    {
        DIFFTEST_RETURN(a1);
    }
    // The asm leaves the scaled last point index in eax
    DIFFTEST_RETURN((int)(pt * 5));
}


static DIFFTEST_REF_FUNC void process_gate1_asm(struct Thing *p_building)
{
    asm volatile ("call ASM_process_gate1\n"
        : : "a" (p_building));
}

void process_gate1(struct Thing *p_building)
{
    short dx, dz;
    short obj;

    DIFFTEST_SHADOW(process_gate1, process_gate1_asm(p_building));
    dx = p_building->U.UObject.TargetDX;
    dz = p_building->U.UObject.TargetDZ;

    switch ((ushort)p_building->State)
    {
    // TODO both panel pairs use MinY[0], and state 13 sets the bottom OffsetY
    // without the +500 used everywhere else; kept as in the original
    case 13: // panels moving down (RaiseY towards -500)
        if (p_building->U.UObject.Timer[0] == 0)
        {
            if (p_building->U.UObject.RaiseY[0] > -500)
            {
                apply_object_morph((ushort)(p_building->U.UObject.Object + 1), (short)(-dx), (short)(-dz), 1);
                apply_object_morph((ushort)(p_building->U.UObject.Object + 3), dx, dz, 0);
                obj = p_building->U.UObject.Object;
                game_objects[obj + 1].OffsetY = p_building->U.UObject.RaiseY[0] + p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 3].OffsetY = p_building->U.UObject.RaiseY[0] + p_building->U.UObject.MinY[0] + 500;
                p_building->U.UObject.RaiseY[0] += p_building->U.UObject.RaiseDY[0];
            }
            else
            {
                obj = p_building->U.UObject.Object;
                game_objects[obj + 1].OffsetY = p_building->U.UObject.MinY[0];
                game_objects[obj + 3].OffsetY = p_building->U.UObject.MinY[0];
            }
        }
        else
        {
            p_building->U.UObject.Timer[0]--;
        }
        if (p_building->U.UObject.Timer[1] == 0)
        {
            if (p_building->U.UObject.RaiseY[1] > -500)
            {
                apply_object_morph((ushort)(p_building->U.UObject.Object + 2), (short)(-dx), (short)(-dz), 0);
                apply_object_morph((ushort)(p_building->U.UObject.Object + 4), dx, dz, 1);
                obj = p_building->U.UObject.Object;
                game_objects[obj + 2].OffsetY = p_building->U.UObject.RaiseY[1] + p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 4].OffsetY = p_building->U.UObject.RaiseY[1] + p_building->U.UObject.MinY[0] + 500;
                p_building->U.UObject.RaiseY[1] += p_building->U.UObject.RaiseDY[1];
            }
            else
            {
                obj = p_building->U.UObject.Object;
                game_objects[obj + 2].OffsetY = p_building->U.UObject.MinY[0];
                game_objects[obj + 4].OffsetY = p_building->U.UObject.MinY[0];
            }
        }
        else
        {
            p_building->U.UObject.Timer[1]--;
        }
        if ((p_building->U.UObject.RaiseY[0] <= -500) && (p_building->U.UObject.RaiseY[1] <= -500))
        {
            p_building->Flag = 0x40;
            p_building->State = 19;
        }
        break;
    case 15: // panels moving back up (RaiseY towards 0)
        if ((p_building->Flag & 0x20) != 0)
        {
            p_building->State = 13;
            p_building->Flag = 0;
            p_building->U.UObject.Timer[0] = 3;
            p_building->U.UObject.Timer[1] = 0;
            p_building->U.UObject.RaiseDY[0] = -50;
            p_building->U.UObject.RaiseDY[1] = -50;
            break;
        }
        if (p_building->U.UObject.Timer[0] == 0)
        {
            if (p_building->U.UObject.RaiseY[0] < 0)
            {
                apply_object_morph((ushort)(p_building->U.UObject.Object + 1), dx, dz, 1);
                apply_object_morph((ushort)(p_building->U.UObject.Object + 3), (short)(-dx), (short)(-dz), 0);
                obj = p_building->U.UObject.Object;
                game_objects[obj + 1].OffsetY = p_building->U.UObject.RaiseY[0] + p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 3].OffsetY = p_building->U.UObject.RaiseY[0] + p_building->U.UObject.MinY[0] + 500;
                p_building->U.UObject.RaiseY[0] += p_building->U.UObject.RaiseDY[0];
            }
            else
            {
                obj = p_building->U.UObject.Object;
                game_objects[obj + 1].OffsetY = p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 3].OffsetY = p_building->U.UObject.MinY[0] + 500;
            }
        }
        else
        {
            p_building->U.UObject.Timer[0]--;
        }
        if (p_building->U.UObject.Timer[1] == 0)
        {
            if (p_building->U.UObject.RaiseY[1] < 0)
            {
                apply_object_morph((ushort)(p_building->U.UObject.Object + 2), dx, dz, 0);
                apply_object_morph((ushort)(p_building->U.UObject.Object + 4), (short)(-dx), (short)(-dz), 1);
                obj = p_building->U.UObject.Object;
                game_objects[obj + 2].OffsetY = p_building->U.UObject.RaiseY[1] + p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 4].OffsetY = p_building->U.UObject.RaiseY[1] + p_building->U.UObject.MinY[0] + 500;
                p_building->U.UObject.RaiseY[1] += p_building->U.UObject.RaiseDY[1];
            }
            else
            {
                obj = p_building->U.UObject.Object;
                game_objects[obj + 2].OffsetY = p_building->U.UObject.MinY[0] + 500;
                game_objects[obj + 4].OffsetY = p_building->U.UObject.MinY[0] + 500;
            }
        }
        else
        {
            p_building->U.UObject.Timer[1]--;
        }
        if ((p_building->U.UObject.RaiseY[0] < 0) || (p_building->U.UObject.RaiseY[1] < 0))
            break;
        obj = p_building->U.UObject.Object;
        p_building->U.UObject.RaiseY[0] = 0;
        p_building->U.UObject.RaiseY[1] = 0;
        game_objects[obj + 1].OffsetY = p_building->U.UObject.MinY[0] + 500;
        game_objects[obj + 3].OffsetY = p_building->U.UObject.MinY[0] + 500;
        game_objects[obj + 2].OffsetY = p_building->U.UObject.MinY[0] + 500;
        game_objects[obj + 4].OffsetY = p_building->U.UObject.MinY[0] + 500;
        p_building->Flag = 0x80;
        p_building->State = 19;
        break;
    case 19: // waiting for a trigger flag
        if ((p_building->Flag & 0x20) != 0)
        {
            p_building->State = 13;
            p_building->Flag = 0;
            p_building->U.UObject.Timer[0] = 3;
            p_building->U.UObject.Timer[1] = 0;
            p_building->U.UObject.RaiseDY[0] = -50;
            p_building->U.UObject.RaiseDY[1] = -50;
        }
        if ((p_building->Flag & 0x100) != 0)
        {
            p_building->State = 15;
            p_building->Flag = 0;
            p_building->U.UObject.Timer[0] = 0;
            p_building->U.UObject.Timer[1] = 3;
            p_building->U.UObject.RaiseDY[0] = 50;
            p_building->U.UObject.RaiseDY[1] = 50;
        }
        break;
    default:
        break;
    }
}

void process_bld36(struct Thing *p_building)
{
    struct M33 *p_matx;
    assert(p_building->U.UObject.MatrixIndex < next_local_mat);
    p_matx = &local_mats[p_building->U.UObject.MatrixIndex];
    rotate_object_axis(p_matx, 0, 0, 32);
}

void process_building(struct Thing *p_building)
{
#if 0
    asm volatile ("call ASM_process_building\n"
        : : "a" (p_building));
#endif
    if ((p_building->Flag & TngF_Unkn00040000) != 0)
    {
        if ((p_building->Flag & TngF_Destroyed) != 0)
        {
            p_building->Flag &= ~TngF_Unkn00040000;
            return;
        }
        collapse_building(p_building->X >> 8, p_building->Y >> 8, p_building->Z >> 8, p_building);
        return;
    }

    switch (p_building->SubType)
    {
    case SubTT_BLD_SHUTLDR:
        if ((p_building->Flag & TngF_Destroyed) == 0)
        {
            if ((p_building->U.UObject.PrevThing == 0) && ((p_building->Flag & TngF_Unkn08000000) == 0)) {
                process_shuttle_loader(p_building);
            }
            if ((p_building->U.UObject.PrevThing == 0) && ((p_building->Flag & TngF_Unkn08000000) != 0)
              && ((LbRandomAnyShort() & 3) == 0) && (((gameturn + p_building->ThingOffset) & 7) == 0)) {
                create_a_pod(0, p_building, 0);
            }
        }
        break;
    case SubTT_BLD_DOME:
        if ((p_building->Flag & TngF_Destroyed) == 0)
            process_dome1(p_building);
        break;
    case SubTT_BLD_MGUN:
        if ((p_building->Flag & TngF_Destroyed) == 0)
            process_mounted_gun(p_building);
        break;
    case SubTT_BLD_GATE:
        process_gate1(p_building);
        break;
    case SubTT_BLD_MOVN_ROTOR:
        process_bld36(p_building);
        break;
    default:
        if (p_building->State == BldSt_OBJ_UNKN09) {
            collapse_building(p_building->X >> 8, p_building->Y >> 8, p_building->Z >> 8, p_building);
        }
        break;
    }
}

int building_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    if ((p_attacker != NULL) && (p_attacker->Type == TT_PERSON)) {
        p_attacker->U.UPerson.Flag3 |= PrsF3_Unkn40;
    }
    if (p_thing->SubType != 32)
    {
        if ((p_thing->Flag & TngF_Destroyed) == 0)
        {
            int health;

            play_dist_sample(p_thing, 65, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            health = p_thing->Health - (hp >> 1);
            if (health < 0) {
                collapse_building(PRCCOORD_TO_MAPCOORD(p_thing->X),
                  PRCCOORD_TO_MAPCOORD(p_thing->Y), PRCCOORD_TO_MAPCOORD(p_thing->Z), p_thing);
            }
            p_thing->Health = health;
        }
        return hp;
    }
    p_thing->Flag |= TngF_Unkn01000000;
    if ((p_thing->Flag & TngF_Destroyed) == 0)
    {
        int health_decr;

        health_decr = hp >> 1 >> 1;
        p_thing->Health -= health_decr;
        p_thing->U.UObject.Cost = 5;
        if (p_thing->Health <= 0)
        {
            init_mgun_explode(p_thing);
            return p_thing->Health + health_decr;
        }
    }
    return 0;
}

/******************************************************************************/
