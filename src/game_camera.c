/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_camera.c
 *     The game's camera, which follows the player's agents.
 * @par Purpose:
 *     Moves the camera after the tracked thing or the selected agent, and
 *     by the scroll input of the front end.
 * @par Comment:
 *     The camera belongs to the game core: the view pass (game_view_pass())
 *     sets the face flags line of sight reads only for vehicles in the
 *     camera's view. Reading the scroll keys and the pointer at the screen
 *     edge stays in the front end (do_scroll_map() in game.c).
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_camera.h"

#include <stdlib.h>

#include "bfmath.h"
#include "bfutility.h"

#include "engincam.h"
#include "engintrns.h"
#include "enginzoom.h"

#include "bigmap.h"
#include "difftest.h"
#include "game.h"
#include "game_options.h"
#include "hud_panel.h"
#include "packet.h"
#include "player.h"
#include "plyr_usrinp.h"
#include "thing.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/

/** Speed of scrolling the map; grows while the scroll input lasts. */
s32 dword_153194 = 0x100;

static DIFFTEST_REF_FUNC void move_camera_asm(int x, int y, int z)
{
    DIFFTEST_ASM(
      "call ASM_move_camera\n"
        : : "a" (x), "d" (y), "b" (z));
}

void move_camera(int x, int y, int z)
{
    s32 dt_x, dt_y, dt_z;
    s32 dist, step;

    DIFFTEST_SHADOW(move_camera, move_camera_asm(x, y, z));
    if (ingame.TrackThing != 0)
    {
        struct Thing *p_thing;

        p_thing = &things[(ushort)ingame.TrackThing];
        x = p_thing->X >> 8;
        y = p_thing->Y >> 8;
        z = p_thing->Z >> 8;
    }
    dt_x = x - engn_xc;
    dt_y = y - engn_yc;
    dt_z = z - engn_zc;
    dist = LbSqrL(dt_x * dt_x + dt_z * dt_z + dt_y * dt_y);
    step = dist / 8;
    if (step > 0x200)
        step = 0x200;
    else if (step < 5)
        step = 5;
    if (dist > 16)
    {
        dt_x = dt_x * step / dist;
        dt_y = dt_y * step / dist;
        dt_z = dt_z * step / dist;
        engn_yc += dt_y;
        engn_zc += dt_z;
        engn_xc += dt_x;
    }
}

static DIFFTEST_REF_FUNC void track_player_asm(int thing)
{
    DIFFTEST_ASM("call ASM_track_player\n"
        : : "a" (thing));
}

void track_player(int thing)
{
    struct Thing *p_thing;
    struct Thing *p_vehicle;

    DIFFTEST_SHADOW(track_player, track_player_asm(thing));
    if (pktrec_mode == 2)
    {
        ingame.TrackX = engn_xc;
        ingame.TrackZ = engn_zc;
        ingame.TrackThing = 0;
    }
    p_thing = &things[(ushort)thing];
    if (((p_thing->Flag & TngF_Unkn4000) != 0) ||
      ((p_thing->Flag & TngF_InVehicle) != 0))
    {
        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        ingame.TrackX = p_vehicle->X >> 8;
        ingame.TrackZ = p_vehicle->Z >> 8;
        move_camera(ingame.TrackX, p_vehicle->Y >> 11, ingame.TrackZ);
        return;
    }
    ingame.TrackX = p_thing->X >> 8;
    ingame.TrackZ = p_thing->Z >> 8;
    move_camera(ingame.TrackX, p_thing->Y >> 8, ingame.TrackZ);
}

static DIFFTEST_REF_FUNC void track_angle_asm(void)
{
    DIFFTEST_ASM("call ASM_track_angle\n"
        :  :  : "eax" );
}

void track_angle(void)
{
    s32 dt_angle, dir;

    DIFFTEST_SHADOW(track_angle, track_angle_asm());
    dt_angle = (s32)(ushort)ingame.fld_unkCA6 - (engn_cam_yaw >> 5);
    if (dt_angle == 0)
        return;
    if (dt_angle < -0x400)
    {
        dir = 1;
    }
    else if (dt_angle > 0x400)
    {
        dir = -1;
    }
    else
    {
        if (abs(dt_angle) < ((s32)(ushort)ingame.fld_unkCA8 >> 4))
        {
            engn_cam_yaw = (s32)(ushort)ingame.fld_unkCA6 << 5;
            return;
        }
        if (dt_angle > 0)
            dir = 1;
        else
            dir = -1;
    }
    engn_cam_yaw += (s32)(ushort)ingame.fld_unkCA8 * dir;
    engn_cam_yaw &= 0xffe0;
}

void track_y(int y)
{
    engn_yc += (y - engn_yc) >> 3;
}

void scroll_map_update_alt(void)
{
    int cumm_alt;

    cumm_alt = alt_at_point(engn_xc, engn_zc) >> 8;
    cumm_alt += alt_at_point(engn_xc + 2048, engn_zc + 2048) >> 8;
    cumm_alt += alt_at_point(engn_xc + 2048, engn_zc - 2048) >> 8;
    cumm_alt += alt_at_point(engn_xc - 2048, engn_zc + 2048) >> 8;
    cumm_alt += alt_at_point(engn_xc - 2048, engn_zc - 2048) >> 8;
    track_y(cumm_alt / 5);
}

/** Whether the player scrolls the map in this turn: with the mouse
 * control mode (or in a replay), while the panel is not used.
 */
TbBool game_camera_scroll_allowed(void)
{
    PlayerInfo *p_locplayer;
    ushort ctlmode;

    p_locplayer = &players[local_player_no];
    ctlmode = user_input_control_mode_get(local_player_no, byte_153198-1);
    if ((ctlmode != UInpCtr_Mouse) && !PacketRecord_IsPlayback())
        return false;
    return (p_locplayer->PanelState[mouser] == PANEL_STATE_NORMAL);
}

/** Moves the camera for one game turn: after the tracked thing or the
 * selected agent, then by the scroll input of the front end (-1, 0 or 1
 * on each screen axis; only used while game_camera_scroll_allowed()).
 */
void game_camera_update(int scroll_dx, int scroll_dy)
{
    PlayerInfo *p_locplayer;
    s32 engn_xc_orig, engn_zc_orig;
    ushort ctlmode;
    s32 abase, angle;
    int dx, dy, dz;
    int dampr;

    dx = 0;
    dy = 0;
    dz = 0;
    dampr = 10;
    if (ingame.fld_unkCA6)
        track_angle();
    p_locplayer = &players[local_player_no];
    if (p_locplayer->State[0] == 1)
    {
        short bitx, bitz;
        bitx = get_agent_move_direction_delta_x(&p_locplayer->UserInput[0]);
        bitz = get_agent_move_direction_delta_z(&p_locplayer->UserInput[0]);
        dx = bitx << 8;
        dz = bitz << 8;
        local_to_worldr(&dx, &dy, &dz);
        engn_xc += dx;
        engn_zc += dz;
    }
    else
    {
        short dcthing;
        dcthing = direct_control_thing_for_player(local_player_no);
        if (p_locplayer->DoubleMode)
        {
            track_player(dcthing);
            return;
        }
        if (dcthing)
        {
            ctlmode = user_input_control_mode_get(local_player_no, 0);
            if (ctlmode == UInpCtr_Mouse || PacketRecord_IsPlayback())
                move_camera(ingame.TrackX, engn_yc, ingame.TrackZ);
            else
                track_player(dcthing);
        }
    }
    dy = 0;
    dx = 0;
    engn_xc_orig = engn_xc;
    engn_zc_orig = engn_zc;
    if (game_camera_scroll_allowed())
    {
        dx = scroll_dx;
        dy = scroll_dy;
        scroll_map_update_alt();
        dampr = 9;
    }

    abase = -engn_cam_yaw >> 5;
    angle = -1;
    if (dx > 0)
        angle = (abase + 3583) & LbFPMath_AngleMask;
    if (dx < 0)
        angle = (abase + 2559) & LbFPMath_AngleMask;
    if (angle >= 0) {
        int wibl_x, wibl_y;
        wibl_x = dword_153194 * lbSinTable[angle] >> 8 >> dampr;
        wibl_y = dword_153194 * lbSinTable[angle + LbFPMath_PI/2] >> 8 >> dampr;
        ingame.TrackX -= wibl_x;
        ingame.TrackZ += wibl_y;
        engn_xc -= wibl_x;
        engn_zc += wibl_y;
    }
    angle = -1;
    if (dy > 0)
        angle = (abase + 3 * LbFPMath_PI - 1) & LbFPMath_AngleMask;
    if (dy < 0)
        angle = (abase + 2 * LbFPMath_PI - 1) & LbFPMath_AngleMask;
    if (angle >= 0) {
        int wibl_x, wibl_y;
        wibl_x = dword_153194 * lbSinTable[angle] >> 8 >> dampr;
        wibl_y = dword_153194 * lbSinTable[angle + LbFPMath_PI/2] >> 8 >> dampr;
        ingame.TrackX -= wibl_x;
        ingame.TrackZ += wibl_y;
        engn_xc -= wibl_x;
        engn_zc += wibl_y;
    }

    if (engn_xc < 0)
        engn_xc = 0;
    else if (engn_xc > MAP_COORD_WIDTH - 1)
        engn_xc = MAP_COORD_WIDTH - 1;
    if (engn_zc < 0)
        engn_zc = 0;
    else if (engn_zc > MAP_COORD_HEIGHT - 1)
        engn_zc = MAP_COORD_HEIGHT - 1;
    // If unsigned value is exceeded by more than half of the range, treat as below minimum
    if (ingame.TrackX > MAP_COORD_WIDTH + MAP_COORD_WIDTH/2)
        ingame.TrackX = 0;
    else if (ingame.TrackX > MAP_COORD_WIDTH - 1)
        ingame.TrackX = MAP_COORD_WIDTH - 1;
    if (ingame.TrackZ > MAP_COORD_HEIGHT + MAP_COORD_HEIGHT/2)
        ingame.TrackZ = 0;
    else if (ingame.TrackZ > MAP_COORD_HEIGHT - 1)
        ingame.TrackZ = MAP_COORD_HEIGHT - 1;

    if (dx) {
        dword_153194 += 4 + ((dword_153194 - 252) >> 5);
        if (dword_153194 > 800)
            dword_153194 = 800;
    } else {
        dword_153194 -= (dword_153194 - 256) >> 2;
    }
    if ((engn_zc - engn_zc_orig) || (engn_xc - engn_xc_orig)) {
        engn_x_vel = engn_xc - engn_xc_orig;
        engn_z_vel = engn_zc - engn_zc_orig;
    } else {
        engn_x_vel >>= 2;
        engn_z_vel >>= 2;
        if (abs(engn_x_vel) < 5)
            engn_x_vel = 0;
        if (abs(engn_z_vel) < 5)
            engn_z_vel = 0;
        engn_xc += engn_x_vel;
        engn_zc += engn_z_vel;
    }
}

/** Applies the view input of one game turn: rotating (-1, 0, 1, the spin
 * slows down without input), zooming and tilting.
 */
void game_camera_input(short rotate_input, short zoom_input, short tilt_input)
{
    // Update zoom level
    if (zoom_input != 0)
    {
        short new_zoom = ingame.UserZoom + (zoom_input * 8);

        if (new_zoom < CAMERA_ZOOM_MIN) {
            if (pktrec_mode != PktR_PLAYBACK) {
                new_zoom = CAMERA_ZOOM_MIN;
            }
        }
        else if (new_zoom > CAMERA_ZOOM_MAX) {
            new_zoom = CAMERA_ZOOM_MAX;
        }

        ingame.UserZoom = new_zoom;
    }

    if (tilt_input != 0)
    {
        s32 new_cam_tilt = engn_cam_tilt + (tilt_input * CAMERA_TILT_INPUT_MULTIPLIER);
        if (new_cam_tilt < CAMERA_TILT_MIN) {
            new_cam_tilt = CAMERA_TILT_MIN;
        }
        else if (new_cam_tilt > CAMERA_TILT_MAX) {
            new_cam_tilt = CAMERA_TILT_MAX;
        }
        engn_cam_tilt = new_cam_tilt;
    }

    s32 new_cam_yaw_vel = engn_cam_yaw_vel + (rotate_input * CAMERA_ROTATION_INPUT_MULTIPLIER);
    new_cam_yaw_vel = (3 * new_cam_yaw_vel) / 4;
    engn_cam_yaw_vel = new_cam_yaw_vel;
}

/** Step overall scale towards given zoom value.
 *
 * Overal scale changes gradually towards given value; shis
 * function does a one step update of the overall_scale.
 */
void process_overall_scale(int zoom)
{
    int zdelta, sdelta;
    short scmin, scmax, scale;

    zoom = get_scaled_zoom(zoom);

    zdelta = zoom - overall_scale;
    if ((zdelta > 0) && (zdelta < 8))
        sdelta = 1;
    else if ((zdelta < 0) && (zdelta > -8))
        sdelta = -1;
    else
        sdelta = zdelta >> 3;

    scmin = get_overall_scale_min();
    scmax = get_overall_scale_max();

    scale = sdelta + overall_scale;
    if (scale < scmin)
        scale = scmin;
    else if (scale > scmax)
        scale = scmax;
    overall_scale = scale;
}

void process_view_inputs(int thing)
{
    struct Thing *p_person;
    int zoom;

    p_person = &things[thing];
    zoom = get_weapon_zoom_min(p_person->U.UPerson.CurrentWeapon);
    // User zoom is not scaled to resolution
    if (zoom >= ingame.UserZoom)
        ingame.UserZoom = zoom;
    else
        zoom = ingame.UserZoom;

    process_overall_scale(zoom);
}

/******************************************************************************/
