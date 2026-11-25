/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_coreinit.c
 *     Setting up the game core and a mission, without the front end.
 * @par Purpose:
 *     The core's steps of the game setup (game_setup(), setup_host(),
 *     prep_single_mission() in game.c call them between their front-end
 *     steps: screen, sprites, fonts, sound, menus), and all of them in a
 *     row for a front end without screens.
 * @par Comment:
 *     A front end without screens sets up the same game: the desktop's
 *     steps between the core's change nothing the game turns read.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_coreinit.h"

#include <stdio.h>
#include "bfdata.h"
#include "bffile.h"
#include "bfmemory.h"

#include "engincam.h"
#include "engincolour.h"
#include "enginzoom.h"

#include "enginfexpl.h"
#include "enginpriobjs.h"
#include "enginsngtxtr.h"
#include "engindrwlstx.h"
#include "engintxtrmap.h"
#include "frame_sprani.h"

#include "bigmap.h"
#include "bmbang.h"
#include "campaign.h"
#include "cybmod.h"
#include "difftest.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_world.h"
#include "lvobjctv.h"
#include "people.h"
#include "player.h"
#include "rules.h"
#include "thing.h"
#include "thing_expld.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/

int data_1c8428;

/** Number of texture atlas pages in the game data (data/texNN.dat); the
 * software renderer loads that many (load_texturemaps()).
 */
static int texturemap_pages = 0;

/** The animation data the game turns read (the frames of people, vehicles
 * and effects; the sprites they point to are the front end's), and the
 * palette: the colours of sparks and other effects are game state
 * (colour_lookup[], setup_color_lookups()).
 */
static struct TbLoadFiles core_load_files[] =
{
  { "data/mele-0.ani",	(void **)&melement_ani,		(void **)&mele_ani_end,		0, 0, 0 },
  { "data/nsta-0.ani",	(void **)&nstart_ani,		(void **)&nstart_ani_end,	0, 0, 0 },
  { "data/nfra-0.ani",	(void **)&frame,			(void **)&frame_end,		0, 0, 0 },
  { "qdata/pal.pal",	(void **)&display_palette,	(void **)NULL,				0, 0, 0 },
  { "",					(void **)NULL, 				(void **)NULL,				0, 0, 0 }
};

TbBool game_core_setup_data(void)
{
    TbBool ret;

    ret = true;
    texturemap_pages = existing_texturemaps();
    engine_mem_alloc_ptr = LbMemoryAlloc(engine_mem_alloc_size);
    if (engine_mem_alloc_ptr == NULL) {
        LOGERR("Allocating engine memory failed.");
        ret = false;
    }
    if (LbDataLoadAll(core_load_files) != 0) {
        LOGERR("Loading of animation files failed.");
        ret = false;
    }
    if (!read_rules_file()) {
        LOGERR("Rules config file loading failed.");
        ret = false;
    }
    read_weapons_conf_file();
    read_cybmods_conf_file();
    bang_init();
    init_object_explode_faces();
    init_search_spiral();
    bang_set_detail(0);
    FIRE_init();
    return ret;
}

void game_core_free_data(void)
{
    LbDataFreeAll(core_load_files);
}

TbBool game_core_setup_memory(void)
{
    TbBool ret;

    ret = true;
    if (init_memory(mem_game) == Lb_FAIL) {
        LOGERR("Init memory failed.");
        ret = false;
    }
    propagate_memory_sizes();
    return ret;
}

void game_core_setup_world(void)
{
    setup_color_lookups();
    init_things();
    load_peep_type_stats();
    load_campaigns();
    player_mission_agents_toggle_reset(local_player_no);
    if (is_single_game || cmdln_param_bcg)
    {
        load_prim_quad();
    }
    engn_xc = TILE_TO_MAPCOORD(60, 128);
    engn_zc = TILE_TO_MAPCOORD(55, 128);
    engn_yc = 0;
    ingame.TrackX = engn_xc;
    ingame.TrackZ = engn_zc;
    ingame.NextRocket = 0;
    ingame.Flags |= GamF_HUDPanel | TngF_ProgressAction | GamF_RenderScene;
}

TbBool game_core_setup(void)
{
    TbBool ret;

    ret = game_core_setup_data();
    if (!game_core_setup_memory())
        ret = false;
    game_core_setup_world();
    return ret;
}

void game_core_view_setup(void)
{
    // Set scale 15% over the min, to create a nice pan effect
    overall_scale = (get_overall_scale_min() * 295) >> 8;
    render_area_a = render_area_b = get_render_area_for_zoom(user_zoom_min);
}

void game_core_mission_load(void)
{
    load_missions(background_type);
    load_objectives_text();
}

void game_core_mission_init(void)
{
    init_game(0);
    preprogress_game_turns();
}

void load_prim_quad(void)
{
    unkn2_pos_x = 64;
    unkn2_pos_y = 64;
    unkn2_pos_z = 64;
    data_1c8428 = 0;
    prim_unknprop01 = 1000;
    read_primveh_obj(primvehobj_fname, 1);
    read_textwalk();
    engine_render_lights = 1;
    ingame.DisplayMode = DpM_PURPLEMNU;
    // the original called test_open(99) or test_open(100) here, for
    // cmdln_param_bcg 99 or 100; test_open() does nothing
}

static DIFFTEST_REF_FUNC void bang_set_detail_asm(int a1)
{
    DIFFTEST_ASM("call ASM_bang_set_detail\n"
        : : "a" (a1));
}

/** Set explosion detail parameters; detail=0 high detail, detail=1 low detail.
 */
void bang_set_detail(int detail)
{
    DIFFTEST_SHADOW(bang_set_detail, bang_set_detail_asm(detail));
    if (detail == 0)
    {
        bangdetail.circle[0].dangle = 0x200;
        bangdetail.circle[0].babynumber = 4;
        bangdetail.circle[1].dangle = 0xe3;
        bangdetail.circle[1].babynumber = 9;
        bangdetail.circle[2].dangle = 0x78;
        bangdetail.circle[2].babynumber = 0x11;
        bangdetail.circle[3].dangle = 0x46;
        bangdetail.circle[3].babynumber = 0x1d;
        bangdetail.sphere[0].babynumber = 3;
        bangdetail.sphere[0].babyshift = 2;
        bangdetail.sphere[0].dangley = 0x100;
        bangdetail.sphere[1].babynumber = 5;
        bangdetail.sphere[1].babyshift = 4;
        bangdetail.sphere[1].dangley = 0x80;
        bangdetail.sphere[2].babyshift = 4;
        bangdetail.sphere[2].dangley = 0x80;
        bangdetail.sphere[3].babyshift = 5;
        bangdetail.sphere[2].babynumber = 8;
        bangdetail.sphere[3].babynumber = 0xd;
        bangdetail.sphere[3].dangley = 0x66;
        bangdetail.mask = 1;
        minimum_explode_and = 0x1f;
        minimum_explode_depth = 1;
        minimum_explode_size = 0x1bc;
    }
    else if (detail == 1)
    {
        bangdetail.circle[0].dangle = 0x2aa;
        bangdetail.circle[0].babynumber = 3;
        bangdetail.circle[1].dangle = 0x199;
        bangdetail.circle[1].babynumber = 5;
        bangdetail.circle[2].dangle = 0xba;
        bangdetail.circle[2].babynumber = 0xb;
        bangdetail.circle[3].dangle = 0x78;
        bangdetail.sphere[0].babynumber = 3;
        bangdetail.sphere[1].babynumber = 3;
        bangdetail.circle[3].babynumber = 0x11;
        bangdetail.sphere[0].babyshift = 2;
        bangdetail.sphere[0].dangley = 0x100;
        bangdetail.sphere[1].babyshift = 2;
        bangdetail.sphere[1].dangley = 0x100;
        bangdetail.sphere[2].babyshift = 4;
        bangdetail.sphere[3].babyshift = 4;
        bangdetail.mask = 3;
        bangdetail.sphere[2].babynumber = 5;
        bangdetail.sphere[2].dangley = 0x80;
        bangdetail.sphere[3].babynumber = 8;
        bangdetail.sphere[3].dangley = 0x80;
        minimum_explode_and = 0xf;
        minimum_explode_size = 0x457;
        minimum_explode_depth = 2;
    }
}

TbBool texturemap_page_exists(ushort page)
{
    return (page < VEC_TMAP_LIMIT) && (page < texturemap_pages);
}

int existing_texturemaps(void)
{
    char locstr[DISKPATH_SIZE];
    int i;

    for (i = 0; ; i++)
    {
        sprintf(locstr, "data/tex%02d.dat", i);
        if (!LbFileExists(locstr))
          break;
    }
    return i;
}

/******************************************************************************/
