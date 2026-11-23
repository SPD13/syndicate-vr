/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file thing_fire.c
 *     Support for creating fires burning on the map.
 * @par Purpose:
 *     Implement creation and updating of the representation of fire.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     19 Aug 2025 - 25 Aug 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "thing_fire.h"

#include <stdlib.h>
#include "bfutility.h"

#include "enginlights.h"
#include "enginshrapn.h"
#include "frame_sprani.h"

#include "bigmap.h"
#include "bmbang.h"
#include "difftest.h"
#include "enginfexpl.h"
#include "sound.h"
#include "swlog.h"
#include "thing.h"
#include "vehicle.h"

/******************************************************************************/

void FIRE_add_flame(ThingIdx firetng, ushort fflame)
{
    struct SimpleThing *p_fire;
    struct FireFlame *p_fflame;

    if (fflame == 0) {
        return;
    }
    p_fire = &sthings[firetng];
    p_fflame = &FIRE_flame[fflame];

    p_fflame->next = p_fire->U.UFire.flame;
    p_fire->U.UFire.flame = fflame;
}

void FIRE_new(int x, int y, int z, ubyte type)
{
#if 0
    asm volatile ("call ASM_FIRE_new\n"
        : : "a" (x), "d" (y), "b" (z), "c" (type));
#endif
    struct SimpleThing *p_fire;
    struct MyMapElement *p_mapel;
    int cor_x, cor_z;
    int cor_y;
    ThingIdx firetng;
    ushort fflame;
    short tile_x, tile_z;
    short sib_tl_x, sib_tl_z;
    ubyte flame_count;

    if ((PRCCOORD_TO_MAPCOORD(x) >= MAP_COORD_WIDTH)
      || (PRCCOORD_TO_MAPCOORD(z) >= MAP_COORD_HEIGHT)) {
        return;
    }
    if (word_1E08B8 == 0) {
        return;
    }

    firetng = get_new_sthing();
    if (firetng == 0) {
        return;
    }

    p_fire = &sthings[firetng];
    p_fire->Type = SmTT_FIRE;
    p_fire->U.UFire.flame = 0;
    p_fire->Z = z;
    p_fire->Y = y;
    p_fire->X = x;
    play_dist_ssample(p_fire, 0x10u, 0x7Fu, 0x40u, 100, -1, 1);

    cor_x = x >> 8;
    cor_z = z >> 8;
    cor_y = y;

    switch (type)
    {
    case 1u:
        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 18 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 4, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 24 + (LbRandomAnyShort() & 0xF);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 5, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 2u:
        flame_count = 2;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 10, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 3u:
        tile_z = cor_z >> 8;
        tile_x = cor_x >> 8;
        p_mapel = &game_my_big_map[128 * (z >> 16) + (cor_x >> 8)];
        p_mapel->Flags2 &= ~0x80;

        sib_tl_z = tile_z - 1;
        sib_tl_x = tile_x - 1;
        if ( sib_tl_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x70;
        }
        sib_tl_z = tile_z - 1;
        if ( tile_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + tile_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x50;
        }
        sib_tl_x = tile_x + 1;
        sib_tl_z = tile_z - 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x30;
        }
        sib_tl_x = tile_x + 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && tile_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * tile_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x10;
        }
        sib_tl_z = tile_z + 1;
        sib_tl_x = tile_x + 1;
        if ( (tile_x + 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xF0;
        }
        sib_tl_z = tile_z + 1;
        if ( tile_x <= 0x7F && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + tile_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xD0;
        }
        sib_tl_z = tile_z + 1;
        sib_tl_x = tile_x - 1;
        if ( (tile_x - 1) >= 0 && sib_tl_x <= 127 && sib_tl_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[128 * sib_tl_z + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0xB0;
        }
        sib_tl_x = tile_x - 1;
        if ( sib_tl_x <= 0x7F && tile_z <= 0x7F )
        {
          p_mapel = &game_my_big_map[(tile_z << 7) + sib_tl_x];
          p_mapel->Flags2 &= 0xF;
          p_mapel->Flags2 |= 0x90;
        }

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 100, 6, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 43 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 30, 7, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0xFF, 100, 8, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);

        flame_count = 53 - (LbRandomAnyShort() & 3);
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x1FF, 30, 9, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    case 4u:
        flame_count = 6;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 21, flame_count);
        FIRE_add_flame(firetng, fflame);
        goto LABEL_184;
    case 5u:
LABEL_184:
        flame_count = 12;
        fflame = FIRE_spawn_flame(cor_x, cor_y, cor_z, 0x3F, 30, 21, flame_count);
        FIRE_add_flame(firetng, fflame);
        break;
    default:
        break;
    }
    add_node_sthing(firetng);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void apply_unkn_vehicle50_damage_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_apply_unkn_vehicle50_damage\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void apply_unkn_vehicle50_damage(int a1, int a2, int a3, int a4)
{
    s32 x, z, damage;
    s32 tile_x, tile_z;
    s32 tl_x, tl_z;

    DIFFTEST_SHADOW(apply_unkn_vehicle50_damage, apply_unkn_vehicle50_damage_asm(a1, a2, a3, a4));
    x = a1;
    z = a2;
    damage = a3;
    tile_x = x >> 16;
    tile_z = z >> 16;

    for (tl_x = tile_x - 2; tl_x != tile_x + 3; tl_x++)
    {
        if (tl_x < 0 || tl_x > 127)
            continue;
        for (tl_z = tile_z - 2; tl_z != tile_z + 3; tl_z++)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;

            if (tl_z < 0 || tl_z > 127)
                continue;
            p_mapel = &game_my_big_map[128 * tl_z + tl_x];
            thing = p_mapel->Child;
            while (thing != 0)
            {
                struct Thing *p_thing;
                s32 dx, dz, dist, dmg;

                if (thing < 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                p_thing = &things[thing];
                thing = p_thing->Next;
                if (p_thing->Type != TT_VEHICLE)
                    continue;
                if (p_thing->SubType != SubTT_VEH_FLYING)
                    continue;
                if ((ushort)p_thing->State != 0x3A && (ushort)p_thing->State != 0x41
                  && (ushort)p_thing->State != 0x42 && (ushort)p_thing->State != 0x43)
                    continue;

                dz = abs(p_thing->Z - z);
                dx = abs(p_thing->X - x);
                if (dx >= dz)
                    dist = dx - (dx >> 5) - (dx >> 7) + (dz >> 2) + (dz >> 3) + (dz >> 6) + (dz >> 7);
                else
                    dist = dz - (dz >> 5) - (dz >> 7) + (dx >> 2) + (dx >> 3) + (dx >> 6) + (dx >> 7);

                dmg = ((600 - (dist >> 8)) * damage) >> 9;
                if (dmg <= 0)
                    continue;
                p_thing->U.UVehicle.WorkPlace |= VWPFlg_Unkn0080;
                // TODO SubType is always SubTT_VEH_FLYING here, so these reductions never apply
                if (p_thing->SubType == SubTT_VEH_TANK)
                    dmg >>= 1;
                if (p_thing->SubType == SubTT_VEH_MECH)
                    dmg >>= 2;
                p_thing->Health -= (short)dmg;
                if (p_thing->Health > 0)
                    continue;
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
    }
}


struct SimpleThing *init_nuclear_bomb(MapCoord x, MapCoord y, MapCoord z);

static DIFFTEST_REF_FUNC void set_static_on_fire_asm(struct SimpleThing *p_static)
{
    DIFFTEST_ASM(
      "call ASM_set_static_on_fire\n"
        :  : "a" (p_static));
}

void set_static_on_fire(struct SimpleThing *p_static)
{
    struct FullLight *p_fulights;
    struct SimpleThing *p_sthing;
    ushort light, cmd, n;
    ushort sframe;
    int damage;

    DIFFTEST_SHADOW(set_static_on_fire, set_static_on_fire_asm(p_static));
    p_fulights = game_full_lights;
    if ((uintptr_t)p_static < (uintptr_t)sthings - STHINGS_LIMIT * sizeof(struct SimpleThing))
        return;
    if ((uintptr_t)p_static > (uintptr_t)sthings)
        return;

    p_static->State = 0x24;
    p_static->StartTimer1 = 10;
    p_static->Flag |= TngF_Destroyed;

    // Switch off lights attached to the static
    light = p_static->U.UScenery.LightHead;
    n = 0;
    while (light != 0)
    {
        struct FullLight *p_fulight;

        if (n >= 1000)
            break;
        n++;
        if (light >= next_full_light)
            break;
        p_fulight = &p_fulights[light];
        cmd = p_fulight->Command;
        p_fulight->Intensity = 0;
        if ((cmd <= next_light_command) && (cmd != 0))
            game_light_commands[cmd].lcfld_0[1] = 5;
        light = p_fulights[light].NextFull;
    }

    sframe = p_static->StartFrame;
    if ((sframe >= 0x424) && (sframe <= 0x428))
    {
        p_static->State = 0xD;
        game_full_lights = p_fulights;
        p_static->Timer1 = 0;
        return;
    }

    if ((sframe > 0x400) && (sframe < 0x405))
    {
        game_full_lights = p_fulights;
        p_static->Timer1 = 100;
        switch (sframe)
        {
        case 0x401:
            p_sthing = init_nuclear_bomb(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8);
            if (p_sthing != NULL)
            {
                p_sthing->Type = SmTT_CANISTER;
                p_sthing->SubType = 4;
                play_dist_ssample(p_sthing, 0x20, 0x7F, 0x40, 100, -1, 3);
            }
            break;
        case 0x402:
            p_sthing = init_nuclear_bomb(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8);
            if (p_sthing != NULL)
            {
                p_sthing->Type = SmTT_CANISTER;
                p_sthing->SubType = 5;
                play_dist_ssample(p_sthing, 0x1F, 0x7F, 0x40, 100, -1, 3);
            }
            break;
        case 0x403:
            p_sthing = init_nuclear_bomb(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8);
            if (p_sthing != NULL)
            {
                p_sthing->Timer1 = 0x13;
                p_sthing->Radius = -5;
            }
            break;
        case 0x404:
            play_dist_ssample(p_static, 0x25, 0x7F, 0x40, 100, 0, 3);
            bang_new4(p_static->X, p_static->Y, p_static->Z, 20);
            do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8,
              0x300, 200, NULL);
            break;
        }
        p_static->StartFrame = 0x422;
        p_static->Frame = nstart_ani[1058];
        return;
    }

    game_full_lights = p_fulights;
    if ((sframe > 0x3EC) && (sframe < 0x3F0))
    {
        p_sthing = create_scale_effect(p_static->X >> 8, p_static->Y >> 8,
          p_static->Z >> 8, 0x39C, 40);
        if (p_sthing != NULL)
        {
            p_sthing->SubType = 0x39;
            p_sthing->Object = 0;
            p_sthing->StartFrame = 0;
            p_sthing->U.UEffect.VX = 0;
            p_sthing->U.UEffect.VY = 0;
            p_sthing->U.UEffect.VZ = 0;
            p_sthing->U.UEffect.OX = p_static->X >> 8;
            p_sthing->U.UEffect.OY = p_static->Y >> 8;
            p_sthing->U.UEffect.OZ = p_static->Z >> 8;
        }
        remove_sthing(p_static->ThingOffset);
        delete_snode(p_static);
        return;
    }

    if ((sframe == 0x3E7) || (sframe == 0x3EA) || (sframe == 0x3EC) ||
      (sframe == 0x3F0) || (sframe == 0x408) || (sframe == 0x40D) ||
      (sframe == 0x40E) || (sframe == 0x41A))
    {
        play_dist_ssample(p_static, 0x36, 0x7F, 0x40, 100, 0, 1);
        p_static->State = 0xC;
        p_static->Timer1 = 9;
        sframe = p_static->StartFrame;
        p_static->State = 0xC;
        if ((sframe == 0x40E) || (sframe == 0x3EC))
        {
            bang_new4(p_static->X, p_static->Y, p_static->Z, 120);
            damage = 2500;
        }
        else
        {
            bang_new4(p_static->X, p_static->Y, p_static->Z, 115);
            damage = 5000;
        }
        apply_unkn_vehicle50_damage(p_static->X, p_static->Z, damage, 0);
        p_static->StartFrame = 0x41C;
        do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8,
          0x300, 100, NULL);
        return;
    }

    if ((sframe == 0x3E9) || (sframe == 0x406) || (sframe == 0x40A) ||
      (sframe == 0x40B) || (sframe == 0x417) || (sframe == 0x419))
    {
        p_static->Timer1 = 40;
        p_static->State = 0xC;
        sframe = p_static->StartFrame;
        switch (sframe)
        {
        case 0x3E9:
            p_static->StartFrame = 0x425;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            break;
        case 0x40A:
            p_static->StartFrame = 0x428;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x40B:
            p_static->StartFrame = 0x426;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x417:
            p_static->StartFrame = 0x429;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 2);
            break;
        case 0x419:
            p_static->StartFrame = 0x427;
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            FIRE_new(p_static->X, p_static->Y >> 5, p_static->Z, 4);
            break;
        }
        return;
    }

    play_dist_ssample(p_static, 0x25, 0x3C, 0x40, 100, 0, 3);
    bang_new4(p_static->X, p_static->Y, p_static->Z, 20);
    do_shockwave(p_static->X >> 8, p_static->Y >> 8, p_static->Z >> 8,
      0x300, 100, NULL);
    p_static->Timer1 = 40;
    remove_sthing(p_static->ThingOffset);
    delete_snode(p_static);
}

/******************************************************************************/
