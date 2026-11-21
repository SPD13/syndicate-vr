/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vehicle.c
 *     Routines implementing vehicle.
 * @par Purpose:
 *     Implement functions for handling vehicle things.
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
#include "vehicle.h"

#include "bfmath.h"
#include "bfmemut.h"
#include "bfutility.h"
#include "poly.h"
#include "ssampply.h"
#include "frame_sprani.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bigmap.h"
#include "bmbang.h"
#include "bmcrater.h"
#include "difftest.h"
#include "engincam.h"
#include "engincolour.h"
#include "enginfexpl.h"
#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "engintrns.h"
#include "febrief.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvwalk.h"
#include "matrix.h"
#include "misstat.h"
#include "pathtrig.h"
#include "people.h"
#include "sound.h"
#include "thing.h"
#include "thing_expld.h"
#include "thing_fire.h"
#include "thing_onface.h"
#include "swlog.h"
#include "weapon.h"
/******************************************************************************/

#pragma pack(1)

// TODO Maybe this is the same as players count, maybe not; owner field does not typically store player number
#define MECH_OWNER_LIMIT 8

struct unkn_mech_struc1 { // sizeof=0x11
    ubyte field_0;
    s32 field_1;
    s32 field_5;
    s32 field_9;
    s32 field_D;
};

struct unkn_mech_struc2 { // sizeof=0x89
    s32 field_0;
    ubyte field_4[132];
    ubyte field_88;
};

struct unkn_mech_struc3 { // sizeof=0x76
    struct unkn_mech_struc4 *mech3_arr4_ptr;
    s32 mech3_unkn_arr4_idx;
    s32 mech3_unkn_fld_8;
    struct M33 mech3_unkn_mat_C;
    s32 mech3_unkn_X;
    s32 mech3_unkn_Y;
    s32 mech3_unkn_Z;
    ubyte mech3_unkn_fld_3C[13];
    s32 mech3_unkn_fld_49;
    ubyte mech3_unkn_fld_4D[7];
    s32 mech3_unkn_fld_54[3];
    ubyte field_60;
    ubyte field_61[3];
    s32 field_64;
    s32 field_68;
    s32 field_6C;
    ubyte field_70[2];
    ubyte field_72[2];
    ubyte field_74;
    ubyte field_75;
};

struct unkn_mech_struc4 { // sizeof=0x7A
    short thing;
    ubyte field_2[33];
    short field_23;
    s32 field_25;
    s32 field_29;
    s32 field_2D;
    s32 field_31;
    s32 field_35;
    s32 field_39;
    s32 field_3D;
    s32 field_41;
    s32 field_45;
    s32 angle_49;
    s32 angle_4D;
    s32 angle_51;
    s32 field_55;
    s32 field_59;
    ubyte field_5D[4];
    struct M31 field_61;
    struct M31 field_6D;
    ubyte field_79;
};

struct unkn_mech_struc5_s1 { // sizeof=24
    s32 unkst5sub_04;
    s32 unkst5sub_08;
    s32 unkst5sub_0C;
    s32 unkst5sub_10;
    s32 unkst5sub_14;
    ubyte unkst5sub_18[4];
};

struct unkn_mech_struc5 { // sizeof=0x1A9
    s32 field_0;
    struct unkn_mech_struc5_s1 field_4[16];
    ubyte field_184[8];
    ubyte field_18C[24];
    ubyte field_1A4[4];
    ubyte field_1A8;
};

struct unkn_mech_struc7 { // sizeof=0x13488
    struct unkn_mech_struc3 field_0[MECH_OWNER_LIMIT];
    struct unkn_mech_struc4 field_3B0[256];
    struct unkn_mech_struc5 field_7DB0[64];
    struct unkn_mech_struc1 field_E7F0[1024];
    ubyte field_12BF0[8];
    struct unkn_mech_struc2 field_12BF8[16];
};

#pragma pack()

/** Configuration options for each person state.
 */
struct VehStateConfig vehicle_states[] = {
  {"NONE_STATE",},
  {"UNKN_1",},
  {"UNKN_2",},
  {"UNKN_3",},
  {"UNKN_4",},
  {"UNKN_5",},
  {"UNKN_6",},
  {"UNKN_7",},
  {"UNKN_8",},
  {"UNKN_9",},
  {"UNKN_A",},
  {"UNKN_B",},
  {"UNKN_C",},
  {"UNKN_D",},
  {"UNKN_E",},
  {"UNKN_F",},
  {"UNKN_10",},
  {"WANDER",},
  {"UNKN_12",},
  {"UNKN_13",},
  {"UNKN_14",},
  {"UNKN_15",},
  {"UNKN_16",},
  {"UNKN_17",},
  {"UNKN_18",},
  {"UNKN_19",},
  {"UNKN_1A",},
  {"UNKN_1B",},
  {"UNKN_1C",},
  {"UNKN_1D",},
  {"UNKN_1E",},
  {"UNKN_1F",},
  {"UNKN_20",},
  {"PARKED_PARAL",},
  {"UNKN_22",},
  {"UNKN_23",},
  {"UNKN_24",},
  {"UNKN_25",},
  {"UNKN_26",},
  {"UNKN_27",},
  {"UNKN_28",},
  {"UNKN_29",},
  {"UNKN_2A",},
  {"UNKN_2B",},
  {"UNKN_2C",},
  {"UNKN_2D",},
  {"UNKN_2E",},
  {"UNKN_2F",},
  {"UNKN_30",},
  {"UNKN_31",},
  {"GOTO_LOC",},
  {"UNKN_33",},
  {"UNKN_34",},
  {"UNKN_35",},
  {"UNKN_36",},
  {"UNKN_37",},
  {"UNKN_38",},
  {"UNKN_39",},
  {"UNKN_3A",},
  {"FLY_LANDING",},
  {"UNKN_3C",},
  {"PARKED_PERPN",},
  {"UNKN_3E",},
  {"UNKN_3F",},
  {"UNKN_40",},
  {"UNKN_41",},
  {"UNKN_42",},
  {"UNKN_43",},
  {"UNKN_44",},
  {"UNKN_45",},
  {"UNKN_46",},
  {"UNKN_47",},
  {"UNKN_48",},
  {"UNKN_49",},
  {"UNKN_4A",},
  {"UNKN_4B",},
  {"UNKN_4C",},
  {"UNKN_4D",},
  {"UNKN_4E",},
  {"UNKN_4F",},
  {"UNKN_50",},
  {"UNKN_51",},
  {"UNKN_52",},
};

void *dword_177750 = NULL;
struct unkn_mech_struc7 *unkn_mech_stct7 = NULL;

struct unkn_mech_struc3 *unkn_mech_arr3 = NULL;
struct unkn_mech_struc4 *unkn_mech_arr4 = NULL;
struct unkn_mech_struc5 *unkn_mech_arr5 = NULL;
struct unkn_mech_struc1 *unkn_mech_arr1 = NULL;
ubyte *unkn_mech_arr6 = NULL;
struct unkn_mech_struc2 *unkn_mech_arr2 = NULL;
struct M33 unkn_mech_mat8;

s32 mech_last_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_last_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_last_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

// to be removed after ASM use is gone
s32 mech_unkn_tile_y2;
s32 mech_unkn_tile_x3;
s32 mech_unkn_tile_y3;

s32 mech_rocket1_launch_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_rocket1_launch_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_rocket1_launch_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

s32 mech_rocket2_launch_cor_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);
s32 mech_rocket2_launch_cor_y = TILE_TO_MAPCOORD(1, 0);
s32 mech_rocket2_launch_cor_z = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 127);

s32 unkn_mech_var10 = 0;
s32 unkn_mech_var11 = 0;

const char *vehicle_type_name(ushort vtype)
{
#if 0
// Enable when we have vehicle config file with names
    struct VehicleDef *p_vhdef;

    p_vhdef = &vehicle_def[vtype];
    if (strlen(p_vhdef->Name) == 0)
        return "OUTRNG_VEHICLE";
    return p_vhdef->Name;
#endif
    switch (vtype)
    {
    case SubTT_VEH_TRAIN:
        return "TRAIN";
    case SubTT_VEH_GROUND:
        return "VEH_GROUND";
    case SubTT_VEH_SHUTTLE_POD:
        return "SHUTTLE_POD";
    case SubTT_VEH_FLYING:
        return "VEH_FLYING";
    case SubTT_VEH_TANK:
        return "TANK";
    case SubTT_VEH_SHIP:
        return "SHIP";
    case SubTT_VEH_MECH:
        return "MECH";
    default:
        break;
    }
    return "VEHICLE";
}

void snprint_vehicle_state(char *buf, ulong buflen, struct Thing *p_thing)
{
    char *s;
    //ubyte nparams;
    struct VehStateConfig *p_vstatcfg;

    p_vstatcfg = &vehicle_states[p_thing->State];
    s = buf;

    sprintf(s, "%s( ", p_vstatcfg->Name);
    s += strlen(s);
    //nparams = 0;

    // TODO support parameters of states

    snprintf(s, buflen - (s-buf), " )");
}

void mech_clear_all_fld72(void)
{
    ushort i;
    ushort owner;

    for (owner = 0; owner < MECH_OWNER_LIMIT; owner++)
    {
        struct unkn_mech_struc3 *p_u3itm;

        p_u3itm = &unkn_mech_arr3[owner];
        for (i = 0; i < 2; i++) {
            p_u3itm->field_72[i] = 0;
        }
    }
}

void mech_unkn_func_02(void)
{
#if 0
    asm volatile ("call ASM_mech_unkn_func_02\n"
        :  :  : "eax" );
    return;
#endif
    unkn_mech_var10 = 0;
    unkn_mech_var11 = 0;

    mech_clear_all_fld72();
}

int load_mech_dat(const char *fname)
{
#if 0
    int ret;
    asm volatile ("call ASM_load_mech_dat\n"
        : "=r" (ret) : "a" (fname));
    return ret;
#endif
    TbFileHandle fh;
    int i, k;

    fh = LbFileOpen(fname, Lb_FILE_MODE_READ_ONLY);
    if (fh == INVALID_FILE) {
        return 0;
    }

    for (i = 0; i < 64; i++)
    {
        struct unkn_mech_struc5 *p_u5itm;

        p_u5itm = &unkn_mech_arr5[i];
        LbFileRead(fh, p_u5itm, sizeof(struct unkn_mech_struc5));

        for (k = 0; k < 17; k++)
        {
            p_u5itm->field_4[k].unkst5sub_0C = 0x800 - p_u5itm->field_4[k].unkst5sub_0C;
            p_u5itm->field_4[k].unkst5sub_14 = 0x800 - p_u5itm->field_4[k].unkst5sub_14;
            p_u5itm->field_4[k].unkst5sub_0C &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_10 &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_14 &= 0x7FF;
            p_u5itm->field_4[k].unkst5sub_04 = -p_u5itm->field_4[k].unkst5sub_04;
        }
    }

    for (i = 0; i < 16; i++) {
        LbFileRead(fh, &unkn_mech_arr2[i], sizeof(struct unkn_mech_struc2));
    }

    for (i = 0; i < 1024; i++) {
        LbFileRead(fh, &unkn_mech_arr1[i], sizeof(struct unkn_mech_struc1));
    }

    LbFileClose(fh);
    return 1;
}

void init_mech(void)
{
#if 0
    asm volatile ("call ASM_init_mech\n"
        :  :  : "eax" );
#endif
    u32 a1idx;

    unkn_mech_arr4 = &unkn_mech_stct7->field_3B0[0];
    unkn_mech_arr5 = &unkn_mech_stct7->field_7DB0[0];
    unkn_mech_arr1 = &unkn_mech_stct7->field_E7F0[0];
    unkn_mech_arr6 = unkn_mech_stct7->field_12BF0;
    unkn_mech_arr2 = &unkn_mech_stct7->field_12BF8[0];
    unkn_mech_arr3 = &unkn_mech_stct7->field_0[0];
    memset(unkn_mech_stct7, 0, sizeof(struct unkn_mech_struc7));

    memset(&unkn_mech_mat8, 0, sizeof(struct M33));
    unkn_mech_mat8.R[0][0] = 0x4000;
    unkn_mech_mat8.R[1][1] = 0x4000;
    unkn_mech_mat8.R[2][2] = 0x4000;

    load_mech_dat("data/mech.dat");

    mech_clear_all_fld72();
    unkn_mech_arr3->field_6C = 0;
    a1idx = unkn_mech_arr2[unkn_mech_arr3->field_6C].field_0;
    unkn_mech_arr3->field_64 = a1idx;
    unkn_mech_arr3->mech3_unkn_fld_54[2] = unkn_mech_arr1[a1idx].field_9;
}

void mech_gameturn_reinit(void)
{
    // Mech rocket coordinates should be recomputed later using proper matrix
    // transforms; but to avoid them being unset, init to last body location
    //TODO why do we think there is a chance of the coords not being properly updated from things?
    mech_rocket1_launch_cor_x = mech_last_cor_x;
    mech_rocket1_launch_cor_y = mech_last_cor_y;
    mech_rocket1_launch_cor_z = mech_last_cor_z;

    mech_rocket2_launch_cor_x = mech_last_cor_x;
    mech_rocket2_launch_cor_y = mech_last_cor_y;
    mech_rocket2_launch_cor_z = mech_last_cor_z;
}

TbBool vehicle_is_destroyed(ThingIdx thing)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];
    return thing_is_destroyed(thing) || (p_thing->Type != TT_VEHICLE);
}
extern ushort game_level_unique_id;

static DIFFTEST_REF_FUNC int init_unkn1thing_asm(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[4];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    stkargs[2] = a7;
    stkargs[3] = a8;
    asm volatile ("push 12(%4)\n""push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_init_unkn1thing\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments are the raw registers and stack
 * args the asm takes: a1 = source matrix, a2/a3/a4 = X/Y/Z,
 * a5/a6/a7 = unkn2_pos x/y/z, a8 = prim object offset.
 * Returns the new thing index.
 */
int init_unkn1thing(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    struct M33 *p_src_mat = (struct M33 *)a1;
    struct Thing *p_thing;
    struct SingleObject *p_snobj;
    short thing;
    short matx;

    DIFFTEST_SHADOW_RET(init_unkn1thing, int, init_unkn1thing_asm(a1, a2, a3, a4, a5, a6, a7, a8));
    thing = get_new_thing();
    p_thing = &things[(ushort)thing];
    p_thing->VX = 0;
    p_thing->VZ = 0;
    p_thing->VY = 0;
    p_thing->U.UVehicle.AngleX = 0;
    p_thing->U.UVehicle.AngleY = 0;
    p_thing->U.UVehicle.AngleZ = 0;
    p_thing->Type = 0x23;
    p_thing->Radius = 0x96;
    p_thing->Parent = 0;
    p_thing->Speed = 0;
    p_thing->StartFrame = 0;
    p_thing->State = 0;
    p_thing->Flag = 0;
    p_thing->X = a2;
    p_thing->U.UVehicle.Object = next_object;
    p_thing->U.UVehicle.MatrixIndex = next_local_mat;
    p_thing->Z = a4;
    p_thing->Y = a3;
    matx = p_thing->U.UVehicle.MatrixIndex;
    next_local_mat++;
    matrix_identity_fill(&local_mats[matx]);
    matx = p_thing->U.UVehicle.MatrixIndex;
    memcpy(&local_mats[matx], p_src_mat, sizeof(struct M33));
    p_thing->Flag |= 0x04000000;
    p_thing->U.UVehicle.UniqueID = game_level_unique_id;
    game_level_unique_id++;
    unkn2_pos_x = a5;
    unkn2_pos_y = a6;
    unkn2_pos_z = a7;
    copy_prim_obj_to_game_object((ushort)(a2 >> 8), (ushort)(a4 >> 8),
      (short)(-prim_unknprop01 - a8), (ushort)(a3 >> 8));
    unkn_object_shift_03((ushort)(next_object - 1));
    p_snobj = &game_objects[next_object - 1];
    p_snobj->ThingNo = thing;
    DIFFTEST_RETURN((int)thing);
}


void mech_unkn_func_09(ThingIdx thing)
{
    asm volatile ("call ASM_mech_unkn_func_09\n"
        : : "a" (thing));
}

void veh_add(struct Thing *p_vehicle, short frame)
{
#if 0
    asm volatile ("call ASM_veh_add\n"
        : : "a" (p_vehicle), "d" (frame));
    return;
#endif
    struct Thing *p_mgun;
    struct SingleObject *p_snobj;
    short coord_x, coord_y, coord_z;
    short mgun;
    ushort snobj;
    ushort matx;

    switch (frame)
    {
    case VehOM_MBTANK:
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -19 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x2800;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    case 26: // Replaced tank (object slots reused for mech)
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -27 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x1E00;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    case VehOM_CLAWTANK:
        mgun = get_new_thing();
        if (mgun <= 0) {
            LOGERR("No thing slots for mounted gun");
            break;
        }
        p_vehicle->U.UVehicle.SubThing = mgun;
        p_vehicle->SubType = SubTT_VEH_TANK;

        p_mgun = &things[mgun];
        p_mgun->U.UMGun.NextThing = 0;
        p_mgun->U.UVehicle.SubThing = 0;
        assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
        assert(next_local_mat < LOCAL_MATS_COUNT);
        matx = next_local_mat++;
        LbMemoryCopy(&local_mats[matx], &local_mats[p_vehicle->U.UVehicle.MatrixIndex], sizeof(struct M33));
        p_mgun->U.UMGun.MatrixIndex = matx;
        p_mgun->StartFrame = frame;

        coord_x = PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        coord_y = PRCCOORD_TO_MAPCOORD(p_vehicle->Y);
        coord_z = PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        snobj = copy_prim_obj_to_game_object(coord_x, coord_z, -30 - prim_unknprop01, coord_y + 20);
        p_mgun->X = 0;
        p_mgun->Y = 0x1E00;
        p_mgun->Z = 0;
        unkn_object_shift_03(snobj);
        p_mgun->Type = TT_VEH_TURRET;
        p_mgun->U.UMGun.Object = snobj;
        p_snobj = &game_objects[snobj];
        p_snobj->ThingNo = p_mgun - things;
        break;
    default:
        p_vehicle->U.UVehicle.SubThing = 0;
        break;
    }
    p_vehicle->U.UVehicle.WobbleZP = 0;
    p_vehicle->U.UVehicle.WobbleZV = 0;
}

ushort veh_passenger_count(struct Thing *p_veh)
{
    struct Thing *p_thing;
    ThingIdx thing;
    ushort c;

    c = 0;

    thing = p_veh->U.UVehicle.PassengerHead;
    while (thing > 0 && c < THINGS_LIMIT)
    {
        c++;
        p_thing = &things[thing];
        thing = p_thing->U.UPerson.LinkPassenger;
    }
    return c;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void VNAV_find_nearnodes_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_find_nearnodes\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Nodes nearest to the point, filled by VNAV_find_nearnodes().
 * The asm data spans VNAV_nearnodes (6 bytes) and data_1c8540 (4 bytes).
 */
extern short VNAV_nearnodes[5];
extern s32 VNAV_nodedists[5];

/** Finds the 5 traffic nodes nearest to point (a1,a2) in map coords,
 * storing their indices in VNAV_nearnodes[] and squared distances
 * in VNAV_nodedists[].
 */
void VNAV_find_nearnodes(int a1, int a2, int a3, int a4)
{
    struct TrafficNode *p_tnode;
    short cor_x, cor_z;
    s32 dist, max_dist, dx, dz;
    int i, tnode, max_i;

    DIFFTEST_SHADOW(VNAV_find_nearnodes, VNAV_find_nearnodes_asm(a1, a2, a3, a4));
    cor_x = a1;
    cor_z = a2;
    for (i = 0; i < 5; i++)
    {
        VNAV_nearnodes[i] = 0;
        VNAV_nodedists[i] = 0xfffffff;
    }
    max_dist = 0xfffffff;
    max_i = 0;
    for (tnode = 1; tnode < next_traffic_node; tnode++)
    {
        p_tnode = &game_traffic_nodes[tnode];
        if (p_tnode->Flags == 0xff)
            continue;
        if ((((ubyte *)&p_tnode->Flags)[1] & 0x0a) != 0)
            continue;
        dx = (s32)cor_x - (s32)p_tnode->X;
        dz = (s32)cor_z - (s32)p_tnode->Z;
        dist = (s32)((u32)dz * (u32)dz + (u32)dx * (u32)dx);
        if (dist >= max_dist)
            continue;
        VNAV_nearnodes[max_i] = tnode;
        VNAV_nodedists[max_i] = dist;
        max_dist = VNAV_nodedists[0];
        max_i = 0;
        for (i = 1; i < 5; i++)
        {
            if (VNAV_nodedists[i] > max_dist)
            {
                max_i = i;
                max_dist = VNAV_nodedists[i];
            }
        }
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void VNAV_qwertyuiop_asm(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[4];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    stkargs[2] = a7;
    stkargs[3] = a8;
    asm volatile ("push 12(%4)\n""push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_VNAV_qwertyuiop\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Distance of point (a7,a8) from segment (a3,a4)-(a5,a6).
 * Stores approximate distance in *a1, and in *a2 the position along
 * the segment (0 before start, 0xff past end, else 256*fraction).
 */
void VNAV_qwertyuiop(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    s32 *p_dist = (s32 *)a1;
    s32 *p_frac = (s32 *)a2;
    s32 dx1, dy1, dx2, dy2;
    s32 ex, ey, fx, fy;
    s32 dot1, dot2, cross;
    s32 adx, ady;
    s32 m, n;

    DIFFTEST_SHADOW(VNAV_qwertyuiop, VNAV_qwertyuiop_asm(a1, a2, a3, a4, a5, a6, a7, a8));
    dx1 = a5 - a3;
    dy1 = a6 - a4;
    dx2 = a7 - a3;
    dy2 = a8 - a4;
    dot1 = (s32)((u32)dx2 * (u32)dx1 + (u32)dy2 * (u32)dy1);
    if (dot1 <= 0)
    {
        *p_frac = 0;
        m = abs(dx2);
        n = abs(dy2);
        if (m > n)
            n >>= 1;
        else
            m >>= 1;
        *p_dist = n + m;
        return;
    }
    ex = a3 - a5;
    ey = a4 - a6;
    fx = a7 - a5;
    fy = a8 - a6;
    dot2 = (s32)((u32)fx * (u32)ex + (u32)ey * (u32)fy);
    if (dot2 <= 0)
    {
        *p_frac = 0xff;
        m = abs(fx);
        n = abs(fy);
        if (m > n)
            n >>= 1;
        else
            m >>= 1;
        *p_dist = n + m;
        return;
    }
    cross = (s32)((u32)dx2 * (u32)dy1 - (u32)dy2 * (u32)dx1);
    adx = abs(dx1);
    ady = abs(dy1);
    cross = abs(cross);
    dot1 = abs(dot1);
    m = adx;
    n = ady;
    if (m > n)
        n >>= 1;
    else
        m >>= 1;
    *p_dist = cross / (m + n);
    m = (s32)((u32)adx * (u32)adx + (u32)ady * (u32)ady);
    *p_frac = dot1 / (m >> 8);
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void VNAV_find_start_dropoff_point_asm(int a1, int a2, int a3, int a4, int a5)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[1];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    asm volatile ("push 0(%4)\n""call ASM_VNAV_find_start_dropoff_point\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Among links of the nodes in VNAV_nearnodes[], finds the one whose
 * segment is closest to point (a4,a5). Stores the node in *a1, link
 * index in *a2 and position along the segment in *a3.
 */
void VNAV_find_start_dropoff_point(int a1, int a2, int a3, int a4, int a5)
{
    short *p_tnode_out = (short *)a1;
    ubyte *p_link_out = (ubyte *)a2;
    s32 *p_frac_out = (s32 *)a3;
    struct TrafficNode *p_tnode;
    struct TrafficNode *p_lnode;
    s32 best_dist, dist, frac;
    s32 node_x, node_z;
    short link;
    int i, j;

    DIFFTEST_SHADOW(VNAV_find_start_dropoff_point,
      VNAV_find_start_dropoff_point_asm(a1, a2, a3, a4, a5));
    if (VNAV_nearnodes[0] == 0)
        return;
    best_dist = 0xfffffff;
    *p_tnode_out = 9999;
    for (i = 0; i < 5; i++)
    {
        p_tnode = &game_traffic_nodes[VNAV_nearnodes[i]];
        node_x = p_tnode->X;
        node_z = p_tnode->Z;
        for (j = 0; j < 4; j++)
        {
            link = p_tnode->UTraffic.Link[j];
            if (link >= 0)
                continue;
            if ((p_tnode->UTraffic.Flags[j] & 0x40) != 0)
                continue;
            p_lnode = &game_traffic_nodes[-link];
            VNAV_qwertyuiop((int)&dist, (int)&frac, node_x, node_z,
              p_lnode->X, p_lnode->Z, (short)a4, (short)a5);
            if (dist >= best_dist)
                continue;
            *p_tnode_out = VNAV_nearnodes[i];
            *p_link_out = j;
            *p_frac_out = frac;
            best_dist = dist;
        }
    }
}



int func_4225c(int a1, int a2, int a3, int a4);
int rotate_obj(int a1, int a2, int a3, int a4);

static DIFFTEST_REF_FUNC void VNAV_unkn_func_207_asm(struct Thing *p_thing)
{
    asm volatile ("call ASM_VNAV_unkn_func_207\n"
        : : "a" (p_thing));
}

static void VNAV_unkn_func_207_radius_health(struct Thing *p_thing)
{
    ushort radius;
    s32 health;

    radius = func_4225c(p_thing->U.UVehicle.Object, 0, 0, 0);
    p_thing->Radius = radius;
    health = 5 * (s32)radius * p_thing->U.UVehicle.Armour;
    if (health > 0x7d00)
        p_thing->Health = 0x7d00;
    else
        p_thing->Health = (short)(p_thing->Radius * 5) * p_thing->U.UVehicle.Armour;
}

void VNAV_unkn_func_207(struct Thing *p_thing)
{
    struct TrafficNode *p_tnode1;
    struct TrafficNode *p_tnode2;
    struct M33 *p_mat;
    s32 frac;
    short tnode;
    ubyte link;
    short cor_x, cor_z;
    s32 nx, nz;
    ThingIdx tng_offs;
    short angle;

    DIFFTEST_SHADOW(VNAV_unkn_func_207, VNAV_unkn_func_207_asm(p_thing));
    switch (p_thing->SubType)
    {
    case SubTT_VEH_TANK:
        p_thing->Speed = 0;
        p_thing->State = VehSt_PARKED_PARAL;
        VNAV_unkn_func_207_radius_health(p_thing);
        p_thing->Speed = 0;
        p_thing->U.UVehicle.PathIndex = 0;
        p_thing->PathOffset = 0;
        p_thing->U.UVehicle.WorkPlace = 0;
        p_thing->U.UVehicle.AngleDY = 0;
        p_thing->U.UVehicle.MaxSpeed = 0;
        p_thing->U.UVehicle.MaxHealth = p_thing->Health;
        p_thing->U.UVehicle.ZebraOldHealth = p_thing->Health;
        return;
    case SubTT_VEH_SHIP:
        p_thing->Speed = 0;
        p_thing->U.UVehicle.LeisurePlace = 0;
        VNAV_unkn_func_207_radius_health(p_thing);
        p_thing->U.UVehicle.WorkPlace = 0;
        p_thing->U.UVehicle.AngleDY = 0;
        p_thing->U.UVehicle.MaxSpeed = 0;
        p_thing->U.UVehicle.PathIndex = 0;
        p_thing->PathOffset = 0;
        p_thing->U.UVehicle.MaxHealth = p_thing->Health;
        p_thing->U.UVehicle.ZebraOldHealth = p_thing->Health;
        return;
    case SubTT_VEH_FLYING:
        VNAV_unkn_func_207_radius_health(p_thing);
        p_thing->Speed = 0;
        p_thing->U.UVehicle.PathIndex = 0;
        p_thing->PathOffset = 0;
        p_thing->U.UVehicle.WorkPlace = 0;
        p_thing->U.UVehicle.AngleDY = 0;
        p_thing->U.UVehicle.MaxSpeed = 0;
        p_thing->U.UVehicle.MaxHealth = p_thing->Health;
        p_thing->U.UVehicle.ZebraOldHealth = p_thing->Health;
        return;
    case SubTT_VEH_MECH:
        p_thing->Radius = 300;
        p_thing->Health = 25000;
        p_thing->Speed = 0;
        p_thing->U.UVehicle.PathIndex = 0;
        p_thing->PathOffset = 0;
        p_thing->U.UVehicle.WorkPlace = 0;
        p_thing->U.UVehicle.AngleDY = 0;
        p_thing->U.UVehicle.MaxSpeed = 0;
        p_thing->U.UVehicle.MaxHealth = p_thing->Health;
        p_thing->U.UVehicle.ZebraOldHealth = p_thing->Health;
        return;
    default:
        break;
    }

    p_thing->U.UVehicle.MaxSpeed = 0;
    p_thing->U.UVehicle.PathIndex = 0;
    p_thing->PathOffset = 0;
    p_thing->Speed = 0;
    p_thing->U.UVehicle.WorkPlace = 0;
    p_thing->U.UVehicle.AngleDY = 0;
    p_thing->U.UVehicle.AccelZ = 0;
    p_thing->U.UVehicle.AccelX = 0;
    cor_x = p_thing->X >> 8;
    cor_z = p_thing->Z >> 8;
    p_thing->U.UVehicle.ZebraOldHealth = p_thing->Health;
    p_thing->U.UVehicle.MaxSpeed = 0;
    VNAV_find_nearnodes(cor_x, cor_z, cor_z, cor_x);
    // TODO the asm leaves these uninitialized; the callee may not write them
    frac = 0;
    tnode = 0;
    link = 0;
    VNAV_find_start_dropoff_point((int)&tnode, (int)&link, (int)&frac, cor_x, cor_z);

    p_thing->U.UVehicle.Dummy4b = 0;
    p_thing->U.UVehicle.Dummy4b |= -tnode;
    p_thing->U.UVehicle.Dummy4a &= 0xff3f;
    p_thing->U.UVehicle.Dummy4a |= ((ushort)link) << 6;
    p_tnode1 = &game_traffic_nodes[tnode];
    p_thing->U.UVehicle.TNode = p_tnode1->UTraffic.Link[link];

    if ((p_thing->U.UVehicle.TNode < 0) &&
      (game_traffic_nodes[-p_thing->U.UVehicle.TNode].Flags & 0x0800))
    {
        p_thing->U.UVehicle.Dummy4a &= 0xffc0;
        p_thing->State = VehSt_PARKED_PERPN;
        p_thing->U.UVehicle.Dummy4a |= 0x13;
        p_tnode1 = &game_traffic_nodes[tnode];
        p_tnode2 = &game_traffic_nodes[-p_thing->U.UVehicle.TNode];
        nx = (((p_tnode2->X - p_tnode1->X) * frac) >> 8) + p_tnode1->X;
        nz = (((p_tnode2->Z - p_tnode1->Z) * frac) >> 8) + p_tnode1->Z;
        nx <<= 8;
        nz <<= 8;
        tng_offs = p_thing->ThingOffset;
        p_thing->ThingOffset = p_thing - things;
        move_mapwho(p_thing, nx, p_thing->Y, nz);
        p_tnode2 = &game_traffic_nodes[-p_thing->U.UVehicle.TNode];
        p_thing->ThingOffset = tng_offs;
        angle = arctan(p_tnode2->X - (p_thing->X >> 8),
          p_tnode2->Z - (p_thing->Z >> 8));
        p_thing->U.UVehicle.AngleY = angle;
        p_mat = &local_mats[p_thing->U.UVehicle.MatrixIndex];
        rotate_obj(0, (short)(-p_thing->U.UVehicle.AngleY), 0, (int)p_mat);
    }
    else
    {
        p_thing->State = VehSt_PARKED_PARAL;
        p_thing->U.UVehicle.Dummy4a &= 0xffc0;
    }

    VNAV_unkn_func_207_radius_health(p_thing);
    p_thing->U.UVehicle.EffectiveGroup = 0x63;
    p_thing->U.UVehicle.MaxHealth = p_thing->Health;
}

static DIFFTEST_REF_FUNC int check_for_a_vehicle_here_asm(int x, int z,
  struct Thing *p_vehicle)
{
    int ra = x, rd = z;
    struct Thing *rb = p_vehicle;
    asm volatile ("call ASM_check_for_a_vehicle_here\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
    return ra;
}

/** Check if a parked ground vehicle other than given one is at given point.
 * Coordinates are in map units (tile size 256).
 */
int check_for_a_vehicle_here(int x, int z, struct Thing *p_vehicle)
{
    s32 tile_x, tile_z, tx, tz;
    ThingIdx thing;

    DIFFTEST_SHADOW_RET(check_for_a_vehicle_here, int,
      check_for_a_vehicle_here_asm(x, z, p_vehicle));
    tile_x = x >> 8;
    tile_z = z >> 8;
    for (tx = tile_x - 2; tx <= tile_x + 2; tx++)
    {
        for (tz = tile_z - 2; tz <= tile_z + 2; tz++)
        {
            if (tx < 0 || tx > 127 || tz < 0 || tz > 127)
                continue;
            thing = game_my_big_map[128 * tz + tx].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                struct M33 *p_mat;
                s32 radius, mat02, mat22, off_x, off_z;
                s32 pos_x, pos_z, dist_x, dist_z, dist;

                if (thing <= 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                thing = p_thing->Next;
                if (p_thing->Type != TT_VEHICLE)
                    continue;
                if (p_thing == p_vehicle)
                    continue;
                if (p_thing->SubType != SubTT_VEH_GROUND)
                    continue;
                if (p_thing->State != VehSt_PARKED_PARAL)
                    continue;

                radius = (s32)((u32)p_thing->Radius * 400) >> 9;
                p_mat = &local_mats[p_thing->U.UVehicle.MatrixIndex];
                mat02 = -4 * p_mat->R[0][2];
                mat22 = -4 * p_mat->R[2][2];
                off_x = (mat02 * radius) >> 8;
                off_z = (mat22 * radius) >> 8;

                // Front point
                pos_x = (p_thing->X + off_x) >> 8;
                pos_z = (p_thing->Z + off_z) >> 8;
                dist_x = abs(x - pos_x);
                dist_z = abs(z - pos_z);
                if (dist_x > dist_z)
                    dist = dist_x + (dist_z >> 1);
                else
                    dist = dist_z + (dist_x >> 1);
                if (dist < radius)
                    DIFFTEST_RETURN((int)1);

                // Rear point
                pos_x = (p_thing->X - off_x) >> 8;
                pos_z = (p_thing->Z - off_z) >> 8;
                dist_x = abs(x - pos_x);
                dist_z = abs(z - pos_z);
                if (dist_x > dist_z)
                    dist = dist_x + (dist_z >> 1);
                else
                    dist = dist_z + (dist_x >> 1);
                if (dist < radius)
                    DIFFTEST_RETURN((int)1);
            }
        }
    }
    DIFFTEST_RETURN((int)0);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int person_poke_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_poke\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

short person_move(struct Thing *p_person);

int person_poke(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_other = (struct Thing *)a4;
    s32 dt_x, dt_z;
    s32 orig_dt_x, orig_dt_z;
    s32 dist_x, dist_z;
    s32 dist1, dist2;
    s32 old_vx, old_vz;
    short old_speed;
    int ret;

    DIFFTEST_SHADOW_RET(person_poke, int, person_poke_asm(a1, a2, a3, a4));
    orig_dt_z = a2;
    orig_dt_x = a3;
    dt_z = a2;
    dt_x = a3;

    dist_x = abs((p_person->X >> 8) - dt_x - (p_other->X >> 8));
    dist_z = abs((p_person->Z >> 8) + dt_z - (p_other->Z >> 8));
    if (dist_x > dist_z)
        dist1 = dist_x + (dist_z >> 1);
    else
        dist1 = dist_z + (dist_x >> 1);

    dist_x = abs((p_other->X >> 8) - (p_person->X >> 8)) + dt_x;
    dist_z = abs((p_other->Z >> 8) - (p_person->Z >> 8)) - dt_z;
    if (dist_x > dist_z)
        dist2 = dist_x + (dist_z >> 1);
    else
        dist2 = dist_z + (dist_x >> 1);

    if (dist2 > dist1)
    {
        dt_z = -dt_z;
        dt_x = -dt_x;
    }

    // TODO the asm mixes original and possibly negated deltas here
    old_vx = p_person->VX;
    p_person->VX = orig_dt_z - dt_x;
    old_vz = p_person->VZ;
    old_speed = p_person->Speed;
    p_person->VZ = orig_dt_x + dt_z;
    p_person->Speed = 16;
    ret = (ushort)person_move(p_person);
    // Return value is the raw eax: low byte from person_move(), second byte from flags
    ret = (ret & ~0xFF00) | ((p_person->Flag & 0xFF) << 8);
    if ((p_person->Flag & TngF_Destroyed) == 0)
    {
        p_person->VX = old_vx;
        p_person->VZ = old_vz;
        p_person->Speed = old_speed;
    }
    DIFFTEST_RETURN(ret);
}


void person_go_sleep(struct Thing *p_person);

static DIFFTEST_REF_FUNC void person_hit_by_car_asm(struct Thing *p_person, struct Thing *p_vehicle)
{
    asm volatile ("call ASM_person_hit_by_car\n"
        : : "a" (p_person), "d" (p_vehicle));
}

void person_hit_by_car(struct Thing *p_person, struct Thing *p_vehicle)
{
    struct MissionStatus *p_mistat;
    struct Thing *p_passngr;
    short dt_x, dt_z;
    short angle;
    ushort frm;
    ubyte agents_lost;
    ThingIdx passngr;

    DIFFTEST_SHADOW(person_hit_by_car, person_hit_by_car_asm(p_person, p_vehicle));
    dt_x = (p_person->X - p_vehicle->X) >> 8;
    dt_z = (p_person->Z - p_vehicle->Z) >> 8;
    person_poke((int)p_person, p_vehicle->VX, p_vehicle->VZ, (int)p_vehicle);
    // TODO the asm mixes the axes: VZ is added to X distance and VX to Z distance
    dt_z = dt_z - (short)p_vehicle->VX;
    dt_x = dt_x + (short)p_vehicle->VZ;
    angle = arctan(dt_x, -dt_z);

    p_person->U.UPerson.RecoilTimer = 9;
    p_person->U.UPerson.RecoilDir = (ubyte)(angle >> 3);
    p_person->U.UPerson.Angle = (ubyte)((ushort)((angle + 0x400) & 0x7FF) >> 8);
    if (((p_person->Flag & TngF_WepRecoil) == 0) && (p_person->U.UPerson.AnimMode != 16))
        p_person->U.UPerson.OldAnimMode = p_person->U.UPerson.AnimMode;
    p_person->U.UPerson.AnimMode = 16;
    frm = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->Frame = nstart_ani[frm + p_person->U.UPerson.Angle];
    p_person->U.UPerson.ShieldEnergy -= 35;
    p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    p_person->Flag |= TngF_WepRecoil;

    if (p_person->U.UPerson.ShieldEnergy > 0)
    {
        if ((p_person->U.UPerson.WeaponsCarried & (1 << (WEP_ENERGYSHLD-1))) != 0)
            p_person->Flag |= TngF_Unkn00200000;
        return;
    }
    if ((p_person->Flag & TngF_PlayerAgent) == 0)
    {
        if (p_person->SubType != SubTT_PERS_MECH_SPIDER)
            person_go_sleep(p_person);
        return;
    }
    p_person->Health -= 200;
    if (p_person->Health > 0)
        return;
    if ((p_person->State == PerSt_DIEING) || (p_person->State == PerSt_DEAD))
        return;

    agents_lost = 0;
    if (!in_network_game)
        agents_lost = mission_status[open_brief].AgentsLost;
    set_person_dead(p_person, 10);

    if (in_network_game)
    {
        passngr = p_vehicle->U.UVehicle.PassengerHead;
        if (passngr == 0)
            return;
        p_passngr = &things[(ushort)passngr];
        if ((p_passngr->Flag & TngF_PlayerAgent) == 0)
            return;
        if ((p_person->Flag & TngF_PlayerAgent) == 0)
            return;
        // TODO no range checks on the player indexes, same as the original
        p_mistat = &mission_status[(ushort)p_passngr->U.UPerson.ComCur >> 2];
        p_mistat->MP.AgentsKilled[(ushort)p_person->U.UPerson.ComCur >> 2]++;
        return;
    }

    passngr = p_vehicle->U.UVehicle.PassengerHead;
    if (passngr == 0)
        return;
    p_passngr = &things[(ushort)passngr];
    if (p_passngr->U.UPerson.EffectiveGroup != ingame.MyGroup)
        return;

    p_mistat = &mission_status[open_brief];
    switch (p_person->SubType)
    {
    case SubTT_PERS_AGENT:
    case SubTT_PERS_ZEALOT:
    case SubTT_PERS_PUNK_F:
    case SubTT_PERS_PUNK_M:
    case SubTT_PERS_HIGH_PRIEST:
        if (p_mistat->AgentsLost == agents_lost)
            p_mistat->SP.EnemiesKilled++;
        break;
    case SubTT_PERS_BRIEFCASE_M:
    case SubTT_PERS_WHITE_BRUN_F:
    case SubTT_PERS_SCIENTIST:
    case SubTT_PERS_SHADY_M:
    case SubTT_PERS_WHIT_BLOND_F:
    case SubTT_PERS_LETH_JACKT_M:
        p_mistat->SP.CivsKilled++;
        break;
    case SubTT_PERS_MERCENARY:
    case SubTT_PERS_MECH_SPIDER:
    case SubTT_PERS_POLICE:
        p_mistat->SP.SecurityKilled++;
        break;
    default:
        break;
    }
}

/** Checks if the vehicle collides with another object, provided as separate params.
 */
static TbBool vehicle_check_collide_with_area(struct Thing *p_vehA,
  int posBx, int posBy, int posBz, int radB, int matBid)
{
    struct M33 *p_matA;
    int radA_incl_speed, matA02, matA22;
    int futureAx, futureAz;
    int posAx, posAz;
    struct M33 *p_matB;
    int radB_incl_speed, matB02, matB22;
    int futureBx, futureBz;
    int posB_beg_x, posB_beg_z;
    int posB_end_x, posB_end_z;
    int dist_begX, dist_begZ;
    int dist_endX, dist_endZ;
    int distXZ_sq;

    assert(p_vehA->U.UVehicle.MatrixIndex < next_local_mat);
    p_matA = &local_mats[p_vehA->U.UVehicle.MatrixIndex];
    matA02 = -4 * p_matA->R[0][2];
    matA22 = -4 * p_matA->R[2][2];
    radA_incl_speed = (386 * p_vehA->Radius >> 8)
      + (p_vehA->Speed >> 3) + (p_vehA->Speed >> 4);
    futureAx = radA_incl_speed * matA02 >> 8;
    futureAz = radA_incl_speed * matA22 >> 8;

    posAx = p_vehA->X + futureAx - (matA22 >> 2);
    posAz = p_vehA->Z + futureAz + (matA02 >> 2);

    if (abs(posBy - p_vehA->Y) >= 2048)
        return false;

    if ((p_vehA->X >> 16 == posBx >> 16) && (p_vehA->Z >> 16 == posBz >> 16))
        return true;

    assert(matBid < next_local_mat);
    p_matB = &local_mats[matBid];
    matB02 = -4 * p_matB->R[0][2];
    matB22 = -4 * p_matB->R[2][2];
    radB_incl_speed = (384 * radB >> 9);
    futureBx = radB_incl_speed * matB02 >> 8;
    futureBz = radB_incl_speed * matB22 >> 8;

    posB_beg_x = posBx - futureBx;
    posB_beg_z = posBz - futureBz;
    posB_end_x = posBx + futureBx;
    posB_end_z = posBz + futureBz;

    dist_begX = (posB_beg_x - posAx) >> 8;
    dist_begZ = (posB_beg_z - posAz) >> 8;
    dist_endX = (posB_end_x - posAx) >> 8;
    dist_endZ = (posB_end_z - posAz) >> 8;

    distXZ_sq = dist_endZ * dist_endZ + dist_endX * dist_endX;
    if (distXZ_sq >= dist_begZ * dist_begZ + dist_begX * dist_begX)
        distXZ_sq = dist_begZ * dist_begZ + dist_begX * dist_begX;

    if (distXZ_sq >= radB_incl_speed * radB_incl_speed)
        return false;

    return true;
}

/** Check if the vehicle is stopped due to state of tnode it rides on.
 */
TbBool vehicle_stopped_on_tnode(struct Thing *p_vehicle)
{
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return false;

    // Repeat the stop conditions from process_vehicle_stop_for_pedestrians() exactly
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
    {
        if (p_vehicle->State == VehSt_GOTO_LOC || p_vehicle->State == VehSt_WANDER)
        {
            struct TrafficNode *p_tnode;
            p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
            if ((p_tnode->Flags & TNdF_Unkn0040) != 0)
            {
                if (p_tnode->GateLink == p_vehicle->ThingOffset)
                    return false;
                return true;
            }
        }
    }
    return false;
}

/** Check if the second vehicle is in the way of first vehicle.
 */
TbBool check_two_vehicles(struct Thing *p_vehA, struct Thing *p_vehB)
{
#if 0
    TbBool ret;
    asm volatile (
      "call ASM_check_two_vehicles\n"
        : "=r" (ret) : "a" (p_vehA), "d" (p_vehB));
    return ret;
#endif
    if (abs(p_vehA->U.UVehicle.AngleDY) > 20)
        return false;

    if (p_vehB->ThingOffset == p_vehA->ThingOffset)
        return false;

    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    return vehicle_check_collide_with_area(p_vehA, p_vehB->X, p_vehB->Y, p_vehB->Z,
      p_vehB->Radius, p_vehB->U.UVehicle.MatrixIndex);
}

TbBool vehicles_check_second_is_blocked(struct Thing *p_vehA, struct Thing *p_vehB)
{
    if (!vehicle_stopped_on_tnode(p_vehB))
        return false;

    // Check if the vehicles are riding on the same TNode, or the second has Agok node set
    if ((p_vehA->U.UVehicle.TNode == p_vehB->U.UVehicle.TNode)
     || ((p_vehB->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0))
    {
        // If one of the vehicles already claimed the TNode, it should go first
        struct TrafficNode *p_tnode;
        p_tnode = &game_traffic_nodes[-p_vehB->U.UVehicle.TNode];
        if (p_tnode->GateLink == p_vehA->ThingOffset)
            return true;
    }
    return false;
}

/** Of two competing (potentially colliding) vehicles, check if the first one has right-of-way to move forward.
 */
TbBool vehicle_first_has_right_of_way(struct Thing *p_vehA, struct Thing *p_vehB)
{
    // If no special circumstances, allow the one with higher ThingOffset first
    return (p_vehA->ThingOffset >= p_vehB->ThingOffset);
}

/** Collision check between two vehicles.
 */
static TbBool check_vehicle_col_with_veh(struct Thing *p_vehA, struct Thing *p_vehB,
  int posAx, int posAy, int posAz)
{
    struct M33 *p_matB;
    int radB_incl_speed, matB02, matB22;
    int futureBx, futureBz;
    int posB_beg_x, posB_beg_z;
    int posB_end_x, posB_end_z;
    int dist_begZ, dist_begX;
    int dist_endX, dist_endZ;
    int distXZ_sq;

    if (p_vehB->ThingOffset == p_vehA->ThingOffset)
        return false;

    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    // The following is the same as in vehicle_check_collide_with_area(), but some position computations were moved
    // to upper function as speed optimization (maybe there's no need for that anymore?)
    if (abs(p_vehB->Y - posAy) >= 2048)
        return false;

    assert(p_vehB->U.UVehicle.MatrixIndex < next_local_mat);
    p_matB = &local_mats[p_vehB->U.UVehicle.MatrixIndex];
    matB02 = -4 * p_matB->R[0][2];
    matB22 = -4 * p_matB->R[2][2];
    radB_incl_speed = 384 * p_vehB->Radius >> 9;
    futureBx = radB_incl_speed * matB02 >> 8;
    futureBz = radB_incl_speed * matB22 >> 8;

    posB_beg_x = p_vehB->X - futureBx;
    posB_beg_z = p_vehB->Z - futureBz;
    posB_end_x = p_vehB->X + futureBx;
    posB_end_z = p_vehB->Z + futureBz;

    dist_begX = (posB_beg_x - posAx) >> 8;
    dist_begZ = (posB_beg_z - posAz) >> 8;
    dist_endX = (posB_end_x - posAx) >> 8;
    dist_endZ = (posB_end_z - posAz) >> 8;

    distXZ_sq = dist_endZ * dist_endZ + dist_endX * dist_endX;
    if (distXZ_sq >= dist_begZ * dist_begZ + dist_begX * dist_begX)
        distXZ_sq = dist_begZ * dist_begZ + dist_begX * dist_begX;

    if (distXZ_sq >= radB_incl_speed * radB_incl_speed)
        return false;

    // Check if the current vehicle has right-of-way over the second
    if (vehicle_first_has_right_of_way(p_vehA, p_vehB) && check_two_vehicles(p_vehB, p_vehA))
        return false;

    // Check if the 2nd vehicle is blocked, in which case the first should assume
    // right of way to avoid blockade; this workarounds an issue where on two crossroads
    // close to each other, two vehicles will get blocked on each other. The issue
    // was prominent on `-m 0,21`, where it would happen sooner or later without any
    // actions required from the player. A better solution to such jams may be developed
    // later, when the workings of the traffic system are better understood.
    if (vehicles_check_second_is_blocked(p_vehA, p_vehB))
        return false;

    LOGSYNC("Stopping %s thing %d rqspeed %d due to %s thing %d rqspeed %d",
      vehicle_type_name(p_vehA->SubType), (int)p_vehA->ThingOffset,
      (int)p_vehA->U.UVehicle.ReqdSpeed,
      vehicle_type_name(p_vehB->SubType), (int)p_vehB->ThingOffset,
      (int)p_vehB->U.UVehicle.ReqdSpeed);

    p_vehA->SubState = 3;
    p_vehA->U.UVehicle.ReqdSpeed = 0;
    p_vehA->U.UVehicle.AngleDY = 0;
    return true;
}

/** Simplified vehicle collision check for when the vehicles are on the same tile.
 * We don't have to check for overlap, as every vehicle is larger than a tile.
 */
static TbBool check_vehicle_col_same_mapel_with_veh(struct Thing *p_vehA, struct Thing *p_vehB,
  int pos_x, int pos_y, int pos_z)
{
    if ((p_vehB->Flag & TngF_Unkn0004) != 0)
        return false;

    if (abs(p_vehB->Y - pos_y) >= 2048)
        return false;

    // Check if the current vehicle has right-of-way over the second
    if (vehicle_first_has_right_of_way(p_vehA, p_vehB))
        return false;

    LOGSYNC("Stopping %s thing %d rqspeed %d due to %s thing %d rqspeed %d",
      vehicle_type_name(p_vehA->SubType), (int)p_vehA->ThingOffset,
      (int)p_vehA->U.UVehicle.ReqdSpeed,
      vehicle_type_name(p_vehB->SubType), (int)p_vehB->ThingOffset,
      (int)p_vehB->U.UVehicle.ReqdSpeed);

    p_vehA->SubState = 3;
    p_vehA->U.UVehicle.ReqdSpeed = 0;
    p_vehA->U.UVehicle.AngleDY = 0;
    return false; //TODO is that the best result?
}

static TbBool check_vehicle_col_with_pers(struct Thing *p_vehicle, struct Thing *p_person,
  int pos_x, int pos_y, int pos_z)
{
    int dx, dz, per_r_sq, veh_r_sq;

    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
        return false;
    if (p_person->State == PerSt_DEAD)
        return false;
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return false;
    if (p_person->State == PerSt_PERSON_BURNING)
        return false;
    if (abs((p_person->Y >> 3) - pos_y) >= 2048)
        return false;

    dx = (p_person->X - pos_x) >> 8;
    dz = (p_person->Z - pos_z) >> 8;
    per_r_sq = p_person->Radius * p_person->Radius;
    veh_r_sq = p_vehicle->Radius * p_vehicle->Radius;

    if ((dx) * (dx) + (dz) * (dz) >= veh_r_sq + per_r_sq)
        return false;

    person_hit_by_car(p_person, p_vehicle);

    return false;
}

static TbBool check_vehicle_col_with_mine(struct Thing *p_vehicle, struct SimpleThing *p_minetng,
  int pos_x, int pos_y, int pos_z)
{
    if (p_minetng->SubType != 12)
        return false;

    if ((p_minetng->Timer1 != 999) && (p_minetng->Timer1 > 0))
        p_minetng->Timer1 = 0;

    return false;
}

static TbBool check_vehicle_col_on_mapel(struct Thing *p_vehicle, struct MyMapElement *p_mapel,
  int pos_x, int pos_y, int pos_z)
{
    ThingIdx thing;

    thing = p_mapel->Child;
    while (thing != 0)
    {
      if (thing > 0)
      {
          struct Thing *p_thing;
          p_thing = &things[thing];
          switch (p_thing->Type)
          {
          case TT_VEHICLE:
              if (check_vehicle_col_with_veh(p_vehicle, p_thing, pos_x, pos_y, pos_z))
                  return true;
              break;
          case TT_PERSON:
              if (check_vehicle_col_with_pers(p_vehicle, p_thing, p_vehicle->X, pos_y, p_vehicle->Z))
                  return true;
              break;
          default:
              break;
          }
          thing = p_thing->Next;
      }
      else
      {
          struct SimpleThing *p_sthing;
          p_sthing = &sthings[thing];
          switch (p_sthing->Type)
          {
          case TT_MINE:
              if (check_vehicle_col_with_mine(p_vehicle, p_sthing, p_vehicle->X, pos_y, p_vehicle->Z))
                  return true;
              break;
          default:
              break;
          }
          thing = p_sthing->Next;
      }
    }
    return false;
}

static TbBool check_vehicle_col_on_same_mapel(struct Thing *p_vehicle, struct MyMapElement *p_mapel,
  int pos_x, int pos_y, int pos_z)
{
    ThingIdx thing;

    thing = p_mapel->Child;
    while (thing != 0)
    {
      if (thing > 0)
      {
          struct Thing *p_thing;
          p_thing = &things[thing];
          switch (p_thing->Type)
          {
          case TT_VEHICLE:
              if (check_vehicle_col_same_mapel_with_veh(p_vehicle, p_thing, pos_x, pos_y, pos_z))
                  return true;
              break;
          default:
              break;
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
    return false;
}

/** Checks vehicle collisions.
 */
TbBool check_vehicle_col(struct Thing *p_vehicle)
{
#if 0
    TbBool ret;
    asm volatile (
      "call ASM_check_vehicle_col\n"
        : "=r" (ret) : "a" (p_vehicle));
    return ret;
#endif
    ushort r;
    struct M33 *p_matA;
    int rad_incl_speed, matA02, matA22;
    int tile_ctr_x, tile_ctr_z;
    int tile_x, tile_z;
    int pos_x, pos_y, pos_z;

    if (abs(p_vehicle->U.UVehicle.AngleDY) > 20)
        return false;

    r = p_vehicle->Radius;
    assert(p_vehicle->U.UVehicle.MatrixIndex < next_local_mat);
    p_matA = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    pos_x = p_vehicle->X;
    pos_y = p_vehicle->Y;
    pos_z = p_vehicle->Z;
    rad_incl_speed = (p_vehicle->Speed >> 4) + (p_vehicle->Speed >> 3) + (386 * r >> 8);
    matA02 = -4 * p_matA->R[0][2];
    matA22 = -4 * p_matA->R[2][2];
    pos_x = (matA02 * rad_incl_speed >> 8) + pos_x - (matA22 >> 2);
    pos_z = (matA22 * rad_incl_speed >> 8) + pos_z + (matA02 >> 2);
    tile_ctr_z = pos_z >> 16;
    tile_ctr_x = pos_x >> 16;

    for (tile_x = tile_ctr_x - 2; tile_x <= tile_ctr_x + 2; tile_x++)
    {
        if ((tile_x < 0) || (tile_x >= 128))
            continue;
        for (tile_z = tile_ctr_z - 2; tile_z <= tile_ctr_z + 2; tile_z++)
        {
            if ((tile_z < 0) || (tile_z >= 128))
                continue;
            struct MyMapElement *p_mapel;
            p_mapel = &game_my_big_map[128 * tile_z + tile_x];
            if (check_vehicle_col_on_mapel(p_vehicle, p_mapel, pos_x, pos_y, pos_z))
                return true;
        }
    }

    struct MyMapElement *p_mapel;
    p_mapel = &game_my_big_map[128 * (p_vehicle->Z >> 16) + (p_vehicle->X >> 16)];
    if (check_vehicle_col_on_same_mapel(p_vehicle, p_mapel, pos_x, pos_y, pos_z))
        return true;

    return false;
}

static DIFFTEST_REF_FUNC void start_crashing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_crashing\n"
        : : "a" (p_vehicle));
}

void start_crashing(struct Thing *p_vehicle)
{
    DIFFTEST_SHADOW(start_crashing, start_crashing_asm(p_vehicle));
    if (p_vehicle->SubType == SubTT_VEH_MECH)
    {
        p_vehicle->State = VehSt_UNKN_D;
        p_vehicle->Flag |= TngF_Destroyed;
        init_mech_explode(p_vehicle);
        return;
    }
    if ((ushort)p_vehicle->State == VehSt_UNKN_45)
        return;

    p_vehicle->Flag |= TngF_Destroyed;
    p_vehicle->State = VehSt_UNKN_45;
    if (p_vehicle->SubType == SubTT_VEH_TRAIN)
    {
        struct Thing *p_station;

        p_station = &things[p_vehicle->U.UVehicle.TNode];
        if ((p_station->Flag & TngF_Destroyed) == 0)
            p_station->Flag |= TngF_Unkn00040000;
    }
    p_vehicle->VY = 0;
    p_vehicle->U.UVehicle.ReqdSpeed = p_vehicle->Speed >> 1;
    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 95);
}

static DIFFTEST_REF_FUNC void start_landing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_landing\n"
        : : "a" (p_vehicle));
}

void start_landing(struct Thing *p_vehicle)
{
    DIFFTEST_SHADOW(start_landing, start_landing_asm(p_vehicle));
    if ((ushort)p_vehicle->State == VehSt_FLY_LANDING)
        return;
    if (is_this_a_nice_place_for_a_picnic(p_vehicle->X, p_vehicle->Z))
    {
        p_vehicle->U.UVehicle.PathIndex = 0;
        p_vehicle->State = VehSt_FLY_LANDING;
        p_vehicle->U.UVehicle.TNode = 0;
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int exit_pod_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_exit_pod\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
    return ra;
}

int exit_pod(int a1, int a2, int a3, int a4)
{
    struct Thing *p_vehicle = (struct Thing *)a1;
    struct Thing *p_passngr;
    short passngr;

    DIFFTEST_SHADOW_RET(exit_pod, int, exit_pod_asm(a1, a2, a3, a4));
    passngr = p_vehicle->U.UVehicle.PassengerHead;
    if (passngr == 0)
    {
        stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 9);
        DIFFTEST_RETURN(1);
    }
    if (passngr < 0)
        DIFFTEST_RETURN(1);

    p_passngr = &things[passngr];
    p_passngr->X = (p_vehicle->VX << 16) + 0x8000;
    p_passngr->Z = (p_vehicle->VZ << 16) + 0x8000;
    p_passngr->Y = alt_at_point(p_passngr->X >> 8, p_passngr->Z >> 8);
    p_passngr->Flag &= ~(TngF_Unkn4000 | TngF_Unkn0010 | TngF_Unkn0004);
    p_passngr->Flag |= (TngF_Unkn0010 | TngF_Unkn0004);
    p_vehicle->U.UVehicle.PassengerHead = p_passngr->Next;
    add_node_thing(p_passngr->ThingOffset);
    DIFFTEST_RETURN(0);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void set_object_flag_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_object_flag\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void set_object_flag(int a1, int a2, int a3, int a4)
{
    struct SingleObject *p_sobj;
    ushort face, face_end;
    ushort flag;

    DIFFTEST_SHADOW(set_object_flag, set_object_flag_asm(a1, a2, a3, a4));
    p_sobj = &game_objects[(ushort)a1];
    flag = (ushort)a2;

    face_end = p_sobj->StartFace + p_sobj->NumbFaces;
    for (face = p_sobj->StartFace; face < face_end; face++)
    {
        game_object_faces3[face].Flags = (ubyte)flag;
    }

    face_end = p_sobj->StartFace4 + p_sobj->NumbFaces4;
    for (face = p_sobj->StartFace4; face < face_end; face++)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[face];
        p_face4->Flags = (ubyte)flag;
        if (flag == 5)
        {
            p_face4->Shade0 = 0x80;
            p_face4->Shade1 = 0x80;
            p_face4->Shade2 = 0x3f;
            p_face4->Shade3 = 0x3f;
        }
        else
        {
            p_face4->Shade0 = 0x20;
            p_face4->Shade1 = 0x20;
            p_face4->Shade2 = 0x20;
            p_face4->Shade3 = 0x20;
        }
    }
}



static DIFFTEST_REF_FUNC void process_shuttle_pod_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_shuttle_pod\n"
        : : "a" (p_vehicle));
}

void process_shuttle_pod(struct Thing *p_vehicle)
{
    struct Thing *p_station;
    short tnode, station;

    DIFFTEST_SHADOW(process_shuttle_pod, process_shuttle_pod_asm(p_vehicle));
    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode < 0)
    {
        if ((short)exit_pod((int)p_vehicle, tnode, (int)p_vehicle, 0) != 0)
        {
            station = -tnode;
            set_object_flag((ushort)things[(ushort)station].U.UObject.Object,
              0x15, (int)p_vehicle, 0);
            remove_thing(p_vehicle->ThingOffset);
        }
        return;
    }

    p_station = &things[tnode];
    set_object_flag((ushort)things[(ushort)tnode].U.UObject.Object,
      5, (int)p_vehicle, 0);
    station = p_station->U.UObject.PrevThing;
    if (station != 0)
    {
        set_object_flag((ushort)things[(ushort)station].U.UObject.Object,
          0x15, (int)p_vehicle, 0);
    }
    p_vehicle->X = p_station->X;
    p_vehicle->Y = p_station->Y;
    p_vehicle->Z = p_station->Z;
    station = p_station->U.UObject.NextThing;
    if (station == 0)
    {
        p_vehicle->U.UVehicle.TNode = -p_vehicle->U.UVehicle.TNode;
        p_vehicle->VX = p_station->VX;
        p_vehicle->VZ = p_station->VZ;
        return;
    }
    p_vehicle->U.UVehicle.TNode = station;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void explode_thing_object_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_explode_thing_object\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void explode_thing_object(int a1, int a2, int a3, int a4)
{
    struct Thing *p_thing;
    struct SingleObject *p_sobj;
    struct M33 *p_mat;
    struct ExplodeFace *p_exface;
    s32 pt_x[4], pt_y[4], pt_z[4];
    s32 cen_x, cen_y, cen_z;
    s32 cor_x, cor_y, cor_z;
    ushort face, exface;
    int i;

    DIFFTEST_SHADOW(explode_thing_object, explode_thing_object_asm(a1, a2, a3, a4));
    dont_bother_with_explode_faces = 0;
    p_thing = &things[(short)a1];
    p_sobj = &game_objects[(ushort)p_thing->U.UObject.Object];
    if (p_thing->Type == TT_VEHICLE)
    {
        cor_x = (short)(p_thing->X >> 8);
        cor_y = (short)(p_thing->Y >> 5);
        cor_z = (short)(p_thing->Z >> 8);
    }
    else
    {
        cor_x = p_sobj->MapX;
        cor_y = p_sobj->OffsetY;
        cor_z = p_sobj->MapZ;
    }
    p_mat = &local_mats[p_thing->U.UObject.MatrixIndex];

    for (face = 0; face < p_sobj->NumbFaces; face++)
    {
        struct SingleObjectFace3 *p_face;

        p_face = &game_object_faces3[(ushort)(p_sobj->StartFace + face)];
        vec_colour = colour_grey1;
        vec_mode = 0;
        for (i = 0; i < 3; i++)
        {
            struct SinglePoint *p_pt;
            s32 px, py, pz;

            p_pt = &game_object_points[(ushort)p_face->PointNo[i]];
            px = 2 * (s32)p_pt->X;
            py = 2 * (s32)p_pt->Y;
            pz = 2 * (s32)p_pt->Z;
            pt_x[i] = cor_x + ((p_mat->R[0][0] * px + p_mat->R[0][1] * py + p_mat->R[0][2] * pz) >> 15);
            pt_y[i] = cor_y + ((p_mat->R[1][0] * px + p_mat->R[1][1] * py + p_mat->R[1][2] * pz) >> 15);
            pt_z[i] = cor_z + ((p_mat->R[2][0] * px + p_mat->R[2][1] * py + p_mat->R[2][2] * pz) >> 15);
        }
        cen_y = (85 * (pt_y[0] + pt_y[1] + pt_y[2])) >> 8;
        cen_x = (85 * (pt_x[0] + pt_x[1] + pt_x[2])) >> 8;
        cen_z = (85 * (pt_z[0] + pt_z[1] + pt_z[2])) >> 8;

        exface = explode_face_alloc();
        if (exface == 0)
            continue;
        p_exface = &ex_faces[exface];
        p_exface->Type = EXPL_FACE_TRI_TYP1;
        p_exface->Texture = p_face->Texture;
        p_exface->Flags = p_face->Flags;
        p_exface->Col = (ubyte)p_face->ExCol;
        p_exface->X = cen_x;
        p_exface->Z = cen_z;
        p_exface->X0 = pt_x[0] - cen_x;
        p_exface->Y0 = pt_y[0] - cen_y;
        p_exface->Z0 = pt_z[0] - cen_z;
        p_exface->X1 = pt_x[1] - cen_x;
        p_exface->Y1 = pt_y[1] - cen_y;
        p_exface->Z1 = pt_z[1] - cen_z;
        p_exface->X2 = pt_x[2] - cen_x;
        p_exface->Y2 = pt_y[2] - cen_y;
        p_exface->Z2 = pt_z[2] - cen_z;
        p_exface->Y = cen_y;
        p_exface->DX = (cen_x - cor_x) >> 3;
        p_exface->DY = (cen_y - cor_y) >> 3;
        p_exface->DZ = (cen_z - cor_z) >> 3;
        p_exface->DY += 50;
        p_exface->Timer = 50 - (exface & 0x1F);
    }

    for (face = 0; face < p_sobj->NumbFaces4; face++)
    {
        struct SingleObjectFace4 *p_face;

        p_face = &game_object_faces4[(ushort)(p_sobj->StartFace4 + face)];
        vec_mode = 0;
        vec_colour = colour_grey1;
        for (i = 0; i < 4; i++)
        {
            struct SinglePoint *p_pt;
            s32 px, py, pz;

            p_pt = &game_object_points[(ushort)p_face->PointNo[i]];
            px = 2 * (s32)p_pt->X;
            py = 2 * (s32)p_pt->Y;
            pz = 2 * (s32)p_pt->Z;
            pt_x[i] = cor_x + ((p_mat->R[0][0] * px + p_mat->R[0][1] * py + p_mat->R[0][2] * pz) >> 15);
            pt_y[i] = cor_y + ((p_mat->R[1][0] * px + p_mat->R[1][1] * py + p_mat->R[1][2] * pz) >> 15);
            pt_z[i] = cor_z + ((p_mat->R[2][0] * px + p_mat->R[2][1] * py + p_mat->R[2][2] * pz) >> 15);
        }
        cen_x = (pt_x[0] + pt_x[1] + pt_x[2] + pt_x[3]) >> 2;
        cen_y = (pt_y[0] + pt_y[1] + pt_y[2] + pt_y[3]) >> 2;
        cen_z = (pt_z[0] + pt_z[1] + pt_z[2] + pt_z[3]) >> 2;

        exface = explode_face_alloc();
        if (exface == 0)
            continue;
        p_exface = &ex_faces[exface];
        p_exface->Type = EXPL_FACE_QUAD_TYP2;
        p_exface->Texture = p_face->Texture;
        p_exface->Flags = p_face->Flags;
        p_exface->Col = (ubyte)p_face->ExCol;
        p_exface->X = cen_x;
        p_exface->Z = cen_z;
        p_exface->X0 = pt_x[0] - cen_x;
        p_exface->Y0 = pt_y[0] - cen_y;
        p_exface->Z0 = pt_z[0] - cen_z;
        p_exface->X1 = pt_x[1] - cen_x;
        p_exface->Y1 = pt_y[1] - cen_y;
        p_exface->Z1 = pt_z[1] - cen_z;
        p_exface->X2 = pt_x[2] - cen_x;
        p_exface->Y2 = pt_y[2] - cen_y;
        p_exface->Z2 = pt_z[2] - cen_z;
        p_exface->X3 = pt_x[3] - cen_x;
        p_exface->Y3 = pt_y[3] - cen_y;
        p_exface->Z3 = pt_z[3] - cen_z;
        p_exface->Y = cen_y;
        p_exface->DX = (cen_x - cor_x) >> 3;
        p_exface->DY = (cen_y - cor_y) >> 3;
        p_exface->DZ = (cen_z - cor_z) >> 3;
        p_exface->DY += 50;
        p_exface->Timer = 50 - (exface & 0x1F);
    }
}


static DIFFTEST_REF_FUNC void init_vehicle_explode_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_vehicle_explode\n"
        : : "a" (p_vehicle));
}

void init_vehicle_explode(struct Thing *p_vehicle)
{
    char locstr[52];
    struct Thing *p_owner;
    ushort count;
    short passngr;
    ubyte ret;
    s32 cond;

    DIFFTEST_SHADOW(init_vehicle_explode, init_vehicle_explode_asm(p_vehicle));
    count = 0;
    if (p_vehicle->SubType == SubTT_VEH_TANK)
        stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 0x31);
    remove_locks(p_vehicle);
    p_vehicle->Flag |= TngF_Destroyed;
    play_dist_sample(p_vehicle, 0x25, 0x7f, 0x40, 100, 0, 3);
    p_vehicle->State = VehSt_UNKN_D;
    while (p_vehicle->U.UVehicle.PassengerHead != 0 && count < 100)
    {
        passngr = p_vehicle->U.UVehicle.PassengerHead;
        ret = person_leave_vehicle(&things[(ushort)passngr], p_vehicle);
        if (ret != 0)
            sprintf(locstr, "TELL MD failed exit car person ret %d \n", (int)ret);
        count++;
        if ((ushort)p_vehicle->U.UVehicle.PassengerHead == (s32)passngr)
            break;
    }

    explode_thing_object(p_vehicle->ThingOffset, 0, 0, 0);
    if (p_vehicle->SubType == SubTT_VEH_FLYING)
    {
        cond = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8))
          + ((s32)(386 * (ushort)p_vehicle->Radius) >> 5) + 250 > p_vehicle->Y;
    }
    else
    {
        cond = 1;
    }
    if (cond)
        quick_crater(p_vehicle->X >> 16, p_vehicle->Z >> 16, 2);

    bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 15);
    delete_node(p_vehicle);
    p_vehicle->Type = TT_UNKN1;
    if (p_vehicle->OldTarget != 0)
        p_owner = &things[p_vehicle->OldTarget];
    else
        p_owner = NULL;
    do_shockwave(p_vehicle->X >> 8, p_vehicle->Y >> 8, p_vehicle->Z >> 8,
      (ushort)p_vehicle->Radius << 2, 5000, p_owner);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void flatten_vehicle_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_flatten_vehicle\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void flatten_vehicle(int a1, int a2, int a3, int a4)
{
    struct Thing *p_thing = (struct Thing *)a1;
    struct SingleObject *p_sobj;
    s32 pt;

    DIFFTEST_SHADOW(flatten_vehicle, flatten_vehicle_asm(a1, a2, a3, a4));

    if (p_thing->SubType != SubTT_VEH_GROUND)
        return;
    if ((p_thing->State != VehSt_PARKED_PARAL) &&
      (p_thing->State != VehSt_PARKED_PERPN))
        return;

    p_sobj = &game_objects[p_thing->U.UVehicle.Object];
    for (pt = p_sobj->StartPoint; pt <= (s32)p_sobj->EndPoint; pt++)
    {
        struct SinglePoint *p_point;
        s32 y;

        p_point = &game_object_points[pt];
        y = p_point->Y;
        p_point->Y = y - (y >> 4);
    }
    if (p_thing->Y > 0)
        p_thing->Y -= p_thing->Y >> 4;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int check_tank_collision_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_check_tank_collision\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int can_the_tank_run_over(int a1, int a2);

int check_tank_collision(int a1, int a2, int a3, int a4)
{
    struct Thing *p_tank = (struct Thing *)a1;
    struct M33 *p_mat;
    s32 dt_x, dt_z;
    s32 front_x, front_z, back_x, back_z;
    s32 tile_x, tile_z;
    s32 tl_x, tl_z, tl_x_end, tl_z_end;

    DIFFTEST_SHADOW_RET(check_tank_collision, int,
      check_tank_collision_asm(a1, a2, a3, a4));

    p_mat = &local_mats[p_tank->U.UVehicle.MatrixIndex];
    dt_x = (-p_mat->R[0][2]) << 2;
    dt_z = (-p_mat->R[2][2]) << 2;
    front_x = p_tank->X + dt_x;
    front_z = p_tank->Z + dt_z;
    back_x = p_tank->X - dt_x;
    back_z = p_tank->Z - dt_z;
    dt_x >>= 4;
    front_x += dt_x;
    back_x += dt_x;
    dt_z >>= 4;
    front_z += dt_z;
    back_z += dt_z;

    tile_x = p_tank->X >> 16;
    tile_z = p_tank->Z >> 16;
    tl_x_end = tile_x + 4;
    for (tl_x = tile_x - 3; tl_x != tl_x_end; tl_x++)
    {
        if (tl_x < 0 || tl_x > 127)
            continue;
        tl_z_end = tile_z + 4;
        for (tl_z = tile_z - 3; tl_z != tl_z_end; tl_z++)
        {
            ThingIdx thing;

            if (tl_z < 0 || tl_z > 127)
                continue;
            thing = game_my_big_map[(tl_z << 7) + tl_x].Child;
            while (thing != 0)
            {
                s32 dx, dz, dist1, dist2, radius;

                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    thing = p_thing->Next;
                    if (p_thing->Type != TT_VEHICLE)
                        continue;
                    if (p_thing == p_tank)
                        continue;
                    if (abs(p_tank->Y - p_thing->Y) >= 0x1000)
                        continue;
                    dx = abs(p_thing->X - front_x);
                    dz = abs(p_thing->Z - front_z);
                    if (dx > dz)
                        dist1 = dx + (dz >> 1);
                    else
                        dist1 = dz + (dx >> 1);
                    dx = abs(p_thing->X - back_x);
                    dz = abs(p_thing->Z - back_z);
                    if (dx > dz)
                        dist2 = dx + (dz >> 1);
                    else
                        dist2 = dz + (dx >> 1);
                    radius = ((s32)(ushort)p_thing->Radius << 8) + 0x13800;
                    if (dist1 >= radius && dist2 >= radius)
                        continue;

                    if (p_thing->SubType == SubTT_VEH_TANK)
                    {
                        struct Thing *p_passngr;

                        p_passngr = &things[(ushort)p_tank->U.UVehicle.PassengerHead];
                        if ((p_passngr->Flag & TngF_PlayerAgent) != 0)
                            p_tank->U.UVehicle.ReqdSpeed = 0;
                        else if (p_tank < p_thing)
                            p_tank->U.UVehicle.ReqdSpeed = 0;
                    }
                    else
                    {
                        short health;

                        health = p_thing->Health;
                        health = health - (health >> 2);
                        p_thing->Health = health;
                        p_thing->Health = health - 16;
                        p_thing->U.UVehicle.RecoilTimer = 5;
                        if (p_thing->Health <= 0)
                        {
                            init_vehicle_explode(p_thing);
                            p_thing->Flag |= TngF_Destroyed;
                        }
                        p_tank->Health--;
                        p_tank->U.UVehicle.RecoilTimer = 5;
                        if (p_tank->Health <= 0)
                        {
                            start_crashing(p_tank);
                            p_tank->Flag |= TngF_Destroyed;
                        }
                        flatten_vehicle((int)p_thing, (int)p_tank, (int)p_thing, (int)p_tank);
                    }
                }
                else
                {
                    struct SimpleThing *p_sthing;

                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                    if ((p_sthing->Flag & TngF_Destroyed) != 0)
                        continue;
                    dx = abs(p_sthing->X - front_x);
                    dz = abs(p_sthing->Z - front_z);
                    if (dx > dz)
                        dist1 = dx + (dz >> 1);
                    else
                        dist1 = dz + (dx >> 1);
                    dx = abs(p_sthing->X - back_x);
                    dz = abs(p_sthing->Z - back_z);
                    if (dx > dz)
                        dist2 = dx + (dz >> 1);
                    else
                        dist2 = dz + (dx >> 1);
                    radius = ((s32)(ushort)p_sthing->Radius << 8) + 0x13800;
                    if (dist1 >= radius && dist2 >= radius)
                        continue;
                    // TODO the asm compares distances to the rear point with 2, probably meant something else
                    if (dx == 2 || dz == 2)
                        continue;
                    // The asm passes edx = sign of the last abs value, which is always 0
                    if (can_the_tank_run_over((int)p_sthing, 0))
                        set_static_on_fire(p_sthing);
                }
            }
        }
    }
    DIFFTEST_RETURN(tl_x_end);
}


static DIFFTEST_REF_FUNC void process_tank_stationary_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank_stationary\n"
        : : "a" (p_vehicle));
}

void process_tank_stationary(struct Thing *p_vehicle)
{
    struct Thing *p_turret;
    struct MyMapElement *p_mapel;
    ubyte flags2;

    DIFFTEST_SHADOW(process_tank_stationary, process_tank_stationary_asm(p_vehicle));
    p_turret = &things[p_vehicle->U.UVehicle.SubThing];
    rotate_obj(0, p_turret->U.UVehicle.AngleY, 0,
      (int)&local_mats[p_turret->U.UVehicle.MatrixIndex]);

    if ((gameturn & 7) == 0)
    {
        if (!IsSamplePlaying((ushort)p_vehicle->ThingOffset, 49, 0))
        {
            if (p_vehicle->U.UVehicle.PassengerHead != 0)
                play_dist_sample(p_vehicle, 49, 127, 64, 100, -1, 1);
        }
        else if (p_vehicle->U.UVehicle.PassengerHead == 0)
        {
            stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 49);
        }
    }

    check_tank_collision((int)p_vehicle, 0, 0, 0);

    // The asm reads the map flags twice, but the outcome is the same as one test
    p_mapel = &game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)];
    flags2 = p_mapel->Flags2;
    if (((flags2 & (MEF2_Unkn01|MEF2_Unkn04)) != 0) && ((flags2 & MEF2_Unkn08) == 0))
        init_vehicle_explode(p_vehicle);
}

int do_next_navigate_index(int a1, int a2, int a3, int a4);

static DIFFTEST_REF_FUNC void process_tank_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank\n"
        : : "a" (p_vehicle));
}

void process_tank(struct Thing *p_vehicle)
{
    struct Thing *p_turret;
    struct M33 *p_mat;
    ubyte flags2;
    s32 dt_x, dt_z, dist_sq;
    s32 angle;
    s32 new_x, new_y, new_z;

    DIFFTEST_SHADOW(process_tank, process_tank_asm(p_vehicle));
    if ((gameturn & 7) == 0)
    {
        if (!IsSamplePlaying((ushort)p_vehicle->ThingOffset, 49, 0))
        {
            if (p_vehicle->U.UVehicle.PassengerHead != 0)
                play_dist_sample(p_vehicle, 49, 127, 64, 100, -1, 1);
        }
        else if (p_vehicle->U.UVehicle.PassengerHead == 0)
        {
            stop_sample_using_heap((ushort)p_vehicle->ThingOffset, 49);
        }
    }

    if ((game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)].Flags2 & MEF2_Unkn08) == 0)
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        alt += 0xdac;
        if (alt > p_vehicle->Y)
            p_vehicle->Y = p_vehicle->Y + ((alt - p_vehicle->Y) >> 4) + 0x40;
        if (alt < p_vehicle->Y)
            p_vehicle->Y = p_vehicle->Y - (((p_vehicle->Y - alt) >> 4) + 0x40);
    }

    dt_x = p_vehicle->U.UVehicle.GotoX - (p_vehicle->X >> 8);
    dt_z = p_vehicle->U.UVehicle.GotoZ - (p_vehicle->Z >> 8);
    angle = (ushort)arctan(dt_x, dt_z) - p_vehicle->U.UVehicle.AngleY;
    if (angle > 0x400)
        angle -= 0x7ff;
    if (angle < -0x400)
        angle += 0x7ff;
    p_vehicle->U.UVehicle.AngleDY = angle >> 1;
    if (p_vehicle->U.UVehicle.AngleDY > 300)
        p_vehicle->U.UVehicle.AngleDY = 300;
    else if (p_vehicle->U.UVehicle.AngleDY < -300)
        p_vehicle->U.UVehicle.AngleDY = -300;

    dist_sq = dt_x * dt_x + dt_z * dt_z;
    p_vehicle->U.UVehicle.ReqdSpeed = 500 - 2 * abs(p_vehicle->U.UVehicle.AngleDY)
      - abs(p_vehicle->U.UVehicle.AngleDY);
    if ((ushort)p_vehicle->State == 0x33)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        if (p_vehicle->U.UVehicle.AngleDY > 0)
            p_vehicle->U.UVehicle.AngleDY -= 8;
        if (p_vehicle->U.UVehicle.AngleDY < 0)
            p_vehicle->U.UVehicle.AngleDY += 8;
        if (p_vehicle->U.UVehicle.AngleDY <= 8)
            p_vehicle->U.UVehicle.AngleDY = 0;
        if (p_vehicle->Speed == 0)
            p_vehicle->State = 0x21;
    }
    else if (dist_sq < 0x30000)
    {
        if ((ubyte)do_next_navigate_index((int)p_vehicle, 0, 0, 0))
        {
            p_vehicle->U.UVehicle.PathIndex = 0;
            p_vehicle->State = 0x33;
        }
        else
        {
            struct MyPath *p_path;
            ubyte pt;

            p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
            pt = p_vehicle->PathOffset & 3;
            p_vehicle->U.UVehicle.GotoX = p_path->X[pt];
            p_vehicle->U.UVehicle.GotoZ = p_path->Z[pt];
        }
    }

    check_tank_collision((int)p_vehicle, 0, 0, 0);
    if (p_vehicle->Speed < p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed += 12;
    if (p_vehicle->Speed > p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed -= 38;

    p_vehicle->U.UVehicle.AngleY = ((p_vehicle->U.UVehicle.AngleDY >> 3)
      + p_vehicle->U.UVehicle.AngleY) & 0x7ff;
    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    p_turret = &things[(ushort)p_vehicle->U.UVehicle.SubThing];
    rotate_obj(0, p_turret->U.UVehicle.AngleY, 0,
      (int)&local_mats[p_turret->U.UVehicle.MatrixIndex]);
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY), 0, (int)p_mat);

    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    new_x = p_vehicle->X + p_vehicle->VX;
    new_z = p_vehicle->Z + p_vehicle->VZ;
    new_y = p_vehicle->Y + p_vehicle->VY;

    flags2 = game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)].Flags2;
    if (((flags2 & (MEF2_Unkn01|MEF2_Unkn04)) != 0) && ((flags2 & MEF2_Unkn08) == 0))
    {
        init_vehicle_explode(p_vehicle);
        return;
    }
    flags2 = game_my_big_map[((new_z >> 16) << 7) + (new_x >> 16)].Flags2;
    if (((flags2 & (MEF2_Unkn01|MEF2_Unkn04)) == 0) || ((flags2 & MEF2_Unkn08) != 0))
        move_mapwho(p_vehicle, new_x, new_y, new_z);
}

static DIFFTEST_REF_FUNC void set_passengers_location_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_set_passengers_location\n"
        : : "a" (p_vehicle));
}

void set_passengers_location(struct Thing *p_vehicle)
{
    struct Thing *p_passngr;
    ushort passngr;
    ushort k;

    DIFFTEST_SHADOW(set_passengers_location, set_passengers_location_asm(p_vehicle));
    p_vehicle->Flag2 &= ~TgF2_Unkn2000;
    k = 0;
    passngr = p_vehicle->U.UVehicle.PassengerHead;
    while (passngr != 0)
    {
        if (k >= 410)
            break;
        p_passngr = &things[passngr];
        p_passngr->X = p_vehicle->X;
        if (p_vehicle->SubType == SubTT_VEH_FLYING)
            p_passngr->Y = p_vehicle->Y >> 3;
        else
            p_passngr->Y = p_vehicle->Y;
        p_passngr->Z = p_vehicle->Z;
        passngr = p_passngr->U.UPerson.LinkPassenger;
        if ((p_passngr->Flag & TngF_SelectedAgent) != 0)
            p_vehicle->Flag2 |= TgF2_Unkn2000;
        k++;
    }
    // TODO the original then walks the passenger list again, up to 100
    // entries, without any side effect (likely stripped debug code)
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void swoosh_veh_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_swoosh_veh\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
}

void swoosh_veh(int a1, int a2, int a3, int a4)
{
    struct Thing *p_vehicle = (struct Thing *)a1;
    s32 dist_x, dist_z, dist;

    DIFFTEST_SHADOW(swoosh_veh, swoosh_veh_asm(a1, a2, a3, a4));
    if ((last_swoosh_thing == p_vehicle->ThingOffset) &&
      ((ulong)(gameturn - last_swoosh_turn) < 150))
        return;
    if (ingame.TrackThing == p_vehicle->ThingOffset)
        return;
    if ((p_vehicle->SubType != SubTT_VEH_GROUND) &&
      (p_vehicle->SubType != SubTT_VEH_UNKN43) &&
      (p_vehicle->SubType != SubTT_VEH_FLYING))
        return;

    dist_x = abs((p_vehicle->X >> 8) - engn_xc);
    dist_z = abs((p_vehicle->Z >> 8) - engn_zc);
    if (dist_x > dist_z)
        dist = dist_x + (dist_z >> 1);
    else
        dist = (dist_x >> 1) + dist_z;
    if (dist >= 1024)
        return;

    if (IsSamplePlaying((ushort)p_vehicle->ThingOffset, 17, 0))
        return;
    last_swoosh_thing = p_vehicle->ThingOffset;
    last_swoosh_turn = gameturn;
    play_dist_sample(p_vehicle, 17, 127, 64, 100, 0, 1);
}


int rotate_obj(int a1, int a2, int a3, int a4);

static DIFFTEST_REF_FUNC void move_flying_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_flying_vehicle\n"
        : : "a" (p_vehicle));
}

void move_flying_vehicle(struct Thing *p_vehicle)
{
    struct M33 *p_mat;
    short angle, dt_angle;
    s32 wobble_p, wobble_v;
    int new_x, new_y, new_z;

    DIFFTEST_SHADOW(move_flying_vehicle, move_flying_vehicle_asm(p_vehicle));
    if (((gameturn + (ushort)p_vehicle->ThingOffset) & 3) == 0)
        swoosh_veh((int)p_vehicle, gameturn, 0, 0);

    angle = p_vehicle->U.UVehicle.AngleY + (p_vehicle->U.UVehicle.AngleDY >> 3);
    p_vehicle->U.UVehicle.AngleY = angle;
    p_vehicle->U.UVehicle.AngleY = (angle + 0x800) & 0x7FF;

    if (p_vehicle->State != 0x45)
    {
        dt_angle = p_vehicle->U.UVehicle.AngleZ - p_vehicle->U.UVehicle.AngleDY;
        if (dt_angle < -100)
            p_vehicle->U.UVehicle.AngleZ += 100;
        else if (dt_angle > 100)
            p_vehicle->U.UVehicle.AngleZ -= 100;
        else if (abs(dt_angle) > 50)
            p_vehicle->U.UVehicle.AngleZ = p_vehicle->U.UVehicle.AngleDY;
    }

    if ((p_vehicle->U.UVehicle.WobbleZP != 0) || (p_vehicle->U.UVehicle.WobbleZV != 0))
    {
        p_vehicle->U.UVehicle.AngleZ = p_vehicle->U.UVehicle.AngleZ +
          (p_vehicle->U.UVehicle.WobbleZP >> 16);
        wobble_p = p_vehicle->U.UVehicle.WobbleZP + p_vehicle->U.UVehicle.WobbleZV;
        wobble_v = p_vehicle->U.UVehicle.WobbleZV - (wobble_p >> 4);
        p_vehicle->U.UVehicle.WobbleZP = wobble_p;
        p_vehicle->U.UVehicle.WobbleZV = wobble_v;
        p_vehicle->U.UVehicle.WobbleZV = wobble_v - (wobble_v >> 6);
    }

    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY),
      (short)(-(p_vehicle->U.UVehicle.AngleZ >> 1)), (int)p_mat);
    if (p_vehicle->U.UVehicle.AngleX != 0)
        rotate_object_axis(p_mat, 0, p_vehicle->U.UVehicle.AngleX, 0);

    p_vehicle->VX = (p_vehicle->Speed * (-p_mat->R[0][2])) >> 10;
    p_vehicle->VZ = (p_vehicle->Speed * (-p_mat->R[2][2])) >> 10;
    new_x = p_vehicle->X + p_vehicle->VX;
    new_y = p_vehicle->Y + p_vehicle->VY;
    new_z = p_vehicle->Z + p_vehicle->VZ;
    move_mapwho(p_vehicle, new_x, new_y, new_z);
}

void process_tank_turret(struct Thing *p_tank)
{
    struct Thing *p_turret;
    int target_x, target_y;
    int turret;
    short angle;
    int dt_angle;

    turret = p_tank->U.UVehicle.SubThing;
    if (turret == 0) {
        p_tank->OldTarget = LbFPMath_PI+1;
        return;
    }
    p_turret = &things[turret];
    if ((p_tank->Flag & TngF_ShootAtPos) != 0)
    {
        target_x = p_tank->U.UVehicle.TargetDX;
        target_y = p_tank->U.UVehicle.TargetDZ;
    }
    else
    {
        struct Thing *p_target;
        p_target = p_tank->PTarget;
        if (p_target == NULL)
        {
            p_tank->OldTarget = 20000;
            return;
        }
        target_x = PRCCOORD_TO_MAPCOORD(p_target->X);
        target_y = PRCCOORD_TO_MAPCOORD(p_target->Z);
    }
    angle = p_turret->U.UMGun.AngleY
        - angle_between_points(target_x, target_y, PRCCOORD_TO_MAPCOORD(p_tank->X), PRCCOORD_TO_MAPCOORD(p_tank->Z));
    if (angle < -LbFPMath_PI)
        angle += 2*LbFPMath_PI;
    else if (angle > LbFPMath_PI)
        angle -= 2*LbFPMath_PI;

    //TODO CONFIG how fast the tank can target could be a difficulty-related setting
    // Travel 1/8 of the distance in each game turn
    dt_angle = angle / 8;
    if (dt_angle > LbFPMath_PI/17)
        dt_angle = LbFPMath_PI/17;
    if (dt_angle < -LbFPMath_PI/17)
        dt_angle = -LbFPMath_PI/17;
    if (dt_angle == 0)
    {
        if (angle > 0)
            dt_angle = 1;
        if (angle < 0)
            dt_angle = -1;
    }

    // Despite being caused by the turret, we bind the sound samples to the vehicle part
    // of the tank. This is because turrets do not contain full position on map, so the
    // sound update would misplace the sound source if it was bound to the turret
    if ((p_turret->Flag2 & TgF2_Unkn0200) != 0)
    {
        if (dt_angle <= 1) {
            // Play rotation stop sample
            if (!IsSamplePlaying(p_tank->ThingOffset, 47, 0))
                play_dist_sample(p_tank, 47, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            p_turret->Flag2 &= ~TgF2_Unkn0200;
        }
    }
    else
    {
        // Play rotation sample if moving over 1.2 degree per turn and the angle is not getting smaller.
        // Huge values of OldTarget (beyond pi) indicate that previously we had no target.
        if ((abs(dt_angle) >= LbFPMath_PI/75) && (p_tank->OldTarget < abs(angle) || p_tank->OldTarget > LbFPMath_PI)) {
            if (!IsSamplePlaying(p_tank->ThingOffset, 48, 0))
                play_dist_sample(p_tank, 48, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
            p_turret->Flag2 |= TgF2_Unkn0200;
        }
    }

    p_turret->U.UMGun.AngleY -= dt_angle;
    p_tank->OldTarget = abs(angle);
    p_turret->U.UMGun.AngleY = (p_turret->U.UMGun.AngleY + 2*LbFPMath_PI) & LbFPMath_AngleMask;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int find_next_ship_tnode_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_next_ship_tnode\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

extern u32 rand_d;

int find_next_ship_tnode(int a1, int a2, int a3, int a4)
{
    struct TrafficNode *p_tnode;
    ubyte flags_match;
    int i;

    DIFFTEST_SHADOW_RET(find_next_ship_tnode, int,
      find_next_ship_tnode_asm(a1, a2, a3, a4));
    flags_match = (ubyte)a2;

    for (i = 0; i < 12; i++)
    {
        u32 rnd;
        short link;

        rnd = rand_d;
        rand_d = ((rnd >> 7) | (rnd << 25)) + 0x16365267;
        rnd &= 3;
        p_tnode = &game_traffic_nodes[(short)a1];
        link = p_tnode->UTraffic.Link[rnd];
        if (link == 0)
            continue;
        if ((p_tnode->UTraffic.Flags[rnd] & 0xC0) == flags_match)
        {
            // TODO the asm only loads the low 16 bits of the link into a
            // register which held the game_traffic_nodes pointer, so the
            // upper half of the result comes from that pointer
            u32 val = ((u32)game_traffic_nodes & 0xFFFF0000) | (ushort)link;
            DIFFTEST_RETURN(-(int)val);
        }
    }

    for (i = 0; i < 4; i++)
    {
        p_tnode = &game_traffic_nodes[(short)a1];
        if (p_tnode->UTraffic.Link[i] == 0)
            continue;
        if ((p_tnode->UTraffic.Flags[i] & 0xC0) == flags_match)
            DIFFTEST_RETURN(-(int)(ushort)p_tnode->UTraffic.Link[i]);
    }
    DIFFTEST_RETURN((int)0);
}


extern s32 data_1c84f4;
extern s32 data_1c84f8[];

static DIFFTEST_REF_FUNC void process_ship_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_ship\n"
        : : "a" (p_vehicle));
}

void process_ship(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    struct TrafficNode *p_lnode;
    struct M33 *p_mat;
    s32 dx, dz;
    s32 x, z;
    s32 dist_sq;
    int angle;
    int i;
    short tnode;
    ubyte flags;

    DIFFTEST_SHADOW(process_ship, process_ship_asm(p_vehicle));

    p_tnode = &game_traffic_nodes[p_vehicle->U.UVehicle.TNode];
    dx = p_tnode->X - (p_vehicle->X >> 8);
    dz = p_tnode->Z - (p_vehicle->Z >> 8);
    angle = (ushort)arctan(dx, dz) - p_vehicle->U.UVehicle.AngleY;
    if (angle > 0x400)
        angle -= 0x7FF;
    if (angle < -0x400)
        angle += 0x7FF;
    if (angle > 160)
        angle = 160;
    if (angle < -160)
        angle = -160;
    if (abs(angle) > 40)
        angle >>= 2;
    if (p_vehicle->U.UVehicle.AngleDY > angle)
        p_vehicle->U.UVehicle.AngleDY--;
    if (p_vehicle->U.UVehicle.AngleDY < angle)
        p_vehicle->U.UVehicle.AngleDY++;
    p_vehicle->U.UVehicle.ReqdSpeed = 300 - 4 * abs(p_vehicle->U.UVehicle.AngleDY);

    dist_sq = dx * dx + dz * dz;
    if (dist_sq < 0x100000)
    {
        p_tnode = &game_traffic_nodes[p_vehicle->U.UVehicle.TNode];
        flags = p_tnode->Flags & 0xFF;
        if ((flags & 0x80) != 0)
        {
            if (dist_sq < 0x20000)
            {
                ushort rnd;

                *(ubyte *)&p_tnode->Flags = flags & 0xBF;
                rnd = LbRandomAnyShort();
                p_vehicle->U.UVehicle.TNode = (short)data_1c84f8[(s32)rnd % data_1c84f4];
                p_tnode = &game_traffic_nodes[p_vehicle->U.UVehicle.TNode];
                x = p_tnode->X << 8;
                z = p_tnode->Z << 8;
                move_mapwho(p_vehicle, x, p_vehicle->Y, z);
                tnode = p_vehicle->U.UVehicle.TNode;
                for (i = 0; i < 4; i++)
                {
                    p_tnode = &game_traffic_nodes[tnode];
                    if (p_tnode->UTraffic.Link[i] == 0)
                        continue;
                    if ((p_tnode->UTraffic.Flags[i] & 0xC0) != 0)
                        continue;
                    p_lnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[i]];
                    dx = (p_lnode->X << 8) - x;
                    dz = (p_lnode->Z << 8) - z;
                    break;
                }
                p_vehicle->U.UVehicle.AngleDY = 0;
                p_vehicle->U.UVehicle.AngleY = arctan(dx, dz);
            }
        }
        else if (p_vehicle->State == 0x3F)
        {
            short gate;

            gate = p_tnode->GateLink;
            if (gate != 0)
            {
                p_vehicle->U.UVehicle.TNode = gate;
            }
            else
            {
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                if (p_vehicle->U.UVehicle.AngleDY > 0)
                    p_vehicle->U.UVehicle.AngleDY -= 4;
                if (p_vehicle->U.UVehicle.AngleDY < 0)
                    p_vehicle->U.UVehicle.AngleDY += 4;
            }
        }
        else if (p_vehicle->State == 0x40)
        {
            short next;

            next = find_next_ship_tnode(p_vehicle->U.UVehicle.TNode, 0, 0, 0);
            if (next == 0)
            {
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
            }
            else if ((game_traffic_nodes[next].Flags & 0x40) != 0)
            {
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                if (p_vehicle->U.UVehicle.AngleDY > 0)
                    p_vehicle->U.UVehicle.AngleDY -= 4;
                if (p_vehicle->U.UVehicle.AngleDY < 0)
                    p_vehicle->U.UVehicle.AngleDY += 4;
            }
            else
            {
                *(ubyte *)&game_traffic_nodes[p_vehicle->U.UVehicle.TNode].Flags &= ~0x40;
                p_vehicle->U.UVehicle.TNode = next;
                *(ubyte *)&game_traffic_nodes[p_vehicle->U.UVehicle.TNode].Flags |= 0x40;
                if ((game_traffic_nodes[p_vehicle->U.UVehicle.TNode].Flags & 0x810) != 0)
                    p_vehicle->U.UVehicle.LeisurePlace = 200;
            }
        }
    }

    tnode = p_vehicle->U.UVehicle.TNode;
    for (i = 0; i < 4; i++)
    {
        p_tnode = &game_traffic_nodes[tnode];
        if ((p_tnode->UTraffic.Flags[i] & 0x40) == 0)
            continue;
        p_lnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[i]];
        if ((p_lnode->Flags & 0x40) == 0)
            continue;
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        if (p_vehicle->U.UVehicle.AngleDY > 0)
            p_vehicle->U.UVehicle.AngleDY -= 4;
        if (p_vehicle->U.UVehicle.AngleDY < 0)
            p_vehicle->U.UVehicle.AngleDY += 4;
        break;
    }

    if (p_vehicle->U.UVehicle.LeisurePlace != 0)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        if (p_vehicle->U.UVehicle.AngleDY > 0)
            p_vehicle->U.UVehicle.AngleDY -= 4;
        if (p_vehicle->U.UVehicle.AngleDY < 0)
            p_vehicle->U.UVehicle.AngleDY += 4;
        p_vehicle->U.UVehicle.LeisurePlace--;
    }

    if (((game_traffic_nodes[p_vehicle->U.UVehicle.TNode].Flags & 0x10) != 0)
      && (p_vehicle->U.UVehicle.LeisurePlace != 0))
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        if (p_vehicle->U.UVehicle.AngleDY > 0)
            p_vehicle->U.UVehicle.AngleDY -= 4;
        if (p_vehicle->U.UVehicle.AngleDY < 0)
            p_vehicle->U.UVehicle.AngleDY += 4;
        p_vehicle->U.UVehicle.LeisurePlace--;
        if ((p_vehicle->U.UVehicle.LeisurePlace == 0)
          && (p_vehicle->U.UVehicle.PassengerHead == 0))
            p_vehicle->U.UVehicle.LeisurePlace = 150;
    }

    if (p_vehicle->Speed < p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed++;
    if (p_vehicle->Speed > p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed -= 10;

    p_vehicle->U.UVehicle.AngleY += (p_vehicle->U.UVehicle.AngleDY >> 4);
    p_vehicle->U.UVehicle.AngleY &= 0x7FF;

    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY), 0, (int)p_mat);
    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    move_mapwho(p_vehicle, p_vehicle->X + p_vehicle->VX,
      p_vehicle->Y + p_vehicle->VY, p_vehicle->Z + p_vehicle->VZ);
}

static DIFFTEST_REF_FUNC void process_mech_stationary_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_stationary\n"
        : : "a" (p_vehicle));
}

void process_mech_stationary(struct Thing *p_vehicle)
{
    struct Thing *p_target;
    struct M33 *p_mat;
    s32 dx, dz;
    s32 angle;

    DIFFTEST_SHADOW(process_mech_stationary, process_mech_stationary_asm(p_vehicle));
    p_target = p_vehicle->PTarget;
    if (p_target != NULL)
    {
        dx = (p_target->X >> 8) - (p_vehicle->X >> 8);
        dz = (p_target->Z >> 8) - (p_vehicle->Z >> 8);
        angle = (ushort)arctan(dx, dz) - p_vehicle->U.UVehicle.AngleY;
        p_vehicle->OldTarget = abs(angle);
        if (angle > 0x400)
            angle -= 0x7ff;
        if (angle < -0x400)
            angle += 0x7ff;
        if (angle > 390)
            angle = 390;
        if (angle < -390)
            angle = -390;
        p_vehicle->U.UVehicle.AngleDY = angle;
    }
    else if ((p_vehicle->Flag & TngF_ShootAtPos) != 0)
    {
        dx = p_vehicle->U.UVehicle.TargetDX - (p_vehicle->X >> 8);
        dz = p_vehicle->U.UVehicle.TargetDZ - (p_vehicle->Z >> 8);
        angle = (ushort)arctan(dx, dz) - p_vehicle->U.UVehicle.AngleY;
        p_vehicle->OldTarget = abs(angle);
        if (angle > 0x400)
            angle -= 0x7ff;
        if (angle < -0x400)
            angle += 0x7ff;
        if (angle > 390)
            angle = 390;
        if (angle < -390)
            angle = -390;
        p_vehicle->U.UVehicle.AngleDY = angle;
    }
    else
    {
        angle = p_vehicle->U.UVehicle.AngleDY;
        p_vehicle->U.UVehicle.AngleDY = angle - (angle >> 4);
        if (abs(p_vehicle->U.UVehicle.AngleDY) < 16)
            p_vehicle->U.UVehicle.AngleDY = 0;
    }
    p_vehicle->U.UVehicle.AngleY += (p_vehicle->U.UVehicle.AngleDY >> 2);
    // The asm masks only the high byte of AngleY
    p_vehicle->U.UVehicle.AngleY &= 0x7ff;
    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY), 0, (int)p_mat);
}

static DIFFTEST_REF_FUNC void process_mech_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech\n"
        : : "a" (p_vehicle));
}

void process_mech(struct Thing *p_vehicle)
{
    struct M33 *p_mat;
    s32 tgt_x, tgt_z;
    s32 dt_x, dt_z, dist_sq;
    s32 angle;
    s32 new_x, new_y, new_z;

    DIFFTEST_SHADOW(process_mech, process_mech_asm(p_vehicle));
    if (((gameturn + (ushort)p_vehicle->ThingOffset) & 0x7f) == 0)
        play_dist_sample(p_vehicle, 99, 127, 64, 100, 0, 3);

    if ((game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)].Flags2 & MEF2_Unkn08) == 0)
    {
        s32 alt;

        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        alt += 0xdac;
        if (alt > p_vehicle->Y)
            p_vehicle->Y = p_vehicle->Y + ((alt - p_vehicle->Y) >> 4) + 0x40;
        if (alt < p_vehicle->Y)
            p_vehicle->Y = p_vehicle->Y - (((p_vehicle->Y - alt) >> 4) + 0x40);
    }

    tgt_x = 0;
    tgt_z = 0;
    if (((ushort)p_vehicle->State == 0x33) &&
      ((p_vehicle->PTarget != NULL) || ((p_vehicle->Flag & TngF_ShootAtPos) != 0)))
    {
        if (p_vehicle->PTarget != NULL)
        {
            tgt_x = p_vehicle->PTarget->X >> 8;
            tgt_z = p_vehicle->PTarget->Z >> 8;
        }
        else if ((p_vehicle->Flag & TngF_ShootAtPos) != 0)
        {
            tgt_x = p_vehicle->U.UVehicle.TargetDX;
            tgt_z = p_vehicle->U.UVehicle.TargetDZ;
        }
        // TODO the asm leaves the target uninitialised if neither is set;
        // unreachable because of the enclosing condition
    }
    else
    {
        tgt_x = p_vehicle->U.UVehicle.GotoX;
        tgt_z = p_vehicle->U.UVehicle.GotoZ;
    }

    dt_x = tgt_x - (p_vehicle->X >> 8);
    dt_z = tgt_z - (p_vehicle->Z >> 8);
    angle = (ushort)arctan(dt_x, dt_z) - p_vehicle->U.UVehicle.AngleY;
    if (angle > 0x400)
        angle -= 0x7ff;
    if (angle < -0x400)
        angle += 0x7ff;
    if (angle > 390)
        angle = 390;
    if (angle < -390)
        angle = -390;
    p_vehicle->U.UVehicle.AngleDY = angle;

    dist_sq = dt_x * dt_x + dt_z * dt_z;
    p_vehicle->U.UVehicle.ReqdSpeed = 900 - 2 * abs(p_vehicle->U.UVehicle.AngleDY);
    if ((ushort)p_vehicle->State == 0x33)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        if (abs(p_vehicle->Speed) < 50)
        {
            p_vehicle->State = 0x21;
            p_vehicle->Speed = 0;
        }
    }
    else if (dist_sq < 0x30000)
    {
        if ((ubyte)do_next_navigate_index((int)p_vehicle, 0, 0, 0))
        {
            p_vehicle->U.UVehicle.PathIndex = 0;
            p_vehicle->State = 0x33;
        }
        else
        {
            struct MyPath *p_path;
            ubyte pt;

            p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
            pt = p_vehicle->PathOffset & 3;
            p_vehicle->U.UVehicle.GotoX = p_path->X[pt];
            p_vehicle->U.UVehicle.GotoZ = p_path->Z[pt];
        }
    }

    if (p_vehicle->Speed < p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed += 0x30;
    if (p_vehicle->Speed > p_vehicle->U.UVehicle.ReqdSpeed)
        p_vehicle->Speed -= 0x30;

    p_vehicle->U.UVehicle.AngleY = ((p_vehicle->U.UVehicle.AngleDY >> 3)
      + p_vehicle->U.UVehicle.AngleY) & 0x7ff;
    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY), 0, (int)p_mat);

    p_vehicle->VX = (-p_mat->R[0][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VY = (-p_mat->R[1][2] * p_vehicle->Speed) >> 10;
    p_vehicle->VZ = (-p_mat->R[2][2] * p_vehicle->Speed) >> 10;
    new_x = p_vehicle->X + p_vehicle->VX;
    new_y = p_vehicle->Y + p_vehicle->VY;
    new_z = p_vehicle->Z + p_vehicle->VZ;
    move_mapwho(p_vehicle, new_x, new_y, new_z);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void matrix_transpose_cp_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_matrix_transpose_cp\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "ecx", "esi", "edi", "cc", "memory");
}

/** Copies the transpose of the 3x3 matrix at a2 into the matrix at a1.
 * The order of stores is kept from the asm, as the matrices may alias.
 */
void matrix_transpose_cp(int a1, int a2, int a3)
{
    s32 *p_dst = (s32 *)a1;
    s32 *p_src = (s32 *)a2;

    DIFFTEST_SHADOW(matrix_transpose_cp, matrix_transpose_cp_asm(a1, a2, a3));
    p_dst[0] = p_src[0];
    p_dst[1] = p_src[3];
    p_dst[2] = p_src[6];
    p_dst[3] = p_src[1];
    p_dst[4] = p_src[4];
    p_dst[5] = p_src[7];
    p_dst[6] = p_src[2];
    p_dst[7] = p_src[5];
    p_dst[8] = p_src[8];
}
/** Moves p_from towards p_to by 1/div of the shortest delta on the 0x800
 * wrapping grid, writing the result to p_out. The asm takes p_out in
 * esi; its asm caller passes it in ecx through ac_mech_unkn_func_20.
 */
static DIFFTEST_REF_FUNC void mech_unkn_func_20_asm(struct M31 *p_from,
  struct M31 *p_to, s32 div, struct M31 *p_out)
{
    int ra = (int)p_from, rd = (int)p_to, rb = div;
    asm volatile ("call ASM_mech_unkn_func_20\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+S" (p_out)
        : : "ecx", "edi", "cc", "memory");
}

void mech_unkn_func_20(struct M31 *p_from, struct M31 *p_to, s32 div,
  struct M31 *p_out)
{
    struct M31 res;
    s32 dt, alt, sign, adt, aalt;
    int i;

    DIFFTEST_SHADOW(mech_unkn_func_20,
      mech_unkn_func_20_asm(p_from, p_to, div, p_out));
    for (i = 0; i < 3; i++)
    {
        dt = p_to->R[i] - p_from->R[i];
        if (dt < 0)
            sign = -1;
        else
            sign = (dt != 0);
        adt = (dt < 0) ? -dt : dt;
        alt = -(sign * (0x800 - adt));
        aalt = (alt < 0) ? -alt : alt;
        if (aalt < adt)
            dt = alt;
        res.R[i] = dt;
    }
    if (div != 0)
    {
        res.R[0] /= div;
        res.R[1] /= div;
        res.R[2] /= div;
    }
    // All three are computed before storing, as p_out may alias p_from
    res.R[0] = (res.R[0] + p_from->R[0]) & 0x7FF;
    res.R[1] = (res.R[1] + p_from->R[1]) & 0x7FF;
    res.R[2] = (res.R[2] + p_from->R[2]) & 0x7FF;
    *p_out = res;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void mech_unkn_func_19_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_mech_unkn_func_19\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void mech_unkn_func_19(int a1, int a2, int a3, int a4)
{
    // The asm keeps these in a 0xCC byte stack buffer, so up to 17 entries
    struct M31 angles[17];
    struct M31 vec;
    struct M31 dlt;
    struct unkn_mech_struc3 *p_u3itm;
    struct unkn_mech_struc4 *p_u4itm;
    struct unkn_mech_struc5 *p_u5itm;
    s32 *p_pos;
    ushort idx;
    s32 div;
    u32 i;

    DIFFTEST_SHADOW(mech_unkn_func_19, mech_unkn_func_19_asm(a1, a2, a3, a4));
    for (i = 0; i < (u32)unkn_mech_arr3[a1].mech3_unkn_fld_8; i++)
    {
        p_u4itm = &unkn_mech_arr4[unkn_mech_arr3[a1].mech3_unkn_arr4_idx + i];
        angles[i].R[0] = p_u4itm->angle_49;
        angles[i].R[1] = p_u4itm->angle_4D;
        angles[i].R[2] = p_u4itm->angle_51;
    }

    for (i = 0; i < (u32)unkn_mech_arr3[a1].mech3_unkn_fld_8; i++)
    {
        p_u3itm = &unkn_mech_arr3[a1];
        p_u4itm = &unkn_mech_arr4[p_u3itm->mech3_unkn_arr4_idx + i];
        p_u5itm = &unkn_mech_arr5[p_u3itm->mech3_unkn_fld_54[2]];
        idx = p_u4itm->field_23;
        if (p_u5itm->field_18C[12 + idx] == 0)
            continue;
        // The asm passes the triple at 0xC + idx * 0x18 in unkn_mech_struc5
        p_pos = (s32 *)((ubyte *)p_u5itm + 0xC + idx * 0x18);
        mech_unkn_func_20((struct M31 *)&p_u4itm->angle_49,
          (struct M31 *)p_pos, p_u3itm->mech3_unkn_fld_54[0], &vec);
        angles[i] = vec;

        p_u3itm = &unkn_mech_arr3[a1];
        p_u4itm = &unkn_mech_arr4[p_u3itm->mech3_unkn_arr4_idx + i];
        p_u5itm = &unkn_mech_arr5[p_u3itm->mech3_unkn_fld_54[2]];
        idx = p_u4itm->field_23;
        // The asm reads the triple at idx * 0x18 in unkn_mech_struc5
        p_pos = (s32 *)((ubyte *)p_u5itm + idx * 0x18);
        div = p_u3itm->mech3_unkn_fld_54[0];
        dlt.R[0] = p_pos[0] - p_u4itm->field_6D.R[0];
        dlt.R[1] = p_pos[1] - p_u4itm->field_6D.R[1];
        dlt.R[2] = p_pos[2] - p_u4itm->field_6D.R[2];
        if (div != 0)
        {
            dlt.R[0] /= div;
            dlt.R[1] /= div;
            dlt.R[2] /= div;
        }
        dlt.R[0] += p_u4itm->field_6D.R[0];
        dlt.R[1] += p_u4itm->field_6D.R[1];
        dlt.R[2] += p_u4itm->field_6D.R[2];
        p_u4itm = &unkn_mech_arr4[unkn_mech_arr3[a1].mech3_unkn_arr4_idx + i];
        p_u4itm->field_6D = dlt;
    }

    for (i = 0; i < (u32)unkn_mech_arr3[a1].mech3_unkn_fld_8; i++)
    {
        p_u4itm = &unkn_mech_arr4[unkn_mech_arr3[a1].mech3_unkn_arr4_idx + i];
        p_u4itm->angle_49 = angles[i].R[0];
        p_u4itm->angle_4D = angles[i].R[1];
        p_u4itm->angle_51 = angles[i].R[2];
    }
}

static DIFFTEST_REF_FUNC void mech_unkn_func_05_asm(int owntng, int a2, int a3)
{
    asm volatile ("call ASM_mech_unkn_func_05\n"
        : : "a" (owntng), "d" (a2), "b" (a3));
}

void mech_unkn_func_05(int owntng, int a2, int a3)
{
    struct unkn_mech_struc3 *p_u3itm;
    struct unkn_mech_struc1 *p_u1itm;
    s32 idx;

    DIFFTEST_SHADOW(mech_unkn_func_05, mech_unkn_func_05_asm(owntng, a2, a3));
    idx = unkn_mech_arr2[a3].field_0;
    p_u3itm = &unkn_mech_arr3[owntng];
    // The asm accesses each s32/ubyte field pair below as a 2-element array
    (&p_u3itm->field_6C)[a2] = a3;
    p_u1itm = &unkn_mech_arr1[idx];
    p_u3itm->mech3_unkn_fld_54[a2] = p_u1itm->field_D;
    (&p_u3itm->mech3_unkn_fld_54[2])[a2] = p_u1itm->field_9;
    (&p_u3itm->field_64)[a2] = idx;
    (&p_u3itm->field_74)[a2] = 1;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int mech_unkn_func_08_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_mech_unkn_func_08\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int mech_unkn_func_08(int a1, int a2, int a3, int a4)
{
    struct unkn_mech_struc3 *p_u3itm;
    struct unkn_mech_struc1 *p_arr1;
    struct unkn_mech_struc2 *p_u2itm;
    s32 ret;
    s32 cnt;
    s32 idx;
    int i;

    DIFFTEST_SHADOW_RET(mech_unkn_func_08, int, mech_unkn_func_08_asm(a1, a2, a3, a4));
    p_arr1 = unkn_mech_arr1;
    ret = 0x7fffffff;
    // The asm accesses each s32/ubyte field pair below as a 2-element array
    for (i = 1; i >= 0; i--)
    {
        p_u3itm = &unkn_mech_arr3[a1];
        if ((&p_u3itm->field_74)[i] == 0)
            continue;
        cnt = p_u3itm->mech3_unkn_fld_54[i];
        if (cnt > 0)
        {
            p_u3itm->mech3_unkn_fld_54[i] = cnt - 1;
            continue;
        }
        idx = p_arr1[(&p_u3itm->field_64)[i]].field_5;
        p_u2itm = &unkn_mech_arr2[(&p_u3itm->field_6C)[i]];
        if (idx == p_u2itm->field_0 && p_u2itm->field_4[4] == 0)
        {
            (&p_u3itm->field_74)[i] = 0;
            p_u3itm->mech3_unkn_fld_54[i] = 0;
            continue;
        }
        (&p_u3itm->field_64)[i] = idx;
        p_u3itm->mech3_unkn_fld_54[i] = p_arr1[idx].field_D;
        ret = p_arr1[(&p_u3itm->field_64)[i]].field_9;
        (&p_u3itm->mech3_unkn_fld_54[2])[i] = ret;
    }
    // The asm stores back the value it loaded at entry
    unkn_mech_arr1 = p_arr1;
    mech_unkn_func_19(a1, 0, 0, 0);
    DIFFTEST_RETURN((int)ret);
}



static DIFFTEST_REF_FUNC void process_mech_unknown1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_unknown1\n"
        : : "a" (p_vehicle));
}

void process_mech_unknown1(struct Thing *p_vehicle)
{
    struct M33 *p_mat;
    ubyte *p_tnode8;
    u32 ret;

    DIFFTEST_SHADOW(process_mech_unknown1, process_mech_unknown1_asm(p_vehicle));
    mech_last_cor_x = p_vehicle->X >> 8;
    mech_last_cor_y = p_vehicle->Y >> 8;
    // TODO: Z is set from X, not from p_vehicle->Z; kept as in the original
    mech_last_cor_z = mech_last_cor_x;
    mech_unkn_tile_y2 = mech_last_cor_x;
    mech_unkn_tile_x3 = mech_last_cor_y;
    mech_unkn_tile_y3 = mech_unkn_tile_y2;

    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    matrix_transpose_cp((int)&unkn_mech_arr3[(ushort)p_vehicle->Owner].mech3_unkn_mat_C,
      (int)p_mat, (int)p_vehicle);

    // The asm modifies only the low byte of TNode
    p_tnode8 = (ubyte *)&p_vehicle->U.UVehicle.TNode;
    ret = mech_unkn_func_08((ushort)p_vehicle->Owner, (int)p_mat, (int)p_vehicle, (int)p_mat);
    switch (ret)
    {
    case 0:
        *p_tnode8 |= 0x04;
        break;
    case 6:
        *p_tnode8 |= 0x02;
        *p_tnode8 &= ~0x04;
        break;
    case 8:
        *p_tnode8 |= 0x01;
        *p_tnode8 &= ~0x04;
        break;
    case 14:
        *p_tnode8 |= 0x08;
        *p_tnode8 &= ~0x04;
        break;
    default:
        break;
    }

    if (p_vehicle->State == VehSt_GOTO_LOC &&
      (ushort)p_vehicle->U.UVehicle.ZebraOldHealth != (ushort)p_vehicle->Health)
    {
        remove_path(p_vehicle);
        p_vehicle->U.UVehicle.PathIndex = 0;
        p_vehicle->State = VehSt_UNKN_33;
    }
    p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
}

static DIFFTEST_REF_FUNC int process_my_crashing_vehicle_asm(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_process_my_crashing_vehicle\n"
        : "=a" (ret) : "a" (p_vehicle));
    return ret;
}

int process_my_crashing_vehicle(struct Thing *p_vehicle)
{
    struct MyMapElement *p_mapel;
    int alt;
    TbBool hit_ground;

    DIFFTEST_SHADOW_RET(process_my_crashing_vehicle, int, process_my_crashing_vehicle_asm(p_vehicle));
    if ((LbRandomAnyShort() & 0xF) == 0)
    {
        ushort rnd_z, rnd_x;

        p_vehicle->U.UVehicle.AngleX -= (LbRandomAnyShort() & 0x7F);
        p_vehicle->U.UVehicle.AngleX += 0x3F;
        if (p_vehicle->SubType != SubTT_VEH_GROUND)
        {
            p_vehicle->VY += 0x400;
            p_vehicle->Y += 0x2000;
            p_vehicle->U.UVehicle.AngleY -= (LbRandomAnyShort() & 0x7F);
            p_vehicle->U.UVehicle.AngleZ -= (LbRandomAnyShort() & 0x7F);
            p_vehicle->U.UVehicle.AngleY += 0x3F;
            p_vehicle->U.UVehicle.AngleZ += 0x3F;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleY -= (LbRandomAnyShort() & 0x3F);
            p_vehicle->U.UVehicle.AngleZ -= (LbRandomAnyShort() & 0x3F);
            p_vehicle->U.UVehicle.AngleY += 0x1F;
            p_vehicle->U.UVehicle.AngleZ += 0x1F;
        }
        rnd_z = LbRandomAnyShort();
        rnd_x = LbRandomAnyShort();
        bang_new4(p_vehicle->X + rnd_x, p_vehicle->Y >> 3,
          p_vehicle->Z + rnd_z, 100);
    }
    if ((gameturn & 1) != 0)
        bang_new4(p_vehicle->X, p_vehicle->Y >> 3, p_vehicle->Z, 0x35);

    p_vehicle->U.UVehicle.AngleX += 5;
    if (p_vehicle->SubType != SubTT_VEH_GROUND)
    {
        p_vehicle->VY -= 0x110;
        p_vehicle->U.UVehicle.AngleZ -= (p_vehicle->VY >> 7);
    }
    else
    {
        p_vehicle->VY -= 0x80;
        p_vehicle->U.UVehicle.AngleZ -= (p_vehicle->VY >> 8);
    }
    if (p_vehicle->U.UVehicle.AngleX < 0)
        p_vehicle->U.UVehicle.AngleX = 0;
    if (p_vehicle->U.UVehicle.AngleY < 0)
        p_vehicle->U.UVehicle.AngleY = 0;
    if (p_vehicle->U.UVehicle.AngleZ < 0)
        p_vehicle->U.UVehicle.AngleZ = 0;
    p_vehicle->U.UVehicle.AngleX &= 0x7FF;
    p_vehicle->U.UVehicle.AngleZ &= 0x7FF;

    p_mapel = &game_my_big_map[((p_vehicle->Z >> 16) << 7) + (p_vehicle->X >> 16)];

    if ((p_vehicle->SubType == SubTT_VEH_GROUND) && ((gameturn & 1) != 0))
    {
        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        FIRE_new(p_vehicle->X, alt >> 5, p_vehicle->Z, 5);
    }

    if (p_vehicle->SubType == SubTT_VEH_FLYING)
    {
        int radius_alt;

        radius_alt = (2 * 193 * (int)(ushort)p_vehicle->Radius) >> 5;
        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        hit_ground = (((alt + radius_alt) << 3) > p_vehicle->Y);
    }
    else
    {
        alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8));
        hit_ground = ((alt << 3) + 0x320 > p_vehicle->Y);
    }
    if (!hit_ground && (p_vehicle->X >= 0x10000) && (p_vehicle->Z >= 0x10000)
      && (p_vehicle->X <= 0x7E0000) && (p_vehicle->Z <= 0x7E0000)
      && ((p_mapel[-1].ColumnHead & 0xFFF) == 0)
      && ((p_mapel[1].ColumnHead & 0xFFF) == 0)
      && ((p_mapel[-MAP_TILE_WIDTH].ColumnHead & 0xFFF) == 0)
      && ((p_mapel[MAP_TILE_WIDTH].ColumnHead & 0xFFF) == 0))
    {
        DIFFTEST_RETURN((int)0x2A);
    }
    p_vehicle->Y >>= 3;
    init_vehicle_explode(p_vehicle);
    p_vehicle->Y <<= 3;
    DIFFTEST_RETURN((int)0);
}
static DIFFTEST_REF_FUNC void set_xz_for_unkn3_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_xz_for_unkn3\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Pick a random point on a face quad, within the triangle of its first
 * three points; returns map coords in *a2 (x) and *a3 (z).
 */
void set_xz_for_unkn3(int a1, int a2, int a3, int a4)
{
    struct SingleObjectFace4 *p_face = (struct SingleObjectFace4 *)a1;
    int *p_pos_x = (int *)a2;
    int *p_pos_z = (int *)a3;
    struct SingleObject *p_sobj;
    struct SinglePoint *p_pt0, *p_pt1, *p_pt2;
    ushort rnd_x, rnd_z;
    short dt_x, dt_z;

    DIFFTEST_SHADOW(set_xz_for_unkn3, set_xz_for_unkn3_asm(a1, a2, a3, a4));
    rnd_x = LbRandomAnyShort() & 0xF;
    rnd_z = LbRandomAnyShort() & 0xF;
    p_sobj = &game_objects[p_face->Object];
    p_pt0 = &game_object_points[(ushort)p_face->PointNo[0]];
    p_pt1 = &game_object_points[(ushort)p_face->PointNo[1]];
    p_pt2 = &game_object_points[(ushort)p_face->PointNo[2]];

    dt_x = (short)((short)(p_pt1->X - p_pt0->X) * rnd_x
      + (short)(p_pt2->X - p_pt0->X) * (16 - rnd_x));
    dt_z = (short)((short)(p_pt1->Z - p_pt0->Z) * rnd_z
      + (short)(p_pt2->Z - p_pt0->Z) * (16 - rnd_z));
    *p_pos_x = (short)((dt_x >> 4) + p_pt0->X + p_sobj->MapX);
    *p_pos_z = (short)((dt_z >> 4) + p_pt0->Z + p_sobj->MapZ);
}


static DIFFTEST_REF_FUNC int train_unkn_st21_exit_func_1_asm(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_train_unkn_st21_exit_func_1\n"
        : "=a" (ret) : "a" (p_vehicle));
    return ret;
}

int train_unkn_st21_exit_func_1(struct Thing *p_vehicle)
{
    DIFFTEST_SHADOW_RET(train_unkn_st21_exit_func_1, int,
      train_unkn_st21_exit_func_1_asm(p_vehicle));
    while (p_vehicle->U.UVehicle.PassengerHead != 0)
    {
        struct Thing *p_passngr;
        int pos_x, pos_z;
        short face;

        things[(ushort)p_vehicle->U.UVehicle.PassengerHead].Flag &=
          ~(TngF_InVehicle | TngF_Unkn02000000);
        p_passngr = &things[(ushort)p_vehicle->U.UVehicle.PassengerHead];
        // TODO the asm reads U.UVehicle.ComCur as the face index of the train
        face = p_vehicle->U.UVehicle.ComCur;
        p_vehicle->U.UVehicle.PassengerHead = p_passngr->U.UPerson.LinkPassenger;
        set_xz_for_unkn3((int)&game_object_faces4[face], (int)&pos_x,
          (int)&pos_z, face);
        p_passngr->X = pos_x << 8;
        p_passngr->Z = pos_z << 8;
        p_passngr->U.UPerson.OnFace = face;
        p_passngr->U.UPerson.OnFace = -face;
        set_thing_height_on_face_quad(p_passngr, pos_x << 8, pos_z << 8, face);
        p_passngr->Flag |= TngF_Unkn0004;
        add_node_thing(p_passngr->ThingOffset);
        p_passngr->U.UPerson.Flag3 |= 0x80;
    }
    DIFFTEST_RETURN((int)0);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void thing_unkn_change_speed_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_thing_unkn_change_speed\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void thing_unkn_change_speed(int a1, int a2, int a3, int a4)
{
    struct Thing *p_thing = (struct Thing *)a1;
    struct Thing *p_other;
    s32 dist_lim, dx, dz, dist_sq;

    DIFFTEST_SHADOW(thing_unkn_change_speed,
      thing_unkn_change_speed_asm(a1, a2, a3, a4));
    p_other = &things[(ushort)a2];
    dist_lim = ((s32)(2 * 193 * (u32)(ushort)p_thing->Radius) >> 8)
      + ((s32)(2 * 193 * (u32)(ushort)p_other->Radius) >> 8) + 0x40;
    dist_lim = dist_lim * dist_lim;
    dx = (p_thing->X - p_other->X) >> 8;
    dz = (p_thing->Z - p_other->Z) >> 8;
    dist_sq = dz * dz + dx * dx;
    if (dist_sq > dist_lim + 0x100)
        p_thing->Speed += (short)a3;
    else if (dist_sq < dist_lim - 0x100)
        p_thing->Speed -= (short)a3;
}


static DIFFTEST_REF_FUNC void train_unkn_st18_func_1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_1\n"
        : : "a" (p_vehicle));
}

void train_unkn_st18_func_1(struct Thing *p_vehicle)
{
    struct Thing *p_station;
    struct Thing *p_other;
    short tnode, angle, dt_angle;
    s32 st_x, st_y, st_z;
    s32 dt_x, dt_y, dt_z, dist_sq;

    DIFFTEST_SHADOW(train_unkn_st18_func_1,
      train_unkn_st18_func_1_asm(p_vehicle));
    // TODO when TNode <= 0 the asm uses uninitialized registers/stack for
    // the station and its position; callers only call it with TNode > 0
    p_station = &things[0];
    st_x = st_y = st_z = 0;
    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode > 0)
    {
        p_station = &things[tnode];
        st_x = p_station->X >> 8;
        st_y = (p_station->Y + 0xf00) >> 3;
        st_z = p_station->Z >> 8;
    }
    dt_x = st_x - (p_vehicle->X >> 8);
    dt_z = st_z - (p_vehicle->Z >> 8);
    dist_sq = dt_x * dt_x + dt_z * dt_z;
    p_vehicle->U.UVehicle.ReqdSpeed = (short)0xfaba;

    if (p_vehicle->U.UVehicle.GotoX != 0)
    {
        p_other = &things[p_vehicle->U.UVehicle.GotoX];
        p_vehicle->U.UVehicle.ReqdSpeed = p_other->U.UVehicle.ReqdSpeed;
        if ((p_other->Flag & TngF_Unkn08000000) != (p_vehicle->Flag & TngF_Unkn08000000))
        {
            ThingIdx tng1, tng2, tng3;

            tng1 = things[p_station->U.UObject.NextThing].U.UObject.NextThing;
            tng2 = things[tng1].U.UObject.NextThing;
            tng3 = things[tng2].U.UObject.NextThing;
            p_vehicle->U.UVehicle.TNode = things[tng3].U.UObject.NextThing;
            p_vehicle->Flag &= ~TngF_Unkn08000000;
            if (p_vehicle->U.UVehicle.ComHead != 0)
                play_dist_sample(p_vehicle, 0x62, 0x7f, 0x40, 100, 0, 1);
            p_other = &things[p_vehicle->U.UVehicle.GotoX];
            p_vehicle->State = 0x15;
            p_vehicle->U.UVehicle.ComHead = p_other->U.UVehicle.ComHead;
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.ComCur = p_other->U.UVehicle.ComCur;
        }
        else
        {
            thing_unkn_change_speed((int)p_vehicle,
              (ushort)p_vehicle->U.UVehicle.GotoX, -200, 0);
        }
    }

    if ((ushort)p_vehicle->State != 0x15 && dist_sq < 0x40000 && tnode > 0)
    {
        if (p_station->U.UObject.PrevThing != 0)
        {
            p_vehicle->U.UVehicle.TNode = p_station->U.UObject.PrevThing;
        }
        else if (p_vehicle->Speed != 0)
        {
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
        }
        else
        {
            ThingIdx tng1, tng2, tng3;
            short new_tnode;

            p_vehicle->Flag &= ~TngF_Unkn08000000;
            tng1 = things[p_station->U.UObject.NextThing].U.UObject.NextThing;
            tng2 = things[tng1].U.UObject.NextThing;
            tng3 = things[tng2].U.UObject.NextThing;
            new_tnode = things[tng3].U.UObject.NextThing;
            p_vehicle->State = 0x13;
            p_vehicle->U.UVehicle.TNode = new_tnode;
            p_vehicle->U.UVehicle.ComHead = p_station->U.UObject.ComHead;
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.ComCur = p_station->U.UObject.ComCur;
        }
    }

    if (dist_sq > 0x4000)
    {
        dt_y = (st_y >> 6) - (p_vehicle->Y >> 6);
        // TODO the asm passes squared values to arctan()
        angle = arctan(dt_y * dt_y, dt_z * dt_z + dt_x * dt_x) - 0x400;
        if (angle > 100)
            angle = 100;
        else if (angle < -100)
            angle = -100;
        if (dt_y > 0)
        {
            p_vehicle->U.UVehicle.AngleX = angle;
            p_vehicle->U.UVehicle.AngleX = -angle;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleX = angle;
        }
    }

    angle = (arctan(dt_x, dt_z) + 0x400) & 0x7ff;
    dt_angle = p_vehicle->U.UVehicle.AngleY - angle;
    if (dt_angle < -0x400)
        dt_angle += 0x800;
    else if (dt_angle > 0x400)
        dt_angle -= 0x800;

    if (dt_angle < 0)
    {
        if (dt_angle < -100)
            p_vehicle->Speed = 0;
        if (dt_angle > -50)
            p_vehicle->U.UVehicle.AngleDY = -2 * dt_angle;
        else
            p_vehicle->U.UVehicle.AngleDY = 0x1e0;
    }
    else if (dt_angle > 0)
    {
        if (dt_angle > 100)
            p_vehicle->Speed = 0;
        if (dt_angle < 50)
        {
            p_vehicle->U.UVehicle.AngleDY = 2 * dt_angle;
            p_vehicle->U.UVehicle.AngleDY = -2 * dt_angle;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleDY = (short)0xfe20;
        }
    }
}

static DIFFTEST_REF_FUNC void train_unkn_st18_func_2_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_2\n"
        : : "a" (p_vehicle));
}

void train_unkn_st18_func_2(struct Thing *p_vehicle)
{
    struct Thing *p_station;
    struct Thing *p_other;
    short tnode, angle, dt_angle;
    s32 st_x, st_y, st_z;
    s32 dt_x, dt_y, dt_z, dist_sq;

    DIFFTEST_SHADOW(train_unkn_st18_func_2,
      train_unkn_st18_func_2_asm(p_vehicle));
    // TODO when TNode <= 0 the asm uses uninitialized registers/stack for
    // the station and its position; callers only call it with TNode > 0
    p_station = &things[0];
    st_x = st_y = st_z = 0;
    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode > 0)
    {
        p_station = &things[tnode];
        st_x = p_station->X >> 8;
        st_y = (p_station->Y + 0xf00) >> 3;
        st_z = p_station->Z >> 8;
    }
    dt_x = st_x - (p_vehicle->X >> 8);
    dt_z = st_z - (p_vehicle->Z >> 8);
    dist_sq = dt_x * dt_x + dt_z * dt_z;
    p_vehicle->U.UVehicle.ReqdSpeed = 0x546;

    if (p_vehicle->Owner != 0)
    {
        p_other = &things[(ushort)p_vehicle->Owner];
        p_vehicle->U.UVehicle.ReqdSpeed = p_other->U.UVehicle.ReqdSpeed;
        if ((p_other->Flag & TngF_Unkn08000000) != (p_vehicle->Flag & TngF_Unkn08000000))
        {
            ThingIdx tng1, tng2, tng3;

            p_vehicle->Flag |= TngF_Unkn08000000;
            tng1 = things[p_station->U.UObject.PrevThing].U.UObject.PrevThing;
            tng2 = things[tng1].U.UObject.PrevThing;
            tng3 = things[tng2].U.UObject.PrevThing;
            p_vehicle->State = 0x15;
            p_vehicle->U.UVehicle.TNode = things[tng3].U.UObject.PrevThing;
            if (p_vehicle->U.UVehicle.ComHead != 0)
                play_dist_sample(p_vehicle, 0x62, 0x7f, 0x40, 100, 0, 1);
            p_other = &things[(ushort)p_vehicle->Owner];
            p_vehicle->U.UVehicle.ComHead = p_other->U.UVehicle.ComHead;
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.ComCur = p_other->U.UVehicle.ComCur;
        }
        else
        {
            thing_unkn_change_speed((int)p_vehicle,
              (ushort)p_vehicle->Owner, 200, (ushort)p_vehicle->Owner);
        }
    }

    if ((ushort)p_vehicle->State != 0x15 && dist_sq < 0x40000 && tnode > 0)
    {
        if (p_station->U.UObject.NextThing != 0)
        {
            p_vehicle->U.UVehicle.TNode = p_station->U.UObject.NextThing;
        }
        else if (p_vehicle->Speed != 0)
        {
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
        }
        else
        {
            ThingIdx tng1, tng2, tng3;
            short new_tnode;

            p_vehicle->Flag |= TngF_Unkn08000000;
            tng1 = things[p_station->U.UObject.PrevThing].U.UObject.PrevThing;
            tng2 = things[tng1].U.UObject.PrevThing;
            tng3 = things[tng2].U.UObject.PrevThing;
            new_tnode = things[tng3].U.UObject.PrevThing;
            p_vehicle->State = 0x13;
            p_vehicle->U.UVehicle.TNode = new_tnode;
            p_vehicle->U.UVehicle.ComHead = p_station->U.UObject.ComHead;
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.ComCur = p_station->U.UObject.ComCur;
        }
    }

    if (dist_sq > 0x4000)
    {
        dt_y = (st_y >> 6) - (p_vehicle->Y >> 6);
        // TODO the asm passes squared values to arctan()
        angle = arctan(dt_y * dt_y, dt_z * dt_z + dt_x * dt_x) - 0x400;
        if (angle > 100)
            angle = 100;
        else if (angle < -100)
            angle = -100;
        if (dt_y < 0)
        {
            p_vehicle->U.UVehicle.AngleX = angle;
            p_vehicle->U.UVehicle.AngleX = -angle;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleX = angle;
        }
    }

    angle = arctan(dt_x, dt_z);
    dt_angle = p_vehicle->U.UVehicle.AngleY - angle;
    if (dt_angle < -0x400)
        dt_angle += 0x800;
    else if (dt_angle > 0x400)
        dt_angle -= 0x800;

    if (dt_angle < 0)
    {
        if (dt_angle < -100)
            p_vehicle->Speed = 0;
        if (dt_angle > -50)
            p_vehicle->U.UVehicle.AngleDY = -2 * dt_angle;
        else
            p_vehicle->U.UVehicle.AngleDY = (0x800 - (p_vehicle->Speed >> 3)) >> 2;
    }
    else if (dt_angle > 0)
    {
        if (dt_angle > 100)
            p_vehicle->Speed = 0;
        if (dt_angle < 50)
        {
            p_vehicle->U.UVehicle.AngleDY = 2 * dt_angle;
            p_vehicle->U.UVehicle.AngleDY = -2 * dt_angle;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleDY = -((0x800 - (p_vehicle->Speed >> 3)) >> 2);
        }
    }
}

static DIFFTEST_REF_FUNC void move_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_vehicle\n"
        : : "a" (p_vehicle));
}

void move_vehicle(struct Thing *p_vehicle)
{
    struct M33 *p_mat;
    short angle, dt_angle;
    ushort state;
    int new_x, new_y, new_z;

    DIFFTEST_SHADOW(move_vehicle, move_vehicle_asm(p_vehicle));
    if (((gameturn + (ushort)p_vehicle->ThingOffset) & 3) == 0)
        swoosh_veh((int)p_vehicle, gameturn, 0, 0);

    if ((p_vehicle->SubType != SubTT_VEH_TRAIN) &&
      (p_vehicle->SubType != SubTT_VEH_FLYING))
        check_vehicle_col(p_vehicle);

    angle = p_vehicle->U.UVehicle.AngleY + (p_vehicle->U.UVehicle.AngleDY >> 3);
    p_vehicle->U.UVehicle.AngleY = angle;
    p_vehicle->U.UVehicle.AngleY = (angle + 0x800) & 0x7FF;

    dt_angle = p_vehicle->U.UVehicle.AngleZ - p_vehicle->U.UVehicle.AngleDY;
    if (p_vehicle->SubType == SubTT_VEH_TRAIN)
        p_vehicle->U.UVehicle.AngleZ = 0;
    else if (dt_angle < -100)
        p_vehicle->U.UVehicle.AngleZ += 100;
    else if (dt_angle > 100)
        p_vehicle->U.UVehicle.AngleZ -= 100;
    else if (abs(dt_angle) > 50)
        p_vehicle->U.UVehicle.AngleZ = p_vehicle->U.UVehicle.AngleDY;

    p_mat = &local_mats[p_vehicle->U.UVehicle.MatrixIndex];
    rotate_obj(0, (short)(-p_vehicle->U.UVehicle.AngleY),
      (short)(-(p_vehicle->U.UVehicle.AngleZ >> 1)), (int)p_mat);
    // The asm also rotates a second matrix pointer here, which is always NULL
    if (p_vehicle->U.UVehicle.AngleX != 0)
        rotate_object_axis(p_mat, 0, p_vehicle->U.UVehicle.AngleX, 0);

    p_vehicle->VX = (p_vehicle->Speed * (-p_mat->R[0][2])) >> 10;
    if (p_vehicle->SubType == SubTT_VEH_TRAIN)
        p_vehicle->VY = (p_vehicle->Speed * (-p_mat->R[1][2])) >> 10;
    else
        p_vehicle->VY = 0;
    p_vehicle->VZ = (p_vehicle->Speed * (-p_mat->R[2][2])) >> 10;

    // TODO the pitch rotation is applied a second time; looks like an original bug
    if (p_vehicle->U.UVehicle.AngleX != 0)
        rotate_object_axis(p_mat, 0, p_vehicle->U.UVehicle.AngleX, 0);

    state = (ushort)p_vehicle->State;
    if (state == VehSt_UNKN_36)
    {
        p_vehicle->VX += p_vehicle->U.UVehicle.AccelX << 5;
        p_vehicle->VZ += p_vehicle->U.UVehicle.AccelZ << 5;
    }
    else if ((state == VehSt_UNKN_38) &&
      ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0200) == 0))
    {
        p_vehicle->VX -= p_vehicle->U.UVehicle.AccelX << 5;
        p_vehicle->VZ -= p_vehicle->U.UVehicle.AccelZ << 5;
    }

    new_x = p_vehicle->X + p_vehicle->VX;
    new_y = p_vehicle->Y + p_vehicle->VY;
    new_z = p_vehicle->Z + p_vehicle->VZ;
    move_mapwho(p_vehicle, new_x, new_y, new_z);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void person_clear_route_and_teleport_to_vehicle_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_clear_route_and_teleport_to_vehicle\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
}

void person_clear_route_and_teleport_to_vehicle(int a1, int a2, int a3, int a4)
{
    struct Thing *p_vehicle = (struct Thing *)a1;
    struct Thing *p_person = (struct Thing *)a2;
    short prev_head;

    DIFFTEST_SHADOW(person_clear_route_and_teleport_to_vehicle,
      person_clear_route_and_teleport_to_vehicle_asm(a1, a2, a3, a4));
    prev_head = p_vehicle->U.UVehicle.PassengerHead;
    p_vehicle->U.UVehicle.PassengerHead = p_person->ThingOffset;
    p_person->U.UPerson.OnFace = 0;
    p_person->U.UPerson.LinkPassenger = prev_head;
    p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn20;
    p_person->Flag |= TngF_Unkn02000000 | TngF_InVehicle;
    delete_node(p_person);
    p_person->U.UPerson.Vehicle = p_vehicle->ThingOffset;
    p_person->X = p_vehicle->X;
    p_person->Y = p_vehicle->Y >> 3;
    p_person->Z = p_vehicle->Z;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int train_unkn_func_02_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    // The asm only sets the low 16 bits of esi before returning it;
    // clear esi so the upper bits of the result are defined.
    asm volatile ("call ASM_train_unkn_func_02\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : "S" (0) : "cc", "memory");
    return ra;
}

/** Finds a person standing on given face which waits for a train.
 *
 * @param a1 Index of the face (positive; persons on it have OnFace == -a1).
 * @return Person thing index, or 0 if none found.
 */
int train_unkn_func_02(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person;
    struct Thing *p_owner;
    ushort face;
    ThingIdx person;

    DIFFTEST_SHADOW_RET(train_unkn_func_02, int, train_unkn_func_02_asm(a1, a2, a3, a4));
    face = (ushort)a1;
    for (person = same_type_head[1]; person != 0; person = p_person->LinkSame)
    {
        s32 onface;

        p_person = &things[(ushort)person];
        onface = p_person->U.UPerson.OnFace;
        if (onface >= 0)
            continue;
        if (-onface != (s32)face)
            continue;
        if ((p_person->Flag & TngF_InVehicle) != 0)
            continue;

        if ((p_person->Flag & TngF_Persuaded) != 0)
        {
            p_owner = &things[(ushort)p_person->Owner];
            if ((p_owner->Flag & TngF_InVehicle) == 0)
                continue;
            if (things[p_owner->U.UPerson.Vehicle].SubType != SubTT_VEH_TRAIN)
                continue;
            DIFFTEST_RETURN((int)(ushort)person);
        }
        else if ((p_person->Flag & TngF_PlayerAgent) != 0)
        {
            if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn20) != 0)
                DIFFTEST_RETURN((int)(ushort)person);
            if ((ushort)p_person->State != PerSt_PROTECT_PERSON)
                continue;
            p_owner = &things[(ushort)p_person->Owner];
            if ((p_owner->U.UPerson.Flag3 & PrsF3_Unkn20) != 0)
                DIFFTEST_RETURN((int)(ushort)person);
            if ((p_owner->Flag & TngF_InVehicle) == 0)
                continue;
            if (things[p_owner->U.UPerson.Vehicle].SubType != SubTT_VEH_TRAIN)
                continue;
            DIFFTEST_RETURN((int)(ushort)person);
        }
        else
        {
            if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn80) == 0)
                DIFFTEST_RETURN((int)(ushort)person);
        }
    }
    DIFFTEST_RETURN((int)0);
}



static DIFFTEST_REF_FUNC void train_unkn_st20_func_1_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st20_func_1\n"
        : : "a" (p_vehicle));
}

void train_unkn_st20_func_1(struct Thing *p_vehicle)
{
    struct WalkHeader *p_walk_head;
    struct WalkHeader *p_walk_head2;
    s32 face;
    s32 i, k;
    short item;
    int person;

    DIFFTEST_SHADOW(train_unkn_st20_func_1, train_unkn_st20_func_1_asm(p_vehicle));
    person = 0;
    while (1)
    {
        // TODO the field at this offset is used as a face index for trains
        if (p_vehicle->U.UVehicle.ComHead != 0)
        {
            person = 0;
            face = (short)p_vehicle->U.UVehicle.ComHead;
            p_walk_head = &game_walk_headers[(short)game_object_faces4[face].WalkHeader];
            for (i = p_walk_head->StartItem;
              i < (s32)p_walk_head->Count + (s32)p_walk_head->StartItem; i++)
            {
                item = game_walk_items[i];
                person = train_unkn_func_02((ushort)(-item), 0, 0, 0);
                if ((ushort)person != 0)
                    break;
                face = -(s32)item;
                p_walk_head2 = &game_walk_headers[(short)game_object_faces4[face].WalkHeader];
                for (k = p_walk_head2->StartItem;
                  k < (s32)p_walk_head2->Count + (s32)p_walk_head2->StartItem; k++)
                {
                    person = train_unkn_func_02((ushort)(-game_walk_items[k]), 0, 0, 0);
                    if ((ushort)person != 0)
                        break;
                }
                if ((ushort)person != 0)
                    break;
            }
            if ((ushort)person == 0)
                person = train_unkn_func_02(p_vehicle->U.UVehicle.ComHead, 0, 0, 0);
        }
        // TODO if ComHead becomes 0 after a teleport, the last person is teleported again forever
        if ((ushort)person == 0)
            break;
        person_clear_route_and_teleport_to_vehicle((int)p_vehicle,
          (int)&things[(ushort)person], 0, 0);
        p_vehicle->U.UVehicle.Timer2 = 100;
    }

    p_vehicle->U.UVehicle.Timer2--;
    if (p_vehicle->U.UVehicle.Timer2 <= 0)
    {
        p_vehicle->State = VehSt_UNKN_12;
        play_dist_sample(p_vehicle, 0x61, 0x7f, 0x40, 100, 0, 1);
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void continue_flying_wander(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_continue_flying_wander\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int do_next_navigate_index_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_do_next_navigate_index\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int do_next_navigate_index(int a1, int a2, int a3, int a4)
{
    struct Thing *p_thing = (struct Thing *)a1;
    struct MyPath *p_path;
    ushort head;
    ushort path;
    ushort next;
    ushort count;

    DIFFTEST_SHADOW_RET(do_next_navigate_index, int,
      do_next_navigate_index_asm(a1, a2, a3, a4));
    // The asm only sets %al; upper bits of %eax keep the Thing pointer.
    // Callers test %al only, the upper bits are kept for an exact match.
    head = head_my_path;
    if (p_thing->U.UVehicle.PathIndex == 0)
    {
        head_my_path = head;
        DIFFTEST_RETURN((int)((a1 & ~0xFF) | 1));
    }

    p_path = &my_paths[p_thing->U.UVehicle.PathIndex];
    p_thing->PathOffset++;
    count = path_count - 1;
    if (p_thing->PathOffset >= p_path->Flag)
    {
        path = p_thing->U.UVehicle.PathIndex;
        p_path = &my_paths[path];
        p_path->Next = head;
        path_count = count;
        p_path->Flag = 0;
        p_thing->U.UVehicle.PathIndex = 0;
        head_my_path = path;
        DIFFTEST_RETURN((int)((a1 & ~0xFF) | 1));
    }
    if ((p_thing->PathOffset & 0x3) == 0)
    {
        path = p_thing->U.UVehicle.PathIndex;
        p_path = &my_paths[path];
        p_path->Flag = 0;
        path_count = count;
        next = p_path->Next;
        p_path->Next = head;
        head = path;
        p_thing->U.UVehicle.PathIndex = next;
    }
    head_my_path = head;
    DIFFTEST_RETURN((int)(a1 & ~0xFF));
}



int flying_alt(int a1, int a2, int a3, int a4);

static DIFFTEST_REF_FUNC void process_my_flying_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_flying_vehicle\n"
        : : "a" (p_vehicle));
}

void process_my_flying_vehicle(struct Thing *p_vehicle)
{
    s32 alt, y;
    s32 dx, dz;
    s32 angle, dangle;
    s32 x, z, rx, rz;
    s32 min_x, min_z, min_d;

    DIFFTEST_SHADOW(process_my_flying_vehicle,
      process_my_flying_vehicle_asm(p_vehicle));
    unkn_path_func_001(p_vehicle, 1);
    alt = flying_alt((int)p_vehicle, 0, 0, 0);
    y = p_vehicle->Y;
    if (alt > y)
        p_vehicle->Y = y + ((alt - y) >> 2);
    else
        p_vehicle->Y = y + ((alt - y) >> 4);

    dx = p_vehicle->U.UVehicle.GotoX - (p_vehicle->X >> 8);
    dz = p_vehicle->U.UVehicle.GotoZ - (p_vehicle->Z >> 8);
    angle = (ushort)arctan(dx, dz);
    dangle = angle - p_vehicle->U.UVehicle.AngleY;
    if (dangle > 0x400)
        dangle -= 0x7ff;
    if (dangle < -0x400)
        dangle += 0x7ff;
    if (abs(dangle) < 15)
    {
        p_vehicle->U.UVehicle.AngleDY = 0;
        p_vehicle->U.UVehicle.AngleY = angle;
    }
    else
    {
        p_vehicle->U.UVehicle.AngleDY = dangle;
        if (dangle > 0)
            p_vehicle->U.UVehicle.AngleDY = dangle + 10;
        if (dangle < 0)
            p_vehicle->U.UVehicle.AngleDY -= 10;
    }

    if (dx * dx + dz * dz < 0x40000)
    {
        if ((ushort)p_vehicle->State == 0x43)
        {
            continue_flying_wander((int)p_vehicle, 0, 0, 0);
        }
        else if ((ubyte)do_next_navigate_index((int)p_vehicle, 0, 0, 0) != 0)
        {
            if ((ushort)p_vehicle->State == 0x42)
            {
                // Head for the nearest map edge
                x = p_vehicle->X;
                z = p_vehicle->Z;
                rx = 0x800000 - x;
                rz = 0x800000 - z;
                min_x = (x < rx) ? x : rx;
                min_z = (z < rz) ? z : rz;
                min_d = (min_x < min_z) ? min_x : min_z;
                if (min_d == x)
                    p_vehicle->U.UVehicle.GotoX = 1;
                else if (min_d == rx)
                    p_vehicle->U.UVehicle.GotoX = 0x7e00;
                else if (min_d == z)
                    p_vehicle->U.UVehicle.GotoZ = 1;
                else
                    p_vehicle->U.UVehicle.GotoZ = 0x7e00;
                p_vehicle->State = 0x43;
            }
            else
            {
                p_vehicle->U.UVehicle.PathIndex = 0;
                p_vehicle->State = 0x41;
                p_vehicle->U.UVehicle.TNode = 0;
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_vehicle->U.UVehicle.AngleDY = 0;
                p_vehicle->U.UVehicle.MaxSpeed = 0;
            }
        }
        else
        {
            struct MyPath *p_path;

            p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
            p_vehicle->U.UVehicle.GotoX = p_path->X[p_vehicle->PathOffset & 0x3];
            p_vehicle->U.UVehicle.GotoZ = p_path->Z[p_vehicle->PathOffset & 0x3];
        }
    }
    p_vehicle->U.UVehicle.ReqdSpeed = 2000 - abs(p_vehicle->U.UVehicle.AngleDY << 2);
}

/** Unplacing animation steps: AngleDY (in 1/8 units), AngleX delta, ReqdSpeed.
 */
extern short unplace[];

static DIFFTEST_REF_FUNC void process_unplacing_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unplacing\n"
        : : "a" (p_vehicle));
}

void process_unplacing(struct Thing *p_vehicle)
{
    struct TrafficNode *p_nodes;
    struct TrafficNode *p_tnode;
    s32 step;
    s32 dx, dz;
    int i;

    DIFFTEST_SHADOW(process_unplacing, process_unplacing_asm(p_vehicle));
    p_nodes = game_traffic_nodes;
    step = p_vehicle->U.UVehicle.Dummy4a & 0x3f;
    if (step == 0x14)
    {
        p_vehicle->U.UVehicle.Dummy4b = 0;
        p_vehicle->U.UVehicle.Dummy4b = p_vehicle->U.UVehicle.TNode;
        step = p_vehicle->U.UVehicle.TNode;
        p_tnode = &p_nodes[-step];
        for (i = 0; i < 4; i++)
        {
            if (p_tnode->UTraffic.Link[i] != 0) {
                p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[i];
                break;
            }
        }
        step = 0x13;
    }

    if (step <= 0x12)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = unplace[3 * step + 2];
        p_vehicle->U.UVehicle.AngleDY = unplace[3 * step + 0] << 3;
        p_vehicle->U.UVehicle.AngleX += unplace[3 * step + 1];
        if (step > 0)
            step--;
        game_traffic_nodes = p_nodes;
        if (step == 0)
        {
            p_vehicle->State = 50;
            start_goto_vehicle(p_vehicle, p_vehicle->U.UVehicle.Dummy5b,
              p_vehicle->U.UVehicle.Dummy5a);
        }
    }
    else
    {
        p_tnode = &p_nodes[-p_vehicle->U.UVehicle.TNode];
        p_vehicle->U.UVehicle.ReqdSpeed = -600;
        dx = (p_vehicle->X >> 8) - p_tnode->X;
        dz = (p_vehicle->Z >> 8) - p_tnode->Z;
        game_traffic_nodes = p_nodes;
        if (dx * dx + dz * dz < 0x10000)
            step = 0x12;
    }
    p_vehicle->U.UVehicle.Dummy4a = (p_vehicle->U.UVehicle.Dummy4a & ~0x3f) | step;
}

static DIFFTEST_REF_FUNC void process_stop_as_soon_as_you_can_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stop_as_soon_as_you_can\n"
        : : "a" (p_vehicle));
}

void process_stop_as_soon_as_you_can(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    s32 *p_dummy5;
    s32 *p_accel;
    ubyte *p_dummy5_8;
    s32 dx, dz, adx, adz, dist;
    s32 accel_x, accel_z;
    ushort angle;
    ushort link;

    DIFFTEST_SHADOW(process_stop_as_soon_as_you_can,
      process_stop_as_soon_as_you_can_asm(p_vehicle));

    // Dummy5a/Dummy5b and AccelZ/AccelX are accessed as 32-bit values.
    p_dummy5 = (s32 *)&p_vehicle->U.UVehicle.Dummy5a;
    p_dummy5_8 = (ubyte *)&p_vehicle->U.UVehicle.Dummy5a;
    p_accel = (s32 *)&p_vehicle->U.UVehicle.AccelZ;

    link = (p_vehicle->U.UVehicle.Dummy4a >> 6) & 0x3;
    if (p_vehicle->U.UVehicle.AngleDY >= 20)
        return;
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return;
    if (p_vehicle->U.UVehicle.Dummy4b >= 0)
        return;

    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Dummy4b];
    if ((p_tnode->Flags & 0x8000) != 0)
        return;
    if ((p_tnode->UTraffic.Flags[link] & 0x40) != 0)
        return;

    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
    dx = p_tnode->X - (p_vehicle->X >> 8);
    dz = (p_vehicle->Z >> 8) - p_tnode->Z;
    angle = arctan(dx, -dz);
    *p_dummy5 = angle;
    *p_dummy5 = ((u32)angle) << 16;
    *p_dummy5_8 |= 0x11;

    adx = abs(dx);
    adz = abs(dz);
    if (adz > adx)
        dist = (adx >> 1) + adz;
    else
        dist = (adz >> 1) + adx;

    accel_x = dz << 5;
    accel_z = dx << 5;
    if (dist != 0)
    {
        accel_x /= dist;
        accel_z /= dist;
    }
    *p_accel = accel_x;
    p_vehicle->State = VehSt_UNKN_36;
    *p_accel = ((u32)accel_x) << 16;
    *p_accel = (((u32)accel_x) << 16) | (accel_z & 0xffff);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int flying_alt_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_flying_alt\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int flying_alt(int a1, int a2, int a3, int a4)
{
    struct Thing *p_vehicle = (struct Thing *)a1;
    struct MyMapElement *p_mapel;
    s32 tile_x, tile_z;
    s32 tx, tz;
    s32 alt;
    short thing;

    DIFFTEST_SHADOW_RET(flying_alt, int, flying_alt_asm(a1, a2, a3, a4));
    tile_x = p_vehicle->X >> 16;
    tile_z = p_vehicle->Z >> 16;
    alt = 0x32000;
    if (tile_x < 0 || tile_x >= MAP_TILE_WIDTH ||
      tile_z < 0 || tile_z >= MAP_TILE_WIDTH)
        DIFFTEST_RETURN((int)alt);

    p_mapel = &game_my_big_map[tile_z * MAP_TILE_WIDTH + tile_x];
    if ((p_mapel->ColumnHead & 0xFFF) == 0 && (p_mapel->Flags2 & 0x08) == 0)
        alt += ((s32)p_mapel->Alt) << 11;

    for (tx = tile_x - 2; tx != tile_x + 3; tx++)
    {
        for (tz = tile_z - 2; tz != tile_z + 3; tz++)
        {
            // Note: tiles at index 0 are skipped, as in the asm
            if (tx <= 0 || tx >= MAP_TILE_WIDTH)
                continue;
            if (tz <= 0 || tz >= MAP_TILE_WIDTH)
                continue;
            thing = game_my_big_map[tz * MAP_TILE_WIDTH + tx].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing = &things[thing];
                    if (p_thing->Type == TT_VEHICLE &&
                      p_thing->SubType == SubTT_VEH_FLYING &&
                      p_thing < p_vehicle)
                        alt += 0x1a000;
                    thing = p_thing->Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
    DIFFTEST_RETURN((int)alt);
}


/** Hover oscillation offsets for parked flyers; 99 marks the loop end.
 * Copy of the read-only `quiver` table from the asm data.
 */
static const sbyte quiver[] = {
       0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      -1,   0,   1,   0,  -1,  -3,  -1,   0,
       1,   3,   4,   3,   1,   0,  -2,  -5,
      -7,  -8,  -7,  -5,  -2,   0,   2,   5,
       8,  10,  12,  13,  13,  13,  12,  10,
       8,   5,   2,  -2,  -5,  -8, -10, -12,
     -13, -13, -13, -12, -10,  -8,  -5,  -2,
      99,
};

static DIFFTEST_REF_FUNC void process_hovering_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_hovering_vehicle\n"
        : : "a" (p_vehicle));
}

void process_hovering_vehicle(struct Thing *p_vehicle)
{
    s32 alt;
    ushort qv;

    DIFFTEST_SHADOW(process_hovering_vehicle, process_hovering_vehicle_asm(p_vehicle));
    p_vehicle->U.UVehicle.ReqdSpeed = 0;
    p_vehicle->U.UVehicle.AngleDY = 0;
    alt = flying_alt((int)p_vehicle, 0, 0, 0);
    if (abs(p_vehicle->Y - alt) < 0x6000)
    {
        // MaxSpeed is reused as the index into the quiver table
        qv = p_vehicle->U.UVehicle.MaxSpeed;
        p_vehicle->U.UVehicle.MaxSpeed = qv + 1;
        p_vehicle->Y += quiver[qv] << 7;
    }
    else
    {
        p_vehicle->Y += (alt - p_vehicle->Y) >> 3;
    }
    if (quiver[p_vehicle->U.UVehicle.MaxSpeed] == 99)
        p_vehicle->U.UVehicle.MaxSpeed -= 26;
}

static DIFFTEST_REF_FUNC void process_my_takeoff_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_takeoff\n"
        : : "a" (p_vehicle));
}

void process_my_takeoff(struct Thing *p_vehicle)
{
    s32 alt, dt_y, new_y, dt_alt;
    s32 angl_diff;
    ushort angl;

    DIFFTEST_SHADOW(process_my_takeoff, process_my_takeoff_asm(p_vehicle));
    alt = flying_alt((int)p_vehicle, 0, 0, 0);
    if ((alt >> 1) < p_vehicle->Y)
    {
        angl = (ushort)arctan(p_vehicle->U.UVehicle.GotoX - (p_vehicle->X >> 8),
          p_vehicle->U.UVehicle.GotoZ - (p_vehicle->Z >> 8));
        angl_diff = (s32)angl - p_vehicle->U.UVehicle.AngleY;
        if (angl_diff > 0x400)
            angl_diff -= 0x7ff;
        if (angl_diff < -0x400)
            angl_diff += 0x7ff;
        if (abs(angl_diff) < 15)
        {
            p_vehicle->U.UVehicle.AngleDY = 0;
            p_vehicle->U.UVehicle.AngleY = angl;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleDY = angl_diff;
            if (angl_diff > 0)
                p_vehicle->U.UVehicle.AngleDY = angl_diff + 10;
            if (angl_diff < 0)
                p_vehicle->U.UVehicle.AngleDY -= 10;
        }
    }
    dt_y = (alt - p_vehicle->Y) >> 3;
    if (dt_y < 1000)
        dt_y = 1000;
    if (dt_y > 10000)
        dt_y = 10000;
    // TNode is used here as vertical velocity
    if (p_vehicle->U.UVehicle.TNode < dt_y)
        p_vehicle->U.UVehicle.TNode += 1000;
    if (p_vehicle->U.UVehicle.TNode > dt_y)
        p_vehicle->U.UVehicle.TNode -= 1000;
    new_y = p_vehicle->Y + p_vehicle->U.UVehicle.TNode;
    dt_alt = alt - new_y;
    p_vehicle->Y = new_y;
    if (dt_alt < 4000)
    {
        if ((ushort)p_vehicle->State == VehSt_UNKN_44)
            p_vehicle->State = VehSt_UNKN_42;
        else
            p_vehicle->State = VehSt_UNKN_3A;
        p_vehicle->U.UVehicle.AngleDY = 0;
        return;
    }
    if (alt - (alt >> 4) < new_y && p_vehicle->U.UVehicle.AngleDY < 15)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 1000;
        return;
    }
    p_vehicle->U.UVehicle.ReqdSpeed = 0;
}

static DIFFTEST_REF_FUNC void process_my_land_vehicle_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_land_vehicle\n"
        : : "a" (p_vehicle));
}

void process_my_land_vehicle(struct Thing *p_vehicle)
{
    s32 ground_y, dt_y, new_y, angl;

    DIFFTEST_SHADOW(process_my_land_vehicle, process_my_land_vehicle_asm(p_vehicle));
    p_vehicle->U.UVehicle.ReqdSpeed = 0;
    ground_y = alt_at_point(p_vehicle->X >> 8, p_vehicle->Z >> 8) * 8 + 0x5dc0;
    dt_y = (ground_y - p_vehicle->Y) >> 3;
    if (dt_y > -500)
        dt_y = -500;
    if (dt_y < -10000)
        dt_y = -10000;
    // TNode is used here as vertical velocity
    if (p_vehicle->U.UVehicle.TNode < dt_y)
        p_vehicle->U.UVehicle.TNode += 1500;
    if (p_vehicle->U.UVehicle.TNode > dt_y)
        p_vehicle->U.UVehicle.TNode -= 1500;
    new_y = p_vehicle->Y + p_vehicle->U.UVehicle.TNode;
    angl = p_vehicle->U.UVehicle.AngleDY;
    p_vehicle->U.UVehicle.AngleDY = angl - (angl >> 3);
    p_vehicle->Y = new_y;
    angl = p_vehicle->U.UVehicle.AngleDY;
    if (abs(angl) < 8)
        p_vehicle->U.UVehicle.AngleDY = 0;
    if (ground_y > p_vehicle->Y)
    {
        p_vehicle->State = VehSt_PARKED_PARAL;
        p_vehicle->U.UVehicle.MaxSpeed = 0;
    }
}

static DIFFTEST_REF_FUNC void set_vehicle_alt_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_set_vehicle_alt\n"
        : : "a" (p_vehicle));
}

void set_vehicle_alt(struct Thing *p_vehicle)
{
    struct MyMapElement *p_mapel;
    s32 alt, dt_alt;

    DIFFTEST_SHADOW(set_vehicle_alt, set_vehicle_alt_asm(p_vehicle));
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return;

    p_mapel = &game_my_big_map[128 * (p_vehicle->Z >> 16) + (p_vehicle->X >> 16)];
    if ((p_mapel->Flags2 & MEF2_Unkn08) != 0)
        return;
    if ((p_mapel->ColumnHead & 0xFFF) != 0)
        return;

    alt = alt_at_point(p_vehicle->X >> 8, p_vehicle->Z >> 8);
    dt_alt = (alt + 0xF00 - p_vehicle->Y) >> 3;
    p_vehicle->Y += dt_alt;
    p_vehicle->U.UVehicle.AngleX = (-dt_alt) >> 3;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int check_for_a_moving_vehicle_here_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_check_for_a_moving_vehicle_here\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Check if a ground vehicle other than given one is at given point.
 * Coordinates are in map units (tile size 256); args are raw registers.
 */
int check_for_a_moving_vehicle_here(int a1, int a2, int a3, int a4)
{
    struct Thing *p_self = (struct Thing *)a3;
    s32 x = a1, z = a2;
    s32 tile_x, tile_z, tx, tz;
    ThingIdx thing;

    DIFFTEST_SHADOW_RET(check_for_a_moving_vehicle_here, int,
      check_for_a_moving_vehicle_here_asm(a1, a2, a3, a4));
    tile_x = x >> 8;
    tile_z = z >> 8;
    for (tx = tile_x - 2; tx <= tile_x + 2; tx++)
    {
        for (tz = tile_z - 2; tz <= tile_z + 2; tz++)
        {
            if (tx < 0 || tx > 127 || tz < 0 || tz > 127)
                continue;
            thing = game_my_big_map[128 * tz + tx].Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                struct M33 *p_mat;
                s32 radius, mat02, mat22, off_x, off_z;
                s32 pos_x, pos_z, dist_x, dist_z, dist;

                if (thing <= 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                thing = p_thing->Next;
                if (p_thing->Type != TT_VEHICLE)
                    continue;
                if (p_thing == p_self)
                    continue;
                if (p_thing->SubType != SubTT_VEH_GROUND)
                    continue;

                radius = (s32)((u32)p_thing->Radius * 400) >> 9;
                p_mat = &local_mats[p_thing->U.UVehicle.MatrixIndex];
                mat02 = -4 * p_mat->R[0][2];
                mat22 = -4 * p_mat->R[2][2];
                off_x = (mat02 * radius) >> 8;
                off_z = (mat22 * radius) >> 8;

                // Front point
                pos_x = (p_thing->X + off_x) >> 8;
                pos_z = (p_thing->Z + off_z) >> 8;
                dist_x = abs(x - pos_x);
                dist_z = abs(z - pos_z);
                if (dist_x > dist_z)
                    dist = dist_x + (dist_z >> 1);
                else
                    dist = dist_z + (dist_x >> 1);
                if (dist < radius)
                    DIFFTEST_RETURN((int)1);

                // Rear point
                pos_x = (p_thing->X - off_x) >> 8;
                pos_z = (p_thing->Z - off_z) >> 8;
                dist_x = abs(x - pos_x);
                dist_z = abs(z - pos_z);
                if (dist_x > dist_z)
                    dist = dist_x + (dist_z >> 1);
                else
                    dist = dist_z + (dist_x >> 1);
                if (dist < radius)
                    DIFFTEST_RETURN((int)1);
            }
        }
    }
    DIFFTEST_RETURN((int)0);
}


static DIFFTEST_REF_FUNC void process_unstopping_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unstopping\n"
        : : "a" (p_vehicle));
}

void process_unstopping(struct Thing *p_vehicle)
{
    s32 accel_x, accel_z, step;
    s32 base_x, base_z;

    DIFFTEST_SHADOW(process_unstopping, process_unstopping_asm(p_vehicle));
    accel_x = p_vehicle->U.UVehicle.AccelX;
    accel_z = p_vehicle->U.UVehicle.AccelZ;
    step = p_vehicle->U.UVehicle.Dummy4a & 0x3f;
    if (step == 10)
    {
        base_x = (p_vehicle->X >> 8) - accel_x * 8;
        base_z = (p_vehicle->Z >> 8) - accel_z * 8;
        if (check_for_a_moving_vehicle_here(base_x + accel_z * 2,
          base_z - accel_x * 2, (int)p_vehicle, (int)p_vehicle) != 0)
        {
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.WorkPlace |= 0x0200;
            return;
        }
        if (check_for_a_moving_vehicle_here(base_x - accel_z * 2,
          base_z + accel_x * 2, (int)p_vehicle, (int)p_vehicle) != 0)
        {
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            p_vehicle->U.UVehicle.WorkPlace |= 0x0200;
            return;
        }
    }

    p_vehicle->U.UVehicle.WorkPlace &= ~0x0200;
    if (step > 5)
    {
        p_vehicle->U.UVehicle.AngleZ = (11 - step) * -100;
        accel_x += accel_x >> 1;
        accel_z += accel_z >> 1;
    }
    else
    {
        p_vehicle->U.UVehicle.AngleZ = step * -100;
        accel_x -= accel_x >> 2;
        accel_z -= accel_z >> 2;
    }
    p_vehicle->U.UVehicle.AccelX = accel_x;
    p_vehicle->U.UVehicle.AccelZ = accel_z;
    p_vehicle->U.UVehicle.ReqdSpeed += 100;
    if (step > 0)
        step--;
    p_vehicle->U.UVehicle.Dummy4a = (p_vehicle->U.UVehicle.Dummy4a & ~0x3f) | step;
    if (step == 0)
    {
        p_vehicle->State = 50;
        start_goto_vehicle(p_vehicle, p_vehicle->U.UVehicle.Dummy5b,
          p_vehicle->U.UVehicle.Dummy5a);
    }
}

static DIFFTEST_REF_FUNC void process_stopping_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stopping\n"
        : : "a" (p_vehicle));
}

void process_stopping(struct Thing *p_vehicle)
{
    s32 step, accel_x, accel_z, dt_angle, angle_dy;

    DIFFTEST_SHADOW(process_stopping, process_stopping_asm(p_vehicle));
    step = (ushort)p_vehicle->U.UVehicle.Dummy5a;
    accel_x = p_vehicle->U.UVehicle.AccelX;
    accel_z = p_vehicle->U.UVehicle.AccelZ;
    if (step > 10)
    {
        p_vehicle->U.UVehicle.AngleZ = (17 - step) * 80;
        accel_x += accel_x >> 1;
        accel_z += accel_z >> 1;
    }
    else
    {
        p_vehicle->U.UVehicle.AngleZ = step * 40;
        accel_x -= accel_x >> 2;
        accel_z -= accel_z >> 2;
    }
    p_vehicle->U.UVehicle.AccelZ = accel_z;
    p_vehicle->U.UVehicle.AccelX = accel_x;

    dt_angle = p_vehicle->U.UVehicle.AngleY - p_vehicle->U.UVehicle.Dummy5b;
    if (dt_angle < -0x400)
        dt_angle += 0x800;
    else if (dt_angle > 0x400)
        dt_angle -= 0x800;

    if (dt_angle < 0)
    {
        if (dt_angle > -50)
            p_vehicle->U.UVehicle.AngleDY = -dt_angle * 2;
        else
            p_vehicle->U.UVehicle.AngleDY = (0x800 - (p_vehicle->Speed >> 3)) >> 2;
    }
    else if (dt_angle > 0)
    {
        if (dt_angle < 50)
            p_vehicle->U.UVehicle.AngleDY = -(dt_angle * 2);
        else
            p_vehicle->U.UVehicle.AngleDY = -((0x800 - (p_vehicle->Speed >> 3)) >> 2);
    }
    p_vehicle->U.UVehicle.ReqdSpeed = 0;

    if (step > 0)
        step--;
    p_vehicle->U.UVehicle.Dummy5a = step;

    if (p_vehicle->Speed != 0 || step != 0)
        return;
    angle_dy = p_vehicle->U.UVehicle.AngleDY;
    if (abs(angle_dy) >= 20)
        return;

    if ((p_vehicle->U.UVehicle.TNode < 0) &&
      (game_traffic_nodes[-p_vehicle->U.UVehicle.TNode].Flags & TNdF_Unkn0800) != 0)
        p_vehicle->State = VehSt_PARKED_PERPN;
    else
        p_vehicle->State = VehSt_PARKED_PARAL;
    p_vehicle->Flag |= TngF_Unkn0004;
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0100;
    remove_locks(p_vehicle);
}

static DIFFTEST_REF_FUNC void process_vehicle_goto_point_land_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_land\n"
        : : "a" (p_vehicle));
}

void process_vehicle_goto_point_land(struct Thing *p_vehicle)
{
    s32 alt;
    short angle_x;

    DIFFTEST_SHADOW(process_vehicle_goto_point_land, process_vehicle_goto_point_land_asm(p_vehicle));
    set_passengers_location(p_vehicle);
    alt = alt_at_point((short)(p_vehicle->X >> 8), (short)(p_vehicle->Z >> 8)) << 3;
    if (alt < p_vehicle->Y)
    {
        p_vehicle->Speed = 0;
        p_vehicle->Y -= 0x800;
    }
    else
    {
        p_vehicle->State = VehSt_PARKED_PARAL;
    }
    angle_x = p_vehicle->U.UVehicle.AngleX;
    if (angle_x > 20)
        p_vehicle->U.UVehicle.AngleX -= 20;
    else if (angle_x < -20)
        p_vehicle->U.UVehicle.AngleX += 20;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void scan_max_height_ahead(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_scan_max_height_ahead\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


static DIFFTEST_REF_FUNC void process_vehicle_goto_point_fly_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_fly\n"
        : : "a" (p_vehicle));
}

void process_vehicle_goto_point_fly(struct Thing *p_vehicle)
{
    s32 dx, dz, dist_sq;
    short angle;

    DIFFTEST_SHADOW(process_vehicle_goto_point_fly, process_vehicle_goto_point_fly_asm(p_vehicle));
    scan_max_height_ahead((int)p_vehicle, 0, 0, 0);
    set_passengers_location(p_vehicle);
    dx = p_vehicle->U.UVehicle.GotoX - (p_vehicle->X >> 8);
    dz = p_vehicle->U.UVehicle.GotoZ - (p_vehicle->Z >> 8);
    dist_sq = dx * dx + dz * dz;
    if (dist_sq < 0x8000)
        p_vehicle->SubState = 7;
    angle = p_vehicle->U.UVehicle.AngleY - arctan(dx, dz);
    if (angle < -0x400)
        angle += 0x800;
    else if (angle > 0x400)
        angle -= 0x800;

    if (dist_sq > 0x400000)
        p_vehicle->U.UVehicle.ReqdSpeed = 0x5e8;
    else if (dist_sq > 0x100000)
        p_vehicle->U.UVehicle.ReqdSpeed = 0x400;
    else
        p_vehicle->U.UVehicle.ReqdSpeed = 0x200;

    if (angle < 0)
    {
        if (angle > -50)
            p_vehicle->U.UVehicle.AngleDY = -angle * 2;
        else
            p_vehicle->U.UVehicle.AngleDY = 0x200;
    }
    else if (angle > 0)
    {
        if (angle < 50)
        {
            p_vehicle->U.UVehicle.AngleDY = angle * 2;
            p_vehicle->U.UVehicle.AngleDY = -angle * 2;
        }
        else
        {
            p_vehicle->U.UVehicle.AngleDY = -0x200;
        }
    }
}

static DIFFTEST_REF_FUNC void process_parked_flyer_asm(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_parked_flyer\n"
        : : "a" (p_vehicle));
}

void process_parked_flyer(struct Thing *p_vehicle)
{
    ushort qv;

    DIFFTEST_SHADOW(process_parked_flyer, process_parked_flyer_asm(p_vehicle));
    // MaxSpeed is reused as the index into the quiver table
    qv = p_vehicle->U.UVehicle.MaxSpeed;
    if (qv == 0)
    {
        s32 alt;
        alt = alt_at_point(p_vehicle->X >> 8, p_vehicle->Z >> 8);
        p_vehicle->U.UVehicle.MaxSpeed = 1;
        p_vehicle->Y = alt + 3000;
        return;
    }
    p_vehicle->U.UVehicle.MaxSpeed = qv + 1;
    p_vehicle->Y += quiver[qv] << 4;
    if (quiver[p_vehicle->U.UVehicle.MaxSpeed] == 99)
        p_vehicle->U.UVehicle.MaxSpeed -= 26;
}

void init_mech_explode(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_mech_explode\n"
        : : "a" (p_vehicle));
}

void process_train(struct Thing *p_vehicle)
{
    int dtvel;

    dtvel = p_vehicle->U.UVehicle.ReqdSpeed - p_vehicle->Speed;
    if (abs(dtvel) >= 4) {
        p_vehicle->Speed += dtvel >> 1;
    } else {
        p_vehicle->Speed = p_vehicle->U.UVehicle.ReqdSpeed;
    }

    if (p_vehicle->State == VehSt_UNKN_45)
    {
          p_vehicle->Y <<= 3;
          if (process_my_crashing_vehicle(p_vehicle))
          {
              move_flying_vehicle(p_vehicle);
              p_vehicle->Y >>= 3;
              set_passengers_location(p_vehicle);
          }
          return;
    }

    if (p_vehicle->U.UVehicle.TNode != 0)
    {
        struct Thing *p_station;

        assert(p_vehicle->SubType == SubTT_VEH_TRAIN); // only trains have stations
        assert(p_vehicle->U.UVehicle.TNode > 0); // trains stations have positive indexes
        p_station = &things[p_vehicle->U.UVehicle.TNode];
        if ((p_station->Type != TT_BUILDING) || (p_station->Flag & TngF_Destroyed) != 0)
        {
            p_vehicle->Flag |= TngF_Destroyed;
            start_crashing(p_vehicle);
            p_vehicle->U.UVehicle.ReqdSpeed = 0;
            return;
        }
    }

    switch (p_vehicle->State)
    {
    case VehSt_UNKN_12:
        if (p_vehicle->U.UVehicle.TNode != 0)
        {
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
                train_unkn_st18_func_1(p_vehicle);
            else
                train_unkn_st18_func_2(p_vehicle);
        }
        set_passengers_location(p_vehicle);
        move_vehicle(p_vehicle);
        break;
    case VehSt_UNKN_13:
        if (!train_unkn_st21_exit_func_1(p_vehicle))
        {
            p_vehicle->State = VehSt_UNKN_14;
            p_vehicle->U.UVehicle.Timer2 = 100;
            play_dist_sample(p_vehicle, 225 + (LbRandomAnyShort() % 4), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
        }
        break;
    case VehSt_UNKN_14:
        train_unkn_st20_func_1(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_15:
        train_unkn_st21_exit_func_1(p_vehicle);
        if (p_vehicle->U.UVehicle.GotoX != 0 && things[p_vehicle->U.UVehicle.GotoX].State == VehSt_UNKN_12) {
            p_vehicle->State = VehSt_UNKN_12;
        }
        else if (p_vehicle->Owner != 0 && things[p_vehicle->Owner].State == VehSt_UNKN_12) {
            p_vehicle->State = VehSt_UNKN_12;
        }
        set_passengers_location(p_vehicle);
        break;
    default:
        LOGERR("Shagged %s %d state %d",
          vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
          (int)p_vehicle->State);
        break;
    }
}

void process_vehicle_goto_point_liftoff(struct Thing *p_vehicle)
{
    int i;

    set_passengers_location(p_vehicle);
    i = alt_at_point(PRCCOORD_TO_MAPCOORD(p_vehicle->X), PRCCOORD_TO_MAPCOORD(p_vehicle->Z));
    if (p_vehicle->Y < MAPCOORD_TO_PRCCOORD(400, 0) + (i << 3)) {
        p_vehicle->Speed = 0;
        p_vehicle->Y += MAPCOORD_TO_PRCCOORD(8, 0);
    } else {
        p_vehicle->SubState = 6;
    }
}

void process_veh_ground(struct Thing *p_vehicle)
{
    int dtvel;

    dtvel = p_vehicle->U.UVehicle.ReqdSpeed - p_vehicle->Speed;
    if (dtvel != 0)
    {
        if (dtvel > 100)
            p_vehicle->Speed += 100;
        else if (dtvel < -400)
            p_vehicle->Speed -= 400;
        else
            p_vehicle->Speed = p_vehicle->U.UVehicle.ReqdSpeed;
    }

    switch (p_vehicle->State)
    {
    case VehSt_UNKN_3A:
    case VehSt_UNKN_42:
    case VehSt_UNKN_43:
        p_vehicle->Y <<= 3;
        process_my_flying_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_3E:
        process_unplacing(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_41:
        p_vehicle->U.UVehicle.ReqdSpeed = 0;
        p_vehicle->U.UVehicle.AngleDY = 0;
        p_vehicle->Y <<= 3;
        process_hovering_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_45:
        p_vehicle->Y <<= 3;
        if (process_my_crashing_vehicle(p_vehicle)) {
            move_flying_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
        }
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_39:
    case VehSt_UNKN_44:
        p_vehicle->Y <<= 3;
        process_my_takeoff(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_FLY_LANDING:
        p_vehicle->Y <<= 3;
        process_my_land_vehicle(p_vehicle);
        move_flying_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        p_vehicle->Y >>= 3;
        break;
    case VehSt_UNKN_3C:
        // Pretend we are wandering
        p_vehicle->State = VehSt_WANDER;
        if (p_vehicle->U.UVehicle.TNode != 0)
              process_next_tnode(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        // Get back to specifics of this state
        p_vehicle->State = VehSt_UNKN_3C;
        process_stop_as_soon_as_you_can(p_vehicle);
        break;
    case VehSt_NONE:
    case VehSt_PARKED_PERPN:
    case VehSt_PARKED_PARAL:
        if (p_vehicle->SubType == SubTT_VEH_FLYING)
            process_parked_flyer(p_vehicle);
        break;
    case VehSt_UNKN_27:
        switch (p_vehicle->SubState)
        {
        case 5:
            process_vehicle_goto_point_liftoff(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        case 6:
            process_vehicle_goto_point_fly(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        case 7:
            process_vehicle_goto_point_land(p_vehicle);
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        default:
            move_vehicle(p_vehicle);
            set_passengers_location(p_vehicle);
            break;
        }
        break;
    case VehSt_UNKN_36:
        process_stopping(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_UNKN_38:
        process_unstopping(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case VehSt_WANDER:
    case VehSt_GOTO_LOC:
    case VehSt_UNKN_33:
    case VehSt_UNKN_34:
        if (p_vehicle->U.UVehicle.TNode != 0)
            process_next_tnode(p_vehicle);
        move_vehicle(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    default:
        LOGERR("Unexpected %s %d state %d",
          vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
          (int)p_vehicle->State);
        break;
    }

    if ((p_vehicle->SubType != SubTT_VEH_FLYING)
     && ((p_vehicle->Flag & TngF_Destroyed) == 0)) {
        set_vehicle_alt(p_vehicle);
    }
}

void vehicle_check_outside_map(struct Thing *p_vehicle)
{
    if ((p_vehicle->X <= MAPCOORD_TO_PRCCOORD(0,0))
      || (p_vehicle->X >= MAPCOORD_TO_PRCCOORD(MAP_COORD_WIDTH,0))
      || (p_vehicle->Z <= MAPCOORD_TO_PRCCOORD(0,0))
      || (p_vehicle->Z >= MAPCOORD_TO_PRCCOORD(MAP_COORD_HEIGHT,0)))
    {
        if (p_vehicle->State != VehSt_UNKN_45)
            start_crashing(p_vehicle);
    }
}

void process_vehicle(struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_process_vehicle\n"
        : : "a" (p_vehicle));
    return;
#endif
    if ((p_vehicle->Flag & TngF_Destroyed) == 0)
        p_vehicle->OldTarget = 0;
    if (p_vehicle->U.UVehicle.RecoilTimer > 0)
        p_vehicle->U.UVehicle.RecoilTimer--;
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0080) != 0)
    {
        p_vehicle->Health -= 16;
        if (p_vehicle->Health < 0)
            start_crashing(p_vehicle);
        if (((gameturn & 7) == 0) && (LbRandomAnyShort() & 7) == 0)
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0080;
    }

    switch (p_vehicle->SubType)
    {
    case SubTT_VEH_TRAIN:
        if (p_vehicle->State == VehSt_UNKN_D)
            break;
        process_train(p_vehicle);
        break;
    case SubTT_VEH_SHUTTLE_POD:
        process_shuttle_pod(p_vehicle);
        break;
    case SubTT_VEH_GROUND:
    case SubTT_VEH_UNKN43:
        process_veh_ground(p_vehicle);
        break;
    case SubTT_VEH_FLYING:
        vehicle_check_outside_map(p_vehicle);
        process_veh_ground(p_vehicle);
        break;
    case SubTT_VEH_TANK:
        if (p_vehicle->State == VehSt_UNKN_45) {
            bang_new4(p_vehicle->X, p_vehicle->Y, p_vehicle->Z, 10);
            init_vehicle_explode(p_vehicle);
        } else if (p_vehicle->State == VehSt_PARKED_PARAL) {
            process_tank_stationary(p_vehicle);
        } else {
            process_tank(p_vehicle);
            set_passengers_location(p_vehicle);
        }
        process_tank_turret(p_vehicle);
        break;
    case SubTT_VEH_SHIP:
        process_ship(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    case SubTT_VEH_MECH:
        unkn_path_func_001(p_vehicle, 0);
        if (p_vehicle->State == VehSt_PARKED_PARAL)
            process_mech_stationary(p_vehicle);
        else
            process_mech(p_vehicle);
        process_mech_unknown1(p_vehicle);
        set_passengers_location(p_vehicle);
        break;
    default:
        break;
    }
}

void preprogress_trains_turns(ulong nturns)
{
    ulong turns;

    for (turns = nturns; turns > 0; turns--)
    {
        ThingIdx thing;
        int tng_remain;
        struct Thing *p_thing;

        thing = things_used_head;
        tng_remain = things_used;
        while (thing > 0)
        {
            if (tng_remain == 0)
                break;
            p_thing = &things[thing];
            thing = p_thing->LinkChild;
            tng_remain--;

            if ((p_thing->Type == TT_VEHICLE)
              && (p_thing->SubType == SubTT_VEH_TRAIN))
                process_vehicle(p_thing);
        }
    }
}

int vehicle_hit_by_bullet(struct Thing *p_vehicle, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    int health_decr;

    if ((p_vehicle->Flag & TngF_Destroyed) != 0) {
        return 0;
    }

    p_vehicle->Flag |= TngF_Unkn01000000;
    if (p_vehicle->SubType == SubTT_VEH_TANK)
        hp = hp >> 1;
    if (p_vehicle->SubType == SubTT_VEH_MECH)
        hp >>= 2;

    play_dist_sample(p_vehicle, 65, FULL_VOL, EQUL_PAN, NORM_PTCH, 0, 1);

    if (type == DMG_UZI || type == DMG_MINIGUN)
        hp >>= 1;
    health_decr = hp >> 1;

    p_vehicle->Health -= health_decr;
    p_vehicle->U.UVehicle.RecoilTimer = 5;
    if (p_vehicle->Health > 0) {
        return health_decr;
    }

    p_vehicle->OldTarget = p_attacker->ThingOffset;
    if (p_vehicle->SubType == SubTT_VEH_MECH)
    {
        p_vehicle->Flag |= TngF_Destroyed;
        init_mech_explode(p_vehicle);
    }
    else
    {
        p_vehicle->Flag |= TngF_Destroyed;
        start_crashing(p_vehicle);
        play_dist_sample(p_vehicle, 95, FULL_VOL, EQUL_PAN, NORM_PTCH, 0, 1);
    }
    return p_vehicle->Health + health_decr;
}

/******************************************************************************/
