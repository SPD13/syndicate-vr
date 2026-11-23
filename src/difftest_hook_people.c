/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file difftest_hook_people.c
 *     Difftest hooks reaching people and thing processing.
 * @par Purpose:
 *     Calls functions no mission reaches by itself, so the difftest can
 *     compare their C remake against the asm. Active only in scenarios
 *     whose suffix names the hook (see difftest_suffix_has()).
 * @par Comment:
 *     Called every game turn from difftest_bot_turn().
 *     Tokens:
 *     - "p_people": tasered persons, support persons, catch-train style
 *       path to a face, group_not_seen() and find_nearest_player();
 *     - "p_net": the network game cleanups of process_things() (bang,
 *       mines explode, random items), with dropped items aged every turn
 *       as process_things() does in a network game;
 *     - "p_mech": wrecks the mech and places it again, as the level load
 *       does, so init_mech_explode() and mech_unkn_func_09() repeat.
 *     All choices depend on the game turn and the game state only.
 * @author   SyndicateVR decomp agent
 * @date     16 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "difftest.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "bffile.h"
#include "bfmath.h"
#include "bigmap.h"
#include "enginpriobjs.h"
#include "enginpritxtr.h"
#include "enginprops.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvobjctv.h"
#include "matrix.h"
#include "pepgroup.h"
#include "people.h"
#include "player.h"
#include "thing.h"
#include "vehicle.h"
#include "weapon.h"
/******************************************************************************/
/* Not in headers */
StateChRes person_init_support_person(struct Thing *p_person, short targetng);
void build_navigate_path_to_face(struct Thing *p_thing, short face);
int find_nearest_player(intptr_t a1, int a2, int a3, int a4);
int process_things_mines_explode(int affected_max);
int process_things_bang(int affected_max);
extern s32 unkn_mech_var11;
extern const char *primvehobj_fname;

#if DIFFTEST_HARNESS

/** Mech slots in unkn_mech_arr3 (MECH_OWNER_LIMIT in vehicle.c). */
#define HP_MECH_SLOTS 8
/** Range around an agent for the face to walk to, in map coords. */
#define HP_FACE_RANGE 2048

/** Alive person, not a player agent, on the map and in no special
 * (tasered, knocked out) mode.
 */
static TbBool hp_person_ok(struct Thing *p_thing)
{
    if (p_thing->Type != TT_PERSON)
        return false;
    if ((p_thing->Flag & (TngF_Destroyed|TngF_InVehicle|TngF_PlayerAgent|TngF_Persuaded)) != 0)
        return false;
    if ((p_thing->Flag2 & (TgF2_KnockedOut|TgF2_Unkn0020|TgF2_Unkn00100000)) != 0)
        return false;
    if ((p_thing->State == PerSt_DEAD) || (p_thing->State == PerSt_DIEING))
        return false;
    return true;
}

/** Person in a plain walking/standing state. */
static TbBool hp_person_idle(struct Thing *p_thing)
{
    switch (p_thing->State)
    {
    case PerSt_NONE:
    case PerSt_GOTO_POINT:
    case PerSt_WANDER:
    case PerSt_WAIT:
        return true;
    default:
        return false;
    }
}

/** Kinds of persons hp_pick_person() looks for. */
enum HpPick {
    HpP_ANY = 0,
    HpP_IDLE,
    HpP_NOCMDS,
};

static TbBool hp_pick_match(struct Thing *p_thing, int kind)
{
    if (!hp_person_ok(p_thing))
        return false;
    if ((kind == HpP_IDLE) && !hp_person_idle(p_thing))
        return false;
    if ((kind == HpP_NOCMDS) &&
      (!hp_person_idle(p_thing) || person_is_executing_commands(p_thing->ThingOffset)))
        return false;
    return true;
}

/** The n-th (modulo their count) person matching kind, in the order of
 * the used things list; 0 if none.
 */
static ThingIdx hp_pick_person(ulong n, int kind)
{
    ThingIdx thing;
    ulong count, k;

    count = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        if (hp_pick_match(&things[thing], kind))
            count++;
    }
    if (count == 0)
        return 0;
    n %= count;
    k = 0;
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        if (!hp_pick_match(&things[thing], kind))
            continue;
        if (k == n)
            return thing;
        k++;
    }
    return 0;
}

/** Player agent which the player could give a goto order now (as the bot
 * checks in bot_agent_usable()), standing or walking.
 */
static TbBool hp_agent_ok(struct Thing *p_agent)
{
    if (p_agent == NULL || p_agent->Type != TT_PERSON)
        return false;
    if ((p_agent->Flag & (TngF_Destroyed|TngF_InVehicle)) != 0)
        return false;
    if ((p_agent->Flag2 & (TgF2_KnockedOut|TgF2_Unkn0020|TgF2_Unkn00100000)) != 0)
        return false;
    if (person_is_executing_commands(p_agent->ThingOffset))
        return false;
    if ((p_agent->U.UPerson.ComCur >> 2) != local_player_no)
        return false;
    switch (p_agent->State)
    {
    case PerSt_NONE:
    case PerSt_GOTO_POINT:
    case PerSt_WAIT:
        return true;
    default:
        return false;
    }
}

/** Centre of a walkable face near the agent, in map coords; false if the
 * face is not walkable or out of range. Face index as in OnFace
 * (positive for triangles, negative for quads).
 */
static TbBool hp_face_centre(short face, struct Thing *p_agent, short *p_x, short *p_z)
{
    struct SingleObject *p_sobj;
    short pts[4];
    int i, npts;
    long x, z;

    if (face < 0) {
        struct SingleObjectFace4 *p_face4 = &game_object_faces4[-face];
        if ((p_face4->GFlags & FGFlg_Unkn04) == 0)
            return false;
        p_sobj = &game_objects[p_face4->Object];
        for (i = 0; i < 4; i++)
            pts[i] = p_face4->PointNo[i];
        npts = 4;
    } else {
        struct SingleObjectFace3 *p_face3 = &game_object_faces3[face];
        if ((p_face3->GFlags & FGFlg_Unkn04) == 0)
            return false;
        p_sobj = &game_objects[p_face3->Object];
        for (i = 0; i < 3; i++)
            pts[i] = p_face3->PointNo[i];
        npts = 3;
    }
    x = z = 0;
    for (i = 0; i < npts; i++)
    {
        struct SinglePoint *p_pt = &game_object_points[(ushort)pts[i]];
        x += p_pt->X;
        z += p_pt->Z;
    }
    x = p_sobj->MapX + x / npts;
    z = p_sobj->MapZ + z / npts;
    if (x < 256 || x > MAP_COORD_WIDTH - 256)
        return false;
    if (z < 256 || z > MAP_COORD_HEIGHT - 256)
        return false;
    if (abs(PRCCOORD_TO_MAPCOORD(p_agent->X) - x) +
      abs(PRCCOORD_TO_MAPCOORD(p_agent->Z) - z) > HP_FACE_RANGE)
        return false;
    *p_x = x;
    *p_z = z;
    return true;
}

/** The first walkable face near the agent, scanning from a start which
 * depends on the turn; 0 if none.
 */
static short hp_pick_face(struct Thing *p_agent, short *p_x, short *p_z)
{
    int nfaces, start, k;

    nfaces = (next_object_face3 - 1) + (next_object_face4 - 1);
    if (nfaces <= 0)
        return 0;
    start = (int)((gameturn * 7919UL) % (ulong)nfaces);
    for (k = 0; k < nfaces; k++)
    {
        int idx = (start + k) % nfaces;
        short face;

        if (idx < next_object_face3 - 1)
            face = 1 + idx;
        else
            face = -(1 + idx - (next_object_face3 - 1));
        if (hp_face_centre(face, p_agent, p_x, p_z))
            return face;
    }
    return 0;
}

/** Tasers a person: the game has the tasered state handling in
 * process_person() (Flag2 bit 0x100000, timer in RecoilTimer), but no
 * code sets it.
 */
static void hp_taser_turn(void)
{
    struct Thing *p_person;
    ThingIdx thing;

    if ((gameturn % 64) != 3)
        return;
    thing = hp_pick_person(gameturn / 64, HpP_IDLE);
    if (thing == 0)
        return;
    p_person = &things[thing];
    p_person->U.UPerson.OldAnimMode = p_person->U.UPerson.AnimMode;
    p_person->U.UPerson.RecoilTimer = 16 + (gameturn / 64) % 8;
    p_person->Flag2 |= TgF2_Unkn00100000;
}

/** Makes a person without script support another member of its group,
 * as the command PCmd_SUPPORT_PERSON (used by no mission) does.
 */
static void hp_support_turn(void)
{
    struct Thing *p_person;
    ThingIdx thing, other;
    int i;

    static ThingIdx last_support = 0;

    if ((gameturn % 64) != 35)
        return;
    // One supporter at a time; the previous one stops, as on a lost target
    if (last_support > 0)
    {
        p_person = &things[last_support];
        if ((p_person->Type == TT_PERSON) && (p_person->State == PerSt_SUPPORT_PERSON))
            p_person->State = PerSt_NONE;
        last_support = 0;
    }
    thing = hp_pick_person(gameturn / 64, HpP_NOCMDS);
    if (thing == 0)
        return;
    p_person = &things[thing];
    other = same_type_head[256 + p_person->U.UPerson.Group];
    for (i = 0; (other > 0) && (i < THINGS_LIMIT); i++)
    {
        struct Thing *p_other = &things[other];
        if ((other != thing) && (p_other->Type == TT_PERSON) &&
          ((p_other->Flag & TngF_Destroyed) == 0) && (p_other->State != PerSt_DEAD))
            break;
        other = p_other->LinkSameGroup;
    }
    if ((other <= 0) || (other == thing))
        return;
    person_init_support_person(p_person, other);
    last_support = thing;
}

/** Sends a player agent to a walkable face, as person_catch_train()
 * does with a station face, and as the goto-face packet does otherwise
 * (build_navigate_path_to_face() takes the face centre itself).
 */
static void hp_navface_turn(void)
{
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    short face, x, z;

    if ((gameturn % 32) != 9)
        return;
    p_locplayer = &players[local_player_no];
    p_agent = p_locplayer->MyAgent[(gameturn / 32) % playable_agents];
    if (!hp_agent_ok(p_agent))
        return;
    face = hp_pick_face(p_agent, &x, &z);
    if (face == 0)
        return;
    p_agent->Flag2 &= ~TgF2_Unkn00020000;
    p_agent->State = PerSt_GOTO_POINT;
    p_agent->U.UPerson.ComTimer = -1;
    p_agent->Timer1 = 0x30;
    p_agent->StartTimer1 = 0x30;
    p_agent->SubState = 0;
    p_agent->U.UPerson.ComRange = 1;
    p_agent->U.UPerson.GotoX = x;
    p_agent->U.UPerson.GotoZ = z;
    if (p_agent->U.UPerson.PathIndex != 0)
        remove_path(p_agent);
    p_agent->U.UPerson.PathIndex = 0;
    // The face argument is negative for triangles here, unlike OnFace
    build_navigate_path_to_face(p_agent, -face);
}

/** Queries as PCmd_UNTIL_G_NOT_SEEN (no mission uses it) does, for every
 * group with members.
 */
static void hp_seen_turn(void)
{
    ushort group;

    if ((gameturn % 32) != 13)
        return;
    for (group = 0; group < PEOPLE_GROUPS_LIMIT; group++)
    {
        if (same_type_head[256 + group] == 0)
            continue;
        group_not_seen(group);
    }
}

/** Nearest player agent to a few persons, as find_nearest_from_group()
 * asks in a network game.
 */
static void hp_near_turn(void)
{
    ThingIdx thing;
    int i;

    if ((gameturn % 32) != 17)
        return;
    for (i = 0; i < 2; i++)
    {
        thing = hp_pick_person(gameturn / 16 + i * 7, HpP_ANY);
        if (thing == 0)
            return;
        find_nearest_player((intptr_t)&things[thing], 0, 0, 0);
    }
}

/** In a network game process_things() counts the age of each dropped
 * item (not mines) in its ammo; process_things_mines_explode() blows up
 * the old ones. Outside a network game they stay at 0.
 */
static void hp_net_age_items(void)
{
    struct SimpleThing *p_sthing;
    ThingIdx thing;

    for (thing = sthings_used_head; thing < 0; thing = p_sthing->LinkChild)
    {
        p_sthing = &sthings[thing];
        if (p_sthing->Type != SmTT_DROPPED_ITEM)
            continue;
        if ((p_sthing->SubType == WEP_EXPLMINE) || (p_sthing->SubType == WEP_ELEMINE))
            continue;
        if (p_sthing->U.UWeapon.WeaponType == 31)
            continue;
        p_sthing->U.UWeapon.Ammo++;
    }
}

/** The cleanups process_things() runs in a network game only. */
static void hp_net_turn(void)
{
    int tech_lv;

    hp_net_age_items();

    if ((gameturn % 16) == 5)
    {
        if (things_used > 900)
            process_things_bang(16);
        else
            process_things_bang(1);
    }
    if ((gameturn % 32) == 21)
        process_things_mines_explode((gameturn / 32) % 32 + 2);
    if ((gameturn % 32) == 7)
    {
        // Tech level of the net game players: 1 to 8
        tech_lv = login_control[0].TechLevel;
        if ((tech_lv < 1) || (tech_lv > 8))
            tech_lv = (gameturn / 32) % 8 + 1;
        process_things_unkn_sub2(tech_lv, ((gameturn / 32) % 3) == 2);
    }
}

/* Mech wreck and replacement */

static TbBool mech_tried = false;
static struct Thing mech_snap;
static struct M33 mech_snap_mat;
static ThingIdx mech_thing = 0;
static GameTurn mech_turn = 0;
/** The primitive objects as the level load uses them, read again from
 * the file; after the load the game reuses their memory, so a vehicle
 * placed later would be copied from garbage.
 */
struct HpPrims {
    struct SinglePoint *points;
    struct SingleObjectFace3 *faces3;
    struct SingleObjectFace4 *faces4;
    struct SingleObject *objects;
    struct SingleFloorTexture *textures4;
    struct SingleTexture *face_textures;
    ushort n_points, n_faces3, n_faces4, n_objects, n_textures4, n_face_textures;
    ushort unknprop01;
};
static struct HpPrims mech_prims;
static TbBool mech_prims_ok = false;

static TbBool hp_prims_load(struct HpPrims *p_prims)
{
    TbFileHandle fh;
    long firstval;
    TbBool ok;

    fh = LbFileOpen(primvehobj_fname, Lb_FILE_MODE_READ_ONLY);
    if (fh == INVALID_FILE)
        return false;
    firstval = 0;
    LbFileRead(fh, &firstval, sizeof(long));
    ok = (firstval != 1);
    if (ok)
    {
        LbFileRead(fh, &p_prims->n_points, sizeof(ushort));
        LbFileRead(fh, &p_prims->n_faces3, sizeof(ushort));
        LbFileRead(fh, &p_prims->n_faces4, sizeof(ushort));
        LbFileRead(fh, &p_prims->n_objects, sizeof(ushort));
        LbFileRead(fh, &p_prims->n_textures4, sizeof(ushort));
        LbFileRead(fh, &p_prims->n_face_textures, sizeof(ushort));
        LbFileRead(fh, &p_prims->unknprop01, sizeof(ushort));
        p_prims->points = calloc(p_prims->n_points + 1, sizeof(struct SinglePoint));
        p_prims->faces3 = calloc(p_prims->n_faces3 + 1, sizeof(struct SingleObjectFace3));
        p_prims->faces4 = calloc(p_prims->n_faces4 + 1, sizeof(struct SingleObjectFace4));
        p_prims->objects = calloc(p_prims->n_objects + 1, sizeof(struct SingleObject));
        p_prims->textures4 = calloc(p_prims->n_textures4 + 1, sizeof(struct SingleFloorTexture));
        p_prims->face_textures = calloc(p_prims->n_face_textures + 1, sizeof(struct SingleTexture));
        ok = (p_prims->points != NULL) && (p_prims->faces3 != NULL) &&
          (p_prims->faces4 != NULL) && (p_prims->objects != NULL) &&
          (p_prims->textures4 != NULL) && (p_prims->face_textures != NULL);
    }
    if (ok)
    {
        LbFileRead(fh, p_prims->points, sizeof(struct SinglePoint) * p_prims->n_points);
        LbFileRead(fh, p_prims->faces3, sizeof(struct SingleObjectFace3) * p_prims->n_faces3);
        LbFileRead(fh, p_prims->faces4, sizeof(struct SingleObjectFace4) * p_prims->n_faces4);
        LbFileRead(fh, p_prims->objects, sizeof(struct SingleObject) * p_prims->n_objects);
        LbFileRead(fh, p_prims->textures4, sizeof(struct SingleFloorTexture) * p_prims->n_textures4);
        LbFileRead(fh, p_prims->face_textures, sizeof(struct SingleTexture) * p_prims->n_face_textures);
    }
    LbFileClose(fh);
    return ok;
}

#define HP_SWAP(type, a, b) { type tmp_ = (a); (a) = (b); (b) = tmp_; }

/** Exchanges the game's primitive tables with the given ones. */
static void hp_prims_swap(struct HpPrims *p_prims)
{
    HP_SWAP(struct SinglePoint *, prim_object_points, p_prims->points);
    HP_SWAP(struct SingleObjectFace3 *, prim_object_faces3, p_prims->faces3);
    HP_SWAP(struct SingleObjectFace4 *, prim_object_faces4, p_prims->faces4);
    HP_SWAP(struct SingleObject *, prim_objects, p_prims->objects);
    HP_SWAP(struct SingleFloorTexture *, prim4_textures, p_prims->textures4);
    HP_SWAP(struct SingleTexture *, prim_face_textures, p_prims->face_textures);
    HP_SWAP(ushort, next_prim_object_point, p_prims->n_points);
    HP_SWAP(ushort, next_prim_object_face3, p_prims->n_faces3);
    HP_SWAP(ushort, next_prim_object_face4, p_prims->n_faces4);
    HP_SWAP(ushort, next_prim_object, p_prims->n_objects);
    HP_SWAP(ushort, prim4_textures_count, p_prims->n_textures4);
    HP_SWAP(ushort, prim_face_textures_count, p_prims->n_face_textures);
    HP_SWAP(ushort, prim_unknprop01, p_prims->unknprop01);
}

static TbBool hp_mech_alive(void)
{
    struct Thing *p_mech;

    if (mech_thing <= 0)
        return false;
    p_mech = &things[mech_thing];
    return (p_mech->Type == TT_VEHICLE) && (p_mech->SubType == SubTT_VEH_MECH)
      && ((p_mech->Flag & TngF_Destroyed) == 0);
}

static TbBool hp_mech_no_agents(struct Thing *p_mech)
{
    ThingIdx passngr;
    int i;

    passngr = p_mech->U.UVehicle.PassengerHead;
    for (i = 0; (passngr > 0) && (i < 32); i++)
    {
        if ((things[passngr].Flag & TngF_PlayerAgent) != 0)
            return false;
        passngr = things[passngr].U.UPerson.LinkPassenger;
    }
    return true;
}

/** Whether the engine has room for another mech (vehicle, 12 parts). */
static TbBool hp_mech_room(void)
{
    if (unkn_mech_var11 >= HP_MECH_SLOTS)
        return false;
    if (next_local_mat + 16 >= LOCAL_MATS_COUNT)
        return false;
    if (things_used >= THINGS_LIMIT - 64)
        return false;
    if (next_object + 16 >= game_objects_limit)
        return false;
    if (next_object_point + 2000 >= game_object_points_limit)
        return false;
    if (next_object_face3 + 2000 >= game_object_faces3_limit)
        return false;
    if (next_object_face4 + 2000 >= game_object_faces4_limit)
        return false;
    if (next_normal + 2000 >= game_normals_limit)
        return false;
    return true;
}

/** Places the mech again from the copy taken on the first turn, with the
 * steps load_level_pc_handle() does for a level vehicle.
 */
static ThingIdx hp_mech_place(void)
{
    struct Thing loc_thing;
    struct Thing *p_thing;
    ThingIdx new_thing, thing;
    ushort snobj;
    int n, k, i;

    new_thing = get_new_thing();
    if (new_thing <= 0)
        return 0;
    // Primitives as at the level load, put back after the placement
    hp_prims_swap(&mech_prims);
    p_thing = &things[new_thing];
    memcpy(&loc_thing, p_thing, sizeof(struct Thing));
    memcpy(p_thing, &mech_snap, sizeof(struct Thing));
    p_thing->ThingOffset = new_thing;
    p_thing->PTarget = NULL;
    p_thing->LinkParent = loc_thing.LinkParent;
    p_thing->LinkChild = loc_thing.LinkChild;
    add_node_thing(new_thing);

    p_thing->U.UVehicle.PassengerHead = 0;
    p_thing->Flag2 &= TgF2_ExistsOffMap;
    {
        int matx;
        matx = next_local_mat++;
        memcpy(&local_mats[matx], &mech_snap_mat, sizeof(struct M33));
        p_thing->U.UVehicle.MatrixIndex = matx;
    }
    // Scale of the copied points, as load_prim_quad() sets it
    unkn2_pos_x = 64;
    unkn2_pos_y = 64;
    unkn2_pos_z = 64;
    n = next_normal;
    snobj = copy_prim_obj_to_game_object(PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_MAPCOORD(p_thing->Z),
      -prim_unknprop01 - p_thing->StartFrame, PRCCOORD_TO_MAPCOORD(p_thing->Y));
    k = next_normal;
    unkn_object_shift_03(snobj);
    unkn_object_shift_02(n, k, snobj);
    p_thing->U.UVehicle.Object = snobj;
    game_objects[snobj].ThingNo = new_thing;
    VNAV_unkn_func_207(p_thing);
    // AngleY is kept from the copy; the level load takes it from the
    // matrix in the file, which mech_unkn_func_09() reset to identity
    veh_add(p_thing, p_thing->StartFrame);

    // Vehicles list, as build_same_type_headers() makes it after the load;
    // the freed slot of the wrecked mech may still be on it
    thing = same_type_head[2];
    for (i = 0; (thing > 0) && (i < THINGS_LIMIT); i++)
    {
        if (thing == new_thing)
            break;
        thing = things[thing].LinkSame;
    }
    if (thing != new_thing) {
        p_thing->LinkSame = same_type_head[2];
        same_type_head[2] = new_thing;
    }

    mech_unkn_func_09(new_thing);
    hp_prims_swap(&mech_prims);
    return new_thing;
}

/** Every 16 turns, hits the mech hard (as the "+wreck" hook does) until
 * it explodes; 48 turns later places it again, while the engine has room.
 */
static void hp_mech_turn(void)
{
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    struct Thing *p_mech;

    if (!mech_tried)
    {
        ThingIdx thing;

        mech_tried = true;
        for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
        {
            p_mech = &things[thing];
            if ((p_mech->Type == TT_VEHICLE) && (p_mech->SubType == SubTT_VEH_MECH)
              && ((p_mech->Flag & TngF_Destroyed) == 0))
                break;
        }
        if (thing <= 0)
            return;
        mech_thing = thing;
        mech_turn = gameturn;
        memcpy(&mech_snap, p_mech, sizeof(struct Thing));
        memcpy(&mech_snap_mat, &local_mats[p_mech->U.UVehicle.MatrixIndex], sizeof(struct M33));
        mech_prims_ok = hp_prims_load(&mech_prims);
    }
    if ((gameturn % 16) != 11)
        return;
    if (hp_mech_alive())
    {
        p_mech = &things[mech_thing];
        if (gameturn - mech_turn < 64)
            return;
        if (!hp_mech_no_agents(p_mech))
            return;
        p_locplayer = &players[local_player_no];
        p_agent = p_locplayer->MyAgent[0];
        if (p_agent == NULL || p_agent->Type != TT_PERSON)
            return;
        thing_hit_by_bullet(p_mech, 30000, 0, 0, 0, p_agent, DMG_RAP);
        if (!hp_mech_alive())
            mech_turn = gameturn;
        return;
    }
    if (mech_thing == 0)
        return;
    if (mech_turn == 0)
        mech_turn = gameturn;
    if (gameturn - mech_turn < 48)
        return;
    if (!mech_prims_ok || !hp_mech_room())
        return;
    mech_thing = hp_mech_place();
    mech_turn = gameturn;
}

void difftest_hook_people_turn(void)
{
    static int on_people = -1, on_net = -1, on_mech = -1;

    if (on_people < 0) {
        on_people = difftest_suffix_has("p_people");
        on_net = difftest_suffix_has("p_net");
        on_mech = difftest_suffix_has("p_mech");
    }
    if (!on_people && !on_net && !on_mech)
        return;
    if (ingame.DisplayMode != DpM_ENGINEPLY)
        return;
    if (on_people)
    {
        hp_taser_turn();
        hp_support_turn();
        hp_navface_turn();
        hp_seen_turn();
        hp_near_turn();
    }
    if (on_net)
        hp_net_turn();
    if (on_mech)
        hp_mech_turn();
}

#else

void difftest_hook_people_turn(void)
{
}

#endif
/******************************************************************************/
