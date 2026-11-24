/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_world.c
 *     Setting up and running the game world of a mission.
 * @par Purpose:
 *     Level and mission setup, map lights and textures, trains, the camera
 *     start and the mission tests: the parts of the former game.c that the
 *     game rules use (part of the game core, libswarscore).
 * @par Comment:
 *     Moved from game.c unchanged; the includes are still those of game.c.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bfconfig.h"
#include "bfcircle.h"
#include "bfdata.h"
#include "bfendian.h"
#include "bfsprite.h"
#include "bfscreen.h"
#include "bfkeybd.h"
#include "bfmouse.h"
#include "bftext.h"
#include "bfbox.h"
#include "bfpalette.h"
#include "bfmemory.h"
#include "bfmemut.h"
#include "bfstrut.h"
#include "bffile.h"
#include "bffont.h"
#include "bfgentab.h"
#include "bfwindows.h"
#include "bfpng.h"
#include "bfutility.h"
#include "bfsvaribl.h"
#include "bfmath.h"
#include "bfline.h"
#include "bfaudio.h"
#include "bfmusic.h"
#include "bfsound.h"
#include "bfdanger.h"
#include "bfplanar.h"
#include "bfscrsurf.h"
#include "bfscrcopy.h"
#include "bfstrut.h"
#include "bfsmack.h"
#include "bftringl.h"
#include "bfscd.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "engincam.h"
#include "engincolour.h"
#include "enginprops.h"
#include "engintxtrmap.h"

#include "bat.h"
#include "bmbang.h"
#include "devcreds.h"
#include "difftest.h"
#include "svesa.h"
#include "swlog.h"
#include "bflib_vidraw.h"
#include "bfjoyst.h"
#include "ssampply.h"
#include "matrix.h"
#include "dos.h"
#include "drawshape.h"
#include "drawtext.h"
#include "drawtext_wrp.h"
#include "embedanim.h"
#include "enginbckt.h"
#include "engindrwlstm_wrp.h"
#include "engindrwlstx.h"
#include "engindrwlstx_tng.h"
#include "enginfexpl.h"
#include "enginfloor.h"
#include "enginlights.h"
#include "enginpriobjs.h"
#include "enginpritxtr.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "enginpeff.h"
#include "enginshadws.h"
#include "engintrns.h"
#include "enginzoom.h"
#include "game_data.h"
#include "game_options.h"
#include "game_save.h"
#include "game_sprts.h"
#include "guiboxes.h"
#include "guitext.h"
#include "feappbar.h"
#include "femail.h"
#include "femain.h"
#include "febrief.h"
#include "fedebrief.h"
#include "fecntrls.h"
#include "fecryo.h"
#include "feequip.h"
#include "felogin.h"
#include "fenet.h"
#include "feoptions.h"
#include "fepause.h"
#include "fepanet.h"
#include "feresearch.h"
#include "feshared.h"
#include "festorage.h"
#include "feworld.h"
#include "fmvplay.h"
#include "hud_panecfg.h"
#include "hud_panel.h"
#include "purpldrw.h"
#include "purpldrwlst.h"
#include "building.h"
#include "campaign.h"
#include "crtrand.h"
#include "cybmod.h"
#include "packet.h"
#include "pathtrig.h"
#include "pepgroup.h"
#include "lvdraw3d.h"
#include "lvobjctv.h"
#include "lvfiles.h"
#include "bigmap.h"
#include "bmcrater.h"
#include "difftest.h"
#include "display.h"
#include "dos.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_data.h"
#include "game_speed.h"
#include "hud_target.h"
#include "keyboard.h"
#include "misstat.h"
#include "mouse.h"
#include "mydraw.h"
#include "network.h"
#include "sound.h"
#include "swcore_game.h"
#include "osunix.h"
#include "oswindws.h"
#include "util.h"
#include "command.h"
#include "packetfe.h"
#include "people.h"
#include "player.h"
#include "plyr_net.h"
#include "plyr_usrinp.h"
#include "plyr_packet.h"
#include "research.h"
#include "rules.h"
#include "scandraw.h"
#include "thing.h"
#include "thing_view.h"
#include "thing_expld.h"
#include "thing_fire.h"
#include "thing_search.h"
#include "thing_onface.h"
#include "thing_ovmous.h"
#include "tngcolisn.h"
#include "tngobjdrw.h"
#include "vehicle.h"
#include "vehtraffic.h"
#include "wadfile.h"
#include "weapon.h"
#include "wrcities.h"
#include "game_world.h"

/** Length of train railcar for carrying passengers, the 3D object.
 */
#define TRAIN_CARRY_LENGTH 550

/** Arc tangent factors table from bflibrary; identical to the asm table. */
extern unsigned short const lbArcTanFactors[];

extern s32 dword_152E38[5];
extern s32 dword_1DDECC;
extern ushort data_1c8468;
extern short data_1c42fe;
extern short data_1c4300;
extern ushort word_1C83E2;
extern ushort word_1C83E8;

int is_animating(int texture);

extern ubyte map_crater_first;
extern ubyte map_crater_next;
extern struct MapCreater map_craters[];

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
s32 dword_152E38[5] = {-1, -1, -1, -1, -1};
s32 unkn01_downcount;
s32 unkn01_pos_x;
s32 unkn01_pos_y;
short data_1c42fe;
short data_1c4300;
ubyte map_crater_first;
ushort word_1C83E2;
ushort word_1C83E8;
ushort word_1C8446;
ushort current_map;
short current_level;
ushort data_1c8468;
#endif /* typed asm data */

/** Macro for returning given array of elements in random order.
 *
 * Provided input and output arrays, type of element and count of elements,
 * this routine will randomize the order of elements.
 */
#define array_elements_in_random_order(out_nubers, in_nubers, elem_type, count) \
{ \
    ushort pos, remain, next; \
    elem_type nums[count]; \
    memcpy(nums, in_nubers, sizeof(elem_type)*count); \
    remain = count; \
    for (pos = 0; pos < count; pos++) \
    { \
        elem_type nbak; \
        next = LbRandomAnyShort() % remain; \
        remain--; \
        out_nubers[pos] = nums[next]; \
        nbak = nums[remain]; \
        nums[remain] = nums[next]; \
        nums[next] = nbak; \
    } \
}

u32 stored_l3d_next_object;
u32 stored_l3d_next_object_face3;
u32 stored_l3d_next_object_face4;
u32 stored_l3d_next_object_point;
u32 stored_l3d_next_normal;
u32 stored_l3d_next_face_texture;
u32 stored_l3d_next_floor_texture;
u32 stored_l3d_next_local_mat;
u32 stored_level3d_inuse;
s32 gamep_unknval_10 = 0;
s32 gamep_unknval_11 = 0;
s32 gamep_unknval_12 = 0;
u32 nav_stats__ThisTurn = 0;
s32 gamep_unknval_14 = 0;
s32 gamep_unknval_15 = 0;
s32 gamep_unknval_16 = 0;
ushort word_1AABD0 = 0;
const char *primvehobj_fname = "qdata/primveh.obj";

/******************************************************************************/

static DIFFTEST_REF_FUNC short arctan_asm(int dx, int dz)
{
    short ret;
    DIFFTEST_ASM("call ASM_arctan\n"
        : "=a" (ret) : "a" (dx), "d" (dz));
    return ret;
}

short arctan(int dx, int dz)
{
    s32 a, b;

    DIFFTEST_SHADOW_RET(arctan, short, arctan_asm(dx, dz));
    a = dx;
    b = dz;
    if ((a == 0) && (b == 0))
        DIFFTEST_RETURN((short)0);
    if (a >= 0)
    {
        if (b >= 0)
        {
            if (a >= b)
                DIFFTEST_RETURN((short)(0x200 + lbArcTanFactors[((u32)b << 8) / (u32)a]));
            else
                DIFFTEST_RETURN((short)(0x400 - lbArcTanFactors[((u32)a << 8) / (u32)b]));
        }
        else
        {
            b = (s32)(-(u32)b);
            if (a >= b)
                DIFFTEST_RETURN((short)(0x200 - lbArcTanFactors[((u32)b << 8) / (u32)a]));
            else
                DIFFTEST_RETURN((short)lbArcTanFactors[((u32)a << 8) / (u32)b]);
        }
    }
    else
    {
        a = (s32)(-(u32)a);
        if (b >= 0)
        {
            if (a >= b)
                DIFFTEST_RETURN((short)(0x600 - lbArcTanFactors[((u32)b << 8) / (u32)a]));
            else
                DIFFTEST_RETURN((short)(0x400 + lbArcTanFactors[((u32)a << 8) / (u32)b]));
        }
        else
        {
            b = (s32)(-(u32)b);
            if (a >= b)
                DIFFTEST_RETURN((short)(0x600 + lbArcTanFactors[((u32)b << 8) / (u32)a]));
            else
                DIFFTEST_RETURN((short)(0x800 - lbArcTanFactors[((u32)a << 8) / (u32)b]));
        }
    }
}

/** Remains of some Bf debug stuff.
 */
void debug_trace_setup(int place)
{
    LOGDBG("reached place %d", place);
}

void player_debug(const char *text)
{
    // TODO place debug/verification code
}

void fill_floor_textures(void)
{
    struct SingleFloorTexture *p_fltextr;
    uint i;

    for (i = 1; i < next_floor_texture; i++)
    {
        p_fltextr = &game_textures[i];
        if (p_fltextr->Page == 5)
        {
            p_fltextr->TMapX1 += 64;
            p_fltextr->TMapX2 += 64;
            p_fltextr->TMapX3 += 64;
            p_fltextr->TMapX4 += 64;
            p_fltextr->TMapY1 -= 96;
            p_fltextr->TMapY2 -= 96;
            p_fltextr->TMapY3 -= 96;
            p_fltextr->TMapY4 -= 96;
            p_fltextr->Page = 4;
        }
    }

    for (i = 1; i < prim4_textures_count; i++)
    {
        p_fltextr = &prim4_textures[i];
        if (p_fltextr->Page == 5)
        {
            p_fltextr->TMapX1 += 64;
            p_fltextr->TMapX2 += 64;
            p_fltextr->TMapX3 += 64;
            p_fltextr->TMapX4 += 64;
            p_fltextr->TMapY1 -= 96;
            p_fltextr->TMapY2 -= 96;
            p_fltextr->TMapY3 -= 96;
            p_fltextr->TMapY4 -= 96;
            p_fltextr->Page = 4;
        }
    }
}

void fill_netgame_agent_pos(int plyr, int group, int num_agents)
{
    ThingIdx thing;
    struct Thing *p_thing;
    short plagent;

    plagent = 0;
    for (thing = things_used_head; thing != 0; thing = p_thing->LinkChild)
    {
        p_thing = &things[thing];

        if (p_thing->Type != TT_PERSON)
            continue;
        if (p_thing->U.UPerson.Group != group)
            continue;

        netgame_agent_pos_x[plyr][plagent] = PRCCOORD_TO_MAPCOORD(p_thing->X);
        netgame_agent_pos_z[plyr][plagent] = PRCCOORD_TO_MAPCOORD(p_thing->Z);
        plagent++;
        if (plagent == num_agents)
            break;
    }
    if (plagent < num_agents) {
        LOGERR("Not enough agents discovered on level");
    }
}

/** Change flags of commands containing "until".
 * This seem to just block execution of commands. Maybe it was
 * used to one-time fix some specific levels?
 * Remove pending.
 */
void unkn_f_pressed_func(void)
{
    struct Thing *p_person;
    ThingIdx person;
    short i;

    person = get_thing_same_type_head(TT_PERSON, -1);
    for (i = 0; person > 0; person = p_person->LinkSame, i++)
    {
        ushort cmd;
        struct Command *p_cmd_prev;

        if (i >= THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
        p_person = &things[person];
        cmd = p_person->U.UPerson.ComHead;
        p_cmd_prev = NULL;
        while (cmd != 0)
        {
            struct Command *p_cmd;

            p_cmd = &game_commands[cmd];
            if (!is_command_any_until(p_cmd))
            {
                if (p_cmd_prev != NULL)
                    p_cmd_prev->Flags |= PCmdF_RunUntil;
                p_cmd->Flags |= PCmdF_IsUntil;
            }
            p_cmd_prev = p_cmd;
            cmd = p_cmd->Next;
        }
    }
}

void load_level_wrap(short level, short missi, ubyte reload)
{
    LbMouseChangeSprite(NULL);
    if (0) { // No need to conserve memory to such extent - mem_game[] was changed
        // Optimization for memory conservation - reserve no space for game_commands,
        // and instead re-use some of triangulation area during map load.
        void **p = mem_game[34].BufferPtr;
        *p = dword_177750;
        mem_game[34].N = 3100;
    }
    read_primveh_obj(primvehobj_fname, 1);

    load_level_pc(level, missi, reload);

    // No idea what exactly pressing F during level load does
    if (is_key_pressed(KC_F, KMod_DONTCARE))
        unkn_f_pressed_func();
}

static DIFFTEST_REF_FUNC void update_map_thing_and_traffic_refs_asm(void)
{
    DIFFTEST_ASM("call ASM_update_map_thing_and_traffic_refs\n"
        :  :  : "eax" );
}

void update_map_thing_and_traffic_refs(void)
{
    ThingIdx thing;
    ushort tnode;
    ushort face;
    ushort i;

    DIFFTEST_SHADOW(update_map_thing_and_traffic_refs,
      update_map_thing_and_traffic_refs_asm());

    // Convert building link indexes into thing indexes
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing;

        p_thing = &things[thing];
        if (p_thing->Type != TT_BUILDING)
            continue;
        if (p_thing->SubType == SubTT_BLD_BEZIER_ROAD)
        {
            if (p_thing->U.UObject.PrevThing > 0)
                p_thing->U.UObject.PrevThing = search_things_for_index(p_thing->U.UObject.PrevThing);
            if (p_thing->U.UObject.NextThing > 0)
                p_thing->U.UObject.NextThing = search_things_for_index(p_thing->U.UObject.NextThing);
        }
        else
        {
            if (p_thing->U.UObject.PrevThing != 0)
                p_thing->U.UObject.PrevThing = search_things_for_index(p_thing->U.UObject.PrevThing);
            if (p_thing->U.UObject.NextThing != 0)
                p_thing->U.UObject.NextThing = search_things_for_index(p_thing->U.UObject.NextThing);
        }
    }

    // Convert traffic link indexes in simple things
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;

        p_sthing = &sthings[thing];
        if (p_sthing->Type != SmTT_TRAFFIC)
            continue;
        for (i = 0; i < 4; i++)
        {
            if (p_sthing->U.UTraffic.Link[i] != 0)
                p_sthing->U.UTraffic.Link[i] = search_things_for_index(p_sthing->U.UTraffic.Link[i]);
        }
    }

    // Convert link indexes in traffic nodes
    for (tnode = 1; tnode < next_traffic_node; tnode++)
    {
        for (i = 0; i < 4; i++)
        {
            short link;

            link = game_traffic_nodes[tnode].UTraffic.Link[i];
            if (link > 0)
                game_traffic_nodes[tnode].UTraffic.Link[i] = search_things_for_index(link);
        }
        if (game_traffic_nodes[tnode].GateLink != 0)
        {
            short link;

            link = game_traffic_nodes[tnode].GateLink;
            game_traffic_nodes[tnode].GateLink = search_things_for_index(link);
        }
    }

    // Store own index in ThingOffset
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing;

        p_thing = &things[thing];
        if (p_thing->Type == TT_BUILDING)
            p_thing->ThingOffset = thing;
    }

    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;

        p_sthing = &sthings[thing];
        if (p_sthing->Type == SmTT_TRAFFIC)
            p_sthing->ThingOffset = thing;
    }

    // Update token of the first building with the flag set
    for (thing = things_used_head; thing > 0; thing = things[thing].LinkChild)
    {
        struct Thing *p_thing;

        p_thing = &things[thing];
        if ((p_thing->Type == TT_BUILDING) && ((p_thing->Flag & TngF_InVehicle) != 0))
        {
            p_thing->U.UObject.Token = 3;
            p_thing->U.UObject.TokenDir = 1;
            break;
        }
    }

    for (face = 1; face < next_object_face4; face++)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[face];
        if ((p_face4->GFlags & FGFlg_Unkn10) != 0)
            p_face4->WalkHeader = 0;
    }
}

static DIFFTEST_REF_FUNC void unkn_lights_processing_asm(void)
{
    DIFFTEST_ASM("call ASM_unkn_lights_processing\n"
        :  :  : "eax" );
}

void unkn_lights_processing(void)
{
    struct FullLight *p_fulights;
    ushort n_cmds;
    ushort cmd, i;

    DIFFTEST_SHADOW(unkn_lights_processing, unkn_lights_processing_asm());
    n_cmds = next_light_command;
    p_fulights = game_full_lights;

    // Clear references to commands beyond the list end
    for (i = 0; i < next_full_light; i++)
    {
        if ((ushort)p_fulights[i].Command > n_cmds)
            p_fulights[i].Command = 0;
    }

    // Remove commands which reference non-existing lights
    for (cmd = 1; cmd < n_cmds; cmd++)
    {
        s32 light;
        ushort k;

        light = (*(s32 *)&game_light_commands[cmd].lcfld_0[0x10]) >> 16;
        if (light >= 0)
        {
            if (light <= (s32)next_full_light)
                continue;
        }
        else if (light >= -1500)
        {
            ushort head;

            head = sthings[light].U.UScenery.LightHead;
            if ((head != 0) && (head <= next_full_light))
                continue;
        }

        for (i = 1; i < next_full_light; i++)
        {
            if (cmd < (ushort)p_fulights[i].Command)
                p_fulights[i].Command--;
        }
        for (k = cmd; k < n_cmds; k++)
        {
            game_light_commands[k] = game_light_commands[k + 1];
        }
        cmd--;
        n_cmds--;
    }

    game_full_lights = p_fulights;
    next_light_command = n_cmds;
}

void change_current_map(ushort mapno)
{
    LbMouseChangeSprite(NULL);
    current_map = mapno;
    init_things();
    load_mad_pc(mapno);
    fill_floor_textures();

    if (current_map == 11) // map011 Orbital Station
        render_floor_flags |= RendFlrF_NonPlanetary;
    else
        render_floor_flags &= ~RendFlrF_NonPlanetary;

    if (current_map == 9) // map009 Singapore on-water map
        render_floor_flags |= RendFlrF_WobblyTerrain;
    else
        render_floor_flags &= ~RendFlrF_WobblyTerrain;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int can_the_tank_run_over_asm(int a1, int a2)
{
    int ra = a1, rd = a2;
    DIFFTEST_ASM("call ASM_can_the_tank_run_over\n"
        : "+a" (ra), "+d" (rd) : : "ebx", "ecx", "esi", "edi", "cc", "memory");
    return ra;
}

int can_the_tank_run_over(struct SimpleThing *p_sthing)
{
    ushort frame;

    DIFFTEST_SHADOW_RET(can_the_tank_run_over, int, can_the_tank_run_over_asm((intptr_t)p_sthing, 0));
    frame = (ushort)p_sthing->StartFrame;
    if ((frame > 1024) && (frame < 1029))
        DIFFTEST_RETURN((int)1);
    if ((frame > 1004) && (frame < 1008))
        DIFFTEST_RETURN((int)1);
    switch (frame)
    {
    case 999:
    case 1002:
    case 1004:
    case 1008:
    case 1032:
    case 1037:
    case 1038:
    case 1050:
        DIFFTEST_RETURN((int)1);
    case 1001:
    case 1030:
    case 1034:
    case 1035:
    case 1047:
    case 1049:
        DIFFTEST_RETURN((int)1);
    default:
        break;
    }
    DIFFTEST_RETURN((int)0);
}

void net_player_colors_reassign(void)
{
    s32 npcolors[5];
    ubyte incolors[8];
    ushort plyr;
    ushort incol_idx;

    //TODO would probalny make more sense to memset with -1, as the source is never written to
    LbMemoryCopy(npcolors, dword_152E38, sizeof(npcolors));
    incolors[0] = colour_lookup[ColLU_GREEN];
    incolors[1] = colour_lookup[ColLU_RED];
    incolors[2] = colour_lookup[ColLU_WHITE];
    incolors[3] = colour_lookup[ColLU_PINK];
    incolors[4] = colour_lookup[ColLU_CYAN];
    incolors[5] = colour_lookup[ColLU_YELLOW];
    incolors[6] = colour_lookup[ColLU_GREYMD];
    incolors[7] = colour_lookup[ColLU_BLACK];

    incol_idx = 0;
    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        ubyte v12;
        TbPixel c;

        if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
            continue;

        v12 = net_player_teams[plyr];
        if (v12 == 0)
        {
            c = incolors[incol_idx];
            net_player_colours[plyr] = c;
            incol_idx++;
        }
        else if (npcolors[v12] == -1)
        {
            c = incolors[incol_idx];
            net_player_colours[plyr] = c;
            npcolors[v12] = c;
            incol_idx++;
        }
        else
        {
            c = npcolors[v12];
            net_player_colours[plyr] = c;
        }
    }
}

void unkn_truce_groups(void)
{
    unkn_truce_groups_sub1();
    net_player_colors_reassign();
}

void blind_progress_game(u32 nturns)
{
    u32 n;

    for (n = 0; n < nturns; n++)
    {
        process_things();
        process_explode();
        gameturn++;
    }
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void unkn_obj_faces4_lights_func_10_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_unkn_obj_faces4_lights_func_10\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Scales the Ratio of every quick light in the chain starting at given index,
 * clearing NextQuick links which point beyond the used lights.
 */
static void face_point_lights_scale(ushort qlight, s32 intens)
{
    while ((qlight != 0) && (qlight < next_quick_light))
    {
        struct QuickLight *p_qlight;

        p_qlight = &game_quick_lights[qlight];
        p_qlight->Ratio = (intens * (s32)p_qlight->Ratio) >> 8;
        if (p_qlight->NextQuick >= next_quick_light)
            p_qlight->NextQuick = 0;
        qlight = game_quick_lights[qlight].NextQuick;
    }
}

/** Darkens one point of an object face: scales the lights chained to it and
 * its shade by intensity (clipped to 0..256) * 140/256 / 256.
 * Positive face is an index into game_object_faces3, negative into
 * game_object_faces4. The 4th register (ecx) is not used.
 */
void unkn_obj_faces4_lights_func_10(int face_idx, int point, int intensity)
{
    short face;
    ushort pt;
    s32 intens;

    DIFFTEST_SHADOW(unkn_obj_faces4_lights_func_10,
      unkn_obj_faces4_lights_func_10_asm(face_idx, point, intensity, 0));
    face = (short)face_idx;
    pt = (ushort)point;
    intens = intensity;
    if (intens < 0)
        intens = 0;
    if (intens > 0x100)
        intens = 0x100;
    intens = (intens * 140) >> 8;

    if (face > 0)
    {
        struct SingleObjectFace3 *p_face3;

        p_face3 = &game_object_faces3[face];
        switch (pt)
        {
        case 0:
            face_point_lights_scale(p_face3->Light0, intens);
            p_face3->Shade0 = (intens * (s32)(ushort)p_face3->Shade0) >> 8;
            break;
        case 1:
            face_point_lights_scale(p_face3->Light1, intens);
            p_face3->Shade1 = (intens * (s32)(ushort)p_face3->Shade1) >> 8;
            break;
        case 2:
            face_point_lights_scale(p_face3->Light2, intens);
            p_face3->Shade2 = (intens * (s32)(ushort)p_face3->Shade2) >> 8;
            break;
        default:
            break;
        }
    }
    else if (face < 0)
    {
        struct SingleObjectFace4 *p_face4;

        p_face4 = &game_object_faces4[(short)-face];
        switch (pt)
        {
        case 0:
            face_point_lights_scale(p_face4->Light0, intens);
            p_face4->Shade0 = (intens * (s32)(ushort)p_face4->Shade0) >> 8;
            break;
        case 1:
            face_point_lights_scale(p_face4->Light1, intens);
            p_face4->Shade1 = (intens * (s32)(ushort)p_face4->Shade1) >> 8;
            break;
        case 2:
            face_point_lights_scale(p_face4->Light2, intens);
            p_face4->Shade2 = (intens * (s32)(ushort)p_face4->Shade2) >> 8;
            break;
        case 3:
            face_point_lights_scale(p_face4->Light3, intens);
            p_face4->Shade3 = (intens * (s32)(ushort)p_face4->Shade3) >> 8;
            break;
        default:
            break;
        }
    }
}

static DIFFTEST_REF_FUNC void map_lights_update_asm(void)
{
    DIFFTEST_ASM("call ASM_map_lights_update\n"
        :  :  : "eax" );
}

/** Darkens a map tile near the map edge: scales its Ambient by intens/256,
 * and either scales or clears the Ratio of the quick lights chained to it.
 */
static void map_lights_edge_tile(struct MyMapElement *p_mapel, s32 intens,
  TbBool scale_lights)
{
    ushort qlight;

    if ((short)intens < 0)
        intens = 0;
    if ((short)intens > 256)
        intens = 256;
    intens = (short)intens;
    p_mapel->Ambient = (intens * (s32)p_mapel->Ambient) >> 8;
    for (qlight = p_mapel->Shade; qlight != 0; qlight = game_quick_lights[qlight].NextQuick)
    {
        struct QuickLight *p_qlight;

        if (qlight >= next_quick_light)
            break;
        p_qlight = &game_quick_lights[qlight];
        if (scale_lights)
            p_qlight->Ratio = ((s32)p_qlight->Ratio * intens) >> 8;
        else
            p_qlight->Ratio = 0;
    }
}

static TbBool map_lights_face_object_is_lit_building(ushort obj)
{
    struct Thing *p_thing;
    s32 thing;

    thing = game_objects[obj].ThingNo;
    if ((thing <= 0) || (thing >= 1000))
        return false;
    p_thing = &things[thing];
    if (p_thing->Type != TT_BUILDING)
        return false;
    return (p_thing->SubType != SubTT_BLD_MGUN);
}

static void map_lights_face_point_edge(short face, short pt, s32 cor_x, s32 cor_z)
{
    // The asm leaves the point index in ecx as well
    if (cor_x < 0x600)
        unkn_obj_faces4_lights_func_10(face, pt, cor_x - 0x100);
    if (cor_x > 0x7a00)
        unkn_obj_faces4_lights_func_10(face, pt, 0x500 - (cor_x - 0x7900));
    if (cor_z < 0x600)
        unkn_obj_faces4_lights_func_10(face, pt, cor_z - 0x100);
    if (cor_z > 0x7a00)
        unkn_obj_faces4_lights_func_10(face, pt, 0x500 - (cor_z - 0x7900));
}

void map_lights_update(void)
{
    ushort i, j;
    ushort face;

    DIFFTEST_SHADOW(map_lights_update, map_lights_update_asm());
    for (j = 0; j < 6; j++)
    {
        for (i = 0; i < 128; i++)
            map_lights_edge_tile(&game_my_big_map[(i << 7) + j],
              70 * ((s32)j - 1), true);
    }
    for (j = 123; j < 128; j++)
    {
        for (i = 0; i < 128; i++)
            map_lights_edge_tile(&game_my_big_map[(i << 7) + j],
              140 - 70 * ((s32)j - 123), false);
    }
    for (i = 0; i < 128; i++)
    {
        for (j = 122; j < 128; j++)
            map_lights_edge_tile(&game_my_big_map[(j << 7) + i],
              70 * (5 - ((s32)j - 121)), false);
    }
    for (i = 0; i < 128; i++)
    {
        for (j = 0; j < 6; j++)
            map_lights_edge_tile(&game_my_big_map[(j << 7) + i],
              70 * ((s32)j - 1), false);
    }

    for (face = 1; face < next_object_face3; face++)
    {
        struct SingleObjectFace3 *p_face;
        ushort pt;

        p_face = &game_object_faces3[face];
        if (!map_lights_face_object_is_lit_building(p_face->Object))
            continue;
        for (pt = 0; pt < 3; pt++)
        {
            struct SinglePoint *p_point;
            struct SingleObject *p_sobj;
            s32 cor_x, cor_z;

            p_face = &game_object_faces3[face];
            p_point = &game_object_points[(ushort)p_face->PointNo[pt]];
            p_sobj = &game_objects[p_face->Object];
            cor_x = p_point->X + p_sobj->MapX;
            cor_z = p_point->Z + p_sobj->MapZ;
            map_lights_face_point_edge((short)face, (short)pt, cor_x, cor_z);
        }
    }

    for (face = 1; face < next_object_face4; face++)
    {
        struct SingleObjectFace4 *p_face;
        ushort pt;

        p_face = &game_object_faces4[face];
        if (!map_lights_face_object_is_lit_building(p_face->Object))
            continue;
        for (pt = 0; pt < 4; pt++)
        {
            struct SinglePoint *p_point;
            struct SingleObject *p_sobj;
            s32 cor_x, cor_z;

            p_face = &game_object_faces4[face];
            p_point = &game_object_points[(ushort)p_face->PointNo[pt]];
            p_sobj = &game_objects[p_face->Object];
            cor_x = p_point->X + p_sobj->MapX;
            cor_z = p_point->Z + p_sobj->MapZ;
            map_lights_face_point_edge((short)(-(s32)face), (short)pt, cor_x, cor_z);
        }
    }
}

static DIFFTEST_REF_FUNC int train_related_func_01_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_train_related_func_01\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label); the train thing comes in eax, the other
 * registers are not used.
 */
int train_related_func_01(struct Thing *p_train)
{
    struct Thing *p_station;
    ThingIdx station;
    short face1, face2;

    DIFFTEST_SHADOW_RET(train_related_func_01, int,
      train_related_func_01_asm((intptr_t)p_train, 0, 0, 0));
    station = search_for_station(p_train->X >> 8, p_train->Z >> 8);
    if (station == 0)
        DIFFTEST_RETURN((int)0);
    p_station = &things[station];
    face1 = search_object_for_qface(p_station->U.UObject.Object, 4, 2, 0);
    if ((ushort)face1 == 0)
        DIFFTEST_RETURN((int)0);
    // TODO the train is a vehicle, but the asm stores the faces at the
    // offsets of UObject.ComHead/ComCur (UVehicle.AngleZ/GotoX)
    p_train->U.UVehicle.AngleZ = face1;
    p_station->U.UObject.ComHead = face1;
    face2 = search_object_for_qface(p_station->U.UObject.Object, 4, 2,
      (ushort)face1);
    if ((ushort)face2 == 0)
        DIFFTEST_RETURN((int)0);
    p_train->U.UVehicle.GotoX = face2;
    p_station->U.UObject.ComCur = face2;
    p_station->GotoThingIndex = p_train->ThingOffset;
    DIFFTEST_RETURN((int)1);
}

static DIFFTEST_REF_FUNC void func_74934_asm(void)
{
    DIFFTEST_ASM("call ASM_func_74934\n"
        :  :  : "eax" );
}

void func_74934(void)
{
    short tile_x, tile_z;

    DIFFTEST_SHADOW(func_74934, func_74934_asm());
    for (tile_x = 0; tile_x < 128; tile_x++)
    {
        for (tile_z = 0; tile_z < 128; tile_z++)
        {
            ThingIdx thing;

            thing = game_my_big_map[(tile_z << 7) + tile_x].Child;
            while (thing != 0)
            {
                if (thing <= 0)
                {
                    thing = sthings[thing].Next;
                    continue;
                }
                {
                    struct Thing *p_thing = &things[thing];

                    if ((p_thing->Type == TT_BUILDING)
                      && (p_thing->SubType == SubTT_BLD_TRAINTRK)
                      && ((p_thing->Flag & TngF_Unkn08000000) == 0)
                      && ((p_thing->U.UObject.NextThing == 0)
                       || (p_thing->U.UObject.PrevThing == 0)))
                    {
                        int ret;

                        p_thing->U.UObject.ComHead = 0;
                        p_thing->U.UObject.ComCur = 0;
                        ret = train_related_func_01(p_thing);
                        if ((ushort)ret != 0)
                            ingame.TrainMode |= 0x02;
                    }
                }
                thing = things[thing].Next;
            }
        }
    }
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void rotate_texture_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_rotate_texture\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Rotates the UV coordinates of a floor texture by one corner, so
 * corner 2 becomes corner 1, etc.
 */
static void rotate_floor_texture_uv(struct SingleFloorTexture *p_fltex)
{
    ubyte x1, y1;

    y1 = p_fltex->TMapY1;
    x1 = p_fltex->TMapX1;
    p_fltex->TMapX1 = p_fltex->TMapX2;
    p_fltex->TMapY1 = p_fltex->TMapY2;
    p_fltex->TMapX2 = p_fltex->TMapX3;
    p_fltex->TMapY2 = p_fltex->TMapY3;
    p_fltex->TMapX3 = p_fltex->TMapX4;
    p_fltex->TMapX4 = x1;
    p_fltex->TMapY3 = p_fltex->TMapY4;
    p_fltex->TMapY4 = y1;
}

/** Rotates floor texture `texture` by one corner. If the texture (ignoring
 * flag bits 0xC000) is animated, rotates every frame of the animation.
 */
void rotate_texture(int texture)
{
    short tex;
    short anim;
    short i;

    DIFFTEST_SHADOW(rotate_texture, rotate_texture_asm(texture, 0, 0, 0));

    tex = (short)texture;
    if (tex == 0)
        return;
    anim = (short)is_animating((s32)(short)(texture & 0x3FFF));
    if (anim != 0)
    {
        for (i = 0; (s32)i <= (s32)(sbyte)game_anim_tmaps[anim].field_23; i++)
        {
            rotate_floor_texture_uv(&game_textures[game_anim_tmaps[anim].TMap[i]]);
        }
    }
    else
    {
        rotate_floor_texture_uv(&game_textures[tex]);
    }
}

static DIFFTEST_REF_FUNC void init_crater_textures_asm(void)
{
    DIFFTEST_ASM("call ASM_init_crater_textures\n"
        :  :  : "eax" );
    return;
}

static void crater_texture_set(ushort tx, ubyte page, ubyte x1, ubyte y1,
  ubyte x2, ubyte y2, ubyte x3, ubyte y3, ubyte x4, ubyte y4)
{
    struct SingleFloorTexture *p_fltex;

    p_fltex = &game_textures[tx];
    p_fltex->Page = page;
    p_fltex->TMapX1 = x1;
    p_fltex->TMapY1 = y1;
    p_fltex->TMapX2 = x2;
    p_fltex->TMapY2 = y2;
    p_fltex->TMapX3 = x3;
    p_fltex->TMapY3 = y3;
    p_fltex->TMapX4 = x4;
    p_fltex->TMapY4 = y4;
}

/** Copies the previous floor texture into slot `tx`, advances
 * next_floor_texture past it and rotates the new copy.
 */
static void crater_texture_copy_rotate(ushort tx)
{
    game_textures[tx] = game_textures[(int)tx - 1];
    next_floor_texture = tx + 1;
    rotate_texture((short)tx);
}

/** Adds a base crater texture at next_floor_texture, followed by
 * three copies, each rotated once more than the previous one.
 */
static void crater_texture_set_with_rotations(ubyte page, ubyte x1, ubyte y1,
  ubyte x2, ubyte y2, ubyte x3, ubyte y3, ubyte x4, ubyte y4)
{
    ushort tx;

    tx = next_floor_texture;
    crater_texture_set(tx, page, x1, y1, x2, y2, x3, y3, x4, y4);
    crater_texture_copy_rotate(tx + 1);
    crater_texture_copy_rotate(next_floor_texture);
    crater_texture_copy_rotate(next_floor_texture);
}

void init_crater_textures(void)
{
    ushort tx;

    DIFFTEST_SHADOW(init_crater_textures, init_crater_textures_asm());

    tx = next_floor_texture;
    crater_texture_set(tx, 0, 0x20, 0x60, 0x3f, 0x60, 0x3f, 0x7f, 0x20, 0x7f);
    crater_unkn_texture_flags = tx;
    tx++;
    crater_texture_set(tx, 2, 0xdf, 0x20, 0xdf, 0x3f, 0xc0, 0x3f, 0xc0, 0x20);
    data_1c42fe = tx;
    crater_texture_copy_rotate(tx + 1);
    crater_texture_copy_rotate(next_floor_texture);
    crater_texture_copy_rotate(next_floor_texture);

    crater_texture_set_with_rotations(2,
      0xc0, 0x60, 0xdf, 0x60, 0xdf, 0x7f, 0xc0, 0x7f);
    crater_texture_set_with_rotations(0,
      0xe0, 0x20, 0xff, 0x20, 0xff, 0x3f, 0xe0, 0x3f);
    crater_texture_set_with_rotations(0,
      0xc0, 0x20, 0xdf, 0x20, 0xdf, 0x3f, 0xc0, 0x3f);

    tx = next_floor_texture;
    crater_texture_set(tx, 2, 0xff, 0x20, 0xff, 0x3f, 0xe0, 0x3f, 0xe0, 0x20);
    data_1c4300 = tx;
    rotate_texture((short)tx);
    crater_texture_copy_rotate(next_floor_texture + 1);
    crater_texture_copy_rotate(next_floor_texture);
    crater_texture_copy_rotate(next_floor_texture);

    crater_texture_set_with_rotations(2,
      0xe0, 0x60, 0xff, 0x60, 0xff, 0x7f, 0xe0, 0x7f);
    crater_texture_set_with_rotations(0,
      0xe0, 0x60, 0xff, 0x60, 0xff, 0x7f, 0xe0, 0x7f);
    crater_texture_set_with_rotations(0,
      0xc0, 0x60, 0xdf, 0x60, 0xdf, 0x7f, 0xc0, 0x7f);
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int calc_object_height_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_calc_object_height\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
    return ra;
}

int calc_object_height(int object)
{
    struct SingleObject *p_sobj;
    struct SinglePoint *p_spoints;
    s32 pt, pt_end;
    s32 height, y;

    DIFFTEST_SHADOW_RET(calc_object_height, int,
      calc_object_height_asm(object, 0, 0, 0));
    p_spoints = game_object_points;
    p_sobj = &game_objects[(ushort)object];
    height = 0;
    pt_end = p_sobj->EndPoint;
    for (pt = p_sobj->StartPoint; pt <= pt_end; pt++)
    {
        y = p_spoints[pt].Y;
        if (y > height)
            height = y;
    }
    game_object_points = p_spoints;
    DIFFTEST_RETURN(height);
}

static DIFFTEST_REF_FUNC void find_the_tall_buildings_asm(void)
{
    DIFFTEST_ASM("call ASM_find_the_tall_buildings\n"
        :  :  : "eax" );
    return;
}

void find_the_tall_buildings(void)
{
    short tile_x, tile_z;
    ThingIdx thing;

    DIFFTEST_SHADOW(find_the_tall_buildings, find_the_tall_buildings_asm());
    for (tile_x = 0; tile_x < 128; tile_x++)
    {
        for (tile_z = 0; tile_z < 128; tile_z++)
        {
            thing = game_my_big_map[tile_z * 128 + tile_x].Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;

                    p_thing = &things[thing];
                    if (p_thing->Type == TT_BUILDING)
                        p_thing->U.UObject.BHeight = calc_object_height(
                          (ushort)p_thing->U.UObject.Object);
                    thing = things[thing].Next;
                }
                else
                {
                    thing = sthings[thing].Next;
                }
            }
        }
    }
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void nullsub_7_asm(void)
{
    DIFFTEST_ASM("call ASM_nullsub_7\n"
        :  : : "cc", "memory");
}

void nullsub_7(void)
{
    DIFFTEST_SHADOW(nullsub_7, nullsub_7_asm());
    // The asm is a bare `ret`.
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void create_unkn_building_subtp27_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_create_unkn_building_subtp27\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void create_unkn_building_subtp27(int cor_x, int cor_z, int object, int cor_y)
{
    struct Thing *p_thing;
    ThingIdx thing;
    ushort obj;

    DIFFTEST_SHADOW(create_unkn_building_subtp27,
      create_unkn_building_subtp27_asm(cor_x, cor_z, object, cor_y));
    thing = get_new_thing();
    p_thing = &things[thing];
    p_thing->U.UObject.NumbObjects = 1;
    p_thing->Y = (s32)(short)cor_y << 8;
    p_thing->Type = TT_BUILDING;
    p_thing->SubType = SubTT_BLD_STANDARD;
    p_thing->Z = (s32)(ushort)cor_z << 8;
    p_thing->X = (s32)(ushort)cor_x << 8;
    obj = (ushort)object;
    p_thing->U.UObject.Object = obj;
    game_objects[obj].ThingNo = thing;
    add_node_thing(thing);
    if ((thing > 1000) || (thing < 0))
        nullsub_7();
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int func_4225c_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    DIFFTEST_ASM("call ASM_func_4225c\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

int func_4225c(int object)
{
    struct SingleObject *p_sobj;
    struct SinglePoint *p_spt;
    ushort pt, pt_end;
    s32 dist, v;

    DIFFTEST_SHADOW_RET(func_4225c, int,
      func_4225c_asm(object, 0, 0, 0));
    p_sobj = &game_objects[(ushort)object];
    dist = 0;
    pt_end = p_sobj->EndPoint;
    for (pt = p_sobj->StartPoint; pt <= pt_end; pt++)
    {
        p_spt = &game_object_points[pt];
        v = abs(p_spt->Z);
        if (v > dist)
            dist = v;
        v = abs(p_spt->X);
        if (v > dist)
            dist = v;
    }
    DIFFTEST_RETURN((dist * 170) >> 8);
}

static DIFFTEST_REF_FUNC int create_train_carriage_asm(short cor_dx, short cor_dy, short cor_dz, short otype)
{
    int ret;
    DIFFTEST_ASM("call ASM_create_train_carriage\n"
        : "=a" (ret) : "a" (cor_dx), "d" (cor_dy), "b" (cor_dz), "c" (otype));
    return ret;
}

int create_train_carriage(short cor_dx, short cor_dy, short cor_dz, short otype)
{
    struct Thing *p_track;
    struct Thing *p_thing;
    ThingIdx thing;
    s32 cor_x, cor_y, cor_z;
    short radius;

    DIFFTEST_SHADOW_RET(create_train_carriage, int,
      create_train_carriage_asm(cor_dx, cor_dy, cor_dz, otype));
    p_track = find_unused_train_track();
    if (p_track == NULL) {
        // TODO the asm returns the caller's EDI register here (undefined value)
        DIFFTEST_RETURN((int)0);
    }
    cor_x = p_track->X >> 8;
    cor_y = p_track->Y >> 8;
    cor_z = p_track->Z >> 8;
    cor_x += cor_dx;
    cor_z += cor_dz;
    // TODO cor_dy is not used by the original code
    copy_prim_obj_to_game_object((ushort)cor_x, (ushort)cor_z,
      (short)(-prim_unknprop01 - otype), 0);
    unkn_object_shift_03((ushort)(next_object - 1));
    create_unkn_building_subtp27((ushort)cor_x, (ushort)cor_z,
      (ushort)(next_object - 1), 0);

    thing = things_used_head;
    p_thing = &things[(ushort)thing];
    p_thing->VX = 0;
    p_thing->VZ = 0;
    p_thing->VY = 0;
    p_thing->U.UVehicle.AngleX = 0;
    p_thing->U.UVehicle.AngleY = 0;
    p_thing->U.UVehicle.AngleZ = 0;
    p_thing->X = cor_x << 8;
    p_thing->Type = TT_VEHICLE;
    p_thing->Z = cor_z << 8;
    p_thing->SubType = SubTT_VEH_TRAIN;
    p_thing->Y = cor_y << 5;
    word_1C83E2 = thing;
    radius = func_4225c((ushort)p_thing->U.UVehicle.Object);

    p_thing = &things[(ushort)thing];
    p_thing->Health = 2000;
    p_thing->U.UVehicle.MaxHealth = 2000;
    p_thing->Radius = radius;
    p_thing->Parent = 0;
    p_thing->U.UVehicle.Object = next_object - 1;
    p_thing->U.UVehicle.ReqdSpeed = 1024;
    p_thing->Speed = 1024;
    p_thing->U.UVehicle.TNode = p_track->ThingOffset;
    p_thing->State = VehSt_UNKN_12;
    p_thing->U.UVehicle.WorkPlace = 0;
    p_thing->U.UVehicle.MatrixIndex = next_local_mat;
    p_thing->Health = 30000;
    p_thing->U.UVehicle.SubThing = 0;
    next_local_mat++;
    matrix_identity_fill(&local_mats[(short)p_thing->U.UVehicle.MatrixIndex]);
    word_1C83E8 = thing;

    DIFFTEST_RETURN((int)(ushort)thing);
}

static DIFFTEST_REF_FUNC struct Thing *find_unused_train_track_asm(void)
{
    struct Thing *ret;
    DIFFTEST_ASM("call ASM_find_unused_train_track\n"
        : "=a" (ret) : );
    return ret;
}

struct Thing *find_unused_train_track(void)
{
    struct Thing *p_things;
    ThingIdx thing;
    ushort remain;
    ushort count;

    DIFFTEST_SHADOW_RET(find_unused_train_track, struct Thing *,
      find_unused_train_track_asm());
    p_things = things;
    count = 0;
    remain = things_used;
    for (thing = things_used_head; thing > 0; )
    {
        struct Thing *p_thing;

        remain--;
        if (remain == 0xFFFF)
            break;
        p_thing = &p_things[thing];
        thing = p_thing->LinkChild;
        if (p_thing->Type != TT_BUILDING)
            continue;
        if (p_thing->SubType != SubTT_BLD_TRAINTRK)
            continue;
        if (p_thing->U.UObject.PrevThing != 0)
            continue;
        if ((p_thing->Flag & TngF_Unkn08000000) != 0)
            continue;
        if (p_thing->U.UObject.ComCur == 0)
            continue;
        if ((short)p_thing->U.UObject.ComHead == 0)
            continue;
        count++;
        if ((s32)count > ingame.fld_unkC59)
        {
            things = p_things;
            DIFFTEST_RETURN(p_thing);
        }
    }
    things = p_things;
    DIFFTEST_RETURN((struct Thing *)NULL);
}

void create_train_for_each_track(void)
{
    ushort k;

    k = 0;
    while (find_unused_train_track() && k < 10)
    {
        ThingIdx veh[4];
        struct Thing *p_thing;

        veh[0] = create_train_carriage(0, 0, 0 * TRAIN_CARRY_LENGTH, 4);
        veh[1] = create_train_carriage(0, 0, 1 * TRAIN_CARRY_LENGTH, 5);
        veh[2] = create_train_carriage(0, 0, 2 * TRAIN_CARRY_LENGTH, 5);
        veh[3] = create_train_carriage(0, 0, 3 * TRAIN_CARRY_LENGTH, 5);

        p_thing = &things[veh[0]];
        p_thing->Owner = 0;
        p_thing->U.UVehicle.GotoX = veh[1];

        p_thing = &things[veh[1]];
        p_thing->Owner = veh[0];
        p_thing->U.UVehicle.GotoX = veh[2];

        p_thing = &things[veh[2]];
        p_thing->Owner = veh[1];
        p_thing->U.UVehicle.GotoX = veh[3];

        p_thing = &things[veh[3]];
        p_thing->U.UVehicle.GotoX = 0;
        p_thing->Owner = veh[2];

        ingame.fld_unkC59++;
        k++;
    }
}

static DIFFTEST_REF_FUNC void init_my_paths_asm(void)
{
    DIFFTEST_ASM("call ASM_init_my_paths\n"
        :  :  : "eax" );
}

void init_my_paths(void)
{
    ushort i;

    DIFFTEST_SHADOW(init_my_paths, init_my_paths_asm());
    for (i = 0; i < 1599; i++)
    {
        my_paths[i].Next = i + 1;
        my_paths[i].Flag = 0;
    }
    my_paths[1599].Flag = 0;
    my_paths[1599].Next = 0;
    head_my_path = 1;
    path_count = 1;
}

void init_level_unknsub01_person(struct Thing *p_person)
{
    person_init_preplay_command(p_person);

    if (((p_person->Flag2 & TgF2_ExistsOffMap) == 0)
      && ((p_person->Flag & TngF_Destroyed) == 0))
    {
        struct GroupAction *p_grpact;

        p_grpact = &group_actions[p_person->U.UPerson.Group];
        p_grpact->Alive++;
    }
    p_person->U.UPerson.CurrentWeapon = 0;
    p_person->U.UPerson.PathIndex = 0;
    p_person->U.UPerson.Target2 = 0;
    p_person->U.UPerson.Within = 0;
    p_person->U.UPerson.EffectiveGroup = p_person->U.UPerson.Group;
    p_person->PTarget = NULL;
    p_person->U.UPerson.WeaponsCarried |= (1 << (WEP_ENERGYSHLD-1));
    p_person->OldTarget = 0;

    if ((p_person->Flag & TngF_Destroyed) != 0) {
        p_person->State = PerSt_DEAD;
    } else {
        p_person->State = PerSt_NONE;
    }
    if ((p_person->Flag & TngF_PlayerAgent) != 0) {
        p_person->U.UPerson.ComCur = 0;
    } else {
        p_person->U.UPerson.ComCur = p_person->U.UPerson.ComHead;
        p_person->Flag |= TngF_Unkn0040;
    }

    if ((p_person->Flag2 & TgF2_ExistsOffMap) != 0)
        delete_node(p_person);
    else
        p_person->Flag2 &= ~TgF2_InsideBuilding;

    if ((p_person->Flag & TngF_Destroyed) == 0)
    {
        if (p_person->U.UPerson.CurrentWeapon != 0)
            switch_person_anim_mode(p_person, ANIM_PERS_WEPLIGHT_IDLE);
        else
            switch_person_anim_mode(p_person, ANIM_PERS_IDLE);
    }
}

void init_level_unknsub01_building(struct Thing *p_buildng)
{
    p_buildng->Flag &= ~TngF_TriggerUse;
    if (p_buildng->SubType == SubTT_BLD_MGUN)
    {
        p_buildng->PTarget = NULL;
        p_buildng->U.UObject.EffectiveGroup = p_buildng->U.UObject.Group;
    }
    if ((p_buildng->Flag2 & TgF2_ExistsOffMap) != 0)
    {
        delete_node(p_buildng);
    }
}

void init_level_unknsub01_vehicle(struct Thing *p_vehicle)
{
    if ((p_vehicle->Flag2 & TgF2_ExistsOffMap) != 0)
    {
        delete_node(p_vehicle);
    }
}

void init_level_unknsub01(void)
{
    ThingIdx thing;

    thing = things_used_head;
    while (thing > 0)
    {
        struct Thing *p_thing;

        p_thing = &things[thing];
        switch (p_thing->Type)
        {
        case TT_VEHICLE:
            init_level_unknsub01_vehicle(p_thing);
            break;
        case TT_PERSON:
            init_level_unknsub01_person(p_thing);
            break;
        case TT_BUILDING:
            init_level_unknsub01_building(p_thing);
            break;
        default:
            break;
        }
        thing = p_thing->LinkChild;
    }
}

void setup_ingame_scene_effect(void)
{
    struct Mission *p_missi;
    ubyte atmos, weather;

    p_missi = &mission_list[ingame.CurrentMission];
    atmos = ((p_missi->Atmosphere) & 0x0F);
    weather = ((p_missi->Atmosphere >> 4) & 0x0F);

    // Reload texture regardless of the effect
    LbFileLoadAt("data/tex00.dat", vec_tmap[0]);

    switch (atmos)
    {
    case MAtmsph_Space:
        gamep_scene_effect_type = ScEff_SPACE;
        gamep_scene_effect_intensity = 1000;
        break;
    case MAtmsph_Moon:
        gamep_scene_effect_type = ScEff_NONE;
        gamep_scene_effect_intensity = 1000;
        break;
    case MAtmsph_Earth:
        stop_sample_using_heap(0, 78);
        if (weather == MWeathr_Rainy) {
            gamep_scene_effect_type = ScEff_RAIN;
            gamep_scene_effect_intensity = 1000;
            scene_post_effect_rain_init();
        } else if (weather == MWeathr_Snowy) {
            gamep_scene_effect_type = ScEff_SNOW;
            gamep_scene_effect_intensity = 1000;
            //TODO add some initial effect to textures?
        } else {
            gamep_scene_effect_type = ScEff_NONE;
            gamep_scene_effect_intensity = 1000;
        }
        break;
    default:
        gamep_scene_effect_type = ScEff_NONE;
        gamep_scene_effect_intensity = 1000;
        break;
    }
}

void start_ingame_ambient_sound(void)
{
    struct Mission *p_missi;
    ubyte atmos;

    p_missi = &mission_list[ingame.CurrentMission];
    atmos = ((p_missi->Atmosphere) & 0x0F);

    switch (atmos)
    {
    case MAtmsph_Space:
    case MAtmsph_Moon:
        stop_sample_using_heap(0, 8);
        stop_sample_using_heap(0, 77);
        play_sample_using_heap(0, 78, 64, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 2);
        break;
    case MAtmsph_Earth:
        stop_sample_using_heap(0, 78);
        if (gamep_scene_effect_type == ScEff_RAIN) {
            stop_sample_using_heap(0, 8);
            play_sample_using_heap(0, 77, 64, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 2);
        } else {
            stop_sample_using_heap(0, 77);
            play_sample_using_heap(0, 8, 64, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 2);
        }
        break;
    default:
        stop_sample_using_heap(0, 78);
        stop_sample_using_heap(0, 8);
        stop_sample_using_heap(0, 77);
        break;
    }
}

void init_level(void)
{
    short plyr_no;

    people_intel(1);
    if (in_network_game)
    {
        ingame.DetailLevel = 1;
        player_chat_clear();
    }
    else
    {
        init_mission_states();
    }

    plyr_no = 0;
    for (plyr_no = 0; plyr_no < PLAYERS_LIMIT; plyr_no++)
    {
        PlayerInfo *p_player;
        short plagent;

        p_player = &players[plyr_no];
        for (plagent = 0; plagent < LOCAL_USERS_MAX_COUNT; plagent++)
        {
            player_agent_clear_user_vect(plyr_no, plagent);
            p_player->SpecialItems[plagent] = 0;
            p_player->PanelItem[plagent] = 0;
            p_player->PanelState[plagent] = PANEL_STATE_NORMAL;
        }
        p_player->GotoFace = 0;
        p_player->field_102 = 0;
        player_target_clear(plyr_no);
    }

    set_user_selected_brightness();
    ingame.Flags &= ~TngF_Unkn8000;
    if (!in_network_game)
        ingame.InNetGame_UNSURE = (1 << 0);
    word_1531DA = 1;
    shield_frames_init();
    ingame.fld_unkCB7 = 0;
    ingame.fld_unkC59 = 0;
    ingame.FlameCount = 0;
    ingame.SoundThing = 0;
    ingame.fld_unkCB5 = 0;
    clear_open_mission_status();
    init_object_explode_faces();
    StopAllSamples();
    ingame.TrackThing = 0;
    func_74934();
    ingame.TrackX = engn_xc;
    ingame.TrackZ = engn_zc;
    ingame.fld_unkCA6 = 0;
    ingame.UserZoom = 120;
    word_1AABD0 = next_floor_texture;
    init_crater_textures();
    bang_init();
    FIRE_init();
    create_train_for_each_track();
    preprogress_trains_turns(50);
    tnext_floor_texture = next_floor_texture + 1;
    init_col_vects_linked_list();
    ingame.fld_unkC91 = dos_clock();
    setup_ingame_scene_effect();
    start_ingame_ambient_sound();
    gamep_unknval_10 = 0;
    gamep_unknval_12 = 0;
    nav_stats__ThisTurn = 0;
    gamep_unknval_16 = 0;
    ingame.fld_unkCB1 = 1;
    ingame.fld_unkCB2 = 1;
    missions_clear_bank_tests();
    thing_groups_clear_all_actions();
    init_my_paths();

    init_level_unknsub01();

    VNAV_preprocess_bezier_turns(1);
    VNAV_init_new_traffic_system();
    map_lights_update();
    init_scanner();
    find_the_tall_buildings();
    PlayCDTrack(ingame.CDTrack);
    StartMusic(ingame.DangerTrack, 0);
}

void init_level_3d(ubyte flag)
{
    ushort i;

    if (flag)
    {
        next_object = stored_l3d_next_object;
        next_object_face3 = stored_l3d_next_object_face3;
        next_object_face4 = stored_l3d_next_object_face4;
        next_object_point = stored_l3d_next_object_point;
        next_normal = stored_l3d_next_normal;
        next_face_texture = stored_l3d_next_face_texture;
        next_floor_texture = stored_l3d_next_floor_texture;
        next_local_mat = stored_l3d_next_local_mat;
        stored_level3d_inuse = 0;
    }
    else
    {
        stored_l3d_next_object = next_object;
        stored_l3d_next_object_face3 = next_object_face3;
        stored_l3d_next_object_face4 = next_object_face4;
        stored_l3d_next_object_point = next_object_point;
        stored_l3d_next_normal = next_normal;
        stored_l3d_next_face_texture = next_face_texture;
        stored_l3d_next_floor_texture = next_floor_texture;
        stored_l3d_next_local_mat = next_local_mat;
        stored_level3d_inuse = 1;

        // Prepare objects for rockets
        for (i = 0; i < WEP_ROCKETS_FIRED_LIMIT; i++)
        {
            ushort snobj;
            snobj = copy_prim_obj_to_game_object(0, 0, -prim_unknprop01 - 20, 0);
            unkn_object_shift_03(snobj);
            ingame.Rocket1[i] = snobj;
        }
        unkn2_pos_x = 64;
        unkn2_pos_y = 64;
        unkn2_pos_z = 64;
    }
}

void place_multiplayer_agent_groups(void)
{
    PlayerIdx plyr;

    for (plyr = 0; plyr < 8; plyr++)
    {
        ushort grp;
        grp = level_def.PlayableGroups[plyr];
        fill_netgame_agent_pos(plyr, grp, 4);
    }

    for (plyr = 0; plyr < 8; plyr++)
    {
        ushort grp;

        if ((ingame.InNetGame_UNSURE & (1 << plyr)) == 0)
            continue;

        grp = level_def.PlayableGroups[plyr];
        playable_agents = 4;
        make_group_into_players(grp, plyr, 4, group_factions[plyr]);
    }

    for (plyr = 0; plyr < 8; plyr++)
    {
        struct Thing *p_thing;
        ThingIdx thing, nxthing;
        int remain;
        ushort grp;

        if ((ingame.InNetGame_UNSURE & (1 << plyr)) == 0)
            continue;

        grp = level_def.PlayableGroups[plyr];
        remain = things_used;
        for (thing = things_used_head; thing > 0; thing = nxthing)
        {
            if (--remain == -1) {
                break;
            }
            p_thing = &things[thing];
            nxthing = p_thing->LinkChild;

            if ((p_thing->U.UPerson.Group == grp) &&
              (p_thing->Type == TT_PERSON) &&
              ((p_thing->Flag & TngF_PlayerAgent) == 0))
            {
                remove_thing(p_thing->ThingOffset);
                delete_node(p_thing);
            }
        }
    }

    if ((net_game_play_flags & 0x10) != 0)
    {
        int i;
        for (i = 0; i < 45; i++) {
            process_things_unkn_sub2(login_control[0].TechLevel, 0);
        }
    }
}

void init_game_controls(void)
{
    reset_user_groups();
    reset_user_input();

    init_user_input_local_controls();
}

struct Thing *unkn_create_random_person(int cor_x, int cor_y, int cor_z,
  int a4, int a5, int a6)
{
    struct Thing *p_person;
    ushort rnd;
    ushort subtype;
    WeaponType wtype;

    subtype = things_used % 12;
    if (subtype == 0)
        subtype = 1;
    p_person = new_sim_person(cor_x, cor_y, cor_z, subtype);

    rnd = things_used;
    p_person->U.UPerson.StartTimer2 = 10;
    p_person->U.UPerson.Angle = rnd & 7;
    p_person->U.UPerson.Timer2 = 10;
    p_person->State = PerSt_WANDER;

    wtype = p_person->U.UPerson.CurrentWeapon;
    weapons_add_one_for_npc(&p_person->U.UPerson.WeaponsCarried, wtype);
    reset_person_frame(p_person);

    return p_person;
}

void simulated_level(void)
{
    int i;
    ushort tl_z, tl_x;
    MapCoord cor_x, cor_y, cor_z;

    for (i = 0; i < PEOPLE_GROUPS_LIMIT; i++)
    {
        war_flags[i].KillOnSight = 0;
        war_flags[i].Guardians[0] = 6;
        war_flags[i].Guardians[1] = 0;
    }

    for (tl_x = 10; tl_x < 122; tl_x += 8)
    {
        cor_x = TILE_TO_MAPCOORD(tl_x, 0);
        for (tl_z = 10; tl_z < 122; tl_z += 8)
        {
            cor_z = TILE_TO_MAPCOORD(tl_z, 0);
            if (!is_this_a_nice_place_for_a_picnic(
              MAPCOORD_TO_PRCCOORD(cor_x,0),
              MAPCOORD_TO_PRCCOORD(cor_z,0)))
                continue;
            unkn_create_random_person(cor_x, 0, cor_z, 0, 50, 64);
        }
    }

    for (i = 0; i < 100; i++)
    {
        struct SimpleThing *p_item;
        ushort wtype;

        tl_x = 10 + (LbRandomAnyShort() % 100);
        tl_z = 10 + (LbRandomAnyShort() % 100);
        cor_x = TILE_TO_MAPCOORD(tl_x, 127);
        cor_z = TILE_TO_MAPCOORD(tl_z, 127);
        cor_y = alt_at_point(cor_x, cor_z) >> 8;

        if (!is_this_a_nice_place_for_a_picnic(
          MAPCOORD_TO_PRCCOORD(cor_x,0),
          MAPCOORD_TO_PRCCOORD(cor_z,0)))
            continue;

        wtype = (i % 26) + 1;
        if (((1 << (i % 26)) & ~0xC0042000) == 0)
            continue;

        p_item = create_item(cor_x, cor_y, cor_z, 1004u, wtype);
        if (p_item != NULL)
        {
            if ((wtype == WEP_EXPLMINE) || (wtype == WEP_ELEMINE))
            {
                p_item->Owner2 = 0;
                p_item->Timer1 = 999;
                p_item->StartTimer1 = 999;
                if (wtype <= WEP_ELEMINE)
                    p_item->SubType = 7;
                else
                    p_item->SubType = 3;
                p_item->Radius = 50;
            }
        }
    }
}

void init_local_player_group(void)
{
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    ushort grp;

    p_locplayer = &players[local_player_no];
    p_agent = p_locplayer->MyAgent[0];

    if (p_agent->Type == TT_PERSON) {
        grp = p_agent->U.UPerson.EffectiveGroup;
    } else {
        grp = 0;
    }
    ingame.MyGroup = grp;
}

void validate_player_double_mode(void)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    switch (playable_agents)
    {
    case 1:
        p_locplayer->DoubleMode = 0;
        break;
    case 2:
        if (p_locplayer->DoubleMode > 1)
            p_locplayer->DoubleMode = 1;
        break;
    case 3:
        if (p_locplayer->DoubleMode > 2)
              p_locplayer->DoubleMode = 2;
        break;
    default:
        break;
    }
}

void game_set_cam_starting_location_xz(void)
{
    MapCoord agent_x, agent_z;
    ThingIdx thing;

    {
        PlayerInfo *p_player;
        struct Thing *p_agent;

        p_player = &players[local_player_no];
        p_agent = p_player->MyAgent[0];
        if (p_agent->Type == TT_PERSON) {
            agent_x = PRCCOORD_TO_MAPCOORD(p_agent->X);
            agent_z = PRCCOORD_TO_MAPCOORD(p_agent->Z);
        } else {
            agent_x = MAP_COORD_WIDTH / 2;
            agent_z = MAP_COORD_HEIGHT / 2;
        }
    }
    engn_xc = MAP_COORD_WIDTH - agent_x;
    engn_zc = MAP_COORD_HEIGHT - agent_z;
    thing = 0;

    if (thing == 0)
    {
        MapCoord cor_x, cor_z;

        if (level_misc_get_starting_camera_pos(&cor_x, &cor_z))
        {
            engn_xc = cor_x;
            engn_zc = cor_z;
            thing = -1;
        }
    }

    if (thing == 0)
    {
        thing = search_things_for_type_farthest_from_xz(agent_x, agent_z, TT_BUILDING, SubTT_BLD_MGUN);
        if (thing > 0)
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            engn_xc = PRCCOORD_TO_MAPCOORD(p_thing->X);
            engn_zc = PRCCOORD_TO_MAPCOORD(p_thing->Z);
        }
    }
    if (thing == 0)
    {
        thing = search_things_for_type_farthest_from_xz(agent_x, agent_z, TT_BUILDING, SubTT_BLD_BILLBOARD);
        if (thing > 0)
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            engn_xc = PRCCOORD_TO_MAPCOORD(p_thing->X);
            engn_zc = PRCCOORD_TO_MAPCOORD(p_thing->Z);
        }
    }
}

TbBool game_cam_tracked_thing_is_player_agent(void)
{
    struct Thing *p_thing;

    p_thing = &things[ingame.TrackThing];
    return ((p_thing->Flag & TngF_PlayerAgent) != 0);
}

void game_set_cam_track_thing_xz(ThingIdx thing)
{
    short tng_x, tng_z;

    get_thing_position_mapcoords(&tng_x, NULL, &tng_z, thing);
    ingame.TrackX = tng_x;
    ingame.TrackZ = tng_z;
}

void game_set_cam_track_player_agent_xz(PlayerIdx plyr, ushort plagent)
{
    PlayerInfo *p_player;
    struct Thing *p_agent;

    p_player = &players[plyr];
    p_agent = p_player->MyAgent[plagent];
    if (p_agent->Type != TT_PERSON)
        return;
    game_set_cam_track_thing_xz(p_agent->ThingOffset);
}

void init_random_seed_default(void)
{
    lbSeed = 0xD15C1234;
    game_srand(lbSeed);
}

void preprogress_game_turns(void)
{
    struct Mission *p_missi;
    u32 bkp_ingame_flags;
    ubyte bkp_execute_commands;

    p_missi = &mission_list[ingame.CurrentMission];
    LOGSYNC("PreProcess %d turns for mission %d, starting at %u",
      (int)p_missi->PreProcess, (int)ingame.CurrentMission, (u32)gameturn);

    // Stopping things update would make the preprocess ineffective
    bkp_ingame_flags = ingame.Flags;
    ingame.Flags |= TngF_ProgressAction;

    bkp_execute_commands = execute_commands;
    execute_commands = 1;

    blind_progress_game(p_missi->PreProcess);

    execute_commands = bkp_execute_commands;
    if ((bkp_ingame_flags & TngF_ProgressAction) == 0)
        ingame.Flags &= ~TngF_ProgressAction;
}

/** Initializes player presence on a level.
 *
 * CurrentMission needs to be set before this funcion is called.
 */
void init_player(void)
{
    gamep_unknval_10 = 0;
    gamep_unknval_12 = 0;
    nav_stats__ThisTurn = 0;
    ingame.Flags &= ~GamF_Unkn0100;
    ingame.Flags &= ~GamF_ThermalView;
    gamep_unknval_16 = 0;
    init_level_3d(0);
    init_level();
    if (in_network_game)
    {
        place_multiplayer_agent_groups();
        unkn_truce_groups();
    }
    else
    {
        place_single_player();
        game_set_cam_track_player_agent_xz(local_player_no, 0);
    }
    player_debug(" after place player");
    if (current_level == 0)
        simulated_level();
    player_debug(" after sim level");
    gameturn = 1;
    player_debug(" after group action");
    init_local_player_group();
    validate_player_double_mode();
    player_agents_init_prev_weapon(local_player_no);
    player_agents_clear_weapon_delays(local_player_no);

    init_game_controls();
}

void randomize_playable_groups_order(void)
{
    static s32 incrementing_nubers[] = {0, 1, 2, 3, 4, 5, 6, 7,};
    array_elements_in_random_order(level_def.PlayableGroups, incrementing_nubers, s32, 8);
}

void init_game(ubyte reload)
{
    ushort missi, next_mapno;
    short next_level;
    TbBool reload_map, reload_level;

    debug_trace_setup(1);
    missi = ingame.CurrentMission;
    next_mapno = mission_list[missi].MapNo;

    if (PacketRecord_IsPlayback()) {
        next_level = packet_rec_use_levelno;
    } else if ((reload) && (mission_list[missi].ReLevelNo != 0)) {
        next_level = mission_list[missi].ReLevelNo;
    } else {
        next_level = mission_list[missi].LevelNo;
    }

    // Whether we use the same map and the same level as currently loaded
    reload_map = (current_map == next_mapno) && reload;
    reload_level = (current_level == next_level) && reload;

    LOGSYNC("Init %s mission %hu on map %hu level %hd", in_network_game ? "MP" : "SP",
      missi, next_mapno, next_level);

    if (!reload_map)
        change_current_map(next_mapno);
    debug_trace_setup(0);

    if (next_level != 0)
        load_level_wrap(-next_level, missi, reload_level);
    else
        LOGWARN("Requested level %hd; load skipped", next_level);
    // The file name is formatted in original code, but doesn't seem to be used
    //sprintf(fname, "maps/map%03d.scn", next_mapno);
    debug_trace_setup(2);

    if (ingame.GameMode == GamM_None)
        ingame.GameMode = GamM_Unkn2;
    if (in_network_game) {
        randomize_playable_groups_order();
    }
    debug_trace_setup(3);
    init_player();
    if (!reload)
        game_set_cam_starting_location_xz();
    debug_trace_setup(4);
    execute_commands = 1;
    ingame.DisplayMode = DpM_ENGINEPLY;
    debug_trace_setup(5);
}

void restart_back_into_mission(ushort missi)
{
    ushort mapno;
    ubyte reload;

    reload = (missi == ingame.CurrentMission);
    mapno = mission_list[missi].MapNo;
    mission_result = 0;
    ingame.MissionEndFade = 0;
    ingame.CurrentMission = missi;
    mission_list[missi].Complete = MResol_UNDECIDED;
    change_current_map(mapno);
    map_lights_update();
    if (ingame.GameMode == GamM_Unkn2)
        execute_commands = 0;
    engn_yc = 0;
    init_game(reload);
    init_random_seed_default();
    if (!in_network_game)
    {
        if (PacketRecord_IsRecord()) {
            PacketRecord_Close();
            PacketRecord_OpenWrite();
            packet_write_whole_player_init();
        }
    }
    preprogress_game_turns();
}

static DIFFTEST_REF_FUNC short test_single_mission_asm(short missi)
{
    short ret;
    DIFFTEST_ASM("call ASM_test_single_mission\n"
        : "=a" (ret) : "a" (missi));
    return ret;
}

short test_single_mission(short missi)
{
    struct Objective *p_objectv;
    ushort objectv;
    ushort level;
    ushort n_total, n_done;
    ushort bank_total, bank_done;
    ushort n_failed;
    ushort n_pri3_done, n_pri3_undone;
    ushort count;
    short ret;

    DIFFTEST_SHADOW_RET(test_single_mission, short, test_single_mission_asm(missi));

    level = (((s32)(ushort)current_level - 1) % 15) + 1;
    n_total = 0;
    n_done = 0;
    bank_total = 0;
    bank_done = 0;
    n_failed = 0;
    n_pri3_done = 0;
    n_pri3_undone = 0;

    count = 0;
    objectv = mission_list[missi].SuccessHead;
    while (objectv != 0)
    {
        if (count++ >= 100)
            break;
        p_objectv = &game_used_objectives[objectv];
        if ((p_objectv->Level == level) && (p_objectv->Map == current_map))
        {
            if (p_objectv->Pri == mission_list[missi].BankTest)
            {
                bank_total++;
                ret = test_objective(objectv, 1);
                n_total++;
                if (ret < 0) {
                    n_failed++;
                } else if (ret > 0) {
                    bank_done++;
                    n_done++;
                }
            }
            else
            {
                if (p_objectv->Pri < mission_list[missi].BankTest)
                    n_done++;
                n_total++;
            }
        }
        objectv = game_used_objectives[objectv].Next;
    }

    if ((bank_total == bank_done) && (bank_total > 0))
        mission_list[missi].BankTest++;

    if ((n_done == n_total) && (n_total > 0))
        DIFFTEST_RETURN((short)1);

    count = 0;
    objectv = mission_list[missi].FailHead;
    while (objectv != 0)
    {
        if (count++ >= 90)
            break;
        p_objectv = &game_used_objectives[objectv];
        if ((p_objectv->Level == level) && (p_objectv->Map == current_map))
        {
            if ((byte_1C844F != 0) && ((lbShift & 0x01) != 0))
                ret = test_objective(objectv, 1);
            else
                ret = test_objective(objectv, 0);
            p_objectv = &game_used_objectives[objectv];
            p_objectv->Status = 0;
            if (p_objectv->Pri == 3)
            {
                if (ret > 0)
                    n_pri3_done++;
                else
                    n_pri3_undone++;
            }
            else if (ret > 0)
            {
                if (p_objectv->ObjText != 0)
                    ingame.fld_unkCB5 = p_objectv->ObjText;
                n_failed++;
                break;
            }
        }
        objectv = game_used_objectives[objectv].Next;
    }

    if ((n_pri3_done != 0) && (n_pri3_undone == 0))
        DIFFTEST_RETURN((short)-1);
    if (n_failed != 0)
        DIFFTEST_RETURN((short)-1);
    DIFFTEST_RETURN((short)0);
}

static DIFFTEST_REF_FUNC void post_process_blips_asm(void)
{
    DIFFTEST_ASM("call ASM_post_process_blips\n"
        :  :  : "eax" );
}

void post_process_blips(void)
{
    struct Thing *p_dcthing;
    s32 cor_x, cor_z;
    s32 dist_x, dist_z, dist;
    s32 min_dist, beep_period;
    ushort prev_count;
    int i;

    DIFFTEST_SHADOW(post_process_blips, post_process_blips_asm());

    p_dcthing = &things[players[local_player_no].DirectControl[0]];
    cor_x = p_dcthing->X >> 8;
    cor_z = p_dcthing->Z >> 8;
    min_dist = 0x7FFFFFFF;
    // TODO the asm checks only 15 of SCANNER_BIG_BLIP_COUNT (16) blips
    for (i = 0; i < 15; i++)
    {
        struct BigBlip *p_bblip;

        p_bblip = &ingame.Scanner.BigBlip[i];
        if (p_bblip->Period == 0)
            continue;
        dist_x = abs((p_bblip->X >> 8) - cor_x);
        dist_z = abs((p_bblip->Z >> 8) - cor_z);
        if (dist_x <= dist_z)
            dist = dist_z + (dist_x >> 1);
        else
            dist = dist_x + (dist_z >> 1);
        if (dist < min_dist)
            min_dist = dist;
    }

    beep_period = (min_dist >> 9) + 2;
    if ((ingame.Flags & GamF_NoScannerBeep) != 0)
        return;
    if (ingame.TrackThing != 0)
        return;
    prev_count = data_1c8468;
    data_1c8468 = prev_count + 1;
    if ((s32)prev_count <= beep_period)
        return;
    if (beep_period < 60)
        play_sample_using_heap(0, 94, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    data_1c8468 = 0;
}

short test_missions(ubyte flag)
{
    ushort mslot;
    short missi;
    short res;

    if (flag == 0 && ingame.TrackThing)
        return 0;

    if (mission_open[1] == 0)
        mission_open[1] = ingame.CurrentMission;

    if (flag == 0) {
        draw_objective(flag, 1);
        add_signal_to_scanner(0, 1);
    }

    if (flag != 0) {
        set_mission_state_using_state_slot(ingame.CurrentMission, MResol_UNDECIDED);
        return 0;
    }

    if (in_network_game) {
        mslot = 1;
        mission_open[mslot] = ingame.CurrentMission;
    } else {
        mslot = find_mission_state_slot(ingame.CurrentMission);
        if (mslot == 0) {
            mslot = find_empty_mission_state_slot();
        }
    }

    if (mission_open[mslot] > next_mission)
        mission_open[mslot] = 1;
    if (mission_state[mslot] == MResol_FAILED)
    {
        screen_objective_text_set_failed();
        return -1;
    }
    missi = mission_open[mslot];
    mission_state[mslot] = MResol_UNDECIDED;

    res = test_single_mission(missi);
    if (ingame.CurrentMission == mission_open[mslot])
    {
        if (mission_result == 1)
            res = 1;
        if (mission_result == -1)
            res = -1;
    }
    if (res == 0)
    {
        short i;

        res = -1;
        for (i = 0; i < playable_agents; i++)
        {
            PlayerInfo *p_locplayer;
            struct Thing *p_agent;

            p_locplayer = &players[local_player_no];
            p_agent = p_locplayer->MyAgent[i];
            if ((p_agent->Type == TT_PERSON) && (p_agent->State != PerSt_DEAD)) {
                res = 0;
                break;
            }
        }
    }
    if (res > 0)
    {
        mission_state[mslot] = MResol_COMPLETED;
        return 1;
    }
    if (res < 0)
    {
        mission_state[mslot] = MResol_FAILED;
        screen_objective_text_set_failed();
        return -1;
    }
    post_process_blips();
    return 0;
}

static DIFFTEST_REF_FUNC void process_map_crater_asm(int crater)
{
    DIFFTEST_ASM("call ASM_process_map_crater\n"
        : : "a" (crater) : "cc", "memory");
}

/** Random roll deciding whether a crater spreads into a neighbour tile.
 */
static TbBool crater_spread_roll(short max_d, short cur, short spread)
{
    s32 div;

    div = (s32)max_d - (s32)cur;
    return (((s32)LbRandomAnyShort() % div) < spread);
}

/** Advances one crater from the map craters buffer.
 */
void process_map_crater(int crater)
{
    struct MapCreater *p_cratr;
    struct MyMapElement *p_mapel;
    short *queue;
    ubyte *level;
    u32 head, tail;
    s32 min_x, max_x, min_z, max_z;
    short max_d, spread, cur;
    ubyte max_lv, lv;
    short x, z;

    DIFFTEST_SHADOW(process_map_crater, process_map_crater_asm(crater));
    p_cratr = &map_craters[(ubyte)crater];
    p_cratr->Iterations++;
    if (p_cratr->Iterations > 5)
    {
        map_crater_first++;
        if ((sbyte)map_crater_first < 0)
            map_crater_first = 0;
        p_cratr->Iterations = 0;
        return;
    }
    max_d = (short)(((s32)p_cratr->Iterations * (s32)p_cratr->Depth) / 5);

    queue = (short *)scratch_buf1;
    level = scratch_buf1 + 0x8000;
    memset(level, 0, 0x4000);

    spread = (short)((max_d * 3) / 7);
    max_lv = (ubyte)(max_d / 10);
    if (max_lv < 3)
        max_lv = 3;

    min_x = p_cratr->MapX - 1;
    max_x = p_cratr->MapX + 1;
    min_z = p_cratr->MapY - 1;
    max_z = p_cratr->MapY + 1;
    if (min_x < 0)
        min_x = 0;
    if (min_x >= 128)
        min_x = 127;
    if (max_x < 0)
        max_x = 0;
    if (max_x >= 128)
        max_x = 127;
    if (min_z < 0)
        min_z = 0;
    if (min_z >= 128)
        min_z = 127;
    if (max_z < 0)
        max_z = 0;
    if (max_z >= 128)
        max_z = 127;

    tail = 0;
    head = 0;
    queue[(ushort)head] = (short)((p_cratr->MapY << 7) + p_cratr->MapX);
    head++;
    level[queue[(ushort)tail]] = 1;

    if ((ushort)head != (ushort)tail)
    {
      do
      {
        short *p_tile;
        s32 tile;
        short thing;

        p_tile = &queue[(ushort)tail];
        tile = *p_tile;
        if ((tile < 0) || (tile >= 0x8000))
            goto next_tile;
        if (floor_texture_is_fluid(game_my_big_map[tile].Texture & 0x3FFF))
            goto next_tile;
        game_my_big_map[*p_tile].Texture &= 0xC000;
        game_my_big_map[*p_tile].Texture |= crater_unkn_texture_flags;
        game_my_big_map[*p_tile].ColumnHead &= COL_COLUMN_MASK;
        tile = *p_tile;
        lv = level[tile];
        {
            short dep;
            s32 dd;

            dep = (short)(p_cratr->Depth - ((s32)lv * p_cratr->Depth) / (s32)max_lv);
            dd = dep / 5;
            dd = ((dd + LbRandomAnyShort()) & 0xF) >> 3;
            p_mapel = &game_my_big_map[tile];
            p_mapel->Alt = (short)(p_mapel->Alt - dd);
        }

        thing = game_my_big_map[*p_tile].Child;
        while (thing != 0)
        {
            if (thing > 0)
            {
                struct Thing *p_thing;

                p_thing = &things[thing];
                if (p_thing->Type == TT_PERSON)
                {
                    s32 dx, dz;

                    tile = queue[(ushort)tail];
                    dx = (p_thing->X >> 8) - (0x80 + ((tile % 128) << 8));
                    dz = (p_thing->Z >> 8) - (((tile / 128) << 8) + 0x80);
                    thing_hit_by_bullet(p_thing, 600, (short)dx, 0, (short)dz, NULL, 9);
                    things[thing].Flag2 &= ~TgF2_InsideBuilding;
                    things[thing].Y = alt_at_point((short)(things[thing].X >> 8),
                      (short)(things[thing].Z >> 8));
                }
                thing = things[thing].Next;
            }
            else
            {
                struct SimpleThing *p_sthing;

                p_sthing = &sthings[thing];
                if (can_the_tank_run_over(p_sthing))
                {
                    sthings[thing].Y = alt_at_point((short)(sthings[thing].X >> 8),
                      (short)(sthings[thing].Z >> 8));
                    set_static_on_fire(&sthings[thing]);
                }
                thing = sthings[thing].Next;
            }
        }

        cur = (short)(max_d - ((s32)lv * max_d) / (s32)max_lv);
        if (cur == max_d)
            cur--;
        if (lv >= max_lv)
            goto next_tile;
        lv++;

        p_tile = &queue[(ushort)tail];
        tile = *p_tile;
        if (((tile % 128) != 0) && (level[tile - 1] == 0))
        {
            if (crater_spread_roll(max_d, cur, spread))
            {
                queue[(ushort)head] = *p_tile - 1;
                head++;
                level[*p_tile - 1] = lv;
                if ((queue[(ushort)head - 1] % 128) == min_x)
                {
                    min_x--;
                    if (min_x < 0)
                        min_x = 0;
                }
            }
        }
        tile = *p_tile;
        if (((tile % 128) < 0x7F) && (level[tile + 1] == 0))
        {
            if (crater_spread_roll(max_d, cur, spread))
            {
                queue[(ushort)head] = *p_tile + 1;
                level[*p_tile + 1] = lv;
                head++;
                if ((queue[(ushort)head - 1] % 128) == max_x)
                {
                    max_x++;
                    if (max_x > 0x7F)
                        max_x = 0x7F;
                }
            }
        }
        tile = *p_tile;
        if ((tile >= 0x80) && (level[tile - 0x80] == 0))
        {
            if (crater_spread_roll(max_d, cur, spread))
            {
                queue[(ushort)head] = *p_tile - 0x80;
                head++;
                level[*p_tile - 0x80] = lv;
                if ((queue[(ushort)head - 1] / 128) == min_z)
                {
                    min_z--;
                    if (min_z < 0)
                        min_z = 0;
                }
            }
        }
        tile = *p_tile;
        if ((tile < 0x3F80) && (level[tile + 0x80] == 0))
        {
            if (crater_spread_roll(max_d, cur, spread))
            {
                queue[(ushort)head] = *p_tile + 0x80;
                level[*p_tile + 0x80] = lv;
                head++;
                if ((queue[(ushort)head - 1] / 128) == max_z)
                {
                    max_z++;
                    if (max_z > 0x7F)
                        max_z = 0x7F;
                }
            }
        }
next_tile:
        tail++;
      }
      while ((ushort)tail != (ushort)head);
    }

    // Fill in tiles surrounded by crater on three sides
    for (z = (short)min_z; z <= max_z; z++)
    {
        for (x = (short)min_x; x <= max_x; x++)
        {
            s32 tile;
            TbBool match_e, match_w, match_s, match_n;

            if ((x < 0) || (x >= 128))
                continue;
            if ((z < 0) || (z >= 128))
                continue;
            tile = (z << 7) + x;
            if ((game_my_big_map[tile].Texture & 0x3FFF) == crater_unkn_texture_flags)
                continue;
            match_e = ((game_my_big_map[tile + 1].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_w = ((game_my_big_map[tile - 1].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_s = ((game_my_big_map[tile + 0x80].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_n = ((game_my_big_map[tile - 0x80].Texture & 0x3FFF) == crater_unkn_texture_flags);
            if ((match_e && match_w && match_s) || (match_e && match_w && match_n)
              || (match_w && match_s && match_n) || (match_e && match_s && match_n))
            {
                p_mapel = &game_my_big_map[tile];
                p_mapel->Texture &= 0xC000;
                p_mapel->Texture |= crater_unkn_texture_flags;
                p_mapel->ColumnHead &= COL_COLUMN_MASK;
            }
        }
    }

    // Set the edge blend of tiles neighbouring the crater
    for (z = (short)min_z; z <= max_z; z++)
    {
        for (x = (short)min_x; x <= max_x; x++)
        {
            s32 tile;
            ushort tear;
            TbBool match_nw, match_n, match_ne, match_w, match_e, match_sw, match_s, match_se;

            if ((x < 0) || (x >= 128))
                continue;
            if ((z < 0) || (z >= 128))
                continue;
            tile = (z << 7) + x;
            p_mapel = &game_my_big_map[tile];
            if ((p_mapel->Texture & 0x3FFF) == crater_unkn_texture_flags)
                continue;
            match_nw = ((game_my_big_map[tile - 0x81].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_n  = ((game_my_big_map[tile - 0x80].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_ne = ((game_my_big_map[tile - 0x7F].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_w  = ((game_my_big_map[tile - 1].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_e  = ((game_my_big_map[tile + 1].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_sw = ((game_my_big_map[tile + 0x7F].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_s  = ((game_my_big_map[tile + 0x80].Texture & 0x3FFF) == crater_unkn_texture_flags);
            match_se = ((game_my_big_map[tile + 0x81].Texture & 0x3FFF) == crater_unkn_texture_flags);

            if (match_e && match_s)
                tear = 12;
            else if (match_n && match_e)
                tear = 9;
            else if (match_w && match_n)
                tear = 6;
            else if (match_s && match_w)
                tear = 3;
            else if (match_s)
                tear = 1;
            else if (match_e)
                tear = 10;
            else if (match_n)
                tear = 7;
            else if (match_w)
                tear = 4;
            else if (match_se)
                tear = 11;
            else if (match_ne)
                tear = 8;
            else if (match_nw)
                tear = 5;
            else if (match_sw)
                tear = 2;
            else
                continue;
            p_mapel->ColumnHead &= COL_COLUMN_MASK;
            p_mapel->ColumnHead |= (tear << 12);
        }
    }
}

static DIFFTEST_REF_FUNC void process_map_craters_asm(void)
{
    DIFFTEST_ASM("call ASM_process_map_craters\n"
        :  :  : "eax" );
}

void process_map_craters(void)
{
    ubyte cratr;

    DIFFTEST_SHADOW(process_map_craters, process_map_craters_asm());
    if (map_crater_first == map_crater_next)
        return;
    cratr = map_crater_first;
    do
    {
        process_map_crater(cratr);
        cratr++;
        if ((sbyte)cratr < 0)
            cratr = 0;
    }
    while (map_crater_next != cratr);
}

/** Orbital station explosion code.
 */
void game_process_orbital_station_explode(void)
{
    if (unkn01_downcount > 0)
    {
        unkn01_downcount--;
        LOGDBG("unkn01_downcount = %d", unkn01_downcount);
        if (unkn01_downcount == 40) {
            mapwho_unkn01(unkn01_pos_x, unkn01_pos_y);
        }
        else if (unkn01_downcount < 40) {
            ushort stl_x, stl_y;

            stl_y = unkn01_pos_y + (LbRandomAnyShort() & 0xF) - 7;
            stl_x = unkn01_pos_x + (LbRandomAnyShort() & 0xF) - 7;
            bang_new4(stl_x << 16, 0, stl_y << 16, 95);
            stl_y = unkn01_pos_y + (LbRandomAnyShort() & 0xF) - 7;
            stl_x = unkn01_pos_x + (LbRandomAnyShort() & 0xF) - 7;
            bang_new4(stl_x << 16, 0, stl_y << 16, 95);
        }
    }
}

/** Triggers the orbital station self-destruct explosion at given map position.
 *
 * Sends a few decorative sparks near the position, clears the mapwho things
 * within a 33x33 tile area centered on it (calling explode faces effects on
 * the way), then removes every remaining Thing in the game and every
 * SimpleThing still linked to the map.
 */
void mapwho_unkn01(int cent_tile_x, int cent_tile_z)
{
    int i;
    int dx, dz;
    int tl_x, tl_z;
    ThingIdx thing, nxthing;

    dont_bother_with_explode_faces = 0;
    SCANNER_clear();

    expl_unkn_cor_x = TILE_TO_MAPCOORD(cent_tile_x, 0);
    expl_unkn_cor_z = TILE_TO_MAPCOORD(cent_tile_z, 0);

    for (i = 0; i < 5; i++)
    {
        int spk_tl_x, spk_tl_z;

        spk_tl_z = cent_tile_z + (LbRandomAnyShort() & 0xF) - 7;
        spk_tl_x = cent_tile_x + (LbRandomAnyShort() & 0xF) - 7;
        bang_new4(TILE_TO_MAPCOORD(spk_tl_x, 0) << 8, 0, TILE_TO_MAPCOORD(spk_tl_z, 0) << 8, 95);
    }

    // Process the outer ring of the 33x33 area.
    // Push explode faces effect onto every Thing.
    for (dx = -16; dx <= 16; dx++)
    {
        for (dz = -16; dz <= 16; dz++)
        {
            struct MyMapElement *p_mapel;

            if ((dx >= -8) && (dx <= 8) && (dz >= -8) && (dz <= 8))
                continue;

            tl_x = cent_tile_x + dx;
            tl_z = cent_tile_z + dz;
            if ((tl_x < 0) || (tl_x >= MAP_TILE_WIDTH))
                continue;
            if ((tl_z < 0) || (tl_z >= MAP_TILE_HEIGHT))
                continue;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];

            for (thing = p_mapel->Child; thing != 0; thing = nxthing)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    nxthing = p_thing->Next;

                    thing_explode_faces(p_thing);
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    nxthing = p_sthing->Next;
                }
            }
        }
    }

    // Process the inner 17x17 area. Explode faces effect on every tile,
    // plus explode faces effect on every Thing.
    for (dx = -8; dx <= 8; dx++)
    {
        for (dz = -8; dz <= 8; dz++)
        {
            struct MyMapElement *p_mapel;

            tl_x = cent_tile_x + dx;
            tl_z = cent_tile_z + dz;
            if ((tl_x < 0) || (tl_x >= MAP_TILE_WIDTH))
                continue;
            if ((tl_z < 0) || (tl_z >= MAP_TILE_HEIGHT))
                continue;

            floor_explode_faces(tl_x, tl_z);

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];

            for (thing = p_mapel->Child; thing != 0; thing = nxthing)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    nxthing = p_thing->Next;

                    thing_explode_faces(p_thing);
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    nxthing = p_sthing->Next;
                }
            }
        }
    }

    // Remove every Thing currently in the game.
    for (thing = things_used_head; thing != 0; thing = nxthing)
    {
        struct Thing *p_thing;

        p_thing = &things[thing];
        nxthing = p_thing->LinkChild;

        remove_thing(p_thing->ThingOffset);
        if (on_mapwho(p_thing))
              delete_node(p_thing);

        p_thing->Flag2 = 0;
        //TODO shouldn't dead state be only valid for people, not all things?
        p_thing->State = PerSt_DEAD;
        p_thing->Flag |= TngF_Destroyed;
    }

    // Remove every SimpleThing still linked to the mapwho chains.
    for (tl_z = 0; tl_z < MAP_TILE_WIDTH; tl_z++)
    {
        for (tl_x = 0; tl_x < MAP_TILE_WIDTH; tl_x++)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];

            thing = p_mapel->Child;
            while (thing != 0)
            {
                if (thing > 0)
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    thing = p_thing->Next;
                }
                else
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    if (p_sthing->Type != SmTT_BANG) {
                        remove_sthing(p_sthing->ThingOffset);
                        delete_snode(p_sthing);
                    }
                    thing = p_sthing->Next;
                }
            }
        }
    }

    // Reset per-tile collision data,
    for (tl_z = 0; tl_z < MAP_TILE_WIDTH; tl_z++)
    {
        for (tl_x = 0; tl_x < MAP_TILE_WIDTH; tl_x++)
        {
            struct MyMapElement *p_mapel;
            ThingIdx thing;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];
            p_mapel->Flags |= 0x80;
            p_mapel->ColHead = 0;
            p_mapel->ColumnHead = 0;

            thing = p_mapel->Child;
            if (thing > 0)
            {
                struct Thing *p_thing;
                p_thing = &things[thing];
                if (p_thing->Type != SmTT_BANG)
                    LOGWARN("Still thing on mapwho type %d", (int)p_thing->Type);
            }
            else if (thing < 0)
            {
                struct SimpleThing *p_sthing;
                p_sthing = &sthings[thing];
                if (p_sthing->Type != SmTT_BANG)
                    LOGWARN("Still simple on mapwho type %d", (int)p_sthing->Type);
            }
        }
    }
}

/** Fields of things which only drawing writes, or which only drawing reads
 * (computed from what drawing changes); the game plays the same without
 * them, so the state hash leaves them out.
 */
struct RenderField {
    ushort type; /**< Thing type the field (or union member) belongs to, or RenderFieldTypes */
    ushort offset;
    ushort size;
    u32 bits;   /**< Bits of the field (as a little endian number) to leave out */
};

/** Types in RenderField: a thing type, or one of these classes. */
enum RenderFieldTypes {
    /** Every type whose state has no matrix (not people, vehicles,
     * buildings and guns): shots and other effects */
    RFT_NO_MATRIX = 0x100,
    /** Every type whose state does not reach DrawTurn (not people and
     * vehicles, whose fields there are game state) */
    RFT_NO_STATE_AT_DRAWTURN,
};

static const struct RenderField state_hash_render_fields[] = {
    // the turn a building was last drawn; also left over in what reuses
    // the slot, e.g. a destroyed building or razor wire
    {RFT_NO_STATE_AT_DRAWTURN, offsetof(struct Thing, U.UObject.DrawTurn), sizeof(u32), 0xFFFFFFFFu},
    // the temporary matrix drawing gives a rocket (build_rocket()); also
    // left over in what reuses the slot, e.g. a laser or a grenade
    {RFT_NO_MATRIX, offsetof(struct Thing, U.UObject.MatrixIndex), sizeof(short), 0xFFFFu},
    // light level of a person from the map shading, which drawing changes
    {TT_PERSON, offsetof(struct Thing, U.UPerson.Brightness), sizeof(ubyte), 0xFFu},
    // hit flash: set by hits, read and cleared by drawing the object
    {TT_VEHICLE, offsetof(struct Thing, Flag), sizeof(u32), TngF_Unkn01000000},
    {TT_BUILDING, offsetof(struct Thing, Flag), sizeof(u32), TngF_Unkn01000000},
    {TT_ROCKET, offsetof(struct Thing, Flag), sizeof(u32), TngF_Unkn01000000},
    // the turn razor wire was last drawn (build_razor_wire()); also left
    // over in what reuses the slot, e.g. a guided laser
    {RFT_NO_MATRIX, offsetof(struct Thing, U.UEffect.LeisurePlace), sizeof(ushort), 0xFFFFu},
};

static TbBool render_field_type_matches(ushort rftype, ubyte type)
{
    switch (rftype)
    {
    case RFT_NO_MATRIX:
        return (type != TT_PERSON) && (type != TT_UNKN4) && (type != TT_VEHICLE)
          && (type != TT_BUILDING) && (type != TT_UNKN55) && (type != TT_VEH_TURRET);
    case RFT_NO_STATE_AT_DRAWTURN:
        return (type != TT_PERSON) && (type != TT_UNKN4) && (type != TT_VEHICLE);
    default:
        return (type == rftype);
    }
}

/** Clears the bytes of render-only fields within the word at `off`; they
 * need not be word aligned.
 */
static u32 state_hash_mask_render_fields(const struct Thing *p_thing, size_t off, u32 w)
{
    size_t i, b;

    for (i = 0; i < sizeof(state_hash_render_fields) / sizeof(state_hash_render_fields[0]); i++)
    {
        const struct RenderField *p_rf = &state_hash_render_fields[i];

        if (!render_field_type_matches(p_rf->type, p_thing->Type))
            continue;
        for (b = 0; b < 4; b++) {
            if ((off + b >= p_rf->offset) && (off + b < (size_t)p_rf->offset + p_rf->size)) {
                u32 fbits = (p_rf->bits >> (8 * (off + b - p_rf->offset))) & 0xFFu;
                w &= ~(fbits << (8 * b));
            }
        }
    }
    return w;
}

/** Hash of the world state: the things memory, with PTarget, the only
 * pointer in a thing, hashed as a thing index (runs place the things memory
 * apart), and the drawing-driven Frame, Timer1 and render-only fields
 * (state_hash_render_fields) left out. The same on every target and
 * pointer size; the state traces of the builds are compared on it.
 */
u32 game_state_hash(void)
{
    const u32 *p, *beg;
    ThingIdx thing;
    u32 h;

    beg = (const u32 *)((ubyte *)sthings - STHINGS_LIMIT * sizeof(struct SimpleThing));
    h = 2166136261u;
    for (p = beg; p < (const u32 *)things; p++)
        h = (h ^ *p) * 16777619u;
    // Things word by word as in the 32-bit layout: the pointer PTarget is
    // one word whatever its size, so 32 and 64-bit builds hash the same
    for (thing = 0; thing < THINGS_LIMIT; thing++)
    {
        const ubyte *p_base = (const ubyte *)&things[thing];
        size_t off;

        for (off = 0; off < sizeof(struct Thing); off += 4)
        {
            u32 w;
            if (off == offsetof(struct Thing, PTarget)) {
                struct Thing *p_target = things[thing].PTarget;
                w = (p_target == NULL) ? 0xFFFFFFFFu : (u32)(ThingIdx)(p_target - things);
                off += sizeof(p_target) - 4;
            } else {
                memcpy(&w, p_base + off, sizeof(w));
            }
            // Frame and its Timer1 advance when the thing is drawn, and
            // drawing depends on timing; keep StartFrame and StartTimer1
            if ((off == offsetof(struct Thing, Frame)) ||
              (off == offsetof(struct Thing, Timer1)))
                w &= 0xFFFF0000u;
            w = state_hash_mask_render_fields(&things[thing], off, w);
            h = (h ^ w) * 16777619u;
        }
    }
    return h;
}

/******************************************************************************/
