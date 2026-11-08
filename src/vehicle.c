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
#include "ssampply.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "bigmap.h"
#include "bmbang.h"
#include "difftest.h"
#include "engincam.h"
#include "enginpriobjs.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "engintrns.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "matrix.h"
#include "pathtrig.h"
#include "sound.h"
#include "thing.h"
#include "thing_fire.h"
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
void VNAV_find_nearnodes(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_find_nearnodes\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void VNAV_unkn_func_207(struct Thing *p_thing)
{
    asm volatile ("call ASM_VNAV_unkn_func_207\n"
        : : "a" (p_thing));
}

int check_for_a_vehicle_here(int x, int z, struct Thing *p_vehicle)
{
    int ret;
    asm volatile (
      "call ASM_check_for_a_vehicle_here\n"
        : "=r" (ret) : "a" (x), "d" (z), "b" (p_vehicle));
    return ret;
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


void person_hit_by_car(struct Thing *p_person, struct Thing *p_vehicle)
{
    asm volatile ("call ASM_person_hit_by_car\n"
        : : "a" (p_person), "d" (p_vehicle));
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

void start_crashing(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_start_crashing\n"
        : : "a" (p_vehicle));
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
int exit_pod(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_exit_pod\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
    return ra;
}


void process_shuttle_pod(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_shuttle_pod\n"
        : : "a" (p_vehicle));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void explode_thing_object(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_explode_thing_object\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void init_vehicle_explode(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_init_vehicle_explode\n"
        : : "a" (p_vehicle));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int check_tank_collision(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_check_tank_collision\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


void process_tank_stationary(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank_stationary\n"
        : : "a" (p_vehicle));
}

void process_tank(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_tank\n"
        : : "a" (p_vehicle));
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


void move_flying_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_flying_vehicle\n"
        : : "a" (p_vehicle));
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
int find_next_ship_tnode(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_next_ship_tnode\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


void process_ship(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_ship\n"
        : : "a" (p_vehicle));
}

void process_mech_stationary(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_stationary\n"
        : : "a" (p_vehicle));
}

void process_mech(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech\n"
        : : "a" (p_vehicle));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void matrix_transpose_cp(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_matrix_transpose_cp\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "ecx", "esi", "edi", "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int mech_unkn_func_08(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_mech_unkn_func_08\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}



void process_mech_unknown1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_mech_unknown1\n"
        : : "a" (p_vehicle));
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
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void set_xz_for_unkn3(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_xz_for_unkn3\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


int train_unkn_st21_exit_func_1(struct Thing *p_vehicle)
{
    int ret;
    asm volatile ("call ASM_train_unkn_st21_exit_func_1\n"
        : "=r" (ret) : "a" (p_vehicle));
    return ret;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void thing_unkn_change_speed(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_thing_unkn_change_speed\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void train_unkn_st18_func_1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_1\n"
        : : "a" (p_vehicle));
}

void train_unkn_st18_func_2(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st18_func_2\n"
        : : "a" (p_vehicle));
}

void move_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_move_vehicle\n"
        : : "a" (p_vehicle));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void person_clear_route_and_teleport_to_vehicle(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_clear_route_and_teleport_to_vehicle\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int train_unkn_func_02(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_train_unkn_func_02\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}



void train_unkn_st20_func_1(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_train_unkn_st20_func_1\n"
        : : "a" (p_vehicle));
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



void process_my_flying_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_flying_vehicle\n"
        : : "a" (p_vehicle));
}

void process_unplacing(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unplacing\n"
        : : "a" (p_vehicle));
}

void process_stop_as_soon_as_you_can(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stop_as_soon_as_you_can\n"
        : : "a" (p_vehicle));
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


void process_hovering_vehicle(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_hovering_vehicle\n"
        : : "a" (p_vehicle));
}

void process_my_takeoff(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_my_takeoff\n"
        : : "a" (p_vehicle));
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
int check_for_a_moving_vehicle_here(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_check_for_a_moving_vehicle_here\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


void process_unstopping(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_unstopping\n"
        : : "a" (p_vehicle));
}

void process_stopping(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_stopping\n"
        : : "a" (p_vehicle));
}

void process_vehicle_goto_point_land(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_land\n"
        : : "a" (p_vehicle));
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


void process_vehicle_goto_point_fly(struct Thing *p_vehicle)
{
    asm volatile ("call ASM_process_vehicle_goto_point_fly\n"
        : : "a" (p_vehicle));
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
