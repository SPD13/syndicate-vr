/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game.c
 *     Game loop and high level functions.
 * @par Purpose:
 *     Implement functions for high level flows of the game application.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 22 Oct 2023
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
#include "vrspectate.h"
#include "weapon.h"
#include "wrcities.h"
#include "game_camera.h"
#include "game_coreinit.h"
#include "game_misend.h"
#include "game_turn.h"
#include "game_world.h"

/** How many game turns should pass before intro is replayed.
 */
#define INTRO_REPLAY_TURNS 1100

enum PostRenderAction {
    PRend_NONE = 0,
    PRend_SaveScreenshot,
};

enum ReloadMenuFlags {
    RelMnuF_ColorsSprites = 0x01,
    RelMnuF_BriefScanner = 0x02,
    RelMnuF_ScrBoxesFull = 0x04,
};

#pragma pack(1)

/** A character of the outro text which was already drawn, and is still
 * bright enough to be worth drawing again while it fades out. */
struct OutroHotChar {
    s32 x;
    s32 y;
    ubyte chr;
    /** Brightness, used as index within pixmap.fade_table[]. */
    ubyte fade_lv;
    /** Font to draw with; 1 selects big_font, anything else med2_font. */
    ubyte font;
    ubyte field_B;
};

#pragma pack()

extern char *fadedat_fname;





TbPixel linear_vec_pal[PALETTE_8b_COLORS];


extern s32 dword_152E38[5]; // = {-1, -1, -1, -1, -1,};

u32 active_flags_general_unkn01 = 0;

ubyte mouse_sprite_anim_frame;
ushort replay_intro_timer = 0;

extern s32 dword_1DDECC;
extern ushort data_1c8468;

extern struct GamePanel unknstrct7_arr2[];



s32 flyby_cam_x = TILE_TO_MAPCOORD(MAP_TILE_WIDTH/2, 0);
s32 flyby_cam_z = TILE_TO_MAPCOORD(MAP_TILE_HEIGHT/2, 0);
s32 flyby_cam_y = TILE_TO_MAPCOORD(0, 50);

ubyte simulated_mouse_click;
ubyte clear_left_button;
ubyte clear_right_button;
ubyte clear_wheel_up;
ubyte clear_wheel_down;
ubyte post_render_action;




char mission_status_text[100];

ubyte byte_1C6D4A;


s32 flyby_cam_yaw = 0;
s32 flyby_cam_tilt = 0;

ubyte old_screentype;
ubyte screentype = 0;
ubyte data_1c498d = 0;



ubyte start_into_mission = false;
ubyte edit_flag = 0;
ubyte change_screen = 0;

ubyte input_char;

/** Miscellaneous text strings.
 *
 * While initialized by constants, it cannot be const array
 * due to where the items are used.
 */
char *misc_text[] = {
  "0",
  "C",
  "/",
  "NC",
  "REF:",
};

/** List of sprites, palettes and animations required to be loaded for the game engine.
 *
 * For files with several variants, it is ought to load the largest - the other
 * files will be stored in the same buffer upon switch, so the buffer allocated
 * here must fit all the variants.
 */
struct TbLoadFiles missionspr_load_files[] =
{
  { "*VESA",			(void **)&lbVesaData,		(void **)NULL,LB_VESA_DATA_SIZE, 1, 0 },
#if 1 // !defined(LB_WSCREEN_CONTROL)
  { "*W_SCREEN",		(void **)&lbDisplay.WScreen,(void **)NULL,MAX_SUPPORTED_SCREEN_WIDTH*(MAX_SUPPORTED_SCREEN_HEIGHT+1), 0, 0 },
#endif
  { "data/pop2-3.dat",	(void **)&pop1_data,		(void **)&pop1_data_end,	0, 0, 0 },
  { "data/pop2-3.tab",	(void **)&pop1_sprites,		(void **)&pop1_sprites_end, 0, 0, 0 },
  { "data/mspr-0.dat",	(void **)&m_sprites_data,	(void **)&m_sprites_data_end,0, 0, 0 },
  { "data/mspr-0.tab",	(void **)&m_sprites,		(void **)&m_sprites_end,	0, 0, 0 },
  { "data/fontc0-7.dat",(void **)&small_font_data,	(void **)&small_font_data_end,0, 0, 0 },
  { "data/fontc0-7.tab",(void **)&small_font,		(void **)&small_font_end,	0, 0, 0 },
  { "data/pointr0-3.dat",(void **)&pointer_data,	(void **)&pointer_data_end,	0, 0, 0 },
  { "data/pointr0-3.tab",(void **)&pointer_sprites,	(void **)&pointer_sprites_end, 0, 0, 0 },
  { "",					(void **)NULL, 				(void **)NULL,				0, 0, 0 }
};

ubyte reload_menu_flags = 0;

/** Arc tangent factors table from bflibrary; identical to the asm table. */
extern unsigned short const lbArcTanFactors[];

void debug_trace_place(int place)
{
    LOGDBG("reached place %d", place);
}

bool game_initialise(void)
{
    TbResult ret;
    ret = LbBaseInitialise();
    if (ret != Lb_SUCCESS)
    {
        LOGERR("Bullfrog Library initialization failed");
        return false;
    }

#ifdef __unix__
    /* clean up after SDL messing around where it shouldn't */
    unix_restore_signal_handlers();
#endif

    LbSetUserResourceMapping(SWResourceMapping);
    LbSetTitle(PACKAGE_NAME);
    LbSetIcon(1);

    // Make sure file names are properly converted before opening
    setup_file_names();

    return true;
}

void game_handle_sdl_events(void)
{
    TbBool contn;
    contn = LbWindowsControl();
    if (!contn) {
        game_quit();
    }
}

int reload_texturemaps(void)
{
    char locstr[DISKPATH_SIZE];
    int i;

    for (i = 0; vec_tmap[i] != NULL; i++)
    {
        sprintf(locstr, "data/tex%02d.dat", i);
        LbFileLoadAt(locstr, vec_tmap[i]);
    }
    return i;
}

void load_texturemaps(void)
{
    if (vec_tmap[0] == NULL)
    {
        int tmap_count;
        tmap_count = existing_texturemaps();
        if (!alloc_texturemaps(tmap_count)) {
            exit_game = 1;
            return;
        }
        shadow_tmap_page = tmap_count - 1;
    }
    reload_texturemaps();
}

void test_open(int num)
{
    // Empty for production version
}

/** Remains of some Bf debug stuff.
 */
void debug_trace_turn_bound(u32 turn)
{
    LOGDBG("turn %u", turn);
}

/** NOOP func which compiler merged from several functions.
 */
void merged_noop_unkn1(int a1)
{
}

void colour_tables_ghost_fixup(void)
{
    TbPixel *ipal;
    TbPixel *opal;
    ushort i;

    ipal = &pixmap.fade_table[63*PALETTE_8b_COLORS];
    opal = &pixmap.ghost_table[0*PALETTE_8b_COLORS];
    for (i = 0; i < PALETTE_8b_COLORS; i++)
        opal[i] = ipal[i];
}

TbBool game_setup_stuff(void)
{
    ushort i;
    TbBool ret;

    ret = true;
    for (i = 0; i < PALETTE_8b_COLORS; i++)
        linear_vec_pal[i] = i;
    vec_pal = linear_vec_pal;

    if (display_palette != NULL) {
        colour_brown2 = LbPaletteFindColour(display_palette, 42, 37, 30);
        colour_grey2 = LbPaletteFindColour(display_palette, 32, 32, 32);
        colour_grey1 = LbPaletteFindColour(display_palette, 16, 16, 16);
    } else {
        colour_brown2 = 60;
        colour_grey2 = 61;
        colour_grey1 = 62;
        LOGERR("Display palette not set, using dummies");
        ret = false;
    }

    return ret;
}

void check_mouse_overvehicle(struct Thing *p_thing, ubyte target_assign)
{
    struct ShEnginePoint sp;
    int cor_x, cor_y, cor_z;
    int ms_dx, ms_dy, tng_dim;

    cor_x = PRCCOORD_TO_MAPCOORD(p_thing->X) - engn_xc;
    cor_y = PRCCOORD_TO_YCOORD(p_thing->Y) - engn_yc;
    cor_z = PRCCOORD_TO_MAPCOORD(p_thing->Z) - engn_zc;

    transform_shpoint(&sp, cor_x, cor_y - 8 * engn_yc, cor_z);

    ms_dx = lbDisplay.MMouseX - sp.X;
    ms_dy = lbDisplay.MMouseY - sp.Y;
    tng_dim = ((p_thing->Radius >> 3) * overall_scale) >> 8;

    if (ms_dy * ms_dy + ms_dx * ms_dx < tng_dim * tng_dim)
    {
        PlayerInfo *p_locplayer;
        p_locplayer = &players[local_player_no];
        p_locplayer->Target = p_thing->ThingOffset;
        p_locplayer->TargetType = target_assign;
    }
}


ubyte get_engine_inputs(void)
{
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    if (is_gamekey_pressed(GKey_TRANS_OBJ_SURF_COL))
    {
        if (lbShift & KMod_SHIFT)
            game_option_inc(GOpt_TranspObjSurfaceColr);
        else
            game_option_dec(GOpt_TranspObjSurfaceColr);
        clear_gamekey_pressed(GKey_TRANS_OBJ_SURF_COL);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_TRANS_OBJ_LINE_COL))
    {
        if (lbShift & KMod_SHIFT)
            game_option_inc(GOpt_TranspObjLineColr);
        else
            game_option_dec(GOpt_TranspObjLineColr);
        clear_gamekey_pressed(GKey_TRANS_OBJ_LINE_COL);
        did_inp |= GINPUT_DIRECT;
    }
    if (game_perspective == ProjM_IsomObjWirefr)
    {
        if (lbKeyOn[KC_RALT]) {
            render_faces_flags |= RendFacF_Perspectv3SkipWireframe;
            did_inp |= GINPUT_DIRECT;
        } else {
            render_faces_flags &= ~RendFacF_Perspectv3SkipWireframe;
        }
    }
    return did_inp;
}

/** Whether the world is drawn while a mission is played; a front end that
 * draws it by itself, or a run without display, switches it off.
 */
TbBool game_draw_world = true;

void process_engine_unk3(void)
{
    get_engine_inputs();
    // the craters, the mechs and what drawing the things in view changed
    // in the game, before drawing
    game_turn_world_view();
    engine_draw_whole_screen_top_down();

    // Which frame of an animated texture is on screen is not part of the game
    // state - no simulation code reads it. Advanced here, after the frame was
    // enlisted, it keeps the pace it had within process_things().
    if (game_draw_world)
        animate_textures();
}

/**
 * Updates engine parameters for best display for current video mode within the tactical mission.
 */
TbBool adjust_mission_engine_to_video_mode(void)
{
    TbBool ret;

    ret = true;
    game_high_resolution = (lbDisplay.ScreenMode == screen_mode_game_hi);

    // loads sprites for the panel, but also panel config file, and stretches to actual resolution
    if (load_pop_sprites_for_current_mode() == Lb_FAIL)
        ret = false;
    if (load_mouse_pointers_sprites_for_current_mode() == Lb_FAIL)
        ret = false;
    if (load_small_font_for_current_ingame_mode() == Lb_FAIL)
        ret = false;

    // the camera scale and the view area, from the screen size
    game_core_view_setup();
    init_scanner_colour();
    srm_scanner_size_update();

    return ret;
}

void show_simple_load_screen(void)
{
    // TODO TEXT instead of repeating "Initiating.." show "Established"
    char *text = gui_strings[376];
    int w,h;

    LbScreenClear(0);
    lbFontPtr = small_font;
    w = LbTextStringWidth(text);
    h = LbTextStringHeight(text);
    LbTextDraw((lbDisplay.GraphicsScreenWidth - w) >> 1,
      (lbDisplay.GraphicsScreenHeight - h) >> 1, text);
    swap_wscreen();
}

void setup_engine_screen_mode(void)
{
    LbFileLoadAt("qdata/pal.pal", display_palette);
    LbPaletteSet(display_palette);

    if (game_high_resolution)
    {
        if (lbDisplay.ScreenMode != screen_mode_game_hi)
            setup_screen_mode(screen_mode_game_hi);
    }
    else
    {
        if (lbDisplay.ScreenMode != screen_mode_game_lo)
            setup_screen_mode(screen_mode_game_lo);
    }
    // Setup colour conversion tables
    LbColourTablesLoad(display_palette, "data/tables.dat");
    LbGhostTableLoad(display_palette, 50, "data/synghost.tab");
}

void setup_initial_screen_mode(void)
{
    if (game_high_resolution)
        lbDisplay.ScreenMode = screen_mode_game_hi;
    else
        lbDisplay.ScreenMode = screen_mode_game_lo;
    setup_simple_screen_mode(lbDisplay.ScreenMode);
    show_black_screen();
    // Setup colour conversion tables, allowing generation
    if (display_palette != NULL)
    {
        LbColourTablesLoad(display_palette, "data/tables.dat");
        LbGhostTableGenerate(display_palette, 50, "data/synghost.tab");
    }
    else
    {
        LOGERR("Palette not ready - skipping colour tables setup");
    }
    if (screen_idle_update_initialize() != Lb_SUCCESS)
    {
        LOGERR("Idle screen update not registered - expect choppy mouse");
    }
#if 0
    // Not sure why we would do this fixup. Maybe it's only for
    // sprite generation? If used before gameplay, it causes bad
    // while fading beyond map borders (always fades to full white)
    colour_tables_ghost_fixup();
#endif
}

void screen_mode_switch_to_next(void)
{
    TbScreenMode nmode;

    if (lbDisplay.ScreenMode == screen_mode_game_lo)
        nmode = screen_mode_game_hi;
    else
        nmode = screen_mode_game_lo;
    LOGSYNC("Switching to mode %d", (int)nmode);

    setup_screen_mode(nmode);
    adjust_mission_engine_to_video_mode();
}

ubyte game_graphics_inputs(void)
{
    PlayerInfo *p_locplayer;
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    p_locplayer = &players[local_player_no];
    if ((ingame.DisplayMode != DpM_ENGINEPLY) &&
        (ingame.DisplayMode != DpM_UNKN_3B))
        return did_inp;
    if (in_network_game && p_locplayer->PanelState[mouser]
      == PANEL_STATE_SEND_MESSAGE)
        return did_inp;

    if (is_gamekey_pressed(GKey_CAMERA_PERSPECTV))
    {
        clear_gamekey_pressed(GKey_CAMERA_PERSPECTV);
        game_option_inc(GOpt_CameraPerspective);
        did_inp |= GINPUT_DIRECT;
    }

    if (is_key_pressed(KC_F8, KMod_DONTCARE))
    {
        clear_key_pressed(KC_F8);
        screen_mode_switch_to_next();
        did_inp |= GINPUT_DIRECT;
    }

    return did_inp;
}

ubyte input_agents_cheats(void)
{
    PlayerInfo *p_locplayer;
    struct Packet *p_pckt;
    ThingIdx dcthing;
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    p_locplayer = &players[local_player_no];

    if (in_network_game && p_locplayer->PanelState[mouser]
      == PANEL_STATE_SEND_MESSAGE)
        return did_inp;

    if ((ingame.UserFlags & UsrF_Cheats) != 0)
    {
        if (is_key_pressed(KC_T, KMod_ALT))
        {
            clear_key_pressed(KC_T);

            p_pckt = &packets[local_player_no];
            dcthing = p_locplayer->DirectControl[0];
            my_build_packet(p_pckt, PAct_CHEAT_AGENT_TELEPORT, dcthing, mouse_map_x, 0, mouse_map_z);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
    }

    if ((ingame.UserFlags & UsrF_Cheats) && !in_network_game)
    {
        if (is_key_pressed(KC_Q, KMod_ALT))
        {
            clear_key_pressed(KC_Q);

            p_pckt = &packets[local_player_no];
            my_build_packet(p_pckt, PAct_CHEAT_ALL_AGENTS, PCheatAA_BEEFUP_AND_MODS, 0, 0, 0);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
        if (is_key_pressed(KC_Q, KMod_SHIFT))
        {
            clear_key_pressed(KC_Q);

            p_pckt = &packets[local_player_no];
            my_build_packet(p_pckt, PAct_CHEAT_ALL_AGENTS, PCheatAA_RESURRECT_AND_WEPAPN, 0, 0, 0);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
    }

    return did_inp;
}

void init_syndwars(void)
{
    //char locstr[100];
    AudioInitOptions audOpts;

    LOGSYNC("Starting");
    //sprintf(locstr, "%sSound", cd_drive); -- unused

    audOpts.SoundDataPath = "sound";
    audOpts.SoundDriverPath = "sound";
    audOpts.IniPath = "sound";
    audOpts.AutoScan = 1;
    audOpts.StereoOption = 1;
    audOpts.DisableLoadSounds = 1;
    audOpts.InitRedbookAudio = 2;
    audOpts.UseCurrentAwe32Soundfont = 1;
    audOpts.AbleFlags = 3;
    audOpts.SoundType = 1622;
    audOpts.MaxSamples = 10;
    InitAudio(&audOpts);

    if (!GetCDAble())
        StopMusicIfActive();
    fill_ail_sample_ids();
    if (GetSoundInstalled() && GetSoundAble() && GetSoundActive())
        ingame.Flags |= GamF_Unkn00020000;
    setup_heaps(SHSC_GameSndAutoQ, language_3str);
}

static DIFFTEST_REF_FUNC void setup_host_sub6_asm(void)
{
    DIFFTEST_ASM("call ASM_setup_host_sub6\n"
        :  :  : "eax" );
}

void setup_host_sub6(void)
{
    uint k;

    DIFFTEST_SHADOW(setup_host_sub6, setup_host_sub6_asm());
    for (k = 0; k < 2; k++)
    {
        struct Animation *p_anim;
        s32 val;

        p_anim = &animations[k];
        p_anim->FrameNumber = 0;
        p_anim->Flags = 0;
        p_anim->Type = 0;
        // The original reads Ypos and FrameNumber as one dword, then shifts
        val = *(s32 *)&p_anim->Ypos;
        p_anim->Xpos = 0;
        val >>= 16;
        p_anim->Ypos = 0;
        p_anim->anfield_4 = val;
        p_anim->FileHandle = (TbFileHandle)-1;
        p_anim->Filename[0] = (ubyte)p_anim->Type;
    }
}

void setup_debug_obj_trace(void)
{
    // Code for setting memory ranges was here
    // Removed, as we no longer have such tracking/profiling mechanism
    debug_trace_place(0);
}

TbResult prep_multicolor_sprites(void)
{
    PathInfo *pinfo;
    TbResult ret;

    pinfo = &game_dirs[DirPlace_Data];
    ret = load_multicolor_sprites(pinfo->directory);
    setup_multicolor_sprites();
    if (ret == Lb_FAIL) {
        LOGERR("Some files were not loaded successfully");
    }
    return ret;
}

TbBool setup_host(void)
{
    TbBool ret;

    ret = true;
    BAT_unknsub_20(0, 0, 0, 0, vec_tmap[4] + 160 * 256 + 64);
    smack_malloc_setup();
    LOGDBG("&setup_host() = 0x%p", (void *)setup_host);
    setup_initial_screen_mode();

    if (keyboard_mode_direct)
        LbKeyboardOpen();
    else
        LbIKeyboardOpen();
    init_buffered_keys();

    setup_mouse_pointers();
    lbMouseAutoReset = false;
    mouse_setup(true);

    setup_debug_obj_trace();

    // Default Trenchcoat color loaded from missionspr_load_files[]
    // should be set earlier with default user options
    assert(ingame.TrenchcoatPreference == 0);
    setup_multicolor_sprites();
    assert(ingame.PanelPermutation == -2);

    if (load_pop_sprites_for_current_mode() == Lb_FAIL) {
        LOGERR("Load pop sprites failed.");
        ret = false;
    }
    if (load_mouse_pointers_sprites_for_current_mode() == Lb_FAIL) {
        LOGERR("Load mouse pinters sprites failed.");
        ret = false;
    }
    if (load_small_font_for_current_ingame_mode() == Lb_FAIL) {
        LOGERR("Load small font sprites failed.");
        ret = false;
    }
    if (!game_core_setup_memory())
        ret = false;

    init_syndwars();
    LoadSounds(0);
    LoadMusic(0);

    setup_host_sub6();
    play_intro();
    embanim_init();

    return ret;
}

void init_engine(void)
{
    p_current_sort_sprite = game_sort_sprites;
    p_current_draw_item = game_draw_list + 1;

    calc_bul_offsets();

    // the starting camera, the next rocket and the game flags this set
    // are set by game_core_setup_world()
    setup_vecs(lbDisplay.WScreen, vec_tmap[0], lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenWidth, lbDisplay.PhysicalScreenHeight);
}



int is_animating(int texture);


extern short data_1c42fe;
extern short data_1c4300;


//TODO change ret type to ThingIdx, when no longer used in ASM

extern ushort word_1C83E2;
extern ushort word_1C83E8;


struct Thing *find_unused_train_track(void);

void init_random_seed(void)
{
    if (in_network_game)
    {
        mission_open[1] = ingame.CurrentMission;
        mission_state[1] = MResol_UNDECIDED;
        mission_open[2] = 0;
        mission_state[2] = MResol_UNDECIDED;

        if (net_local_player_hosts_the_game()) {
            lbSeed = time(0);
        }
        net_immediate_random_seed_sync();
    }
    else
    {
        lbSeed = time(0);
#if DIFFTEST_HARNESS
        // fixed seed, for comparing whole runs of two builds
        if (getenv("SWARS_SEED") != NULL)
            lbSeed = strtoul(getenv("SWARS_SEED"), NULL, 0);
#endif
    }
    game_srand(lbSeed);
}

void prep_single_mission(void)
{
    game_core_mission_load();
    if (PacketRecord_IsPlayback())
    {
        PacketRecord_OpenRead();
    }

    // TODO this code repeats for starting game from menu, close into one function
    if (PacketRecord_IsPlayback())
    {
        packet_read_whole_player_init();
    }
    game_core_mission_init();
    if (!in_network_game)
    {
        if (PacketRecord_IsRecord()) {
            PacketRecord_OpenWrite();
            packet_write_whole_player_init();
        }
    }
    prep_multicolor_sprites();
    LbScreenClear(0);
    generate_shadows_for_multicolor_sprites();
    adjust_mission_engine_to_video_mode();

    embanim_reinit(AniSl_BILLBOARD);
    embanim_do_next_frame(AniSl_BILLBOARD);
}

void create_tables_file_from_fade(void)
{
    s32 len;
    int i, k;
    unsigned char *curbuf;
    // Note that the input file is not normally available with the game
    len = LbFileLoadAt("data/fade.dat", scratch_buf1);
    if (len == Lb_FAIL) {
        LOGERR("Cannot find input file for tables update");
        return;
    }
    curbuf = scratch_buf1;
    for (i = 0; i < 256; i++) {
        for (k = 0; k < 64; k++) {
          pixmap.fade_table[256 * k + i] = *(curbuf + (256+64) * k + i);
        }
    }
    LbFileSaveAt("data/tables.dat", &pixmap, sizeof(pixmap));
}

/** Universal way to generate the colour tables.
 */
void create_tables_file_from_palette(void)
{
    LbColourTablesGenerate(display_palette, fade_unaffected_colours,
      "data/tables.dat");
}

/** Shows a screen with startup fail message.
 *
 * This special screen requires zero data files, so it can shown even
 * if the data load has failed. It is supposed to be the last information
 * message before the game terminates.
 */
void game_startup_fail_screen(void)
{
    ubyte vga_pal[PALETTE_8b_SIZE];
    TbBool was_locked;
    int i, step;
    short cent_x, cent_y, circle_diam;
    short base_x, base_y, embeded_square_dim;
    TbPixel col_grey1, col_grey2, col_white, col_blue1;

    was_locked = LbScreenIsLocked();
    if (!was_locked) {
        while (LbScreenLock() != Lb_SUCCESS)
            ;
    }

    if (lbDisplay.WScreen == NULL) {
        if (!was_locked)
            LbScreenUnlock();
        return;
    }

    lbDisplay.DrawFlags = 0;
    LbPaletteDataFillDefVGA(vga_pal);
    LbPaletteSet(vga_pal);
    LbScreenClear(0);

    col_white = LbPaletteFindColour(vga_pal, 63, 63, 63);
    col_grey2 = LbPaletteFindColour(vga_pal, 32, 32, 32);
    col_grey1 = LbPaletteFindColour(vga_pal, 16, 16, 16);
    col_blue1 = LbPaletteFindColour(vga_pal,  0,  0, 16);

    cent_x = lbDisplay.GraphicsScreenWidth / 2;
    cent_y = lbDisplay.GraphicsScreenHeight / 2;
    circle_diam = min(lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight);

    step = circle_diam / 20;
    base_x = ((lbDisplay.GraphicsScreenWidth % step) / 2) - circle_diam*1/256;
    base_y = ((lbDisplay.GraphicsScreenHeight % step) / 2) - circle_diam*1/256;
    for (i = 0; i < lbDisplay.GraphicsScreenWidth; i += step) {
        LbDrawBox(base_x + i, 0, circle_diam*1/128, lbDisplay.GraphicsScreenHeight, col_grey2);
    }
    for (i = 0; i < lbDisplay.GraphicsScreenHeight; i += step) {
        LbDrawBox(0, base_y + i, lbDisplay.GraphicsScreenWidth, circle_diam*1/128, col_grey2);
    }

    LbDrawCircle(cent_x, cent_y, circle_diam*32/64, col_grey1);
    LbDrawCircle(cent_x, cent_y, circle_diam*31/64, col_blue1);

    embeded_square_dim = circle_diam * 128 / 181; // circle_diam divided by sqrt(2)

    LbPaletteDraw(cent_x - embeded_square_dim/2, cent_y - embeded_square_dim/2, embeded_square_dim/16, embeded_square_dim/16, 0);
    LbDrawBox(cent_x - embeded_square_dim/6, cent_y - circle_diam*2/64, embeded_square_dim * 2 / 6, circle_diam*5/64, 0);

    draw_simple_text(cent_x - 40, cent_y - circle_diam*1/64, "U.T.O.P.I.A.", col_white);
    draw_simple_text(cent_x - 40, cent_y - circle_diam*0/64, "Game startup failed!", col_white);
    draw_simple_text(cent_x - 40, cent_y + circle_diam*1/64, "Check \"error.log\".", col_white);

    if (!was_locked)
        LbScreenUnlock();
    swap_wscreen();

    for (i = 5*game_num_fps; i > 0; i--)
    {
        game_update();
    }
}

TbBool game_setup(void)
{
    TbBool ret;

    // the core's steps (game_coreinit.c) are game_core_setup_data(),
    // game_core_setup_memory() in setup_host() and game_core_setup_world()
    ret = game_core_setup_data();
    load_texturemaps();
    if (LbDataLoadAll(missionspr_load_files) != 0) {
        LOGERR("Initial loading of mission sprite files failed.");
        ret = false;
    }
    ingame.draw_unknprop_01 = 0;
    debug_trace_setup(-5);
    if (!game_setup_stuff()) {
        LOGERR("Setting up some misc stuff failed.");
        ret = false;
    }
    if (!create_strings_list(gui_strings, gui_strings_data, gui_strings_data_end)) {
        LOGERR("National strings list creation failed.");
        ret = false;
    }
    if (!setup_host())
        ret = false;
    debug_trace_setup(-4);
    read_user_settings();
    debug_trace_setup(-3);
    setup_sprites_small_font();
    debug_trace_setup(-2);
    game_core_setup_world();
    debug_trace_setup(0);
    init_engine();
    if ( !cmdln_param_bcg )
    {
        LbScreenClear(0);
        swap_wscreen();
        LbPaletteSet(display_palette);
    }
    test_open(15);
    debug_trace_setup(1);
    if (is_single_game && (ingame.CurrentMission != 0) && ret)
    {
        prep_single_mission();
    }
    if (in_network_game || cmdln_param_bcg) {
        ingame.DisplayMode = DpM_PURPLEMNU;
        reload_menu_flags |= RelMnuF_ColorsSprites;
    }
    debug_trace_setup(2);
    switch (cmdln_colour_tables)
    {
    case 1:
        create_tables_file_from_palette();
        break;
    case 2:
        create_tables_file_from_fade();
        break;
    }
    if (!ret) {
        LOGERR("Part of the game startup has failed. See the log lines above for a cause.");
        game_startup_fail_screen();
    }
    return ret;
}

void show_unkn3A_screen(int a1)
{
    // Empty
}

void compute_scanner_zoom(void)
{
    short zoom, scmin, scmax;

    if (ingame.Scanner.X2 > ingame.Scanner.X1)
        zoom = SCANNER_base_zoom_factor * 128 / (ingame.Scanner.X2 - ingame.Scanner.X1);
    else
        zoom = SCANNER_base_zoom_factor;
    scmin = get_overall_scale_min();
    scmax = get_overall_scale_max();
    if (scmax <= scmin)
        scmin = scmax - 1;
    zoom += SCANNER_user_zoom_factor * (user_zoom_max - get_unscaled_zoom(overall_scale)) / (scmax - scmin);
    SCANNER_set_zoom(zoom);
}

void recalc_mouse_pos(void)
{
    int cor_dx, cor_dy, cor_dz;
    int fctr_xz;
    int chk_x, chk_y, chk_z;
    short mag;
    short i;

    cor_dy = (transf_vec_tlt_y >> 8);
    fctr_xz = (transf_vec_tlt_xz >> 8);
    cor_dx = (fctr_xz * transf_vec_yaw_x) >> 16;
    cor_dz = (fctr_xz * transf_vec_yaw_z) >> 16;

    chk_x = 200 * cor_dx + 16 * mouse_map_x;
    chk_y = 200 * cor_dy;
    chk_z = 200 * cor_dz + 16 * mouse_map_z;

    mag = 0;
    for (i = 0; i < 400; i++)
    {
        if ( chk_y >> 4 < PRCCOORD_TO_YCOORD(alt_at_point(chk_x >> 4, chk_z >> 4)))
            mag = i;
        chk_x -= cor_dx;
        chk_y -= cor_dy;
        chk_z -= cor_dz;
    }

    if (mag != 0)
    {
        mag -= 200;
        mouse_map_x -= (mag * cor_dx) >> 4;
        mouse_map_z -= (mag * cor_dz) >> 4;
        mouse_map_y = alt_at_point(mouse_map_x, mouse_map_z) >> 8;
    }
}

void prepare_mouse_on_map(void)
{
    short msx, msy;
    int offs_y;
    int scr_x, scr_y;
    int map_dxc, map_dzc;

    msx = lbDisplay.MMouseX;
    msy = lbDisplay.MMouseY;

    if (ingame.DisplayMode == DpM_ENGINEPLY)
      offs_y = (engn_yc * overall_scale) >> 8;
    else
      offs_y = 0;

    scr_y = msy - offs_y;
    scr_x = msx;

    transform_screen_to_map_isometric(&map_dxc, &map_dzc, scr_x, scr_y);

    mouse_map_x = engn_xc + map_dxc;
    mouse_map_z = engn_zc + map_dzc;
    if (ingame.DisplayMode == DpM_ENGINEPLY)
        recalc_mouse_pos();
}

void show_game_engine(void)
{
    // the zoom, which the scanner follows (camera_apply_velocity() in
    // game_turn_camera() only changes the yaw)
    game_turn_camera();
    compute_scanner_zoom();
    // the camera transform, which the game also uses for the pointer
    prepare_drawlist();
    prepare_mouse_on_map();
    process_engine_unk3();
}

TbResult clear_vecs_screen(TbPixel colour)
{
    TbPixel *ptr;
    s32 h;

    ptr = vec_screen;
    if (ptr == NULL)
        return Lb_FAIL;
    for (h = vec_window_height; h >= 0; h--)
    {
        LbMemorySet(ptr, colour, vec_window_width);
        ptr += vec_screen_width;
    }
    return Lb_SUCCESS;
}

void gproc3_unknsub2(void)
{
    struct CameraState cam_bkp;
    short ms_x, ms_y, ms_limit;
    TbPixel *outbuf;
    int i;

    u32 bkp_ingame_flags;

    ingame.Flags &= ~GamF_BillboardMovies;
    if (is_key_pressed(KC_Q, KMod_DONTCARE))
    {
        flyby_cam_x = 0x4000;
        flyby_cam_z = 0x4000;
        flyby_cam_yaw = 0;
        flyby_cam_tilt = 0;
        flyby_cam_y = 50;
    }

    bkp_ingame_flags = ingame.Flags;

    camera_save_backup_state(&cam_bkp);

    render_area_a = 24;
    render_area_b = 24;
    overall_scale = 18;

    ingame.Flags = 0;

    ms_x = lbDisplay.MMouseX;
    ms_y = lbDisplay.MMouseY;

    ms_limit = lbDisplay.MouseWindowX + lbDisplay.MouseWindowWidth * 1 / 3;
    if (ms_x < ms_limit)
        flyby_cam_yaw -= 16;
    ms_limit = lbDisplay.MouseWindowX + lbDisplay.MouseWindowWidth * 2 / 3;
    if (ms_x > ms_limit)
        flyby_cam_yaw += 16;
    flyby_cam_yaw &= 0x7FF;

    if (!lbDisplay.MRightButton)
    {
        flyby_cam_x += lbSinTable[flyby_cam_yaw] >> 9;
        flyby_cam_z += lbSinTable[flyby_cam_yaw + LbFPMath_PI/2] >> 9;
    }

    ms_limit = lbDisplay.MouseWindowY + lbDisplay.MouseWindowHeight * 1 / 3;
    if (ms_y < ms_limit)
        flyby_cam_tilt -= (ms_limit - ms_y) >> 5;
    ms_limit = lbDisplay.MouseWindowY + lbDisplay.MouseWindowHeight * 2 / 3;
    if (ms_y > ms_limit)
        flyby_cam_tilt -= (ms_limit - ms_y) >> 5;
    if (flyby_cam_tilt > 300)
        flyby_cam_tilt = 300;
    if (flyby_cam_tilt < -300)
        flyby_cam_tilt = -300;

    i = (alt_at_point(flyby_cam_x, flyby_cam_z) >> 8) - 50;
    if (i > flyby_cam_y)
        flyby_cam_y = i;

    if (flyby_cam_x < 0)
        flyby_cam_x = 0x8000;
    if (flyby_cam_x > 0x8000)
        flyby_cam_x = 0;

    if (flyby_cam_z < 0)
        flyby_cam_z = 0x8000;
    if (flyby_cam_z > 0x8000)
        flyby_cam_z = 0;

    engn_cam_tilt = flyby_cam_tilt;
    engn_xc = flyby_cam_x;
    engn_yc = flyby_cam_y;
    engn_zc = flyby_cam_z;
    engn_cam_yaw = 32 * flyby_cam_yaw;

#if 0
    outbuf = vec_tmap[5];
#else
    outbuf = vec_tmap[4] + 256 * (5 * 32) + 2 * 32;
#endif
    setup_vecs(outbuf, vec_tmap[0], 256, 96, 64);
    prepare_drawlist();

    clear_vecs_screen(0);
    drawturn -= 10;
    engine_draw_whole_screen_flyby();
    drawturn += 10;

    setup_vecs(lbDisplay.WScreen, vec_tmap[0],
      lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenWidth,
      lbDisplay.PhysicalScreenHeight);
    transform_reinit_vec_window();

    camera_load_backup_state(&cam_bkp);

    ingame.Flags = bkp_ingame_flags;

    prepare_drawlist();
}

ubyte load_game_slot(ubyte click)
{
    char *ldname;
    int ldslot;
    int ret;

    if (login_control[0].State != LognCt_Unkn6) {
        return 0;
    }
    if (save_slot == -1) {
        alert_box_text_fmt("%s", gui_strings[564]);
        return 1;
    }
    if (save_slot != 0) {
        ldname = save_slot_names[save_slot - save_slot_base - 1];
        ldslot = save_slot;
    } else {
        ldname = save_active_desc;
        ldslot = 0;
    }

    ret = load_game(ldslot, ldname);
    if (ret == 1) {
        alert_box_text_fmt("%s", gui_strings[565]);
        return 1;
    } else if (ret == 2) {
        alert_box_text_fmt("%s", gui_strings[567]);
        return 1;
    }

    alert_box_text_fmt("%s", gui_strings[572]);

    // Reading the save might have caused campaign switch
    load_objectives_text();
    load_wep_mod_desc_text();

    mark_system_menu_screen_boxes_redraw();
    mark_sys_scr_shared_header_box_redraw();
    mark_storage_screen_boxes_redraw();
    if (save_slot == 0) {
        ingame.Flags |= GamF_MortalGame;
    }
    reset_world_screen_player_state();
    selected_agent = 0;
    screentype = SCRT_99;
    game_system_screen = SySc_NONE;
    reload_background_flag = 1;
    if (restore_savegame) {
        restore_savegame = 0;
        sysmnu_button_enable(0, 5);
    }
    if ((ingame.Flags & GamF_MortalGame) != 0) {
        sysmnu_button_disable(1,2);
    }
    return 1;
}

ubyte save_game_slot(ubyte click)
{
    char *slot_str;
    int ret;

    if (login_control[0].State != LognCt_Unkn6) {
        return 0;
    }
    if (save_slot == -1) {
        alert_box_text_fmt("%s", gui_strings[564]);
        return 1;
    }
    slot_str = save_slot_names[save_slot - save_slot_base - 1];
    ret = save_game_write(save_slot, slot_str);
    if (ret) {
        alert_box_text_fmt("%s", gui_strings[566]);
    } else {
        alert_box_text_fmt("%s", gui_strings[571]);
    }
    return 1;
}

void reinit_unkn6_always_reset_variables(void)
{
    login_control[0].TechLevel = 4;
    login_control[0].Money = starting_cash_amounts[login_control[0].TechLevel];
    if (login_control[0].State == LognCt_Unkn6)
    {
        ingame.Credits = 50000;
        ingame.CashAtStart = 50000;
    }
    else
    {
        ingame.Credits = login_control[0].Money;
        ingame.CashAtStart = login_control[0].Money;
    }
    ingame.Expenditure = 0;
    login_control[0].State = LognCt_Unkn6;
    net_game_play_flags = NGPF_Unkn20 | NGPF_Unkn10 | NGPF_Unkn08 | NGPF_Unkn04;
}

void reinit_unkn6_adjustable_variables(void)
{
    login_control[0].City = -1;

    reinit_unkn6_always_reset_variables();
}

void init_unkn6_adjustable_variables(void)
{
    ingame.MissionStatus = ObvStatu_COMPLETED;
    login_control[0].City = 19; // Tokyo
    login_control[0].Team = 0;

    reinit_unkn6_always_reset_variables();
}

void init_agents(void)
{
    PlayerIdx plyr;
    ushort cryo_no;

    cryo_agents.NumAgents = 8;
    cryo_agents_assign_random_names_and_sex();
    cryo_agents_clear_wep_mod();

    // TODO starting equipment could be a part of campaign file
    for (cryo_no = 0; cryo_no < cryo_agents.NumAgents; cryo_no++)
    {
        player_cryo_add_weapon_one(cryo_no, WEP_UZI);
    }

    // Initialize all players from the same starting cryo
    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        PlayerInfo *p_player;

        p_player = &players[plyr];
        player_update_agents_from_cryo(p_player);
    }
}

void play_post_mission_fmv(ubyte misend)
{
    switch (misend)
    {
    case OMiSta_EndSuccess:
        play_smacker(MPly_MissiComplete);
        break;
    case OMiSta_ContImmSuccess:
        // Reload the palette, potentially damaged by fading
        show_black_screen();
        LbFileLoadAt("qdata/pal.pal", display_palette);
        // The setup_screen_mode() within play_smacker() will
        // automatically switch current pal to display_palette
        play_smacker_then_back_to_engine(MPly_MPartComplete);
        break;
    case OMiSta_EndFailed:
        play_smacker(MPly_MissiFail);
        break;
    case OMiSta_ContSuccess:
        break;
    case OMiSta_ContFailed:
        break;
    case OMiSta_CampaignDone:
        init_outro();
        overall_scale = get_overall_scale_max();
        break;
    }
}

void mission_over(void)
{
    ubyte misend;

    // the rules first, as the original did, then the front end
    game_core_mission_over_start();
    reload_menu_flags |= RelMnuF_ColorsSprites;

    LbMouseChangeSprite(0);
    StopCD();
    StopAllSamples();
    SetMusicVolume(100, 0);

    misend = game_core_mission_over_outcome();

    play_post_mission_fmv(misend);

    if (misend == OMiSta_ContImmSuccess) {
        compound_mission_immediate_start_next();
    }

    game_core_mission_over_end();
    if (ingame.GameOver)
        play_smacker(MPly_GameOver);
}

TbBool player_try_spend_money(s32 cost)
{
    if (ingame.Credits - cost < 0)
        return false;
    ingame.Credits -= cost;
    ingame.Expenditure += cost;
    return true;
}

void campaign_new_game_prepare(void)
{
    struct Campaign *p_campgn;

    p_campgn = &campaigns[background_type];

    screentype = SCRT_99;
    game_system_screen = SySc_NONE;

    load_missions(background_type);
    clear_mission_state_slots();
    load_objectives_text();

    load_city_data(0);
    load_wep_mod_desc_text();
    load_city_prop_text();
    reset_frontend_player_state();

    player_mission_agents_toggle_reset(local_player_no);
    global_date_new_game_reset();
    init_unkn6_adjustable_variables();
    srm_reset_research();
    init_agents();

    {
        open_new_mission(p_campgn->FirstTrigger);
        update_brief_screen_netscan_button(p_campgn->NetscanTextId);
    }

    reload_background_flag = 1;
    edit_flag = 0;
}

ubyte goto_savegame(ubyte click)
{
    restore_savegame = 1;
    game_system_screen = SySc_STORAGE;
    screentype = SCRT_SYSMENU;
    sysmnu_button_disable(0, 5);
    update_sys_scr_shared_header(game_system_screen);
    ingame.Flags &= ~GamF_MortalGame;

    load_city_data(0);
    load_wep_mod_desc_text();
    load_city_prop_text();
    reset_frontend_player_state();

    player_mission_agents_toggle_reset(local_player_no);
    global_date_new_game_reset();
    init_unkn6_adjustable_variables();
    srm_reset_research();
    init_agents();

    edit_flag = 0;
    save_slot_base = 0;
    redraw_screen_flag = 1;

    load_save_slot_names();

    return 1;
}

void net_new_game_prepare(void)
{
    switch_net_screen_boxes_to_initiate();

    load_missions(background_type);
    load_objectives_text();

    selected_net_user = -1;
    selected_net_session = -1;
    reset_world_screen_player_state();

    reinit_unkn6_adjustable_variables();
    srm_reset_research();
    init_agents();

    net_player_chat_init();
    net_grpaint_clear_op();
}

ubyte do_storage_NEW_MORTAL(ubyte click)
{
    if (login_control[0].State != LognCt_Unkn6)
        return 0;

    if (strlen(login_name) == 0)
        strcpy(login_name, "ANON");
    read_user_settings();

    ingame.Flags |= GamF_MortalGame;

    campaign_new_game_prepare();

    if (autosave_game()) {
        alert_box_text_fmt("%s", gui_strings[566]);
    }

    if (restore_savegame)  {
        restore_savegame = 0;
        sysmnu_button_enable(0, 5);
    }

    if ((ingame.Flags & GamF_MortalGame) != 0) {
        sysmnu_button_disable(1,2);
    }

    if (new_mail)
      play_sample_using_heap(0, 119 + (LbRandomAnyShort() % 3), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);

    return 1;
}

void init_screen_boxes(void)
{
    init_global_boxes();
    init_alert_screen_boxes();
    init_main_screen_boxes();
    init_system_menu_boxes();

    init_sys_scr_shared_boxes();
    init_controls_screen_boxes();
    init_options_audio_screen_boxes();
    init_options_gfx_screen_boxes();
    init_net_screen_boxes();
    init_storage_screen_boxes();

    init_login_screen_boxes();

    init_brief_screen_boxes();
    init_world_screen_boxes();
    init_debrief_screen_boxes();
    init_equip_screen_boxes();
    init_cryo_screen_boxes();
    init_research_screen_boxes();
    init_equip_screen_shapes();
}

void scroll_map_input(int *p_dx, int *p_dy)
{
    PlayerInfo *p_locplayer;
    int mv_border;
    int dx, dy;

    dx = 0;
    dy = 0;
    p_locplayer = &players[local_player_no];
    // Define a move border, getting the mouse beyond it causes map scroll
    if (lbDisplay.GraphicsScreenWidth >= 640)
        mv_border = 3;
    else
        mv_border = 2;

    // Only allow scrolling via keyboard arrows if not DoubleMode, and if the arrows
    // are not used for something else currntly (like changing weapon/mood)
    if ((!p_locplayer->DoubleMode) && ((p_locplayer->UserInput[0].Bits & SpUIn_ChangeMoodOrWep) == 0))
    {
        dx = (is_gamekey_kbd_pressed(GKey_RIGHT) & 1) - (is_gamekey_kbd_pressed(GKey_LEFT) & 1);
        dy = (is_gamekey_kbd_pressed(GKey_DOWN) & 1) - (is_gamekey_kbd_pressed(GKey_UP) & 1);
    }

    {
        if (dx == 0)
        {
            int mx;
            mx = lbDisplay.MMouseX;
            if (mx >= lbDisplay.GraphicsScreenWidth - mv_border)
                dx = 1;
            if (mx < mv_border)
                dx = -1;
        }
        if (dy == 0)
        {
            int my;
            my = lbDisplay.MMouseY;
            if (my >= lbDisplay.GraphicsScreenHeight - mv_border)
                dy = 1;
            if (my < mv_border)
                dy = -1;
        }
    }
    *p_dx = dx;
    *p_dy = dy;
}

void do_scroll_map(void)
{
    int dx, dy;

    // Reading the input has no effect on the game; the core moves the camera
    dx = 0;
    dy = 0;
    if (game_camera_scroll_allowed())
        scroll_map_input(&dx, &dy);
    game_camera_update(dx, dy);
}

ubyte weapon_select_input(void)
{
    PlayerInfo *p_locplayer;
    struct Packet *p_pckt;
    ThingIdx dcthing;
    WeaponType wtype;
    int n;

    static const GameKey sel_weapon_gkeys[] = {
        GKey_SEL_WEP_1, GKey_SEL_WEP_2, GKey_SEL_WEP_3, GKey_SEL_WEP_4, GKey_SEL_WEP_5, GKey_SEL_WEP_6,
    };

    static GameTurn last_sel_weapon_turn[WEAPONS_CARRIED_MAX_COUNT] = {0};

    p_locplayer = &players[local_player_no];
    p_pckt = &packets[local_player_no];
    dcthing = p_locplayer->DirectControl[0];
    wtype = WEP_NULL;

    if (is_gamekey_pressed(GKey_USE_MEDIKIT))
    {
        clear_gamekey_pressed(GKey_USE_MEDIKIT);
        if (person_can_use_medikit(dcthing))
        {
            my_build_packet(p_pckt, PAct_AGENT_USE_MEDIKIT, dcthing, 0, 0, 0);
            return GINPUT_PACKET;
        }
    }

    if (is_gamekey_pressed(GKey_SUPERSHIELD))
    {
        clear_gamekey_pressed(GKey_SUPERSHIELD);
        if (person_can_toggle_supershield(dcthing))
        {
            my_build_packet(p_pckt, PAct_SHIELD_TOGGLE, dcthing, 0, 0, 0);
            return GINPUT_PACKET;
        }
    }

    if (is_gamekey_pressed(GKey_VIEW_THERMAL))
    {
        clear_gamekey_pressed(GKey_VIEW_THERMAL);
        if (player_can_toggle_thermal(local_player_no))
        {
            my_build_packet(p_pckt, PAct_THERMAL_TOGGLE, dcthing, 0, 0, 0);
            return GINPUT_PACKET;
        }
    }

    assert(sizeof(sel_weapon_gkeys)/sizeof(sel_weapon_gkeys[0]) <= WEAPONS_CARRIED_MAX_COUNT);

    for (n = 0; n < (int)(sizeof(sel_weapon_gkeys)/sizeof(sel_weapon_gkeys[0])); n++)
    {
        GameKey gkey = sel_weapon_gkeys[n];
        if (is_gamekey_pressed(gkey))
        {
            clear_gamekey_pressed(gkey);
            wtype = find_nth_weapon_held(dcthing, n+1);
            if (wtype != WEP_NULL)
                break;
        }
    }
    if (wtype != WEP_NULL)
    {
        // Double tapping - select for all agents
        if (gameturn - last_sel_weapon_turn[n] < game_num_fps / 2)
        {
            ubyte flag;

            flag = (person_get_selected_weapon(dcthing) == wtype) ? WepSel_SELECT : WepSel_HIDE;
            my_build_packet(p_pckt, PAct_SELECT_GRP_SPEC_WEAPON, dcthing, wtype, flag, 0);
            last_sel_weapon_turn[n] -= game_num_fps / 2;
        }
        else
        {
            my_build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, dcthing, wtype, WepSel_TOGGLE, 0);
            last_sel_weapon_turn[n] = gameturn;
        }

        return GINPUT_PACKET;
    }

    return GINPUT_NONE;
}

void do_rotate_map(void)
{

    short rotate_input = 0;
    if (is_gamekey_pressed(GKey_VIEW_SPIN_R))
        rotate_input++;
    if (is_gamekey_pressed(GKey_VIEW_SPIN_L))
        rotate_input--;

    if (rotate_input == 0) {
#ifdef MORE_GAME_KEYS
        // these keys are used for rotation only here, but since
        // at the same time they are also used for moving the
        // viewport (GKey_LEFT/GKey_LEFT), the result is panning
        if (is_gamekey_pressed(GKey_VIEW_PAN_R))
            rotate_input++;
        if (is_gamekey_pressed(GKey_VIEW_PAN_L))
            rotate_input--;
#else
        if (is_key_pressed(kbkeys[GKey_RIGHT], KMod_SHIFT))
            rotate_input++;
        if (is_key_pressed(kbkeys[GKey_LEFT], KMod_SHIFT))
            rotate_input--;
#endif
    }

    short zoom_input = 0;
    if (is_gamekey_pressed(GKey_ZOOM_IN))
        zoom_input++;
    if (is_gamekey_pressed(GKey_ZOOM_OUT))
        zoom_input--;

    short tilt_input = 0;
    if (is_gamekey_pressed(GKey_VIEW_TILT_U))
        tilt_input++;
    if (is_gamekey_pressed(GKey_VIEW_TILT_D))
        tilt_input--;

    game_camera_input(rotate_input, zoom_input, tilt_input);
}

ubyte process_mouse_inputs(void)
{
    PlayerInfo *p_locplayer;
    struct Thing *p_agent;
    struct Packet *p_pckt;
    ThingIdx dcthing;
    int map_y;
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    p_locplayer = &players[local_player_no];

    if (!lbDisplay.MLeftButton)
        user_input_control_flags_clear(local_player_no, mouser, UInpCtrF_LBtnDown);
    if (!lbDisplay.MRightButton)
        user_input_control_flags_clear(local_player_no, mouser, UInpCtrF_RBtnDown);
    if ((ingame.DisplayMode != DpM_ENGINEPLY) && (ingame.DisplayMode != DpM_UNKN_3B))
        return did_inp;
    did_inp |= process_panel_state();
    if ((did_inp & GINPUT_PACKET) != 0)
        return did_inp;
    if (lbDisplay.LeftButton || lbDisplay.RightButton || lbDisplay.MLeftButton || lbDisplay.MRightButton)
    {
        did_inp = check_panel_button();
        if ((did_inp & GINPUT_PACKET) != 0)
            return did_inp;
    }

    dcthing = p_locplayer->DirectControl[mouser];
    p_agent = &things[dcthing];
    if ((p_agent->State == PerSt_PERSON_BURNING) || ((p_agent->Flag & TngF_Destroyed) != 0)) {
        return did_inp;
    }

    if (lbDisplay.LeftButton)
    {
        lbDisplay.LeftButton = 0;

        p_pckt = &packets[local_player_no];
        if (p_locplayer->field_102 != 0)
        {
            switch (p_locplayer->TargetType)
            {
            case 1:
            case 7:
                my_build_packet(p_pckt, PAct_FOLLOW_PERSON, dcthing,
                  p_locplayer->field_102, 0, 0);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            case 2:
            case 6:
                break;
            case 3:
                if (p_locplayer->Target == 0)
                    break;
                if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
                {
                    p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                    my_build_packet(p_pckt, PAct_AGENT_GOTO_FACE_PT_ABS, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                }
                else
                {
                    p_locplayer->UserInput[mouser].Turn = 0;
                    my_build_packet(p_pckt, PAct_AGENT_GOTO_FACE_PT_ABS_FF, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                }
                did_inp |= GINPUT_PACKET;
                return did_inp;
            case 4:
                if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
                {
                    p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                    my_build_packet(p_pckt, PAct_GO_ENTER_VEHICLE, dcthing,
                      p_locplayer->field_102, 0, 0);
                }
                else
                {
                    p_locplayer->UserInput[mouser].Turn = 0;
                    my_build_packet(p_pckt, PAct_GO_ENTER_VEHICLE_FF, dcthing,
                      p_locplayer->field_102, 0, 0);
                }
                did_inp |= GINPUT_PACKET;
                return did_inp;
            case 5:
                if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
                {
                    p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                    my_build_packet(p_pckt, PAct_GET_ITEM, dcthing,
                      p_locplayer->field_102, 0, 0);
                }
                else
                {
                    p_locplayer->UserInput[mouser].Turn = 0;
                    my_build_packet(p_pckt, PAct_GET_ITEM_FAST, dcthing,
                      p_locplayer->field_102, 0, 0);
                }
                did_inp |= GINPUT_PACKET;
                return did_inp;
            default:
                break;
            }
        }

        if (!p_locplayer->DoubleMode)
        {
            ushort ctlmode;
            ctlmode = user_input_control_mode_get(local_player_no, 0);
            if (ctlmode != UInpCtr_Mouse)
            {
                do_change_mouse(8);
                build_packet(p_pckt, PAct_CONTROL_MODE, 1, 0, 0, 0);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
        }
        if ((mouse_map_x > 0) && (mouse_map_x < MAP_COORD_WIDTH) &&
          (mouse_map_z > 0) && (mouse_map_z < MAP_COORD_HEIGHT))
        {
            short map_y;

            map_y = (alt_at_point(mouse_map_x, mouse_map_z) >> 8) + 20;
            if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
            {
                p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                my_build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS, dcthing,
                  mouse_map_x, map_y, mouse_map_z);
            }
            else
            {
                p_locplayer->UserInput[mouser].Turn = 0;
                my_build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS_FF, dcthing,
                  mouse_map_x, map_y, mouse_map_z);
            }
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
        return did_inp;
    }

    if (lbDisplay.RightButton && !user_input_control_flags_check(local_player_no, mouser, UInpCtrF_RBtnDown))
    {
        WeaponType wtype;
        lbDisplay.RightButton = 0;

        p_pckt = &packets[local_player_no];
        map_y = (alt_at_point(mouse_map_x, mouse_map_z) >> 8) + 20;
        wtype = p_agent->U.UPerson.CurrentWeapon;
        if (weapon_is_for_planting(wtype))
        {
            if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
            {
                p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                if (p_locplayer->TargetType == 3)
                    my_build_packet(p_pckt, PAct_PLANT_MINE_AT_FACE_PT, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                else
                    my_build_packet(p_pckt, PAct_PLANT_MINE_AT_GND_PT, dcthing,
                      mouse_map_x, map_y, mouse_map_z);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
            else
            {
                p_locplayer->UserInput[mouser].Turn = 0;
                if (p_locplayer->TargetType == 3)
                    my_build_packet( p_pckt, PAct_PLANT_MINE_AT_FACE_PT_FF, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                else
                    my_build_packet(p_pckt, PAct_PLANT_MINE_AT_GND_PT_FF, dcthing,
                      mouse_map_x, map_y, mouse_map_z);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
        }
        else if (weapon_is_for_spreading_on_ground(wtype))
        {
            if ((gameturn & 0x7FFF) - p_locplayer->UserInput[mouser].Turn >= 7)
            {
                p_locplayer->UserInput[mouser].Turn = (gameturn & 0x7FFF);
                if (p_locplayer->TargetType == 3)
                    my_build_packet(p_pckt, PAct_SHOOT_AT_FACE_POINT, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                else
                    my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT, dcthing,
                      mouse_map_x, map_y, mouse_map_z);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
            else
            {
                p_locplayer->UserInput[mouser].Turn = 0;
                if (p_locplayer->TargetType == 3)
                    my_build_packet(p_pckt, PAct_SHOOT_AT_FACE_POINT_FF, dcthing,
                      mouse_map_x, p_locplayer->Target, mouse_map_z);
                else
                    my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT_FF, dcthing,
                      mouse_map_x, map_y, mouse_map_z);
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
        }
    }

    if (lbDisplay.MRightButton && !user_input_control_flags_check(local_player_no, mouser, UInpCtrF_RBtnDown))
    {
        p_pckt = &packets[local_player_no];
        map_y = (alt_at_point(mouse_map_x, mouse_map_z) >> 8) + 20;
        if (p_locplayer->TargetType == 3)
        {
            my_build_packet(p_pckt, PAct_SHOOT_AT_FACE_POINT, dcthing,
              mouse_map_x, p_locplayer->Target, mouse_map_z);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
        if (p_locplayer->field_102 > 0)
        {
            my_build_packet(p_pckt, PAct_SHOOT_AT_THING, dcthing,
              p_locplayer->field_102, 0, 0);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
        if ((mouse_map_x > 0) && (mouse_map_x < MAP_COORD_WIDTH) &&
          (mouse_map_z > 0) && (mouse_map_z < MAP_COORD_HEIGHT))
        {
            my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT, dcthing,
              mouse_map_x, map_y, mouse_map_z);
            did_inp |= GINPUT_PACKET;
            return did_inp;
        }
    }

    return did_inp;
}

ubyte do_music_user_input(void)
{
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    // MIDI Music (tension) volume control
    if (is_gamekey_pressed(GKey_DANGR_VOL_INC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_midivol += 1;
        else
            startscr_midivol += 10;
        sfx_apply_midivol();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_DANGR_VOL_DEC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_midivol -= 1;
        else
            startscr_midivol -= 10;
        sfx_apply_midivol();
        did_inp |= GINPUT_DIRECT;
    }

    // Sample volume control
    if (is_gamekey_pressed(GKey_SOUND_VOL_INC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_samplevol += 1;
        else
            startscr_samplevol += 10;
        sfx_apply_samplevol();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_SOUND_VOL_DEC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_samplevol -= 1;
        else
            startscr_samplevol -= 10;
        sfx_apply_samplevol();
        did_inp |= GINPUT_DIRECT;
    }

    // CD Music volume control
    if (is_gamekey_pressed(GKey_MUSIC_VOL_INC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_cdvolume += 1;
        else
            startscr_cdvolume += 10;
        sfx_apply_cdvolume();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_MUSIC_VOL_DEC))
    {
        if (lbShift & KMod_SHIFT)
            startscr_cdvolume -= 1;
        else
            startscr_cdvolume -= 10;
        sfx_apply_cdvolume();
        did_inp |= GINPUT_DIRECT;
    }

    // Music track control
    if (is_gamekey_pressed(GKey_MUSIC_TRACK))
    {
        clear_gamekey_pressed(GKey_MUSIC_TRACK);
        game_option_inc(GOpt_CDATrack);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_DANGR_TRACK))
    {
        clear_gamekey_pressed(GKey_DANGR_TRACK);
        game_option_inc(GOpt_DangerTrack);
        StartMusic(ingame.DangerTrack, 0);
        did_inp |= GINPUT_DIRECT;
    }

    return did_inp;
}

ubyte do_user_interface(void)
{
    PlayerInfo *p_locplayer;
    int n;
    ubyte did_inp;

    did_inp = GINPUT_NONE;
    p_locplayer = &players[local_player_no];
    update_danger_music(0);
    do_scroll_map();
    do_rotate_map();
    if (in_network_game)
    {
        if (is_key_pressed(KC_RETURN, KMod_DONTCARE))
        {
            clear_key_pressed(KC_RETURN);
            if ((p_locplayer->PanelState[mouser] != PANEL_STATE_SEND_MESSAGE)
              && player_message_add_allowed(local_player_no))
            {
                p_locplayer->PanelState[mouser] = PANEL_STATE_SEND_MESSAGE;
                reset_buffered_keys();
                player_message_clear(local_player_no);
                panel_objective_info_start();
                did_inp |= GINPUT_DIRECT;
            }
        }
    }
    if (p_locplayer->PanelState[mouser] == PANEL_STATE_SEND_MESSAGE)
    {
        did_inp |= process_mouse_inputs();
        // do not allow other keyboard inputs when writing chat message
        return did_inp;
    }

    // screenshot
    if (is_gamekey_pressed(GKey_SCREENSHOT))
    {
        clear_gamekey_pressed(GKey_SCREENSHOT);
        LbPngSaveScreen("synII", lbDisplay.WScreen, display_palette, 0);
        did_inp |= GINPUT_DIRECT;
    }

#if 0
    // This looks like some kind of early idea for grouping agents like in C&C
    // clearly unfinished; remove pending
    for (n = 0; n < 5; n++)
    {
        if (is_key_pressed(KC_F1+n, KMod_ALT))
        {
            clear_key_pressed(KC_F1+n);
            my_build_packet(&packets[local_player_no], PAct_AGENT_UNKGROUP_ADD, n, 0, 0, 0);
            return 1;
        }
        if (is_key_pressed(KC_F1+n, KMod_SHIFT))
        {
            clear_key_pressed(KC_F1+n);
            my_build_packet(&packets[local_player_no], PAct_AGENT_UNKGROUP_PROT, n, 0, 0, 0);
            return 1;
        }
    }
#endif

    // change panel style
    if (is_key_pressed(KC_F9, KMod_NONE))
    {
        clear_key_pressed(KC_F9);
        game_option_inc(GOpt_PanelPermutation);
        load_pop_sprites_for_current_mode();
        init_scanner_colour();
        did_inp |= GINPUT_DIRECT;
    }

    // Switch old/new panel
    if (is_key_pressed(KC_F9, KMod_CONTROL))
    {
        clear_key_pressed(KC_F9);
        //game_option_inc(GOpt_PanelType);
        if (ingame.PanelPermutation < 0)
            game_option_set(GOpt_PanelPermutation, -ingame.PanelPermutation-1);
        else
            game_option_set(GOpt_PanelPermutation, -ingame.PanelPermutation-1);
        load_pop_sprites_for_current_mode();
        init_scanner_colour();
        did_inp |= GINPUT_DIRECT;
    }

    // change agents colours
    if (is_key_pressed(KC_F10, KMod_NONE))
    {
        clear_key_pressed(KC_F10);
        game_option_inc(GOpt_TrenchcoatPreference);
        prep_multicolor_sprites();
        did_inp |= GINPUT_DIRECT;
    }

    // adjust palette brightness
    if (is_key_pressed(KC_F11, KMod_CONTROL))
    {
        clear_key_pressed(KC_F11);
        reset_user_selected_brightness();
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F11, KMod_SHIFT))
    {
        clear_key_pressed(KC_F11);
        change_brightness(-1);
        user_sel_brightness--;
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F11, KMod_NONE))
    {
        clear_key_pressed(KC_F11);
        change_brightness(1);
        user_sel_brightness++;
        did_inp |= GINPUT_DIRECT;
    }

    if (is_key_pressed(KC_F1, KMod_CONTROL))
    {
        clear_key_pressed(KC_F1);
        game_option_inc(GOpt_BillboardMovies);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F2, KMod_CONTROL))
    {
        clear_key_pressed(KC_F2);
        if (ingame.Flags & GamF_RenderScene)
            ingame.Flags &= ~GamF_RenderScene;
        else
            ingame.Flags |= GamF_RenderScene;
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F3, KMod_CONTROL))
    {
        clear_key_pressed(KC_F3);
        if (ingame.Flags & TngF_ProgressAction)
            ingame.Flags &= ~TngF_ProgressAction;
        else
            ingame.Flags |= TngF_ProgressAction;
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F4, KMod_CONTROL))
    {
        clear_key_pressed(KC_F4);
        game_option_inc(GOpt_AdvancedLights);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F6, KMod_CONTROL))
    {
        clear_key_pressed(KC_F6);
        game_option_inc(GOpt_DeepRadar);
        did_inp |= GINPUT_DIRECT;
    }
    if (is_key_pressed(KC_F10, KMod_CONTROL))
    {
        clear_key_pressed(KC_F10);
        if (ingame.Flags & GamF_HUDPanel)
            ingame.Flags &= ~GamF_HUDPanel;
        else
            ingame.Flags |= GamF_HUDPanel;
        did_inp |= GINPUT_DIRECT;
    }

    // Game Speed control
    if (!in_network_game)
    {
        did_inp |= get_speed_control_inputs();
    }

    // Toggle Scanner beep
    if (is_gamekey_pressed(GKey_SCANNER_PULSE))
    {
        clear_gamekey_pressed(GKey_SCANNER_PULSE);
        game_option_inc(GOpt_ScannerPulse);
        did_inp |= GINPUT_DIRECT;
    }

    // Control map area to draw
    if ((ingame.UserFlags & UsrF_Cheats) != 0)
    {
        if (is_key_pressed(KC_E, KMod_ALT) || is_key_pressed(KC_E, KMod_ALT|KMod_SHIFT))
        {
            short dt;

            if (lbShift & KMod_SHIFT)
                dt = -2;
            else
                dt = 2;
            render_area_a = bound_render_area(render_area_a + dt);
            render_area_b = bound_render_area(render_area_b + dt);
            did_inp |= GINPUT_DIRECT;
        }
    }

    // Entering pause screen
    if (!in_network_game)
    {
        if (is_gamekey_pressed(GKey_PAUSE))
        {
            if (pause_screen_handle()) {
                return GINPUT_EXIT;
            }
        }
    }

    // Scanner appearence control
    if (is_gamekey_pressed(GKey_SCANNR_BRIGH_INC)) {
        ingame.Scanner.Brightness += 4;
        did_inp |= GINPUT_DIRECT;
    }
    if (is_gamekey_pressed(GKey_SCANNR_BRIGH_DEC)) {
        ingame.Scanner.Brightness -= 4;
        did_inp |= GINPUT_DIRECT;
    }
    if (ingame.Scanner.Brightness < 0)
        ingame.Scanner.Brightness = 0;
    if (ingame.Scanner.Brightness > 64)
        ingame.Scanner.Brightness = 64;

    do_music_user_input();
    // Restart level
    if (!in_network_game && (ingame.Flags & GamF_MortalGame) == 0)
    {
        if (is_gamekey_pressed(GKey_MISSN_RESTART))
        {
            clear_gamekey_pressed(GKey_MISSN_RESTART);
            StopCD();
            test_missions(1);
            init_level_3d(1);

            restart_back_into_mission(ingame.CurrentMission);
            did_inp |= GINPUT_DIRECT;
        }
    }

    if (ingame.TrackThing)
    {
        short dcthing;
        dcthing = p_locplayer->DirectControl[0];
        build_packet(&packets[local_player_no], 0, dcthing, 0, 0, 0);
        did_inp |= GINPUT_PACKET;
        return did_inp;
    }

    static GameKey sel_agent_gkeys[] = {
        GKey_SEL_AGENT_1, GKey_SEL_AGENT_2, GKey_SEL_AGENT_3, GKey_SEL_AGENT_4
    };
    static GameTurn last_sel_agent_turn[AGENTS_SQUAD_MAX_COUNT] = {0};
    assert(sizeof(sel_agent_gkeys)/sizeof(sel_agent_gkeys[0]) <= AGENTS_SQUAD_MAX_COUNT);
    for (n = 0; n < (int)(sizeof(sel_agent_gkeys)/sizeof(sel_agent_gkeys[0])); n++)
    {
        GameKey gkey = sel_agent_gkeys[n];
        if (is_gamekey_pressed(gkey))
        {
            struct Thing *p_agent;

            p_agent = p_locplayer->MyAgent[n];
            if (p_agent->Type != TT_PERSON) continue;

            if (person_can_accept_control(p_agent->ThingOffset) && ((p_agent->Flag2 & TgF2_KnockedOut) == 0))
            {
                clear_gamekey_pressed(gkey);
                if (p_locplayer->DoubleMode)
                {
                    byte_153198 = n+1;
                }
                else
                {
                    short dcthing;
                    dcthing = p_locplayer->DirectControl[n];
                    my_build_packet(&packets[local_player_no], PAct_SELECT_AGENT, dcthing, p_agent->ThingOffset, 0, 0);
                    user_input_control_flags_raise(local_player_no, 0, UInpCtrF_LBtnDown);
                    // Double tapping - center view on the agent
                    if (gameturn - last_sel_agent_turn[n] < 7)
                    {
                      struct Packet *p_pckt;

                      p_pckt = &packets[local_player_no];

                      game_set_cam_track_thing_xz(p_agent->ThingOffset);
                      engn_yc = PRCCOORD_TO_MAPCOORD(p_agent->Y);
                      dcthing = p_locplayer->DirectControl[mouser];
                      build_packet(p_pckt, PAct_SELECT_AGENT, dcthing, p_agent->ThingOffset, 0, 0);
                      if (p_agent->ThingOffset == (short)p_locplayer->DirectControl[mouser])
                      {
                          engn_xc = PRCCOORD_TO_MAPCOORD(p_agent->X);
                          engn_zc = PRCCOORD_TO_MAPCOORD(p_agent->Z);
                      }
                    }
                    last_sel_agent_turn[n] = gameturn;
                }
                did_inp |= GINPUT_PACKET;
                return did_inp;
            }
        }
    }

    if ((ingame.Flags & GamF_Unkn0100) != 0)
    {
        short dcthing;
        dcthing = p_locplayer->DirectControl[0];
        build_packet(&packets[local_player_no], 0, dcthing, 0, 0, 0);
        did_inp |= GINPUT_PACKET;
        return did_inp;
    }

    struct SpecialUserInput *p_usrinp;
    ushort ctlmode;
    ubyte dmuser;

    if (p_locplayer->DoubleMode)
    {
        for (dmuser = 0; dmuser < p_locplayer->DoubleMode + 1; dmuser++)
        {
            short dcthing;

            p_usrinp = &p_locplayer->UserInput[dmuser];
            do_user_input_bits_control_clear_nonmove(p_usrinp);
            ctlmode = user_input_control_mode_get(local_player_no, dmuser);
            if (ctlmode == UInpCtr_Mouse)
            {
                do_user_input_bits_control_clear_all(p_usrinp);
                process_mouse_inputs();
            }
            else if (ctlmode <= UInpCtr_Keyboard)
            {
                dcthing = p_locplayer->DirectControl[dmuser];
                if (person_can_accept_control(dcthing))
                {
                    did_inp = weapon_select_input();
                    if ((did_inp & GINPUT_PACKET) == 0)
                    {
                        do_user_input_bits_direction_clear(p_usrinp);
                        do_user_input_bits_direction_from_kbd(p_usrinp);
                        do_user_input_bits_actions_from_kbd(p_usrinp);
                    }
                }
            }
            else
            {
                dcthing = p_locplayer->DirectControl[dmuser];
                if (!person_can_accept_control(dcthing))
                    return did_inp;

                do_user_input_bits_direction_clear(p_usrinp);
                do_user_input_bits_direction_from_joy(p_usrinp, ctlmode - 2);
                do_user_input_bits_actions_from_joy(p_usrinp, ctlmode - 2);
            }
            ctlmode = user_input_control_mode_get(local_player_no, dmuser);
            if (ctlmode != UInpCtr_Mouse)
            {
                update_agent_move_direction_deltas(p_usrinp);
            }

            if ((debug_log_things & 0x02) != 0)
            {
                LOGSYNC_F("User %d.%d ControlMode 0x%04X Bits 0x%04X Turn %u Dt(%d,%d) OnFace %d",
                  (int)local_player_no, (int)dmuser, (uint)p_usrinp->ControlMode, (uint)p_usrinp->Bits,
                  (uint)p_usrinp->Turn, (int)p_usrinp->DtX, (int)p_usrinp->DtZ,
                  (int)p_usrinp->OnFace);
            }
        }
    }
    else
    {
        short dcthing;

        p_usrinp = &p_locplayer->UserInput[0];
        ctlmode = user_input_control_mode_get(local_player_no, 0);
        if ((ctlmode == UInpCtr_Mouse) && is_gamekey_pressed(GKey_KEY_CONTROL))
        {
            clear_gamekey_pressed(GKey_KEY_CONTROL);
            p_usrinp->ControlMode &= UInpCtr_AllFlagsMask;
            p_usrinp->ControlMode |= UInpCtr_Keyboard;
            do_change_mouse(8);
            p_locplayer->State[0] = 0;
            did_inp |= GINPUT_DIRECT;
        }
        do_user_input_bits_control_clear_all(p_usrinp);
        do_user_input_bits_direction_clear(p_usrinp);

        did_inp |= process_mouse_inputs();
        if ((did_inp & GINPUT_PACKET) != 0)
            return did_inp;

        did_inp |= weapon_select_input();
        if ((did_inp & GINPUT_PACKET) != 0)
            return did_inp;

        dcthing = p_locplayer->DirectControl[0];
        if (person_can_accept_control(dcthing))
        {
            do_user_input_bits_actions_from_joy_and_kbd(p_usrinp);

            ctlmode = user_input_control_mode_get(local_player_no, 0);
            if (ctlmode != UInpCtr_Mouse)
            {
                do_user_input_bits_direction_clear(p_usrinp);
                do_user_input_bits_direction_from_kbd(p_usrinp);
                do_user_input_bits_direction_from_joy(p_usrinp, 0);
                update_agent_move_direction_deltas(p_usrinp);
            }
            else if ((p_usrinp->Bits & SpUIn_ChangeMoodOrWep) != 0)
            {
                // Changing weapon or mood - prepare direction values, but do not treat them as movement
                do_user_input_bits_direction_clear(p_usrinp);
                do_user_input_bits_direction_from_kbd(p_usrinp);
                do_user_input_bits_direction_from_joy(p_usrinp, 0);
            }
        }
        if ((debug_log_things & 0x02) != 0)
        {
            LOGSYNC_F("User %d.%d ControlMode 0x%04X Bits 0x%04X Turn %u Dt(%d,%d) OnFace %d",
              (int)local_player_no, (int)n, (uint)p_usrinp->ControlMode, (uint)p_usrinp->Bits,
              (uint)p_usrinp->Turn, (int)p_usrinp->DtX, (int)p_usrinp->DtZ,
              (int)p_usrinp->OnFace);
        }
    }
    return did_inp;
}

void show_menu_screen_st0(void)
{
    LOGSYNC("Init main menu, %s game", in_network_game ? "MP" : "SP");

    debug_trace_place(16);

    lbInkeyToAscii[KC_OEM_102] = '\\';
    lbInkeyToAsciiShift[KC_OEM_102] = '|';

    players_init_default_control_mode();

    sprintf(net_unkn2_text, "01234567890");

    {
        s32 pos = 0;
        hotspot_buffer = (struct ScreenPoint *)((ubyte *)scratch_malloc_mem + pos);
        pos += hotspot_buffer_len;

        mission_briefing_text = (char *)scratch_malloc_mem + pos;
        pos += mission_briefing_text_len;

        memload_netscan_text = (char *)scratch_malloc_mem + pos;
        pos += memload_netscan_text_len;

        memload_wep_mod_desc_text = (char *)scratch_malloc_mem + pos;
        pos += memload_wep_mod_desc_text_len;

        memload_city_prop_text = (char *)scratch_malloc_mem + pos;
        pos += memload_city_prop_text_len;

        purple_draw_list = (struct PurpleDrawItem *)((ubyte *)scratch_malloc_mem + pos);
    }

    load_city_data(0);
    load_city_prop_text();

    player_mission_agents_toggle_reset(local_player_no);
    global_date_new_game_reset();
    ingame.Credits = 50000;
    login_control[0].State = LognCt_Unkn6;

    debug_trace_place(17);
    // Need to set screen type before gfx background is reloaded
    if (in_network_game)
        screentype = SCRT_LOGIN;
    else
        screentype = SCRT_MAINMENU;
    reload_menu_flags |= RelMnuF_ColorsSprites | RelMnuF_ScrBoxesFull | RelMnuF_BriefScanner;

    debug_trace_place(18);
    save_game_buffer = vec_tmap[5];

    net_system_init0();
}

void update_mission_time(TbBool a1)
{
    if (a1)
    {
        // dword_1C4B84 = gameturn; //TODO no function - remove
        return;
    }
    //dword_1C4B88 = gameturn; //TODO no function - remove

    mission_status_time_rand_progress(open_brief);
    global_date_update_after_mission();
}

void show_menu_screen_st2(void)
{
    LOGSYNC("Init debrief, %s game", in_network_game ? "MP" : "SP");

    if (in_network_game)
    {
        local_player_no = 0;
        net_new_game_prepare();
        net_sessionlist_clear();
        scientists_lost = 0;
        update_mission_time(0);
        in_network_game = 0;

        reset_cryo_screen_player_state();
        reset_equip_screen_player_state();
        screentype = SCRT_NETDEBRF;
        redraw_screen_flag = 1;
        set_heading_box_text(gui_strings[374]);
    }
    else
    {
      update_mission_time(0);
      selected_city_id = -1;
      open_ref = brief_store[open_brief - 1].RefNum;
      // Original code compared MissionStatus to 0 and 2, is 2 a valid value?
      if ((ingame.MissionStatus != ObvStatu_UNDECIDED) && (ingame.MissionStatus != ObvStatu_FAILED))
      {
            LbMemoryCopy(&mission_status[0], &mission_status[open_brief],
              sizeof(struct MissionStatus));
            delete_mail(open_brief - 1, MlTp_Mission);
            open_brief = 0;
            clear_city_netscan(map_hl_city_id);
            reset_brief_screen_player_state();
      }
      else
      {
            old_mission_brief = open_brief;
      }
      if (ingame.GameOver)
      {
            screentype = SCRT_MAINMENU;
            if ((ingame.Flags & GamF_MortalGame) != 0) {
                char fname[DISKPATH_SIZE];
                get_saved_game_fname(fname, 0);
                LbFileDelete(fname);
            }
            ingame.GameOver = 0;
      }
      else
      {
            struct MissionStatus *p_mistat;
            p_mistat = &mission_status[open_brief];
            forward_research_progress_after_mission(p_mistat->CityDays);
            autosave_game();
            screentype = SCRT_DEBRIEF;
            set_heading_box_text(gui_strings[374]);
            redraw_screen_flag = 1;
      }
    }

    load_wep_mod_desc_text();
    load_city_prop_text();

    update_options_screen_state();

    reload_menu_flags |= RelMnuF_ColorsSprites | RelMnuF_BriefScanner;

    if ((new_mail != 0) && (screentype != SCRT_MAINMENU))
        play_sample_using_heap(0, 119 + (LbRandomAnyShort() % 3), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);

    net_system_init2();
}

void update_open_brief(void)
{
    short brief;
    open_brief = 0;
    for (brief = 0; brief < next_brief; brief++)
    {
        ushort missi;

        missi = find_mission_for_city_in_brief(brief, map_hl_city_id);
        if (missi != 0) {
            open_brief = brief + 1;
            break;
        }
    }
}

void show_load_and_prep_mission(void)
{
    init_random_seed();
    show_mission_loading_screen();

    if ( start_into_mission )
    {
        if (!in_network_game) {
            update_open_brief();
        }

        ingame.MissionEndFade = 0;
        engine_render_lights = 1;

        debug_trace_place(10);
        if ( in_network_game )
        {
            ushort missi;
            ingame.MissionNo = 1;
            missi = 0;
            if (login_control[0].City != -1)
                missi = find_first_mission_with_map(cities[login_control[0].City].MapID);
            if (missi > 0) {
                ingame.MissionNo = missi;
            }
            missi = ingame.MissionNo;
            ingame.CurrentMission = missi;
            debug_trace_place(11);
        }
        else
        {
            ushort missi;
            missi = find_mission_for_city_in_brief(open_brief - 1, map_hl_city_id);
            load_mission_name_text(missi);
            ingame.CurrentMission = missi;
            // The names are propagated by fenet only in network game
            net_player_names_clear();
            net_player_name_set(0, login_name);
            debug_trace_place(12);
        }
    }
    else
    {
        LbMouseChangeSprite(&pointer_sprites[1]);
        ingame.DisplayMode = DpM_UNKN_1;
    }

    reload_background_flag = 1;
    // Setup screen, palette and colour tables
    debug_trace_place(13);
    setup_engine_screen_mode();
    // Show something on screen until the load finishes
    show_simple_load_screen();
    // Now we can call the init; it uses current video mode and colour tables
    // to scale correctly, so has to be done after video setup
    // (though we should at some point separate the part linked to current video settings)
    if (start_into_mission)
    {
        if (PacketRecord_IsPlayback())
        {
            packet_read_whole_player_init();
        }
        init_game(0);
        preprogress_game_turns();
    }

    // Update game progress and prepare level to play
    if (start_into_mission)
    {
        clear_open_mission_status();
        update_mission_time(1);
        if (in_network_game)
        {
            gameturn = 0;
        }
        else
        {
            clear_city_netscan(map_hl_city_id);
            mission_result = 0;
        }
        if (!in_network_game)
        {
            if (PacketRecord_IsRecord()) {
                PacketRecord_OpenWrite();
                packet_write_whole_player_init();
            }
        }
        debug_trace_place(19);
    }
    render_clock_set_turn(gameturn);

    // Set up remaining graphics data and controls
    if (start_into_mission)
    {
        prep_multicolor_sprites();
        LbScreenClear(0);
        generate_shadows_for_multicolor_sprites();
        adjust_mission_engine_to_video_mode();

        embanim_reinit(AniSl_BILLBOARD);
        embanim_do_next_frame(AniSl_BILLBOARD);

        if (in_network_game)
        {
            if (!netgame_service_is_multi_client_capable())
                ingame.InNetGame_UNSURE = ((1 << 0) | (1 << 1)); // two players
            ingame.DetailLevel = 0;
            bang_set_detail(ingame.DetailLevel == 0);
        }

        lbDisplay.MLeftButton = 0;
        lbDisplay.LeftButton = 0;
    }

    map_editor = 0;
    start_into_mission = 0;
    reload_background_flag = 0;
    net_system_reset();
    stop_sample_using_heap(0, 122);
}

void mouse_sprite_animate(void)
{
    if (gameturn & 1)
    {
      struct TbSprite *p_mspr;
      if (++mouse_sprite_anim_frame > 7)
          mouse_sprite_anim_frame = 0;
      p_mspr = &fe_mouseptr_sprites[1 + mouse_sprite_anim_frame];
      LbMouseChangeSprite(p_mspr);
    }
}

void menu_screen_redraw(void)
{
    mo_weapon = 0;
    reload_background_flag = 1;
    if (screentype == SCRT_WORLDMAP)
    {
        open_brief = old_mission_brief;
        activate_cities(0);
    }
    else if (screentype == SCRT_MISSION)
    {
        activate_cities(open_brief);
    }

    reset_system_menu_boxes_flags();
    reset_sys_scr_shared_boxes_flags();

    reset_brief_screen_boxes_flags();
    reset_heading_screen_boxes_flags();
    reset_debrief_screen_boxes_flags();
    reset_net_screen_boxes_flags();
    reset_world_screen_boxes_flags();
    reset_login_screen_boxes_flags();
    reset_controls_screen_boxes_flags();
    reset_storage_screen_boxes_flags();
    reset_cryo_screen_boxes_flags();
    reset_equip_screen_boxes_flags();
    reset_research_screen_boxes_flags();

    reset_options_audio_boxes_flags();
    reset_options_gfx_boxes_flags();

    set_flag01_storage_screen_boxes();
    set_flag01_login_screen_boxes();
    set_flag01_main_screen_boxes();
    set_flag01_cryo_screen_boxes();
    set_flag01_research_screen_boxes();
    set_flag01_net_screen_boxes();
    set_flag01_equip_screen_boxes();
    set_flag01_controls_screen_boxes();
    set_flag01_brief_screen_boxes();
    set_flag01_world_screen_boxes();

    if (!game_projector_speed && screentype != SCRT_99)
        play_sample_using_heap(0, 113, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
}

/** Mark beginning of user input processing code.
 */
void input_processing_beg(void)
{
    if (joy.Buttons[0] && !net_unkn_pos_02)
    {
        if (!simulated_mouse_click)
        {
            int i;

            simulated_mouse_click = 1;

            // Simulate mouse click at current move position
            lbDisplay.LeftButton = 1;
            if (lbDisplay.GraphicsScreenWidth > 320)
                i = lbDisplay.MMouseX * lbDisplay.GraphicsScreenWidth / 320;
            else
                i = lbDisplay.MMouseX;
            lbDisplay.MouseX = i;
            if (lbDisplay.GraphicsScreenHeight > 200)
                i = lbDisplay.MMouseY * lbDisplay.GraphicsScreenHeight / 200;
            else
                i = lbDisplay.MMouseY;
            lbDisplay.MouseY = i;
        }
    }
    else if (simulated_mouse_click)
    {
        simulated_mouse_click = 0;
    }

    clear_left_button = lbDisplay.LeftButton;
    clear_right_button = lbDisplay.RightButton;
#if defined(LB_ENABLE_MOUSE_WHEEL)
    clear_wheel_up = lbDisplay.WheelMoveUp;
    clear_wheel_down = lbDisplay.WheelMoveDown;
#endif
}

/** Mark end of user input processing code.
 */
void input_processing_end(void)
{
    if (clear_left_button && lbDisplay.LeftButton) {
        clear_left_button = 0;
        lbDisplay.LeftButton = 0;
    }
    if (clear_right_button && lbDisplay.RightButton) {
        clear_right_button = 0;
        lbDisplay.RightButton = 0;
    }
#if defined(LB_ENABLE_MOUSE_WHEEL)
    // Clear cummulative inputs if they remained unused after processing all inputs
    if (clear_wheel_up && lbDisplay.WheelMoveUp) {
        clear_wheel_up = 0;
        lbDisplay.WheelMoveUp = 0;
    }
    if (clear_wheel_down && lbDisplay.WheelMoveDown) {
        clear_wheel_down = 0;
        lbDisplay.WheelMoveDown = 0;
    }
#endif
}

void apply_change_screen(void)
{
    if (change_screen == ChSCRT_SYSMENU)
    {
        screentype = SCRT_SYSMENU;
        redraw_screen_flag = 1;
        set_heading_box_text("");
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
    }
    if (change_screen == ChSCRT_PANET)
    {
        screentype = SCRT_PANET;
        redraw_screen_flag = 1;
        set_heading_box_text(gui_strings[367]);
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
    }
    if (change_screen == ChSCRT_WORLDMAP)
    {
        set_heading_box_text(gui_strings[368]);
        redraw_screen_flag = 1;
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
        screentype = SCRT_WORLDMAP;
        if (selected_city_id != -1)
          map_hl_city_id = selected_city_id;
    }
    if (change_screen == ChSCRT_CRYO)
    {
        screentype = SCRT_CRYO;
        switch_shared_equip_screen_buttons_to_cybmod();

        update_cybmod_cost_text();
        redraw_screen_flag = 1;
        reset_mod_draw_states_flag08();
        current_drawing_mod = ModDPt_CHEST;
        new_current_drawing_mod = ModDPt_CHEST;
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
    }
    if (change_screen == ChSCRT_EQUIP)
    {
        screentype = SCRT_EQUIP;
        switch_shared_equip_screen_buttons_to_equip();
        update_equip_cost_text();
        redraw_screen_flag = 1;
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
    }
    if (change_screen == ChSCRT_RESEARCH)
    {
        screentype = SCRT_RESEARCH;
        set_heading_box_text(gui_strings[371]);
        clear_research_screen();
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
        redraw_screen_flag = 1;
    }
    if (change_screen == ChSCRT_MISBRIEF)
    {
        selected_city_id = -1;
        screentype = SCRT_MISSION;
        brief_load_mission_info();
        redraw_screen_flag = 1;
        edit_flag = 0;
        change_screen = ChSCRT_NONE;
    }

    assert(change_screen == 0);
}

void show_menu_screen(void)
{
    switch (data_1c498d)
    {
    case 2:
        data_1c498d = 1;
        show_menu_screen_st2();
        play_sample_using_heap(0, 122, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 3);
        break;
    case 0:
        data_1c498d = 1;
        show_menu_screen_st0();
        play_sample_using_heap(0, 122, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 3);
        break;
    default:
        break;
    }

    if (lbDisplay.ScreenMode != screen_mode_menu)
    {
        LbMouseReset();
        LbScreenClear(0);
        setup_screen_mode(screen_mode_menu);
        reload_menu_flags |= RelMnuF_ColorsSprites;
    }

    if ((reload_menu_flags & RelMnuF_ColorsSprites) != 0)
    {
        reload_menu_flags &= ~RelMnuF_ColorsSprites;
        init_purple_mode_colors_and_sprites();
        my_set_text_window(0, 0, lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight);
    }

    if ((reload_menu_flags & RelMnuF_ScrBoxesFull) != 0)
    {
        reload_menu_flags &= ~RelMnuF_ScrBoxesFull;
        init_screen_boxes();
    }

    if ((reload_menu_flags & RelMnuF_BriefScanner) != 0)
    {
        reload_menu_flags &= ~RelMnuF_BriefScanner;
        init_brief_screen_scanner();
    }

    if (screentype == SCRT_MAINMENU)
    {
        replay_intro_timer++;
        if (replay_intro_timer > INTRO_REPLAY_TURNS)
        {
            play_smacker_then_back_to_menu(MPly_Intro);
            replay_intro_timer = 0;
            data_1c498d = 0;
            return;
          }
    }

    if (screentype != SCRT_MAINMENU) {
        replay_intro_timer = 0;
    }
    text_buf_pos = lbDisplay.GraphicsScreenWidth * lbDisplay.GraphicsScreenHeight;

    show_purple_status_top_bar();

    if (is_purple_apps_selection_bar_visible())
          show_purple_apps_selection_bar();

    if ((screentype == SCRT_DEBRIEF || screentype == SCRT_NETDEBRF) && change_screen == ChSCRT_MISBRIEF)
    {
        change_screen = ChSCRT_MISBRIEF;
        apply_change_screen();
    }

    input_processing_beg();

    input_date_time();

    // TODO separate input functions from draw functions and update functions
    switch (screentype)
    {
    case SCRT_MISSION:
        show_mission_screen();
        break;
    case SCRT_WORLDMAP:
        show_worldmap_screen();
        break;
    case SCRT_CRYO:
        show_cryo_chamber_screen();
        break;
    case SCRT_EQUIP:
        show_equipment_screen();
        break;
    case SCRT_MAINMENU:
        show_main_screen();
        break;
    case SCRT_SYSMENU:
        show_sysmenu_screen();
        break;
    case SCRT_RESEARCH:
        show_research_screen();
        break;
    case SCRT_PANET:
        show_panet_screen();
        break;
    case SCRT_DEBRIEF:
        show_debrief_screen();
        break;
    case SCRT_LOGIN:
        show_login_screen();
        break;
    case SCRT_NETDEBRF:
        show_debrief_screen();
        break;
    case SCRT_ALERTBOX:
        show_alert_box();
        break;
    }

    if (is_purple_apps_selection_bar_visible() && !is_purple_alert_on_top())
        input_purple_apps_selection_bar();

    if (is_key_pressed(KC_F12, KMod_DONTCARE)) {
        clear_key_pressed(KC_F12);
        post_render_action = PRend_SaveScreenshot;
    }

    input_processing_end();

    if (login_control[0].State == LognCt_NetStarted)
    {
        net_unkn_func_33();
    }
    else if (login_control[0].State == LognCt_Unkn8)
    {
        start_into_mission = 1;
        in_network_game = 1;
        redraw_screen_flag = 1;
        local_player_no = LbNetworkPlayerNumber();
        net_players_num = LbNetworkSessionNumberPlayers();
        switch_net_screen_boxes_to_initiate();
        net_players_copy_equip_and_cryo_now();
        net_player_chat_init();
    }

    memcpy(lbDisplay.WScreen, back_buffer, lbDisplay.GraphicsScreenWidth * lbDisplay.GraphicsScreenHeight);
    draw_purple_screen();

    switch (post_render_action)
    {
    case PRend_SaveScreenshot:
        LbPngSaveScreen("synII", lbDisplay.WScreen, display_palette, 0);
        break;
    default:
        break;
    }

    update_date_time();

    apply_change_screen();

    if (show_alert)
    {
        reset_alert_screen_boxes_flags();
        edit_flag = 0;
        old_screentype = screentype;
        show_alert = 0;
        screentype = SCRT_ALERTBOX;
        set_flag01_alert_screen_boxes();
        redraw_screen_flag = 1;
    }

    if (redraw_screen_flag && !edit_flag)
    {
        redraw_screen_flag = 0;
        menu_screen_redraw();
    }

    mouse_sprite_animate();

    if (start_into_mission || map_editor)
    {
        show_load_and_prep_mission();
        data_1c498d = 2;
    }
    else if (reload_background_flag)
    {
        reload_background();
        reload_background_flag = 0;
    }

    if (exit_game)
    {
        wait_for_sound_sample_finish(111);
        stop_sample_using_heap(0, 122);
    }
}

void show_game_screen(void)
{
    PlayCDTrack(ingame.CDTrack);

    if (skip_redraw_this_turn())
        return;
    show_game_engine();

    if ((ingame.Flags & GamF_Unkn0800) != 0)
        gproc3_unknsub2();

    game_turn_view_end();
}

void draw_game(void)
{
    // One more frame is about to be drawn. The counters the drawing uses
    // advance here, so that they follow the frames and not the game turns.
    render_clock_next_frame(RENDER_ANIM_TURN_UNIT);

    switch (ingame.DisplayMode)
    {
    case DpM_UNKN_1:
        // No action
        break;
    case DpM_ENGINEPLY:
        show_game_screen();
        break;
    case DpM_PURPLEMNU:
        show_menu_screen();
        break;
    case DpM_UNKN_3A:
        show_unkn3A_screen(0);
        break;
    default:
        LOGERR("DisplayMode %d empty\n", (int)ingame.DisplayMode);
        break;
    }
}

ubyte critical_action_input(void)
{
    char locstr[52];
    TbKeyCode key;

    strcpy(locstr, "Confirm Critical Action Y/N?");

    DangerMusicFadeSwitch(2, 4u);
    SetMusicTempo(110, 1000);

    draw_message_box(locstr);
    key = screen_wait_seconds_or_until_yesno_key(20);

    DangerMusicFadeSwitch(1, 1);
    SetMusicTempoNormal();

    return (key == KC_Y);
}

ubyte process_send_person(PlayerIdx plyr, ubyte dmuser)
{
    PlayerInfo *p_player;
    TbBool usrinp_is_goto_point;

    p_player = &players[plyr];

    if (p_player->State[dmuser] != 1)
    {
        p_player->State[dmuser] = 1;
        p_player->SubState[dmuser] = 0;
    }

    usrinp_is_goto_point = ((p_player->UserInput[dmuser].Bits & SpUIn_GotoPoint) != 0);

    if (!usrinp_is_goto_point && (p_player->SubState[dmuser] == 0))
    {
        p_player->SubState[dmuser] = 1;
        return 0;
    }
    if (usrinp_is_goto_point && (p_player->SubState[dmuser] == 1))
    {
        p_player->SubState[dmuser] = 2;
        return 0;
    }
    if (!usrinp_is_goto_point && (p_player->SubState[dmuser] == 2))
    {
        p_player->SubState[dmuser] = 3;
        p_player->State[dmuser] = 0;
        return 1;
    }
    return 0;
}

void do_agent_track_only(void)
{
    static const TbKeyCode sel_agent_keys[] = {
        KC_1, KC_2, KC_3, KC_4,
    };
    PlayerInfo *p_locplayer;
    int n;

    p_locplayer = &players[local_player_no];

    for (n = 0; n < (int)(sizeof(sel_agent_keys)/sizeof(sel_agent_keys[0])); n++)
    {
        TbKeyCode kkey = sel_agent_keys[n];
        if (is_key_pressed(kkey, KMod_DONTCARE))
        {
            struct Thing *p_agent;

            p_agent = p_locplayer->MyAgent[n];
            ingame.TrackX = PRCCOORD_TO_MAPCOORD(p_agent->X);
            ingame.TrackZ = PRCCOORD_TO_MAPCOORD(p_agent->Z);
            engn_xc = ingame.TrackX;
            engn_zc = ingame.TrackZ;
        }
    }
}

void input_packet_playback(void)
{
    struct Packet *p_pckt;

    p_pckt = &packets[local_player_no];

    if (is_key_pressed(KC_ESCAPE, KMod_SHIFT))
    {
        clear_key_pressed(KC_ESCAPE);
        if (critical_action_input())
        {
            exit_game = 1;
            p_pckt->Action = PAct_MISSN_ABORT;
        }
    }
    do_scroll_map();
    do_rotate_map();

    do_agent_track_only();
}

ubyte input_mission_cheats(void)
{
    if ((ingame.UserFlags & UsrF_Cheats) != 0)
    {
        if (is_key_pressed(KC_C, KMod_ALT))
        {
            clear_key_pressed(KC_C);
            mission_result = 1;
        }
        if (is_key_pressed(KC_F, KMod_ALT))
        {
            clear_key_pressed(KC_F);
            mission_result = -1;
        }
    }
    return GINPUT_NONE;
}

void draw_mission_concluded(void)
{
    uint tm;

    tm = (dos_clock() - ingame.fld_unkC91) / 100;
    if (ingame.fld_unkCB5)
    {
        sprintf(mission_status_text, "%s %s: %s ", gui_strings[GSTR_CHK_MISSION_STA_PRE],
          gui_strings[GSTR_ENM_MISSION_STATUS + 1 + ingame.MissionStatus], scrollinfo_text);
        panel_conclusion_info_set(mission_status_text);
    }
    else
    {
        uint tm_h, tm_m, tm_s;

        tm_h = tm / 3600;
        tm_m = tm / 60;
        tm_s = tm % 60;

        sprintf(mission_status_text, "%s %s %s %s %02d:%02d:%02d", gui_strings[GSTR_CHK_MISSION_STA_PRE],
          gui_strings[GSTR_ENM_MISSION_STATUS + 1 + ingame.MissionStatus],
          gui_strings[GSTR_CHK_MISSION_STA_SUF_KEYS], gui_strings[GSTR_CHK_MISSION_STA_TIME],
          tm_h, tm_m % 60, tm_s);
        LbStringToUpper(mission_status_text);
        panel_conclusion_info_set(mission_status_text);
        scrollinfo_text = mission_status_text;
    }
    panel_conclusion_info_draw();
}

void input_mission_concluded(void)
{
    if (is_key_pressed(KC_RETURN, KMod_DONTCARE) ||
      is_key_pressed(KC_SPACE, KMod_DONTCARE) ||
      is_key_pressed(KC_ESCAPE, KMod_NONE))
    {
        if (ingame.MissionStatus != ObvStatu_MissTmp_FAILED || is_key_pressed(KC_ESCAPE, KMod_NONE))
        {
          clear_key_pressed(KC_ESCAPE);
          clear_key_pressed(KC_SPACE);
          clear_key_pressed(KC_RETURN);
          ingame.MissionEndFade = 1;
        }
        else
        {
          simulate_key_press(KC_R);
          clear_key_pressed(KC_SPACE);
          clear_key_pressed(KC_RETURN);
        }
    }
}

void load_packet(void)
{
    PlayerInfo *p_locplayer;
    struct Packet *p_pckt;
    ubyte did_inp;

    p_locplayer = &players[local_player_no];
    p_pckt = &packets[local_player_no];

    game_graphics_inputs();
    if (PacketRecord_IsPlayback()) // packet replay controls
    {
        if (!in_network_game)
            PacketRecord_Read(p_pckt, p_locplayer->DoubleMode);
        input_packet_playback();
        ingame.MissionStatus = test_missions(0);
        return;
    }

    did_inp = GINPUT_NONE;
    if (ingame.DisplayMode == DpM_ENGINEPLY || ingame.DisplayMode == DpM_UNKN_3B)
    {
        did_inp |= do_user_interface();
        // commands of a front end using the game library, as mouse clicks
        if (((did_inp & GINPUT_PACKET) == 0) && swcore_input_packet(p_pckt))
            did_inp |= GINPUT_PACKET;
        // commands from the VR viewer (shared memory), taken the same way
        if (((did_inp & GINPUT_PACKET) == 0) && vr_spectator_input())
            did_inp |= GINPUT_PACKET;
        if ((did_inp & GINPUT_PACKET) == 0)
            did_inp |= input_agents_cheats();
        if ((did_inp & GINPUT_PACKET) == 0)
            did_inp |= input_mission_cheats();
        ingame.MissionStatus = test_missions(0);
        if ((ingame.MissionStatus != ObvStatu_UNDECIDED) && !in_network_game)
        {
            draw_mission_concluded();
            input_mission_concluded();
            game_core_mission_end_fade_start();
        }
    }

    if ((ingame.MissionEndFade != 0) && (ingame.MissionEndFade < 30))
        change_brightness(-2);
    if (game_core_mission_end_fade_turn())
        mission_over();
    if (did_inp == GINPUT_EXIT)
    {
        if (is_single_game) {
            exit_game = 1;
        } else {
            init_level_3d(1);
            if (ingame.MissionStatus != ObvStatu_COMPLETED)
                ingame.MissionStatus = ObvStatu_MissTmp_FAILED;
            mission_over();
        }
    }

    if ((did_inp == 0) || (p_locplayer->DoubleMode != 0))
    {
        short dmuser;

        for (dmuser = 0; dmuser < p_locplayer->DoubleMode + 1; dmuser++)
        {
            if (p_locplayer->DoubleMode != 0) {
                ushort ctlmode;
                ctlmode = user_input_control_mode_get(local_player_no, dmuser);
                if (ctlmode == UInpCtr_Mouse)
                    continue;
            }
            if (ingame.TrackThing != 0)
                continue;

            did_inp |= input_user_control_agent(local_player_no, dmuser);
        }
    }

    if (p_locplayer->PanelState[mouser] != PANEL_STATE_SEND_MESSAGE)
    {
        if (in_network_game || (ingame.UserFlags & UsrF_Cheats) != 0)
        {
            if (!in_network_game && is_key_pressed(KC_ESCAPE, KMod_CONTROL))
            {
                clear_key_pressed(KC_ESCAPE);
                exit_game = 1;
            }
            else if (is_key_pressed(KC_ESCAPE, KMod_SHIFT))
            {
                clear_key_pressed(KC_ESCAPE);
                if (in_network_game || critical_action_input()) {
                    change_brightness(-32);
                    p_pckt->Action = PAct_MISSN_ABORT;
                    did_inp |= GINPUT_EXIT;
                }
            }
        }

        if (PacketRecord_IsRecord() && !in_network_game)
        {
            PacketRecord_Write(p_pckt, p_locplayer->DoubleMode);
        }
    }
}

void joy_input(void)
{
    JoyUpdateInputs(&joy);

    PlayerInfo *p_locplayer = &players[local_player_no];

    if (p_locplayer->DoubleMode && ingame.DisplayMode != DpM_PURPLEMNU)
    {
        if (ingame.DisplayMode == DpM_ENGINEPLY)
        {
            for (int i = 0; i < joy.NumberOfDevices; i++)
            {
                if (!joy.Init[i])
                    continue;

                JoyButtonSet rotate_btns = jskeys[GKey_VIEW_SPIN_R] | jskeys[GKey_VIEW_SPIN_L]
                                        | jskeys[GKey_VIEW_TILT_U] | jskeys[GKey_VIEW_TILT_D] 
                                        | jskeys[GKey_ZOOM_IN] | jskeys[GKey_ZOOM_OUT]
                                        #if defined MORE_GAME_KEYS
                                        | jskeys[GKey_VIEW_PAN_R] | jskeys[GKey_VIEW_PAN_L]
                                        #endif
                                           ;
                joy.Buttons[0] |= (joy.Buttons[i] & rotate_btns);
            }
        }
    }
    else
    {
        for (int i = 0; i < joy.NumberOfDevices; i++)
        {
            if (!joy.Init[i])
                continue;

            joy.Buttons[0] |= joy.Buttons[i];
            joy.DigitalX[0] |= joy.DigitalX[i];
            joy.DigitalY[0] |= joy.DigitalY[i];
        }
    }
}


void game_process_begin(void)
{
    debug_multicolor_sprite(193);
    LOGDBG("WSCREEN 0x%p", (void *)lbDisplay.WScreen);
}

/** One turn of the main loop: input, world update, drawing, sound.
 * Returns false once the game asked to exit.
 */
TbBool game_process_turn(void)
{
    if (exit_game)
        return false;
    process_sound_heap();
    navi2_unkn_counter -= 2;
    if (navi2_unkn_counter < 0)
        navi2_unkn_counter = 0;
    if (navi2_unkn_counter > navi2_unkn_counter_max)
        navi2_unkn_counter_max = navi2_unkn_counter;
    if (keyboard_mode_direct)
        input_char = LbKeyboard();
    if (ingame.DisplayMode == DpM_PURPLEMNU) {
        LOGDBG("id=%d  trial alloc = %d turn %u", 0, triangulation, gameturn);
    }
#if DIFFTEST_HARNESS
    // Test runs with the pointer away from the screen, so that drawing,
    // which picks the thing under the pointer, picks none
    if (getenv("SWARS_DIFFTEST_NOPOINTER") != NULL) {
        lbDisplay.MMouseX = -10000;
        lbDisplay.MMouseY = -10000;
    }
#endif
    input();
    update_tick_time();
    // Faces thrown by an explosion can hit people once they land, so they
    // are world state and progress with the world - not from within the
    // renderer, where this used to be called from.
    if ((ingame.DisplayMode == DpM_ENGINEPLY)
      && ((ingame.Flags & TngF_ProgressAction) != 0))
        process_explode();
    draw_game();
    debug_trace_turn_bound(gameturn);
    load_packet();
    difftest_bot_turn();
    if ( ((active_flags_general_unkn01 & 0x8000) != 0) !=
      ((ingame.Flags & GamF_ThermalView) != 0) )
        LbPaletteSet(display_palette);
    active_flags_general_unkn01 = ingame.Flags;
    if ((ingame.DisplayMode == DpM_ENGINEPLY)
      || (ingame.DisplayMode == DpM_UNKN_1)
      || (ingame.DisplayMode == DpM_UNKN_3B))
        process_things();
    if (debug_hud_things) {
        bad_tile_debug();
        things_debug_hud();
    }
    if (ingame.DisplayMode != DpM_PURPLEMNU)
        process_packets();
    joy_input();

    if (ingame.DisplayMode == DpM_PURPLEMNU)
    {
        game_update();
        swap_wscreen();
    }
    else if (!skip_redraw_this_turn())
    {
        game_update();
        LbScreenSwapClear(0);
    }

    update_unkn_changing_colors();
    game_process_orbital_station_explode();
    gameturn++;
    difftest_game_turn();
    game_turn_statetrace();
    vr_spectator_update();
    scene_post_effect_prepare();
    return !exit_game;
}

void game_process_end(void)
{
    PacketRecord_Close();
    LbPaletteFade(NULL, 0x10u, 1);
}

void game_process(void)
{
    game_process_begin();
    while (game_process_turn())
        ;
    game_process_end();
}

/** Frees what the game set up, without ending the process.
 */
void game_shutdown(void)
{
    host_reset();
    LbBaseReset();
}

void game_quit(void)
{
    game_shutdown();
    exit(0);
}

static void game_update_full(bool wait)
{
    display_unlock();

    game_handle_sdl_events();

    if (wait && !difftest_skip_turn_wait())
    {
        wait_next_gameturn();
    }

    display_lock();
}

int game_hacky_update(void)
{
    game_update_full(false);
    return 1;
}

void game_update(void)
{
    game_update_full(true);
}

void engine_reset(void)
{
    LbMemoryFree(engine_mem_alloc_ptr);
}

void host_reset(void)
{
    StopCD();
    setup_heaps(SHSC_ResetGameSnd, language_3str);
    FreeAudio();
    engine_reset();
    reset_multicolor_sprites();
    reset_mouse_pointers();
    LbMouseReset();
    LbKeyboardClose();
    LbScreenReset();
    LbNetworkReset();
}

void game_reset(void)
{
    host_reset();
    free_texturemaps();
    LbDataFreeAll(missionspr_load_files);
    game_core_free_data();
}

/******************************************************************************/
