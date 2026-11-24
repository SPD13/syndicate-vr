/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file bat.c
 *     Routines implementing Breakout minigame.
 * @par Purpose:
 *     Implement functions for handling breakout-like minigame.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     27 May 2022 - 22 Sep 2025
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bat.h"

#include "bfgentab.h"
#include "bfkeybd.h"
#include "bfmemut.h"
#include "bfscreen.h"
#include "ssampply.h"
#include <stdlib.h>
#include <string.h>

#include "engincolour.h"
#include "engintxtrmap.h"

#include "bigmap.h"
#include "crtrand.h"
#include "difftest.h"
#include "game.h"
#include "game_options.h"
#include "keyboard.h"
#include "player.h"
#include "sound.h"
#include "swlog.h"
#include "thing.h"
/******************************************************************************/

#pragma pack(1)

/** Minigame screen line width.
 *
 * To allow the output buffer to be used as texture, it needs to have
 * size which is compatible with textures. The actual size which we will
 * utilize can be smaller.
 */
#define BAT_SCREEN_LINE_WIDTH 256

#define BAT_SCREEN_WIDTH 96
#define BAT_SCREEN_HEIGHT 64

#define BAT_BRICK_COLUMNS 12
#define BAT_BRICK_ROWS 10

#define BAT_BALLS_LIMIT 32
#define BAT_BONUSES_LIMIT 16

struct BreakoutLevel {
    char *lv_name;
    ubyte bricks[BAT_BRICK_COLUMNS * BAT_BRICK_ROWS];
};

struct BATBall;

struct BATBall {
    int X;
    int Y;
    int UnkDw2;
    int UnkDw3;
    int NextX;
    int NextY;
    int UnkDw6;
    int UnkDw7;
    struct BATBall *ListNext;
    struct BATBall **ListPvNxPtr;
};

struct BATBonus;

struct BATBonus {
    int X;
    int Y;
    int UnkDw2;
    int UnkDw3;
    struct BATBonus *ListNext;
    struct BATBonus **ListPvNxPtr;
};

enum BATStates {
  BATSt_Reset = 0,
  BATSt_LevelIntro,
  BATSt_Gameplay,
  BATSt_BallLost,
  BATSt_GameOver,
  BATSt_GameWon,
};

#pragma pack()

extern int bat_unknvalA_min;
extern int bat_unknvalB_min;
extern int bat_unknvalA_max;
extern int bat_unknvalB_max;
extern int BAT_data_1e26e8;
extern int BAT_game_won_timer;
extern int BAT_level_intro_timer;
extern int BAT_ball_lost_timer;
extern int BAT_game_over_timer;
extern int BAT_state;
extern int BAT_levelno;
extern int BAT_num_lives;
extern int BAT_score;
extern int BAT_bricks_remain;
extern ubyte *BAT_screen;
extern ubyte BAT_bricks[BAT_BRICK_COLUMNS * BAT_BRICK_ROWS];
extern int BAT_paddle_x;
extern int BAT_data_1e2798;
extern TbPixel BAT_lives_colour;

extern struct BATBall BAT_balls_store[BAT_BALLS_LIMIT];
/** Linked list of unused balls. */
extern struct BATBall *BAT_free_balls;
/** Linked list of existing balls. */
extern struct BATBall *BAT_balls_head;
extern TbPixel BAT_ball_colour;

extern struct BATBonus BAT_bonus_store[BAT_BONUSES_LIMIT];
/** Linked list of unused bonus items. */
extern struct BATBonus *BAT_free_bonuses;
/** Linked list of existing bonus items. */
extern struct BATBonus *BAT_bonuses_head;

extern struct BreakoutLevel BAT_levels[];

static void BAT_screen_clear_rect(int beg_x, int beg_y, int end_x, int end_y)
{
    ubyte *o;
    int y;

    o = BAT_screen + beg_y * BAT_SCREEN_LINE_WIDTH;
    for (y = beg_y; y < end_y; y++)
    {
        memset(o + beg_x, 0, end_x - beg_x);
        o += BAT_SCREEN_LINE_WIDTH;
    }
}

static void BAT_screen_clear(void)
{
    BAT_screen_clear_rect(0, 0, BAT_SCREEN_WIDTH, BAT_SCREEN_HEIGHT);
}

static void BAT_draw_remainig_lives(int pos_x, int pos_y)
{
    ubyte *o;
    int y;

    o = BAT_screen + pos_y * BAT_SCREEN_LINE_WIDTH + pos_x;
    for (y = 0; y < BAT_num_lives; y++, o -= 5)
    {
        TbPixel px;

        px = BAT_lives_colour;
        o[1] = px;
        o[2] = px;
        o[3] = px;
        o[4] = px;
    }
}

/** Character type table, indexed by char + 1 (exported by the asm). */
extern ubyte _IsTable[];

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (bat_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
struct BreakoutLevel BAT_levels[10] = {
    {
        .lv_name = "DESTROY THE BRICKS",
        .bricks = {
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
            0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
            0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "FORTIFICATION",
        .bricks = {
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x35, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
            0x07, 0x35, 0x35, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
            0x07, 0x07, 0x35, 0x35, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x16,
            0x16, 0x16, 0x16, 0x35, 0x35, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
            0x05, 0x05, 0x05, 0x05, 0x35, 0x35, 0x05, 0x05, 0x05, 0x05, 0x05,
            0x05, 0x05, 0x05, 0x05, 0x05, 0x35, 0x35, 0x16, 0x16, 0x16, 0x16,
            0x16, 0x16, 0x16, 0x16, 0x16, 0x16, 0x35, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "BULLFROG FOREVER",
        .bricks = {
            0x35, 0x00, 0x16, 0x02, 0x02, 0x16, 0x00, 0x16, 0x02, 0x02, 0x16,
            0x00, 0x35, 0x16, 0x00, 0x16, 0x02, 0x16, 0x00, 0x16, 0x02, 0x16,
            0x00, 0x16, 0x35, 0x02, 0x16, 0x00, 0x16, 0x16, 0x00, 0x16, 0x16,
            0x00, 0x16, 0x02, 0x35, 0x02, 0x02, 0x16, 0x00, 0x16, 0x00, 0x16,
            0x00, 0x16, 0x02, 0x02, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x35, 0x02, 0x02, 0x16, 0x00, 0x16,
            0x00, 0x16, 0x00, 0x16, 0x02, 0x02, 0x35, 0x02, 0x16, 0x00, 0x16,
            0x16, 0x00, 0x16, 0x16, 0x00, 0x16, 0x02, 0x35, 0x16, 0x00, 0x16,
            0x02, 0x16, 0x00, 0x16, 0x02, 0x16, 0x00, 0x16, 0x35, 0x00, 0x16,
            0x02, 0x02, 0x16, 0x00, 0x16, 0x02, 0x02, 0x16, 0x00, 0x35, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "BRICKY",
        .bricks = {
            0x16, 0x16, 0x16, 0x16, 0x80, 0x80, 0x80, 0x80, 0x16, 0x16, 0x16,
            0x16, 0x24, 0x00, 0x00, 0x24, 0x80, 0x80, 0x80, 0x80, 0x24, 0x05,
            0x05, 0x24, 0x35, 0x35, 0x35, 0x35, 0x80, 0x80, 0x80, 0x80, 0x35,
            0x35, 0x35, 0x35, 0x80, 0x80, 0x80, 0x80, 0x80, 0x16, 0x16, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x24, 0x03, 0x03,
            0x24, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x24,
            0x24, 0x80, 0x80, 0x80, 0x80, 0x80, 0x16, 0x16, 0x16, 0x16, 0x80,
            0x80, 0x80, 0x80, 0x16, 0x16, 0x16, 0x16, 0x24, 0x02, 0x02, 0x24,
            0x80, 0x80, 0x80, 0x80, 0x24, 0x07, 0x07, 0x24, 0x35, 0x35, 0x35,
            0x35, 0x80, 0x80, 0x80, 0x80, 0x35, 0x35, 0x35, 0x35, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "STIFF UPPER LIP",
        .bricks = {
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x24, 0x24, 0x80, 0x80, 0x24, 0x24, 0x80,
            0x80, 0x80, 0x80, 0x24, 0x24, 0x00, 0x00, 0x24, 0x24, 0x00, 0x00,
            0x24, 0x24, 0x80, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x24, 0x80, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x03, 0x80, 0x80, 0x80, 0x03, 0x03, 0x00, 0x00,
            0x00, 0x00, 0x03, 0x03, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x03,
            0x03, 0x03, 0x03, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "RAINDROPS KEEP...",
        .bricks = {
            0x80, 0x80, 0x35, 0x80, 0x80, 0x24, 0x24, 0x24, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x35, 0x35, 0x35, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
            0x80, 0x80, 0x80, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24,
            0x38, 0x38, 0x80, 0x24, 0x24, 0x24, 0x24, 0x24, 0x24, 0x38, 0x38,
            0x38, 0x38, 0x80, 0x80, 0x80, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
            0x38, 0x38, 0x80, 0x02, 0x80, 0x80, 0x02, 0x80, 0x02, 0x80, 0x35,
            0x35, 0x80, 0x80, 0x02, 0x80, 0x80, 0x02, 0x80, 0x02, 0x80, 0x02,
            0x80, 0x35, 0x35, 0x02, 0x80, 0x02, 0x80, 0x80, 0x02, 0x80, 0x02,
            0x80, 0x02, 0x80, 0x35, 0x80, 0x02, 0x80, 0x80, 0x02, 0x80, 0x02,
            0x80, 0x02, 0x80, 0x35, 0x80, 0x02, 0x80, 0x02, 0x80, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "DOWN THE SPOUT",
        .bricks = {
            0x35, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x35, 0x35, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x35, 0x35, 0x80, 0x35, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x35, 0x80, 0x35, 0x35, 0x05, 0x35, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x80, 0x35, 0x05, 0x35, 0x35, 0x05, 0x35, 0x80, 0x35, 0x80, 0x80,
            0x35, 0x80, 0x35, 0x05, 0x35, 0x35, 0x05, 0x35, 0x05, 0x35, 0x80,
            0x80, 0x35, 0x05, 0x35, 0x05, 0x35, 0x35, 0x01, 0x35, 0x05, 0x35,
            0x05, 0x05, 0x35, 0x05, 0x35, 0x01, 0x35, 0x35, 0x01, 0x35, 0x01,
            0x35, 0x05, 0x05, 0x35, 0x01, 0x35, 0x01, 0x35, 0x35, 0x01, 0x35,
            0x01, 0x35, 0x01, 0x01, 0x35, 0x01, 0x35, 0x01, 0x35, 0x35, 0x38,
            0x35, 0x38, 0x35, 0x01, 0x01, 0x35, 0x38, 0x35, 0x38, 0x35,
        },
    },
    {
        .lv_name = "UP THE TUBE",
        .bricks = {
            0x80, 0x80, 0x80, 0x80, 0x06, 0x80, 0x80, 0x06, 0x80, 0x80, 0x80,
            0x80, 0x02, 0x02, 0x02, 0x02, 0x24, 0x00, 0x00, 0x24, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x24, 0x00, 0x00, 0x24, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x38, 0x01, 0x01, 0x38,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x38, 0x01, 0x01,
            0x38, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x35, 0x01,
            0x01, 0x35, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x35,
            0x01, 0x01, 0x35, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
            0x35, 0x01, 0x01, 0x35, 0x02, 0x02, 0x02, 0x02, 0x35, 0x35, 0x35,
            0x35, 0x35, 0x01, 0x01, 0x35, 0x35, 0x35, 0x35, 0x35, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "EUROCORP",
        .bricks = {
            0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
            0x35, 0x35, 0x35, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
            0x02, 0x02, 0x35, 0x35, 0x02, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35,
            0x35, 0x35, 0x02, 0x35, 0x35, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x35, 0x35, 0x02, 0x35, 0x35, 0x35, 0x35,
            0x35, 0x02, 0x02, 0x02, 0x02, 0x35, 0x35, 0x02, 0x02, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x35, 0x35, 0x02, 0x35, 0x35,
            0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x02, 0x35, 0x35, 0x02, 0x02,
            0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x35, 0x35, 0x35,
            0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x24, 0x80, 0x80,
            0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        },
    },
    {
        .lv_name = "THE BULLFROG CHALLENGE",
        .bricks = {
            0x80, 0x80, 0x05, 0x80, 0x80, 0x05, 0x05, 0x80, 0x80, 0x05, 0x80,
            0x05, 0x80, 0x05, 0x05, 0x80, 0x05, 0x03, 0x03, 0x05, 0x80, 0x05,
            0x05, 0x80, 0x05, 0x05, 0x05, 0x80, 0x05, 0x03, 0x03, 0x05, 0x80,
            0x05, 0x05, 0x05, 0x80, 0x80, 0x05, 0x35, 0x03, 0x03, 0x03, 0x03,
            0x35, 0x05, 0x80, 0x80, 0x80, 0x80, 0x80, 0x35, 0x16, 0x16, 0x16,
            0x16, 0x35, 0x80, 0x80, 0x80, 0x80, 0x35, 0x80, 0x35, 0x16, 0x16,
            0x16, 0x16, 0x35, 0x80, 0x35, 0x80, 0x35, 0x35, 0x35, 0x35, 0x16,
            0x16, 0x16, 0x16, 0x35, 0x35, 0x35, 0x35, 0x80, 0x35, 0x35, 0x35,
            0x24, 0x24, 0x24, 0x24, 0x35, 0x35, 0x35, 0x80, 0x80, 0x80, 0x35,
            0x35, 0x35, 0x38, 0x38, 0x35, 0x35, 0x35, 0x80, 0x80, 0x35, 0x35,
            0x35, 0x35, 0x35, 0x05, 0x05, 0x35, 0x35, 0x35, 0x35, 0x35,
        },
    },
};
ubyte _IsTable[260] = {
    0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x0a, 0x0c, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
    0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48,
    0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x48, 0x0c, 0x0c, 0x0c, 0x0c,
    0x0c, 0x0c, 0x98, 0x98, 0x98, 0x98, 0x98, 0x98, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
    0x88, 0x88, 0x88, 0x88, 0x0c, 0x0c, 0x0c, 0x0c, 0x01,
};
int bat_unknvalA_min;
int bat_unknvalB_min;
int bat_unknvalA_max;
int bat_unknvalB_max;
ubyte *BAT_screen;
int BAT_data_1e26e8;
int BAT_game_won_timer;
int BAT_level_intro_timer;
int BAT_ball_lost_timer;
int BAT_game_over_timer;
int BAT_state;
int BAT_levelno;
int BAT_num_lives;
int BAT_score;
int BAT_bricks_remain;
ubyte BAT_bricks[BAT_BRICK_COLUMNS * BAT_BRICK_ROWS];
int BAT_paddle_x;
int BAT_data_1e2798;
TbPixel BAT_lives_colour;
struct BATBall BAT_balls_store[BAT_BALLS_LIMIT];
struct BATBall *BAT_free_balls;
struct BATBall *BAT_balls_head;
TbPixel BAT_ball_colour;
struct BATBonus BAT_bonus_store[BAT_BONUSES_LIMIT];
struct BATBonus *BAT_free_bonuses;
struct BATBonus *BAT_bonuses_head;
#endif /* typed asm data */

/* Minigame font glyphs, 5 rows of 3 pixels each; copies of the read-only
 * asm data at BAT_unkn1, BAT_unkn2 and data_155864..data_155873, which
 * are not exported from the asm.
 */
static const ubyte BAT_font_digits[10 * 5] = {
    0x07, 0x05, 0x05, 0x05, 0x07, 0x06, 0x02, 0x02,
    0x02, 0x07, 0x07, 0x01, 0x07, 0x04, 0x07, 0x07,
    0x01, 0x07, 0x01, 0x07, 0x05, 0x05, 0x07, 0x01,
    0x01, 0x07, 0x04, 0x07, 0x01, 0x07, 0x07, 0x04,
    0x07, 0x05, 0x07, 0x07, 0x01, 0x01, 0x01, 0x01,
    0x07, 0x05, 0x07, 0x05, 0x07, 0x07, 0x05, 0x07,
    0x01, 0x01,
};

static const ubyte BAT_font_letters[26 * 5] = {
    0x02, 0x05, 0x07, 0x05, 0x05, 0x06, 0x05, 0x06,
    0x05, 0x06, 0x03, 0x04, 0x04, 0x04, 0x03, 0x06,
    0x05, 0x05, 0x05, 0x06, 0x07, 0x04, 0x07, 0x04,
    0x07, 0x07, 0x04, 0x07, 0x04, 0x04, 0x03, 0x04,
    0x05, 0x05, 0x07, 0x05, 0x05, 0x07, 0x05, 0x05,
    0x07, 0x02, 0x02, 0x02, 0x07, 0x07, 0x01, 0x01,
    0x01, 0x06, 0x05, 0x05, 0x06, 0x05, 0x05, 0x04,
    0x04, 0x04, 0x04, 0x07, 0x05, 0x07, 0x05, 0x05,
    0x05, 0x05, 0x07, 0x07, 0x07, 0x05, 0x02, 0x05,
    0x05, 0x05, 0x02, 0x06, 0x05, 0x06, 0x04, 0x04,
    0x02, 0x05, 0x05, 0x05, 0x03, 0x06, 0x05, 0x06,
    0x05, 0x05, 0x03, 0x04, 0x02, 0x01, 0x06, 0x07,
    0x02, 0x02, 0x02, 0x02, 0x05, 0x05, 0x05, 0x05,
    0x07, 0x05, 0x05, 0x05, 0x05, 0x02, 0x05, 0x05,
    0x05, 0x07, 0x05, 0x05, 0x05, 0x02, 0x05, 0x05,
    0x05, 0x05, 0x02, 0x02, 0x02, 0x07, 0x01, 0x02,
    0x04, 0x07,
};

static const ubyte BAT_font_exclam[5] = {0x04, 0x04, 0x04, 0x00, 0x04};
static const ubyte BAT_font_period[5] = {0x00, 0x00, 0x00, 0x00, 0x04};
static const ubyte BAT_font_colon[5] = {0x00, 0x04, 0x00, 0x04, 0x00};
static const ubyte BAT_font_question[5] = {0x07, 0x01, 0x03, 0x00, 0x02};

static DIFFTEST_REF_FUNC void BAT_draw_char_asm(int x, int y, char chr, ubyte col)
{
    DIFFTEST_ASM(
      "call ASM_BAT_draw_char\n"
        : : "a" (x), "d" (y), "b" (chr), "c" (col));
}

void BAT_draw_char(int x, int y, char chr, ubyte col)
{
    const ubyte *p_glyph;
    const ubyte *p_glyph_end;
    ubyte *o;
    ubyte chtype;

    DIFFTEST_SHADOW(BAT_draw_char, BAT_draw_char_asm(x, y, chr, col));
    chtype = _IsTable[(ubyte)(chr + 1)];
    if ((chtype & 0x20) != 0)
        p_glyph = &BAT_font_digits[5 * ((s32)chr - '0')];
    else if ((chtype & 0x40) != 0)
        p_glyph = &BAT_font_letters[5 * ((s32)chr - 'A')];
    else if (chr == '!')
        p_glyph = BAT_font_exclam;
    else if (chr == '.')
        p_glyph = BAT_font_period;
    else if (chr == ':')
        p_glyph = BAT_font_colon;
    else if (chr == '?')
        p_glyph = BAT_font_question;
    else
        return;

    o = BAT_screen + x + (y << 8);
    p_glyph_end = p_glyph + 5;
    for (; p_glyph != p_glyph_end; p_glyph++, o += BAT_SCREEN_LINE_WIDTH)
    {
        if ((*p_glyph & 0x01) != 0)
            o[2] = col;
        if ((*p_glyph & 0x02) != 0)
            o[1] = col;
        if ((*p_glyph & 0x04) != 0)
            o[0] = col;
    }
}

static void BAT_print(short pos_x, short pos_y, const char *str, ubyte coll)
{
    char chr;
    ubyte colour;

    colour = colour_lookup[coll];
    chr = *str;
    while (chr != '\0')
    {
        if (chr != ' ')
            BAT_draw_char(pos_x, pos_y, chr, colour);
        chr = *++str;
        pos_x += 4;
    }
}

static void BAT_draw_score_and_level(void)
{
    char locstr[32];

    sprintf(locstr, "%d", BAT_score);
    BAT_print(1, 1, locstr, ColLU_WHITE);
    sprintf(locstr, "%d", BAT_levelno);
    BAT_print(BAT_SCREEN_WIDTH / 2 - 2 * strlen(locstr), 1, locstr, ColLU_WHITE);
}

static void BAT_draw_win_message(void)
{
    const char *str;

    str = "CONGRATULATIONS";
    BAT_print(18, 10, str, ColLU_YELLOW);
    str = "BONUS GAME COMPLETE";
    BAT_print(10, 16, str, ColLU_YELLOW);
    str = "BONUS ITEM";
    BAT_print(28, 30, str, ColLU_WHITE);
    str = "AWARDED";
    BAT_print(34, 38, str, ColLU_WHITE);
}

static void BAT_draw_lost_life_message(void)
{
    const char *str;

    str = "BAD LUCK!";
    BAT_print(30, 52, str, 8);
}

static void BAT_draw_game_over_message(void)
{
    const char *str;

    str = "GAME OVER";
    BAT_print(30, 52, str, 8);
}

static void BAT_draw_level_intro_text(void)
{
    char locstr[64];

    if (BAT_level_intro_timer < 60)
          sprintf(locstr, "%s", BAT_levels[BAT_levelno - 1].lv_name);
    else
          sprintf(locstr, "ENTERING LEVEL : %d", BAT_levelno);
    BAT_print(BAT_SCREEN_WIDTH / 2 - 2 * strlen(locstr) + 1, 52, locstr, 8);
}

static void BAT_input_paddle(void)
{
    int ms_x, scr_x;

    ms_x = lbDisplay.MMouseX;
    scr_x = lbDisplay.GraphicsScreenWidth;
    BAT_paddle_x = BAT_SCREEN_WIDTH / 2 + ((ms_x - scr_x/2) * BAT_SCREEN_WIDTH) / (scr_x / 2);

    if (BAT_paddle_x < BAT_data_1e2798 + 1)
        BAT_paddle_x = BAT_data_1e2798 + 1;
    if (BAT_paddle_x + BAT_data_1e2798 > 95)
        BAT_paddle_x = 95 - BAT_data_1e2798;
}

/** Inputs active only during a level play.
 */
static void BAT_input_level(void)
{
      if (is_key_pressed(KC_N, KMod_SHIFT))
      {
          BAT_bricks_remain = 0;
          BAT_num_lives++;
      }
}

static void BAT_bricks_set_rect(int beg_c, int beg_r, int end_c, int end_r, ubyte val)
{
    int c, r;
    for (r = beg_r; r < end_r; r++)
    {
        for (c = beg_c; c < end_c; c++)
            BAT_bricks[BAT_BRICK_COLUMNS * r + c] = val;
    }
}

static void BAT_bricks_clear(void)
{
    BAT_bricks_set_rect(0, 0, BAT_BRICK_COLUMNS, BAT_BRICK_ROWS, 0x80);
}

static DIFFTEST_REF_FUNC void BAT_unknsub_28_asm(int a1, int a2)
{
    int ra = a1, rd = a2;
    DIFFTEST_ASM("call ASM_BAT_unknsub_28\n"
        : "+a" (ra), "+d" (rd) : : "cc", "memory");
}

/** Routed asm (internal label). Maybe spawns a bonus at position
 * (pos_x, pos_y), which come from eax, edx.
 */
void BAT_unknsub_28(int pos_x, int pos_y)
{
    struct BATBonus *p_bonus;
    struct BATBonus *p_next;
    int kind;
    int rnd;

    DIFFTEST_SHADOW(BAT_unknsub_28, BAT_unknsub_28_asm(pos_x, pos_y));
    if ((game_rand() & 0xff) > 0x2a)
        return;
    rnd = game_rand() % 32;
    if (rnd < 7) {
        kind = 0;
    } else if ((rnd -= 7) < 7) {
        kind = 1;
    } else if ((rnd -= 7) < 6) {
        kind = 3;
    } else if ((rnd -= 6) < 5) {
        kind = 2;
    } else if ((rnd -= 5) < 4) {
        kind = 4;
    } else if ((rnd -= 4) < 2) {
        kind = 5;
    } else if ((rnd -= 2) < 1) {
        kind = 6;
    } else {
        return;
    }

    p_bonus = BAT_free_bonuses;
    if (p_bonus != NULL)
    {
        p_next = p_bonus->ListNext;
        BAT_free_bonuses = p_next;
        if (p_next != NULL)
            p_next->ListPvNxPtr = &BAT_free_bonuses;
        p_next = BAT_bonuses_head;
        if (p_next != NULL)
        {
            p_bonus->ListNext = p_next;
            p_next->ListPvNxPtr = &p_bonus->ListNext;
            BAT_bonuses_head = p_bonus;
            p_bonus->ListPvNxPtr = &BAT_bonuses_head;
        }
        else
        {
            p_bonus->ListPvNxPtr = &BAT_bonuses_head;
            p_bonus->ListNext = p_next;
            BAT_bonuses_head = p_bonus;
        }
    }
    if (p_bonus == NULL)
        return;
    p_bonus->Y = pos_x;
    p_bonus->UnkDw2 = pos_y;
    p_bonus->X = kind;
    p_bonus->UnkDw3 = (game_rand() & 0x3f) + 0x5f;
}

static DIFFTEST_REF_FUNC void BAT_unknsub_22_asm(void)
{
    DIFFTEST_ASM("call ASM_BAT_unknsub_22\n"
        :  :  : "eax" );
}

/** Speed change on brick hit; copy of asm data at data_155794.
 */
static const int BAT_ball_brick_speed_chg[4] = {-2, 0, 0, 2};

/** Approximate length of a vector, from absolute values of its components.
 */
static int BAT_approx_dist(int abs_a, int abs_b)
{
    int mx, mn;

    if (abs_b < abs_a) {
        mx = abs_a;
        mn = abs_b;
    } else {
        mx = abs_b;
        mn = abs_a;
    }
    return mx - (mx >> 5) - (mx >> 7) + (mn >> 2) + (mn >> 3) + (mn >> 6) + (mn >> 7);
}

static int BAT_ball_speed_limit(void)
{
    return (BAT_levelno * 6 + 30) << 10;
}

void BAT_unknsub_22(void)
{
    struct BATBall *p_ball;
    struct BATBall *p_next;
    int dist;

    DIFFTEST_SHADOW(BAT_unknsub_22, BAT_unknsub_22_asm());

    // Slow down horizontally, speed up vertically if too slow
    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        int dw7;

        p_ball->UnkDw6 = p_ball->UnkDw6 - (p_ball->UnkDw6 >> 9);
        dw7 = p_ball->UnkDw7;
        if (dw7 < 0) {
            p_ball->UnkDw7 = dw7 + (BAT_levelno * 32 + 0xbc);
            continue;
        }
        dist = BAT_approx_dist(abs(dw7), abs(p_ball->UnkDw6));
        if (dist < BAT_ball_speed_limit())
            p_ball->UnkDw7 += BAT_levelno * 32 + 0xbc;
    }

    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        p_ball->UnkDw2 = p_ball->X;
        p_ball->UnkDw3 = p_ball->Y;
        p_ball->NextX = p_ball->X + (p_ball->UnkDw6 >> 8);
        p_ball->NextY = p_ball->Y + (p_ball->UnkDw7 >> 8);
    }

    // Collisions with bricks
    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        int col, row, idx, hits;
        int left, right, top, bottom;

        col = p_ball->NextX / 2048;
        row = (p_ball->NextY - 0x700) / 1024;
        if (col < 0 || col >= BAT_BRICK_COLUMNS)
            continue;
        if (row < 0 || row >= BAT_BRICK_ROWS)
            continue;
        idx = BAT_BRICK_COLUMNS * row + col;
        if ((BAT_bricks[idx] & 0x80) != 0)
            continue;
        top = (row << 10) + 0x700;
        bottom = (row << 10) + 0xb00;
        left = col << 11;
        right = left + 0x800;
        if (left > p_ball->NextX || p_ball->NextX > right)
            continue;
        if (p_ball->NextY < top || p_ball->NextY > bottom)
            continue;

        BAT_score += 10;
        hits = (BAT_bricks[idx] >> 4) & 3;
        BAT_bricks[idx] |= 0x40;
        p_ball->UnkDw6 += BAT_ball_brick_speed_chg[game_rand() & 3] << 8;

        if (p_ball->UnkDw3 <= top)
        {
            p_ball->UnkDw7 = -p_ball->UnkDw7;
            if ((BAT_bricks[idx] & 7) == 5 && hits > 1)
            {
                int range;

                range = 2 * abs(p_ball->UnkDw7);
                if (range < 0xfff)
                    range = 0xfff;
                p_ball->UnkDw6 += ((game_rand() << 8) % range) - (range >> 1);
                dist = BAT_approx_dist(abs(p_ball->UnkDw7), abs(p_ball->UnkDw6));
                if (dist < BAT_ball_speed_limit())
                {
                    p_ball->UnkDw6 = p_ball->UnkDw6 + (p_ball->UnkDw6 >> 1);
                    p_ball->UnkDw7 = p_ball->UnkDw7 + (p_ball->UnkDw7 >> 1);
                }
            }
            else
            {
                p_ball->UnkDw6 = p_ball->UnkDw6 - (p_ball->UnkDw6 >> 3);
                p_ball->UnkDw7 = p_ball->UnkDw7 - (p_ball->UnkDw7 >> 3);
            }
        }
        if (bottom <= p_ball->UnkDw3)
        {
            p_ball->UnkDw7 = -p_ball->UnkDw7;
            p_ball->UnkDw6 = p_ball->UnkDw6 - (p_ball->UnkDw6 >> 2);
            p_ball->UnkDw7 = p_ball->UnkDw7 - (p_ball->UnkDw7 >> 2);
        }
        if (left >= p_ball->UnkDw2 || p_ball->UnkDw2 >= right)
        {
            p_ball->UnkDw6 = -p_ball->UnkDw6;
            p_ball->UnkDw6 = p_ball->UnkDw6 - (p_ball->UnkDw6 >> 2);
            p_ball->UnkDw7 = p_ball->UnkDw7 - (p_ball->UnkDw7 >> 2);
        }

        if (hits != 0)
        {
            if ((BAT_bricks[idx] & 7) == 5)
            {
                BAT_score -= 10;
                play_sample_using_heap(0, 0x4a, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
            }
            else
            {
                ubyte brick;

                hits--;
                brick = BAT_bricks[idx] & 0xcf;
                BAT_bricks[idx] = brick;
                BAT_bricks[idx] = brick | (ubyte)(hits << 4);
                play_sample_using_heap(0, 0x48, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
            }
        }
        else
        {
            BAT_bricks[idx] |= 0x80;
            BAT_bricks_remain--;
            BAT_score += 20;
            play_sample_using_heap(0, 0x4b, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
            BAT_unknsub_28((left + right) >> 1, (top + bottom) >> 1);
        }
    }

    // Collisions with screen edges; remove balls which fell out
    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_next)
    {
        p_next = p_ball->ListNext;
        if (p_ball->NextX < 0x100)
            p_ball->UnkDw6 = abs(p_ball->UnkDw6);
        if (p_ball->NextX > 0x5f00) {
            p_ball->UnkDw6 = abs(p_ball->UnkDw6);
            p_ball->UnkDw6 = -p_ball->UnkDw6;
        }
        if (p_ball->NextY < 0x100)
            p_ball->UnkDw7 = abs(p_ball->UnkDw7);
        if (p_ball->NextY <= 0x3e00)
            continue;

        *p_ball->ListPvNxPtr = p_ball->ListNext;
        if (p_ball->ListNext != NULL)
            p_ball->ListNext->ListPvNxPtr = p_ball->ListPvNxPtr;
        if (BAT_free_balls != NULL)
        {
            p_ball->ListNext = BAT_free_balls;
            BAT_free_balls->ListPvNxPtr = &p_ball->ListNext;
            BAT_free_balls = p_ball;
            p_ball->ListPvNxPtr = &BAT_free_balls;
        }
        else
        {
            // TODO BAT_free_balls is not updated here, so the ball is lost
            p_ball->ListPvNxPtr = &BAT_free_balls;
            p_ball->ListNext = NULL;
        }
    }

    // Collisions with the paddle
    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        int dist_lim, div;

        if (p_ball->NextY < 0x3c00)
            continue;
        if (((BAT_paddle_x - BAT_data_1e2798) << 8) - 0x100 > p_ball->NextX)
            continue;
        if (((BAT_paddle_x + BAT_data_1e2798) << 8) + 0x100 < p_ball->NextX)
            continue;

        dist = BAT_approx_dist(abs(p_ball->UnkDw7), abs(p_ball->UnkDw6));
        p_ball->UnkDw7 = abs(p_ball->UnkDw7);
        p_ball->UnkDw7 = -p_ball->UnkDw7;
        p_ball->UnkDw6 = ((p_ball->NextX - (BAT_paddle_x << 8)) * (-p_ball->UnkDw7))
          / (BAT_data_1e2798 << 8);
        p_ball->UnkDw6 = p_ball->UnkDw6 + ((game_rand() & 0xff) - 0x7f);

        dist_lim = dist + (dist >> 3);
        if (dist_lim > BAT_ball_speed_limit())
            dist_lim = BAT_ball_speed_limit();
        div = dist >> 6;
        dist_lim >>= 6;
        if (div == 0)
            div = 1;
        p_ball->UnkDw6 = (p_ball->UnkDw6 * dist_lim) / div;
        p_ball->UnkDw7 = (p_ball->UnkDw7 * dist_lim) / div;
        play_sample_using_heap(0, 0x4c, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
        p_ball->NextY = 0x3c00;
    }

    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        p_ball->X = p_ball->NextX;
        p_ball->Y = p_ball->NextY;
    }
}

static void BAT_ball_colour_fade(void)
{
    int fade_lv;

    if (BAT_level_intro_timer > 60)
    {
        fade_lv = (PALETTE_FADE_LEVELS / 2) - 2 * (90 - BAT_level_intro_timer);
        if (fade_lv < 0)
            fade_lv = 0;
        BAT_ball_colour = pixmap.fade_table[256 * fade_lv + colour_lookup[1]];
        BAT_unknsub_22();
        BAT_unknsub_22();
        BAT_unknsub_22();
        BAT_unknsub_22();
    }
    else
    {
        BAT_ball_colour = colour_lookup[1];
    }
}

static void BAT_reset_free_balls(void)
{
    int i;

    BAT_free_balls = &BAT_balls_store[0];
    BAT_balls_store[0].ListPvNxPtr = &BAT_free_balls;
    for (i = 0; i < BAT_BALLS_LIMIT - 1; i++)
    {
        BAT_balls_store[i].ListNext = &BAT_balls_store[i + 1];
        BAT_balls_store[i + 1].ListPvNxPtr = &BAT_balls_store[i].ListNext;
    }
    BAT_balls_store[i].ListNext = NULL;
    BAT_balls_head = NULL;
}

static void BAT_reset_free_bonuses(void)
{
    int i;

    BAT_free_bonuses = &BAT_bonus_store[0];
    BAT_bonus_store[0].ListPvNxPtr = &BAT_free_bonuses;
    for (i = 0; i < BAT_BONUSES_LIMIT - 1; i++)
    {
        BAT_bonus_store[i].ListNext = &BAT_bonus_store[i + 1];
        BAT_bonus_store[i + 1].ListPvNxPtr = &BAT_bonus_store[i].ListNext;
    }
    BAT_bonus_store[i].ListNext = 0;
    BAT_bonuses_head = NULL;
}

static void BAT_start_new_game(void)
{
    BAT_state = BATSt_LevelIntro;
    BAT_levelno = 1;
    BAT_score = 0;
    BAT_num_lives = 2;
    BAT_level_intro_timer = 90;
}

static struct BATBall *new_ball(void)
{
    struct BATBall *curr;
    struct BATBall *next;

    curr = BAT_free_balls;
    if (curr == NULL) {
        LOGERR("BAT: no more free balls");
        return NULL;
    }

    next = curr->ListNext;
    BAT_free_balls = next;
    if (next != NULL)
        next->ListPvNxPtr = &BAT_free_balls;

    next = BAT_balls_head;
    if (next != NULL) {
          curr->ListNext = next;
          next->ListPvNxPtr = &curr->ListNext;
    } else {
          curr->ListNext = NULL;
    }
    curr->ListPvNxPtr = &BAT_balls_head;
    BAT_balls_head = curr;

    return curr;
}

static void BAT_create_starting_ball(void)
{
    struct BATBall *ball;

    ball = new_ball();

    if (ball != NULL)
    {
        ushort rnd;

        ball->X = (BAT_SCREEN_WIDTH / 2) << 8;
        ball->Y = (49) << 8;
        rnd = game_rand();
        ball->UnkDw7 = 0;
        ball->UnkDw6 = ((rnd & 7) << 8) - 768;
    }
}

int BAT_unknsub_27(void)
{
    int tile_x, tile_z;

    for (tile_x = bat_unknvalA_min; tile_x <= bat_unknvalA_max; tile_x++)
    {
        for (tile_z = bat_unknvalB_min; tile_z <= bat_unknvalB_max; tile_z++)
        {
            struct MyMapElement *p_mapel;
            short thing;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            thing = p_mapel->Child;

            while (thing != 0)
            {
                if (thing <= 0)
                {
                    struct SimpleThing *p_sthing;
                    p_sthing = &sthings[thing];
                    thing = p_sthing->Next;
                }
                else
                {
                    struct Thing *p_thing;
                    p_thing = &things[thing];
                    thing = p_thing->Next;

                    if ((p_thing->Type == TT_PERSON)
                      && ((p_thing->Flag & TngF_PlayerAgent) != 0)
                      && ((p_thing->Flag & TngF_Destroyed) == 0)
                      && (p_thing->U.UPerson.CurrentWeapon == WEP_PERSUADRTRN))
                    {
                        ingame.Flags |= GamF_BillboardBAT;
                        return 1;
                    }
                }
            }
        }
    }
    ingame.Flags &= ~GamF_BillboardBAT;
    return 0;
}

static DIFFTEST_REF_FUNC void BAT_unknsub_21_asm(void)
{
    DIFFTEST_ASM("call ASM_BAT_unknsub_21\n"
        :  :  : "eax" );
}

void BAT_unknsub_21(void)
{
    ubyte *p_brick_colours;
    ubyte *o;
    ubyte *p_row;
    int row, i, end_i, x, y;
    int k, n;
    u32 shade;
    ubyte brick, colour, light, dark, mid;
    struct BATBall *p_ball;
    TbPixel bcol, bdark;

    DIFFTEST_SHADOW(BAT_unknsub_21, BAT_unknsub_21_asm());
    // The brick colours table (data_1e2710) is not exported from the asm;
    // it directly follows the 4-byte BAT_bricks_remain.
    p_brick_colours = (ubyte *)&BAT_bricks_remain + 4;

    // Draw the bricks
    y = 7;
    end_i = BAT_BRICK_COLUMNS;
    for (row = 0; row < BAT_BRICK_ROWS; row++)
    {
        x = 0;
        for (i = BAT_BRICK_COLUMNS * row; i != end_i; i++, x += 8)
        {
            brick = BAT_bricks[i];
            if ((brick & 0x80) != 0)
                continue;
            shade = ((u32)brick >> 4) & 3;
            colour = p_brick_colours[brick & 0x0F];
            if ((brick & 0x40) != 0)
            {
                colour = colour_lookup[1];
                BAT_bricks[i] = brick & 0xBF;
            }
            if (shade >= 1 && shade <= 3)
            {
                light = pixmap.ghost_table[colour];
                dark = pixmap.fade_table[24 * PALETTE_8b_COLORS + colour];
                p_row = BAT_screen + x + (y << 8);
                mid = light;
                for (k = 0; k < 3; k++)
                {
                    p_row[0] = light;
                    for (n = 1; n < 6; n++)
                        p_row[n] = mid;
                    mid = colour;
                    p_row[6] = dark;
                    p_row += BAT_SCREEN_LINE_WIDTH;
                }
            }
            else
            {
                p_row = BAT_screen + x + (y << 8);
                for (k = 0; k < 3; k++)
                {
                    for (n = 0; n < 7; n++)
                        p_row[n] = colour;
                    p_row += BAT_SCREEN_LINE_WIDTH;
                }
            }
        }
        end_i += BAT_BRICK_COLUMNS;
        y += 4;
    }

    // Draw the balls
    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        o = BAT_screen + (p_ball->X >> 8) + ((p_ball->Y >> 8) << 8);
        bcol = BAT_ball_colour;
        bdark = pixmap.fade_table[20 * PALETTE_8b_COLORS + bcol];
        o[-0x101] = bdark;
        o[-0x100] = bcol;
        o[-0xFF] = bdark;
        o[-0x1] = bcol;
        o[0] = bcol;
        o[0x1] = bcol;
        o[0xFF] = bdark;
        o[0x100] = bcol;
        o[0x101] = bdark;
    }

    // Draw the paddle
    o = BAT_screen + BAT_paddle_x - BAT_data_1e2798 + 0x3D00;
    bcol = BAT_lives_colour;
    bdark = pixmap.fade_table[20 * PALETTE_8b_COLORS + bcol];
    for (k = 0; k < 2 * BAT_data_1e2798; k++)
        o[k] = bcol;
    for (k = 0; k < 2 * BAT_data_1e2798; k++)
        o[BAT_SCREEN_LINE_WIDTH + k] = bdark;
}

static DIFFTEST_REF_FUNC void breakout_play_sub1_asm(void)
{
    DIFFTEST_ASM("call ASM_breakout_play_sub1\n"
        :  :  : "eax" );
}

/** Moves a bonus from the active list to the free list.
 */
static void BAT_bonus_free(struct BATBonus *p_bonus)
{
    struct BATBonus *p_next;
    struct BATBonus *p_free;

    p_next = p_bonus->ListNext;
    *p_bonus->ListPvNxPtr = p_next;
    if (p_next != NULL)
        p_next->ListPvNxPtr = p_bonus->ListPvNxPtr;
    p_free = BAT_free_bonuses;
    if (p_free != NULL)
    {
        p_bonus->ListNext = p_free;
        p_free->ListPvNxPtr = &p_bonus->ListNext;
        BAT_free_bonuses = p_bonus;
        p_bonus->ListPvNxPtr = &BAT_free_bonuses;
    }
    else
    {
        // TODO the original does not set BAT_free_bonuses here, so the bonus is lost
        p_bonus->ListPvNxPtr = &BAT_free_bonuses;
        p_bonus->ListNext = p_free;
    }
}

/** Duplicates every existing ball, with randomized speed.
 */
static void BAT_balls_duplicate(void)
{
    struct BATBall *p_ball;
    struct BATBall *p_nball;

    for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
    {
        p_nball = new_ball();
        if (p_nball == NULL)
            break;
        p_nball->X = p_ball->X;
        p_nball->Y = p_ball->Y;
        p_nball->UnkDw6 = (game_rand() & 0xFFFF) + p_ball->UnkDw6 - 0x7FFF;
        p_nball->UnkDw7 = (game_rand() & 0xFFFF) + p_ball->UnkDw7 - 0x7FFF;
    }
}

/** Moves the falling bonuses, applies the ones caught by the paddle,
 * and updates the paddle half-width.
 */
void breakout_play_sub1(void)
{
    // Three asm-local dwords (data_1e2cac, data_1e2cb0, data_1e2cb4)
    // directly follow the 4-byte BAT_ball_colour.
    s32 *p_paddle_timer = (s32 *)((ubyte *)&BAT_ball_colour + 4);
    s32 *p_paddle_width = (s32 *)((ubyte *)&BAT_ball_colour + 8);
    s32 *p_paddle_target = (s32 *)((ubyte *)&BAT_ball_colour + 12);
    struct BATBonus *p_bonus;
    struct BATBonus *p_next;
    struct BATBall *p_ball;
    s32 w, d;

    DIFFTEST_SHADOW(breakout_play_sub1, breakout_play_sub1_asm());
    for (p_bonus = BAT_bonuses_head; p_bonus != NULL; p_bonus = p_next)
    {
        s32 y;

        y = p_bonus->UnkDw2 + p_bonus->UnkDw3;
        p_next = p_bonus->ListNext;
        p_bonus->UnkDw2 = y;
        if (y < 0x3B00)
            continue;
        if ((((BAT_paddle_x - BAT_data_1e2798) << 8) - 0x300 <= p_bonus->Y) &&
          (((BAT_paddle_x + BAT_data_1e2798) << 8) + 0x300 >= p_bonus->Y))
        {
            switch ((u32)p_bonus->X)
            {
            case 0:
                *p_paddle_target = 0xB00;
                *p_paddle_timer = 0xA4;
                break;
            case 1:
                *p_paddle_target = 0x400;
                *p_paddle_timer = 0xA4;
                break;
            case 2:
                for (p_ball = BAT_balls_head; p_ball != NULL; p_ball = p_ball->ListNext)
                {
                    p_ball->UnkDw6 >>= 2;
                    p_ball->UnkDw7 >>= 2;
                }
                break;
            case 3:
                BAT_balls_duplicate();
                break;
            case 4:
                BAT_balls_duplicate();
                BAT_balls_duplicate();
                break;
            case 5:
                BAT_num_lives++;
                break;
            case 6:
                BAT_bricks_remain = 0;
                break;
            default:
                break;
            }
            BAT_bonus_free(p_bonus);
            continue;
        }
        if (p_bonus->UnkDw2 < 0x3E00)
            continue;
        BAT_bonus_free(p_bonus);
    }

    if (*p_paddle_timer == 0)
        *p_paddle_target = 0x600;
    else
        *p_paddle_timer = *p_paddle_timer - 1;

    w = *p_paddle_width + ((*p_paddle_target - *p_paddle_width) >> 3);
    *p_paddle_width = w;
    d = w - *p_paddle_target;
    if (d < 0)
        d = -d;
    if (d < 8)
        *p_paddle_width = *p_paddle_target;
    BAT_data_1e2798 = *p_paddle_width >> 8;
}

static DIFFTEST_REF_FUNC void breakout_play_sub2_asm(void)
{
    DIFFTEST_ASM("call ASM_breakout_play_sub2\n"
        :  :  : "eax" );
}

/** Draws the falling bonuses; X is the bonus kind.
 */
void breakout_play_sub2(void)
{
    static const char bonus_chr[] = {'W', 'T', 'S', 'D', 'Q', 'X', 'N'};
    static const ubyte bonus_col[] = {4, 4, 3, 5, 5, 2, 1};
    struct BATBonus *p_bonus;

    DIFFTEST_SHADOW(breakout_play_sub2, breakout_play_sub2_asm());
    for (p_bonus = BAT_bonuses_head; p_bonus != NULL;
      p_bonus = p_bonus->ListNext)
    {
        u32 kind;
        ubyte colour;
        int rnd;

        kind = p_bonus->X;
        if (kind > 6)
            continue;
        rnd = game_rand() & 0x3f;
        colour = pixmap.fade_table[(rnd << 8) + colour_lookup[bonus_col[kind]]];
        BAT_draw_char((p_bonus->Y - 0x100) >> 8, (p_bonus->UnkDw2 - 0x200) >> 8,
          bonus_chr[kind], colour);
        if (kind == 5)
            play_sample_using_heap(0, 71, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
    }
}

static DIFFTEST_REF_FUNC void BAT_bricks_load_asm(int a1)
{
    int ra = a1;
    DIFFTEST_ASM("call ASM_BAT_bricks_load\n"
        : "+a" (ra) : : "cc", "memory");
}

/** Loads the bricks of level `level_no` (from eax), counting from 1.
 */
void BAT_bricks_load(int level_no)
{
    struct BreakoutLevel *p_level;
    int remain;
    int r, c;

    DIFFTEST_SHADOW(BAT_bricks_load, BAT_bricks_load_asm(level_no));
    p_level = &BAT_levels[level_no - 1];
    remain = 0;
    for (r = 0; r < BAT_BRICK_ROWS; r++)
    {
        for (c = 0; c < BAT_BRICK_COLUMNS; c++)
        {
            ubyte brick;

            brick = p_level->bricks[BAT_BRICK_COLUMNS * r + c];
            BAT_bricks[BAT_BRICK_COLUMNS * r + c] = brick;
            if ((brick & 0x80) == 0 && brick != 0x35)
                remain++;
        }
    }
    BAT_bricks_remain = remain;
}

void BAT_play(void)
{
    switch (BAT_state)
    {
    case BATSt_Reset:
        if (BAT_unknsub_27())
        {
            BAT_start_new_game();
            BAT_bricks_clear();
        }
        return;

    case BATSt_LevelIntro:
        BAT_screen_clear();
        BAT_level_intro_timer--;
        BAT_input_paddle();

        BAT_ball_colour_fade();
        breakout_play_sub1();
        BAT_draw_score_and_level();
        BAT_draw_remainig_lives(90, 2);
        BAT_unknsub_21();
        breakout_play_sub2();
        BAT_draw_level_intro_text();

        if (BAT_level_intro_timer == 60)
        {
            BAT_bricks_load(BAT_levelno);
            BAT_reset_free_balls();
            BAT_create_starting_ball();
        }
        if (BAT_level_intro_timer == 0)
        {
            BAT_state = BATSt_Gameplay;
        }
        break;

    case BATSt_Gameplay:
        BAT_screen_clear();
        BAT_input_paddle();

        BAT_unknsub_22();
        BAT_unknsub_22();
        BAT_unknsub_22();
        BAT_unknsub_22();
        breakout_play_sub1();
        BAT_draw_score_and_level();
        BAT_draw_remainig_lives(90, 2);
        BAT_unknsub_21();
        breakout_play_sub2();
        if (BAT_balls_head == NULL)
        {
            BAT_state = BATSt_BallLost;
            BAT_ball_lost_timer = 80;
            play_sample_using_heap(0, 73, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
        }
        BAT_input_level();
        if (BAT_bricks_remain == 0)
        {
            if (BAT_levelno == 10)
            {
              BAT_state = BATSt_GameWon;
              BAT_game_won_timer = 150;
              game_rand();
              BAT_data_1e26e8 = 0;
            }
            else
            {
              BAT_state = BATSt_LevelIntro;
              BAT_level_intro_timer = 90;
            }
            BAT_bricks_clear();
            BAT_levelno++;
            play_sample_using_heap(0, 70, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
        }
        break;

    case BATSt_BallLost:
      BAT_screen_clear();
      BAT_ball_lost_timer--;
      BAT_input_paddle();

      breakout_play_sub1();
      BAT_draw_score_and_level();
      BAT_draw_remainig_lives(90, 2);
      BAT_unknsub_21();
      breakout_play_sub2();
      BAT_draw_lost_life_message();

      if (BAT_ball_lost_timer == 50)
      {
          if (BAT_num_lives) {
            BAT_num_lives--;
          } else {
            BAT_state = BATSt_GameOver;
            BAT_game_over_timer = 100;
          }
      }
      if (BAT_ball_lost_timer == 40)
      {
          BAT_create_starting_ball();
      }
      if (BAT_ball_lost_timer == 0)
      {
          BAT_state = BATSt_Gameplay;
      }
      break;

    case BATSt_GameOver:
      BAT_screen_clear();
      BAT_game_over_timer--;
      BAT_input_paddle();

      breakout_play_sub1();
      BAT_draw_score_and_level();
      BAT_draw_remainig_lives(90, 2);
      breakout_play_sub2();
      BAT_unknsub_21();
      BAT_draw_game_over_message();

      if (BAT_game_over_timer == 0)
      {
          BAT_bricks_clear();
          BAT_start_new_game();
      }
      break;

    case BATSt_GameWon:
      BAT_screen_clear();
      BAT_game_won_timer--;

      BAT_draw_win_message();
      player_agents_add_random_epidermises(&players[local_player_no]);

      if (BAT_game_won_timer == 0)
      {
          BAT_bricks_clear();
          BAT_reset_free_balls();
          BAT_start_new_game();
          ingame.UserFlags |= UsrF_WonBAT;
      }
      break;

    default:
      break;
    }
    if (!BAT_unknsub_27())
    {
        BAT_state = BATSt_Reset;
        BAT_bricks_clear();
        BAT_screen_clear();
        BAT_reset_free_balls();
        BAT_reset_free_bonuses();
    }
}

static DIFFTEST_REF_FUNC void BAT_unknsub_20_asm(int a1, int a2, int a3, int a4, ubyte *p_screen)
{
    DIFFTEST_ASM(
      "push %4\n"
      "call ASM_BAT_unknsub_20\n"
        : : "a" (a1), "d" (a2), "b" (a3), "c" (a4), "g" (p_screen));
}

/** Sets the map tile range (x from eax to ebx, z from edx to ecx) and the
 * colours, and resets the bricks, balls and bonuses.
 */
void BAT_unknsub_20(int tile_x_min, int tile_z_min, int tile_x_max, int tile_z_max,
  ubyte *p_screen)
{
    ubyte *p_brick_colours;
    int i;

    DIFFTEST_SHADOW(BAT_unknsub_20, BAT_unknsub_20_asm(tile_x_min, tile_z_min,
      tile_x_max, tile_z_max, p_screen));
    // The brick colours table (data_1e2710, 9 bytes) is not exported from
    // the asm; it directly follows the 4-byte BAT_bricks_remain.
    p_brick_colours = (ubyte *)&BAT_bricks_remain + 4;

    bat_unknvalA_min = tile_x_min;
    p_brick_colours[0] = colour_lookup[2];
    p_brick_colours[1] = colour_lookup[3];
    p_brick_colours[2] = colour_lookup[4];
    p_brick_colours[3] = colour_lookup[1];
    p_brick_colours[4] = colour_lookup[9];
    p_brick_colours[5] = colour_lookup[5];
    bat_unknvalB_min = tile_z_min;
    p_brick_colours[6] = colour_lookup[8];
    bat_unknvalA_max = tile_x_max;
    p_brick_colours[7] = colour_lookup[7];
    bat_unknvalB_max = tile_z_max;
    p_brick_colours[8] = colour_lookup[10];

    for (i = 0; i < 8; i++)
        p_brick_colours[i] = pixmap.fade_table[32 * PALETTE_8b_COLORS + p_brick_colours[i]];
    p_brick_colours[8] = pixmap.fade_table[48 * PALETTE_8b_COLORS + p_brick_colours[8]];

    BAT_lives_colour = colour_lookup[1];
    BAT_bricks_clear();
    BAT_reset_free_balls();
    BAT_reset_free_bonuses();
    // TODO the asm ignores p_screen argument and uses a fixed position instead
    BAT_screen = vec_tmap[4] + 0xA040;
    BAT_state = BATSt_Reset;
}

/******************************************************************************/
