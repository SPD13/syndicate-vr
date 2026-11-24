/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vehtraffic.c
 *     Pathfinding triangulation support required by the 3D engine.
 * @par Purpose:
 *     Implement functions for handling pathfinding triangulation.
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
#include "vehtraffic.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "bfutility.h"
#include "bigmap.h"
#include "building.h"
#include "difftest.h"
#include "engincam.h"
#include "enginzoom.h"
#include "game.h"
#include "game_speed.h"
#include "thing.h"
#include "vehicle.h"
#include "swlog.h"
/******************************************************************************/
struct TrafficNode *game_traffic_nodes = NULL;
ushort next_traffic_node = 1;

extern uint dword_1C8470;
extern int dword_1C8474[32];
/** Second traffic node list, see vehicle.c */
extern s32 data_1c84f4[];

short tnode_find_used_link(struct TrafficNode *p_tnode)
{
    short i;
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] != 0)
            return i;
    }
    return -1;
}

void tnode_unlink_thing(short tnode, ThingIdx thing)
{
    struct TrafficNode *p_tnode;
    int i;

    p_tnode = &game_traffic_nodes[tnode];
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] == thing)
            p_tnode->UTraffic.Link[i] = 0;
    }
}

void tnode_all_unlink_thing(ThingIdx thing)
{
    short tnode;
    for (tnode = 1; tnode < next_traffic_node; tnode++) {
        tnode_unlink_thing(tnode, thing);
    }
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int good_node_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    DIFFTEST_ASM("call ASM_good_node\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "ecx", "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Checks whether the traffic node -node
 * (eax; positive values are things) leads anywhere. Returns 1 if so.
 */
int good_node(int node)
{
    struct TrafficNode *p_tnode;
    s32 i, last;
    s32 tnode;

    DIFFTEST_SHADOW_RET(good_node, int, good_node_asm(node, 0, 0));
    if (node > 0)
        DIFFTEST_RETURN((int)1);

    p_tnode = &game_traffic_nodes[-node];
    last = 0xfffff;
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] != 0)
        {
            if ((p_tnode->UTraffic.Flags[i] & 0x80) == 0)
                DIFFTEST_RETURN((int)1);
            last = i;
        }
    }
    if (last == 0xfffff)
        DIFFTEST_RETURN((int)0);

    tnode = p_tnode->UTraffic.Link[last];
    if (tnode > 0)
        DIFFTEST_RETURN((int)1);

    p_tnode = &game_traffic_nodes[-tnode];
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] != 0 &&
          (p_tnode->UTraffic.Flags[i] & 0x80) == 0)
            DIFFTEST_RETURN((int)1);
    }
    DIFFTEST_RETURN((int)0);
}


static DIFFTEST_REF_FUNC short get_next_tnode_asm(struct Thing *p_vehicle, struct TrafficNode *p_tnode)
{
    short ret;
    DIFFTEST_ASM(
      "call ASM_get_next_tnode\n"
        : "=a" (ret) : "a" (p_vehicle), "d" (p_tnode));
    return ret;
}

short get_next_tnode(struct Thing *p_vehicle, struct TrafficNode *p_tnode)
{
    struct TrafficNode *p_lntnode;
    s32 i, k, n, last;
    s32 lnk, lp;
    TbBool ok;

    DIFFTEST_SHADOW_RET(get_next_tnode, short, get_next_tnode_asm(p_vehicle, p_tnode));
    n = 0;
    last = 0;
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] != 0) {
            last = i;
            n++;
        }
    }
    if (n == 0)
        DIFFTEST_RETURN((short)0);

    if (n == 1)
    {
        p_vehicle->U.UVehicle.WorkPlace |= 0x04;
        p_vehicle->U.UVehicle.LeisurePlace =
          (short)p_vehicle->U.UVehicle.LeisurePlace >> 1;
        if ((p_tnode->UTraffic.Flags[last] & 0x80) != 0)
            p_vehicle->U.UVehicle.LeisurePlace |= 0x02;
        DIFFTEST_RETURN((short)last);
    }

    // Try random links first
    for (i = 0; i < 8; i++)
    {
        k = LbRandomAnyShort() & 0x3;
        if (p_tnode->UTraffic.Link[k] == 0)
            continue;
        lnk = p_tnode->UTraffic.Link[k];
        if (lnk <= 0) {
            if ((game_traffic_nodes[-lnk].Flags & 0x1800) != 0)
                continue;
        }
        if ((p_tnode->UTraffic.Flags[k] & 0x80) == 0)
        {
            p_vehicle->U.UVehicle.LeisurePlace =
              (short)p_vehicle->U.UVehicle.LeisurePlace >> 1;
            DIFFTEST_RETURN((short)k);
        }
        // TODO the asm indexes the node array with -lnk even if lnk is a positive (station) link
        p_lntnode = &game_traffic_nodes[-lnk];
        if ((p_lntnode->Flags & 0x40) != 0)
            continue;
        lp = (short)p_vehicle->U.UVehicle.LeisurePlace;
        if (lp == 2)
        {
            if (lnk > 0) {
                ok = true;
            } else {
                s32 j;
                ok = false;
                for (j = 0; j < 4; j++)
                {
                    if (p_lntnode->UTraffic.Link[j] != 0 &&
                      (p_lntnode->UTraffic.Flags[j] & 0x80) == 0) {
                        ok = true;
                        break;
                    }
                }
            }
            if (!ok)
                continue;
        }
        else
        {
            if (lp == 3)
                continue;
            if (!good_node(lnk))
                continue;
        }
        p_vehicle->U.UVehicle.LeisurePlace =
          ((short)p_vehicle->U.UVehicle.LeisurePlace >> 1) | 0x02;
        DIFFTEST_RETURN((short)k);
    }

    // Any link without 0x80 flag, to a node which is not blocked
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] == 0)
            continue;
        if ((p_tnode->UTraffic.Flags[i] & 0x80) != 0)
            continue;
        lnk = p_tnode->UTraffic.Link[i];
        if (lnk < 0) {
            if ((game_traffic_nodes[-lnk].Flags & 0x1800) != 0)
                continue;
        }
        p_vehicle->U.UVehicle.LeisurePlace =
          (short)p_vehicle->U.UVehicle.LeisurePlace >> 1;
        DIFFTEST_RETURN((short)i);
    }

    // Any link to a node which is not blocked
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] == 0)
            continue;
        lnk = p_tnode->UTraffic.Link[i];
        if (lnk < 0) {
            if ((game_traffic_nodes[-lnk].Flags & 0x1800) != 0)
                continue;
        }
        p_vehicle->U.UVehicle.LeisurePlace =
          ((short)p_vehicle->U.UVehicle.LeisurePlace >> 1) | 0x02;
        DIFFTEST_RETURN((short)i);
    }

    // Any link at all
    for (i = 0; i < 4; i++)
    {
        if (p_tnode->UTraffic.Link[i] == 0)
            continue;
        p_vehicle->U.UVehicle.LeisurePlace =
          (short)p_vehicle->U.UVehicle.LeisurePlace >> 1;
        if ((p_tnode->UTraffic.Flags[i] & 0x80) != 0)
            p_vehicle->U.UVehicle.LeisurePlace |= 0x02;
        DIFFTEST_RETURN((short)i);
    }
    DIFFTEST_RETURN((short)0);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int VNAV_renavigate_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_VNAV_renavigate\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Route buffer filled by VNAV_do(), asm data at VNAV_route: [2] is the
 * route length, the route nodes start at [3] (the asm label
 * VNAV_route__route) and [4] is 42 if no route was found.
 */
extern short VNAV_route[];

void VNAV_do(int start_node, int dest_node, short *p_route);

/** Computes a route from the vehicle's traffic node to node AccelX, and
 * packs the link index taken at each route step (2 bits per step, up to
 * 16 steps) into Dummy5a/Dummy5b, step count into low bits of Dummy4a.
 * Returns 0 if no route exists or a step has no matching link.
 * p_vehicle comes in eax.
 */
int VNAV_renavigate(struct Thing *p_vehicle)
{
    short *p_route_hdr = VNAV_route;
    u32 *p_path;
    u32 *p_flags32;
    ubyte *p_path8;
    ubyte *p_flags8;
    s32 nsteps, ridx;
    s32 j, lnk, node;
    u32 val;
    uint sh;

    DIFFTEST_SHADOW_RET(VNAV_renavigate, int,
      VNAV_renavigate_asm((intptr_t)p_vehicle, 0, 0, 0));
    // Dummy5a/Dummy5b and Dummy4a/Dummy4b are accessed as 32-bit values
    p_path = (u32 *)&p_vehicle->U.UVehicle.Dummy5a;
    p_path8 = (ubyte *)&p_vehicle->U.UVehicle.Dummy5a;
    p_flags32 = (u32 *)&p_vehicle->U.UVehicle.Dummy4a;
    p_flags8 = (ubyte *)&p_vehicle->U.UVehicle.Dummy4a;

    // The asm passes its ecx on here; VNAV_do does not read it
    VNAV_do(p_vehicle->U.UVehicle.TNode, p_vehicle->U.UVehicle.AccelX,
      VNAV_route);
    if (VNAV_route[4] == 42)
        DIFFTEST_RETURN((int)0);

    // route length is the short at VNAV_route[2]
    ridx = p_route_hdr[2] - 2;
    nsteps = 0;
    while (ridx >= 0)
    {
        for (j = 0; ; j++)
        {
            node = -VNAV_route[5 + ridx];
            lnk = game_traffic_nodes[node].UTraffic.Link[j];
            if (lnk < 0 && lnk == VNAV_route[4 + ridx])
                break;
            if (lnk > 0 && (ushort)things[lnk].U.UObject.TNode ==
              (ushort)VNAV_route[4 + ridx])
                break;
            if (2 * j > 6)
                DIFFTEST_RETURN((int)0);
        }
        *p_path8 &= 0xfc;
        val = *p_path | (u32)j;
        *p_path = val;
        val = (val >> 2) | (val << 30);
        *p_path = val;
        ridx--;
        nsteps++;
        if (nsteps >= 16)
            break;
    }
    val = *p_path;
    sh = (2 * nsteps) & 31;
    if (sh != 0)
        val = (val << sh) | (val >> (32 - sh));
    *p_path = val;
    *p_flags8 &= 0xc0;
    *p_flags32 = *p_flags32 | (u32)nsteps;
    DIFFTEST_RETURN((int)1);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int VNAV_there_asm(int a1)
{
    int ra = a1;
    DIFFTEST_ASM("call ASM_VNAV_there\n"
        : "+a" (ra) : : "edx", "ebx", "ecx", "esi", "edi", "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Returns 1 when the vehicle p_vehicle (eax)
 * has no turns left on its route.
 */
int VNAV_there(struct Thing *p_vehicle)
{
    DIFFTEST_SHADOW_RET(VNAV_there, int, VNAV_there_asm((intptr_t)p_vehicle));
    DIFFTEST_RETURN((int)((p_vehicle->U.UVehicle.Dummy4a & 0x3F) == 0));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int VNAV_turn_asm(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    DIFFTEST_ASM("call ASM_VNAV_turn\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "ecx", "esi", "edi", "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Takes the next turn of the route of
 * p_vehicle (eax) and returns its link index (0..3).
 */
int VNAV_turn(struct Thing *p_vehicle)
{
    s32 *p_turns_left;
    s32 *p_turns;
    s32 turns;

    DIFFTEST_SHADOW_RET(VNAV_turn, int, VNAV_turn_asm((intptr_t)p_vehicle, 0, 0));
    // Dummy4a/Dummy4b and Dummy5a/Dummy5b are accessed as 32-bit values:
    // a counter of remaining turns, and a list of 2-bit turn directions.
    p_turns_left = (s32 *)&p_vehicle->U.UVehicle.Dummy4a;
    p_turns = (s32 *)&p_vehicle->U.UVehicle.Dummy5a;
    (*p_turns_left)--;
    turns = *p_turns;
    *p_turns = turns >> 2;
    DIFFTEST_RETURN((int)(turns & 0x3));
}




static DIFFTEST_REF_FUNC void process_next_vnav_turn_asm(struct Thing *p_vehicle)
{
    DIFFTEST_ASM("call ASM_process_next_vnav_turn\n"
        : : "a" (p_vehicle));
}

/** Moves the vehicle to the next traffic node of its VNAV route.
 * Dummy4a/Dummy4b and Dummy5a/Dummy5b are accessed as 32-bit values.
 */
void process_next_vnav_turn(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    struct TrafficNode *p_nxtnode;
    u32 *p_d4;
    u32 *p_d5;
    ubyte *p_d4b;
    s32 tnode, link, frac;
    s32 x, z;
    TbBool follow_route;

    DIFFTEST_SHADOW(process_next_vnav_turn, process_next_vnav_turn_asm(p_vehicle));
    p_d4 = (u32 *)&p_vehicle->U.UVehicle.Dummy4a;
    p_d4b = (ubyte *)&p_vehicle->U.UVehicle.Dummy4a;
    p_d5 = (u32 *)&p_vehicle->U.UVehicle.Dummy5a;

    follow_route = !VNAV_there(p_vehicle);
    if (!follow_route &&
      p_vehicle->U.UVehicle.AccelX != p_vehicle->U.UVehicle.TNode)
    {
        follow_route = (VNAV_renavigate(p_vehicle) != 0);
        if (!follow_route)
            p_vehicle->State = 0x3c;
    }

    if (follow_route)
    {
        // Follow the next turn of the computed route
        tnode = p_vehicle->U.UVehicle.TNode;
        p_tnode = &game_traffic_nodes[-tnode];
        p_vehicle->U.UVehicle.Dummy4b = 0;
        *p_d4 |= (u32)tnode << 16;
        link = VNAV_turn(p_vehicle);
        *p_d4b &= 0x3f;
        *p_d4 |= (u32)link << 6;
        p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[link];
        if ((p_tnode->UTraffic.Flags[link] & 0x01) != 0)
            p_vehicle->Flag |= TngF_Unkn08000000;
        else
            p_vehicle->Flag &= ~TngF_Unkn08000000;
        if (p_vehicle->U.UVehicle.TNode == 0)
            start_crashing(p_vehicle);
        return;
    }

    // Arrived; take the link stored in low bits of AccelZ
    tnode = p_vehicle->U.UVehicle.TNode;
    p_tnode = &game_traffic_nodes[-tnode];
    p_vehicle->U.UVehicle.Dummy4b = 0;
    *p_d4 |= (u32)tnode << 16;
    link = p_vehicle->U.UVehicle.AccelZ & 0xf;
    *p_d4b &= 0x3f;
    *p_d4 |= (u32)link << 6;
    p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[link];
    p_vehicle->State = 0x33;

    p_nxtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
    frac = (*p_d4 & 0xff00) >> 8;
    if (frac < 0x30)
        frac = 0x30;
    else if (frac > 0xe0)
        frac = 0xe0;
    x = (((s32)p_nxtnode->X - p_tnode->X) * frac >> 8) + p_tnode->X;
    z = (((s32)p_nxtnode->Z - p_tnode->Z) * frac >> 8) + p_tnode->Z;
    *p_d5 = (u32)x;
    *p_d5 = (u32)x << 16;
    *p_d5 = ((u32)x << 16) | (u32)z;
}

void add_agphase1_lock(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.TNode;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags |= TNdF_Unkn0040;
    p_tnode->GateLink = p_vehicle->ThingOffset;
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgPhase1;
}

void remove_agphase1_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase1;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agphase1_lock(struct Thing *p_vehicle)
{
    remove_agphase1_node_lock(p_vehicle, p_vehicle->U.UVehicle.TNode);
}

void promote_agphase1_lock(struct Thing *p_vehicle)
{
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgPhase2;
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase1;
}

void remove_agphase2_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgPhase2;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agphase2_lock(struct Thing *p_vehicle)
{
    remove_agphase2_node_lock(p_vehicle, p_vehicle->U.UVehicle.Dummy4b);
}

void promote_agphase2_lock(struct Thing *p_vehicle)
{
    p_vehicle->U.UVehicle.Agok = p_vehicle->U.UVehicle.Dummy4b;
    p_vehicle->U.UVehicle.WorkPlace &= ~(VWPFlg_HasAgPhase2|VWPFlg_HasAgok);
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgok;
}

void add_agok_lock(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.Agok;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags |= TNdF_Unkn0040;
    p_tnode->GateLink = p_vehicle->ThingOffset;
    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_HasAgok;
}

void remove_agok_node_lock(struct Thing *p_vehicle, short tnode)
{
    struct TrafficNode *p_tnode;

    assert(tnode <= 0);

    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_HasAgok;
    p_tnode = &game_traffic_nodes[-tnode];
    p_tnode->Flags &= ~TNdF_Unkn0040;
    p_tnode->GateLink = 0;
}

void remove_agok_lock(struct Thing *p_vehicle)
{
    remove_agok_node_lock(p_vehicle, p_vehicle->U.UVehicle.Agok);
}

void remove_locks(struct Thing *p_vehicle)
{
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
        remove_agphase1_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
        remove_agphase2_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
        remove_agok_lock(p_vehicle);
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
}

void remove_locks_apart_from_agok(struct Thing *p_vehicle)
{
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
        remove_agphase1_lock(p_vehicle);
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
        remove_agphase2_lock(p_vehicle);
    p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
}

void update_workplace_unkn0001_flag(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    short tnode;

    tnode = p_vehicle->U.UVehicle.TNode;
    assert(tnode < 0);

    p_tnode = &game_traffic_nodes[-tnode];
    if ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0)
        p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0001;
    else
        p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0001;
}

static TbBool check_person_close_on_mapel(struct MyMapElement *p_mapel)
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
          case TT_PERSON:
              if ((p_thing->State != PerSt_DEAD)
                && (p_thing->State != PerSt_PERSON_BURNING)
                && (p_thing->State != PerSt_DIEING)
                //&& ((p_thing->Flag & TngF_InVehicle) == 0) -- people in vehicles should not be within the list
                && ((p_thing->Flag2 & TgF2_KnockedOut) == 0))
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

int check_person_close(struct TrafficNode *p_tnode)
{
    int tile_ctr_x, tile_ctr_z;
    int tlno;
    int tile_x;

    tile_ctr_z = p_tnode->Z >> 8;
    tile_ctr_x = p_tnode->X >> 8;

    for (tile_x = tile_ctr_x - 1; tile_x <= tile_ctr_x + 1; tile_x++)
    {
        tlno = 128 * (tile_ctr_z - 1);
        while (tlno < 128 * tile_ctr_z + 256)
        {
            struct MyMapElement *p_mapel;

            p_mapel = &game_my_big_map[tlno + tile_x];
            if (check_person_close_on_mapel(p_mapel))
                return true;
            tlno += 128;
        }
    }
    return 0;
}

void update_vehicle_elevation(struct Thing *p_vehicle, short statn)
{
    short angleX;

    angleX = 0;
    if (statn > 0)
    {
        struct Thing *p_station;
        short nxstatn;

        p_station = &things[statn];
        if ((p_station->Flag & TngF_Destroyed) != 0)
        {
            LOGERR("Crashing vehicle %d due to destroyed station %d",
              p_vehicle->ThingOffset, statn);
            start_crashing(p_vehicle);
            return;
        }
        if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
            nxstatn = p_station->U.UObject.NextThing;
        else
            nxstatn = p_station->U.UObject.PrevThing;

        if (nxstatn > 0)
        {
            int cux, cuy, cuz, nxx, nxy, nxz, faxz;
            struct Thing *p_nxstation;
            int dt_x, dt_y, dt_z;

            p_nxstation = &things[nxstatn];
            cux = (p_station->X >> 8) - 5 * p_station->U.UObject.OffX;
            nxx = (p_nxstation->X >> 8) - 5 * p_nxstation->U.UObject.OffX;
            cuz = (p_station->Z >> 8) - 5 * p_station->U.UObject.OffZ;
            nxz = (p_nxstation->Z >> 8) - 5 * p_nxstation->U.UObject.OffZ;
            cuy = (p_station->Y >> 3) + 3840;
            nxy = (p_nxstation->Y >> 3) + 3840;

            dt_x = (cux - nxx);
            dt_y = (cuy - nxy) >> 5;
            dt_z = (cuz - nxz);
            faxz = map_distance_deltas_fast(dt_x, dt_y, dt_z);
            angleX = arctan(abs(dt_y), faxz) - 1024;
            if (dt_y < 0)
                angleX = -angleX;
            p_vehicle->Y += (cuy - p_vehicle->Y) >> 3;
        }
    }

    if ((p_vehicle->State == VehSt_GOTO_LOC) && ((gameturn & 3) == 0)
      && ((p_vehicle->U.UVehicle.Dummy4a & 0x3F) > 2))
    {
        int dx, dz;

        dx = abs(p_vehicle->U.UVehicle.destx - (p_vehicle->X >> 8));
        dz = abs(p_vehicle->U.UVehicle.destz - (p_vehicle->Z >> 8));
        if (dx <= dz)
            dx >>= 1;
        else
            dz >>= 1;
        if (dx + dz < 768)
            p_vehicle->State = VehSt_UNKN_3C;
    }
    if (p_vehicle->U.UVehicle.AngleX < angleX)
        p_vehicle->U.UVehicle.AngleX += 4;
    if (p_vehicle->U.UVehicle.AngleX > angleX)
        p_vehicle->U.UVehicle.AngleX -= 4;
}

void update_vehicle_speed_and_direction(struct Thing *p_vehicle, int tndt_x, int tndt_z, s32 nxdist_sq)
{
    short angleY, angDY;

    angleY = p_vehicle->U.UVehicle.AngleY - arctan(tndt_x, tndt_z);
    if (angleY < -1024)
        angleY += 2048;
    else if (angleY > 1024)
        angleY -= 2048;

    if (angleY > 0)
    {
        if (angleY >= 50)
          angDY = (2048 - (p_vehicle->Speed >> 3)) >> 2;
        else
          angDY = 2 * angleY;
        angDY = -angDY;
    }
    else
    {
        if (angleY <= -50)
            angDY = (2048 - (p_vehicle->Speed >> 3)) >> 2;
        else
            angDY = -2 * angleY;
    }
    p_vehicle->U.UVehicle.AngleDY = angDY;

    if ((angleY > 512) || (angleY < -512))
        angleY = 512;
    if (p_vehicle->SubState == 2)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = nxdist_sq >= 0x8000 ? 2048 - 4 * abs(angleY) : 512;
        if (p_vehicle->U.UVehicle.ReqdSpeed < 10)
            p_vehicle->U.UVehicle.ReqdSpeed = 10;
    }
}

void process_vehicle_stop_for_pedestrians(struct Thing *p_vehicle)
{
    struct TrafficNode *p_tnode;
    struct Thing *p_gtveh;

    // Only Traffic Nodes support stopping for pedestrians
    if (p_vehicle->U.UVehicle.TNode >= 0)
        return;

    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
    {
        if (p_vehicle->State == VehSt_GOTO_LOC || p_vehicle->State == VehSt_WANDER)
        {
            if ((p_tnode->Flags & TNdF_Unkn0040) != 0)
            {
                LOGDBG("Stopping %s thing %d rqspeed %d due to TNode %d flags",
                  vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                  (int)p_vehicle->U.UVehicle.ReqdSpeed, (int)p_vehicle->U.UVehicle.TNode);

                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_gtveh = &things[p_tnode->GateLink];
                if ((p_gtveh->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
                {
                    struct TrafficNode *p_tztnode;

                    p_tztnode = &game_traffic_nodes[-p_gtveh->U.UVehicle.TNode];
                    if ((p_tztnode->Flags & TNdF_Unkn0040) != 0)
                    {
                        if (p_tztnode->GateLink == p_vehicle->ThingOffset)
                        {
                            if (p_vehicle->U.UVehicle.Dummy4b == p_gtveh->U.UVehicle.TNode) {
                                remove_agphase2_lock(p_vehicle);
                            } else {
                                remove_agok_node_lock(p_vehicle, p_gtveh->U.UVehicle.TNode);
                            }
                            if (p_gtveh->U.UVehicle.Dummy4b == p_vehicle->U.UVehicle.TNode) {
                                remove_agphase2_lock(p_gtveh);
                            } else {
                                remove_agok_node_lock(p_gtveh, p_vehicle->U.UVehicle.TNode);
                            }
                        }
                    }
                }
            }
            else
            {
                remove_locks_apart_from_agok(p_vehicle);
                add_agphase1_lock(p_vehicle);
                p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
            }
        }
    }

    if (((p_tnode->Flags & TNdF_Unkn0008) != 0) && check_person_close(p_tnode))
    {
        if (p_vehicle->U.UVehicle.PissedOffWithWaiting < 100)
        {
            if (p_vehicle->U.UVehicle.PissedOffWithWaiting == 0) {
                p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
            }
            if (p_vehicle->U.UVehicle.ZebraOldHealth == p_vehicle->Health) {
                LOGDBG("Stopping %s thing %d rqspeed %d due to TNode %d near pedestrian",
                  vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                  (int)p_vehicle->U.UVehicle.ReqdSpeed, (int)p_vehicle->U.UVehicle.TNode);

                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
                p_vehicle->U.UVehicle.PissedOffWithWaiting++;
            } else {
                // Lost health while waiting - stop the wait immediatelly
                p_vehicle->U.UVehicle.PissedOffWithWaiting = 200;
            }
        }
    }
    else
    {
        p_vehicle->U.UVehicle.PissedOffWithWaiting = 0;
    }
}

void vehicle_workplace_states_finish(struct Thing *p_vehicle)
{
    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
    {
        struct TrafficNode *p_dmtnode;
        int dx, dz;

        p_dmtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Dummy4b];
        dx = (p_vehicle->X >> 8) - p_dmtnode->X;
        dz = (p_vehicle->Z >> 8) - p_dmtnode->Z;
        if (dz * dz + dx * dx > 0x20000)
        {
            remove_agphase2_lock(p_vehicle);
        }
    }

    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
    {
        struct TrafficNode *p_agtnode;
        int dx, dz;

        p_agtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Agok];
        dx = (p_vehicle->X >> 8) - p_agtnode->X;
        dz = (p_vehicle->Z >> 8) - p_agtnode->Z;
        if (dz * dz + dx * dx > 0x20000)
        {
            remove_agok_lock(p_vehicle);
        }
    }
}

static DIFFTEST_REF_FUNC void VNAV_process_bezier_asm(struct Thing *p_road)
{
    DIFFTEST_ASM("call ASM_VNAV_process_bezier\n"
        : : "a" (p_road));
}

/** Fast approximation of distance: max + min/2.
 */
static int VNAV_bezier_approx_dist(int x1, int z1, int x2, int z2)
{
    int adx, adz;

    adx = abs(x2 - x1);
    adz = abs(z2 - z1);
    if (adx > adz)
        adz >>= 1;
    else
        adx >>= 1;
    return adx + adz;
}

void VNAV_process_bezier(struct Thing *p_road)
{
    struct Thing *p_thing;
    struct TrafficNode *p_tnode;
    int prv_x, prv_z;
    int cur_x, cur_z;
    int dist;

    DIFFTEST_SHADOW(VNAV_process_bezier, VNAV_process_bezier_asm(p_road));
    if ((p_road->Type != TT_BUILDING) || (p_road->SubType != SubTT_BLD_BEZIER_ROAD))
        return;

    if (p_road->U.UObject.PrevThing < 0)
    {
        p_tnode = &game_traffic_nodes[-p_road->U.UObject.PrevThing];
        prv_x = p_tnode->X;
        prv_z = p_tnode->Z;
        dist = 0;
        p_thing = p_road;
        while (1)
        {
            cur_x = (p_thing->X >> 8) + p_thing->U.UObject.OffX * 7;
            cur_z = (p_thing->Z >> 8) + p_thing->U.UObject.OffZ * 7;
            dist += VNAV_bezier_approx_dist(prv_x, prv_z, cur_x, cur_z);
            prv_x = cur_x;
            prv_z = cur_z;
            if (p_thing->U.UObject.NextThing <= 0)
                break;
            p_thing = &things[p_thing->U.UObject.NextThing];
        }
        p_tnode = &game_traffic_nodes[-p_thing->U.UObject.NextThing];
        dist += VNAV_bezier_approx_dist(prv_x, prv_z, p_tnode->X, p_tnode->Z);
        p_road->U.UObject.TNode = p_thing->U.UObject.NextThing;
        p_road->U.UObject.Cost = dist;
        p_thing->U.UObject.TNode = p_road->U.UObject.PrevThing;
        p_thing->U.UObject.Cost = dist;
    }
    else if (p_road->U.UObject.NextThing < 0)
    {
        p_tnode = &game_traffic_nodes[-p_road->U.UObject.NextThing];
        prv_x = p_tnode->X;
        prv_z = p_tnode->Z;
        dist = 0;
        p_thing = p_road;
        while (1)
        {
            cur_x = (p_thing->X >> 8) - p_thing->U.UObject.OffX * 7;
            cur_z = (p_thing->Z >> 8) - p_thing->U.UObject.OffZ * 7;
            dist += VNAV_bezier_approx_dist(prv_x, prv_z, cur_x, cur_z);
            prv_x = cur_x;
            prv_z = cur_z;
            if (p_thing->U.UObject.PrevThing <= 0)
                break;
            p_thing = &things[p_thing->U.UObject.PrevThing];
        }
        p_tnode = &game_traffic_nodes[-p_thing->U.UObject.PrevThing];
        dist += VNAV_bezier_approx_dist(prv_x, prv_z, p_tnode->X, p_tnode->Z);
        p_road->U.UObject.TNode = p_thing->U.UObject.PrevThing;
        p_road->U.UObject.Cost = dist;
        p_thing->U.UObject.TNode = p_road->U.UObject.NextThing;
        p_thing->U.UObject.Cost = dist;
    }
}

void VNAV_preprocess_bezier_turns(u32 nturns)
{
    u32 turns;

    for (turns = nturns; turns > 0; turns--)
    {
        ThingIdx thing;
        ushort i;

        thing = things_used_head;
        i = things_used;
        while ((thing > 0) && (i > 0))
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if ((p_thing->Type == TT_BUILDING)
              && (p_thing->SubType == SubTT_BLD_BEZIER_ROAD))
                VNAV_process_bezier(p_thing);
            thing = p_thing->LinkChild;
            i--;
        }
    }
}

static DIFFTEST_REF_FUNC void VNAV_init_new_traffic_system_asm(void)
{
    DIFFTEST_ASM("call ASM_VNAV_init_new_traffic_system\n"
        :  :  : "eax" );
}

/** Returns whether any traffic node has a link to the given node.
 */
static TbBool tnode_is_linked_to(struct TrafficNode *p_nodes, int tnode)
{
    int j, k;

    for (j = 1; j < next_traffic_node; j++)
    {
        for (k = 0; k < 4; k++)
        {
            if (p_nodes[j].UTraffic.Link[k] == -tnode)
                return true;
        }
    }
    return false;
}

void VNAV_init_new_traffic_system(void)
{
    struct TrafficNode *p_nodes;
    struct TrafficNode *p_tnode;
    s32 *p_list2_count;
    s32 count;
    int i, k;

    DIFFTEST_SHADOW(VNAV_init_new_traffic_system, VNAV_init_new_traffic_system_asm());
    // The second node list: count followed by entries [1..16]; the asm
    // reaches it as dword_1C8474 + 32
    p_list2_count = data_1c84f4;

    p_nodes = game_traffic_nodes;
    dword_1C8470 = 0;
    *p_list2_count = 0;

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        p_tnode->Flags &= ~(TNdF_Unkn0040 | TNdF_Unkn0080 | TNdF_Unkn0100);
    }

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        for (k = 0; k < 4; k++)
        {
            if (p_tnode->UTraffic.Link[k] != 0)
                break;
        }
        if (k >= 4)
            p_tnode->Flags |= TNdF_Unkn0080;
    }

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        if ((p_tnode->Flags & (TNdF_Unkn0200 | TNdF_Unkn0400)) != 0)
            continue;
        if (tnode_is_linked_to(p_nodes, i))
            continue;
        p_tnode->Flags |= TNdF_Unkn0100;
        if (dword_1C8470 != 32)
        {
            dword_1C8474[dword_1C8470] = i;
            dword_1C8470++;
        }
    }

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        if ((p_tnode->Flags & TNdF_Unkn0080) == 0)
            continue;
        if ((p_tnode->Flags & TNdF_Unkn0100) == 0)
            continue;
        p_tnode->Flags = 0xff;
    }

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        if ((p_tnode->Flags & TNdF_Unkn0200) == 0)
            continue;
        if ((p_tnode->Flags & TNdF_Unkn0400) != 0)
            continue;
        if (tnode_is_linked_to(p_nodes, i))
            continue;
        p_tnode->Flags |= TNdF_Unkn0100;
        count = *p_list2_count;
        if (count < 16)
        {
            count++;
            p_list2_count[count] = i;
            *p_list2_count = count;
        }
    }

    for (i = 1; i < next_traffic_node; i++)
    {
        p_tnode = &p_nodes[i];
        if (p_tnode->Flags == 0xff)
            continue;
        if ((p_tnode->Flags & TNdF_Unkn8000) == 0)
            continue;
        for (k = 0; k < 4; k++)
            p_tnode->UTraffic.Flags[k] |= 0x40;
    }

    game_traffic_nodes = p_nodes;
}

extern s32 dword_1556F0;
extern s32 dword_1556F4;
extern s32 dword_1556F8;
extern s32 dword_1556FC;
extern s32 dword_155700;
extern s32 data_1dd8fc;
extern s32 data_1dd900;
extern s32 data_1dd904;
extern s32 data_1dd908;
extern s32 data_1dd90c;
extern s32 data_1dd910;
extern s32 data_1dd914;
extern s32 data_1dd918;

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
s32 dword_1556F0 = -50;
s32 dword_1556F4 = 0x4000;
s32 dword_1556F8 = 0x200;
s32 dword_1556FC = 0x4000;
s32 dword_155700 = 0x400;
uint dword_1C8470;
int dword_1C8474[32];
s32 data_1dd8fc;
s32 data_1dd900;
s32 data_1dd904;
s32 data_1dd908;
s32 data_1dd90c;
s32 data_1dd910;
s32 data_1dd914;
s32 data_1dd918;
s32 data_1dd91c;
#endif /* typed asm data */

static DIFFTEST_REF_FUNC void traffic_unkn_func_01_asm(void)
{
    DIFFTEST_ASM("call ASM_traffic_unkn_func_01\n"
        :  :  : "eax" );
}

void traffic_unkn_func_01(void)
{
    struct TrafficNode *p_tnode;
    struct TrafficNode *p_lnode;
    s32 node_x, node_y, node_z;
    s32 dt_x, dt_y, dt_z;
    s32 abs_x, abs_y, abs_z;
    s32 dist, mv_x, mv_y, mv_z;
    s32 delta, vel;
    ushort i;
    int k;

    DIFFTEST_SHADOW(traffic_unkn_func_01, traffic_unkn_func_01_asm());
    if (data_1dd91c == 0)
    {
        render_area_a = render_area_b = 40;
        for (i = 1; i < next_traffic_node; i++)
        {
            p_tnode = &game_traffic_nodes[i];
            if (p_tnode->Flags == 0xff)
                continue;
            if ((p_tnode->Flags & 0x1000) == 0)
                continue;
            break;
        }
        if (i >= next_traffic_node)
        {
            char locstr[100];
            sprintf(locstr, "next_traffic = %d\n", (int)next_traffic_node);
        }
        data_1dd91c = (short)i;
        p_tnode = &game_traffic_nodes[data_1dd91c];
        dword_1556F4 = p_tnode->X;
        dword_1556F8 = p_tnode->Y;
        dword_1556FC = p_tnode->Z;
        for (k = 0; k < 4; k++)
        {
            if (p_tnode->UTraffic.Link[k] == 0)
                continue;
            if ((p_tnode->UTraffic.Flags[k] & 0xC0) == 0)
                continue;
            p_lnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[k]];
            delta = (ushort)arctan(p_lnode->X - dword_1556F4,
              -(p_lnode->Z - dword_1556FC));
            delta <<= 5;
            data_1dd910 = 0;
            data_1dd908 = delta;
            data_1dd918 = delta;
        }
    }

    p_tnode = &game_traffic_nodes[data_1dd91c];
    node_x = p_tnode->X;
    node_y = p_tnode->Y;
    node_z = p_tnode->Z;
    dt_x = node_x - dword_1556F4;
    dt_y = node_y - dword_1556F8;
    dt_z = node_z - dword_1556FC;
    if (dt_x * dt_x + dt_y * dt_y + dt_z * dt_z < 0x40000)
    {
        for (k = 0; k < 4; k++)
        {
            ushort fl;

            if (p_tnode->UTraffic.Link[k] == 0)
                continue;
            if ((p_tnode->UTraffic.Flags[k] & 0xC0) != 0)
            {
                p_lnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[k]];
                dt_x = p_lnode->X - node_x;
                dt_y = p_lnode->Y - node_y;
                dt_z = p_lnode->Z - node_z;
                delta = (ushort)arctan(dt_x, -dt_z);
                data_1dd918 = delta << 5;
                continue;
            }
            data_1dd91c = p_tnode->UTraffic.Link[k];
            data_1dd91c = -p_tnode->UTraffic.Link[k];
            if ((game_traffic_nodes[data_1dd91c].Flags & 0x0400) != 0)
                dword_155700 = 0x400;
            if ((game_traffic_nodes[data_1dd91c].Flags & 0x0008) != 0)
                dword_155700 = 0x800;
            if ((game_traffic_nodes[data_1dd91c].Flags & 0x0800) != 0)
                dword_155700 = 0x1000;
            fl = game_traffic_nodes[data_1dd91c].Flags;
            if ((fl & 0x10) != 0)
                dword_1556F0 = -70;
            else if ((fl & 0x20) != 0)
                dword_1556F0 = -250;
            else
                dword_1556F0 = -125;
        }
    }

    abs_x = abs(dt_x);
    abs_y = abs(dt_y);
    abs_z = abs(dt_z);
    if (abs_x > abs_y && abs_x > abs_z)
        dist = abs_x - (abs_x >> 4) + ((abs_y + abs_z) >> 1) - ((abs_y + abs_z) >> 3);
    else if (abs_x <= abs_y && abs_y > abs_z)
        dist = abs_y - (abs_y >> 4) + ((abs_x + abs_z) >> 1) - ((abs_x + abs_z) >> 3);
    else
        dist = abs_z - (abs_z >> 4) + ((abs_x + abs_y) >> 1) - ((abs_x + abs_y) >> 3);
    if (dist == 0)
        dist = 1;

    mv_x = (dword_155700 * dt_x) / dist;
    mv_y = (dword_155700 * dt_y) / dist;
    mv_z = (dword_155700 * dt_z) / dist;
    mv_x = (mv_x - data_1dd8fc) >> 3;
    mv_z = (mv_z - data_1dd904) >> 3;
    mv_y = (mv_y - data_1dd900) >> 3;
    if (mv_x < -200)
        mv_x = -200;
    if (mv_x > 200)
        mv_x = 200;
    if (mv_y < -200)
        mv_y = -200;
    if (mv_y > 200)
        mv_y = 200;
    if (mv_z < -200)
        mv_z = -200;
    if (mv_z > 200)
        mv_z = 200;

    data_1dd8fc += mv_x;
    data_1dd904 += mv_z;
    data_1dd900 += mv_y;
    dword_1556F4 += data_1dd8fc >> 4;
    engn_xc = dword_1556F4;
    dword_1556F8 += data_1dd900 >> 4;
    engn_yc = dword_1556F8;
    dword_1556FC += data_1dd904 >> 4;
    engn_zc = dword_1556FC;

    delta = data_1dd918 - data_1dd908;
    if (delta < -0x8000)
        delta += 0x10000;
    if (delta > 0x8000)
        delta -= 0x10000;
    delta = (delta - data_1dd910) >> 3;
    if (delta < -350)
        delta = -350;
    if (delta > 350)
        delta = 350;
    vel = data_1dd910 + delta;
    data_1dd908 += vel >> 3;
    // The asm clears the upper half of data_1dd908 by writing the word
    // label data_1dd90a over it
    data_1dd908 &= 0xFFFF;
    engn_cam_yaw = data_1dd908;
    data_1dd910 = vel;

    delta = dword_1556F0 - data_1dd90c;
    if (delta < -0x400)
        delta += 0x800;
    if (delta > 0x400)
        delta -= 0x800;
    delta = (delta - data_1dd914) >> 3;
    if (delta < -25)
        delta = -25;
    if (delta > 25)
        delta = 25;
    vel = data_1dd914 + delta;
    data_1dd90c += vel >> 3;
    data_1dd914 = vel;
    data_1dd90c &= 0x7FF;
    engn_cam_tilt = data_1dd90c;
}

void process_next_tnode(struct Thing *p_vehicle)
{
    short tnode;
    int tndt_x, tndt_z;
    int nxdist_sq; //< Squared distance to next traffic node or station

    tnode = p_vehicle->U.UVehicle.TNode;
    if (tnode == 0) {
        LOGERR("Crashing %s %d due to no TNode assigned",
          vehicle_type_name(p_vehicle->SubType), p_vehicle->ThingOffset);
        start_crashing(p_vehicle);
        return;
    }

    if ((p_vehicle->State == VehSt_UNKN_34 || p_vehicle->State == VehSt_UNKN_33)
      && (tnode == 0) && (p_vehicle->Speed == 0))
    {
        p_vehicle->State = VehSt_PARKED_PARAL;
        p_vehicle->U.UVehicle.AccelZ = 0;
        p_vehicle->U.UVehicle.AccelX = 0;
        p_vehicle->Flag |= TngF_Unkn0004;
        p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0100;
    }

    {
        int cux, cuz;

        if (tnode <= 0) // we've alteady made sure it's not 0
        {
            struct TrafficNode *p_tnode;

            p_tnode = &game_traffic_nodes[-tnode];
            if (p_vehicle->State == VehSt_UNKN_33) {
                cux = p_vehicle->U.UVehicle.Dummy5b;
                cuz = p_vehicle->U.UVehicle.Dummy5a;
            } else {
                cux = p_tnode->X;
                cuz = p_tnode->Z;
            }
        }
        else
        {
            struct Thing *p_station;

            p_station = &things[tnode];
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0) {
                cux = PRCCOORD_TO_MAPCOORD(p_station->X) - 5 * p_station->U.UObject.OffX;
                cuz = PRCCOORD_TO_MAPCOORD(p_station->Z) - 5 * p_station->U.UObject.OffZ;
            } else {
                cux = PRCCOORD_TO_MAPCOORD(p_station->X) + 5 * p_station->U.UObject.OffX;
                cuz = PRCCOORD_TO_MAPCOORD(p_station->Z) + 5 * p_station->U.UObject.OffZ;
            }
        }

        tndt_x = cux - PRCCOORD_TO_MAPCOORD(p_vehicle->X);
        tndt_z = cuz - PRCCOORD_TO_MAPCOORD(p_vehicle->Z);
        nxdist_sq = tndt_z * tndt_z + tndt_x * tndt_x;
    }

    if (tnode > 0)
    {
        p_vehicle->U.UVehicle.ReqdSpeed = 1512;
    }
    else if (tnode < 0)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[-tnode];
        p_vehicle->U.UVehicle.ReqdSpeed = 1200 - 2 * abs(p_vehicle->U.UVehicle.AngleDY);
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
         && ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
         && (p_tnode->GateLink == p_vehicle->ThingOffset)) {
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
        }
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0008) != 0)
         && ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) == 0)) {
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0008;
        }
        if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
         && ((p_tnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) == 0)) {
            remove_agphase1_lock(p_vehicle);
        }
    }

    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;
        struct Thing *p_gtveh;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && ((p_tnode->Flags & TNdF_Unkn8000) != 0) && (nxdist_sq < 0x90000))
        {
            p_gtveh = &things[p_tnode->GateLink];
            if ((p_gtveh->Flag & TngF_Unkn0040) == 0) {
                p_gtveh->Flag |= TngF_Unkn0020;
                p_vehicle->U.UVehicle.ReqdSpeed = 0;
                p_vehicle->U.UVehicle.AngleDY = 0;
                return;
            }
        }
    }

    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;
        struct Thing *p_gtveh;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && ((p_tnode->Flags & TNdF_Unkn8000) == 0) && (nxdist_sq < 0x10000))
        {
            p_gtveh = &things[p_tnode->GateLink];
            p_gtveh->Flag |= TngF_VehUnkn0100;
        }
    }

    if (tnode > 0)
    {
        struct Thing *p_station;
        short nxstatn;

        if (nxdist_sq < 0xC000)
        {
            p_station = &things[tnode];
            if ((p_vehicle->Flag & TngF_Unkn08000000) != 0)
                nxstatn = p_station->U.UObject.PrevThing;
            else
                nxstatn = p_station->U.UObject.NextThing;
            p_vehicle->U.UVehicle.TNode = nxstatn;
        }
    }

    TbBool do_state = false;
    if (tnode < 0)
    {
        struct TrafficNode *p_tnode;

        p_tnode = &game_traffic_nodes[-tnode];
        if ((p_tnode->GateLink != 0) && (nxdist_sq < 0x10000))
            do_state = true;
        if ((p_tnode->GateLink == 0) && (nxdist_sq < 0x20000))
            do_state = true;
    }

    if (do_state)
    {
        struct TrafficNode *p_tnode;
        struct TrafficNode *p_nxtnode;

        p_tnode = &game_traffic_nodes[-tnode];
        switch (p_vehicle->State)
        {
        case VehSt_GOTO_LOC:
            update_workplace_unkn0001_flag(p_vehicle);
            if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
            {
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0) {
                    remove_agok_lock(p_vehicle);
                }
                promote_agphase2_lock(p_vehicle);
            }
            if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0) {
                promote_agphase1_lock(p_vehicle);
            }
            process_next_vnav_turn(p_vehicle);
            if (p_vehicle->U.UVehicle.TNode < 0)
            {
                p_nxtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                if ((p_nxtnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0) {
                    nxdist_sq = 0x7FFFFFFF;
                    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0008;
                }
            }
            break;
        case VehSt_UNKN_33:
            p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
            p_nxtnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.Dummy4b];
            if ((p_tnode->Flags & TNdF_Unkn0800) != 0)
            {
                int dx, dz;

                dx = p_tnode->X - p_nxtnode->X;
                dz = p_tnode->Z - p_nxtnode->Z;
                p_vehicle->U.UVehicle.AccelZ = 0;
                p_vehicle->U.UVehicle.AccelX = 0;
                p_vehicle->State = VehSt_UNKN_36;
                p_vehicle->U.UVehicle.Dummy5b = arctan(dx, dz);
                p_vehicle->U.UVehicle.Dummy5a = 0;
            }
            else
            {
                int simp_dist;
                int accX, accZ;
                int selX, selZ;
                int dx, dz;

                dx = p_tnode->X - p_nxtnode->X;
                dz = p_nxtnode->Z - p_tnode->Z;
                if (abs(dx) >= abs(dz)) {
                    simp_dist = abs(dx) + (abs(dz) >> 1);
                } else {
                    simp_dist = abs(dz) + (abs(dx) >> 1);
                }
                accX = 32 * dz;
                accZ = 32 * dx;
                if (simp_dist > 0) {
                    accX /= simp_dist;
                    accZ /= simp_dist;
                }
                selX = (p_vehicle->X >> 8) + 4 * accX + 4 * accZ + 8 * accX;
                selZ = (p_vehicle->Z >> 8) - 4 * accX + 4 * accZ + 8 * accZ;
                if (((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_Unkn0100) != 0)
                  || !check_for_a_vehicle_here(selX, selZ, p_vehicle))
                {
                    dx = p_tnode->X - p_nxtnode->X;
                    dz = p_tnode->Z - p_nxtnode->Z;
                    p_vehicle->State = VehSt_UNKN_36;
                    p_vehicle->U.UVehicle.Dummy5a = 0;
                    p_vehicle->U.UVehicle.Dummy5b = arctan(dx, dz);
                    p_vehicle->U.UVehicle.AccelZ = accZ;
                    p_vehicle->U.UVehicle.AccelX = accX;
                    p_vehicle->U.UVehicle.Dummy5a |= 0x11;
                }
                else
                {
                    p_vehicle->U.UVehicle.Dummy5a -= 32 * accX;
                    p_vehicle->U.UVehicle.Dummy5b += 32 * accZ;
                    p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0100;
                }
            }
            break;
        case VehSt_UNKN_34:
            tnode = p_vehicle->U.UVehicle.TNode;
            p_vehicle->U.UVehicle.Dummy4b = 0;
            p_vehicle->U.UVehicle.Dummy4a = tnode;
            p_vehicle->State = VehSt_UNKN_36;
            p_tnode = &game_traffic_nodes[-tnode];
            p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[0];
            break;
        default:
            p_vehicle->U.UVehicle.WorkPlace &= ~VWPFlg_Unkn0004;
            if ((p_tnode->Flags & TNdF_Unkn0080) == 0)
            {
                short lnk;

                update_workplace_unkn0001_flag(p_vehicle);
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase2) != 0)
                {
                    if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgok) != 0)
                    {
                        remove_agok_lock(p_vehicle);
                    }
                    promote_agphase2_lock(p_vehicle);
                }

                p_vehicle->U.UVehicle.Dummy4a = 0;
                p_vehicle->U.UVehicle.Dummy4b = p_vehicle->U.UVehicle.TNode;
                if ((p_vehicle->U.UVehicle.WorkPlace & VWPFlg_HasAgPhase1) != 0)
                {
                    promote_agphase1_lock(p_vehicle);
                }
                lnk = get_next_tnode(p_vehicle, p_tnode);
                p_vehicle->U.UVehicle.TNode = p_tnode->UTraffic.Link[lnk];
                if (p_vehicle->U.UVehicle.TNode == 0) {
                    LOGERR("Crashing %s %d state %d.%d due to no next TNode in TNode %d link %d",
                       vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                       p_vehicle->State, p_vehicle->SubState, p_vehicle->U.UVehicle.Dummy4b, lnk);
                    start_crashing(p_vehicle);
                }

                if (p_vehicle->U.UVehicle.TNode < 0)
                {
                    struct TrafficNode *p_lntnode;

                    p_lntnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                    if (((p_tnode->UTraffic.Flags[lnk] & 0x80) != 0) ||
                      ((p_lntnode->Flags & (TNdF_Unkn0010|TNdF_Unkn0020)) != 0))
                    {
                        nxdist_sq = 0x7FFFFFFF;
                        p_vehicle->U.UVehicle.WorkPlace |= VWPFlg_Unkn0008;
                    }
                }
                if ((p_tnode->UTraffic.Flags[lnk] & 0x01) != 0)
                    p_vehicle->Flag |= TngF_TriggerUse;
                else
                    p_vehicle->Flag &= ~TngF_TriggerUse;
            }
            else
            {
                int i, lnk;
                int dx, dz;

                dx = 0;
                dz = 1;

                for (i = 0; i < 8; i++)
                {
                    struct TrafficNode *p_lntnode;

                    p_vehicle->U.UVehicle.TNode = -dword_1C8474[LbRandomAnyShort() % dword_1C8470];
                    p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];

                    lnk = tnode_find_used_link(p_tnode);
                    if (lnk < 0)
                        break;

                    p_lntnode = &game_traffic_nodes[-p_tnode->UTraffic.Link[lnk]];
                    if ((p_lntnode->Flags & TNdF_Unkn0040) == 0)
                    {
                        dx = (p_lntnode->X << 8) - (p_tnode->X << 8);
                        dz = (p_lntnode->Z << 8) - (p_tnode->Z << 8);
                        p_tnode = &game_traffic_nodes[-p_vehicle->U.UVehicle.TNode];
                        move_mapwho(p_vehicle, p_tnode->X << 8, (p_tnode->Y + 15) << 8, p_tnode->Z << 8);
                        remove_locks(p_vehicle);
                        p_vehicle->U.UVehicle.Agok = p_tnode->UTraffic.Link[lnk];
                        add_agok_lock(p_vehicle);
                        break;
                    }
                }
                if (i >= 8) {
                    LOGERR("Crashing %s %d state %d.%d due to no random next TNode despite %d tries",
                      vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
                      p_vehicle->State, p_vehicle->SubState, i);
                    start_crashing(p_vehicle);
                    return;
                }
                p_vehicle->U.UVehicle.AngleDY = 0;
                p_vehicle->U.UVehicle.AngleY = arctan(dx, dz);
            }
            break;
        }
    }

    update_vehicle_elevation(p_vehicle, tnode);

    update_vehicle_speed_and_direction(p_vehicle, tndt_x, tndt_z, nxdist_sq);

    if (nxdist_sq < 0xC0000)
    {
        process_vehicle_stop_for_pedestrians(p_vehicle);
    }

    vehicle_workplace_states_finish(p_vehicle);
}

/******************************************************************************/
