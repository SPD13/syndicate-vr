/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file plyr_usrstate.c
 *     Player user input state.
 * @par Purpose:
 *     The input state of players kept in the game (bits of the special user
 *     input, control modes, agent groups); part of the game core. Reading
 *     the keyboard and joystick into this state is in plyr_usrinp.c, in the
 *     front end.
 * @par Comment:
 *     Moved from plyr_usrinp.c unchanged.
 * @author   Tomasz Lis
 * @date     11 Dec 2024 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "plyr_usrinp.h"

#include "game.h"
#include "player.h"
/******************************************************************************/

struct UnkPlayerGroup unkn_player_groups[8][5];

/******************************************************************************/
short get_agent_move_direction_delta_x(const struct SpecialUserInput *p_usrinp)
{
    return (sbyte)(p_usrinp->Bits >> 0);
}

void set_agent_move_direction_delta_x(struct SpecialUserInput *p_usrinp, short dt)
{
    if (dt > 127)
        dt = 127;
    if (dt < -127)
        dt = -127;
    //p_usrinp->Bits &= ~(0xFF << 0); -- no need, should be always clear before set
    p_usrinp->Bits |= (((ubyte)dt) << 0);
}

short get_agent_move_direction_delta_z(const struct SpecialUserInput *p_usrinp)
{
    return (sbyte)(p_usrinp->Bits >> 8);
}

void set_agent_move_direction_delta_z(struct SpecialUserInput *p_usrinp, short dt)
{
    if (dt > 127)
        dt = 127;
    if (dt < -127)
        dt = -127;
    //p_usrinp->Bits &= ~(0xFF << 8); -- no need, should be always clear before set
    p_usrinp->Bits |= (((ubyte)dt) << 8);
}

void do_user_input_bits_direction_clear(struct SpecialUserInput *p_usrinp)
{
    p_usrinp->Bits &= ~(0xFF << 0);
    p_usrinp->Bits &= ~(0xFF << 8);
}

void do_user_input_bits_control_clear_all(struct SpecialUserInput *p_usrinp)
{
    p_usrinp->Bits &= ~SpUIn_AllControlBits;
}

void do_user_input_bits_control_clear_nonmove(struct SpecialUserInput *p_usrinp)
{
    p_usrinp->Bits &= ~SpUIn_AllNonMoveBits;
}

void reset_user_groups(void)
{
    PlayerIdx plyr;

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        ushort plgroup;

        for (plgroup = 0; plgroup < 5; plgroup++) {
            unkn_player_groups[plyr][plgroup].GroupActive = 0;
        }
    }
}

void reset_user_input(void)
{
    PlayerIdx plyr;

    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        PlayerInfo *p_player;
        struct SpecialUserInput *p_usrinp;
        ushort dmuser;

        p_player = &players[plyr];

        for (dmuser = 0; dmuser < LOCAL_USERS_MAX_COUNT; dmuser++) {
            p_player->State[dmuser] = 0;
            p_player->SubState[dmuser] = 0;

            p_usrinp = &p_player->UserInput[dmuser];
            do_user_input_bits_control_clear_all(p_usrinp);
        }
    }
}

void user_input_control_mode_set(PlayerIdx plyr, ubyte dmuser, ushort ctrmode)
{
    PlayerInfo *p_player;

    p_player = &players[plyr];
    p_player->UserInput[dmuser].ControlMode = ctrmode;
}

ushort user_input_control_mode_get(PlayerIdx plyr, ubyte dmuser)
{
    PlayerInfo *p_player;

    p_player = &players[plyr];
    return p_player->UserInput[dmuser].ControlMode & ~UInpCtr_AllFlagsMask;
}

void user_input_control_flags_raise(PlayerIdx plyr, ubyte dmuser, ushort ctrflags)
{
    PlayerInfo *p_player;

    p_player = &players[plyr];
    p_player->UserInput[dmuser].ControlMode |= ctrflags;
}

void user_input_control_flags_clear(PlayerIdx plyr, ubyte dmuser, ushort ctrflags)
{
    PlayerInfo *p_player;

    p_player = &players[plyr];
    p_player->UserInput[dmuser].ControlMode &= ~ctrflags;
}

TbBool user_input_control_flags_check(PlayerIdx plyr, ubyte dmuser, ushort ctrflags)
{
    PlayerInfo *p_player;

    p_player = &players[plyr];
    return (p_player->UserInput[dmuser].ControlMode & ctrflags) == ctrflags;
}

/******************************************************************************/
