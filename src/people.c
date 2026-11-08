/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file people.c
 *     Routines implementing people.
 * @par Purpose:
 *     Implement general functions for handling person things.
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
#include "people.h"

#include "bfmath.h"
#include "bfmemory.h"
#include "bfpalette.h"
#include "bffile.h"
#include "bfini.h"
#include "bfutility.h"
#include "ssampply.h"
#include <assert.h>
#include <stdlib.h>

#include "engincam.h"
#include "engincolour.h"
#include "enginlights.h"
#include "drawtext.h"
#include "huddrwlstm.h"

#include "bmbang.h"
#include "bigmap.h"
#include "building.h"
#include "command.h"
#include "difftest.h"
#include "drawshape.h"
#include "drawtext_wrp.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "engintrns.h"
#include "febrief.h"
#include "frame_sprani.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_speed.h"
#include "hud_draw.h"
#include "lvdraw3d.h"
#include "lvobjctv.h"
#include "misstat.h"
#include "pathtrig.h"
#include "pepgroup.h"
#include "player.h"
#include "scandraw.h"
#include "sound.h"
#include "thing.h"
#include "thing_fire.h"
#include "thing_onface.h"
#include "thing_search.h"
#include "tngcolisn.h"
#include "vehicle.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/
#define ALERT_PEEPS_RANGE 4

struct PeepStat peep_type_stats[] = {
    {   0,    0,    0,    0,   0, 0,   0, 0, 0, 0},
    {1000, 1024, 2048, 1024,  20, 5, 512, 0, 0, 0},
    {1000, 1024, 2048, 1024,  20, 1, 512, 0, 0, 0},
    { 600,  524, 2048,  601,  10, 2, 312, 0, 0, 0},
    { 100,  100,  400,  300,   0, 1, 172, 0, 0, 0},
    { 100,  100,  400,  300,   0, 1, 160, 0, 0, 0},
    { 600,  600,  900,  601,  15, 3, 312, 0, 0, 0},
    {4000, 1000,  800, 2001,9999, 2, 362, 0, 0, 0},
    { 600,  400,  800,  601,   6, 2, 362, 0, 0, 0},
    { 600,  524, 2024,  601,  10, 2, 312, 0, 0, 0},
    { 200,  100,  300,  200,   0, 1, 212, 0, 0, 0},
    { 600,  524, 2024,  601,   0, 1, 312, 0, 0, 0},
    {2000, 1024, 2048, 1024,  20, 5, 512, 0, 0, 0},
    { 100,  100,  400,  300,   0, 1, 150, 0, 0, 0},
    { 110,  100,  400,  300,   0, 1, 212, 0, 0, 0},
    { 100,  100,  400,  300,   0, 1, 212, 0, 0, 0},
    {   0,    0,    0,    0,   0, 0,   0, 0, 0, 0},
};

ushort people_frames[SubTT_PERS_COUNT][ANIM_PERS_TOTAL_COUNT] = {
  {   1,   33,   33,    1,   40,   40,   33,   33,   33,   33,   73,   65,    9,   41,   25,   25,   57, 1010, 1018,   17, 1200,   49,},
  {   1,   33,   33,    1,   40,   40,   33,   33,   33,   33,   73,   65,    9,   41,   25,   25,   57, 1010, 1018,   17, 1200,   49,},
  { 113,  113,  113,   81,  113,  113,  113,  113,  113,  113,  153,  145,   89,  121,  105,  105,  137, 1010, 1018,   97, 1212,  129,},
  { 241,  273,  273,    1,   40,   40,  273,  273,  273,  273,  313,  305,  249,  281,  265,  265,  297, 1010, 1018,  241, 1236,  289,},
  { 321,  353,  353,  321,  353,  353,  353,  353,  353,  353,  393,  385,  329,  361,  345,  345,  377, 1010, 1018,  337, 1248,  369,},
  { 401,  433,  433,    1,  433,  433,  433,  433,  433,  433,  473,  465,  409,  441,  425,  425,  457, 1010, 1018,  401, 1260,  449,},
  { 561,  593,  593,  561,  593,  593,  593,  593,  593,  593,  633,  625,  569,  601,  593,  593,  617, 1010, 1018,  577, 1415,  609,},
  {1040, 1040, 1040,    1,   40,   40, 1040, 1040, 1040, 1040,  313,  313, 1018, 1040, 1040, 1040, 1040, 1010, 1018, 1040, 1040, 1040,},
  { 481,  513,  513,  513,  513,  513,  513,  513,  513,  513,  553,  545,  489,  521,  505,  505,  537, 1010, 1018,  497, 1272,  529,},
  { 161,  193,  193,  161,  193,  193,  193,  193,  193,  193,  233,  225,  169,  201,  185,  185,  217, 1010, 1018,  177, 1224,  209,},
  { 641,  673,  673,  673,  673,  673,  673,  673,  673,  673,  713,  705,  649,  665,  689,  689,  697, 1010, 1018,  657, 1295,  689,},
  { 721,  753,  753,  753,  753,  753,  753,  753,  753,  753,  713,  785,  729,  761,  753,  753,  777, 1010, 1018,  737, 1307,  769,},
  { 801,  833,  833,  833,  833,  833,  833,  833,  833,  833,  873,  865,  809,  841,  825,  825,  857, 1010, 1018,  817, 1319,  825,},
  {1331, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1395, 1387, 1339, 1371, 1355, 1355, 1379, 1010, 1018, 1331, 1403, 1347,},
  {1427, 1459, 1459, 1459, 1459, 1459, 1459, 1459, 1459, 1459, 1491, 1483, 1435, 1467, 1451, 1451, 1475, 1010, 1018, 1427, 1499, 1443,},
  {1331, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1363, 1395, 1387, 1339, 1371, 1355, 1355, 1379, 1010, 1018, 1331, 1403, 1331,},
};

/** Configuration options for each person state.
 */
struct PeepStateConfig person_states[] = {
  {"NONE_STATE",},
  {"GOTO_POINT",},
  {"BOMB_BUILDING",},
  {"WANDER",},
  {"EXPLODE_EFFECT",},
  {"WAIT",},
  {"AGENT_DEFENSIVE",},
  {"PICKUP_ITEM",},
  {"DROP_ITEM",},
  {"SHOOT_PERS_WT_BG",},
  {"SHOOT_BGUN_AT",},
  {"DIE",},
  {"DIEING",},
  {"DEAD",},
  {"INIT_SAVE_VICTIM",},
  {"SAVE_VICTIM",},
  {"GOTO_PERSON",},
  {"WAIT_P_DEAD",},
  {"WAIT_ALL_G_DEAD",},
  {"WAIT_MEM_G_DEAD",},
  {"WAIT_P_NEAR",},
  {"WAIT_MEM_G_NEAR",},
  {"WAIT_P_ARRIVES",},
  {"WAIT_MEM_G_ARRIV",},
  {"WAIT_TIME",},
  {"DEFINE_ROUTE",},
  {"STATE_UNUSED_1A",},
  {"KILL_PERSON",},
  {"BLOCK_PERSON",},
  {"SCARE_PERSON",},
  {"USE_ITEM",},
  {"STATIONARY_SHOT",},
  {"RECOIL_HIT",},
  {"VEHICLE_PARKED",},
  {"VEHICLE_DRIVEN",},
  {"GOTO_POINT_REL",},
  {"PERSON_BURNING",},
  {"IN_VEHICLE",},
  {"DRIVING_VEHICLE",},
  {"V_GOTO_POINT_FLY",},
  {"PERSUADE_PERSON",},
  {"FOLLOW_PERSON",},
  {"SUPPORT_PERSON",},
  {"PROTECT_PERSON",},
  {"GET_ITEM",},
  {"USE_VEHICLE",},
  {"USE_VEH_TO_POINT",},
  {"WAIT_VEHICLE",},
  {"GOTO_VEHICLE",},
  {"CATCH_TRAIN",},
  {"WAIT_TRAIN",},
  {"DESTROY_BUILDING",},
  {"WANDER_DRIVE",},
  {"GO_PLANT_MINE",},
  {"WAIT_TO_EXIT_VEH",},
  {"CATCH_FERRY",},
  {"EXIT_FERRY",},
  {"AVOID_GROUP",},
  {"INIT_COMMAND",},
  {"BEING_PERSUADED",},
};

struct PeepStatAdd peep_type_stats_a[SubTT_PERS_COUNT] = {0};
struct TbNamedEnum peep_names[SubTT_PERS_COUNT] = {0};

enum PeepTypesConfigCmd {
    PpTyp_PersonTypesCount = 1,
    PpTyp_Name,
    PpTyp_MaxHealth,
    PpTyp_MaxShield,
    PpTyp_MaximumStamina,
    PpTyp_MaxEnergy,
    PpTyp_PersuadeReqd,
    PpTyp_PersuadeWorth,
    PpTyp_Speed,

};

const struct TbNamedEnum people_conf_common_cmds[] = {
  {"PersonTypesCount",	PpTyp_PersonTypesCount},
  {NULL,		0},
};

const struct TbNamedEnum people_conf_person_cmds[] = {
  {"Name",			PpTyp_Name},
  {"MaxHealth",		PpTyp_MaxHealth},
  {"MaxShield",		PpTyp_MaxShield},
  {"MaximumStamina",PpTyp_MaximumStamina},
  {"MaxEnergy",		PpTyp_MaxEnergy},
  {"PersuadeRequired",PpTyp_PersuadeReqd},
  {"PersuadeWorth",	PpTyp_PersuadeWorth},
  {"Speed",			PpTyp_Speed},
  {NULL,			0},
};

const ubyte follow_dist[4][4] = {
  { 0, 3, 6, 9 },
  { 3, 0, 6, 9 },
  { 3, 6, 0, 9 },
  { 3, 6, 9, 0 },
};

ubyte sfx_man_shot[] = {
  11, 26, 41, 42, 43, 11, 26, 41,
};
ubyte sfx_woman_shot[] = {
  12, 27,
};

ushort female_peep = 0x2028;

struct MyPath my_paths[1600];
ushort path_count = 0;
ushort head_my_path = 1;
ushort word_1531DA = 1;

ushort word_17FA58[400];

sbyte burning_timer_delatas[] = {
  0, 0, 1, 0, 1, 0, 0, 0,
};
extern short word_1AA38E;
extern short word_1AA390;
extern short word_1AA392;
extern short word_1AA394;
extern ushort word_1AA678;

short unkn01_thing_idx = 0;

void read_people_conf_file(void)
{
    char conf_fname[DISKPATH_SIZE];
    PathInfo *pinfo;
    char *conf_buf;
    TbFileHandle conf_fh;
    TbBool done;
    int i;
    long k;
    int cmd_num;
    struct TbIniParser parser;
    int conf_len;
    int peep_count, ptype;

    pinfo = &game_dirs[DirPlace_Config];
    snprintf(conf_fname, DISKPATH_SIZE-1, "%s/people.ini", pinfo->directory);
    conf_fh = LbFileOpen(conf_fname, Lb_FILE_MODE_READ_ONLY);
    if (conf_fh != INVALID_FILE) {
        conf_len = LbFileLengthHandle(conf_fh);
        if (conf_len > 1024*1024)
            conf_len = 1024*1024;
        conf_buf = LbMemoryAlloc(conf_len+16);
        conf_len = LbFileRead(conf_fh, conf_buf, conf_len);
        LOGSYNC("Processing '%s' file, %d bytes", conf_fname, conf_len);
        LbFileClose(conf_fh);
    } else {
        LOGERR("Could not open people types config file, going with defaults.");
        conf_buf = LbMemoryAlloc(16);
        conf_len = 0;
    }
    conf_buf[conf_len] = '\0';
    LbIniParseStart(&parser, conf_buf, conf_len);
#define CONFWRNLOG(format,args...) LOGWARN("%s(line %lu): " format, conf_fname, parser.line_num, ## args)
#define CONFDBGLOG(format,args...) LOGDBG("%s(line %lu): " format, conf_fname, parser.line_num, ## args)
    peep_count = 0;
    // Parse the [common] section of loaded file
    if (LbIniFindSection(&parser, "common") != Lb_SUCCESS) {
        CONFWRNLOG("Could not find \"[%s]\" section, file skipped.", "common");
        LbIniParseEnd(&parser);
        LbMemoryFree(conf_buf);
        return;
    }
    done = false;
#define COMMAND_TEXT(cmd_num) LbNamedEnumGetName(people_conf_common_cmds,cmd_num)
    while (!done)
    {
        // Finding command number in this line
        i = 0;
        cmd_num = LbIniRecognizeKey(&parser, people_conf_common_cmds);
        // Now store the config item in correct place
        switch (cmd_num)
        {
        case PpTyp_PersonTypesCount:
            i = LbIniValueGetLongInt(&parser, &k);
            if (i <= 0) {
                CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                break;
            }
            peep_count = k;
            CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), peep_count);
            break;
        case 0: // comment
            break;
        case -1: // end of buffer
        case -3: // end of section
            done = true;
            break;
        default:
            CONFWRNLOG("Unrecognized command.");
            break;
        }
        LbIniSkipToNextLine(&parser);
    }
#undef COMMAND_TEXT
    for (ptype = 0; ptype < peep_count; ptype++)
    {
        char sect_name[32];
        struct PeepStat *p_pestat;
        struct PeepStatAdd *p_pestata;

        // Parse the [personN] sections of loaded file
        sprintf(sect_name, "person%d", ptype);
        p_pestat = &peep_type_stats[ptype];
        p_pestata = &peep_type_stats_a[ptype];
        if (LbIniFindSection(&parser, sect_name) != Lb_SUCCESS) {
            CONFWRNLOG("Could not find \"[%s]\" section.", sect_name);
            continue;
        }
        done = false;
#define COMMAND_TEXT(cmd_num) LbNamedEnumGetName(people_conf_person_cmds,cmd_num)
        while (!done)
        {
            // Finding command number in this line
            i = 0;
            cmd_num = LbIniRecognizeKey(&parser, people_conf_person_cmds);
            // Now store the config item in correct place
            switch (cmd_num)
            {
            case PpTyp_Name:
                i = LbIniValueGetStrWord(&parser, p_pestata->Name, sizeof(p_pestata->Name));
                if (i <= 0) {
                    CONFWRNLOG("Couldn't read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                CONFDBGLOG("%s \"%s\"", COMMAND_TEXT(cmd_num), (int)p_pestata->Name);
                break;
            case PpTyp_MaxHealth:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->MaxHealth = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->MaxHealth);
                break;
            case PpTyp_MaxShield:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->MaxShield = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->MaxShield);
                break;
            case PpTyp_MaximumStamina:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->MaximumStamina = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->MaximumStamina);
                break;
            case PpTyp_MaxEnergy:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->MaxEnergy = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->MaxEnergy);
                break;
            case PpTyp_PersuadeReqd:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->PersuadeReqd = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->PersuadeReqd);
                break;
            case PpTyp_PersuadeWorth:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->PersuadeWorth = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->PersuadeWorth);
                break;
            case PpTyp_Speed:
                i = LbIniValueGetLongInt(&parser, &k);
                if (i <= 0) {
                    CONFWRNLOG("Could not read \"%s\" command parameter.", COMMAND_TEXT(cmd_num));
                    break;
                }
                p_pestat->Speed = k;
                CONFDBGLOG("%s %d", COMMAND_TEXT(cmd_num), (int)p_pestat->Speed);
                break;

            case 0: // comment
                break;
            case -1: // end of buffer
            case -3: // end of section
                done = true;
                break;
            default:
                CONFWRNLOG("Unrecognized command.");
                break;
            }
            LbIniSkipToNextLine(&parser);
        }
#undef COMMAND_TEXT
    }
#undef CONFDBGLOG
#undef CONFWRNLOG
    LbIniParseEnd(&parser);
    LbMemoryFree(conf_buf);

    i = 0;
    for (ptype = 1; ptype < peep_count; ptype++)
    {
        struct PeepStatAdd *p_pestata;

        p_pestata = &peep_type_stats_a[ptype];
        if (strlen(p_pestata->Name) > 0) {
            peep_names[i].name = p_pestata->Name;
            peep_names[i].num = ptype;
            i++;
        }
    }
    peep_names[i].name = NULL;
    peep_names[i].num = 0;
}

void load_peep_type_stats_bin(void)
{
    char defs_fname[DISKPATH_SIZE];
    PathInfo *pinfo;
    TbFileHandle fp;

    pinfo = &game_dirs[DirPlace_Data];
    snprintf(defs_fname, DISKPATH_SIZE-1, "%s/peepdefs.dat", pinfo->directory);
    fp = LbFileOpen(defs_fname, Lb_FILE_MODE_READ_ONLY);
    if (fp != INVALID_FILE)
    {
        LbFileRead(fp, peep_type_stats, 12 * sizeof(struct PeepStat));
        LbFileClose(fp);
    }
}

void load_peep_type_stats(void)
{
#if 0
    load_peep_type_stats_bin();
#else
    read_people_conf_file();
#endif
}

const char *person_type_name(ushort ptype)
{
    struct PeepStatAdd *p_pestata;

    p_pestata = &peep_type_stats_a[ptype];
    if (strlen(p_pestata->Name) == 0)
        return "OUTRNG_PERSON";
    return p_pestata->Name;
}

TbBool person_type_is_basic_civilian(ushort ptype)
{
    return (ptype == SubTT_PERS_BRIEFCASE_M
         || ptype == SubTT_PERS_WHITE_BRUN_F
         || ptype == SubTT_PERS_WHIT_BLOND_F
         || ptype == SubTT_PERS_LETH_JACKT_M);
}

TbBool person_type_is_tough_civilian(ushort ptype)
{
    return (ptype == SubTT_PERS_SHADY_M
         || ptype == SubTT_PERS_FAST_BLOND_F);
}

TbBool person_type_is_synd_agent(ushort ptype)
{
    return (ptype == SubTT_PERS_AGENT);
}

TbBool person_type_is_scientist(ushort ptype)
{
    return (ptype == SubTT_PERS_SCIENTIST);
}

TbBool person_type_is_security(ushort ptype)
{
    return (ptype == SubTT_PERS_MERCENARY
         || ptype == SubTT_PERS_MECH_SPIDER
         || ptype == SubTT_PERS_POLICE);
}

TbBool person_type_is_wide_definition_civilian(ushort ptype)
{
    return (person_type_is_basic_civilian(ptype)
         || person_type_is_tough_civilian(ptype)
         || person_type_is_scientist(ptype));
}

TbBool person_type_is_any_major_faction(ushort ptype)
{
    return (person_type_faction_is_syndicate(ptype)
         || person_type_faction_is_church(ptype)
         || person_type_faction_is_punks(ptype));
}

TbBool person_type_only_affected_by_adv_persuader(ushort ptype)
{
    return (ptype == SubTT_PERS_ZEALOT);
}

TbBool person_type_faction_is_syndicate(ushort ptype)
{
    return (ptype == SubTT_PERS_AGENT);
}

TbBool person_type_faction_is_church(ushort ptype)
{
    return (ptype == SubTT_PERS_ZEALOT
      || ptype == SubTT_PERS_HIGH_PRIEST);
}

TbBool person_type_faction_is_punks(ushort ptype)
{
    return (ptype == SubTT_PERS_PUNK_M
      || ptype == SubTT_PERS_PUNK_F);
}

WeaponType person_type_get_favourite_weapon(ushort ptype)
{
    switch (ptype)
    {
    case SubTT_PERS_AGENT:
        return WEP_MINIGUN;
    case SubTT_PERS_ZEALOT:
    case SubTT_PERS_HIGH_PRIEST:
        return WEP_ELLASER;
    case SubTT_PERS_PUNK_F:
    case SubTT_PERS_PUNK_M:
        return WEP_UZI;
    case SubTT_PERS_BRIEFCASE_M:
    case SubTT_PERS_WHITE_BRUN_F:
    case SubTT_PERS_MERCENARY:
    case SubTT_PERS_SCIENTIST:
    case SubTT_PERS_WHIT_BLOND_F:
    case SubTT_PERS_LETH_JACKT_M:
        return WEP_NULL;
    case SubTT_PERS_MECH_SPIDER:
        return WEP_LASER;
    case SubTT_PERS_POLICE:
        return WEP_LASER;
    case SubTT_PERS_SHADY_M:
        return WEP_UZI;
    default:
        return WEP_NULL;
    }
}

int person_type_get_persuasion_credit(ushort ptype)
{
    if (person_type_faction_is_church(ptype))
        return 1000;
    if (person_type_faction_is_syndicate(ptype))
        return 1000;
    if (person_type_faction_is_punks(ptype))
        return 150;
    if (person_type_is_scientist(ptype))
        return 500;
    return 100;
}

void snprint_person_state(char *buf, ulong buflen, struct Thing *p_thing)
{
    char *s;
    ubyte nparams;
    struct PeepStateConfig *pstatcfg;

    pstatcfg = &person_states[p_thing->State];
    s = buf;

    sprintf(s, "%s( ", pstatcfg->Name);
    s += strlen(s);
    nparams = 0;

    if (p_thing->State == PerSt_PROTECT_PERSON)
    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Person(%d)", (int)p_thing->GotoThingIndex);
        s += strlen(s);
        nparams++;
    }
    if (p_thing->State == PerSt_GOTO_POINT)
    {
        if (nparams) { sprintf(s, ", "); s += strlen(s); }
        sprintf(s, "Coord(%d,%d,%d)", (int)p_thing->U.UPerson.GotoX,
          0, (int)p_thing->U.UPerson.GotoZ);
        s += strlen(s);
        nparams++;
    }
    // TODO support more properties

    snprintf(s, buflen - (s-buf), " )");
}


TbBool person_is_dead(ThingIdx thing)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];

    if (p_thing->Type != TT_PERSON)
        return false;

    return (p_thing->State == PerSt_DEAD);
}

TbBool person_is_dead_or_dying(ThingIdx thing)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];

    if (p_thing->Type != TT_PERSON)
        return false;

    return (p_thing->State == PerSt_DEAD)
      || (p_thing->State == PerSt_PERSON_BURNING)
      || (p_thing->State == PerSt_DIEING);
}

ubyte person_mod_chest_level(struct Thing *p_person)
{
    return cybmod_chest_level(&p_person->U.UPerson.UMod);
}

void set_person_mod_chest_level(struct Thing *p_person, ubyte nmod)
{
    set_cybmod_chest_level(&p_person->U.UPerson.UMod, nmod);
}

ubyte person_mod_legs_level(struct Thing *p_person)
{
    return cybmod_legs_level(&p_person->U.UPerson.UMod);
}

void set_person_mod_legs_level(struct Thing *p_person, ubyte nmod)
{
    set_cybmod_legs_level(&p_person->U.UPerson.UMod, nmod);
}

ubyte person_mod_arms_level(struct Thing *p_person)
{
    return cybmod_arms_level(&p_person->U.UPerson.UMod);
}

void set_person_mod_arms_level(struct Thing *p_person, ubyte nmod)
{
    set_cybmod_arms_level(&p_person->U.UPerson.UMod, nmod);
}

ubyte person_mod_brain_level(struct Thing *p_person)
{
    return cybmod_brain_level(&p_person->U.UPerson.UMod);
}

void set_person_mod_brain_level(struct Thing *p_person, ubyte nmod)
{
    set_cybmod_brain_level(&p_person->U.UPerson.UMod, nmod);
}

ubyte person_mod_skin_level(struct Thing *p_person)
{
    return cybmod_skin_level(&p_person->U.UPerson.UMod);
}

void set_person_mod_skin_level(struct Thing *p_person, ubyte nmod)
{
    set_cybmod_skin_level(&p_person->U.UPerson.UMod, nmod);
}

TbBool person_carries_weapon(struct Thing *p_person, ubyte weapon)
{
    return weapons_has_weapon(p_person->U.UPerson.WeaponsCarried, weapon);
}

TbBool person_carries_any_medikit(ThingIdx person)
{
    struct Thing *p_person;

    if (person <= 0)
        return false;

    p_person = &things[person];
    return person_carries_weapon(p_person, WEP_MEDI2) || person_carries_weapon(p_person, WEP_MEDI1);
}

void person_weapons_reset_previous(struct Thing *p_person)
{
    PlayerIdx plyr;
    ushort plagent;

    if ((p_person->Flag & TngF_PlayerAgent) == 0) {
        return;
    }

    plyr = p_person->U.UPerson.ComCur >> 2;
    plagent = p_person->U.UPerson.ComCur & 3;
    player_agent_reset_prev_weapon(plyr, plagent);
}

void person_weapons_update_previous(struct Thing *p_person)
{
    PlayerIdx plyr;
    ushort plagent;

    if ((p_person->Flag & TngF_PlayerAgent) == 0) {
        return;
    }

    plyr = p_person->U.UPerson.ComCur >> 2;
    plagent = p_person->U.UPerson.ComCur & 3;
    player_agent_update_prev_weapon(plyr, plagent);
}

TbBool person_can_accept_control(ThingIdx person)
{
    return !person_is_dead_or_dying(person)
      && !thing_is_destroyed(person);
}

TbBool person_can_use_medikit(ThingIdx person)
{
    if (!person_carries_any_medikit(person))
        return false;
    return true;
}

void person_give_best_mods(struct Thing *p_person)
{
    set_person_mod_brain_level(p_person, 3);
    set_person_mod_legs_level(p_person, 3);
    set_person_mod_arms_level(p_person, 3);
    set_person_mod_chest_level(p_person, 3);
}

TbBool person_is_executing_commands(ThingIdx person)
{
    struct Thing *p_person;

    if (person <= 0)
        return false;

    p_person = &things[person];
    return ((p_person->Flag2 & TgF2_Unkn0800) != 0);
}

TbBool person_only_affected_by_adv_persuader(ThingIdx person)
{
    struct Thing *p_person;
    ushort ptype;

    if (person <= 0)
        return false;

    p_person = &things[person];

    if ((p_person->Flag2 & TgF2_AlteredSubType) != 0)
        ptype = p_person->U.UPerson.OldSubType;
    else
        ptype = p_person->SubType;

    return person_type_only_affected_by_adv_persuader(ptype);
}

TbBool person_is_persuaded(ThingIdx person)
{
    struct Thing *p_person;

    if (person <= 0)
        return false;

    p_person = &things[person];
    return ((p_person->Flag & TngF_Persuaded) != 0);
}

TbBool person_is_persuaded_by_person(ThingIdx thing, ThingIdx owntng)
{
    struct Thing *p_person;

    if (thing <= 0)
        return false;

    p_person = &things[thing];
    if ((p_person->Flag & TngF_Persuaded) == 0)
        return false;

    return (p_person->Owner == owntng);
}

TbBool person_is_persuaded_by_player(ThingIdx thing, ushort plyr)
{
    struct Thing *p_thing;
    short plyagent, plygroup;
    struct Thing *p_person;

    if (thing <= 0)
        return false;

    p_thing = &things[thing];
    if ((p_thing->Flag & TngF_Persuaded) == 0)
        return false;

    plyagent = players[plyr].DirectControl[0];
    plygroup = things[plyagent].U.UPerson.Group;

    p_person = &things[p_thing->Owner];
    return (p_person->U.UPerson.Group == plygroup);
}

ubyte person_get_selected_weapon(ThingIdx thing)
{
    struct Thing *p_thing;

    if (thing <= 0)
        return WEP_NULL;

    p_thing = &things[thing];
    return p_thing->U.UPerson.CurrentWeapon;
}

short calc_person_speed(struct Thing *p_person)
{
    struct PeepStat *pstat;
    short mood;
    short speed;

    pstat = &peep_type_stats[p_person->SubType];

    mood = p_person->U.UPerson.Mood;
    if (mood < 0)
        mood = -mood;

    speed = pstat->Speed + 2 * mood + 100 * person_mod_legs_level(p_person);

    // Flamer slows you down? Why? This makes it even more useless..
    if (p_person->U.UPerson.CurrentWeapon == WEP_FLAMER)
        speed >>= 1;

    if ((p_person->Flag & TngF_Persuaded) != 0)
        speed += 250;

    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
        speed += 512;

    if (speed > PERSON_MAX_SPEED)
        speed = PERSON_MAX_SPEED;

    return speed;
}

short calc_person_heavy_weapon_spread(struct Thing *p_person)
{
    // The wider the shooter's mood swing and the less arm mod level
    // they have, the more the shot is allowed to stray off target.
    return 60 + abs(p_person->U.UPerson.Mood)
      - 16 * cybmod_arms_level(&p_person->U.UPerson.UMod);
}

void person_set_helath_to_max_limit(struct Thing *p_person)
{
    p_person->U.UPerson.MaxHealth = PERSON_MAX_HEALTH_LIMIT;
    p_person->Health = 2 * p_person->U.UPerson.MaxHealth; // double health - fill red bar
}

void person_set_energy_to_max_limit(struct Thing *p_person)
{
    p_person->U.UPerson.MaxEnergy = PERSON_MAX_ENERGY_LIMIT;
    p_person->U.UPerson.Energy = p_person->U.UPerson.MaxEnergy;
}

void set_person_health_shield_type(struct Thing *p_person, ushort stype)
{
    struct PeepStat *pstat;

    pstat = &peep_type_stats[stype];

    p_person->U.UPerson.ShieldEnergy = pstat->MaxShield +
        (person_mod_brain_level(p_person) * pstat->MaxShield * 50 / 100);

    p_person->Health = pstat->MaxHealth +
       ((person_mod_chest_level(p_person) * pstat->MaxHealth * 50 / 100) +
        (person_mod_brain_level(p_person) * pstat->MaxHealth * 50 / 100) +
        (person_mod_legs_level(p_person) * pstat->MaxHealth * 25 / 100) +
        (person_mod_arms_level(p_person) * pstat->MaxHealth * 25 / 100)) / 4;

    p_person->U.UPerson.MaxHealth = p_person->Health;
    p_person->U.UPerson.MaxShieldEnergy = p_person->U.UPerson.ShieldEnergy;

    if (person_mod_chest_level(p_person) == 4)
    {
        person_set_helath_to_max_limit(p_person);
    }
}

void set_person_energy_stamina_type(struct Thing *p_person, ushort stype)
{
    struct PeepStat *pstat;

    pstat = &peep_type_stats[stype];

    p_person->U.UPerson.Energy = pstat->MaxEnergy +
        (person_mod_chest_level(p_person) * pstat->MaxEnergy * 50 / 100);

    p_person->U.UPerson.Stamina = pstat->MaximumStamina;

    p_person->U.UPerson.MaxEnergy = p_person->U.UPerson.Energy;
    p_person->U.UPerson.MaxStamina = p_person->U.UPerson.Stamina;

    if (person_mod_chest_level(p_person) == 4)
    {
        person_set_energy_to_max_limit(p_person);
    }
}

void person_init_wait(struct Thing *p_person, short wait_turns)
{
    p_person->State = PerSt_WAIT;
    p_person->U.UPerson.ComTimer = wait_turns;
}

void person_start_executing_commands(struct Thing *p_person)
{
    p_person->Flag2 |= TgF2_Unkn0800;
    p_person->Flag |= TngF_Unkn0040;
    ingame.Flags |= GamF_Unkn0100;
}

short person_slot_as_player_agent(struct Thing *p_person, PlayerIdx plyr)
{
    PlayerInfo *p_player;
    struct Thing *p_agent;
    short plagent;

    p_player = &players[plyr];

    for (plagent = 0; plagent < playable_agents; plagent++)
    {
        p_agent = p_player->MyAgent[plagent];
        if (p_person->ThingOffset == p_agent->ThingOffset)
        {
            return plagent;
        }
    }
    return -1;
}

TbBool person_is_player_agent_in_slot(struct Thing *p_person, PlayerIdx plyr, short plagent)
{
    PlayerInfo *p_player;
    struct Thing *p_agent;

    p_player = &players[plyr];
    p_agent = p_player->MyAgent[0];

    return (p_person->ThingOffset == p_agent->ThingOffset);
}

TbBool person_has_slot_in_any_player_dcontrol(ThingIdx person)
{
    struct Thing *p_person;

    if (person <= 0)
        return false;

    if (person_is_executing_commands(person))
        return false;

    p_person = &things[person];

    return ((p_person->Flag & TngF_PlayerAgent) != 0);
}

short person_get_dcontrol_player(ThingIdx person)
{
    struct Thing *p_person;

    if (!person_has_slot_in_any_player_dcontrol(person)) {
        return -1;
    }

    p_person = &things[person];

    return (p_person->U.UPerson.ComCur >> 2);
}

short person_slot_in_player_dcontrol(ThingIdx person, PlayerIdx plyr)
{
    struct Thing *p_person;
    PlayerIdx pers_plyr;

    if (!person_has_slot_in_any_player_dcontrol(person)) {
        return -1;
    }

    p_person = &things[person];

    pers_plyr = p_person->U.UPerson.ComCur >> 2;
    if (pers_plyr != plyr) {
        return -1;
    }

    return (p_person->U.UPerson.ComCur & 3);
}

MapCoord player_agent_person_clear_user_vect_y(struct Thing *p_person)
{
    PlayerIdx plyr;
    ushort plagent;

    plyr = p_person->U.UPerson.ComCur >> 2;
    plagent = p_person->U.UPerson.ComCur & 3;

    return player_agent_clear_user_vect_y(plyr, plagent);
}

void player_agent_person_get_user_vect(struct Thing *p_person, struct MapCoords *p_usrv)
{
    PlayerIdx plyr;
    ushort plagent;

    plyr = p_person->U.UPerson.ComCur >> 2;
    plagent = p_person->U.UPerson.ComCur & 3;

    player_agent_get_user_vect(plyr, plagent, p_usrv);
}

ubyte person_sex(struct Thing *p_person)
{
    return (((1 << p_person->SubType) & female_peep) != 0) ? PERSON_FEMALE : PERSON_MALE;
}

void set_peep_comcur(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_set_peep_comcur\n"
        : : "a" (p_person));
#endif
    ushort plyr;
    short plagent;

    plyr = local_player_no;
    plagent = person_slot_as_player_agent(p_person, plyr);

    if (plagent >= 0)
    {
        p_person->U.UPerson.ComHead = 0;
        p_person->U.UPerson.ComCur = (plyr << 2) | plagent;
    }

    if (person_is_player_agent_in_slot(p_person, plyr, 0))
    {
        game_set_cam_track_thing_xz(p_person->ThingOffset);
    }
}

void person_finish_executing_commands(struct Thing *p_person)
{
    if (((p_person->Flag & TngF_PlayerAgent) != 0) && ((p_person->Flag2 & TgF2_Unkn0800) != 0))
    {
        p_person->Flag2 &= ~TgF2_Unkn0800;
        ingame.Flags &= ~GamF_Unkn0100;
        set_peep_comcur(p_person);
    }
}

void person_burning_stifle_fire(struct Thing *p_person)
{
    p_person->Flag &= ~TngF_Unkn40000000;
    p_person->State = PerSt_WAIT;
    remove_path(p_person);
    person_reset_default_anim_mode(p_person);
}

void person_resurrect(struct Thing *p_person)
{
    p_person->Flag &= ~TngF_Destroyed;
    p_person->Flag &= ~TngF_Unkn02000000;
    p_person->Flag &= ~TngF_Unkn40000000;
    p_person->State = PerSt_WAIT;
    remove_path(p_person);
    p_person->Health = p_person->U.UPerson.MaxHealth * 3 / 4;
    person_reset_default_anim_mode(p_person);
}

void person_set_persuade_power__to_allow_all(struct Thing *p_person)
{
    short max_required_pers_power;

    max_required_pers_power = 20;
    p_person->U.UPerson.PersuadePower = max(p_person->U.UPerson.PersuadePower,
      max_required_pers_power);
}

TbBool person_has_weapon_target_within_range(struct Thing *p_person, ThingIdx target)
{
    struct Thing *p_target;
    int dist, range;

    p_target = &things[target];

    range = get_weapon_range(p_person) + 127;
    dist = get_things_distance_mapcoords_fast(p_person->ThingOffset, target);

    return  (dist <= range + p_target->Radius);
}

TbBool person_is_basic_civilian(struct Thing *p_person, TbBool ignore_disguise)
{
    ubyte subType;

    if (ignore_disguise && ((p_person->Flag2 & TgF2_AlteredSubType) != 0))
        subType = p_person->U.UPerson.OldSubType;
    else
        subType = p_person->SubType;

    return person_type_is_basic_civilian(subType);
}

TbBool person_is_scientist(struct Thing *p_person, TbBool ignore_disguise)
{
    ubyte subType;

    if (ignore_disguise && ((p_person->Flag2 & TgF2_AlteredSubType) != 0))
        subType = p_person->U.UPerson.OldSubType;
    else
        subType = p_person->SubType;

    return person_type_is_scientist(subType);
}

ushort calc_person_radius_type(struct Thing *p_person, ushort stype)
{
    ushort r;
    switch (stype)
    {
    case SubTT_PERS_AGENT:
    case SubTT_PERS_PUNK_F:
    case SubTT_PERS_POLICE:
    case SubTT_PERS_PUNK_M:
    default:
        r = 80;
        break;
    case SubTT_PERS_ZEALOT:
    case SubTT_PERS_HIGH_PRIEST:
        r = 100;
        break;
    case SubTT_PERS_MERCENARY:
    case SubTT_PERS_SCIENTIST:
    case SubTT_PERS_SHADY_M:
    case SubTT_PERS_BRIEFCASE_M:
    case SubTT_PERS_WHITE_BRUN_F:
    case SubTT_PERS_WHIT_BLOND_F:
    case SubTT_PERS_LETH_JACKT_M:
        r = 100;
        break;
    case SubTT_PERS_MECH_SPIDER:
        r = 384;
        break;
    }
    return r;
}

void set_person_stats_type(struct Thing *p_person, ushort stype)
{
    set_person_health_shield_type(p_person, stype);
    set_person_energy_stamina_type(p_person, stype);
    p_person->U.UPerson.PersuadePower = 0;
    p_person->Speed = calc_person_speed(p_person);
}

/** Set a person frame with single-angle sprite.
 * Person frames are a bit different from other things, even the
 * single-angle ones, because StartFrame in incremented when used as frame index.
 */
void set_person_frame_noangle(struct Thing *p_person, ushort anim_start)
{
    p_person->StartFrame = anim_start - 1;
    p_person->Frame = nstart_ani[p_person->StartFrame + 1];
}

void reset_person_frame(struct Thing *p_person)
{
    ushort person_anim;

    person_anim = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->StartFrame = person_anim - 1;
    p_person->Frame = nstart_ani[p_person->StartFrame + 1 + p_person->U.UPerson.Angle];
}

ushort get_person_anim_subframe(struct Thing *p_person)
{
    short sbfrm;

    sbfrm = p_person->Frame - nstart_ani[p_person->StartFrame + 1 + p_person->U.UPerson.Angle];

    // Person animations have limited frames, as some arrays are indexing on them
    // though if some special effect frames exceed that, no problem - we will limit the value
    // We may also encounter outranged value here if frame was modified to an animation
    // which is not stored in StartFrame, this happens for some effects
    if ((sbfrm < 0) || (sbfrm >= PERSON_ANIM_MAX_FRAMES))
        sbfrm = 0;

    return sbfrm;
}

void switch_person_anim_mode(struct Thing *p_person, ubyte animode)
{
    ushort person_anim;

    person_anim = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->Frame -= nstart_ani[person_anim + p_person->U.UPerson.Angle];
    p_person->U.UPerson.AnimMode = animode;
    person_anim = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->StartFrame = person_anim - 1;
    p_person->Frame += nstart_ani[p_person->StartFrame + 1 + p_person->U.UPerson.Angle];
}

void set_person_anim_mode(struct Thing *p_person, ubyte animode)
{
    p_person->U.UPerson.AnimMode = animode;
    reset_person_frame(p_person);
}

void change_person_angle(struct Thing *p_person, ubyte angl)
{
    short person_anim, sframe;

    if (angl == p_person->U.UPerson.Angle)
        return;

    p_person->U.UPerson.Angle = angl;
    person_anim = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];

    // TODO why are we not updating p_person->StartFrame here?
    sframe = person_anim - 1;
    p_person->Frame = nstart_ani[sframe + 1 + p_person->U.UPerson.Angle];
}

void change_person_angle_full(struct Thing *p_person, short full_angle)
{
    ubyte angl;
    angl = (((full_angle + 128) >> 8) + 8) & 7;
    change_person_angle(p_person, angl);
}

void init_person_thing(struct Thing *p_person)
{
    set_person_health_shield_type(p_person, p_person->SubType);
    set_person_energy_stamina_type(p_person, p_person->SubType);

    p_person->U.UPerson.PersuadePower = 0;
    p_person->Radius = calc_person_radius_type(p_person, p_person->SubType);

    if (current_level != 0)
        p_person->U.UPerson.CurrentWeapon = 0;

    if ((p_person->Flag & TngF_Destroyed) != 0)
    {
        p_person->U.UPerson.AnimMode = ANIM_PERS_DEAD_BODY;
        p_person->State = PerSt_DEAD;
        p_person->U.UPerson.FrameId.Version[3] = 1;
        p_person->U.UPerson.FrameId.Version[4] = 0;
    }
    else
    {
        switch_person_anim_mode(p_person, ANIM_PERS_IDLE);
    }
}

/** Frees a linked list of paths, and adds the paths to free list.
 */
void path_free_linked_list(ushort first_path)
{
    ushort free_head, path;
    ushort count;

    free_head = head_my_path;
    count = path_count;

    path = first_path;
    while (path != 0)
    {
        ushort tmpath;

        tmpath = my_paths[path].Next;
        my_paths[path].Flag = 0;
        my_paths[path].Next = free_head;
        free_head = path;
        path = tmpath;
        count--;
    }
    path_count = count;
    head_my_path = free_head;
}

void remove_path(struct Thing *p_thing)
{
    ushort path;

    p_thing->Flag2 &= ~TgF2_Unkn0040;
    if (p_thing->U.UPerson.PathIndex != 0)
    {
        if (p_thing->Type == TT_PERSON)
            p_thing->U.UPerson.LastDist = 32000;
        path = p_thing->U.UPerson.PathIndex;
        p_thing->U.UPerson.PathIndex = 0;
        path_free_linked_list(path);
        p_thing->PathOffset = 0;
        p_thing->Flag &= ~TngF_Unkn00020000;
    }
}

void set_person_animmode_walk(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_set_person_animmode_walk\n"
        : : "a" (p_person));
#endif
    if (p_person->State == PerSt_PERSON_BURNING)
        return;
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    p_person->Flag2 &= ~(TgF2_IgnoreEnemies|TgF2_Unkn00080000);
    p_person->Speed = calc_person_speed(p_person);
}

static DIFFTEST_REF_FUNC int check_col_collision_floor_asm(int cor_x, int cor_y, int cor_z)
{
    int ret;
    asm volatile ("call ASM_check_col_collision_floor\n"
        : "=a" (ret) : "a" (cor_x), "d" (cor_y), "b" (cor_z));
    return ret;
}

int check_col_collision_floor(int cor_x, int cor_y, int cor_z)
{
    struct MyMapElement *p_mel;
    s32 tile_x, tile_z;
    s32 alt00, alt10, alt01, alt11;
    s32 frac_x, frac_z;
    s32 alt;

    DIFFTEST_SHADOW_RET(check_col_collision_floor, int,
      check_col_collision_floor_asm(cor_x, cor_y, cor_z));
    tile_x = cor_x >> 8;
    if (tile_x < 0 || tile_x >= 128)
        DIFFTEST_RETURN(0);
    tile_z = cor_z >> 8;
    if (tile_z < 0 || tile_z >= 128)
        DIFFTEST_RETURN(0);

    p_mel = &game_my_big_map[tile_z * 128 + tile_x];
    alt00 = p_mel[0].Alt;
    alt10 = p_mel[1].Alt;
    alt01 = p_mel[128].Alt;
    alt11 = p_mel[129].Alt;

    if (alt10 == alt00 && alt00 == alt01 && alt01 == alt11)
    {
        alt = alt00;
    }
    else
    {
        frac_x = (short)(cor_x & 0xFF);
        frac_z = (short)(cor_z & 0xFF);
        if (frac_x + frac_z < 256)
        {
            alt = (((alt01 - alt00) * frac_z) >> 8) +
              ((((alt10 - alt00) * frac_x) >> 8) + alt00);
        }
        else
        {
            alt = ((((alt01 - alt11) * (256 - frac_x)) >> 8) + alt11) +
              (((alt10 - alt11) * (256 - frac_z)) >> 8);
        }
    }

    if (alt > cor_y)
        DIFFTEST_RETURN(-1);
    DIFFTEST_RETURN(0);
}

static DIFFTEST_REF_FUNC short do_vect_colide_asm(struct MyMapElement *p_me,
  int x1, int y1, int z1, int x2, int y2, int z2)
{
    short ret;
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[3];

    stkargs[0] = (int)(intptr_t)x2;
    stkargs[1] = (int)(intptr_t)y2;
    stkargs[2] = (int)(intptr_t)z2;

    asm volatile (
      "push 8(%5)\n"
      "push 4(%5)\n"
      "push 0(%5)\n"
      "call ASM_do_vect_colide\n"
        : "=a" (ret)
        : "a" (p_me), "d" (x1), "b" (y1), "c" (z1), "S" (stkargs)
        : "cc", "memory");
    return ret;
}

short do_vect_colide(struct MyMapElement *p_me, int x1, int y1, int z1, int x2, int y2, int z2)
{
    struct ColVectList *p_cvlist;
    struct ColVect *p_colvect;
    ushort cvlist;
    ushort count;
    short face;
    s32 dy;

    DIFFTEST_SHADOW_RET(do_vect_colide, short,
      do_vect_colide_asm(p_me, x1, y1, z1, x2, y2, z2));
    cvlist = p_me->ColHead;
    count = 0;
    while (cvlist != 0)
    {
        if (count >= 30)
            break;
        p_cvlist = &game_col_vects_list[cvlist];
        count++;
        if (p_cvlist->Object >= 0)
        {
            p_colvect = &game_col_vects[p_cvlist->Vect];
            face = p_colvect->Face;
            if ((face >= 0) || (game_object_faces4[-face].GFlags & FGFlg_Unkn04) == 0)
            {
                // TODO the original indexes faces3 with a negated positive
                // face, reading before the array start; kept as is
                if ((face <= 0) || (game_object_faces3[-face].GFlags & FGFlg_Unkn04) == 0)
                {
                    dy = (short)((y1 << 3) - p_colvect->Y1);
                    if (dy < 0)
                        dy = -dy;
                    if (dy < 160)
                    {
                        if (two4_line_intersection(p_colvect->X1, p_colvect->Z1,
                          p_colvect->X2, p_colvect->Z2, x1, z1, x2, z2))
                            DIFFTEST_RETURN((short)cvlist);
                    }
                }
            }
        }
        cvlist = game_col_vects_list[cvlist].NextColList & 0x7FFF;
    }
    DIFFTEST_RETURN((short)0);
}

int can_i_see_thing(struct Thing *p_me, struct Thing *p_him, int max_dist, ushort flags)
{
    int cor_dx, cor_dy, cor_dz;
    int distance;
    int tl_dx, tl_dz;
    int cor_swp_inc_x, cor_swp_inc_y, cor_swp_inc_z;
    TbBool vehicle_involved;
    int cor_him_x, cor_him_y, cor_him_z;
    int cor_me_x, cor_me_y, cor_me_z;
    int cor_swp_x, cor_swp_y, cor_swp_z;
    short delta_angle;
    int sweep_steps, step;
    ubyte tl_him_zip;

    if ((p_me == NULL) || (p_him == NULL))
        return 0;
    if ((p_him->Flag2 & (TgF2_ExistsOffMap|TgF2_Unkn0002)) != 0)
        return 0;

    vehicle_involved = (p_me->Type == TT_VEHICLE) || (p_him->Type == TT_VEHICLE);
    if (((p_me->Flag & TngF_InVehicle) != 0) ||
      ((p_him->Flag & TngF_InVehicle) != 0)) {
        vehicle_involved = 1;
    }

    cor_him_x = PRCCOORD_TO_MAPCOORD(p_him->X);
    cor_him_y = PRCCOORD_TO_MAPCOORD(p_him->Y);
    cor_him_z = PRCCOORD_TO_MAPCOORD(p_him->Z);

    cor_me_x = PRCCOORD_TO_MAPCOORD(p_me->X);
    cor_me_y = PRCCOORD_TO_MAPCOORD(p_me->Y);
    cor_me_z = PRCCOORD_TO_MAPCOORD(p_me->Z);

    // Subtracting pre-shifted values would not always produce the same results
    cor_dx = PRCCOORD_TO_MAPCOORD(p_him->X - p_me->X);
    cor_dy = cor_him_y - cor_me_y;
    cor_dz = PRCCOORD_TO_MAPCOORD(p_him->Z - p_me->Z);

    distance = cor_dz * cor_dz + cor_dx * cor_dx;
    if (max_dist < distance)
        return 0;

    distance += ((cor_dy * cor_dy) << 6);
    delta_angle = arctan(cor_dx, -cor_dz);

    if ((flags & 0x01) == 0)
    {
        short angl;
        angl = (p_me->U.UPerson.Angle << 8) - delta_angle;
        angl = (angl + 0x0800) & 0x7FF;
        if (angl > 200 && angl < 0x7FF - 200)
          return 0;
    }
    distance = LbSqrL(distance);

    sweep_steps = distance >> 6;
    if (sweep_steps == 0)
        sweep_steps = 1;
    if (distance == 0)
        distance = 1;

    {
        struct MyMapElement *p_mapel;
        short tl_him_x, tl_him_z;

        tl_him_x = MAPCOORD_TO_TILE(cor_him_x);
        tl_him_z = MAPCOORD_TO_TILE(cor_him_z);
        p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_him_z + tl_him_x];
        tl_him_zip = p_mapel->Zip;
    }

    cor_me_y += 18;

    // TODO wouldn't it make sense to keep the values with fractional part, to avoid rounding down on each step?
    cor_swp_x = cor_me_x;
    cor_swp_y = cor_me_y;
    cor_swp_z = cor_me_z;

    cor_swp_inc_x = cor_dx / sweep_steps;
    cor_swp_inc_y = cor_dy / sweep_steps;
    cor_swp_inc_z = cor_dz / sweep_steps;

    tl_dx = (cor_dx <= 0) ? -1 : 1;
    tl_dz = (cor_dz <= 0) ? -1 : 1;

    if (cor_dy != 0)
        tl_him_zip = 0;

    for (step = sweep_steps; step > 0; step--)
    {
      struct MyMapElement *p_mapel;
      short tl_me_x, tl_me_z;


      if ((cor_swp_x < 0) || (cor_swp_x >= MAP_COORD_WIDTH))
          return 0;
      if ((cor_swp_z < 0) || (cor_swp_z >= MAP_COORD_HEIGHT))
          return 0;
      if (vehicle_involved) {
          if (check_col_collision(cor_swp_x, cor_swp_y, cor_swp_z))
              return 0;
      } else {
          if (check_col_collision_floor(cor_swp_x, cor_swp_y, cor_swp_z))
              return 0;
      }

      tl_me_x = MAPCOORD_TO_TILE(cor_swp_x);
      tl_me_z = MAPCOORD_TO_TILE(cor_swp_z);

      p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_me_z + tl_me_x];

      if (debug_hud_collision || byte_1C844F)
      {
          char locstr[12];
          sprintf(locstr, "%d", (int)p_mapel->Zip);
          draw_mapcoord_simple_text_at_ground(cor_swp_x, cor_swp_z, locstr);
      }

      if ((tl_him_zip != 0) && (tl_him_zip == p_mapel->Zip))
        break;

      if (!vehicle_involved)
      {
          int cor_x, cor_z;
          {
              if (debug_hud_collision || byte_1C844F) {
                  cor_x = (cor_swp_x & 0xFF00);
                  cor_z = (cor_swp_z & 0xFF00);
                  draw_map_flat_rect(cor_x, 8 * cor_swp_y, cor_z, 256, 256, colour_lookup[0]);
              }
              p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tl_me_z) + (tl_me_x)];
              if (do_vect_colide(p_mapel, cor_me_x, cor_me_y, cor_me_z,
                cor_him_x, cor_him_y, cor_him_z))
                  return 0;
          }
          if (tl_dx != 0)
          {
              if (debug_hud_collision || byte_1C844F) {
                  cor_x = (cor_swp_x & 0xFF00) + TILE_TO_MAPCOORD(tl_dx, 0);
                  cor_z = (cor_swp_z & 0xFF00);
                  draw_map_flat_rect(cor_x, 8 * cor_swp_y, cor_z, 256, 256, colour_lookup[2]);
              }
              p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tl_me_z) + (tl_me_x + tl_dx)];
              if (do_vect_colide(p_mapel, cor_me_x, cor_me_y, cor_me_z,
                cor_him_x, cor_him_y, cor_him_z))
                  return 0;
          }
          if (tl_dz != 0)
          {
              if (debug_hud_collision || byte_1C844F) {
                  cor_x = (cor_swp_x & 0xFF00);
                  cor_z = (cor_swp_z & 0xFF00) + TILE_TO_MAPCOORD(tl_dz, 0);
                  draw_map_flat_rect(cor_x, 8 * cor_swp_y, cor_z, 256, 256, colour_lookup[4]);
              }
              p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tl_me_z + tl_dz) + (tl_me_x)];
              if (do_vect_colide(p_mapel, cor_me_x, cor_me_y, cor_me_z,
                cor_him_x, cor_him_y, cor_him_z))
                  return 0;
          }
          {
              if (debug_hud_collision || byte_1C844F) {
                  cor_x = (cor_swp_x & 0xFF00) + TILE_TO_MAPCOORD(tl_dx, 0);
                  cor_z = (cor_swp_z & 0xFF00) + TILE_TO_MAPCOORD(tl_dz, 0);
                  draw_map_flat_rect(cor_x, 8 * cor_swp_y, cor_z, 256, 256, colour_lookup[3]);
              }
              p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tl_me_z + tl_dz) + (tl_me_x + tl_dx)];
              if (do_vect_colide(p_mapel, cor_me_x, cor_me_y, cor_me_z,
                cor_him_x, cor_him_y, cor_him_z))
                  return 0;
          }
      }
      if (debug_hud_collision || byte_1C844F)
      {
          int cor_x, cor_y, cor_z;
          ubyte colkp;

          cor_x = cor_swp_x + cor_swp_inc_x;
          cor_y = cor_swp_y + cor_swp_inc_y;
          cor_z = cor_swp_z + cor_swp_inc_z;

          if ((step & 1) != 0)
              colkp = 4;
          else
              colkp = 2;
          draw_mapcoord_line_col(cor_swp_x, 8 * cor_swp_y, cor_swp_z,
            cor_x, 8 * cor_y, cor_z, colour_lookup[colkp]);
          colkp = 0;
          draw_mapcoord_line_col(cor_swp_x, 0, cor_swp_z,
            cor_x, 0, cor_z, colour_lookup[colkp]);
      }
      cor_swp_x += cor_swp_inc_x;
      cor_swp_y += cor_swp_inc_y;
      cor_swp_z += cor_swp_inc_z;
    }

    if ((flags == 0x01) && delta_angle >> 8 != p_me->U.UPerson.Angle )
        change_person_angle(p_me, ((delta_angle + 128) & 0x7FF) >> 8);

    return distance;
}

TbBool can_i_enter_vehicle(struct Thing *p_me, struct Thing *p_vehicle)
{
#if 0
    TbBool ret;
    asm volatile ("call ASM_can_i_enter_vehicle\n"
        : "=r" (ret) : "a" (p_me), "d" (p_vehicle));
    return ret;
#endif
    struct Thing *p_thing;
    ThingIdx thing;
    ushort tngroup, mygroup;

    if (person_is_executing_commands(p_me->ThingOffset)) {
        // if person is executing commands, allow unconditionally
        return true;
    }

    thing = p_vehicle->U.UVehicle.PassengerHead;
    if (thing <= 0)
        return true;
    p_thing = &things[thing];

    tngroup = p_thing->U.UObject.EffectiveGroup & 0x7F;
    mygroup = p_me->U.UObject.EffectiveGroup & 0x7F;
    if (groups_equal(tngroup, mygroup))
        return true;
    if (groups_have_truce(tngroup, mygroup) || groups_have_truce(mygroup, tngroup))
        return true;

    return false;
}

void set_person_persuaded(struct Thing *p_person, struct Thing *p_attacker, ushort energy)
{
    // Cannot persuade other players agent
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
        return;

    if (p_person->State == PerSt_DEAD)
        return;

    play_dist_sample(p_person, 20, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    remove_path(p_person);

    p_attacker->U.UPerson.Energy -= energy;
    if (p_attacker->U.UPerson.Energy < 0)
        p_attacker->U.UPerson.Energy = 0;

    p_person->PTarget = NULL;
    p_person->U.UPerson.Target2 = 0;
    p_person->State = PerSt_NONE;

    p_person->Flag |= TngF_Unkn40000000 | TngF_Unkn0004;
    p_person->Owner = p_attacker->ThingOffset;
    p_person->Flag &= ~(TngF_Unkn00800000|TngF_DangerFlee|TngF_Unkn00020000|TngF_TriggerUse|TngF_PersKeepPost);

    set_person_animmode_walk(p_person);
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 3;
    p_person->U.UPerson.Timer2 = 5;
    p_person->U.UPerson.StartTimer2 = 5;
    p_person->SubState = 0;

    p_person->U.UPerson.EffectiveGroup = p_attacker->U.UPerson.EffectiveGroup;
    p_person->U.UPerson.Within = 0;
    {
        struct PeepStat *pstat;

        pstat = &peep_type_stats[p_person->SubType];
        p_attacker->U.UPerson.PersuadePower += pstat->PersuadeWorth;
    }
    if ((p_person->Flag2 & TgF2_KnockedOut) == 0)
    {
          set_person_frame_noangle(p_person, 1060);
    }
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    {
        word_17FA58[word_1531DA] = p_person->ThingOffset;
        word_1531DA++;
    }
    p_person->State = PerSt_BEING_PERSUADED;
    {
        ushort group;
        group = p_person->U.UPerson.Group & PEOPLE_GROUPS_INDEX_MASK;
        group_actions[group].Persuaded++;
    }
    if (!in_network_game && (p_attacker->Flag & TngF_PlayerAgent) &&
      (p_attacker->U.UPerson.EffectiveGroup == ingame.MyGroup))
    {
        short plagent;

        persuaded_person_add_to_stats(p_person, open_brief);

        for (plagent = 0; plagent < playable_agents; plagent++)
        {
            struct Thing *p_agent;

            p_agent = players[local_player_no].MyAgent[plagent];
            if (p_person == p_agent->PTarget)
                p_agent->PTarget = NULL;
            if (p_agent->U.UPerson.Target2 == p_person->ThingOffset)
                p_agent->U.UPerson.Target2 = 0;
        }
    }
}

void stop_being_persuaded(struct Thing *p_person)
{
#if 1
    asm volatile ("call ASM_stop_being_persuaded\n"
        :  : "a" (p_person));
    return;
#endif
}

void unpersuade_my_peeps(struct Thing *p_owntng)
{
    ThingIdx person;
    struct Thing *p_person;
    ushort count;
    int i, k;

    count = word_1531DA;
    for (i = 1; i < count; i++)
    {
        person = word_17FA58[i];
        p_person = &things[person];
        if (p_person->Owner != p_owntng->ThingOffset)
            continue;
        p_person->Flag &= ~TngF_Persuaded;
        p_person->U.UPerson.EffectiveGroup = p_owntng->U.UPerson.Group;
        p_person->State = PerSt_NONE;
        p_person->Flag |= TngF_Unkn0040 | TngF_Unkn0004;
        --group_actions[p_person->U.UPerson.Group].Persuaded;
        for (k = i; k < count - 1; k++) {
            word_17FA58[k] = word_17FA58[k+1];
        }
        count--;
        if (!in_network_game && (p_owntng->U.UPerson.EffectiveGroup == ingame.MyGroup))
        {
            persuaded_person_remove_from_stats(p_person, open_brief);
        }
    }
    p_owntng->U.UPerson.PersuadePower = 0;
    word_1531DA = count;
}

void person_reset_default_anim_mode(struct Thing *p_person)
{
    ushort animode;

    if (p_person->U.UPerson.CurrentWeapon != WEP_NULL)
        animode = gun_out_anim(p_person, 0);
    else
        animode = ANIM_PERS_IDLE;
    set_person_anim_mode(p_person, animode);
}

struct Thing *new_sim_person(int x, int y, int z, ubyte subtype)
{
#if 0
    struct Thing *p_person;
    asm volatile ("call ASM_new_sim_person\n"
        : "=r" (p_person) : "a" (x), "d" (y), "b" (z), "c" (subtype));
    return p_person;
#endif
    ubyte ptype;
    struct PeepStat *p_pestat;
    ThingIdx person;
    struct Thing *p_person;
    ushort rnd;
    int ry;

    ptype = subtype;
    if (subtype <= 100) {
        person = get_new_thing();
    } else {
        ptype = SubTT_PERS_AGENT;
        person = unkn01_thing_idx;
    }
    if (person <= 0)
        return &things[0];
    p_pestat = &peep_type_stats[ptype];
    rnd = LbRandomAnyShort();

    p_person = &things[person];
    p_person->X = MAPCOORD_TO_PRCCOORD(x,0);
    p_person->Z = MAPCOORD_TO_PRCCOORD(z,0);
    p_person->U.UPerson.Target2 = 0;
    ry = alt_at_point(x, z);
    p_person->Speed = 512;
    p_person->U.UPerson.Angle = (rnd >> 1) % 8;
    p_person->U.UPerson.AnimMode = ANIM_PERS_IDLE;
    p_person->StartTimer1 = 48;
    p_person->Timer1 = 48;
    p_person->Y = ry;
    p_person->U.UPerson.OnFace = 0;
    p_person->Type = TT_PERSON;
    p_person->Radius = calc_person_radius_type(p_person, ptype);
    p_person->U.UPerson.CurrentWeapon = person_type_get_favourite_weapon(ptype);
    switch (ptype)
    {
    case SubTT_PERS_AGENT:
        p_person->U.UPerson.Angle = 0;
        break;
    case SubTT_PERS_PUNK_F:
        p_person->U.UPerson.FrameId.Version[0] = rnd % 3;
        break;
    default:
        break;
    }
    p_person->SubType = ptype;
    p_person->U.UPerson.Group = 4 + ptype;
    p_person->U.UPerson.EffectiveGroup = p_person->U.UPerson.Group;
    person_reset_default_anim_mode(p_person);
    init_person_thing(p_person);
    p_person->U.UPerson.WeaponsCarried = 0;
    if (p_person->U.UPerson.CurrentWeapon)
        p_person->U.UPerson.WeaponsCarried |= (1 << (p_person->U.UPerson.CurrentWeapon - 1));
    p_person->U.UPerson.WeaponsCarried |= (1 << (WEP_ENERGYSHLD-1));
    p_person->U.UPerson.EffectiveGroup = p_person->U.UPerson.Group;
    p_person->Speed = p_pestat->Speed;
    add_node_thing(person);
    p_person->U.UPerson.ComHead = 0;
    p_person->U.UPerson.ComCur = 0;
    return p_person;
}

void should_i_get_item(struct Thing *p_person, struct SimpleThing *p_sthing)
{
    asm volatile ("call ASM_should_i_get_item\n"
        : : "a" (p_person), "d" (p_sthing));
}

/** Checks one tile for collisions. `found` is what earlier tiles found:
 * the original keeps a single result for all tiles, and a person or static
 * already found stops further bumps.
 */
ThingIdx check_for_other_people_on_tile(int x, int y, int z,
  short tile_x, short tile_z, struct Thing *p_person, ThingIdx found)
{
    ThingIdx thing;
    uint k;

    thing = get_mapwho_thing_index(tile_x, tile_z);
    k = 0;
    while (thing != 0)
    {
        if (thing <= 0)
        {
            struct SimpleThing *p_sthing;

            p_sthing = &sthings[thing];
            if (p_sthing->Type == SmTT_DROPPED_ITEM)
            {
                if ((p_person->Flag & TngF_Persuaded) != 0)
                    should_i_get_item(p_person, p_sthing);

                {
                    int tdy;

                    tdy = PRCCOORD_TO_MAPCOORD(p_sthing->Y - y);
                    if ((tdy < 25) && (tdy > -25))
                    {
                        int tdx, tdz, dist, radsum;

                        tdx = PRCCOORD_TO_MAPCOORD(p_sthing->X - x);
                        tdz = PRCCOORD_TO_MAPCOORD(p_sthing->Z - z);
                        dist = map_distance_deltas_fast(tdx, tdy, tdz);
                        radsum = p_sthing->Radius + p_person->Radius;
                        if (dist < radsum)
                        {
                            p_person->Flag |= TngF_Unkn08000000;
                            p_person->U.UPerson.StandOnThing = p_sthing->ThingOffset;
                        }
                    }
                }
            }
            else if (p_sthing->Type == SmTT_STATIC)
            {
                if ((found == 0) && (p_sthing->SubType != 1) &&
                  (p_person->U.UPerson.BumpCount < 2))
                {
                    int tdy;

                    tdy = PRCCOORD_TO_MAPCOORD(p_sthing->Y - y);
                    if ((tdy < 25) && (tdy > -25))
                    {
                        int tdx, tdz, dist, rad;

                        tdx = PRCCOORD_TO_MAPCOORD(p_sthing->X - x);
                        tdz = PRCCOORD_TO_MAPCOORD(p_sthing->Z - z);
                        dist = map_distance_deltas_fast(tdx, tdy, tdz);
                        rad = static_radii[p_sthing->StartFrame - 999] / 2;
                        if (rad != 0)
                        {
                            int radsum;
                            radsum = rad + p_person->Radius;
                            if (dist < radsum)
                                found = p_sthing->ThingOffset;
                        }
                    }
                }
            }
            thing = p_sthing->Next;
        }
        else
        {
            struct Thing *p_thing;

            p_thing = &things[thing];
            if (p_thing->Type == TT_PERSON)
            {
                if (((p_thing->Flag & (TngF_Destroyed | TngF_PlayerAgent)) == 0) &&
                  ((p_person->Flag & TngF_Persuaded) == (p_thing->Flag & TngF_Persuaded)) &&
                  (p_person->State != PerSt_WANDER) &&
                  ((p_person->Flag & TngF_PlayerAgent) == 0) &&
                  (p_thing != p_person) &&
                  (p_thing->State != PerSt_DEAD) &&
                  (found == 0))
                {
                    int tdy;

                    tdy = PRCCOORD_TO_MAPCOORD(p_thing->Y - y);
                    if ((tdy < 25) && (tdy > -25))
                    {
                        int tdx, tdz, dist, radsum;

                        tdx = PRCCOORD_TO_MAPCOORD(p_thing->X - x);
                        tdz = PRCCOORD_TO_MAPCOORD(p_thing->Z - z);
                        dist = 2 * map_distance_deltas_fast(tdx, tdy, tdz);
                        radsum = p_thing->Radius + p_person->Radius;
                        if ((dist < radsum) &&
                          (((p_person->Flag & TngF_Persuaded) == 0) || (p_person >= p_thing)))
                        {
                            ubyte bump;

                            bump = p_person->U.UPerson.BumpMode;
                            if (bump == 0)
                                bump = p_person->U.UPerson.Angle;
                            bump++;
                            p_thing->U.UPerson.BumpMode = bump;
                            found = p_thing->ThingOffset;
                        }
                    }
                }
            }
            else if (p_thing->Type == TT_VEHICLE)
            {
                if (p_thing->SubType != VehOM_CLAWTANK)
                {
                    int tdy;

                    tdy = PRCCOORD_TO_MAPCOORD(p_thing->Y - y);
                    if ((tdy < 25) && (tdy > -25))
                    {
                        int tdx, tdz, dist, radsum;

                        tdx = PRCCOORD_TO_MAPCOORD(p_thing->X - x);
                        tdz = PRCCOORD_TO_MAPCOORD(p_thing->Z - z);
                        dist = map_distance_deltas_fast(tdx, tdy, tdz);
                        radsum = p_person->Radius + (p_thing->Radius - (p_thing->Radius >> 2));
                        if (dist < radsum)
                        {
                            p_person->Flag &= ~(TngF_Unkn08000000|TngF_Unkn02000000);
                            p_person->U.UPerson.StandOnThing = p_thing->ThingOffset;
                            p_person->Flag |= TngF_Unkn01000000;
                            if (!can_i_enter_vehicle(p_person, p_thing))
                                found = p_thing->ThingOffset;
                        }
                    }
                }
            }
            thing = p_thing->Next;
        }
        k++;
        if (k >= MAX_THINGS_ON_TILE) {
            bad_mapwho_tl_x = tile_x;
            bad_mapwho_tl_z = tile_z;
            break;
        }
    }

    return found;
}

short check_for_other_people(int x, int y, int z, struct Thing *p_person)
{
    short tile_x_center, tile_z_center;
    short dx, dz;
    ThingIdx found;

    p_person->Flag &= ~(TngF_StandOnVehicle | TngF_Unkn08000000);
    found = 0;

    if ((x < 0) || (x >= 0x800000) || (z < 0) || (z >= 0x800000))
        return 1;

    tile_x_center = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(x));
    tile_z_center = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(z));

    for (dx = -1; dx <= 1; dx++)
    {
        short cur_tile_x;

        cur_tile_x = tile_x_center + dx;
        if ((cur_tile_x <= 0) || (cur_tile_x >= MAP_TILE_WIDTH))
            continue;

        for (dz = -1; dz <= 1; dz++)
        {
            short cur_tile_z;

            cur_tile_z = tile_z_center + dz;
            if ((cur_tile_z <= 0) || (cur_tile_z >= MAP_TILE_HEIGHT))
                continue;

            found = check_for_other_people_on_tile(x, y, z,
              cur_tile_x, cur_tile_z, p_person, found);
        }
    }
    return found;
}

void check_persons_target(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_check_persons_target\n"
        : : "a" (p_person));
#endif
    struct Thing *p_target;
    int range;

    range = get_weapon_range(p_person) + 256;
    if (range < 1024) {
        return;
    }

    p_target = p_person->PTarget;

    // Handle shooting dead targets
    if ((p_target == NULL) || ((p_target->Flag & TngF_Destroyed) != 0))
    {
        if (p_person->Type == TT_MINE)
            p_person->PTarget = NULL;

        if ((p_person->Flag & TngF_SelectedAgent) == 0)
            p_person->Flag &= ~TngF_TriggerUse;
    }

    // If lost target, no need for any more checks
    if (p_target == NULL) {
        return;
    }

    // If stopped shooting, no need for any more checks
    if (((p_person->Flag & TngF_TriggerUse) == 0) &&
      ((p_person->Flag & TngF_WepCharging) == 0)) {
        return;
    }

    // Handle shooting allies
    if (things_check_same_group(p_person->ThingOffset, p_target->ThingOffset))
    {
        p_person->PTarget = NULL;
        p_person->Flag &= ~TngF_TriggerUse;
        return;
    }

    // Handle shooting out of range
    if (!person_has_weapon_target_within_range(p_person, p_target->ThingOffset))
    {
        if (p_person->Type == TT_MINE)
            p_person->PTarget = NULL;
        // If the player wants to shoot toward an outranged target, he can.
        // Other agents or people should release the trigger in such case.
        if (!thing_is_player_agent_under_direct_control(p_person->ThingOffset))
            p_person->Flag &= ~TngF_TriggerUse;
        p_person->U.UPerson.Flag3 |= PrsF3_Unkn40;
    }
}

void check_persons_target2(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_check_persons_target2\n"
        : : "a" (p_person));
#endif
    struct Thing *p_target;
    int range;

    range = get_weapon_range(p_person) + 128;
    if (range < 1024) {
        return;
    }

    p_target = &things[p_person->U.UPerson.Target2];

    if ((p_target == NULL) || ((p_target->Flag & TngF_Destroyed) != 0)) {
        p_person->Flag &= ~TngF_TriggerUse;
        return;
    }

    // Handle shooting allies
    if (things_check_same_group(p_person->ThingOffset, p_target->ThingOffset))
    {
        p_person->U.UPerson.Target2 = 0;
        p_person->Flag &= ~TngF_TriggerUse;
        return;
    }

    if (!person_has_weapon_target_within_range(p_person, p_target->ThingOffset))
    {
        // If the player wants to shoot toward an outranged target, he can.
        // Other agents or people should release the trigger in such case.
        if (!thing_is_player_agent_under_direct_control(p_person->ThingOffset))
            p_person->Flag &= ~TngF_TriggerUse;
        p_person->U.UPerson.Flag3 |= PrsF3_Unkn40;
    }
}

TbBool person_is_other_players_agent(struct Thing *p_person, PlayerIdx plyr)
{
    return (((p_person->Flag & TngF_PlayerAgent) != 0) && (p_person->U.UPerson.ComCur >> 2 != plyr));
}

short person_shield_glow_brightness(struct Thing *p_thing)
{
    short br_inc;

    br_inc = 0;
    if ((p_thing->Flag & TngF_Destroyed) == 0)
    {
        if ((p_thing->Flag & TngF_Unkn00200000) != 0)
        {
            br_inc += 16;
            if (p_thing->U.UPerson.ShieldGlowTimer) {
                br_inc += 16;
            }
        }
    }

    return br_inc;
}

static DIFFTEST_REF_FUNC void process_stamina_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_process_stamina\n"
        : : "a" (p_person));
}

void process_stamina(struct Thing *p_person)
{
    s32 mood_limit;

    DIFFTEST_SHADOW(process_stamina, process_stamina_asm(p_person));
    if (p_person->Type != TT_PERSON)
        return;

    // TODO: division by zero if MaxStamina is 0, as in the original
    mood_limit = (88 * (s32)p_person->U.UPerson.Stamina) /
      (s32)p_person->U.UPerson.MaxStamina;
    if (p_person->U.UPerson.Mood > mood_limit)
    {
        p_person->U.UPerson.Mood = mood_limit;
    }
    else if (p_person->U.UPerson.Mood < -mood_limit)
    {
        p_person->U.UPerson.Mood = mood_limit;
        p_person->U.UPerson.Mood = -mood_limit;
    }

    if (p_person->U.UPerson.Stamina < p_person->U.UPerson.MaxStamina)
    {
        p_person->U.UPerson.Stamina = p_person->U.UPerson.Stamina +
          cybmod_chest_level(&p_person->U.UPerson.UMod) + 2;
    }
    if (p_person->U.UPerson.Stamina < 0)
        p_person->U.UPerson.Stamina = 0;
}

void process_shield(struct Thing *p_person)
{
#if 0
    asm volatile (
      "call ASM_process_shield\n"
        : : "a" (p_person));
#endif
    struct PeepStat *p_pestat;

    if (p_person->U.UPerson.ShieldEnergy < -300)
        p_person->U.UPerson.ShieldEnergy = -300;

    if (p_person->U.UPerson.ShieldGlowTimer > 0)
        p_person->U.UPerson.ShieldGlowTimer--;

    p_pestat = &peep_type_stats[p_person->SubType];

    if ((p_person->Flag & TngF_Unkn00200000) != 0)
    {
        p_person->U.UPerson.ShieldEnergy -= 16;
        if (p_person->U.UPerson.ShieldEnergy <= 0)
            p_person->Flag &= ~TngF_Unkn00200000;
    }
    else
    {
        p_person->U.UPerson.ShieldEnergy += 8;
        if (p_person->U.UPerson.ShieldEnergy > p_pestat->MaxShield)
            p_person->U.UPerson.ShieldEnergy = p_pestat->MaxShield;
    }

    if ((p_person->Flag & TngF_PersSupShld) != 0)
    {
        short delta, en_drain;

        delta = p_pestat->MaxShield - p_person->U.UPerson.ShieldEnergy;
        if (delta > 0)
        {
            en_drain = delta / PERSON_ENERGY_TO_SHIELD_MUL;
            if (en_drain > p_person->U.UPerson.Energy) {
                delta = PERSON_ENERGY_TO_SHIELD_MUL * p_person->U.UPerson.Energy;
                p_person->Flag &= ~TngF_PersSupShld;
            }
            en_drain = delta / PERSON_ENERGY_TO_SHIELD_MUL;
            p_person->U.UPerson.ShieldEnergy += delta;
            p_person->U.UPerson.Energy -= en_drain;
        }
    }
}

ubyte conditional_command_state_true(ushort cmd, struct Thing *p_me, ubyte from)
{
#if 0
    ubyte ret;
    asm volatile (
      "call ASM_conditional_command_state_true\n"
        : "=r" (ret) : "a" (cmd), "d" (p_me), "b" (from));
    return ret;
#endif
    struct Command *p_cmd;
    struct Thing *p_othertng;
    short tng_x, tng_y, tng_z;
    ubyte unmet;

    p_cmd = &game_commands[cmd];
    unmet = (p_cmd->Flags & PCmdF_RevertFunct) != 0;

    switch (p_cmd->Type)
    {
    case PCmd_WAIT_MEM_G_ARRIVE:
    case PCmd_WAND_MEM_G_ARRIVE:
    case PCmd_UNTIL_MEM_G_ARRIVE:
        if ((p_cmd->Flags & PCmdF_AreaIsRect) != 0)
        {
            if (mem_group_arrived_square2(p_me, p_cmd->OtherThing,
              p_cmd->X, p_cmd->Z, p_cmd->Arg1, p_cmd->Time, p_cmd->Arg2)) {
                return !unmet;
            }
        }
        else
        {
            if (mem_group_arrived(p_cmd->OtherThing,
              p_cmd->X, p_cmd->Y, p_cmd->Z, p_cmd->Arg1, p_cmd->Arg2)) {
                return !unmet;
            }
        }
        return unmet;

    case PCmd_WAIT_P_V_DEAD:
    case PCmd_WAND_P_V_DEAD:
    case PCmd_UNTIL_P_V_DEAD:
        p_othertng = &things[p_cmd->OtherThing];
        if (p_othertng->State == PerSt_DEAD)
        {
            return !unmet;
        }
        return unmet;

    case PCmd_WAIT_P_V_I_ARRIVE:
    case PCmd_WAND_P_V_I_ARRIVE:
    case PCmd_UNTIL_P_V_I_ARRIVE:
        if (thing_is_destroyed(p_cmd->OtherThing))
        {
            return unmet;
        }
        if ((p_cmd->Flags & PCmdF_AreaIsRect) != 0)
        {
            if (thing_arrived_at_obj_square2(p_cmd->OtherThing,
              p_cmd->X, p_cmd->Z, p_cmd->Arg1, p_cmd->Time)) {
                return !unmet;
            }
        }
        else
        {
            int radius_sq;
            radius_sq = (p_cmd->Arg1 * p_cmd->Arg1) << 12;
            if (thing_arrived_at_obj_radius(p_cmd->OtherThing,
              p_cmd->X, p_cmd->Y, p_cmd->Z, radius_sq)) {
                return !unmet;
            }
        }
        return unmet;

    case PCmd_WAIT_ALL_G_NEAR:
    case PCmd_WAND_ALL_G_NEAR:
    case PCmd_UNTIL_ALL_G_NEAR:
        if (all_group_arrived( p_cmd->OtherThing, PRCCOORD_TO_MAPCOORD(p_me->X),
          PRCCOORD_TO_MAPCOORD(p_me->Y), PRCCOORD_TO_MAPCOORD(p_me->Z), p_cmd->Arg1))
        {
            return !unmet;
        }
        return unmet;

    case PCmd_WAND_MEM_G_NEAR:
    case PCmd_WAIT_MEM_G_NEAR:
    case PCmd_UNTIL_MEM_G_NEAR:
        get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, p_me->ThingOffset);
        if (mem_group_arrived(p_cmd->OtherThing,
          tng_x, tng_y >> 3, tng_z, p_cmd->Arg1, p_cmd->Arg2))
        {
            return !unmet;
        }
        return unmet;

    case PCmd_WAIT_MEM_G_PERSUADE:
    case PCmd_WAND_MEM_G_PERSUADE:
    case PCmd_UNTIL_MEM_G_PERSUADE:
        if (group_actions[p_cmd->OtherThing].Persuaded >= p_cmd->Arg2)
            return !unmet;
        return unmet;

    case PCmd_WAIT_P_V_I_NEAR:
    case PCmd_WAND_P_V_I_NEAR:
    case PCmd_UNTIL_P_V_I_NEAR:
        if (thing_arrived_at_obj_radius(p_cmd->OtherThing,
          PRCCOORD_TO_MAPCOORD(p_me->X), PRCCOORD_TO_MAPCOORD(p_me->Y), PRCCOORD_TO_MAPCOORD(p_me->Z),
          ((p_cmd->Arg1) * (p_cmd->Arg1)) << 12))
        {
            return !unmet;
        }
        return unmet;

    case PCmd_WAIT_ALL_G_PERSUADE:
    case PCmd_WAND_ALL_G_PERSUADE:
    case PCmd_UNTIL_ALL_G_PERSUADE:
        if (group_actions[p_cmd->OtherThing].Persuaded == group_actions[p_cmd->OtherThing].Alive)
            return !unmet;
        return unmet;

    case PCmd_UNTIL_ALL_G_DEAD:
    case PCmd_WAIT_ALL_G_DEAD:
    case PCmd_WAND_ALL_G_DEAD:
        if (group_actions[p_cmd->OtherThing].Alive == 0)
        {
            return 1;
        }
        return unmet;

    case PCmd_UNTIL_TIME:
    case PCmd_WAND_TIME:
    case PCmd_WAIT_TIME:
        if (p_cmd->Time < 9999)
        {
            p_cmd->Arg1++;
            if (p_cmd->Arg1 >= p_cmd->Time) {
                p_cmd->Arg1 = 0;
                return 1;
            }
        }
        return unmet;

    case PCmd_UNTIL_G_NOT_SEEN:
        if (group_not_seen(p_cmd->OtherThing))
        {
            return !unmet;
        }
        if (group_actions[p_cmd->OtherThing].Alive == 0)
        {
            return 1;
        }
        return unmet;

    case PCmd_WAIT_OBJV:
    case PCmd_WAND_OBJV:
    case PCmd_UNTIL_OBJV:
        return test_objective(p_cmd->OtherThing, 2);

    case PCmd_WAIT_OBJT_DESTROY:
    case PCmd_WAND_OBJT_DESTROY:
    case PCmd_UNTIL_OBJT_DESTROY:
        if (thing_is_destroyed(p_cmd->OtherThing))
        {
            return !unmet;
        }
        return unmet;

    case PCmd_WAIT_P_PERSUADE:
    case PCmd_WAND_P_PERSUADE:
    case PCmd_UNTIL_P_PERSUADE:
        p_othertng = &things[p_cmd->OtherThing];
        if ((p_othertng->Flag & TngF_Persuaded) != 0)
            return !unmet;
        return unmet;

    case PCmd_WAIT_MEM_G_DEAD:
    case PCmd_WAND_MEM_G_DEAD:
    case PCmd_UNTIL_MEM_G_DEAD:
        if (group_actions[p_cmd->OtherThing].Dead >= p_cmd->Arg2)
            return 1;
        return unmet;

    case PCmd_WAIT_ALL_G_ARRIVE:
    case PCmd_WAND_ALL_G_ARRIVE:
    case PCmd_UNTIL_ALL_G_ARRIVE:
        if ((p_cmd->Flags & PCmdF_AreaIsRect) != 0)
        {
            if (all_group_arrived_square(p_cmd->OtherThing,
              p_cmd->X, p_cmd->Z, p_cmd->Arg1, p_cmd->Time)) {
                return !unmet;
            }
        }
        else
        {
            if (all_group_arrived(p_cmd->OtherThing,
              p_cmd->X, p_cmd->Y, p_cmd->Z, p_cmd->Arg1)) {
                return !unmet;
            }
        }
        return unmet;

    case PCmd_NONE:
    case PCmd_STAY:
    case PCmd_GO_TO_POINT:
        return 0;

    default:
        break;
    }
    return unmet;
}

void person_init_drop_special(struct Thing *p_person, ThingIdx item)
{
    asm volatile ("call ASM_person_init_drop_special\n"
        : : "a" (p_person), "d" (item));
}

int build_tank_path(struct Thing *p_thing, int x, int z)
{
    int ret;
    asm volatile (
      "call ASM_build_tank_path\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z));
    return ret;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void veh_heap_add(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_veh_heap_add\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void VNAV_do(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_do\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void VNAV_find_dropoff_point(int a1, int a2, int a3, int a4, int a5)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[1];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    asm volatile ("push 0(%4)\n""call ASM_VNAV_find_dropoff_point\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int VNAV_can_i_get_there(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_can_i_get_there\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
    return ra;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int VNAV_goto(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_goto\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void VNAV_goto_and_unpark(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_goto_and_unpark\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "edi", "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void VNAV_goto_and_unplace(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_goto_and_unplace\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "esi", "edi", "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void VNAV_goto_and_unstop(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_VNAV_goto_and_unstop\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int func_5fe50_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_func_5fe50\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

extern s32 dword_1AA3AC;
extern s32 dword_1AA3B0;

/** Returns true if the map tile at given coords has a collision column
 * with bit 0x40 set in any of its QBits.
 */
static TbBool func_5fe50_tile_blocked(s32 cor_x, s32 cor_z)
{
    struct ColColumn *p_ccol;
    ushort ccol;

    ccol = game_my_big_map[(cor_z >> 8) * 128 + (cor_x >> 8)].ColumnHead & COL_COLUMN_MASK;
    if (ccol == 0)
        return false;
    p_ccol = &game_col_columns[ccol];
    return ((p_ccol->QBits[0] | p_ccol->QBits[1] | p_ccol->QBits[2] | p_ccol->QBits[3]) & 0x40) != 0;
}

int func_5fe50(int a1, int a2, int a3, int a4)
{
    s32 x, z, dt_x, dt_z, adt_x, adt_z;
    s32 dist, n, wid_x, wid_z;

    DIFFTEST_SHADOW_RET(func_5fe50, int, func_5fe50_asm(a1, a2, a3, a4));
    dt_x = a3 - a1;
    dt_z = a4 - a2;
    adt_z = abs(dt_z);
    adt_x = abs(dt_x);
    if (adt_x < adt_z)
        dist = adt_z - (adt_z >> 5) - (adt_z >> 7)
          + (adt_x >> 2) + (adt_x >> 3) + (adt_x >> 6) + (adt_x >> 7);
    else
        dist = adt_x - (adt_x >> 5) - (adt_x >> 7)
          + (adt_z >> 2) + (adt_z >> 3) + (adt_z >> 6) + (adt_z >> 7);

    dword_1AA3AC = a1;
    dword_1AA3B0 = a2;
    if (dist < 0x100)
        DIFFTEST_RETURN((int)0);

    n = dist >> 7;
    dt_x /= n;
    dt_z /= n;
    wid_x = 2 * dt_z;
    wid_z = 2 * dt_x;
    x = a1;
    z = a2;
    while (1)
    {
        n--;
        if (n == -1)
            DIFFTEST_RETURN((int)0x2a);
        if (func_5fe50_tile_blocked(x, z))
            DIFFTEST_RETURN((int)0);
        if (func_5fe50_tile_blocked(x + wid_x, z - wid_z))
            DIFFTEST_RETURN((int)0);
        if (func_5fe50_tile_blocked(x - wid_x, z + wid_z))
            DIFFTEST_RETURN((int)0);
        dword_1AA3AC = x - dt_x;
        x += dt_x;
        dword_1AA3B0 = z - dt_z;
        z += dt_z;
    }
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_bak(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_bak\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_given(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_given\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_unkn3(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_unkn3\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_unkn4(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_unkn4\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int ma_triangle_route(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_ma_triangle_route\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int ma_triangle_route_5(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_ma_triangle_route_5\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void path_init8_wide(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[2];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    asm volatile ("push 4(%4)\n""push 0(%4)\n""call ASM_path_init8_wide\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int ariadne_initialise_route(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_ariadne_initialise_route\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void send_ship_to(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_send_ship_to\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}








static DIFFTEST_REF_FUNC int start_goto_vehicle_asm(struct Thing *p_vehicle, int x, int z)
{
    int ret;
    asm volatile (
      "call ASM_start_goto_vehicle\n"
        : "=a" (ret) : "a" (p_vehicle), "d" (x), "b" (z));
    return ret;
}

int start_goto_vehicle(struct Thing *p_vehicle, int x, int z)
{
    struct MyPath *p_path;
    short goto_x, goto_z;
    int ret;

    DIFFTEST_SHADOW_RET(start_goto_vehicle, int,
      start_goto_vehicle_asm(p_vehicle, x, z));

    if ((ushort)p_vehicle->State == 0x45)
        DIFFTEST_RETURN(0);

    if (x < 0)
        x = 0x80;
    else if (x >= 0x8000)
        x = 0x7f80;
    if (z < 0)
        z = 0x80;
    else if (z >= 0x8000)
        z = 0x7f80;

    switch (p_vehicle->SubType)
    {
    case 0x28:
        if (x < 0x200)
            x = 0x200;
        if (z < 0x200)
            z = 0x200;
        if (x > 0x7e00)
            x = 0x7e00;
        if (z > 0x7e00)
            z = 0x7e00;
        if (p_vehicle->U.UVehicle.TNode >= 0)
            DIFFTEST_RETURN(0);

        if ((ushort)p_vehicle->State == 0x36)
        {
            if (!VNAV_can_i_get_there((int)p_vehicle, (short)x, (short)z, (int)p_vehicle))
                DIFFTEST_RETURN(0);
            VNAV_goto_and_unstop((int)p_vehicle, (short)x, (short)z, (int)p_vehicle);
            p_vehicle->Flag &= ~0x04;
            DIFFTEST_RETURN(0);
        }
        if ((ushort)p_vehicle->State == 0x3d)
        {
            if (!VNAV_can_i_get_there((int)p_vehicle, (short)x, (short)z, (int)p_vehicle))
                DIFFTEST_RETURN(0);
            VNAV_goto_and_unplace((int)p_vehicle, (short)x, (short)z, (int)p_vehicle);
            p_vehicle->Flag &= ~0x04;
            DIFFTEST_RETURN(0);
        }
        if ((ushort)p_vehicle->State == 0x21)
        {
            if (!VNAV_can_i_get_there((int)p_vehicle, (short)x, (short)z, (int)p_vehicle))
                DIFFTEST_RETURN(0);
            VNAV_goto_and_unpark((int)p_vehicle, (short)x, (short)z, (int)p_vehicle);
            p_vehicle->Flag &= ~0x04;
            DIFFTEST_RETURN(0);
        }
        ret = VNAV_goto((int)p_vehicle, (short)x, (short)z, (int)p_vehicle);
        if (ret == 1)
        {
            p_vehicle->State = 0x32;
            p_vehicle->U.UVehicle.GotoY = 0;
            p_vehicle->U.UVehicle.GotoX = x;
            p_vehicle->U.UVehicle.GotoZ = z;
            p_vehicle->Flag &= ~0x04;
            DIFFTEST_RETURN(1);
        }
        if (ret == 2)
            DIFFTEST_RETURN(1);
        if ((ushort)p_vehicle->State == 0x36)
            DIFFTEST_RETURN(0);
        p_vehicle->State = 0x3c;
        DIFFTEST_RETURN(0);

    case 0x32:
        remove_path(p_vehicle);
        if (ariadne_initialise_route((int)p_vehicle, x, z, (int)p_vehicle) <= 0)
            DIFFTEST_RETURN(0);
        unkn_path_func_001(p_vehicle, 1);
        p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
        goto_x = p_path->X[p_vehicle->PathOffset & 3];
        p_vehicle->U.UVehicle.GotoX = goto_x;
        p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
        goto_z = p_path->Z[p_vehicle->PathOffset & 3];
        p_vehicle->State = 0x39;
        p_vehicle->U.UVehicle.TNode = 0;
        p_vehicle->U.UVehicle.GotoZ = goto_z;
        DIFFTEST_RETURN(1);

    case 0x33:
    case 0x3b:
        remove_path(p_vehicle);
        if (build_tank_path(p_vehicle, x, z) == 0)
            DIFFTEST_RETURN(0);
        p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
        goto_x = p_path->X[p_vehicle->PathOffset & 3];
        p_vehicle->U.UVehicle.GotoX = goto_x;
        p_path = &my_paths[p_vehicle->U.UVehicle.PathIndex];
        goto_z = p_path->Z[p_vehicle->PathOffset & 3];
        p_vehicle->State = 0x32;
        p_vehicle->U.UVehicle.GotoZ = goto_z;
        DIFFTEST_RETURN(1);

    case 0x36:
        send_ship_to((int)p_vehicle, (short)x, (short)z, (int)p_vehicle);
        DIFFTEST_RETURN(0);

    default:
        DIFFTEST_RETURN(0);
    }
}

short find_peep_in_area(struct Thing *p_me, struct Command *p_cmd)
{
    short ret;
    asm volatile (
      "call ASM_find_peep_in_area\n"
        : "=r" (ret) : "a" (p_me), "d" (p_cmd));
    return ret;
}

void set_person_animmode_run(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_set_person_animmode_run\n"
        : : "a" (p_person));
#endif
    if (p_person->State == PerSt_PERSON_BURNING) {
        return;
    }
    if ((p_person->Flag & TngF_Destroyed) != 0 || (p_person->Flag2 & TgF2_KnockedOut) != 0) {
        return;
    }
    if (p_person->U.UPerson.AnimMode != ANIM_PERS_Unkn19) {
        set_person_anim_mode(p_person, ANIM_PERS_Unkn19);
    }
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    p_person->Flag2 |= TgF2_Unkn00080000;
    p_person->Speed = calc_person_speed(p_person);
}

void set_xz_for_tri(struct SingleObjectFace3 *p_face, int *x, int *z)
{
#if 0
    asm volatile (
      "call ASM_set_xz_for_tri\n"
        :  : "a" (p_face), "d" (x), "b" (z));
    return;
#endif
    struct SingleObject *p_obj;
    struct SinglePoint *p_pt1;
    struct SinglePoint *p_pt0;
    struct SinglePoint *p_pt2;

    p_obj = &game_objects[p_face->Object];
    p_pt1 = &game_object_points[p_face->PointNo[1]];
    p_pt0 = &game_object_points[p_face->PointNo[0]];
    p_pt2 = &game_object_points[p_face->PointNo[2]];

    *x = p_obj->MapX + (p_pt1->X + p_pt0->X + p_pt2->X) / 3;
    *z = p_obj->MapZ + (p_pt1->Z + p_pt0->Z + p_pt2->Z) / 3;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int path_init8_thing(int a1, int a2, int a3, int a4, int a5, int a6)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[2];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    asm volatile ("push 4(%4)\n""push 0(%4)\n""call ASM_path_init8_thing\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
    return ra;
}


ushort build_navigate_path(struct Thing *p_thing, int x, int z, int face)
{
    ushort ret;
    asm volatile (
      "call ASM_build_navigate_path\n"
        : "=r" (ret) : "a" (p_thing), "d" (x), "b" (z), "c" (face));
    return ret;
}

static DIFFTEST_REF_FUNC ushort build_navigate_path_to_person_asm(struct Thing *p_thing, struct Thing *p_thing_to)
{
    ushort ret;
    asm volatile (
      "call ASM_build_navigate_path_to_person\n"
        : "=a" (ret) : "a" (p_thing), "d" (p_thing_to));
    return ret;
}

extern struct Path g_nav_data;
extern u32 nav_stats__ThisTurn;
extern s32 gamep_unknval_14;
extern s32 navi2_unkn_counter;

void path_init8_unkn3(struct Path *p_path, int ax8, int ay8, int bx8, int by8, int a6);
ubyte check_person_within(struct Command *p_cmd, int x, int z);
void count_fnavs(TbBool now_draw);
void set_xz_for_quad210(int a1, int a2, int a3, int a4);
void set_xz_for_quad_unkn03(int a1, int a2, int a3, int a4);
void use_nav_data(int a1, int a2, int a3, int a4);
void fatter_than_nothing(int a1, int a2, int a3, int a4);
int tri_path_init8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8);

/** Set the position of a face for navigation into x and z, depending on
 * the face index sign (positive - triangle, negative - quad).
 */
static void build_navigate_face_xz(struct Thing *p_thing, struct Thing *p_thing_to,
  short face, int *p_x, int *p_z, short *p_tringl)
{
    struct SingleObjectFace3 *p_face3;
    struct SingleObjectFace4 *p_face4;

    if (face > 0)
    {
        p_face3 = &game_object_faces3[face];
        *p_tringl = p_face3->UnknTringl;
        set_xz_for_tri(p_face3, p_x, p_z);
    }
    else if ((p_thing->Flag & TngF_Unkn0020) != 0)
    {
        p_face4 = &game_object_faces4[-face];
        *p_tringl = p_face4->UnknTringl2;
        set_xz_for_quad210((int)(intptr_t)p_face4, (int)(intptr_t)p_x,
          (int)(intptr_t)p_z, (int)(intptr_t)p_thing_to);
    }
    else
    {
        p_face4 = &game_object_faces4[-face];
        *p_tringl = p_face4->UnknTringl1;
        set_xz_for_quad_unkn03((int)(intptr_t)p_face4, (int)(intptr_t)p_x,
          (int)(intptr_t)p_z, (int)(intptr_t)p_thing_to);
    }
    *p_x <<= 8;
    *p_z <<= 8;
}

ushort build_navigate_path_to_person(struct Thing *p_thing, struct Thing *p_thing_to)
{
    struct Thing *p_vehicle;
    struct Command *p_cmd;
    struct SingleObjectFace4 *p_face4;
    int x0, z0, x1, z1;
    s32 dx, dz, dist;
    short face0, face1;
    ubyte zip;

    DIFFTEST_SHADOW_RET(build_navigate_path_to_person, ushort,
      build_navigate_path_to_person_asm(p_thing, p_thing_to));

    face0 = -1;
    face1 = -1;
    if ((p_thing->Flag & TngF_InVehicle) != 0)
    {
        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        if (p_vehicle->SubType != SubTT_VEH_TRAIN)
            DIFFTEST_RETURN((ushort)0);
    }
    if ((p_thing->Flag & TngF_PlayerAgent) == 0 && (p_thing->Flag2 & TgF2_Unkn00020000) != 0)
    {
        p_thing->Flag2 |= TgF2_Unkn00020000 | TgF2_Unkn0040;
        DIFFTEST_RETURN((ushort)0);
    }

    if (p_thing->U.UPerson.Within != 0)
    {
        p_cmd = &game_commands[p_thing->U.UPerson.ComCur];
        if (!check_person_within(p_cmd, p_thing_to->X >> 8, p_thing_to->Z >> 8))
        {
            s32 cx, cz;

            dx = (p_thing_to->X - p_thing->X) >> 8;
            cx = dx;
            if (cx > 0x800)
                cx = 0x800;
            if (cx < -0x800)
                cx = -0x800;
            // TODO the original clamps the X difference for Z as well; looks like a bug
            cz = dx;
            if (cz > 0x800)
                cz = 0x800;
            if (cz < -0x800)
                cz = -0x800;
            cz += p_thing->Z >> 8;
            cx += p_thing->X >> 8;
            DIFFTEST_RETURN(build_navigate_path(p_thing, (short)cx, (short)cz, 0));
        }
    }

    if ((p_thing->Flag2 & TgF2_Unkn0008) == 0)
        p_thing->U.UPerson.BumpCount = 0;
    p_thing->U.UPerson.LastDist = 0x7d00;
    p_thing->Flag2 |= TgF2_Unkn8000;

    if ((p_thing->X >> 8) < 0 || (p_thing->X >> 8) >= 0x8000 ||
      (p_thing->Z >> 8) < 0 || (p_thing->Z >> 8) >= 0x8000 ||
      (p_thing_to->X >> 8) < 0 || (p_thing_to->X >> 8) >= 0x8000 ||
      (p_thing_to->Z >> 8) < 0 || (p_thing_to->Z >> 8) >= 0x8000)
    {
        p_thing->Flag2 |= TgF2_Unkn00020000 | TgF2_Unkn0040;
        DIFFTEST_RETURN((ushort)0);
    }

    zip = game_my_big_map[((p_thing->Z >> 16) << 7) + (p_thing->X >> 16)].Zip;
    if (zip != 0 &&
      game_my_big_map[((p_thing_to->Z >> 16) << 7) + (p_thing_to->X >> 16)].Zip == zip)
    {
        g_fat_data.PathLength = 1;
        g_fat_data.WayPoints[0].x8 = p_thing_to->X;
        g_fat_data.WayPoints[0].y8 = p_thing_to->Z;
        gamep_unknval_14++;
        use_nav_data((int)(intptr_t)p_thing, 0, 0, 0);
        DIFFTEST_RETURN((ushort)1);
    }

    if (p_thing->U.UPerson.OnFace != 0)
    {
        build_navigate_face_xz(p_thing, p_thing_to, p_thing->U.UPerson.OnFace,
          &x0, &z0, &face0);
    }
    else
    {
        x0 = p_thing->X;
        z0 = p_thing->Z;
    }

    if (p_thing_to->U.UPerson.OnFace != 0)
    {
        build_navigate_face_xz(p_thing_to, p_thing_to, p_thing_to->U.UPerson.OnFace,
          &x1, &z1, &face1);
    }
    else if ((p_thing_to->Flag & TngF_InVehicle) != 0 &&
      things[p_thing_to->U.UPerson.Vehicle].SubType == SubTT_VEH_TRAIN)
    {
        p_vehicle = &things[p_thing_to->U.UPerson.Vehicle];
        if (p_vehicle->Type != TT_VEHICLE)
            DIFFTEST_RETURN((ushort)0);
        p_face4 = &game_object_faces4[(short)p_vehicle->U.UVehicle.ComHead];
        face1 = p_face4->UnknTringl2;
        set_xz_for_quad210((int)(intptr_t)p_face4, (int)(intptr_t)&x1,
          (int)(intptr_t)&z1, (int)(intptr_t)p_thing_to);
        x1 <<= 8;
        z1 <<= 8;
    }
    else
    {
        x1 = p_thing_to->X;
        z1 = p_thing_to->Z;
    }

    nav_stats__ThisTurn++;
    dx = abs(x0 - x1);
    dz = abs(z0 - z1);
    if (dx > dz)
        dist = dx + (dz >> 1);
    else
        dist = dz + (dx >> 1);

    if (dist < 0x100000 && face0 == -1 && face1 == -1 &&
      (p_thing->Flag & TngF_PlayerAgent) == 0 &&
      p_thing->U.UPerson.Target2 != 0 &&
      p_thing->U.UPerson.Within == 0 &&
      (p_thing->Flag & TngF_Persuaded) == 0 &&
      p_thing->SubType != SubTT_PERS_MECH_SPIDER &&
      (LbRandomAnyShort() & 1) != 0)
    {
        p_thing->U.UPerson.Flag3 |= 0x10;
        navi2_unkn_counter++;
        path_init8_unkn3(&g_nav_data, x0, z0, x1, z1, -2);
        set_person_animmode_run(p_thing);
    }
    else
    {
        p_thing->U.UPerson.Flag3 &= ~0x10;
        navi2_unkn_counter++;
        tri_path_init8(face0, face1, (int)(intptr_t)&g_nav_data, x0, z0, x1, z1, -2);
    }

    if (g_nav_data.PathLength == 0)
    {
        g_fat_data.ax8 = x0;
        g_fat_data.ay8 = z0;
        g_fat_data.bx8 = x1;
        g_fat_data.by8 = z1;
        g_fat_data.WayPoints[0].x8 = x1;
        g_fat_data.WayPoints[0].y8 = z1;
        g_fat_data.PathLength = 1;
        use_nav_data((int)(intptr_t)p_thing, 0, 0, 0);
        count_fnavs(0);
        p_thing->Flag2 |= TgF2_Unkn00020000 | TgF2_Unkn0040;
        DIFFTEST_RETURN((ushort)0);
    }

    fatter_than_nothing((int)(intptr_t)&g_nav_data, (int)(intptr_t)&g_fat_data, 0x80, 0);
    use_nav_data((int)(intptr_t)p_thing, 0, 0, 0);
    DIFFTEST_RETURN((ushort)1);
}

void build_navigate_path_to_face(struct Thing *p_thing, short face)
{
    asm volatile ("call ASM_build_navigate_path_to_face\n"
        : : "a" (p_thing), "d" (face));
}
static DIFFTEST_REF_FUNC void fatter_than_nothing_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_fatter_than_nothing\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Normalize a vector to length 256, with length approximated as
 * max + min/2; a zero length leaves the vector unchanged.
 */
static void fatter_normalize(s32 *p_x, s32 *p_y)
{
    s32 len;

    if (abs(*p_x) > abs(*p_y))
        len = (abs(*p_y) >> 1) + abs(*p_x);
    else
        len = (abs(*p_x) >> 1) + abs(*p_y);
    if (len != 0)
    {
        *p_x = (*p_x << 8) / len;
        *p_y = (*p_y << 8) / len;
    }
}

/** Routed asm (internal label). Copies the path in eax (struct Path) to
 * the path in edx, moving each inner way point outwards by the width in
 * ebx; sharp turns get an additional way point. The ecx register is unused.
 */
void fatter_than_nothing(int a1, int a2, int a3, int a4)
{
    struct Path *p_src = (struct Path *)a1;
    struct Path *p_dst = (struct Path *)a2;
    s32 width = a3;
    struct WayPoint *p_wp;
    struct MyMapElement *p_mapel;
    s32 prev_x, prev_y;
    s32 dx1, dy1, dx2, dy2;
    s32 len, dot, cross;
    s32 ox, oy, nx, ny;
    s32 i, out;

    DIFFTEST_SHADOW(fatter_than_nothing, fatter_than_nothing_asm(a1, a2, a3, a4));

    p_dst->ax8 = p_src->ax8;
    p_dst->ay8 = p_src->ay8;
    p_dst->bx8 = p_src->bx8;
    p_dst->by8 = p_src->by8;
    p_dst->PathLength = p_src->PathLength;

    prev_x = p_src->ax8;
    prev_y = p_src->ay8;
    out = 0;
    for (i = 0; i < p_src->PathLength - 1; i++, out++)
    {
        p_wp = &p_src->WayPoints[i];
        dx1 = p_wp[0].x8 - prev_x;
        dy1 = p_wp[0].y8 - prev_y;
        dx2 = p_wp[1].x8 - p_wp[0].x8;
        dy2 = p_wp[1].y8 - p_wp[0].y8;

        if (abs(dy1) >= abs(dx1))
            len = (abs(dx1) >> 1) + abs(dy1);
        else
            len = (abs(dy1) >> 1) + abs(dx1);
        if (len == 0)
            len = 1;
        dx1 = (dx1 << 8) / len;
        dy1 = (dy1 << 8) / len;

        if (abs(dy2) >= abs(dx2))
            len = (abs(dx2) >> 1) + abs(dy2);
        else
            len = (abs(dy2) >> 1) + abs(dx2);
        if (len == 0)
            len = 1;
        dx2 = (dx2 << 8) / len;
        dy2 = (dy2 << 8) / len;

        dot = dx1 * dx2 + dy1 * dy2;
        cross = dx1 * dy2 - dy1 * dx2;

        if (dot < -20000)
        {
            // Sharp turn - add two way points around the corner
            if (cross > 0) {
                ox = -dy2 - dx2;
                oy = dx2 - dy2;
            } else {
                ox = -dy1 + dx1;
                oy = dx1 + dy1;
            }
            fatter_normalize(&ox, &oy);
            ox *= width;
            oy *= width;
            p_dst->WayPoints[out].x8 = p_wp->x8 + ox;
            p_dst->WayPoints[out].y8 = p_wp->y8 + oy;
            out++;
            p_dst->PathLength++;

            if (cross < 0) {
                ox = -dy2 - dx2;
                oy = dx2 - dy2;
            } else {
                ox = dx1 - dy1;
                oy = dy1 + dx1;
            }
            fatter_normalize(&ox, &oy);
            ox *= width;
            oy *= width;
            p_dst->WayPoints[out].x8 = p_wp->x8 + ox;
            p_dst->WayPoints[out].y8 = p_wp->y8 + oy;
        }
        else
        {
            ox = dx1 - dx2;
            oy = dy1 - dy2;
            fatter_normalize(&ox, &oy);
            ox *= width;
            oy *= width;
            ny = p_wp->y8 + oy;
            nx = p_wp->x8 + ox;
            p_mapel = &game_my_big_map[((ny >> 16) << 7) + (nx >> 16)];
            if ((p_mapel->Flags2 & MEF2_Unkn04) != 0)
            {
                ox >>= 1;
                p_dst->WayPoints[out].x8 = p_wp->x8 + ox;
                // TODO the original adds the X offset to Y as well; looks like a bug
                p_dst->WayPoints[out].y8 = p_wp->y8 + ox;
            }
            else
            {
                p_dst->WayPoints[out].x8 = nx;
                p_dst->WayPoints[out].y8 = ny;
            }
        }
        prev_x = p_wp->x8;
        prev_y = p_wp->y8;
    }
    p_dst->WayPoints[out].x8 = p_src->WayPoints[i].x8;
    p_dst->WayPoints[out].y8 = p_src->WayPoints[i].y8;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void set_xz_for_quad123(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_xz_for_quad123\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void set_xz_for_quad210(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_xz_for_quad210\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void set_xz_for_quad_unkn03(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_xz_for_quad_unkn03\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
static DIFFTEST_REF_FUNC void use_nav_data_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_use_nav_data\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Copy the way points of g_fat_data into a chain of my_paths entries,
 * and attach that chain to the thing.
 * Routed asm (internal label); only a1 (the thing) is used.
 */
void use_nav_data(int a1, int a2, int a3, int a4)
{
    struct Thing *p_thing = (struct Thing *)a1;
    struct MyPath *p_mypath;
    ushort path, next_path;
    ushort i;

    DIFFTEST_SHADOW(use_nav_data, use_nav_data_asm(a1, a2, a3, a4));

    next_path = head_my_path;
    if (p_thing->U.UPerson.PathIndex != 0)
        unkn_path_func_001(p_thing, 0);

    if ((g_fat_data.PathLength >> 2) + (s32)path_count > 1596)
    {
        char locstr[60];
        // The message is formatted but never shown
        sprintf(locstr, "Pathlength == %d, path_count = %d",
          g_fat_data.PathLength, (s32)path_count);
        p_thing->U.UPerson.PathIndex = 0;
        head_my_path = next_path;
        return;
    }

    path = head_my_path;
    p_thing->U.UPerson.PathIndex = path;
    p_mypath = &my_paths[path];
    p_mypath->Flag = (ubyte)g_fat_data.PathLength;
    next_path = p_mypath->Next;
    p_mypath->Next = 0;
    path_count++;

    for (i = 0; (s32)i < g_fat_data.PathLength; i++)
    {
        if (((i & 3) == 0) && (i > 3))
        {
            my_paths[path].Next = next_path;
            path = next_path;
            p_mypath = &my_paths[path];
            next_path = p_mypath->Next;
            p_mypath->Next = 0;
            path_count++;
            p_mypath->Flag = (ubyte)g_fat_data.PathLength;
        }
        p_mypath = &my_paths[path];
        p_mypath->Z[i & 3] = g_fat_data.WayPoints[i].y8 >> 8;
        p_mypath->X[i & 3] = g_fat_data.WayPoints[i].x8 >> 8;
    }
    p_thing->PathOffset = 0;
    head_my_path = next_path;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_unkn5(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_unkn5\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int triangle_route_do_unkn6(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_triangle_route_do_unkn6\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int ma_triangle_route_3(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_ma_triangle_route_3\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int tri_path_init8(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[4];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    stkargs[1] = a6;
    stkargs[2] = a7;
    stkargs[3] = a8;
    asm volatile ("push 12(%4)\n""push 8(%4)\n""push 4(%4)\n""push 0(%4)\n""call ASM_tri_path_init8\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
    return ra;
}







void build_navigate_path_to_face_xz(struct Thing *p_thing, short face, int x, int z)
{
    asm volatile ("call ASM_build_navigate_path_to_face_xz\n"
        : : "a" (p_thing), "d" (face), "b" (x), "c" (z));
}

/** Initialize drop item state.
 *
 * @param item The item to drop, SimpleThing if < 0 (mark in Flag2 required),
 *   weapon type in hand if 0, carried weapon type if > 0
 */
void person_init_drop(struct Thing *p_person, ThingIdx item)
{
#if 0
    asm volatile ("call ASM_person_init_drop\n"
        : : "a" (p_person), "d" (item));
#endif
    ubyte PrevAnimMode;

    if ((p_person->Flag & (TngF_Unkn40000000|TngF_WepRecoil|TngF_StationrSht|TngF_Destroyed)) != 0) {
        return;
    }

    p_person->U.UPerson.TempWeapon = item;
    p_person->Flag |= TngF_Unkn0001;
    p_person->State = PerSt_DROP_ITEM;
    p_person->U.UPerson.Timer2 = 3;

    PrevAnimMode = p_person->U.UPerson.AnimMode;
    set_person_anim_mode(p_person, ANIM_PERS_Unkn13);
    p_person->U.UPerson.OldAnimMode = PrevAnimMode;
}

void person_init_pickup(struct Thing *p_person, ThingIdx item)
{
    asm volatile ("call ASM_person_init_pickup\n"
        : : "a" (p_person), "d" (item));
}

static DIFFTEST_REF_FUNC void check_weapon_asm(struct Thing *p_person, int range)
{
    asm volatile ("call ASM_check_weapon\n"
        : : "a" (p_person), "d" (range));
}

void check_weapon(struct Thing *p_person, int range)
{
    DIFFTEST_SHADOW(check_weapon, check_weapon_asm(p_person, range));
    if (p_person->State == PerSt_PERSON_BURNING)
        return;
    if (p_person->U.UPerson.CurrentWeapon != 0)
        return;
    // TODO: the original ignores the range argument and always uses 0x500
    choose_best_weapon_for_range(p_person, 0x500);
    if (p_person->U.UPerson.CurrentWeapon == 0)
        return;
    p_person->U.UPerson.AnimMode = gun_out_anim(p_person, 0);
    p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
      + p_person->U.UPerson.Angle];
    p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
}

static DIFFTEST_REF_FUNC void get_weapon_out_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_get_weapon_out\n"
        : : "a" (p_person));
}

void get_weapon_out(struct Thing *p_person)
{
    DIFFTEST_SHADOW(get_weapon_out, get_weapon_out_asm(p_person));
    if (p_person->U.UPerson.CurrentWeapon != 0)
        return;
    choose_best_weapon_for_range(p_person, 0x500);
    if (p_person->U.UPerson.CurrentWeapon == 0)
        return;
    p_person->Frame -= nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
      + p_person->U.UPerson.Angle];
    p_person->U.UPerson.AnimMode = gun_out_anim(p_person, 0);
    p_person->Frame += nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
      + p_person->U.UPerson.Angle];
    p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
}

#define CMD_CONDITION_CONTINUE 0xFFFF

/** When one of "until" conditions of a command is met, give the next command.
 *
 * A command can have multiple "until" conditions below. Meeting at least one
 * of the conditions terminates the command.
 *
 * @return If loop should terminate and the next command should start, this call
 *   returns its index; otherwise, CMD_CONDITION_CONTINUE is returned.
 */
ushort person_command_until_check_condition(struct Thing *p_person, ushort cond_cmd)
{
    struct Command *p_cmd;
    ushort cmd;
    TbBool until_met, has_more_conditions;

    p_cmd = &game_commands[cond_cmd];
    cmd = p_cmd->Next;
    until_met = false;
    has_more_conditions = ((p_cmd->Flags & PCmdF_RunUntil) != 0);

    while (1)
    {
        if (cmd == 0)
            break;
        p_cmd = &game_commands[cmd];
        if ((p_cmd->Flags & PCmdF_RunUntil) == 0)
            has_more_conditions = false;
        if (!has_more_conditions) {
            if ((p_cmd->Flags & PCmdF_IsUntil) == 0)
                break;
            // After RunUntil commands and then IsUntil command, this loop should be broken
            // If it is not, then something is wrong with flags of the commands.
            if ((p_cmd->Flags & PCmdF_RunUntil) != 0) {
                LOGWARN("Person %s %d command %d has broken chain of UNTIL commands at %d",
                  person_type_name(p_person->SubType), (int)p_person->ThingOffset, cond_cmd, cmd);
            }
        }
        if (conditional_command_state_true(cmd, p_person, 3)) {
            if (gameturn <= 1) {
                LOGWARN("Person %s %d command %d end condition %d met just at start of the game",
                  person_type_name(p_person->SubType), (int)p_person->ThingOffset, cond_cmd, cmd);
            }
            if ((debug_log_things & 0x01) != 0) {
                char cond_locstr[192];
                char locstr[192];
                snprint_command(cond_locstr, sizeof(cond_locstr), cond_cmd);
                snprint_command(locstr, sizeof(locstr), cmd);
                LOGSYNC_F("Person %s %d %s %d ends, met %s %d, state %d.%d",
                  person_type_name(p_person->SubType), (int)p_person->ThingOffset,
                  cond_locstr, cond_cmd, locstr, cmd, p_person->State, p_person->SubState);
            }
            until_met = true;
        }
        cmd = p_cmd->Next;
    }

    if (!until_met)
        return CMD_CONDITION_CONTINUE;

    return cmd;
}

/** For given command, return the next command beyond any "until" conditions.
 *
 * @return Gives index of next command beyond "until" conditions, regardless
 *   whether any of the conditions is met or not.
 */
ushort person_command_until_skip_condition(struct Thing *p_person, ushort cond_cmd)
{
    struct Command *p_cmd;
    ushort cmd;
    TbBool has_more_conditions;

    p_cmd = &game_commands[cond_cmd];
    cmd = p_cmd->Next;
    has_more_conditions = ((p_cmd->Flags & PCmdF_RunUntil) != 0);

    while (1)
    {
        if (cmd == 0)
            break;
        p_cmd = &game_commands[cmd];
        if ((p_cmd->Flags & PCmdF_RunUntil) == 0)
            has_more_conditions = false;
        if (!has_more_conditions) {
            if ((p_cmd->Flags & PCmdF_IsUntil) == 0)
                break;
            if ((p_cmd->Flags & PCmdF_RunUntil) != 0) {
                LOGWARN("Person %s %d command %d has broken chain of UNTIL commands at %d",
                  person_type_name(p_person->SubType), (int)p_person->ThingOffset, cond_cmd, cmd);
            }
        }
        cmd = p_cmd->Next;
    }

    return cmd;
}

/** Select next command on a person.
 */
void person_command_select_next(struct Thing *p_person)
{
    ushort cmd;

    cmd = p_person->U.UPerson.ComCur;
    if (cmd == 0)
        return;

    p_person->U.UPerson.ComCur = person_command_until_skip_condition(p_person, cmd);
}

/** Discards a command at top of the person stack.
 * The command is then never used again.
 */
void person_command_skip_at_start(struct Thing *p_person)
{
    ushort cmd;

    cmd = p_person->U.UPerson.ComHead;
    if (cmd == 0)
        return;

    p_person->U.UPerson.ComHead = person_command_until_skip_condition(p_person, cmd);
}

/** Jump to specific command on a person.
 */
StateChRes person_command_jump(struct Thing *p_person, ushort cmd)
{
    p_person->State = PerSt_INIT_COMMAND;
    p_person->U.UPerson.ComCur = cmd;
    return StCh_ACCEPTED;
}

StateChRes person_init_cmd_wander(struct Thing *p_person, ubyte sstate)
{
    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;

        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        if ((p_vehicle->SubType == SubTT_VEH_GROUND) &&
          ((p_vehicle->Flag & TngF_Destroyed) == 0)) {
            p_vehicle->State = VehSt_WANDER;
        }
    }
    p_person->State = PerSt_WANDER;
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.Timer2 = -1;
    p_person->SubState = sstate;

    if (sstate == PCmd_NONE) {
        p_person->SubState = PCmd_WAND_TIME;
        p_person->U.UPerson.ComTimer = 9999;
        p_person->U.UPerson.Timer2 = 5;
        p_person->U.UPerson.StartTimer2 = 50;
    } else if ((p_person->Flag & TngF_InVehicle) == 0) {
        p_person->U.UPerson.Timer2 = 10;
        p_person->U.UPerson.StartTimer2 = 10;
    }
    return StCh_ACCEPTED;
}

StateChRes person_init_go_to_point(struct Thing *p_person, short x, short y,
  short z, ushort range, TbBool fast_run)
{
    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;

        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        p_vehicle->Flag &= ~TngF_PlayerAgent;
        start_goto_vehicle(p_vehicle, x, z);
        p_person->State = PerSt_WAIT_VEHICLE;
        return StCh_ACCEPTED;
    }

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);

    if (thing_arrived_at_obj_radius(p_person->ThingOffset, x, y, z, (range * range) << 12)) {
        p_person->State = PerSt_NONE;
        return StCh_ALREADY;
    }

    if (fast_run)
        set_person_animmode_run(p_person);

    p_person->State = PerSt_GOTO_POINT;
    p_person->U.UPerson.GotoX = x;
    p_person->U.UPerson.GotoZ = z;
    p_person->U.UPerson.ComRange = range;
    p_person->U.UPerson.ComTimer = -1;
    p_person->SubState = 0;
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    return StCh_ACCEPTED;
}

StateChRes person_init_go_to_person(struct Thing *p_person, short target,
  ushort range, TbBool fast_run)
{
    if (fast_run)
        set_person_animmode_run(p_person);

    p_person->State = PerSt_GOTO_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->GotoThingIndex = target;
    p_person->U.UPerson.Timer2 = 50;
    p_person->U.UPerson.StartTimer2 = 50;
    p_person->SubState = 0;
    p_person->U.UPerson.ComRange = range;
    return StCh_ACCEPTED;
}

StateChRes person_init_kill_person(struct Thing *p_person, short targetng)
{
    int weapon_range;

    p_person->Flag2 &= ~TgF2_IgnoreEnemies;
    p_person->GotoThingIndex = targetng;
    // Command to kill target 0 is equivalent to taking out weapon
    if (targetng == 0)
    {
        get_weapon_out(p_person);
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    check_weapon(p_person, 1280);
    p_person->State = PerSt_KILL_PERSON;
    p_person->PTarget = &things[targetng];
    p_person->U.UPerson.ComTimer = -1;
    p_person->SubState = 0;

    get_weapon_out(p_person);

    weapon_range = get_weapon_range(p_person);
    p_person->U.UPerson.ComRange = weapon_range >> 6;

    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;
    return StCh_ACCEPTED;
}

short person_get_command_range_for_persuade(struct Thing *p_person)
{
    int weapon_range;
    int cmd_range;

    if (person_carries_weapon(p_person, WEP_PERSUADER2))
        weapon_range = get_hand_weapon_range(p_person, WEP_PERSUADER2);
    else
        weapon_range = get_hand_weapon_range(p_person, WEP_PERSUADRTRN);

    cmd_range = (weapon_range >> 6) * 3 / 4;
    if (cmd_range < 1)
        cmd_range = 1;
    return cmd_range;
}

StateChRes person_init_persuade_person(struct Thing *p_person, short targetng)
{
    if (targetng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_PERSUADE_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->PTarget = &things[targetng];

    p_person->U.UPerson.ComRange = person_get_command_range_for_persuade(p_person);
    p_person->SubState = 0;

    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;
    return StCh_ACCEPTED;
}

StateChRes person_init_block_person(struct Thing *p_person, short targetng)
{
    if (targetng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_BLOCK_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->PTarget = &things[targetng];
    p_person->U.UPerson.ComRange = 2;
    p_person->U.UPerson.Timer2 = 20;
    p_person->U.UPerson.StartTimer2 = 20;
    p_person->SubState = 0;
    return StCh_ACCEPTED;
}

StateChRes person_init_scare_person(struct Thing *p_person, short targetng)
{
    if (targetng == 0)
    {
        get_weapon_out(p_person);
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_SCARE_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->PTarget = &things[targetng];
    p_person->U.UPerson.ComRange = 2;
    p_person->U.UPerson.Timer2 = 50;
    p_person->U.UPerson.StartTimer2 = 50;
    p_person->SubState = 0;
    get_weapon_out(p_person);
    return StCh_ACCEPTED;
}

void person_init_follow_person(struct Thing *p_person, struct Thing *p_other)
{
    asm volatile (
      "call ASM_person_init_follow_person\n"
        : : "a" (p_person), "d" (p_other));
}

StateChRes person_init_cmd_follow_person(struct Thing *p_person, short targetng)
{
    //TODO it would probably make sense to reuse person_init_follow_person() here
    if (targetng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_FOLLOW_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 1;
    p_person->U.UPerson.Timer2 = 50;
    p_person->U.UPerson.StartTimer2 = 50;
    p_person->SubState = 0;
    return StCh_ACCEPTED;
}

StateChRes person_init_support_person(struct Thing *p_person, short targetng)
{
    if (targetng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_SUPPORT_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 3;
    p_person->U.UPerson.Timer2 = 50;
    p_person->U.UPerson.StartTimer2 = 50;
    p_person->SubState = 0;
    return StCh_ACCEPTED;
}

StateChRes person_init_protect_person(struct Thing *p_person, short targetng, TbBool one_target)
{
    if ((targetng == 0) && one_target)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_PROTECT_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 8;
    p_person->U.UPerson.Timer2 = 50;
    p_person->U.UPerson.StartTimer2 = 50;
    p_person->SubState = 0;
    if (one_target)
        p_person->Owner = targetng;
    return StCh_ACCEPTED;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void set_get_item(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_set_get_item\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int send_another_player_peep(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_send_another_player_peep\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


void person_init_get_item(struct Thing *p_person, short item, ushort plyr)
{
    asm volatile ("call ASM_person_init_get_item\n"
        : : "a" (p_person), "d" (item), "b" (plyr));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void person_unkn_start_get_item(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_unkn_start_get_item\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void person_init_get_item_fast(struct Thing *p_person, short item, ushort plyr)
{
    asm volatile ("call ASM_person_init_get_item_fast\n"
        : : "a" (p_person), "d" (item), "b" (plyr));
}

StateChRes person_init_cmd_get_item(struct Thing *p_person, short targetng)
{
    short tgtng_x, tgtng_z;

    //TODO it would probably make sense to reuse person_init_get_item() here
    if (targetng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    get_thing_position_mapcoords(&tgtng_x, NULL, &tgtng_z, targetng);
    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_GET_ITEM;
    p_person->U.UPerson.GotoX = tgtng_x;
    p_person->U.UPerson.GotoZ = tgtng_z;
    p_person->U.UPerson.StandOnThing = 0;
    p_person->U.UPerson.ComTimer = -1;
    p_person->SubState = 0;
    p_person->U.UPerson.ComRange = 0;
    return StCh_ACCEPTED;
}

StateChRes person_init_cmd_use_weapon(struct Thing *p_person, short x, short y,
  short z, ushort weapon)
{
    struct WeaponDef *wdef;

    if (weapon == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    wdef = &weapon_defs[weapon];
    if (p_person->U.UPerson.Energy < wdef->EnergyUsed + 1)
        p_person->U.UPerson.Energy = wdef->EnergyUsed + 1;
    p_person->U.UPerson.CurrentWeapon = weapon;
    p_person->U.UPerson.WeaponDir = 32 * p_person->U.UPerson.Angle;
    thing_shoot_at_point(p_person, x, y, z, 0);
    process_weapon(p_person);
    p_person->Flag &= ~TngF_TriggerUse;
    return StCh_ACCEPTED;
}

StateChRes person_init_wand_avoid_group(struct Thing *p_person, ushort group, ubyte sstate)
{
    // Note we are not storing `group` - the one from `Command` will be used
    // (we should probably change that later though, state should not need to rely on command)
    p_person->State = PerSt_AVOID_GROUP;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.Timer2 = 5;
    p_person->U.UPerson.StartTimer2 = 5;
    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    p_person->SubState = sstate;
    return StCh_ACCEPTED;
}

StateChRes person_init_destroy_building(struct Thing *p_person, short x, short z, short targetng)
{
    int weapon_range;

    p_person->GotoThingIndex = targetng;
    p_person->State = PerSt_DESTROY_BUILDING;
    p_person->U.UPerson.ComTimer = -1;
    p_person->PTarget = &things[targetng];
    p_person->SubState = 0;

    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);

    get_weapon_out(p_person);

    // Make sure the target position is within current weapon range; our ComRange is in strange
    // units, they seem to be quarters of a tile; weapon_range is in normal map coords
    weapon_range = get_weapon_range(p_person);
    // Narrow range for drop weapons, wide range for throwing / shooting weapons
    if (((p_person->Flag & TngF_InVehicle) == 0) &&
      weapon_is_deployed_at_wielder_pos(p_person->U.UPerson.CurrentWeapon)) {
        p_person->U.UPerson.ComRange = (weapon_range >> 6);
    } else {
        p_person->U.UPerson.ComRange = (weapon_range >> 6) * 2 / 3;
    }
    //  the range needs to be larger than the person step distance
    p_person->U.UPerson.ComRange += PRCCOORD_TO_MAPCOORD(p_person->Speed + 255) >> 6;
    // Avoid zero range
    if (p_person->U.UPerson.ComRange < 2)
        p_person->U.UPerson.ComRange = 2;
    p_person->U.UPerson.GotoX = x;
    p_person->U.UPerson.GotoZ = z;

    return StCh_ACCEPTED;
}

StateChRes person_init_use_vehicle(struct Thing *p_person, short vehicle)
{
    short vhtng_x, vhtng_z;

    if (vehicle == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    get_thing_position_mapcoords(&vhtng_x, NULL, &vhtng_z, vehicle);
    p_person->GotoThingIndex = vehicle;
    p_person->State = PerSt_USE_VEHICLE;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 1;
    p_person->SubState = 0;
    p_person->U.UPerson.GotoX = vhtng_x;
    p_person->U.UPerson.GotoZ = vhtng_z;

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);
    return StCh_ACCEPTED;
}

StateChRes person_init_exit_vehicle(struct Thing *p_person)
{
    if ((p_person->Flag & TngF_InVehicle) == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_ALREADY;
    }
    person_attempt_to_leave_vehicle(p_person);
    return StCh_ACCEPTED;
}

StateChRes person_init_catch_train(struct Thing *p_person, short face)
{
    // Station coords were converted to face index during level load
    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);

    build_navigate_path_to_face(p_person, face);
    p_person->State = PerSt_CATCH_TRAIN;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 0;
    p_person->SubState = 0;

    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;
    return StCh_ACCEPTED;
}

StateChRes person_open_dome(struct Thing *p_person, short dome)
{
    struct Thing *p_dome;

    if ((dome == 0) || !building_can_transform_open(dome))
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_dome = &things[dome];
    if (p_dome->SubType == SubTT_BLD_DOME)
        p_dome->Flag |= TngF_TransOpenRq;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_close_dome(struct Thing *p_person, short dome)
{
    struct Thing *p_dome;

    if ((dome == 0) || !building_can_transform_open(dome))
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_dome = &things[dome];
    p_dome->Flag |= TngF_TransCloseRq;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_init_drop_item_where_standing(struct Thing *p_person, ThingIdx item)
{
    StateChRes res;

    if ((item >= 0) && (item < WEP_TYPES_COUNT))
    {
        person_init_drop(p_person, item);
        res = (p_person->State == PerSt_DROP_ITEM) ? StCh_ACCEPTED : StCh_DENIED;
    }
    else if ((item < 0) && thing_is_pickup_item(item))
    {
        person_init_drop(p_person, item);
        res = (p_person->State == PerSt_DROP_ITEM) ? StCh_ACCEPTED : StCh_DENIED;
    }
    else
    {
        LOGWARN("Drop called for unsupported item %d", (int)item);
        res = StCh_UNATTAIN;
    }
    return res;
}

StateChRes person_init_plant_mine_where_standing(struct Thing *p_person, WeaponType wtype)
{
    struct WeaponDef *wdef;

    if (!weapon_is_for_planting(wtype)) {
        LOGWARN("Weapon %s is not desgined for planting",
          weapon_codename(wtype));
        return StCh_UNATTAIN;
    }
    person_init_drop(p_person, wtype);

    if (p_person->State != PerSt_DROP_ITEM) {
        return StCh_DENIED;
    }
    wdef = &weapon_defs[wtype];
    p_person->Flag2 |= TgF2_DroppedActivate;
    p_person->U.UPerson.Energy -= wdef->EnergyUsed;
    p_person->U.UPerson.WeaponTurn = wdef->ReFireDelay;
    return StCh_ACCEPTED;
}

StateChRes person_lock_building(struct Thing *p_person, short bldng)
{
    struct Thing *p_building;

    if (bldng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_building = &things[bldng];
    p_building->Flag |= TngF_PassageLocked;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_unlock_building(struct Thing *p_person, short bldng)
{
    struct Thing *p_building;

    if (bldng == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    p_building = &things[bldng];
    p_building->Flag &= ~TngF_PassageLocked;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_cmd_select_weapon(struct Thing *p_person, ushort weapon)
{
    p_person->U.UPerson.CurrentWeapon = weapon;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_cmd_start_danger_music(struct Thing *p_person, TbBool revert)
{
    ubyte flag;

    flag = revert ? 0 : 1;
    update_danger_music(flag);
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_init_catch_ferry(struct Thing *p_person, short cor_x, short cor_z, ushort radius)
{
    p_person->State = PerSt_CATCH_FERRY;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 1;
    p_person->SubState = 0;
    p_person->U.UPerson.GotoX = cor_x;
    p_person->U.UPerson.GotoZ = cor_z;

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);
    return StCh_ACCEPTED;
}

StateChRes person_init_exit_ferry(struct Thing *p_person, short portbld)
{
    short prtng_x, prtng_z;

    if (portbld == 0)
    {
        p_person->State = PerSt_NONE;
        return StCh_UNATTAIN;
    }
    get_thing_position_mapcoords(&prtng_x, NULL, &prtng_z, portbld);
    p_person->State = PerSt_EXIT_FERRY;
    p_person->U.UPerson.ComTimer = -1;
    p_person->U.UPerson.ComRange = 1;
    p_person->SubState = 0;
    p_person->U.UPerson.GotoX = prtng_x;
    p_person->U.UPerson.GotoZ = prtng_z;

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);
    return StCh_ACCEPTED;
}

StateChRes person_or_vehicle_ping_exist(struct Thing *p_thing, TbBool revert)
{
    if (revert)
    {
        if (on_mapwho(p_thing))
            delete_node(p_thing);
        p_thing->Flag2 |= TgF2_ExistsOffMap;
    }
    else if ((p_thing->Flag2 & TgF2_ExistsOffMap) != 0)
    {
        p_thing->Flag2 &= ~TgF2_ExistsOffMap;
        if ((p_thing->Flag & TngF_InVehicle) == 0) {
            if (p_thing->Type == TT_PERSON)
                group_actions[p_thing->U.UPerson.Group].Alive++;
            add_node_thing(p_thing->ThingOffset);
        }
    }
    return StCh_ACCEPTED;
}

StateChRes person_guard_switch(struct Thing *p_person, TbBool revert)
{
    if ((revert && ((p_person->Flag & TngF_Unkn0008) == 0)) ||
      (!revert && ((p_person->Flag & TngF_Unkn0008) != 0)))
    {
        p_person->State = PerSt_NONE;
        return StCh_ALREADY;
    }
    if (revert)
    {
        p_person->Flag &= ~TngF_Unkn0008;
    }
    else
    {
        p_person->Flag |= TngF_Unkn0008;
    }
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_init_go_to_point_face(struct Thing *p_person, short x,
  short z, short face, ushort range)
{
    if ((p_person->Flag & TngF_InVehicle) != 0) {
        p_person->State = PerSt_NONE;
        return StCh_DENIED;
    }

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);

    build_navigate_path_to_face_xz(p_person, -face, x, z);
    p_person->U.UPerson.GotoX = x;
    p_person->U.UPerson.GotoZ = z;
    p_person->U.UPerson.ComRange = range;
    p_person->U.UPerson.ComTimer = -1;
    p_person->State = PerSt_GOTO_POINT;
    p_person->SubState = 0;

    p_person->Timer1 = 48;
    p_person->StartTimer1 = 48;
    return StCh_ACCEPTED;
}

StateChRes person_make_hard_as_agent(struct Thing *p_person)
{
    set_person_health_shield_type(p_person, SubTT_PERS_AGENT);
    set_person_energy_stamina_type(p_person, SubTT_PERS_AGENT);
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_make_fit_as_agent(struct Thing *p_person)
{
    set_person_energy_stamina_type(p_person, SubTT_PERS_AGENT);
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_init_cmd_wait_patiently(struct Thing *p_person, ubyte sstate)
{
    p_person->State = PerSt_WAIT;
    p_person->U.UPerson.ComTimer = -1;
    p_person->SubState = sstate;
    return StCh_ACCEPTED;
}

StateChRes person_init_cmd_wait_wth_timeout(struct Thing *p_person,
  ubyte sstate, short timeout)
{
    p_person->State = PerSt_WAIT;
    p_person->U.UPerson.ComTimer = timeout;
    p_person->SubState = sstate;
    return StCh_ACCEPTED;
}

StateChRes person_cmd_ignore_enemies(struct Thing *p_person, TbBool revert)
{
    if ((revert && ((p_person->Flag2 & TgF2_IgnoreEnemies) == 0)) ||
      (!revert && ((p_person->Flag2 & TgF2_IgnoreEnemies) != 0)))
    {
        p_person->State = PerSt_NONE;
        return StCh_ALREADY;
    }
    if (revert)
    {
        p_person->Flag2 &= ~TgF2_IgnoreEnemies;
    }
    else
    {
        p_person->Flag2 |= TgF2_IgnoreEnemies;
    }
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_cmd_stay_within_area(struct Thing *p_person, ushort cmd)
{
    if (p_person->U.UPerson.Within == cmd)
    {
        p_person->State = PerSt_NONE;
        return StCh_ALREADY;
    }
    p_person->U.UPerson.Within = cmd;
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

StateChRes person_cmd_play_sample(struct Thing *p_person, short smptbl_id,
  TbBool revert)
{
    if (revert)
    {
        play_sample_using_heap(0, smptbl_id, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
    }
    else
    {
        play_dist_sample(p_person, smptbl_id, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 1);
    }
    p_person->State = PerSt_NONE;
    return StCh_ACCEPTED;
}

void camera_track_thing(short thing, TbBool revert)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];

    if (revert)
    {
        ThingIdx dcthing;
        dcthing = p_locplayer->DirectControl[0];
        LOGDBG("Revert track to dcthing offs=%d", (int)dcthing);
        ingame.TrackThing = 0;
        game_set_cam_track_thing_xz(dcthing);
        return;
    }
    LOGDBG("Begin track thing offs=%d", (int)thing);
    ingame.TrackThing = thing;
    // Disallow camera movement being blocked by user modes like goto point
    reset_user_input();
}

void camera_rotate_view(short ang1, short ang2, TbBool revert)
{
    if (revert)
    {
        ingame.fld_unkCA6 = 0;
        return;
    }
    ingame.fld_unkCA6 = ang1;
    ingame.fld_unkCA8 = ang2;
}

TbBool person_init_specific_command(struct Thing *p_person, ushort cmd)
{
    struct Command *p_cmd;
    struct Thing *p_othertng;
    StateChRes res;
    short othertng;

    if ((debug_log_things & 0x01) != 0) {
        LOGSYNC_F("Thing %s %d inits %s %d",
          thing_type_name(p_person->Type, p_person->SubType),
          (int)p_person->ThingOffset, command_codename(cmd), (int)cmd);
    }

    p_cmd = &game_commands[cmd];
    switch (p_cmd->Type)
    {
    case PCmd_NONE:
        res = person_init_cmd_wander(p_person, p_cmd->Type);
        break;
    case PCmd_GO_TO_POINT:
    case PCmd_RUN_TO_POINT:
        res = person_init_go_to_point(p_person, p_cmd->X, p_cmd->Y, p_cmd->Z, p_cmd->Arg1,
          (p_cmd->Type == PCmd_RUN_TO_POINT));
        break;
    case PCmd_GO_TO_PERSON:
        res = person_init_go_to_person(p_person, p_cmd->OtherThing, p_cmd->Arg1, false);
        break;
    case PCmd_KILL_PERSON:
        res = person_init_kill_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_KILL_MEM_GROUP:
        p_person->Flag2 &= ~TgF2_IgnoreEnemies;
        othertng = find_nearest_from_group(p_person, p_cmd->OtherThing, 0);
        if (othertng != 0)
        {
            //TODO is it really supposed to modify a vehicle? What is this?
            struct Thing *p_vehicle;
            p_vehicle = &things[p_person->U.UPerson.Vehicle];
            p_vehicle->Flag |= TngF_PlayerAgent;
        }
        res = person_init_kill_person(p_person, othertng);
        break;
    case PCmd_KILL_ALL_GROUP:
        othertng = find_nearest_from_group(p_person, p_cmd->OtherThing, 0);
        groups_set_kill_on_sight(p_person->U.UPerson.Group, p_cmd->OtherThing, true);
        res = person_init_kill_person(p_person, othertng);
        break;
    case PCmd_PERSUADE_PERSON:
        res = person_init_persuade_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_PERSUADE_MEM_GROUP:
    case PCmd_PERSUADE_ALL_GROUP:
        othertng = find_nearest_from_group(p_person, p_cmd->OtherThing, 1);
        res = person_init_persuade_person(p_person, othertng);
        break;
    case PCmd_BLOCK_PERSON:
        res = person_init_block_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_SCARE_PERSON:
        res = person_init_scare_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_FOLLOW_PERSON:
        res = person_init_cmd_follow_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_SUPPORT_PERSON:
        res = person_init_support_person(p_person, p_cmd->OtherThing);
        break;
    case PCmd_PROTECT_PERSON:
        res = person_init_protect_person(p_person, p_cmd->OtherThing, true);
        break;
    case PCmd_GET_ITEM:
        res = person_init_cmd_get_item(p_person, p_cmd->OtherThing);
        break;
    case PCmd_USE_WEAPON:
        res = person_init_cmd_use_weapon(p_person, p_cmd->X, p_cmd->Y, p_cmd->Z, p_cmd->OtherThing);
        break;
    case PCmd_DROP_SPEC_ITEM:
        person_init_drop_special(p_person, p_cmd->OtherThing);
        res = StCh_ACCEPTED;
        break;
    case PCmd_WAND_AVOID_GROUP:
        res = person_init_wand_avoid_group(p_person, p_cmd->OtherThing, p_cmd->Type);
        break;
    case PCmd_DESTROY_BUILDING:
        res = person_init_destroy_building(p_person, p_cmd->X, p_cmd->Z, p_cmd->OtherThing);
        break;
    case PCmd_USE_VEHICLE:
        res = person_init_use_vehicle(p_person, p_cmd->OtherThing);
        break;
    case PCmd_EXIT_VEHICLE:
        res = person_init_exit_vehicle(p_person);
        break;
    case PCmd_CATCH_TRAIN:
        res = person_init_catch_train(p_person, p_cmd->OtherThing);
        break;
    case PCmd_OPEN_DOME:
        res = person_open_dome(p_person, p_cmd->OtherThing);
        break;
    case PCmd_CLOSE_DOME:
        res = person_close_dome(p_person, p_cmd->OtherThing);
        break;
    case PCmd_DROP_WEAPON:
        res = person_init_drop_item_where_standing(p_person, p_cmd->OtherThing);
        break;
    case PCmd_CATCH_FERRY:
        res = person_init_catch_ferry(p_person, p_cmd->X, p_cmd->Z, p_cmd->Arg1);
        break;
    case PCmd_EXIT_FERRY:
        res = person_init_exit_ferry(p_person, p_cmd->OtherThing);
        break;
    case PCmd_PING_EXIST:
        res = person_or_vehicle_ping_exist(p_person, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        p_person->State = PerSt_NONE;
        break;
    case PCmd_GOTOPOINT_FACE:
        res = person_init_go_to_point_face(p_person, p_cmd->X, p_cmd->Z,
          p_cmd->OtherThing, p_cmd->Arg1);
        break;
    case PCmd_SELF_DESTRUCT:
        if (cybmod_chest_level(&p_person->U.UPerson.UMod) < 2)
            set_cybmod_chest_level(&p_person->U.UPerson.UMod, 2);
        person_self_destruct(p_person);
        res = StCh_ACCEPTED;
        break;
    case PCmd_PROTECT_MEM_G:
        othertng = find_nearest_from_group(p_person, p_cmd->OtherThing, 0);
        res = person_init_protect_person(p_person, othertng, false);
        break;
    case PCmd_KILL_EVERYONE:
        othertng = find_peep_in_area(p_person, p_cmd);
        res = person_init_kill_person(p_person, othertng);
        break;
    case PCmd_GUARD_OFF:
        res = person_guard_switch(p_person, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        break;
    case PCmd_WAIT_TIME:
        p_cmd->Arg1 = 0;
        res = person_init_cmd_wait_patiently(p_person, p_cmd->Type);
        break;
    case PCmd_WAIT_P_V_DEAD:
    case PCmd_WAIT_MEM_G_DEAD:
    case PCmd_WAIT_ALL_G_DEAD:
    case PCmd_WAIT_P_V_I_NEAR:
    case PCmd_WAIT_MEM_G_NEAR:
    case PCmd_WAIT_ALL_G_NEAR:
    case PCmd_WAIT_P_V_I_ARRIVE:
    case PCmd_WAIT_MEM_G_ARRIVE:
    case PCmd_WAIT_ALL_G_ARRIVE:
    case PCmd_WAIT_P_PERSUADE:
    case PCmd_WAIT_MEM_G_PERSUADE:
    case PCmd_WAIT_ALL_G_PERSUADE:
    case PCmd_WAIT_MISSION_SUCC:
    case PCmd_WAIT_MISSION_FAIL:
    case PCmd_WAIT_MISSION_START:
    case PCmd_WAIT_OBJT_DESTROY:
    case PCmd_WAIT_OBJV:
        res = person_init_cmd_wait_patiently(p_person, p_cmd->Type);
        break;
    case PCmd_WAND_TIME:
        p_cmd->Arg1 = 0;
        res = person_init_cmd_wander(p_person, p_cmd->Type);
        break;
    case PCmd_WAND_P_V_DEAD:
    case PCmd_WAND_MEM_G_DEAD:
    case PCmd_WAND_ALL_G_DEAD:
    case PCmd_WAND_P_V_I_NEAR:
    case PCmd_WAND_MEM_G_NEAR:
    case PCmd_WAND_ALL_G_NEAR:
    case PCmd_WAND_P_V_I_ARRIVE:
    case PCmd_WAND_MEM_G_ARRIVE:
    case PCmd_WAND_ALL_G_ARRIVE:
    case PCmd_WAND_P_PERSUADE:
    case PCmd_WAND_MEM_G_PERSUADE:
    case PCmd_WAND_ALL_G_PERSUADE:
    case PCmd_WAND_MISSION_SUCC:
    case PCmd_WAND_MISSION_FAIL:
    case PCmd_WAND_MISSION_START:
    case PCmd_WAND_OBJT_DESTROY:
    case PCmd_WAND_OBJV:
        res = person_init_cmd_wander(p_person, p_cmd->Type);
        break;
    case PCmd_UNKN66:
        // Nothing done here
        person_command_select_next(p_person);
        return true;
    case PCmd_ADD_STATIC:
        add_static(p_cmd->X, p_cmd->Y, p_cmd->Z, 900, 10);
        person_command_select_next(p_person);
        return true;
    case PCmd_WAIT_TIME2:
        res = person_init_cmd_wait_wth_timeout(p_person, 0, p_cmd->Time);
        break;
    case PCmd_LOOP_COM:
        res = person_command_jump(p_person, p_cmd->OtherThing);
        break;
    case PCmd_WITHIN_AREA:
        res = person_cmd_stay_within_area(p_person, cmd);
        break;
    case PCmd_WITHIN_OFF:
        res = person_cmd_stay_within_area(p_person, 0);
        break;
    case PCmd_LOCK_BUILDN:
        res = person_lock_building(p_person, p_cmd->OtherThing);
        break;
    case PCmd_UNLOCK_BUILDN:
        res = person_unlock_building(p_person, p_cmd->OtherThing);
        break;
    case PCmd_SELECT_WEAPON:
        res = person_cmd_select_weapon(p_person, p_cmd->OtherThing);
        break;
    case PCmd_HARD_AS_AGENT:
        res = person_make_hard_as_agent(p_person);
        break;
    case PCmd_START_DANGER_MUSIC:
        res = person_cmd_start_danger_music(p_person, (p_cmd->Flags & PCmdF_RevertFunct) != 0);
        break;
    case PCmd_PING_P_V:
        p_othertng = &things[p_cmd->OtherThing];
        res = person_or_vehicle_ping_exist(p_othertng, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        p_person->State = PerSt_NONE;
        break;
    case PCmd_CAMERA_TRACK:
        camera_track_thing(p_cmd->OtherThing, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        p_person->State = PerSt_NONE;
        res = StCh_ACCEPTED;
        break;
    case PCmd_UNTRUCE_GROUP:
        groups_set_truce(p_person->U.UPerson.Group, p_cmd->OtherThing, false);
        p_person->State = PerSt_NONE;
        res = StCh_ACCEPTED;
        break;
    case PCmd_PLAY_SAMPLE:
        res = person_cmd_play_sample(p_person, p_cmd->OtherThing,
          ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        break;
    case PCmd_IGNORE_ENEMIES:
        res = person_cmd_ignore_enemies(p_person, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        break;
    case PCmd_FIT_AS_AGENT:
        res = person_make_fit_as_agent(p_person);
        break;
    case PCmd_CAMERA_ROTATE:
        camera_rotate_view(p_cmd->OtherThing, p_cmd->Arg1, ((p_cmd->Flags & PCmdF_RevertFunct) != 0));
        res = StCh_ACCEPTED;
        break;
    case PCmd_STAY:
    case PCmd_HIDE:
    case PCmd_AVOID_PERSON:
    case PCmd_UNKN16:
    case PCmd_EXECUTE_COMS:
    case PCmd_UNKN27:
    case PCmd_UNKN28:
    case PCmd_UNKN29:
    case PCmd_UNKN2A:
    case PCmd_UNKN2B:
    case PCmd_UNKN2C:
    case PCmd_UNKN2D:
    case PCmd_UNKN2E:
    case PCmd_UNKN2F:
    case PCmd_UNKN30:
    case PCmd_UNKN31:
    case PCmd_UNKN32:
    case PCmd_UNKN44:
    case PCmd_UNKN45:
    case PCmd_UNKN46:
    case PCmd_UNKN58:
    case PCmd_UNKN59:
    case PCmd_UNKN5A:
    case PCmd_UNKN5B:
    case PCmd_UNKN5C:
    case PCmd_UNKN5D:
    case PCmd_UNKN5E:
    case PCmd_UNKN5F:
    case PCmd_UNKN60:
    case PCmd_UNKN61:
    case PCmd_UNKN62:
    case PCmd_UNKN63:
    case PCmd_UNKN64:
    case PCmd_UNKN65:
    case PCmd_UNKN67:
    case PCmd_UNKN6A:
    case PCmd_UNKN6B:
    case PCmd_UNKN6C:
    case PCmd_UNKN6D:
    default:
        res = StCh_ACCEPTED;
        break;
    case PCmd_UNTIL_P_V_DEAD:
    case PCmd_UNTIL_MEM_G_DEAD:
    case PCmd_UNTIL_ALL_G_DEAD:
    case PCmd_UNTIL_P_V_I_NEAR:
    case PCmd_UNTIL_MEM_G_NEAR:
    case PCmd_UNTIL_ALL_G_NEAR:
    case PCmd_UNTIL_P_V_I_ARRIVE:
    case PCmd_UNTIL_MEM_G_ARRIVE:
    case PCmd_UNTIL_ALL_G_ARRIVE:
    case PCmd_UNTIL_P_PERSUADE:
    case PCmd_UNTIL_MEM_G_PERSUADE:
    case PCmd_UNTIL_ALL_G_PERSUADE:
    case PCmd_UNTIL_MISSION_SUCC:
    case PCmd_UNTIL_MISSION_FAIL:
    case PCmd_UNTIL_MISSION_START:
    case PCmd_UNTIL_OBJT_DESTROY:
    case PCmd_UNTIL_TIME:
    case PCmd_UNTIL_OBJV:
    case PCmd_UNTIL_G_NOT_SEEN:
        // It should not be possible to reach this function with `until` command
        // is the `until` command not properly marked by flag?
        res = StCh_DENIED;
        break;
    }

    if ((debug_log_things & 0x01) != 0)
    {
        char locstr[192];

        snprint_command(locstr, sizeof(locstr), cmd);

        if (res <= StCh_ALREADY)
            LOGSYNC_F("Person %s %d %s %d %s, state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset, locstr,
              cmd, state_change_result_name(res), p_person->State, p_person->SubState);
        else
            LOGWARN("Person %s %d %s %d %s, state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset, locstr,
              cmd, state_change_result_name(res), p_person->State, p_person->SubState);
    }

    return false;
}

TbBool person_init_specific_preplay_command(struct Thing *p_person, ushort cmd)
{
    struct Command *p_cmd;

    p_cmd = &game_commands[cmd];

    switch (p_cmd->Type)
    {
    case PCmd_HARD_AS_AGENT:
        set_person_stats_type(p_person, SubTT_PERS_AGENT);
        break;
    case PCmd_FIT_AS_AGENT:
        set_person_energy_stamina_type(p_person, SubTT_PERS_AGENT);
        break;
    default:
        // found in-play command; terminate the pre-play loop
        return false;
    }

    if ((debug_log_things & 0x01) != 0)
    {
        char locstr[192];

        snprint_command(locstr, sizeof(locstr), cmd);

        LOGSYNC_F("Person %s %d %s %d %s, state %d.%d",
          person_type_name(p_person->SubType), (int)p_person->ThingOffset, locstr,
          cmd, "pre-played", p_person->State, p_person->SubState);
    }
    return true;
}

void person_init_preplay_command(struct Thing *p_person)
{
    ushort cmd;
    ubyte do_next;

    //TODO why are we discarding the commands completely from stack? just jump above them instead?
    for (cmd = p_person->U.UPerson.ComHead; cmd != 0; cmd = p_person->U.UPerson.ComHead)
    {
        do_next = person_init_specific_preplay_command(p_person, cmd);
        if (!do_next)
            break;
        person_command_skip_at_start(p_person);
    }
}

void person_init_command(struct Thing *p_person, ushort from)
{
#if 0
    asm volatile ("call ASM_person_init_command\n"
        : : "a" (p_person), "d" (from));
    return;
#endif
    struct Command *p_cmd;
    ushort cmd;
    ushort nxcmd;
    ubyte do_next;

    for (cmd = p_person->U.UPerson.ComCur; 1; )
    {
        p_person->Flag &= ~TngF_TriggerUse;
        p_person->U.UPerson.Target2 = 0;

        p_cmd = &game_commands[cmd];

        if ((p_person->Flag & TngF_Destroyed) != 0)
        {
            p_person->State = PerSt_DEAD;
            break;
        }

        if (cmd == 0)
        {
            if ((p_person->Flag2 & TgF2_Unkn0800) != 0)
            {
                p_person->Flag2 &= ~TgF2_Unkn0800;
                //TODO should we really keep previous substate?
                person_init_cmd_wait_wth_timeout(p_person, p_person->SubState, 50);
                ingame.Flags &= ~GamF_Unkn0100;
                set_peep_comcur(p_person);
            }
            break;
        }

        if ((p_cmd->Flags & PCmdF_RunUntil) != 0)
        {
            nxcmd = person_command_until_check_condition(p_person, cmd);
            if (nxcmd != CMD_CONDITION_CONTINUE) {
                person_command_jump(p_person, nxcmd);
                break;
            }
        }

        do_next = person_init_specific_command(p_person, cmd);
        if (!do_next)
            break;
    }
}

static DIFFTEST_REF_FUNC void assign_next_target_from_group_asm(struct Thing *p_person, ushort group, ubyte flag)
{
    asm volatile ("call ASM_assign_next_target_from_group\n"
        : : "a" (p_person), "d" (group), "b" (flag));
}

void assign_next_target_from_group(struct Thing *p_person, ushort group, ubyte flag)
{
    ThingIdx target;
    s32 range;

    DIFFTEST_SHADOW(assign_next_target_from_group,
      assign_next_target_from_group_asm(p_person, group, flag));
    if (p_person->U.UPerson.Group == group)
        return;

    target = find_nearest_from_group(p_person, group, flag);
    if (target == 0)
    {
        p_person->State = PerSt_NONE;
        return;
    }
    check_weapon(p_person, 1280);
    p_person->State = PerSt_KILL_PERSON;
    p_person->U.UPerson.ComTimer = -1;
    p_person->PTarget = &things[target];
    p_person->GotoThingIndex = target;
    range = get_weapon_range(p_person);
    p_person->U.UPerson.Timer2 = 10;
    p_person->U.UPerson.StartTimer2 = 10;
    p_person->SubState = 0;
    p_person->U.UPerson.ComRange = range >> 6;
}

short is_group_all_persuaded_by_me(ushort group, struct Thing *p_me, short count)
{
    // looks like a variation on group_has_no_less_members_persuaded_by_person(); reuse?
    short ret;
    asm volatile (
      "call ASM_is_group_all_persuaded_by_me\n"
        : "=r" (ret) : "a" (group), "d" (p_me), "b" (count));
    return ret;
}

ubyte is_command_completed(struct Thing *p_person)
{
#if 0
    ubyte ret;
    asm volatile (
      "call ASM_is_command_completed\n"
        : "=r" (ret) : "a" (p_person));
    return ret;
#else
    struct Command *p_cmd;

    p_cmd = &game_commands[p_person->U.UPerson.ComCur];
    switch (p_cmd->Type)
    {
    case PCmd_KILL_MEM_GROUP:
        if (group_actions[p_cmd->OtherThing].Dead >= p_cmd->Arg2)
            return 1;
        assign_next_target_from_group(p_person, p_cmd->OtherThing, 0);
        p_person->State = PerSt_KILL_PERSON;
        return 0;
    case PCmd_KILL_ALL_GROUP:
        if (all_group_members_destroyed(p_cmd->OtherThing))
            return 1;
        assign_next_target_from_group(p_person, p_cmd->OtherThing, 0);
        p_person->State = PerSt_KILL_PERSON;
        return 0;
    case PCmd_PERSUADE_MEM_GROUP:
        if (is_group_all_persuaded_by_me(p_cmd->OtherThing, p_person, p_cmd->Arg2))
            return 1;
        assign_next_target_from_group(p_person, p_cmd->OtherThing, 1);
        p_person->State = PerSt_PERSUADE_PERSON;
        p_person->U.UPerson.ComRange = person_get_command_range_for_persuade(p_person);
        return 0;
    case PCmd_PERSUADE_ALL_GROUP:
        if (is_group_all_persuaded_by_me(p_cmd->OtherThing, p_person, -1))
            return 1;
        assign_next_target_from_group(p_person, p_cmd->OtherThing, 1);
        p_person->State = PerSt_PERSUADE_PERSON;
        p_person->U.UPerson.ComRange = person_get_command_range_for_persuade(p_person);
        return 0;
    default:
        return 1;
    }
#endif
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int do_fvect_colide_asm(int a1, int a2, int a3, int a4, int a5)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    int stkargs[1];
    int *p_stk = stkargs;
    stkargs[0] = a5;
    asm volatile ("push 0(%4)\n""call ASM_do_fvect_colide\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc), "+S" (p_stk) : : "cc", "memory");
    return ra;
}

int do_fvect_colide(int a1, int a2, int a3, int a4, int a5)
{
    struct MyMapElement *p_me = (struct MyMapElement *)a5;
    struct ColVectList *p_cvlist;
    struct ColVect *p_colvect;
    ushort cvlist;
    short face;
    int ret;
    int pass;

    DIFFTEST_SHADOW_RET(do_fvect_colide, int,
      do_fvect_colide_asm(a1, a2, a3, a4, a5));
    ret = 0;
    cvlist = p_me->ColHead;
    while (cvlist != 0)
    {
        if (ret != 0)
            break;
        p_cvlist = &game_col_vects_list[cvlist];
        if (((p_cvlist->NextColList & 0x8000) == 0) && (p_cvlist->Object >= 0))
        {
            p_colvect = &game_col_vects[p_cvlist->Vect];
            face = p_colvect->Face;
            if (face < 0)
            {
                struct SingleObjectFace4 *p_face;

                p_face = &game_object_faces4[-face];
                pass = p_face->GFlags & FGFlg_Unkn04;
                if (((p_face->GFlags & FGFlg_Unkn10) != 0) && (p_face->WalkHeader != 0))
                {
                    if ((sthings[(short)p_face->WalkHeader].Flag & 0x0002) != 0)
                        pass = 1;
                    else
                        pass = 0;
                }
            }
            else
            {
                // TODO the original has a dead branch here which would check
                // game_object_faces3[face].GFlags & FGFlg_Unkn04 for face > 0;
                // its jump condition is always true, so it is never reached
                pass = 0;
            }
            if (two4_line_intersection(p_colvect->X1, p_colvect->Z1,
              p_colvect->X2, p_colvect->Z2, a1, a2, a3, a4))
            {
                if (pass == 0)
                    ret = cvlist;
                else
                    ret = -(int)cvlist;
            }
        }
        cvlist = game_col_vects_list[cvlist].NextColList & 0x7FFF;
    }
    DIFFTEST_RETURN(ret);
}


int can_i_see_building(struct Thing *p_i, struct Thing *p_thing,
        int max_dist, ushort flags)
{
    int ret;
    asm volatile (
      "call ASM_can_i_see_building\n"
        : "=r" (ret) : "a" (p_i), "d" (p_thing), "b" (max_dist), "c" (flags));
    return ret;
}

void process_random_speech(struct Thing *p_person, ubyte a2)
{
#if 0
    asm volatile ("call ASM_process_random_speech\n"
        : : "a" (p_person), "d" (a2));
    return;
#endif
    ushort rndval;

    rndval = LbRandomAnyShort();
    if (ingame.TrackThing != 0)
        return;
    if (((p_person->Flag2 & TgF2_ExistsOffMap) != 0) || ((p_person->Flag & TngF_Persuaded) != 0))
        return;
    if (a2)
        return;

    switch (p_person->SubType)
    {
    case SubTT_PERS_MERCENARY:
        play_dist_speech(p_person, 57 + (rndval % 3), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        break;
    case SubTT_PERS_POLICE:
        play_dist_speech(p_person, 50 + (rndval & 3), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        break;
    case SubTT_PERS_SCIENTIST:
        play_dist_speech(p_person, 60 + (rndval % 2), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        break;
    }
}

static DIFFTEST_REF_FUNC void set_interrupt_target_asm(struct Thing *p_person, struct Thing *p_target)
{
    asm volatile ("call ASM_set_interrupt_target\n"
        : : "a" (p_person), "d" (p_target));
}

void set_interrupt_target(struct Thing *p_person, struct Thing *p_target)
{
    ubyte range;

    DIFFTEST_SHADOW(set_interrupt_target, set_interrupt_target_asm(p_person, p_target));
    // Pointer range check is inclusive of things[THINGS_LIMIT], as in the asm
    if ((p_target < &things[0]) || (p_target > &things[THINGS_LIMIT]))
        return;

    if ((p_person->Flag & TngF_SelectedAgent) != 0)
    {
        if ((p_person->Flag2 & TgF2_Unkn0008) == 0)
            return;
    }
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
    {
        if ((p_person->U.UPerson.Mood >= -10) && (p_person->U.UPerson.Mood <= 20)
          && ((ushort)p_person->State == PerSt_PROTECT_PERSON))
        {
            if ((things[(ushort)p_person->Owner].Flag & TngF_SelectedAgent) != 0)
                return;
        }
    }
    if ((p_person->Flag2 & TgF2_IgnoreEnemies) != 0)
        return;
    if ((p_person->U.UPerson.WeaponsCarried & 0x283ceff) == 0)
        return;
    if ((ushort)p_person->State == PerSt_PROTECT_PERSON)
    {
        if (p_person->U.UPerson.WeaponTurn != 0)
            return;
    }

    if ((p_person->U.UPerson.Target2 == 0) || (p_person->PTarget == NULL))
    {
        if (((p_person->Flag & (TngF_InVehicle | TngF_PlayerAgent)) == 0)
          && ((p_person->Flag2 & TgF2_Unkn0008) == 0))
        {
            p_person->VY = (p_person->X << 8) & 0xFFFF0000;
            p_person->VY |= (p_person->Z >> 8) & 0xFFFF;
            p_person->Flag |= TngF_PersKeepPost;
        }
    }

    if ((s32)p_person->U.UPerson.Target2 == (s32)(ushort)p_target->ThingOffset)
        return;

    check_weapon(p_person, 0x500);
    if (p_person->U.UPerson.Target2 == 0)
    {
        if (p_person->PTarget != NULL)
            p_person->OldTarget = p_person->PTarget->ThingOffset;
        else
            p_person->OldTarget = 0;
    }
    p_person->PTarget = p_target;
    p_person->U.UPerson.Target2 = p_target->ThingOffset;
    if (((p_person->Flag & TngF_PlayerAgent) == 0)
      && (p_person->U.UPerson.PathIndex != 0))
        remove_path(p_person);

    p_person->U.UPerson.ComTimer = -1;
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
        return;

    range = (get_weapon_range(p_person) >> 6) - 1;
    p_person->U.UPerson.ComRange = range;
    if (p_person->U.UPerson.ComRange > 100)
        p_person->U.UPerson.ComRange = 2;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int person_drops_weapons_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_person_drops_weapons\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

void plant_mine(struct Thing *p_person);
ubyte vector_in_way(struct Thing *p_thing, int dx, int dz);

int person_drops_weapons(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    s32 cor_y;
    u32 weapons;
    int i;

    // The asm leaves an unspecified value in eax; the only caller ignores it
    DIFFTEST_SHADOW(person_drops_weapons, person_drops_weapons_asm(a1, a2, a3, a4));
    cor_y = p_person->Y >> 8;
    weapons = p_person->U.UPerson.WeaponsCarried & ~(1u << (WEP_ENERGYSHLD - 1));
    if ((p_person->SubType == SubTT_PERS_ZEALOT) ||
      (p_person->SubType == SubTT_PERS_HIGH_PRIEST))
        weapons &= ~(1u << (WEP_UZI - 1));

    for (i = 0; i < 32; i++)
    {
        s32 wtype;
        short tries;

        if ((weapons & (1u << i)) == 0)
            continue;

        wtype = i + 1;
        if ((wtype == WEP_EXPLMINE) || (wtype == WEP_ELEMINE))
        {
            p_person->U.UPerson.TempWeapon = wtype;
            plant_mine(p_person);
            continue;
        }
        if ((wtype == WEP_UZI) && ((p_person->Flag & TngF_PlayerAgent) == 0)
          && in_network_game)
            continue;

        if (in_network_game && (net_game_play_flags & NGPF_Unkn08)
          && ((p_person->Flag & TngF_PlayerAgent) != 0))
        {
            ushort plyr, plagent;

            plyr = p_person->U.UPerson.ComCur >> 2;
            plagent = p_person->U.UPerson.ComCur & 3;
            if ((players[plyr].Weapons[plagent] & (weapons & (1u << i))) != 0)
                continue;
        }

        tries = 0;
        while (1)
        {
            struct MyMapElement *p_mapel;
            struct SimpleThing *p_item;
            s32 dx, dz, cor_x, cor_z;
            short prev_tries;

            prev_tries = tries;
            tries++;
            // TODO: running out of tries ends the function, so the remaining
            // weapons are not dropped; maybe an original bug
            if (prev_tries > 30)
                return 0;

            dx = (LbRandomAnyShort() & 0xFF) - 127;
            dz = (LbRandomAnyShort() & 0xFF) - 127;
            cor_x = (p_person->X >> 8) + dx;
            cor_z = (p_person->Z >> 8) + dz;
            if ((cor_x < 0) || (cor_x >= 0x8000))
                continue;
            if ((cor_z < 0) || (cor_z >= 0x8000))
                continue;

            p_mapel = &game_my_big_map[(cor_z >> 8) * 128 + (cor_x >> 8)];
            if ((p_mapel->Flags2 & MEF2_Unkn01) != 0)
                continue;

            if (!vector_in_way(p_person, dx, dz))
                continue;

            p_item = create_item(cor_x, cor_y, cor_z, 1004, (ubyte)wtype);
            if (p_item != NULL)
                p_item->U.UWeapon.OnFace = p_person->U.UPerson.OnFace;
            break;
        }
    }
    return 0;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void scare_civs(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_scare_civs\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}



void set_person_dead(struct Thing *p_person, ushort anim_mode)
{
#if 1
    asm volatile ("call ASM_set_person_dead\n"
        : : "a" (p_person), "d" (anim_mode));
    return;
#endif
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int available_for_guard_duty(int a1, int a2, int a3)
{
    int ra = a1, rd = a2, rb = a3;
    asm volatile ("call ASM_available_for_guard_duty\n"
        : "+a" (ra), "+d" (rd), "+b" (rb) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int find_best_guardian(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_best_guardian\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


uint find_and_alert_guardian(struct Thing *p_person, struct Thing *p_attacker)
{
    uint ret;
    asm volatile (
      "call ASM_find_and_alert_guardian\n"
        : "=r" (ret) : "a" (p_person), "d" (p_attacker));
    return ret;
}

static DIFFTEST_REF_FUNC void init_recoil_asm(struct Thing *p_person,
  short vx, short vy, short vz, ushort type)
{
    // Pushed through a register holding them: a "g" operand may be placed
    // relative to the stack pointer, which each push moves.
    int stkargs[1];

    stkargs[0] = (int)(u32)type;

    asm volatile (
      "push 0(%4)\n"
      "call ASM_init_recoil\n"
        : : "a" (p_person), "d" (vx), "b" (vy), "c" (vz), "S" (stkargs)
        : "cc", "memory");
}

void init_recoil(struct Thing *p_person, short vx, short vy, short vz, ushort type)
{
    ushort angle;
    ushort frm;

    DIFFTEST_SHADOW(init_recoil, init_recoil_asm(p_person, vx, vy, vz, type));
    if ((ushort)p_person->State == PerSt_PERSON_BURNING)
        return;
    if (p_person->SubType == SubTT_PERS_MECH_SPIDER)
        return;
    if (p_person->U.UPerson.RecoilTimer != 0)
        return;

    if ((p_person->Flag & TngF_Unkn0001) != 0)
    {
        p_person->Flag &= ~TngF_Unkn0001;
        p_person->U.UPerson.AnimMode = p_person->U.UPerson.OldAnimMode;
        p_person->State = 0;
        frm = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
        p_person->Frame = nstart_ani[frm + p_person->U.UPerson.Angle];
        p_person->StartFrame =
          people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    }

    angle = (ushort)arctan(vx, -vz);

    if (((p_person->U.UPerson.WeaponsCarried & (1 << (WEP_ENERGYSHLD-1))) != 0)
      && (p_person->U.UPerson.ShieldEnergy > 0))
    {
        p_person->Flag |= TngF_Unkn00200000;
        play_dist_sample(p_person, 0x1e, 0x7f, 0x40, 100, 0, 3);
        return;
    }

    if ((p_person->Flag & TngF_WepRecoil) != 0)
        return;

    p_person->U.UPerson.RecoilDir = angle >> 3;
    if (type == 1)
        p_person->U.UPerson.RecoilTimer = 3;
    else if (type == 7)
        p_person->U.UPerson.RecoilTimer = 4;
    else if (type == 8)
        p_person->U.UPerson.RecoilTimer = 7;
    else
        p_person->U.UPerson.RecoilTimer = 5;

    p_person->U.UPerson.Angle = ((angle + 0x400) & 0x7FF) >> 8;
    if (p_person->U.UPerson.AnimMode != 16)
        p_person->U.UPerson.OldAnimMode = p_person->U.UPerson.AnimMode;
    p_person->U.UPerson.AnimMode = 16;
    p_person->Flag |= TngF_WepRecoil;
    frm = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode];
    p_person->Frame = nstart_ani[frm + p_person->U.UPerson.Angle];
    p_person->StartFrame =
      people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
}

void person_update_kill_stats(struct Thing *p_attacker, struct Thing *p_victim)
{
    if (p_attacker == NULL)
        return;

    if (in_network_game)
    {
        if ((p_victim->Flag & TngF_PlayerAgent) != 0)
        {
          struct Thing *p_realowner;
          PlayerIdx attack_plyr;

          p_realowner = effective_owner_of_thing(p_attacker);

          if ((p_realowner != NULL) && ((p_realowner->Flag & TngF_PlayerAgent) != 0))
          {
              attack_plyr = p_realowner->U.UPerson.ComCur >> 2;
              killed_mp_agent_add_to_stats(p_victim, attack_plyr);
          }
        }
    }
    else
    {
        if (p_attacker->U.UObject.EffectiveGroup == ingame.MyGroup) {
            killed_person_add_to_stats(p_victim, open_brief);
        }
    }
}

/** Set a person into dying state.
 *
 * @param p_person The person which will start dying.
 * @param hp Amount of Hit Points subtracted from health by the final blow.
 * @param type Type of damage which caused the death.
 */
void person_start_dying(struct Thing *p_person, int hp, ushort type)
{
    switch (type)
    {
    case DMG_ELLASER:
    case DMG_BEAM:
    case DMG_LASER:
    case DMG_ELSTRAND:
        set_person_dead(p_person, ANIM_PERS_Unkn12);
        break;
    default:
    case DMG_UNKN5:
    case DMG_MINIGUN:
    case DMG_UNKN9:
        set_person_dead(p_person, ANIM_PERS_LAY_DOWN);
        break;
    case DMG_RAP:
    case DMG_LONGRANGE:
        set_person_dead(p_person, ANIM_PERS_Unkn10);
        break;
    }
}

TbBool persons_have_truce(struct Thing *p_person1, struct Thing *p_person2)
{
    ubyte pers1grp, pers2grp;

    if (p_person1 == NULL) {
        return false;
    }

    if (p_person2 == NULL) {
        return false;
    }

    // This function can be called for objects, vehicles, mguns, people, even effects/shots
    assert(offsetof(struct Thing, U.UPerson.EffectiveGroup) == offsetof(struct Thing, U.UObject.EffectiveGroup));
    assert(offsetof(struct Thing, U.UPerson.EffectiveGroup) == offsetof(struct Thing, U.UVehicle.EffectiveGroup));
    assert(offsetof(struct Thing, U.UPerson.EffectiveGroup) == offsetof(struct Thing, U.UMGun.EffectiveGroup));
    assert(offsetof(struct Thing, U.UPerson.EffectiveGroup) == offsetof(struct Thing, U.UEffect.EffectiveGroup));

    pers1grp = p_person1->U.UPerson.EffectiveGroup & 0x7F;
    pers2grp = p_person2->U.UPerson.EffectiveGroup & 0x7F;

    return groups_have_truce(pers1grp, pers2grp);
}

TbBool things_have_same_group(struct Thing *p_thing1, struct Thing *p_thing2)
{
    if (p_thing1 == NULL) {
        return false;
    }

    if (p_thing2 == NULL) {
        return false;
    }

    return things_check_same_group(p_thing1->ThingOffset, p_thing2->ThingOffset);
}

void persons_set_groups_kill_on_sight_if_player_attacked(struct Thing *p_attacker, struct Thing *p_victim)
{
    if ((p_attacker == NULL) || ((p_attacker->Flag & TngF_PlayerAgent) == 0)) {
        return;
    }

    if ((p_victim == NULL) || (p_victim != p_attacker->PTarget)) {
        return;
    }

    if (person_is_basic_civilian(p_victim, false)) {
        return;
    }

    thing_groups_set_kill_on_sight_one_way(p_attacker->ThingOffset, p_victim->ThingOffset);
}

int mods_affect_hit_points(struct Thing *p_thing, ushort type, int hp)
{
    switch (type)
    {
    case DMG_UZI:
    case DMG_MINIGUN:
    case DMG_LONGRANGE:
        if (person_mod_skin_level(p_thing) == 1)
            hp >>= 1;
        break;
    case DMG_ELLASER:
    case DMG_BEAM:
    case DMG_LASER:
    case DMG_ELSTRAND:
        if (person_mod_skin_level(p_thing) == 3)
            hp >>= 1;
        break;
    case DMG_UNKN5:
    case DMG_RAP:
        break;
    case DMG_UNKN9:
        if (person_mod_skin_level(p_thing) == 1)
            hp = 3 * (hp >> 2);
        break;
    default:
        break;
    }
    return hp;
}

int dead_person_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    if (p_thing->U.UPerson.AnimMode == ANIM_PERS_DEAD_BODY)
    {
        if (type == DMG_RAP)
        {
            p_thing->Timer1 = 16;
            p_thing->StartTimer1 = 16;
            p_thing->SubState = 26;
            p_thing->U.UPerson.FrameId.Version[4] = 0;
            p_thing->U.UPerson.FrameId.Version[3] = 0;
            set_person_anim_mode(p_thing, ANIM_PERS_Unkn10);
            return 1;
        }
        if (type == DMG_UZI || type == DMG_MINIGUN || type == DMG_LONGRANGE || type == DMG_UNKN9)
        {
            p_thing->Frame = frame[p_thing->Frame].Next;
            return 1;
        }
        p_thing->Timer1 = 16;
        p_thing->StartTimer1 = 16;
        p_thing->SubState = 26;
        p_thing->U.UPerson.FrameId.Version[4] = 0;
        p_thing->U.UPerson.FrameId.Version[3] = 0;
        p_thing->U.UPerson.AnimMode = ANIM_PERS_Unkn12;
        set_person_frame_noangle(p_thing, 1067);
        play_dist_sample(p_thing, 57, FULL_VOL, EQUL_PAN, NORM_PTCH, 0, 3);
    }
    return 1;
}

/** Returns if given agent holds a weapon different than weapon of selected agent.
 *
 * Very specific function, expected to be required in exactly one situation - to
 * decide whether we should search around hit position for neutral factions to
 * antagonize due to the shooting.
 *
 * Usually the selected agent will shoot as well as the rest, so we do not care
 * for antagonizing check to be repeated. But if selected agent has shorter range
 * weapon, or just different weapon - then we should repeat the search, as very
 * likely it will be at different destination.
 *
 * Can the whole condition be removed instead, to always check all agents shot
 * target - maybe. Not sure it it would have other consequences.
 */
TbBool player_agent_selected_holds_different_weapon_than(struct Thing *p_person)
{
    struct Thing *p_dcthing;
    ThingIdx dcthing;
    PlayerIdx plyr;

    if ((p_person->Flag & TngF_PlayerAgent) == 0) {
        return false;
    }
    if (p_person->U.UPerson.CurrentWeapon == WEP_NULL) {
        return false;
    }

    plyr = p_person->U.UPerson.ComCur >> 2;
    dcthing = (ThingIdx)players[plyr].DirectControl[0];
    if (dcthing <= 0) {
        return false;
    }
    p_dcthing = &things[dcthing];

    return (p_dcthing->U.UPerson.CurrentWeapon != p_person->U.UPerson.CurrentWeapon);
}

int person_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
    short hp1;
    int energy_decr;

    hp1 = hp;

    if (persons_have_truce(p_attacker, p_thing)) {
        return 1;
    }
    // Make attacker group aggressive toward the victim group, but under some conditions
    persons_set_groups_kill_on_sight_if_player_attacked(p_attacker, p_thing);
    hp1 = mods_affect_hit_points(p_thing, type, hp);

    if ((p_attacker != NULL) && (type != DMG_ELSTRAND) && ((p_thing->Flag & TngF_SelectedAgent) == 0)
      && ((p_thing->Flag & TngF_Destroyed) == 0) && ((p_attacker->Flag2 & TgF2_ExistsOffMap) == 0)
      && ((p_thing->Flag2 & TgF2_KnockedOut) == 0))
    {
        set_interrupt_target(p_thing, p_attacker);
    }

    if ((p_thing->Flag & TngF_Destroyed) != 0) {
        return 1;
    }

    if ((p_attacker == NULL) || (type == DMG_ELSTRAND)) {
        // No action
    } else if (things_have_same_group(p_attacker, p_thing) || persons_have_truce(p_attacker, p_thing)) {
        // No action
    } else if ((((p_attacker->Flag & TngF_SelectedAgent) != 0) || player_agent_selected_holds_different_weapon_than(p_attacker)) ||
      ((p_thing == p_attacker->PTarget) && (p_attacker->Flag2 & TgF2_ExistsOffMap) == 0)) {
        if ((p_thing->Flag & TngF_Persuaded) == 0)
            // Make victim group aggressive back toward the attacker group
            thing_groups_set_kill_on_sight_one_way(p_thing->ThingOffset, p_attacker->ThingOffset);

        if (((p_thing->Flag & TngF_Persuaded) == 0) && thing_group_has_guardians(p_thing->ThingOffset))
            find_and_alert_guardian(p_thing, p_attacker);
    }

    energy_decr = 0;
    if ((p_thing->Flag & TngF_Unkn00200000) != 0)
    {
        p_thing->U.UPerson.ShieldEnergy -= hp1;
        p_thing->U.UPerson.ShieldGlowTimer = 4;
        if (p_thing->U.UPerson.ShieldEnergy >= 0)
        {
            if (p_thing->U.UPerson.ShieldEnergy > PERSON_MAX_SHIELD)
                p_thing->U.UPerson.ShieldEnergy = PERSON_MAX_SHIELD;
            return 0;
        }
        p_thing->Health += p_thing->U.UPerson.ShieldEnergy;
        energy_decr = -p_thing->U.UPerson.ShieldEnergy;
    }
    else
    {
        if (p_thing->U.UPerson.RecoilTimer <= 1)
        {
            ushort smpl_no;

            switch (person_sex(p_thing))
            {
            case PERSON_FEMALE:
                smpl_no = sfx_woman_shot[(gameturn + p_thing->ThingOffset) & 1];
                break;
            case PERSON_MALE:
            default:
                smpl_no = sfx_man_shot[(gameturn + p_thing->ThingOffset) & 7];
                break;
            }
            play_dist_sample(p_thing, smpl_no, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
        }
        p_thing->Health -= hp1;
        p_thing->U.UPerson.ShieldEnergy -= hp1;
    }

    if (p_thing->Health <= 0)
    {
        int prev_health;
        prev_health = p_thing->Health + hp1 + energy_decr;
        person_start_dying(p_thing, hp1 + energy_decr, type);
        person_update_kill_stats(p_attacker, p_thing);
        return prev_health;
    }

    if ((p_thing->Flag2 & TgF2_KnockedOut) == 0)
    {
        init_recoil(p_thing, vx, vy, vz, type);
    }
    return 0;
}

int thing_hit_by_bullet(struct Thing *p_thing, short hp,
  int vx, int vy, int vz, struct Thing *p_attacker, ushort type)
{
#if 0
    int ret;
    asm volatile (
      "push %7\n"
      "push %6\n"
      "push %5\n"
      "call ASM_thing_hit_by_bullet\n"
        : "=r" (ret) : "a" (p_thing), "d" (hp), "b" (vx), "c" (vy), "g" (vz), "g" (p_attacker), "g" (type));
    return ret;
#endif
    if ((p_thing->Flag & TngF_Unkn40000000) != 0)
        return 1;

    switch (p_thing->Type)
    {
    case TT_PERSON:
        if (p_thing->State == PerSt_PERSON_BURNING)
            return 1;
        if ((p_thing->Flag & TngF_InVehicle) != 0)
        {
            p_thing = &things[p_thing->U.UPerson.Vehicle];
            if (p_thing->Type != TT_VEHICLE)
                return 0;
        }
        if (p_thing->State == PerSt_DEAD)
        {
            return dead_person_hit_by_bullet(p_thing, hp, vx, vy, vz, p_attacker, type);
        }
        if ((p_thing->Flag & TngF_Destroyed) != 0)
            return 1;
        if ((p_thing->Flag & TngF_Unkn40000000) != 0)
            return 1;
        break;
    default:
        break;
    }

    switch (p_thing->Type)
    {
    case SmTT_STATIC:
        return static_hit_by_bullet((struct SimpleThing *)p_thing,
          hp, vx, vy, vz, p_attacker, type);
    case TT_VEHICLE:
        return vehicle_hit_by_bullet(p_thing, hp, vx, vy, vz, p_attacker, type);
    case TT_PERSON:
        return person_hit_by_bullet(p_thing, hp, vx, vy, vz, p_attacker, type);
    case TT_BUILDING:
        return building_hit_by_bullet(p_thing, hp, vx, vy, vz, p_attacker, type);
    case TT_MINE:
        return mine_hit_by_bullet(p_thing, hp, vx, vy, vz, p_attacker, type);
    default:
        break;
    }
    return 0;
}

TbBool person_use_medikit(struct Thing *p_person, PlayerIdx plyr)
{
    if (plyr == local_player_no)
        play_sample_using_heap(0, 2, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3u);
    p_person->Health = p_person->U.UPerson.MaxHealth;
    if (person_carries_weapon(p_person, WEP_MEDI1)) {
        person_weapons_remove_one(p_person, WEP_MEDI1);
    } else if (person_carries_weapon(p_person, WEP_MEDI2)) {
        person_weapons_remove_one(p_person, WEP_MEDI2);
    }
    return true;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void do_person_gotothingindex_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_do_person_gotothingindex\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

int do_next_navigate_index(int a1, int a2, int a3, int a4);
short person_move(struct Thing *p_person);

void do_person_gotothingindex(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_target;
    struct MyPath *p_path;
    struct MyMapElement *p_mapel;
    s32 dx, dz;
    s32 dist, lim;
    s32 adx, adz;
    s32 vx, vz;
    ushort angle;
    ushort found;
    ubyte zip;
    short mv;
    short tmp;

    DIFFTEST_SHADOW(do_person_gotothingindex,
      do_person_gotothingindex_asm(a1, a2, a3, a4));

    if ((p_person->Flag & TngF_Unkn00020000) != 0)
        p_person->U.UPerson.LastDist = 0x7d00;
    if (p_person->U.UPerson.PathIndex != 0)
        unkn_path_func_001(p_person, 1);

    if (p_person->U.UPerson.PathIndex != 0)
    {
        p_path = &my_paths[p_person->U.UPerson.PathIndex];
        dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
        dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
        adx = abs(dx);
        adz = abs(dz);
        if (adx > adz)
            dist = (adz >> 1) + adx;
        else
            dist = (adx >> 1) + adz;

        if (p_person->PathOffset + 1 < p_path->Flag) {
            lim = 20;
        } else {
            lim = p_person->U.UPerson.ComRange << 6;
            if (lim < 20)
                lim = 20;
        }
        if ((p_person->Flag2 & TgF2_Unkn4000) != 0)
        {
            p_person->U.UPerson.LastDist = 0x7d00;
        }
        else
        {
            if (dist < lim) {
                p_person->U.UPerson.ComTimer = -100;
            } else if ((p_person->Speed > 0) && (dist > (ushort)p_person->U.UPerson.LastDist)) {
                p_person->U.UPerson.ComTimer = -100;
            }
            p_person->U.UPerson.LastDist = dist;
        }
    }

    if (((p_person->Flag2 & TgF2_Unkn4000) == 0) &&
      ((((ushort)p_person->ThingOffset + gameturn) & 7) == 0))
    {
        p_target = &things[p_person->GotoThingIndex];
        p_mapel = &game_my_big_map[((p_person->Z >> 16) << 7) + (p_person->X >> 16)];
        zip = p_mapel->Zip;
        found = 0;
        if (zip != 0)
        {
            p_mapel = &game_my_big_map[((p_target->Z >> 16) << 7) + (p_target->X >> 16)];
            if (p_mapel->Zip == zip)
                found = 1;
        }
        if (found != 0)
        {
            p_person->U.UPerson.ComTimer = -1;
            remove_path(p_person);
        }
    }

    if (p_person->U.UPerson.ComTimer < 0)
    {
        p_person->Flag &= ~TngF_Unkn00020000;
        if (p_person->U.UPerson.PathIndex == 0)
        {
            p_target = &things[p_person->GotoThingIndex];
            if (((p_target->Flag & TngF_Unkn4000) != 0) ||
              ((p_person->Flag2 & TgF2_Unkn4000) != 0))
                found = build_navigate_path(p_person, p_target->U.UPerson.GotoX,
                  p_target->U.UPerson.GotoZ, 0);
            else
                found = build_navigate_path_to_person(p_person, p_target);
            if (found == 0)
                return;
        }
        p_path = &my_paths[p_person->U.UPerson.PathIndex];
        dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
        dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
        dist = LbSqrL(dx * dx + dz * dz);
        if (dist == 0)
            dist = 1;
        if (p_person->U.UPerson.ComTimer == -100)
        {
            p_person->U.UPerson.LastDist = 0x7d00;
            if ((ubyte)do_next_navigate_index((int)p_person, 0, 0, 0) != 0)
            {
                p_person->U.UPerson.PathIndex = 0;
                p_person->State = 0;
            }
            else
            {
                p_path = &my_paths[p_person->U.UPerson.PathIndex];
                dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
                dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
                dist = LbSqrL(dx * dx + dz * dz);
            }
        }
        if (dist == 0)
            dist = 1;
        vx = (dx << 8) / dist;
        vz = (dz << 8) / dist;
        if (p_person->Speed < 16)
            p_person->U.UPerson.ComTimer = 10;
        else
            p_person->U.UPerson.ComTimer = dist / (p_person->Speed >> 4);
        p_person->VX = vx;
        p_person->VZ = vz;

        if (((p_person->Flag & TngF_PlayerAgent) != 0) && (p_person->State == 0x2b) &&
          ((p_person->PTarget != NULL) || ((p_person->Flag & TngF_ShootAtPos) != 0)))
        {
            // keep current angle
        }
        else if (((p_person->Flag & TngF_Persuaded) != 0) &&
          (((p_person->Flag & (TngF_ShootAtPos|TngF_TriggerUse)) == (TngF_ShootAtPos|TngF_TriggerUse)) ||
          (p_person->PTarget != NULL)))
        {
            // keep current angle
        }
        else
        {
            angle = (((ushort)arctan(vx, -vz) + 0x80) & 0x7ff) >> 8;
            if (angle != p_person->U.UPerson.Angle)
            {
                p_person->Frame -= nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
                  + p_person->U.UPerson.Angle];
                p_person->U.UPerson.Angle = angle;
                p_person->Frame += nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
                  + (ubyte)angle];
            }
        }
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    }

    p_person->U.UPerson.ComTimer--;
    mv = person_move(p_person);
    if (mv != 0)
    {
        if ((p_person->Flag & TngF_Destroyed) != 0)
            return;
        if ((p_person->Flag & TngF_Unkn00020000) != 0)
        {
            p_person->U.UPerson.ComTimer = 3;
            p_person->VX = -p_person->VX;
            p_person->VZ = -p_person->VZ;
        }
        else if ((word_1AA392 == 0) && ((p_person->Flag2 & TgF2_Unkn0080) == 0))
        {
            p_person->Flag |= TngF_Unkn00020000;
            p_person->U.UPerson.BumpCount++;
            if (mv < 0)
            {
                tmp = (short)p_person->VX;
                p_person->VX = p_person->VZ >> 2;
                p_person->VZ = (-(s32)tmp) >> 2;
            }
            else
            {
                tmp = (short)p_person->VX;
                p_person->VX = p_person->VZ;
                p_person->VZ = tmp;
                p_person->VZ = -(s32)tmp;
            }
            p_person->U.UPerson.ComTimer = 3;
        }
    }

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;
    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        p_person->Frame = frame[(ushort)p_person->Frame].Next;
    }
}


static DIFFTEST_REF_FUNC int person_goto_person_nav_asm(struct Thing *p_person)
{
    int ret;
    asm volatile (
      "call ASM_person_goto_person_nav\n"
        : "=a" (ret) : "a" (p_person));
    return ret;
}

int person_goto_person_nav(struct Thing *p_person)
{
    struct Thing *p_target;
    struct Thing *p_vehicle;
    s32 dist_sq, range_sq;
    s32 dx, dz;
    int anim;
    ushort rnd;
    short old_state;
    short timer;

    DIFFTEST_SHADOW_RET(person_goto_person_nav, int,
      person_goto_person_nav_asm(p_person));

    p_target = &things[p_person->GotoThingIndex];
    if ((p_target->Flag & TngF_Unkn4000) != 0)
    {
        p_person->Flag |= TngF_Unkn0004;
        dist_sq = 0x800000;
    }
    else
    {
        dx = (p_person->X - p_target->X) >> 8;
        dz = (p_person->Z - p_target->Z) >> 8;
        dist_sq = dx * dx + dz * dz;
    }

    if ((p_person->Flag & TngF_Persuaded) != 0)
    {
        if ((p_person->U.UPerson.OnFace != 0) && (p_target->U.UPerson.OnFace == 0))
            dist_sq = 0x800000;
        if ((p_person->U.UPerson.OnFace == 0) && (p_target->U.UPerson.OnFace != 0))
            dist_sq = 0x800000;
    }

    if ((p_person->Flag2 & TgF2_Unkn00020000) != 0)
        DIFFTEST_RETURN(dist_sq);

    range_sq = (p_person->U.UPerson.ComRange * p_person->U.UPerson.ComRange) << 12;

    if (((p_target->Flag & TngF_Destroyed) != 0) && (dist_sq < 0x400000))
    {
        p_person->State = 0;
        remove_path(p_person);
        DIFFTEST_RETURN(1);
    }

    if (dist_sq < range_sq)
    {
        if ((p_person->Flag2 & TgF2_Unkn0100) == 0)
        {
            p_person->U.UPerson.GotoX = p_target->X >> 8;
            p_person->U.UPerson.GotoZ = p_target->Z >> 8;
            remove_path(p_person);
            p_person->U.UPerson.ComTimer = -1;
        }
        if ((p_person->U.UPerson.AnimMode == 14) || (p_person->U.UPerson.AnimMode == 15))
            DIFFTEST_RETURN(dist_sq);
        anim = gun_out_anim(p_person, 1);
        if ((ushort)anim == p_person->U.UPerson.AnimMode)
            DIFFTEST_RETURN(dist_sq);
        p_person->U.UPerson.AnimMode = (ubyte)anim;
        p_person->Frame = nstart_ani[people_frames[p_person->SubType][(ubyte)anim]
          + p_person->U.UPerson.Angle];
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
        DIFFTEST_RETURN(dist_sq);
    }

    if ((p_person->U.UPerson.AnimMode == 14) || (p_person->U.UPerson.AnimMode == 15))
    {
        p_person->U.UPerson.AnimMode = gun_out_anim(p_person, 0);
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
        p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
          + p_person->U.UPerson.Angle];
    }
    p_person->Speed = calc_person_speed(p_person);

    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        if ((p_vehicle->SubType == 0x32) &&
          (p_vehicle->U.UVehicle.ZebraOldHealth != p_vehicle->Health))
            p_person->U.UPerson.ComTimer -= 25;
        p_vehicle->U.UVehicle.ZebraOldHealth = p_vehicle->Health;
        timer = p_person->U.UPerson.ComTimer;
        p_person->U.UPerson.ComTimer = timer - 1;
        if (timer >= 0)
            DIFFTEST_RETURN(dist_sq);

        rnd = LbRandomAnyShort() & 0x7ff;
        if (things[p_person->U.UPerson.Vehicle].SubType != 0x3b)
            p_person->U.UPerson.ComTimer = 20;
        else
            p_person->U.UPerson.ComTimer = (LbRandomAnyShort() & 0x3f) + 0x5a;
        dz = (p_target->Z >> 8) + (lbSinTable[rnd] >> 6);
        dx = (p_target->X >> 8) + (lbSinTable[rnd + 512] >> 6);
        start_goto_vehicle(&things[p_person->U.UPerson.Vehicle], dx, dz);
        DIFFTEST_RETURN(dist_sq);
    }

    if (p_person->SubState == 0)
    {
        p_person->SubState = 1;
        p_person->U.UPerson.GotoX = p_target->X >> 8;
        p_person->U.UPerson.GotoZ = p_target->Z >> 8;
    }
    old_state = p_person->State;
    do_person_gotothingindex((int)p_person, old_state, 0, 0);

    if ((ushort)p_person->State == 0)
    {
        dx = (p_person->X - p_target->X) >> 8;
        dz = (p_person->Z - p_target->Z) >> 8;
        dist_sq = dx * dx + dz * dz;
        range_sq = (p_person->U.UPerson.ComRange * p_person->U.UPerson.ComRange) << 12;
        if (dist_sq >= range_sq)
        {
            p_person->SubState = 0;
            p_person->State = old_state;
            DIFFTEST_RETURN(0);
        }
        if ((p_person->Flag & TngF_PlayerAgent) == 0)
            DIFFTEST_RETURN(dist_sq);
        p_person->SubState = 0;
        p_person->State = old_state;
        DIFFTEST_RETURN(dist_sq);
    }

    if (p_person->U.UPerson.ComTimer == 1)
    {
        dx = p_target->X >> 8;
        dz = p_target->Z >> 8;
        if ((p_person->U.UPerson.GotoX - dx) * (p_person->U.UPerson.GotoX - dx)
          + (p_person->U.UPerson.GotoZ - dz) * (p_person->U.UPerson.GotoZ - dz) > 0x40000)
        {
            p_person->U.UPerson.GotoX = dx;
            p_person->U.UPerson.GotoZ = p_target->Z >> 8;
            remove_path(p_person);
            p_person->U.UPerson.ComTimer = -1;
        }
    }
    DIFFTEST_RETURN(dist_sq);
}

static DIFFTEST_REF_FUNC void person_goto_point_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_goto_point\n"
        : : "a" (p_person));
}

void person_goto_point(struct Thing *p_person)
{
    struct MyPath *p_path;
    s32 dx, dz;
    s32 dist, lim;
    s32 adx, adz;
    s32 vx, vz;
    ushort angle;
    short mv;
    short tmp;

    DIFFTEST_SHADOW(person_goto_point, person_goto_point_asm(p_person));

    dx = 0;
    dz = 0;
    if ((p_person->Flag & TngF_Unkn00020000) != 0)
        p_person->U.UPerson.LastDist = 0x7d00;
    if (p_person->U.UPerson.PathIndex != 0)
        unkn_path_func_001(p_person, 1);

    if ((p_person->U.UPerson.AnimMode == 14) || (p_person->U.UPerson.AnimMode == 15))
    {
        p_person->U.UPerson.AnimMode = gun_out_anim(p_person, 0);
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
        p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
          + p_person->U.UPerson.Angle];
    }

    if (p_person->U.UPerson.PathIndex != 0)
    {
        p_path = &my_paths[p_person->U.UPerson.PathIndex];
        dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
        dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
        adx = abs(dx);
        adz = abs(dz);
        if (adx > adz)
            dist = (adz >> 1) + adx;
        else
            dist = (adx >> 1) + adz;

        if (p_person->PathOffset + 1 < p_path->Flag) {
            lim = 30;
        } else {
            lim = p_person->U.UPerson.ComRange << 6;
            if (lim < 30)
                lim = 30;
        }
        if (dist < lim) {
            p_person->U.UPerson.ComTimer = -100;
        } else if ((p_person->Speed > 0) && (dist >= (ushort)p_person->U.UPerson.LastDist)) {
            p_person->U.UPerson.ComTimer = -100;
        }
        p_person->U.UPerson.LastDist = dist;
    }

    if (p_person->U.UPerson.ComTimer < 0)
    {
        p_person->Flag &= ~TngF_Unkn00020000;
        if (p_person->U.UPerson.PathIndex == 0)
        {
            build_navigate_path(p_person, p_person->U.UPerson.GotoX,
              p_person->U.UPerson.GotoZ, 0);
            p_path = &my_paths[p_person->U.UPerson.PathIndex];
            dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
            dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
        }
        dist = LbSqrL(dx * dx + dz * dz);
        if (dist == 0)
            dist = 1;
        if (p_person->U.UPerson.ComTimer == -100)
        {
            p_person->U.UPerson.LastDist = 0x7d00;
            if ((ubyte)do_next_navigate_index((int)p_person, 0, 0, 0) != 0)
            {
                p_person->U.UPerson.PathIndex = 0;
                p_person->State = 0;
            }
            else
            {
                p_path = &my_paths[p_person->U.UPerson.PathIndex];
                dx = p_path->X[p_person->PathOffset & 3] - (p_person->X >> 8);
                dz = p_path->Z[p_person->PathOffset & 3] - (p_person->Z >> 8);
                dist = LbSqrL(dx * dx + dz * dz);
            }
        }
        if (dist == 0)
            dist = 1;
        vx = (dx << 8) / dist;
        vz = (dz << 8) / dist;
        if (p_person->Speed < 16)
            p_person->U.UPerson.ComTimer = 10;
        else
            p_person->U.UPerson.ComTimer = dist / (p_person->Speed >> 4);
        p_person->VX = vx;
        p_person->VZ = vz;
        angle = (((ushort)arctan(vx, -vz) + 0x80) & 0x7ff) >> 8;
        if (angle != p_person->U.UPerson.Angle)
        {
            p_person->Frame -= nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
              + p_person->U.UPerson.Angle];
            p_person->U.UPerson.Angle = angle;
            p_person->Frame += nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
              + (ubyte)angle];
        }
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
    }

    p_person->U.UPerson.ComTimer--;
    mv = person_move(p_person);
    if (mv != 0)
    {
        if ((p_person->Flag & TngF_Destroyed) != 0)
            return;
        if ((p_person->Flag & TngF_Unkn00020000) != 0)
        {
            if (((p_person->Flag & TngF_Unkn01000000) != 0) && ((gameturn & 1) != 0))
            {
                p_person->U.UPerson.ComTimer = 2;
                p_person->VX = p_person->VX * 2;
                p_person->VZ = p_person->VZ * 2;
            }
            else
            {
                p_person->U.UPerson.ComTimer = 3;
                p_person->VX = -p_person->VX;
                p_person->VZ = -p_person->VZ;
            }
        }
        else if ((word_1AA392 == 0) && ((p_person->Flag2 & TgF2_Unkn0080) == 0))
        {
            p_person->Flag |= TngF_Unkn00020000;
            p_person->U.UPerson.ComTimer = 3;
            p_person->U.UPerson.BumpCount++;
            if (((p_person->Flag & TngF_Unkn01000000) != 0) && ((gameturn & 1) != 0))
            {
                p_person->U.UPerson.ComTimer = 5;
                p_person->VX = -p_person->VX;
                p_person->VZ = -p_person->VZ;
            }
            else if (mv < 0)
            {
                tmp = (short)p_person->VX;
                p_person->VX = p_person->VZ >> 2;
                p_person->VZ = (-(s32)tmp) >> 2;
            }
            else
            {
                tmp = (short)p_person->VX;
                p_person->VX = p_person->VZ;
                p_person->VZ = tmp;
                p_person->VZ = -(s32)tmp;
            }
        }
    }

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        p_person->Frame = frame[(ushort)p_person->Frame].Next;
    }
    if ((p_person->State == 0) && ((p_person->Flag2 & TgF2_Unkn0001) != 0))
        finalise_razor_wire(p_person);
}

void person_goto_point_rel(struct Thing *p_person)
{
    asm volatile ("call ASM_person_goto_point_rel\n"
        : : "a" (p_person));
}

void vehicle_passenger_list_add_first(struct Thing *p_vehicle, ThingIdx passngr)
{
    struct Thing *p_passngr;
    ThingIdx nxpassngr;

    p_passngr = &things[passngr];
    nxpassngr = p_vehicle->U.UVehicle.PassengerHead;
    p_vehicle->U.UVehicle.PassengerHead = passngr;
    p_passngr->U.UPerson.LinkPassenger = nxpassngr;
}

void vehicle_passenger_list_remove(struct Thing *p_vehicle, ThingIdx passngr)
{
    struct Thing *p_passngr;
    struct Thing *p_pvpassngr;
    ThingIdx crpassngr, pvpassngr;
    ushort k;

    p_passngr = &things[passngr];

    // If removing first passenger
    if (passngr == p_vehicle->U.UVehicle.PassengerHead)
    {
        p_vehicle->U.UVehicle.PassengerHead = p_passngr->U.UPerson.LinkPassenger;
        return;
    }

    k = 0;
    crpassngr = p_vehicle->U.UVehicle.PassengerHead;
    pvpassngr = 0;
    while (crpassngr != 0)
    {
        if (k >= VEHICLE_PASSENGER_LIMIT)
            break;
        if (crpassngr == passngr) {
            p_pvpassngr = &things[pvpassngr];
            p_pvpassngr->U.UPerson.LinkPassenger = p_passngr->U.UPerson.LinkPassenger;
            break;
        }
        pvpassngr = crpassngr;
        p_pvpassngr = &things[pvpassngr];
        crpassngr = p_pvpassngr->U.UPerson.LinkPassenger;
        k++;
    }
}

TbBool person_is_in_a_vehicle(struct Thing *p_person)
{
    return ((p_person->Flag & TngF_InVehicle) != 0);
}

TbBool person_is_in_a_train(struct Thing *p_person)
{
    struct Thing *p_vehicle;

    if ((p_person->Flag & TngF_InVehicle) == 0)
        return false;

    p_vehicle = &things[p_person->U.UPerson.Vehicle];
    return (p_vehicle->SubType == SubTT_VEH_TRAIN);
}

TbBool person_is_in_vehicle(struct Thing *p_person, ThingIdx vehicle)
{
    if ((p_person->Flag & TngF_InVehicle) == 0)
        return false;
    return (vehicle == p_person->U.UPerson.Vehicle);
}

TbBool person_is_standing_on_vehicle(struct Thing *p_person)
{
    struct Thing *p_standtng;

    if (p_person->U.UPerson.StandOnThing <= 0)
        return false;
    if ((p_person->Flag & TngF_StandOnVehicle) == 0)
        return false;
    p_standtng = &things[p_person->U.UPerson.StandOnThing];

    return (p_standtng->Type == TT_VEHICLE);
}

void person_enter_vehicle(struct Thing *p_person, struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_person_enter_vehicle\n"
        : : "a" (p_person), "d" (p_vehicle));
#endif
    if ((p_vehicle->SubType == SubTT_VEH_SHIP) && (p_vehicle->State == VehSt_UNKN_40))
    {
        // Allow ships to be entered when in motion, because we just do not have the proper functionality to stop them
    }
    else if ((p_vehicle->State != VehSt_PARKED_PARAL) && (p_vehicle->State != VehSt_PARKED_PERPN) && (p_vehicle->State != VehSt_NONE))
    {
        if ((debug_log_things & 0x01) != 0) {
            LOGSYNC_F("Person %s %d state %d.%d cannot enter %s %d state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              p_person->State, p_person->SubState,
              vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
              p_vehicle->State, p_vehicle->SubState);
        }
        return;
    }

    p_person->Flag |= TngF_InVehicle|TngF_Unkn02000000;
    p_person->Flag &= ~TngF_StandOnVehicle;
    p_person->State = PerSt_DRIVING_VEHICLE;

    vehicle_passenger_list_add_first(p_vehicle, p_person->ThingOffset);

    if ((p_person->Flag2 & TgF2_ExistsOffMap) == 0)
        delete_node(p_person);
    p_person->X = p_vehicle->X;
    p_person->Y = p_vehicle->Y;
    p_person->Z = p_vehicle->Z;
    p_vehicle->U.UVehicle.EffectiveGroup = p_person->U.UPerson.EffectiveGroup;

    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
        set_person_animmode_walk(p_person);
}

void person_go_sleep(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_go_sleep\n"
        : : "a" (p_person));
    return;
#endif
    if (p_person->State == PerSt_PERSON_BURNING) {
        return;
    }
    if (((p_person->Flag & TngF_Destroyed) != 0) ||
      (p_person->State == PerSt_DIEING)) {
        return;
    }
    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
    {
        p_person->U.UPerson.BumpCount = 150;
    }
    else
    {
        ubyte PrevAnimMode;
        p_person->Flag |= TngF_Unkn0001;

        PrevAnimMode = p_person->U.UPerson.AnimMode;
        if (PrevAnimMode != ANIM_PERS_PUSH_BACK)
            p_person->U.UPerson.OldAnimMode = PrevAnimMode;
         // make sure to draw frame without blood
        p_person->U.UPerson.FrameId.Version[4] = 0;
        p_person->U.UPerson.FrameId.Version[3] = 0;
        set_person_anim_mode(p_person, ANIM_PERS_LAY_DOWN);

        p_person->U.UPerson.RecoilTimer = 0;
        p_person->Timer1 = 48;
        p_person->StartTimer1 = 48;
        p_person->U.UPerson.BumpCount = 150;
        p_person->Flag &= ~(TngF_TriggerUse|TngF_WepRecoil);
        p_person->Flag2 |= TgF2_KnockedOut;
    }
}

static DIFFTEST_REF_FUNC ubyte vector_in_way_asm(struct Thing *p_thing, int dx, int dz)
{
    ubyte ret;
    asm volatile (
      "call ASM_vector_in_way\n"
        : "=a" (ret) : "a" (p_thing), "d" (dx), "b" (dz));
    return ret;
}

short do_move_colide(struct Thing *p_person, int dx, int dz, struct MyMapElement *p_mapel);

ubyte vector_in_way(struct Thing *p_thing, int dx, int dz)
{
    s32 cor_x, cor_z;
    s32 tgt_x, tgt_z;
    s32 adx, adz;
    s32 dist, steps;
    s32 step_x, step_z;
    s32 i;

    DIFFTEST_SHADOW_RET(vector_in_way, ubyte, vector_in_way_asm(p_thing, dx, dz));
    cor_x = p_thing->X >> 8;
    cor_z = p_thing->Z >> 8;
    tgt_x = dx + cor_x;
    tgt_z = cor_z + dz;
    adz = abs(tgt_z - cor_z);
    adx = abs(tgt_x - cor_x);
    // Approximate distance
    if (adx < adz)
        dist = adz - (adz >> 5) - (adz >> 7) + (adx >> 2) + (adx >> 3) + (adx >> 6) + (adx >> 7);
    else
        dist = adx - (adx >> 5) - (adx >> 7) + (adz >> 2) + (adz >> 3) + (adz >> 6) + (adz >> 7);
    steps = (dist >> 6) + 1;
    step_x = (tgt_x - cor_x) / steps;
    step_z = (tgt_z - cor_z) / steps;

    if (steps < 0)
        DIFFTEST_RETURN((ubyte)1);
    for (i = 0; i <= steps; i++)
    {
        struct MyMapElement *p_mapel;

        p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (cor_z >> 8) + (cor_x >> 8)];
        if (do_move_colide(p_thing, dx << 8, dz << 8, p_mapel) != 0)
            DIFFTEST_RETURN((ubyte)0);
        cor_x += step_x;
        cor_z += step_z;
    }
    DIFFTEST_RETURN((ubyte)1);
}

static DIFFTEST_REF_FUNC ushort check_col_collision_asm(int x, int y, int z)
{
    ushort ret;
    asm volatile (
      "call ASM_check_col_collision\n"
        : "=a" (ret) : "a" (x), "d" (y), "b" (z));
    return ret;
}

ushort check_col_collision(int x, int y, int z)
{
    struct MyMapElement *p_mapel;
    s32 tile_x, tile_z;
    s32 alt0, alt1, alt2, alt3, alt;
    s32 fr_x, fr_z;
    s32 quad;
    short colmn, hlevel;

    DIFFTEST_SHADOW_RET(check_col_collision, ushort, check_col_collision_asm(x, y, z));
    tile_x = x >> 8;
    tile_z = z >> 8;
    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH) || (tile_z < 0) || (tile_z >= MAP_TILE_WIDTH))
        DIFFTEST_RETURN((ushort)0);

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
    // Altitudes at the 4 corners of the tile
    alt0 = p_mapel[0].Alt;
    alt1 = p_mapel[1].Alt;
    alt2 = p_mapel[MAP_TILE_WIDTH].Alt;
    alt3 = p_mapel[MAP_TILE_WIDTH + 1].Alt;
    if ((alt0 == alt1) && (alt2 == alt0) && (alt3 == alt2))
    {
        alt = alt1;
    }
    else
    {
        fr_x = x & 0xFF;
        fr_z = z & 0xFF;
        if (fr_x + fr_z < 256)
        {
            alt = alt0 + (((alt1 - alt0) * fr_x) >> 8);
            alt += ((alt2 - alt0) * fr_z) >> 8;
        }
        else
        {
            alt = alt3 + (((alt2 - alt3) * (256 - fr_x)) >> 8);
            alt += ((alt1 - alt3) * (256 - fr_z)) >> 8;
        }
    }
    if (alt > y)
        DIFFTEST_RETURN((ushort)0xFFFF);

    colmn = p_mapel->ColumnHead & 0x0FFF;
    if (colmn == 0)
        DIFFTEST_RETURN((ushort)0);

    if ((x & 0xFF) > 127)
        quad = ((z & 0xFF) > 127) ? 2 : 1;
    else
        quad = ((z & 0xFF) > 127) ? 3 : 0;

    hlevel = (short)((s32)(((u32)y << 3) - ((u32)p_mapel->Alt << 3)) >> 7);
    if (hlevel > 31)
        DIFFTEST_RETURN((ushort)0);

    DIFFTEST_RETURN((ushort)(game_col_columns[colmn].QBits[quad] & (3u << (hlevel & 0x1F))));
}

ThingIdx thing_collide_with_thing_when_moved_by(struct Thing *p_person, short sh_x, short sh_z)
{
    int nxcor_x, nxcor_z;
    ThingIdx thing, nxthing;

    nxcor_x = PRCCOORD_TO_MAPCOORD(p_person->X) + sh_x;
    nxcor_z = PRCCOORD_TO_MAPCOORD(p_person->Z) + sh_z;

    if ((nxcor_x < 0) || (nxcor_x >= MAP_COORD_WIDTH) || (nxcor_z < 0) || (nxcor_z >= MAP_COORD_HEIGHT))
        return 0;

    thing = get_mapwho_thing_index(MAPCOORD_TO_TILE(nxcor_x), MAPCOORD_TO_TILE(nxcor_z));
    for (; thing != 0; thing = nxthing)
    {
        int dist_x, dist_z, r;
        if (thing < 0)
        {
            struct SimpleThing *p_sthing;
            p_sthing = &sthings[thing];
            dist_x = nxcor_x - PRCCOORD_TO_MAPCOORD(p_sthing->X);
            dist_z = nxcor_z - PRCCOORD_TO_MAPCOORD(p_sthing->Z);
            r = p_person->Radius + p_sthing->Radius;
            nxthing = p_sthing->Next;
        }
        else
        {
            struct Thing *p_thing;
            p_thing = &things[thing];
            dist_x = nxcor_x - PRCCOORD_TO_MAPCOORD(p_thing->X);
            dist_z = nxcor_z - PRCCOORD_TO_MAPCOORD(p_thing->Z);
            r = p_person->Radius + p_thing->Radius;
            nxthing = p_thing->Next;
        }
        if (LbSqrL(dist_x * dist_x + dist_z * dist_z) <= r)
        {
            break;
        }
    }
    return thing;
}

TbBool check_ground_unkn01(struct Thing *p_person, short sh_x, short sh_z)
{
    struct MyMapElement *p_mapel;
    ushort tflags;
    MapCoord cor_x, cor_z;
    short tile_x, tile_z;
    ushort txtr;
    TbBool tile_blocking;

    cor_x = PRCCOORD_TO_MAPCOORD(p_person->X) + sh_x;
    cor_z = PRCCOORD_TO_MAPCOORD(p_person->Z) + sh_z;
    tile_x = MAPCOORD_TO_TILE(cor_x);
    tile_z = MAPCOORD_TO_TILE(cor_z);

    tile_blocking = compute_map_tile_is_blocking_walk(tile_x, tile_z);

    txtr = floor_texture_at_point(cor_x, cor_z);
    tflags = ((get_my_texture_bits(txtr) & 0xC0) >> 6) & 0x02;

    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH) || (tile_z < 0) || (tile_z >= MAP_TILE_HEIGHT))
    {
      return true;
    }

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];

    if (tile_blocking)
        tflags |= 0x04;
    if ((p_mapel->Flags2 & MEF2_Unkn02) != 0)
        tflags &= ~0x04;
    if (((tflags & 0x04) != 0) || (p_mapel->Flags2 & (MEF2_Unkn01|MEF2_Unkn04)) != 0)
    {
        return true;
    }
    return false;
}


short check_col_collision_when_moved_by(struct Thing *p_person, short sh_x, short sh_z)
{
    int cor_x, cor_y, cor_z;
    int dist;
    int dt_x, dt_z;

    dist = LbSqrL(sh_x * sh_x + sh_z * sh_z) >> 6;
    if (dist == 0)
        dist = 1;
    cor_x = p_person->X >> 8;
    cor_z = p_person->Z >> 8;
    cor_y = (alt_at_point(cor_x, cor_z) >> 8) + 3;
    dt_x = sh_x / dist;
    dt_z = sh_z / dist;
    while (dist != 0)
    {
        short qbit;

        qbit = check_col_collision(cor_x, cor_y, cor_z);
        if (qbit != 0)
        {
            return qbit;
        }
        cor_z += dt_z;
        cor_x += dt_x;
        dist--;
        cor_y = (alt_at_point(cor_x, cor_z) >> 8) + 3;
    }
    return 0;
}

void person_protect_update_follow_distance(struct Thing *p_person, ThingIdx leadtng)
{
    struct Thing *p_leadtng;
    short plagent, leadagnt;

    if (leadtng > 0) {
        p_leadtng = &things[leadtng];
        leadagnt = p_leadtng->U.UPerson.ComCur & 3;
    } else {
        leadagnt = 0;
    }
    plagent = p_person->U.UPerson.ComCur & 3;
    p_person->U.UPerson.ComRange = follow_dist[leadagnt][plagent];
}

ubyte person_leave_vehicle(struct Thing *p_person, struct Thing *p_vehicle)
{
#if 0
    ubyte ret;
    asm volatile (
      "call ASM_person_leave_vehicle\n"
        : "=r" (ret) : "a" (p_person), "d" (p_vehicle));
    return ret;
#endif
    int sh_x, sh_z;
    int can_move;

    if ((p_vehicle->State != VehSt_PARKED_PARAL) && (p_vehicle->State != VehSt_PARKED_PERPN) &&
      (p_vehicle->State != VehSt_UNKN_D))
    {
        if ((p_vehicle->State != VehSt_UNKN_33) &&  (p_vehicle->State != VehSt_UNKN_36) &&
          (p_vehicle->State != VehSt_UNKN_3E) && (p_vehicle->State != VehSt_UNKN_38))
            p_vehicle->State = VehSt_UNKN_3C;
        return 1;
    }
    if ((p_person->Flag & TngF_InVehicle) == 0)
    {
        return 2;
    }

    can_move = 0;
    sh_x = 0;
    sh_z = 0;
    if ((p_vehicle->SubType != SubTT_VEH_FLYING)
      || alt_at_point(p_vehicle->X >> 8, p_vehicle->Z >> 8) + 3500 >= p_vehicle->Y)
    {
        int dt_angle;

        for (dt_angle = 256; dt_angle < 2048; )
        {
            short angle;
            ushort radius;
            int cor_x, cor_z;

            angle = (dt_angle + p_vehicle->U.UVehicle.AngleY) & 0x7FF;
            radius = p_vehicle->Radius;
            sh_z = -(radius * lbSinTable[angle + LbFPMath_PI/2]) >> 16;
            sh_x = (radius * lbSinTable[angle]) >> 16;

            cor_x = (p_person->X >> 8) + sh_x;
            cor_z = (p_person->Z >> 8) + sh_z;
            can_move = (cor_z >= 0) && (cor_z < MAP_COORD_HEIGHT) && (cor_x >= 0) && (cor_x < MAP_COORD_WIDTH);

            if (can_move && check_ground_unkn01(p_person, sh_x, sh_z)) {
                can_move = 0;
            }
            if (can_move) {
                can_move = vector_in_way(p_person, sh_x, sh_z);
            }
            if (can_move && check_col_collision_when_moved_by(p_person, sh_x, sh_z)) {
                can_move = 0;
            }
            if (can_move && thing_collide_with_thing_when_moved_by(p_person, sh_x, sh_z)) {
                can_move = 0;
            }
            dt_angle += 180;
            if (dt_angle >= 1024 && dt_angle < 1280)
                dt_angle = 1280;
            if (can_move != 0)
                break;
        }
    }

    if ( can_move || (p_vehicle->State == VehSt_UNKN_D))
    {
      struct Command *p_cmd;
      struct Command *p_nxcmd;

      p_person->Flag &= ~(TngF_InVehicle|TngF_Unkn02000000);
      vehicle_passenger_list_remove(p_vehicle, p_person->ThingOffset);
      p_person->State = PerSt_NONE;

      p_cmd = &game_commands[p_person->U.UPerson.ComCur];
      p_nxcmd = &game_commands[p_cmd->Next];

      if ((p_person->U.UPerson.ComHead != 0) &&
        ((p_nxcmd->Type == PCmd_GO_TO_POINT) || (p_nxcmd->Type == PCmd_GOTOPOINT_FACE)
         || (p_nxcmd->Type == PCmd_RUN_TO_POINT)))
      {
          sh_x >>= 2;
          sh_z >>= 2;
      }
      p_person->X += sh_x << 8;
      p_person->Z += sh_z << 8;
      if ((p_person->X >> 8) >= MAP_COORD_WIDTH || (p_person->Z >> 8) >= MAP_COORD_HEIGHT)
      {
          p_person->X -= (sh_x << 8);
          p_person->Z -= (sh_z << 8);
      }
      p_person->Y = alt_at_point(p_person->X >> 8, p_person->Z >> 8);
      p_person->Flag |= TngF_Unkn0010|TngF_Unkn0004;

      struct MyMapElement *p_mapel;
      short tile_x, tile_z;

      tile_z = (p_person->Z >> 16);
      tile_x = (p_person->X >> 16);
      p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];

      if (((p_mapel->Flags2 & MEF2_Unkn01) != 0) && can_move)
          can_move = 0;

      if ((p_person->Flag2 & TgF2_ExistsOffMap) == 0)
          add_node_thing(p_person->ThingOffset);
      p_person->Timer1 = 48;
      p_person->StartTimer1 = 48;
      if (p_vehicle->U.UVehicle.PassengerHead == 0)
          p_vehicle->U.UVehicle.EffectiveGroup = 99;
      remove_path(p_person);

      if (((p_person->Flag & TngF_PlayerAgent) != 0) && (p_person->State == PerSt_PROTECT_PERSON))
      {
          person_protect_update_follow_distance(p_person, p_person->Owner);
      }

      if ((p_vehicle->Flag & TngF_Destroyed) != 0)
      {
        if (!can_move)
        {
          // SP stats are updated inside set_person_dead(), but MP stats not? maybe add a wrapper to include both?
          if (in_network_game)
              stats_mp_add_person_kills_person(p_vehicle->OldTarget, p_person->ThingOffset);
          set_person_dead(p_person, ANIM_PERS_Unkn10);
          return 0;
        }
        p_person->Health -= 800;
        if (p_person->Health <= 0)
        {
            set_person_dead(p_person, ANIM_PERS_Unkn10);
            return 0;
        }
        if ((p_person->Flag & TngF_PlayerAgent) == 0)
        {
            person_go_sleep(p_person);
        }
      }
    }
    return 0;
}

ubyte person_attempt_to_leave_vehicle(struct Thing *p_person)
{
#if 0
    ubyte ret;
    asm volatile (
      "call ASM_person_attempt_to_leave_vehicle\n"
        : "=r" (ret) : "a" (p_person));
    return ret;
#endif
    struct Thing *p_vehicle;

    p_vehicle = &things[p_person->U.UPerson.Vehicle];
    if (p_vehicle->State == VehSt_UNKN_45) {
        return 0;
    }
    if ((p_vehicle->Flag & TngF_Destroyed) != 0) {
        return 0;
    }
    switch (p_vehicle->SubType)
    {
    case SubTT_VEH_TRAIN:
        break;
    case SubTT_VEH_TANK:
        if (p_vehicle->State != SubTT_VEH_UNKN33) {
            p_vehicle->U.UVehicle.GotoX = p_vehicle->X >> 8;
            p_vehicle->U.UVehicle.GotoZ = p_vehicle->Z >> 8;
            break;
        }
        person_leave_vehicle(p_person, p_vehicle);
        return 1;
    case SubTT_VEH_FLYING:
        if (p_vehicle->State != VehSt_PARKED_PARAL)
        {
            if (p_vehicle->State != VehSt_FLY_LANDING)
                start_landing(p_vehicle);
            p_person->State = VehSt_UNKN_36;
            break;
        }
        if (!person_leave_vehicle(p_person, p_vehicle))
        {
            if ((p_person->Flag & TngF_PlayerAgent) != 0)
                p_person->State = PerSt_NONE;
            break;
        }
        return 1;
    case SubTT_VEH_SHIP:
    default:
        if ((p_vehicle->State != VehSt_PARKED_PARAL) && (p_vehicle->State != VehSt_PARKED_PERPN)
          && (p_vehicle->State != VehSt_NONE)
          && (p_vehicle->State != VehSt_UNKN_3E) && (p_vehicle->State != VehSt_UNKN_38))
        {
            if ((p_vehicle->State != VehSt_UNKN_33) && (p_vehicle->State != VehSt_UNKN_36))
                p_vehicle->State = VehSt_UNKN_3C;
            p_person->State = PerSt_WAIT_TO_EXIT_VEHICLE;
            break;
        }
        person_leave_vehicle(p_person, p_vehicle);
        return 1;
    }
    return 0;
}

void make_peep_protect_peep(struct Thing *p_protector, struct Thing *p_leader)
{
    asm volatile ("call ASM_make_peep_protect_peep\n"
        : : "a" (p_protector), "d" (p_leader));
}

void player_change_person(short thing, ushort plyr)
{
#if 0
    asm volatile ("call ASM_player_change_person\n"
        : : "a" (thing), "d" (plyr));
#endif
    struct Thing *p_person;

    {
        struct Thing *p_dcthing;
        short dcthing;
        dcthing = players[plyr].DirectControl[0];
        p_dcthing = &things[dcthing];
        p_dcthing->Flag &= ~TngF_SelectedAgent;
        // Avoid shooting a target not intended for that, set for previous state
        p_dcthing->PTarget = NULL;
    }

    p_person = &things[thing];
    if ((p_person->Flag2 & TgF2_Unkn10000000) != 0)
    {
        p_person->GotoThingIndex = p_person->Owner;
        p_person->State = PerSt_PROTECT_PERSON;
        person_protect_update_follow_distance(p_person, p_person->GotoThingIndex);
        p_person->Flag2 &= ~TgF2_Unkn10000000;
    }
    players[plyr].DirectControl[0] = thing;
    p_person->U.UPerson.Target2 = 0;
    p_person->Flag |= TngF_SelectedAgent;

    if (p_person->State == PerSt_PROTECT_PERSON)
    {
        struct Thing *p_protng;
        ushort plagent;

        p_protng = &things[p_person->GotoThingIndex];
        if (((p_protng->Flag & (TngF_Unkn40000000|TngF_Destroyed)) == 0) &&
          (p_protng->State != PerSt_PROTECT_PERSON))
        {
            make_peep_protect_peep(p_protng, p_person);
        }
        for (plagent = 0; plagent < playable_agents; plagent++)
        {
            struct Thing *p_agent;

            p_agent = players[plyr].MyAgent[plagent];
            if ((p_agent != p_person) && (p_agent->State == PerSt_PROTECT_PERSON) &&
              (p_agent->GotoThingIndex == p_person->GotoThingIndex))
            {
                p_agent->GotoThingIndex = p_person->ThingOffset;
                p_agent->Owner = p_person->ThingOffset;
                person_protect_update_follow_distance(p_agent, p_agent->GotoThingIndex);
                remove_path(p_agent);
            }
        }
        p_person->State = PerSt_NONE;
        p_person->Owner = 0;
        p_person->GotoThingIndex = 0;
    }
}

ThingIdx person_find_ferry_to_catch(struct Thing *p_person)
{
    struct Command *p_cmd;
    int radius;
    ThingIdx thing;
    short cor_x, cor_y, cor_z;

    p_cmd = &game_commands[p_person->U.UPerson.ComCur];
    radius = (p_cmd->Arg1 << 6);
    cor_x = PRCCOORD_TO_MAPCOORD(p_person->X);
    cor_y = PRCCOORD_TO_MAPCOORD(p_person->Y);
    cor_z = PRCCOORD_TO_MAPCOORD(p_person->Z);

    thing = search_for_ferry(cor_x, cor_y, cor_z, radius);

    return thing;
}

void person_catch_ferry(struct Thing *p_person)
{
    struct Thing *p_vehicle;
    ThingIdx veh;

    if ((p_person->Flag & TngF_InVehicle) != 0)
        return;
    person_goto_point(p_person);
    p_person->State = PerSt_CATCH_FERRY;
    veh = person_find_ferry_to_catch(p_person);
    if (veh == 0)
        return;

    p_vehicle = &things[veh];
    person_enter_vehicle(p_person, p_vehicle);
    p_person->State = PerSt_NONE;
    p_person->U.UPerson.Vehicle = veh;
}

void make_peep_flee(int b_x, int b_z, struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_make_peep_flee\n"
        : : "a" (b_x), "d" (b_z), "b" (p_person));
#endif
    short full_angle;

    if ((p_person->Flag2 & (TgF2_Unkn0008|TgF2_KnockedOut)) != 0) {
        return;
    }
    if (p_person->State == PerSt_PERSON_BURNING) {
        return;
    }
    if ((p_person->Flag & (TngF_Persuaded|TngF_DangerFlee|TngF_WepRecoil)) != 0) {
        return;
    }
    full_angle = LbArcTanAngle(b_x, -b_z);
    change_person_angle_full(p_person, full_angle);
    set_person_animmode_run(p_person);
    p_person->U.UPerson.Mood = -64;
    p_person->U.UPerson.RecoilTimer = 50;
    p_person->Flag |= TngF_DangerFlee;
    p_person->U.UPerson.RecoilDir = full_angle >> 3;
    p_person->Flag &= ~TngF_WepRecoil;
}

void alert_person_to_madman(struct Thing *p_person, struct MapCoords *p_alert_pos, struct Thing *p_madman)
{
    if (p_person->Type != TT_PERSON ||
      (p_person->Flag & (TngF_PlayerAgent|TngF_Destroyed)) != 0)
        return;

    if (person_is_basic_civilian(p_person, true))
    {
        if (((p_person->Flag2 & TgF2_IgnoreEnemies) == 0) &&
          (person_mod_brain_level(p_person) < 3) &&
          (!persons_have_truce(p_person, p_madman)))
        {
            short dx, dz;
            dx = PRCCOORD_TO_MAPCOORD(p_person->X) - p_alert_pos->X;
            dz = PRCCOORD_TO_MAPCOORD(p_person->Z) - p_alert_pos->Z;
            // Make the escape direction slightly related to the direction the person was facing
            dx += angle_direction[p_person->U.UPerson.Angle].DiX;
            dz += angle_direction[p_person->U.UPerson.Angle].DiY;
            make_peep_flee(dx, dz, p_person);
        }
    }
    else
    {
        short check_grp, target_grp;
        check_grp = p_person->U.UPerson.EffectiveGroup & PEOPLE_GROUPS_INDEX_MASK;
        target_grp = p_madman->U.UPerson.EffectiveGroup & PEOPLE_GROUPS_INDEX_MASK;
        if (check_grp != target_grp)
        {
            if (!groups_have_truce(check_grp, target_grp) &&
              (groups_have_kill_on_sight(check_grp, target_grp) ||
              groups_have_kill_if_armed(check_grp, target_grp) ||
              groups_have_kill_if_weapon_out(check_grp, target_grp)))
            {
                set_interrupt_target(p_person, p_madman);
            }
        }
    }
}

void alert_peeps_on_mapwho_tile(short tile_x, short tile_z,
  struct MapCoords *p_alert_pos, struct Thing *p_madman)
{
    ThingIdx thing;
    ulong k;

    k = 0;
    thing = get_mapwho_thing_index(tile_x, tile_z);
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
            // Per thing code start
            alert_person_to_madman(p_thing, p_alert_pos, p_madman);
            // Per thing code end
            thing = p_thing->Next;
        }
        k++;
        if (k >= STHINGS_LIMIT+THINGS_LIMIT) {
            LOGERR("Infinite loop in mapwho (%d,%d) things list",
              (int)tile_x, (int)tile_z);
            bad_mapwho_tl_x = tile_x;
            bad_mapwho_tl_z = tile_z;
            break;
        }
    }
    return;
}

void alert_peeps(int cor_x, int cor_y, int cor_z, struct Thing *p_madman)
{
#if 0
    asm volatile (
      "call ASM_alert_peeps\n"
        : : "a" (cor_x), "d" (cor_y), "b" (cor_z), "c" (p_madman));
#endif
    struct MapCoords alert_pos;
    short tile_x, tile_z;
    short dx, dz;

    alert_pos.X = cor_x;
    alert_pos.Y = cor_y;
    alert_pos.Z = cor_z;
    tile_x = MAPCOORD_TO_TILE(cor_x);
    tile_z = MAPCOORD_TO_TILE(cor_z);
    for (dx = -ALERT_PEEPS_RANGE; dx <= ALERT_PEEPS_RANGE; dx++)
    {
        for (dz = -ALERT_PEEPS_RANGE; dz <= ALERT_PEEPS_RANGE; dz++)
        {
            alert_peeps_on_mapwho_tile(tile_x + dx, tile_z + dz, &alert_pos, p_madman);
        }
    }
}

void person_attempt_to_leave_ferry(struct Thing *p_person)
{
    struct Command *p_cmd;
    struct Thing *p_vehicle;
    int radius_sqr;
    short state_bkp;

    p_cmd = &game_commands[p_person->U.UPerson.ComCur];
    radius_sqr = (p_cmd->Arg1 << 6) * (p_cmd->Arg1 << 6);
    p_vehicle = &things[p_person->U.UPerson.Vehicle];
    //TODO do not draw from thing state update
    enlist_hud_draw_line_from_thing_ptr_to_mapcoord(p_person,
      p_cmd->X, p_cmd->Y, p_cmd->Z, 0, ColLU_RED);
    enlist_hud_draw_line_from_thing_ptr_to_mapcoord(p_vehicle,
      p_cmd->X, p_cmd->Y, p_cmd->Z, 0, ColLU_BLUE);

    if ((p_vehicle->U.UVehicle.LeisurePlace == 0) || (p_vehicle->Speed != 0)) {
        return;
    }
    if (!thing_arrived_at_obj_radius(p_person->ThingOffset,
      p_cmd->X, p_cmd->Y, p_cmd->Z, radius_sqr)) {
        return;
    }

    state_bkp = p_vehicle->State;
    p_vehicle->State = VehSt_NONE;
    person_leave_vehicle(p_person, p_vehicle);
    p_vehicle->State = state_bkp;

    delete_node(p_person);
    p_person->X = MAPCOORD_TO_PRCCOORD(p_cmd->X,0);
    p_person->Y = alt_at_point(p_cmd->X, p_cmd->Z);
    p_person->Z = MAPCOORD_TO_PRCCOORD(p_cmd->Z,0);
    add_node_thing(p_person->ThingOffset);
    p_person->State = PerSt_NONE;
    p_person->Flag |= TngF_Unkn0004;
}

void thing_shoot_at_point(struct Thing *p_thing, short x, short y, short z, uint fast_flag)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_thing_shoot_at_point\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z), "g" (fast_flag));
    return;
#endif
    PlayerInfo *p_player;
    short src_x, src_y, src_z;
    int weapon_range;
    short face;
    PlayerIdx plyr;
    ushort plagent;

    face = 0;
    if ((fast_flag & 0x02) != 0)
    {
        int height;

        face = y;
        if (y <= 0)
            height = get_height_on_face_quad(x << 8, z << 8, -y);
        else
            height = get_height_on_face(x << 8, z << 8, y);
        y = (height >> 8) + 20;
    }

    p_thing->Flag |= TngF_TriggerUse;
    if ((p_thing->Flag & TngF_InVehicle) != 0) {
        struct Thing *p_vehicle;
        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        p_vehicle->Flag |= TngF_Unkn01000000;
    }

    if ((p_thing->Flag & (TngF_Unkn40000000|TngF_StationrSht|TngF_Destroyed)) != 0) {
        return;
    }

    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0) {
        finalise_razor_wire(p_thing);
    }

    if (p_thing->State == PerSt_DROP_ITEM || p_thing->State == PerSt_PICKUP_ITEM ||
      p_thing->State == PerSt_DEAD || p_thing->State == PerSt_DIEING) {
        return;
    }
    if ((p_thing->Flag & TngF_Destroyed) != 0) {
        return;
    }

    {
        short full_angle;

        full_angle = angle_between_points(PRCCOORD_TO_MAPCOORD(p_thing->X),
          PRCCOORD_TO_MAPCOORD(p_thing->Z), x, z);
        change_person_angle_full(p_thing, full_angle);
    }

    p_thing->PTarget = NULL;
    p_thing->Flag |= TngF_ShootAtPos|TngF_TriggerUse;

    plyr = p_thing->U.UPerson.ComCur >> 2;
    plagent = p_thing->U.UPerson.ComCur & 3;
    p_player = &players[plyr];

    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
    {
        p_player->SpecialItems[0] = x;
        p_player->SpecialItems[1] = y;
        p_player->SpecialItems[2] = z;
    }

    src_x = PRCCOORD_TO_MAPCOORD(p_thing->X);
    src_y = PRCCOORD_TO_MAPCOORD(p_thing->Y);
    src_z = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    weapon_range = get_weapon_range(p_thing);
    map_limit_distance_to_target_fast(src_x, src_y, src_z,
      &x, &y, &z, weapon_range);

    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
    {
        player_agent_set_user_vect(plyr, plagent, x, y, z);
    }
    else
    {
        p_thing->VX = x;
        p_thing->VY = y;
        p_thing->VZ = z;
        p_thing->State = PerSt_NONE;
    }

    switch (p_thing->U.UPerson.CurrentWeapon)
    {
    case WEP_RAZORWIRE:
        init_lay_razor(p_thing, x, y, z, 0);
        if ((fast_flag & 0x02) == 0)
        {
            build_navigate_path(p_thing, x, z, 0);
            p_player->GotoFace = 0;
        } else
        {
            build_navigate_path_to_face_xz(p_thing, -face, x, z);
            p_player->GotoFace = face;
        }
        break;
    case WEP_EXPLWIRE:
        init_lay_razor(p_thing, x, y, z, 1);
        if ((fast_flag & 0x02) == 0)
        {
            build_navigate_path(p_thing, x, z, 0);
            p_player->GotoFace = 0;
        } else
        {
            build_navigate_path_to_face_xz(p_thing, -face, x, z);
            p_player->GotoFace = face;
        }
        break;
    }

    if ((fast_flag & 0x01) != 0) {
        set_person_animmode_run(p_thing);
    }
}

void call_protect(struct Thing *p_thing, ushort plyr)
{
    asm volatile ("call ASM_call_protect\n"
        : : "a" (p_thing), "d" (plyr));
}

void call_protect_specific(struct Thing *p_person_client, struct Thing *p_protector)
{
    asm volatile ("call ASM_call_protect_specific\n"
        : : "a" (p_person_client), "d" (p_protector));
}

ushort count_protect(struct Thing *p_thing, ushort plyr)
{
    ushort ret;
    asm volatile (
      "call ASM_count_protect\n"
        : "=r" (ret) : "a" (p_thing), "d" (plyr));
    return ret;
}

void call_unprotect(struct Thing *p_thing, ushort plyr, ubyte flag)
{
    asm volatile ("call ASM_call_unprotect\n"
        : : "a" (p_thing), "d" (plyr), "b" (flag));
}

TbBool weapon_can_target_enemy_thing(WeaponType wtype)
{
    switch (wtype)
    {
    case WEP_ELEMINE:
    case WEP_EXPLMINE:
    case WEP_MEDI1:
    case WEP_MEDI2:
    case WEP_RAZORWIRE:
    case WEP_EXPLWIRE:
        return false;

    case WEP_NULL:
    case WEP_NUCLGREN:
    case WEP_H2HTASER:
    case WEP_CRAZYGAS:
    case WEP_KOGAS:
    case WEP_NAPALMMINE:
    case WEP_AIRSTRIKE:
    case WEP_ENERGYSHLD:
    case WEP_CEREBUSIFF:
    case WEP_CLONESHLD:
    default:
        return true;
    }
}

void thing_shoot_at_target_position(struct Thing *p_thing, ThingIdx target)
{
    struct Thing *p_targetng;
    MapCoord cor_x, cor_y, cor_z;

    p_targetng = &things[target];
    cor_x = PRCCOORD_TO_MAPCOORD(p_targetng->X);
    cor_y = PRCCOORD_TO_MAPCOORD(p_targetng->Y);
    cor_z = PRCCOORD_TO_MAPCOORD(p_targetng->Z);
    thing_shoot_at_point(p_thing, cor_x, cor_y, cor_z, 0);
}

void thing_shoot_at_thing(struct Thing *p_thing, short target)
{
#if 0
    asm volatile ("call ASM_thing_shoot_at_thing\n"
        : : "a" (p_thing), "d" (target));
#endif
    struct Thing *p_targetng;

    if ((p_thing->Flag & (TngF_Unkn40000000|TngF_Destroyed)) != 0)
        return;
    p_targetng = &things[target];

    if (!weapon_can_target_enemy_thing(p_thing->U.UPerson.CurrentWeapon))
    {
        thing_shoot_at_target_position(p_thing, target);
        return;
    }

    p_thing->Flag |= TngF_TriggerUse;
    if ((p_thing->Flag & TngF_InVehicle) != 0) {
        struct Thing *p_vehicle;
        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        p_vehicle->Flag |= TngF_Unkn01000000;
    }
    if ((p_thing->Flag2 & TgF2_Unkn0001) != 0) {
        finalise_razor_wire(p_thing);
    }

    if (p_thing->State == PerSt_DROP_ITEM || p_thing->State == PerSt_PICKUP_ITEM ||
      p_thing->State == PerSt_DEAD || p_thing->State == PerSt_DIEING) {
        return;
    }
    if ((p_thing->Flag & TngF_Destroyed) != 0) {
        return;
    }

    if (((p_thing->Flag & TngF_InVehicle) != 0) &&
      (p_targetng->ThingOffset == p_thing->U.UPerson.Vehicle)) {
        struct Thing *p_vehicle;
        p_vehicle = &things[p_thing->U.UPerson.Vehicle];
        p_vehicle->Flag &= ~TngF_ShootAtPos;
        return;
    }

    {
        int dist_x, dist_y, dist_z, range;
        dist_x = PRCCOORD_TO_MAPCOORD(p_targetng->X) - PRCCOORD_TO_MAPCOORD(p_thing->X);
        dist_y = PRCCOORD_TO_MAPCOORD(p_targetng->Y) - PRCCOORD_TO_MAPCOORD(p_thing->Y);
        dist_z = PRCCOORD_TO_MAPCOORD(p_targetng->Z) - PRCCOORD_TO_MAPCOORD(p_thing->Z);
        range = get_weapon_range(p_thing);
        if (dist_x * dist_x + dist_y * dist_y + dist_z * dist_z > range * range) {
            thing_shoot_at_target_position(p_thing, target);
            return;
        }
    }
    p_thing->Flag &= ~(TngF_ShootAtPos|TngF_TriggerUse);

    p_thing->Flag |= TngF_TriggerUse;
    p_thing->PTarget = &things[target];

    p_targetng = p_thing->PTarget;
    if ((p_targetng->Flag & TngF_Destroyed) != 0)
    {
        if (weapon_can_be_charged(p_thing->U.UPerson.CurrentWeapon))
            p_thing->Flag2 |= TngF_Unkn00200000;
    }
    if ((p_thing->Flag & TngF_PlayerAgent) != 0)
        p_thing->U.UPerson.ComTimer = -1;
    else
        p_thing->State = PerSt_NONE;

    if (p_targetng != NULL)
    {
        short full_angle;

        full_angle = angle_between_points(
          PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_MAPCOORD(p_thing->Z),
          PRCCOORD_TO_MAPCOORD(p_targetng->X), PRCCOORD_TO_MAPCOORD(p_targetng->Z));
        change_person_angle_full(p_thing, full_angle);
    }
}

void person_init_plant_mine(struct Thing *p_person, short x, short y, short z, int face)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_person_init_plant_mine\n"
        : : "a" (p_person), "d" (x), "b" (y), "c" (z), "g" (face));
#endif
    if ((p_person->Flag & TngF_Unkn0001) != 0) {
        return;
    }
    if ((p_person->Flag & (TngF_StationrSht|TngF_Destroyed)) != 0) {
        return;
    }

    p_person->Flag2 &= ~(TgF2_Unkn00020000|TgF2_Unkn0040);

    if (p_person->U.UPerson.PathIndex != 0)
        remove_path(p_person);
    if (face != 0) {
        build_navigate_path_to_face_xz(p_person, -face, x, z);
    } else {
        build_navigate_path(p_person, x, z, 0);
    }

    if ((p_person->Flag2 & TgF2_Unkn0040) == 0)
    {
        p_person->State = PerSt_GO_PLANT_MINE;
        p_person->U.UPerson.ComTimer = -1;
        p_person->SubState = 0;
        p_person->U.UPerson.ComRange = 1;
        // The flag is raised again when planting, so this is just for early hint
        p_person->Flag2 |= TgF2_DroppedActivate;
        p_person->U.UPerson.GotoX = x;
        p_person->U.UPerson.GotoZ = z;
    }
}

void person_init_plant_mine_fast(struct Thing *p_thing, short x, short y, short z, int face)
{
#if 0
    asm volatile (
      "push %4\n"
      "call ASM_person_init_plant_mine_fast\n"
        : : "a" (p_thing), "d" (x), "b" (y), "c" (z), "g" (face));
#endif
    if ((p_thing->Flag & TngF_Unkn0001) != 0) {
        return;
    }

    person_init_plant_mine(p_thing, x, y, z, face);

    if ((p_thing->Flag2 & TgF2_Unkn0040) == 0)
    {
        set_person_animmode_run(p_thing);
    }
}

ubyte thing_select_specific_weapon(struct Thing *p_person, WeaponType wtype, ubyte flag)
{
#if 0
    int ret;
    asm volatile ("call ASM_thing_select_specific_weapon\n"
        : "=r" (ret) : "a" (p_person), "d" (wtype), "b" (flag));
    return ret;
#endif
    ubyte animode;

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return WepSel_SKIP;
    if (wtype == WEP_AIRSTRIKE && current_map == 65) // map065 The Moon
    {
        play_dist_sample(p_person, 129, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        p_person->U.UPerson.CurrentWeapon = WEP_NULL;
        return WepSel_SKIP;
    }

    if ((p_person->Flag2 & TgF2_Unkn0001) != 0)
        finalise_razor_wire(p_person);
    stop_looped_weapon_sample(p_person, p_person->U.UPerson.CurrentWeapon);

    if (flag == WepSel_SKIP)
        return WepSel_SKIP;
    if (!person_carries_weapon(p_person, wtype))
        return WepSel_SKIP;

#if 0 // Disabled by community consensus - medikit should not be auto-consumed when selected
    if (wtype == WEP_MEDI1)
    {
        if ((p_person->Flag & TngF_PlayerAgent) != 0) {
            PlayerIdx plyr;

            plyr = p_person->U.UPerson.ComCur >> 2;
            if (plyr == local_player_no)
                play_sample_using_heap(0, 2, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        }

        p_person->Health = p_person->U.UPerson.MaxHealth;
        person_weapons_remove_one(p_person, wtype);
        return WepSel_HIDE;
    }
#endif

    if (((p_person->U.UPerson.CurrentWeapon == wtype) || (flag == WepSel_HIDE)) && (flag != WepSel_SELECT))
    {
        if ((p_person->Flag & TngF_PlayerAgent) != 0 &&
          (p_person->Flag2 & TgF2_Unkn0800) == 0 &&
          p_person->U.UPerson.CurrentWeapon != WEP_NULL) {
            person_weapons_update_previous(p_person);
        }
        p_person->U.UPerson.CurrentWeapon = WEP_NULL;

        animode = ANIM_PERS_IDLE;
    }
    else
    {
        p_person->U.UPerson.CurrentWeapon = wtype;

        animode = gun_out_anim(p_person, 0);
    }
    switch_person_anim_mode(p_person, animode);
    p_person->Speed = calc_person_speed(p_person);

    return (p_person->U.UPerson.CurrentWeapon != WEP_NULL) ? WepSel_SELECT : WepSel_HIDE;
}

ubyte thing_select_best_weapon_for_range(struct Thing *p_person, int range)
{
    choose_best_weapon_for_range(p_person, range);
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);

    return (p_person->U.UPerson.CurrentWeapon != WEP_NULL) ? WepSel_SELECT : WepSel_HIDE;
}

ubyte thing_deselect_weapon(struct Thing *p_person)
{
    person_weapons_update_previous(p_person);
    p_person->U.UPerson.CurrentWeapon = WEP_NULL;
    set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
    p_person->Speed = calc_person_speed(p_person);

    return WepSel_HIDE;
}

void person_go_enter_vehicle_fast(struct Thing *p_person, struct Thing *p_vehicle, ushort plyr)
{
    asm volatile (
      "call ASM_person_go_enter_vehicle_fast\n"
        : : "a" (p_person), "d" (p_vehicle), "b" (plyr));
}

void person_go_enter_vehicle(struct Thing *p_person, struct Thing *p_vehicle)
{
    asm volatile (
      "call ASM_person_go_enter_vehicle\n"
        : : "a" (p_person), "d" (p_vehicle));
}

TbBool person_has_supershield_active(ThingIdx person)
{
    struct Thing *p_person;

    if (person <= 0)
        return false;

    p_person = &things[person];

    if (p_person->Type != TT_PERSON)
        return false;

    return ((p_person->Flag & TngF_PersSupShld) != 0);
}

TbBool person_can_toggle_supershield(ThingIdx person)
{
    if (person_is_executing_commands(person))
        return false;
    // Restrict only enabling supershield; for disabling, it can be done almost always
    if (!person_has_supershield_active(person))
    {
        short plyr; // stores PlayerIdx or -1

        if (thing_is_destroyed(person))
            return false;
        plyr = person_get_dcontrol_player(person);
        if (plyr >= 0)
        {
            PlayerInfo *p_player;

            p_player = &players[plyr];
            //TODO not sure why cannot enable supershield in DoubleMode; verify, maybe remove.
            if (p_player->DoubleMode != 0)
                return false;
        }
    }
    return true;
}

void person_supershield_toggle(struct Thing *p_person)
{
    if ((p_person->Flag & TngF_PersSupShld) != 0)
    {
        p_person->Flag &= ~(TngF_Unkn00200000|TngF_PersSupShld);
    }
    else
    {
        short plagent;
        p_person->Flag |= (TngF_Unkn00200000|TngF_PersSupShld);
        plagent = person_slot_in_player_dcontrol(p_person->ThingOffset, local_player_no);
        if (plagent >= 0)
            play_sample_using_heap(0, 96, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
    }
}

TbBool person_can_sustain_thermal(ThingIdx person)
{
    struct Thing *p_person;

    p_person = &things[person];

    if (p_person->U.UPerson.Energy <= 100)
        return false;

    return true;
}

TbBool person_update_thermal(ThingIdx person)
{
    struct Thing *p_person;

    p_person = &things[person];

    p_person->U.UPerson.Energy -= 3;

    if (p_person->U.UPerson.Energy <= 0) {
        return false;
    }

    return true;
}

void make_peeps_scatter(struct Thing *p_person, int x, int z)
{
    asm volatile (
      "call ASM_make_peeps_scatter\n"
        : : "a" (p_person), "d" (x), "b" (z));
}

int limit_mood(struct Thing *p_thing, short mood)
{
    int ret;
    asm volatile ("call ASM_limit_mood\n"
        : "=r" (ret) : "a" (p_thing), "d" (mood));
    return ret;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void yank_from_vehicle(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_yank_from_vehicle\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void person_self_destruct(struct Thing *p_person)
{
    asm volatile (
      "call ASM_person_self_destruct\n"
        : : "a" (p_person));
}

void person_scare_person(struct Thing *p_person)
{
    struct Thing *p_target;

    person_goto_person_nav(p_person);
    if (p_person->State == PerSt_NONE)
    {
        int range;

        p_person->State = PerSt_SCARE_PERSON;
        range = get_weapon_range(p_person);
        if (can_i_see_thing(p_person, p_person->PTarget, range, 0) > 0)
        {
            p_person->Flag |= TngF_TriggerUse;
            if ((p_person->Flag & TngF_InVehicle) != 0)
            {
                struct Thing *p_vehicle;

                p_vehicle = &things[p_person->U.UPerson.Vehicle];
                p_vehicle->Flag |= TngF_Unkn01000000;
            }
        }
    }
    p_target = p_person->PTarget;
    if (p_target->State == PerSt_DEAD)
    {
        p_person->State = PerSt_NONE;
    }
}

void person_block_person(struct Thing *p_person)
{
    struct Thing *p_target;

    person_goto_person_nav(p_person);
    if (p_person->State == PerSt_NONE)
    {
        int range;

        p_person->State = PerSt_BLOCK_PERSON;
        range = get_weapon_range(p_person);
        if (can_i_see_thing(p_person, p_person->PTarget, range * range, 0) > 0)
        {
            p_person->Flag |= TngF_TriggerUse;
            if ((p_person->Flag & TngF_InVehicle) != 0)
            {
                struct Thing *p_vehicle;

                p_vehicle = &things[p_person->U.UPerson.Vehicle];
                p_vehicle->Flag |= TngF_Unkn01000000;
            }
        }
    }
    p_target = p_person->PTarget;
    if (p_target->State == PerSt_DEAD)
    {
        p_person->State = PerSt_NONE;
    }
}

void process_stationary_shot(struct Thing *p_person)
{
    if (p_person->U.UPerson.WeaponTurn == 0)
    {
        if ((p_person->Flag & TngF_TriggerUse) == 0)
            p_person->Flag &= ~TngF_StationrSht;
    }
}

void process_wait_train(struct Thing *p_person)
{
    if ((p_person->Flag & TngF_Unkn4000) != 0)
    {
        p_person->SubState = 25;
    }
    else if (p_person->SubState == 25)
    {
        p_person->State = PerSt_NONE;
    }
}

void person_follow_person(struct Thing *p_person)
{
    struct Thing *p_target;

    person_goto_person_nav(p_person);
    if (p_person->State == PerSt_NONE)
    {
        p_person->State = PerSt_FOLLOW_PERSON;
    }
    p_target = &things[p_person->GotoThingIndex];
    if ((p_target->Flag & TngF_Destroyed) != 0)
    {
        p_person->State = PerSt_NONE;
    }
}

void person_go_plant_mine(struct Thing *p_person)
{
    person_goto_point(p_person);
    if (p_person->State == PerSt_NONE)
    {
        person_init_plant_mine_where_standing(p_person, p_person->U.UPerson.CurrentWeapon);
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            set_player_weapon_turn(p_person, p_person->U.UPerson.WeaponTurn);
    }
}

void process_knocked_out(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_process_knocked_out\n"
        : : "a" (p_person));
#endif
    ushort nxframe;

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        nxframe = frame[p_person->Frame].Next;
        if ((frame[nxframe].Flags & 0x01) != 0) {
            p_person->Timer1 = 0;
        } else {
            p_person->Timer1 = p_person->StartTimer1;
            p_person->Frame = nxframe;
        }
    }

    p_person->U.UPerson.BumpCount--;
    if (p_person->U.UPerson.BumpCount == 0)
    {
        set_person_anim_mode(p_person, p_person->U.UPerson.OldAnimMode);

        p_person->Flag2 &= ~TgF2_KnockedOut;
        p_person->Timer1 = 48;
        p_person->Flag &= ~TngF_StandOnVehicle;
    }
}

void process_tasered_person(struct Thing *p_person)
{
    asm volatile ("call ASM_process_tasered_person\n"
        : : "a" (p_person));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int find_target_from_persuaded_peeps_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_target_from_persuaded_peeps\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    // Only al is set by the asm; upper bits are leftovers
    return (ubyte)ra;
}

int find_target_from_persuaded_peeps(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_other;
    struct WarFlag *p_wflag;
    u32 kill_on_sight, kill_weapon_out, kill_armed;
    u32 mask;
    s32 range_sq;
    ubyte see_flags;
    ushort i;

    DIFFTEST_SHADOW_RET(find_target_from_persuaded_peeps, int,
      find_target_from_persuaded_peeps_asm(a1, a2, a3, a4));

    see_flags = (ubyte)a3;
    // TODO the asm does not mask the group with 0x1f here, unlike find_target_from_group()
    p_wflag = &war_flags[p_person->U.UPerson.EffectiveGroup];
    kill_on_sight = p_wflag->KillOnSight;
    kill_weapon_out = p_wflag->KillIfWeaponOut;
    kill_armed = p_wflag->KillIfArmed;

    if (word_1531DA <= 1)
        DIFFTEST_RETURN((int)0);
    range_sq = a2 * a2;

    for (i = 1; i < word_1531DA; i++)
    {
        p_other = &things[word_17FA58[i]];
        if ((p_other->Flag & TngF_Destroyed) != 0)
            continue;
        mask = 1u << (p_other->U.UPerson.EffectiveGroup & 0x1f);
        if ((kill_on_sight & mask) == 0)
        {
            if ((p_other->U.UPerson.CurrentWeapon == 0) || ((kill_weapon_out & mask) == 0))
            {
                if ((p_other->U.UPerson.WeaponsCarried & 0x283ceff) == 0)
                    continue;
                if ((kill_armed & mask) == 0)
                    continue;
            }
        }
        if ((p_other->Flag2 & TgF2_AlteredSubType) != 0)
            continue;
        if ((ushort)p_other->State == 0x24 || (ushort)p_other->State == 0x0c)
            continue;
        if ((p_other->Flag & TngF_Destroyed) != 0)
            continue;
        if (can_i_see_thing(p_person, p_other, range_sq, see_flags) != 0)
        {
            set_interrupt_target(p_person, p_other);
            DIFFTEST_RETURN((int)1);
        }
    }
    DIFFTEST_RETURN((int)0);
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC int find_target_from_players_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_target_from_players\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    // Only al is set by the asm; upper bits are leftovers
    return (ubyte)ra;
}

int find_target_from_players(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_agent;
    u32 visible[8];
    u32 count;
    s32 range_sq;
    ubyte see_flags;
    ubyte plyr;
    ubyte i;
    ubyte pick;

    DIFFTEST_SHADOW_RET(find_target_from_players, int,
      find_target_from_players_asm(a1, a2, a3, a4));

    see_flags = (ubyte)a3;
    // TODO the asm always uses players[0], not the local player
    plyr = 0;
    count = 0;
    p_person->PTarget = NULL;
    p_agent = NULL;
    if (playable_agents > 0)
    {
        range_sq = a2 * a2;
        for (i = 0; i < playable_agents; i++)
        {
            p_agent = players[0].MyAgent[i];
            if (((p_agent->Flag2 & TgF2_AlteredSubType) == 0) &&
              ((ushort)p_agent->State != 0x24) &&
              ((ushort)p_agent->State != 0x0c) &&
              ((p_agent->Flag & TngF_Destroyed) == 0))
            {
                if (can_i_see_thing(p_person, p_agent, range_sq, see_flags) != 0)
                    visible[i] = ++count;
                else
                    visible[i] = 0;
            }
            else
            {
                visible[i] = 0;
            }
        }
    }
    if (count == 0)
        DIFFTEST_RETURN((int)0);

    pick = (gameturn + (u32)(ushort)p_person->ThingOffset) % count + 1;
    for (i = 0; i < playable_agents; i++)
    {
        if (visible[i] == pick)
        {
            p_agent = players[plyr].MyAgent[i];
            break;
        }
    }
    set_interrupt_target(p_person, p_agent);
    ingame.fld_unkC8B = 1;
    DIFFTEST_RETURN((int)1);
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
static DIFFTEST_REF_FUNC void find_target_from_group_asm(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_target_from_group\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}

void find_target_from_group(int a1, int a2, int a3, int a4)
{
    struct Thing *p_person = (struct Thing *)a1;
    struct Thing *p_other;
    struct WarFlag *p_wflag;
    u32 not_truce, kill_on_sight, kill_weapon_out, kill_armed;
    u32 mask;
    s32 range, brain;
    ubyte see_flags, other_grp;
    TbBool any_group;
    ushort thing;

    DIFFTEST_SHADOW(find_target_from_group, find_target_from_group_asm(a1, a2, a3, a4));

    any_group = false;
    see_flags = 1;
    p_wflag = &war_flags[p_person->U.UPerson.EffectiveGroup & 0x1f];
    not_truce = ~p_wflag->Truce;
    p_wflag->KillOnSight &= not_truce;
    p_wflag = &war_flags[p_person->U.UPerson.EffectiveGroup & 0x1f];
    p_wflag->KillIfWeaponOut &= not_truce;
    p_wflag = &war_flags[p_person->U.UPerson.EffectiveGroup & 0x1f];
    p_wflag->KillIfArmed &= not_truce;
    p_wflag = &war_flags[p_person->U.UPerson.EffectiveGroup & 0x1f];
    kill_on_sight = p_wflag->KillOnSight;
    kill_weapon_out = p_wflag->KillIfWeaponOut;
    kill_armed = p_wflag->KillIfArmed;

    if ((p_person->U.UPerson.EffectiveGroup & 0x80) != 0)
        kill_on_sight = 0xFFFFFFFF;
    else if (p_person->PTarget != NULL)
        see_flags = 0;

    if (p_person->U.UPerson.CurrentWeapon == 0)
    {
        choose_best_weapon_for_range(p_person, 0x500);
        range = weapon_defs[p_person->U.UPerson.CurrentWeapon].RangeBlocks;
        p_person->U.UPerson.CurrentWeapon = 0;
    }
    else
    {
        range = weapon_defs[p_person->U.UPerson.CurrentWeapon].RangeBlocks;
    }
    range <<= 8;
    if (range == 0)
        return;

    brain = (p_person->U.UPerson.UMod.Mods >> 9) & 7;
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
    {
        brain = 0;
    }
    else if ((kill_on_sight | kill_weapon_out | kill_armed) == 0)
    {
        return;
    }

    if (p_person->U.UPerson.Target2 == 0)
        p_person->PTarget = NULL;
    // TODO the asm reduces brain levels 4 and above to 1; likely an original bug
    if (brain >= 4)
        brain = 1;
    range += brain << 8;

    if (!in_network_game && (p_person->Flag2 & TgF2_Unkn0008) == 0 &&
      (kill_on_sight & (1u << (level_def.PlayableGroups[0] & 0x1f))) != 0)
    {
        if ((ubyte)find_target_from_players((int)p_person, range, see_flags, see_flags) != 0)
            return;
        if ((ubyte)find_target_from_persuaded_peeps((int)p_person, range, see_flags, see_flags) != 0)
            return;
        if (((~(1u << (level_def.PlayableGroups[0] & 0x1f)) & kill_on_sight) |
          kill_weapon_out | kill_armed) == 0)
            return;
    }

    if (((p_person->Flag & TngF_PlayerAgent) != 0 && p_person->U.UPerson.Mood < -50) ||
      (p_person->Flag2 & TgF2_Unkn0008) != 0)
    {
        any_group = true;
        see_flags = 1;
    }

    thing = same_type_head[1];
    if (thing == 0)
        return;
    range = (s32)((u32)range * (u32)range);
    do
    {
        p_other = &things[thing];
        other_grp = p_other->U.UPerson.EffectiveGroup;
        if (!any_group || other_grp == p_person->U.UPerson.EffectiveGroup)
        {
            mask = 1u << (other_grp & 0x1f);
            if ((kill_on_sight & mask) == 0)
            {
                if ((p_other->U.UPerson.CurrentWeapon == 0) || ((kill_weapon_out & mask) == 0))
                {
                    if ((p_other->U.UPerson.WeaponsCarried & 0x283ceff) == 0)
                        goto next;
                    if ((kill_armed & mask) == 0)
                        goto next;
                }
            }
        }
        if ((p_other->Flag2 & TgF2_AlteredSubType) != 0)
            goto next;
        if ((ushort)p_other->State == 0x24 || (ushort)p_other->State == 0x0c)
            goto next;
        if ((p_other->Flag & TngF_Destroyed) != 0)
            goto next;
        if (can_i_see_thing(p_person, p_other, range, see_flags) != 0)
        {
            set_interrupt_target(p_person, p_other);
            return;
        }
next:
        thing = p_other->LinkSame;
    } while (thing != 0);
}


void person_intel(struct Thing *p_person)
{
    asm volatile ("call ASM_person_intel\n"
        : : "a" (p_person));
}

short person_move(struct Thing *p_person);

static DIFFTEST_REF_FUNC void process_im_shoved_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_process_im_shoved\n"
        : : "a" (p_person));
}

void process_im_shoved(struct Thing *p_person)
{
    short prev_vx, prev_vz;
    s32 dir;

    DIFFTEST_SHADOW(process_im_shoved, process_im_shoved_asm(p_person));
    prev_vx = p_person->VX;
    prev_vz = p_person->VZ;
    dir = (p_person->U.UPerson.BumpMode + 7) & 7;
    p_person->VX = angle_direction[dir].DiX >> 4;
    p_person->VZ = angle_direction[dir].DiY >> 4;
    if (person_move(p_person))
        p_person->U.UPerson.BumpMode--;
    else
        p_person->U.UPerson.BumpMode = 0;
    p_person->VX = prev_vx;
    p_person->VZ = prev_vz;
}

void try_and_kill_target(struct Thing *p_person)
{
    asm volatile ("call ASM_try_and_kill_target\n"
        : : "a" (p_person));
}

ubyte protect_person_simultaneous_fire(struct Thing *p_person, struct Thing *p_protng)
{
    return weapon_simultaneous_fire_in_group(p_protng->U.UPerson.CurrentWeapon,
      p_person->U.UPerson.CurrentWeapon);
}

void process_protect_person(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_process_protect_person\n"
        : : "a" (p_person));
    return;
#endif
    struct Thing *p_leadtng;

    p_leadtng = &things[p_person->GotoThingIndex];
    if ((p_leadtng->Flag & TngF_PersSupShld) != 0)
        p_person->Flag |= TngF_Unkn00200000 | TngF_PersSupShld;
    else
        p_person->Flag &= ~TngF_PersSupShld;
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
    {
        if (p_person->GotoThingIndex != p_person->Owner)
            p_person->GotoThingIndex = p_person->Owner;
        if (((p_leadtng->Flag & TngF_PlayerAgent) == 0) &&
          ((p_person->Flag2 & TgF2_Unkn0800) == 0) && in_network_game)
            p_person->State = PerSt_NONE;
    }
    if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn04) != 0)
    {
        if ((p_leadtng->U.UPerson.Flag3 & PrsF3_Unkn04) == 0)
        {
            p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn04;
            p_person->Flag2 &= ~TgF2_IgnoreEnemies;
        }
        if ((p_leadtng->State == PerSt_WAIT) &&
          ((p_person->Flag2 & TgF2_Unkn00080000) != 0))
        {
            set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
            p_person->Timer1 = 48;
            p_person->StartTimer1 = 48;
            p_person->Flag2 &= ~TgF2_Unkn00080000;
            p_person->Speed = calc_person_speed(p_person);
        }
        p_person->Flag2 &= TgF2_Unkn00080000;
        if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
            return;
    }

    if ((p_leadtng->Flag & TngF_Unkn4000) == 0)
    {
        p_person->Flag2 &= ~TgF2_Unkn4000;
    }
    else if (((p_person->Flag & TngF_Unkn4000) == 0) &&
      ((p_person->Flag2 & TgF2_Unkn4000) == 0))
    {
        remove_path(p_person);
        p_person->U.UPerson.ComTimer = -1;
        p_person->Flag2 |= TgF2_Unkn4000;
    }
    if ((p_person->Flag & TngF_Unkn4000) != 0)
    {
        p_person->Flag2 &= ~TgF2_Unkn4000;
    }

    if ((p_leadtng->Flag2 & (TgF2_Unkn8000|TgF2_Unkn00080000)) != 0)
    {
        if (((p_leadtng->Flag2 & TgF2_Unkn00080000) != 0)
          && ((p_person->Flag2 & TgF2_Unkn00080000) == 0)
          && (p_person->U.UPerson.Stamina > (p_person->U.UPerson.MaxStamina >> 2) + 64))
        {
            p_person->Flag2 |= TgF2_IgnoreEnemies;
            set_person_animmode_run(p_person);
        }
        if ((p_leadtng->Flag2 & TgF2_Unkn8000) != 0)
        {
            remove_path(p_person);
            p_person->U.UPerson.ComTimer = -1;
        }
    }

    if ( (p_person->Flag & TngF_InVehicle) != 0 )
    {
        if ((p_leadtng->Flag & TngF_InVehicle) != 0)
        {
            p_person->Flag2 &= ~TgF2_IgnoreEnemies;
        }
        else
        {
            person_attempt_to_leave_vehicle(p_person);
            if ((p_person->Flag & TngF_PlayerAgent) != 0)
                person_protect_update_follow_distance(p_person, p_leadtng->ThingOffset);
            else
                p_person->U.UPerson.ComRange = 8;
            p_person->State = PerSt_PROTECT_PERSON;
        }
    }
    else if ((p_leadtng->Flag & TngF_InVehicle) != 0)
    {
        p_person->U.UPerson.ComRange = 0;
        if (((p_person->Flag & TngF_Unkn01000000) != 0) && (p_person->U.UPerson.Vehicle == p_leadtng->U.UPerson.Vehicle))
        {
            person_enter_vehicle(p_person, &things[p_person->U.UPerson.Vehicle]);
            if (p_person->U.UPerson.PathIndex != 0)
                remove_path(p_person);
            p_person->State = PerSt_PROTECT_PERSON;
            return;
        }
    }
    if ((p_leadtng->Flag & TngF_Destroyed) != 0)
    {
        p_person->State = PerSt_NONE;
        return;
    }

    if ((((p_leadtng->Flag & (TngF_TriggerUse|TngF_WepCharging)) == 0) && ((p_leadtng->Flag2 & TgF2_Unkn0400) == 0)) ||
      ((p_person->Flag2 & TgF2_IgnoreEnemies) != 0))
    {
        // no action
    }
    else if ((p_leadtng->PTarget == NULL) || (p_person->U.UPerson.CurrentWeapon == WEP_NULL))
    {
        if (((p_person->Flag & TngF_PlayerAgent) == 0) || ((p_leadtng->Flag & TngF_ShootAtPos) == 0))
        {
            // no action
        }
        else if (protect_person_simultaneous_fire(p_person, p_leadtng) == 0)
        {
            // no action
        }
        else if ((p_leadtng->PTarget == NULL) && (p_person->U.UPerson.Target2 == 0))
        {
            short face_cor_X, face_cor_Y, face_cor_Z; // store both full positions and delta pos on map
            int weapon_range;
            PlayerIdx prot_plyr;

            p_person->Flag |= TngF_ShootAtPos;
            prot_plyr = p_leadtng->U.UPerson.ComCur >> 2;
            face_cor_X = players[prot_plyr].SpecialItems[0];
            face_cor_Y = players[prot_plyr].SpecialItems[1];
            face_cor_Z = players[prot_plyr].SpecialItems[2];

            weapon_range = get_weapon_range(p_person);
            map_limit_distance_to_target_fast(
              PRCCOORD_TO_MAPCOORD(p_person->X),
              PRCCOORD_TO_MAPCOORD(p_person->Y),
              PRCCOORD_TO_MAPCOORD(p_person->Z),
              &face_cor_X, &face_cor_Y, &face_cor_Z, weapon_range);

            if ((p_person->Flag & TngF_PlayerAgent) != 0)
            {
                PlayerIdx plyr;
                short plagent;

                plyr = p_person->U.UPerson.ComCur >> 2;
                plagent = p_person->U.UPerson.ComCur & 3;
                player_agent_set_user_vect(plyr, plagent, face_cor_X, face_cor_Y, face_cor_Z);
            }
            p_person->Flag |= TngF_TriggerUse;
            if ((p_person->Flag & TngF_InVehicle) != 0) {
                things[p_person->U.UPerson.Vehicle].Flag |= TngF_Unkn01000000;
            }

            {
                short full_angle;

                full_angle = angle_between_points(PRCCOORD_TO_MAPCOORD(p_person->X),
                  PRCCOORD_TO_MAPCOORD(p_person->Z), face_cor_X, face_cor_Z);
                change_person_angle_full(p_person, full_angle);
            }
        }
    }
    else
    {
        if (protect_person_simultaneous_fire(p_person, p_leadtng) == 0)
        {
            // no action
        }
        else if (persons_have_truce(p_leadtng, p_leadtng->PTarget))
        {
            // no action
        }
        else
        {
            int weapon_range;
            int see_range;

            weapon_range = get_weapon_range(p_person);
            see_range = can_i_see_thing(p_person, p_leadtng->PTarget, weapon_range * weapon_range + 1280 * 1280, 1);
            if (see_range != 0)
            {
                if (see_range * see_range <= weapon_range * weapon_range)
                {
                    p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn08;
                }
                else if ((p_person->Flag & TngF_PlayerAgent) == 0)
                {
                    p_person->U.UPerson.Flag3 |= PrsF3_Unkn08;
                }
                p_person->PTarget = p_leadtng->PTarget;
            }
        }
    }

    if ((p_person->Flag & TngF_ShootAtPos) != 0 && (p_person->Flag & (TngF_TriggerUse|TngF_WepCharging)) == 0)
        p_person->Flag &= ~TngF_ShootAtPos;
    if ((p_person->Flag & TngF_ShootAtPos) != 0)
    {
        if ((p_leadtng->Flag & TngF_TriggerUse) == 0) {
            p_person->Flag &= ~TngF_TriggerUse;
        }
    }
    else if (p_person->PTarget != NULL)
    {
        struct Thing *p_target;
        p_target = p_person->PTarget;
        if ((p_target->Flag & TngF_Destroyed) != 0) {
            p_person->PTarget = NULL;
            p_person->Flag &= ~TngF_TriggerUse;
        }
    }
    else
    {
        p_person->Flag &= ~TngF_TriggerUse;
    }
    if ((p_person->PTarget != NULL) && (p_person->Flag2 & TgF2_IgnoreEnemies) != 0) {
        p_person->PTarget = NULL;
    }
    if (((p_person->Flag & TngF_PlayerAgent) == 0) && ((p_leadtng->Flag & TngF_InVehicle) == 0)) {
        p_person->U.UPerson.ComRange = 8;
    }
    if (((p_person->Flag & TngF_InVehicle) == 0)
      && ((p_person->U.UPerson.Flag3 & PrsF3_Unkn04) == 0)
      && person_goto_person_nav(p_person) < (p_person->U.UPerson.ComRange * p_person->U.UPerson.ComRange) << 12
      && ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
      && ((p_leadtng->Flag2 & TgF2_Unkn00080000) == 0))
    {
        set_person_animmode_walk(p_person);
    }
    if ((p_person->PTarget == NULL)
      && (p_person->U.UPerson.Target2 == 0)
      && ((p_person->Flag & TngF_ShootAtPos) == 0))
    {
        if ((p_person->Flag & TngF_TriggerUse) != 0)
            p_person->Flag &= ~TngF_TriggerUse;
    }
    if (p_person->PTarget != NULL)
    {
        p_person->Flag &= ~TngF_ShootAtPos;
        try_and_kill_target(p_person);
    }
    if (p_person->State == 0) {
        p_person->State = PerSt_PROTECT_PERSON;
    }
}

void process_wander_and_fly(struct Thing *p_person, struct Thing *p_vehicle);

static DIFFTEST_REF_FUNC void process_wander_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_process_wander\n"
        : : "a" (p_person));
}

void process_wander(struct Thing *p_person)
{
    ubyte dirs[32];
    struct MyMapElement *p_mapel;
    int count;
    ubyte old_angle;
    ubyte angle;

    DIFFTEST_SHADOW(process_wander, process_wander_asm(p_person));

    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;

        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        if (p_vehicle->SubType == SubTT_VEH_FLYING)
            process_wander_and_fly(p_person, p_vehicle);
        if (conditional_command_state_true(p_person->U.UPerson.ComCur, p_person, 0))
            p_person->State = 0;
        return;
    }

    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
        set_person_animmode_walk(p_person);

    if ((p_person->U.UPerson.AnimMode == ANIM_PERS_Unkn14) ||
      (p_person->U.UPerson.AnimMode == ANIM_PERS_WEPHEAVY_Unkn15))
    {
        p_person->U.UPerson.AnimMode = gun_out_anim(p_person, 0);
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
        p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
          + p_person->U.UPerson.Angle];
    }

    if (p_person->U.UPerson.RecoilTimer != 0)
        return;

    p_person->Flag |= TngF_Unkn0004;
    p_person->VX = angle_direction[p_person->U.UPerson.Angle].DiX;
    p_person->VZ = angle_direction[p_person->U.UPerson.Angle].DiY;

    if (person_move(p_person) != 0)
    {
        if (((p_person->Flag2 & TgF2_Unkn40000000) == 0) ||
          ((things[game_col_vects_list[word_1AA390].Object].Flag & TngF_TriggerUse) != 0))
        {
            angle = p_person->U.UPerson.Angle;
            p_person->U.UPerson.Angle = angle + 1;
            p_person->U.UPerson.Angle = (angle + 9) & 7;
            p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
              + p_person->U.UPerson.Angle];
            p_person->U.UPerson.Timer2 = (LbRandomAnyShort() & 3) + 6;
        }
        p_person->Flag2 &= ~TgF2_Unkn40000000;
    }

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    if (conditional_command_state_true(p_person->U.UPerson.ComCur, p_person, 1))
        p_person->State = 0;

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        p_person->Frame = frame[p_person->Frame].Next;
    }

    {
        short timer;

        timer = p_person->U.UPerson.Timer2;
        p_person->U.UPerson.Timer2 = timer - 1;
        if (timer >= 0)
            return;
    }

    old_angle = p_person->U.UPerson.Angle;
    p_person->U.UPerson.Timer2 = p_person->U.UPerson.StartTimer2 + (LbRandomAnyShort() & 7);

    p_mapel = &game_my_big_map[(p_person->Z >> 16) * MAP_TILE_WIDTH + (p_person->X >> 16)];
    count = 0;
    if ((p_mapel->Flags2 & 0x10) != 0)
    {
        int diff;

        diff = p_person->U.UPerson.Angle - (p_mapel->Flags2 >> 5);
        if (diff > 4)
            diff -= 8;
        if (diff < -4)
            diff += 8;
        if (diff < 0)
            diff = -diff;
        if (diff <= 2)
            dirs[count++] = p_mapel->Flags2 >> 5;
        if (diff <= 1)
            dirs[count++] = p_mapel->Flags2 >> 5;
    }

    if (count == 0)
    {
        int dt_x, dt_z;

        for (dt_x = -1; dt_x <= 1; dt_x++)
        {
            for (dt_z = -1; dt_z <= 1; dt_z++)
            {
                int tl_x, tl_z;
                int diff;

                tl_x = (p_person->X >> 16) + dt_x;
                if ((tl_x < 0) || (tl_x >= MAP_TILE_WIDTH))
                    continue;
                tl_z = (p_person->Z >> 16) + dt_z;
                if ((tl_z < 0) || (tl_z >= MAP_TILE_HEIGHT))
                    continue;
                p_mapel = &game_my_big_map[tl_z * MAP_TILE_WIDTH + tl_x];
                if ((p_mapel->Flags2 & 0x10) == 0)
                    continue;
                diff = p_person->U.UPerson.Angle - (p_mapel->Flags2 >> 5);
                if (diff > 4)
                    diff -= 8;
                if (diff < -4)
                    diff += 8;
                if (diff < 0)
                    diff = -diff;
                if (diff <= 2)
                    dirs[count++] = p_mapel->Flags2 >> 5;
                if (diff <= 1)
                    dirs[count++] = p_mapel->Flags2 >> 5;
            }
        }
    }

    if (count == 0)
    {
        u32 turn_offs;

        turn_offs = (ushort)p_person->ThingOffset;
        if (((gameturn + turn_offs) & 3) == 0)
        {
            u32 rv;

            rv = ((gameturn >> 1) + turn_offs) & 0xF;
            if (rv == 0)
            {
                angle = p_person->U.UPerson.Angle + 1;
                p_person->U.UPerson.Angle = angle;
                p_person->U.UPerson.Angle = angle & 7;
            }
            else if (rv == 1)
            {
                angle = p_person->U.UPerson.Angle - 1;
                p_person->U.UPerson.Angle = angle;
                p_person->U.UPerson.Angle = angle & 7;
            }
        }
    }
    else
    {
        s32 rnd;

        rnd = (ushort)LbRandomAnyShort();
        p_person->U.UPerson.Angle = dirs[rnd % count] & 7;
    }

    if (old_angle != p_person->U.UPerson.Angle)
    {
        p_person->Frame = nstart_ani[(ushort)p_person->StartFrame + p_person->U.UPerson.Angle + 1];
    }
}

void person_recoil(struct Thing *p_person)
{
    asm volatile ("call ASM_person_recoil\n"
        : : "a" (p_person));
}

static DIFFTEST_REF_FUNC void calc_lighting_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_calc_lighting\n"
        : : "a" (p_person));
}

void calc_lighting(struct Thing *p_person)
{
    struct MyMapElement *p_map;
    s32 tile;
    short sh00, sh01, sh10, sh11;
    s32 frx, frz;
    s32 light;

    DIFFTEST_SHADOW(calc_lighting, calc_lighting_asm(p_person));
    p_map = game_my_big_map;
    tile = (p_person->X >> 16) + ((p_person->Z >> 16) << 7);
    sh00 = p_map[tile].ShadeR;
    sh01 = p_map[tile + 1].ShadeR;
    sh10 = p_map[tile + 128].ShadeR;
    sh11 = p_map[tile + 129].ShadeR;
    frx = (ubyte)(p_person->X >> 8);
    frz = (ubyte)(p_person->Z >> 8);
    if (frx + frz < 256)
    {
        light = sh00 + (((sh01 - sh00) * frx) >> 8);
        light += ((sh10 - sh00) * frz) >> 8;
    }
    else
    {
        light = sh11 + (((sh10 - sh11) * (256 - frx)) >> 8);
        light += ((sh01 - sh11) * (256 - frz)) >> 8;
    }
    p_person->U.UPerson.Brightness = (ubyte)light;
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void copy_owner_with_weapon(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_copy_owner_with_weapon\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}


void process_persuaded(struct Thing *p_person)
{
    asm volatile ("call ASM_process_persuaded\n"
        : : "a" (p_person));
}

/** Looks around the person for threats to react to.
 */
static DIFFTEST_REF_FUNC void search_for_danger_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_search_for_danger\n"
        : : "a" (p_person) : "cc", "memory");
}

void search_for_danger(struct Thing *p_person)
{
    s32 best_dist;
    s32 flee_x, flee_z;
    short plyr, i;

    DIFFTEST_SHADOW(search_for_danger, search_for_danger_asm(p_person));
    best_dist = 0x7fffffff;
    flee_x = 0;
    flee_z = 0;
    if (!in_network_game)
    {
        ubyte group;

        group = p_person->U.UPerson.EffectiveGroup & 0x1F;
        // TODO the asm returns 1 in eax when it made the person flee, 0 otherwise;
        // the wrapper is declared void, so the value is dropped
        if ((war_flags[group].Truce & (1 << level_def.PlayableGroups[0])) != 0)
            return;
    }
    for (plyr = 0; plyr < PLAYERS_LIMIT; plyr++)
    {
        if (((1 << plyr) & ingame.InNetGame_UNSURE) == 0)
            continue;
        for (i = 0; i < playable_agents; i++)
        {
            struct Thing *p_agent;
            s32 dx, dz;
            s32 adx, adz;
            s32 dist;

            p_agent = players[plyr].MyAgent[i];
            if (p_agent == NULL)
                continue;
            if (p_agent->U.UPerson.CurrentWeapon == WEP_NULL)
                continue;
            if (p_agent->U.UPerson.CurrentWeapon == WEP_CLONESHLD)
                continue;
            dx = (p_person->X - p_agent->X) >> 8;
            dz = (p_person->Z - p_agent->Z) >> 8;
            adx = abs(dx);
            adz = abs(dz);
            if (adx > adz)
                dist = adx + (adz >> 1);
            else
                dist = adz + (adx >> 1);
            if (dist >= best_dist || dist >= 0x600)
                continue;
            if (!in_network_game &&
              !can_i_see_thing(p_person, p_agent, 0x190000, 1))
                continue;
            best_dist = dist;
            flee_x = dx;
            flee_z = dz;
        }
    }
    if (best_dist < 0x500)
        make_peep_flee(flee_x, flee_z, p_person);
}

static DIFFTEST_REF_FUNC void process_danger_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_process_danger\n"
        : : "a" (p_person));
}

void process_danger(struct Thing *p_person)
{
    ubyte anmode;

    DIFFTEST_SHADOW(process_danger, process_danger_asm(p_person));
    if ((p_person->Flag & (TngF_Unkn40000000 | TngF_WepRecoil |
      TngF_SelectedAgent | TngF_Destroyed | TngF_Unkn0001)) != 0)
        return;
    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
        return;
    anmode = p_person->U.UPerson.AnimMode;
    if ((anmode == 14) || (anmode == 15))
        return;
    if ((p_person->Flag2 & (TgF2_AlteredSubType | TgF2_ExistsOffMap)) != 0)
        return;
    if ((((ushort)p_person->ThingOffset + gameturn) & 3) != 0)
        return;
    if (cybmod_brain_level(&p_person->U.UPerson.UMod) >= 3)
        return;
    search_for_danger(p_person);
}

void peep_return_to_point(struct Thing *p_person)
{
    asm volatile ("call ASM_peep_return_to_point\n"
        : : "a" (p_person));
}

void person_kill_target2(struct Thing *p_person)
{
    struct Thing *p_target;
    int wep_range;

    if (((gameturn + p_person->ThingOffset) & 0x7F) == 0)
    {
        ushort rnd;
        rnd = LbRandomAnyShort();
        // Only police taunt their target with a speech sample while attacking.
        if (p_person->SubType == SubTT_PERS_POLICE) {
            play_dist_speech(p_person, 54 + (rnd % 3), 0x7Fu, 0x40u, 100, 0, 3);
        }
    }
    if ((p_person->Flag & TngF_ShootAtPos) != 0)
    {
        p_person->Flag &= ~TngF_ShootAtPos;
    }

    p_target = &things[p_person->U.UPerson.Target2];
    if (p_person->State == PerSt_PROTECT_PERSON)
    {
        struct Thing *p_prottng;

        // A bodyguard currently protecting someone usually keeps attacking
        // its own target2, but may instead take over the protected person's
        // target, effectively delegating target selection to the protectee.
        p_prottng = &things[p_person->GotoThingIndex];
        if ((p_prottng->Flag2 & TgF2_IgnoreEnemies) != 0)
        {
            p_person->U.UPerson.Target2 = 0;
            p_person->PTarget = NULL;
            p_prottng->Flag2 |= TgF2_IgnoreEnemies;
            return;
        }
        if ((p_prottng->Flag & TngF_PersSupShld) != 0)
        {
            p_person->U.UPerson.Target2 = 0;
            p_person->PTarget = NULL;
            p_prottng->Flag |= TngF_Unkn00200000|TngF_PersSupShld;
            return;
        }
        if ((abs(p_person->U.UPerson.Mood) < 32) &&
          (p_prottng->PTarget != NULL) &&
          (p_prottng->U.UPerson.WeaponTurn != 0))
        {
            p_person->U.UPerson.Target2 = p_prottng->PTarget->ThingOffset;
            p_person->PTarget = p_prottng->PTarget;
        }
    }
    if ((p_person->U.UPerson.EffectiveGroup == p_target->U.UPerson.EffectiveGroup) ||
      (p_person->Flag2 & TgF2_IgnoreEnemies) != 0)
    {
        p_person->U.UPerson.Target2 = 0;
        return;
    }

    ushort bkp_State;
    short bkp_GotoThingIndex;
    TbBool person_in_ground_veh;
    TbBool target_within_range;

    person_in_ground_veh = false;
    bkp_State = p_person->State;
    bkp_GotoThingIndex = p_person->GotoThingIndex;

    p_person->GotoThingIndex = p_person->U.UPerson.Target2;
    wep_range = get_weapon_range(p_person);
    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        struct Thing *p_vehicle;
        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        if (p_vehicle->SubType == SubTT_VEH_GROUND)
        {
            person_in_ground_veh = true;
            // only the driver gets to steer the vehicle towards the target
            if ((p_vehicle->U.UVehicle.PassengerHead == p_person->ThingOffset) &&
              ((gameturn + p_person->ThingOffset) & 0x7F) == 0) {
                start_goto_vehicle(p_vehicle, p_target->X >> 8, p_target->Z >> 8);
            }
        }
    }
    if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn01) != 0)
    {
        ushort grp;
        ushort brain_lv;

        grp = p_target->U.UPerson.EffectiveGroup & 0x1F;
        if (((gameturn & 0xFF) != 0) && (group_actions[grp].Storming <= 5))
        {
            p_person->Flag &= ~TngF_Unkn0004;
        }
        else
        {
            p_person->U.UPerson.Flag3 &= ~(PrsF3_Unkn02|PrsF3_Unkn01);
            p_person->U.UPerson.Flag3 |= PrsF3_Unkn02;
            brain_lv = person_mod_brain_level(p_person);
            if ((LbRandomAnyShort() & 3) < brain_lv + 1)
                set_person_animmode_run(p_person);
            if ((p_target->Flag & TngF_PlayerAgent) != 0)
                ingame.fld_unkC8B = 1;
        }
    }
    if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn02) != 0)
    {
        p_person->Flag |= TngF_Unkn0004;
    }

    int target_dist_sq;

    if ((((p_person->Flag & TngF_PlayerAgent) != 0) && ((p_person->U.UPerson.Flag3 & PrsF3_Unkn08) == 0))
      || person_in_ground_veh || ((p_person->U.UPerson.Flag3 & PrsF3_Unkn01) != 0))
    {
        int dx, dy;

        dx = (p_person->X - p_target->X) >> 8;
        dy = (p_person->Z - p_target->Z) >> 8;
        target_dist_sq = dy * dy + dx * dx;
        if ((wep_range * wep_range < target_dist_sq) &&
          ((p_person->Flag & TngF_Unkn00800000) == 0))
        {
            p_person->U.UPerson.Target2 = 0;
            p_person->State = bkp_State;
            p_person->GotoThingIndex = bkp_GotoThingIndex;
            if ((p_person->Flag & TngF_PersKeepPost) != 0)
                peep_return_to_point(p_person);
            return;
        }
    }
    else
    {
        struct Thing *p_vehicle;

        p_vehicle = &things[p_person->U.UPerson.Vehicle];
        target_dist_sq = 0;
        if (((p_person->Flag & TngF_InVehicle) == 0) ||
          (p_vehicle->U.UVehicle.PassengerHead == p_person->ThingOffset))
        {
            //TODO separate drawing from state update
            if (p_person->U.UPerson.Target2 != p_person->GotoThingIndex) {
                draw_map_flat_circle(p_person->X >> 8,
                  p_person->Y >> 8, p_person->Z >> 8, 320, colour_lookup[0]);
            }
            target_dist_sq = person_goto_person_nav(p_person);
        }
        if ((p_person->U.UPerson.Within) && ((p_person->Flag2 & TgF2_Unkn0080) != 0))
        {
            p_person->Flag2 &= ~TgF2_Unkn0080;
            p_person->U.UPerson.Target2 = 0;
            p_person->State = bkp_State;
            p_person->GotoThingIndex = bkp_GotoThingIndex;
            if ((p_person->Flag & TngF_PersKeepPost) != 0)
                peep_return_to_point(p_person);
            return;
        }
    }
    if ((p_person->Flag & TngF_Destroyed) != 0) {
        return;
    }

    target_within_range = false;
    if ((target_dist_sq != 0) && (wep_range * wep_range > target_dist_sq))
        target_within_range = true;
    else
        p_person->Flag &= ~TngF_TriggerUse;

    if (target_within_range && (p_person->U.UPerson.WeaponTurn <= 1) &&
      (p_target->State != PerSt_DIEING) && ((p_target->Flag & TngF_Destroyed) == 0))
    {
        wep_range = get_weapon_range(p_person);
        target_dist_sq = can_i_see_thing(p_person, p_target, wep_range * wep_range, 1u);
        if (target_dist_sq > 0)
        {
            p_person->Flag &= ~(TngF_Unkn00200000|TngF_TriggerUse|TngF_PersSupShld);
            p_person->Flag |= TngF_TriggerUse;
            if ((p_person->Flag & TngF_InVehicle) != 0) {
                struct Thing *p_vehicle;
                p_vehicle = &things[p_person->U.UPerson.Vehicle];
                p_vehicle->Flag |= TngF_Unkn01000000;
            }
            if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn10) != 0) {
                ushort grp;
                grp = p_target->U.UPerson.EffectiveGroup & 0x1F;
                group_actions[grp].Storming |= GrpAF_UnderAttack;
                p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn10;
            }
            if ((p_person->Flag & TngF_PlayerAgent) == 0) {
                short range;
                range = p_person->U.UPerson.ComRange + 2;
                if (range > 20)
                    range = 20;
                p_person->U.UPerson.ComRange = range;
            }
        }
        else
        {
            target_within_range = false;
            p_person->Flag &= ~TngF_TriggerUse;
        }

        if ((p_person->Flag & TngF_PlayerAgent) != 0)
        {
          if (!target_within_range && ((gameturn & 0xF) == 0))
            p_person->U.UPerson.Target2 = 0;
        }
        else if (((p_person->U.UPerson.Flag3 & PrsF3_Unkn40) != 0) ||
          (!target_within_range && (p_person->U.UPerson.Flag3 & PrsF3_Unkn01) == 0))
        {
            if ((p_person->U.UPerson.Flag3 & PrsF3_Unkn02) != 0)
            {
                short range;
                range = p_person->U.UPerson.ComRange - 2;
                if (range < 2)
                    range = 2;
                p_person->U.UPerson.ComRange = range;
            }
            else
            {
                ushort path;
                short pc;

                path = p_person->U.UPerson.PathIndex;
                pc = my_paths[path].Flag - p_person->PathOffset;
                if ((pc == 2) || (pc == 3 && (gameturn & 7) == 0))
                {
                    ushort grp;
                    p_person->U.UPerson.Flag3 |= PrsF3_Unkn01;
                    grp = p_target->U.UPerson.EffectiveGroup & 0x1F;
                    group_actions[grp].Storming++;
                    p_person->Flag |= TngF_Unkn0004;
                    set_person_animmode_walk(p_person);
                }
                else
                {
                    short range;
                    range = p_person->U.UPerson.ComRange - 2;
                    if (range < 2)
                        range = 2;
                    p_person->U.UPerson.ComRange = range;
                }
            }
        }
    }

    if (p_person->State == PerSt_NONE) {
        p_person->Flag &= ~TngF_TriggerUse;
    }

    if (((p_target->Flag & TngF_Destroyed) == 0) &&
      ((p_person->Flag2 & (TgF2_Unkn00020000|TgF2_Unkn0040)) == 0) &&
      ((3 * wep_range * wep_range >= target_dist_sq) || (bkp_State == PerSt_KILL_PERSON))) {
        if ((p_person->Flag & TngF_Destroyed) == 0)
            p_person->State = bkp_State;
        p_person->GotoThingIndex = bkp_GotoThingIndex;
        return;
    }
    if ((p_person->U.UPerson.Flag3 & (PrsF3_Unkn02|PrsF3_Unkn01)) != 0)
    {
        ushort grp;
        p_person->U.UPerson.Flag3 &= ~(PrsF3_Unkn02|PrsF3_Unkn01);
        grp = p_target->U.UPerson.EffectiveGroup & 0x1F;
        group_actions[grp].Storming--;
    }

    p_target = NULL;
    p_person->Flag2 &= ~TgF2_Unkn0040;
    p_person->U.UPerson.Target2 = 0;

    // We are giving up on Target2: if the person was mid-kill-mission on
    // its PTarget, keep chasing it unless that target has since been destroyed.
    // Otherwise, fall back to whatever target was being pursued before this
    // kill-mission started (OldTarget).
    if (bkp_State == PerSt_KILL_PERSON)
    {
        p_target = p_person->PTarget;
        if (p_target != NULL)
        {
            if ((p_target->Flag & TngF_Destroyed) != 0) {
                bkp_State = PerSt_NONE;
                p_person->State = PerSt_NONE;
            }
        }
    }
    if (bkp_State != PerSt_KILL_PERSON)
    {
        struct Thing *p_ntarget;

        p_ntarget = NULL;
        if (p_person->OldTarget != 0)
            p_ntarget = &things[p_person->OldTarget];
        p_person->PTarget = p_ntarget;
    }

    p_person->Flag &= ~TngF_TriggerUse;
    remove_path(p_person);

    if (((p_person->Flag & TngF_PlayerAgent) == 0) &&
      (p_person->U.UPerson.ComCur != 0))
    {
        if ((p_person->Flag & TngF_PersKeepPost) != 0) {
            peep_return_to_point(p_person);
            return;
        }
        person_init_command(p_person, PerSt_DROP_ITEM);
    }
    else
    {
        if ((p_person->Flag & TngF_Destroyed) == 0)
            p_person->State = bkp_State;
        p_person->GotoThingIndex = bkp_GotoThingIndex;
    }
}

void person_wait(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_wait\n"
        : : "a" (p_person));
#endif
    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
    {
        set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
        p_person->Timer1 = 48;
        p_person->StartTimer1 = 48;
        p_person->Flag2 &= ~TgF2_Unkn00080000;
        p_person->Speed = calc_person_speed(p_person);
    }
    if ((p_person->U.UPerson.AnimMode != ANIM_PERS_Unkn21) && (p_person->U.UPerson.CurrentWeapon == WEP_NULL))
    {
        set_person_anim_mode(p_person, ANIM_PERS_Unkn21);
    }
    p_person->Flag &= ~TngF_Unkn0001;
    if (((p_person->Flag & TngF_WepCharging) != 0) || (p_person->U.UPerson.WeaponTurn != 0))
    {
        if (p_person->U.UPerson.AnimMode == ANIM_PERS_Unkn14 || p_person->U.UPerson.AnimMode == ANIM_PERS_WEPHEAVY_Unkn15)
        {
            p_person->Timer1 -= fifties_per_gameturn;
            if (p_person->Timer1 < 0) {
                p_person->Timer1 = p_person->StartTimer1;
                p_person->Frame = frame[p_person->Frame].Next;
            }
        }
    }
    else if (((gameturn + 32 * p_person->ThingOffset) & 0x7F) == 0)
    {
        process_random_speech(p_person, 0);
    }

    if (((p_person->Flag & TngF_PlayerAgent) == 0 || (p_person->Flag2 & TgF2_Unkn0800) != 0)
      && (p_person->U.UPerson.ComHead != 0)
      && conditional_command_state_true(p_person->U.UPerson.ComCur, p_person, 2))
    {
        if (gameturn <= 1) {
            LOGWARN("Person %s %d command %d condition met just at start of the game",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              (int)p_person->U.UPerson.ComCur);
        }
        if ((debug_log_things & 0x01) != 0) {
            char locstr[192];
            snprint_command(locstr, sizeof(locstr), p_person->U.UPerson.ComCur);
            LOGSYNC_F("Person %s %d %s %d condition met, state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              locstr, (int)p_person->U.UPerson.ComCur, p_person->State, p_person->SubState);
        }
        p_person->State = PerSt_NONE;
    }
    if (p_person->State < 0)
        p_person->Frame = frame[p_person->Frame].Next;
}

void person_pickup(struct Thing *p_person)
{
    asm volatile ("call ASM_person_pickup\n"
        : : "a" (p_person));
}

void plant_mine(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_plant_mine\n"
        : : "a" (p_person));
#else
    struct SimpleThing *p_item;

    p_item = create_item(PRCCOORD_TO_MAPCOORD(p_person->X),
      PRCCOORD_TO_MAPCOORD(p_person->Y),
      PRCCOORD_TO_MAPCOORD(p_person->Z),
      1004, p_person->U.UPerson.TempWeapon);
    if (p_item == NULL) {
        return;
    }

    p_item->Timer1 = 140;
    p_item->StartTimer1 = 140;
    p_item->Owner2 = p_person->ThingOffset;
    p_item->U.UWeapon.Ammo = 7;
    p_item->U.UWeapon.OnFace = p_person->U.UPerson.OnFace;

    give_take_me_weapon(p_person, p_person->U.UPerson.TempWeapon, -1, p_item->ThingOffset);

    if (!person_carries_weapon(p_person, p_person->U.UPerson.CurrentWeapon))
    {
      if (((p_person->Flag & TngF_PlayerAgent) != 0) &&
        ((p_person->Flag2 & TgF2_Unkn0800) == 0)) {
          person_weapons_reset_previous(p_person);
      }
      p_person->U.UPerson.CurrentWeapon = WEP_NULL;
    }
    p_item->Radius = 50;
#endif
}

void person_drop_item(struct Thing *p_person)
{
    asm volatile ("call ASM_person_drop_item\n"
        : : "a" (p_person));
}

static DIFFTEST_REF_FUNC void person_dieing_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_dieing\n"
        : : "a" (p_person));
}

void person_dieing(struct Thing *p_person)
{
    ushort nxframe;

    DIFFTEST_SHADOW(person_dieing, person_dieing_asm(p_person));
    p_person->Radius = 40;
    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 >= 0)
        return;

    p_person->Timer1 = p_person->StartTimer1;
    nxframe = frame[(ushort)p_person->Frame].Next;
    if ((frame[nxframe].Flags & 0x01) == 0)
    {
        p_person->Frame = nxframe;
        return;
    }

    p_person->State = PerSt_DEAD;
    group_actions[p_person->U.UPerson.Group].Alive--;
    group_actions[p_person->U.UPerson.Group].Dead++;
    if ((p_person->Flag & TngF_Persuaded) != 0)
        stop_being_persuaded(p_person);
    p_person->U.UPerson.Timer2 = 50;
    p_person->Flag &= ~TngF_Persuaded;
    if (p_person->U.UPerson.AnimMode == ANIM_PERS_Unkn12)
        p_person->Flag |= TngF_Unkn02000000;
    if (p_person->U.UPerson.AnimMode == ANIM_PERS_LAY_DOWN)
    {
        p_person->U.UPerson.AnimMode = ANIM_PERS_DEAD_BODY;
        p_person->StartFrame = people_frames[p_person->SubType][p_person->U.UPerson.AnimMode] - 1;
        p_person->U.UPerson.FrameId.Version[3] = 1;
        p_person->U.UPerson.FrameId.Version[4] = 0;
        p_person->Frame = nstart_ani[people_frames[p_person->SubType][p_person->U.UPerson.AnimMode]
          + p_person->U.UPerson.Angle];
    }
}

static DIFFTEST_REF_FUNC void person_dead_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_dead\n"
        : : "a" (p_person));
}

void person_dead(struct Thing *p_person)
{
    s32 i;

    DIFFTEST_SHADOW(person_dead, person_dead_asm(p_person));
    if (p_person->SubState == 0x1A)
    {
        ushort nxframe;

        p_person->Timer1 -= fifties_per_gameturn;
        p_person->Radius = 40;
        if (p_person->Timer1 < 0)
        {
            p_person->Timer1 = p_person->StartTimer1;
            nxframe = frame[(ushort)p_person->Frame].Next;
            if ((frame[nxframe].Flags & 0x01) != 0) {
                p_person->SubState = 0;
                p_person->Flag |= TngF_Unkn02000000;
            } else {
                p_person->Frame = nxframe;
            }
        }
    }

    if (!in_network_game)
        return;
    if ((net_game_play_flags & NGPF_Unkn04) == 0)
        return;
    if ((p_person->Flag & TngF_SelectedAgent) == 0)
        return;

    for (i = 0; (ushort)i < 8; i++)
    {
        PlayerIdx plyr;

        if ((ingame.InNetGame_UNSURE & (1 << i)) == 0)
            continue;
        plyr = (ushort)i;
        p_person->Flag &= ~TngF_SelectedAgent;
        if (players[plyr].DirectControl[0] != (ushort)p_person->ThingOffset)
            continue;
        place_default_player(plyr, 0);
    }
}

static DIFFTEST_REF_FUNC void person_kill_person_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_kill_person\n"
        : : "a" (p_person));
}

void person_kill_person(struct Thing *p_person)
{
    struct Thing *p_target;
    u32 turn;

    DIFFTEST_SHADOW(person_kill_person, person_kill_person_asm(p_person));
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;
    if ((ushort)p_person->State == PerSt_DEAD)
        return;
    if ((ushort)p_person->State == PerSt_DIEING)
        return;

    turn = (ushort)p_person->ThingOffset + gameturn;
    if ((turn & 0x0F) == 0)
    {
        struct Command *p_cmd;

        p_cmd = &game_commands[(ushort)p_person->U.UPerson.ComCur];
        p_target = p_person->PTarget;
        if ((p_cmd->Type == PCmd_KILL_ALL_GROUP) && (p_target != NULL))
        {
            if ((p_target->U.UPerson.EffectiveGroup != p_cmd->OtherThing)
              || ((turn & 0x7F) == 0))
            {
                p_person->State = 0;
                p_person->U.UPerson.Target2 = 0;
                return;
            }
        }
    }

    p_target = p_person->PTarget;
    if (p_target == NULL)
    {
        p_person->State = 0;
        return;
    }
    if ((p_target->Flag & TngF_Destroyed) != 0)
    {
        p_person->State = 0;
        return;
    }

    p_person->U.UPerson.Target2 = p_target->ThingOffset;
    person_kill_target2(p_person);

    if (((p_person->Flag2 & TgF2_Unkn0040) != 0) &&
      ((p_person->Flag & TngF_Unkn00800000) != 0))
    {
        things[(ushort)p_person->Owner].Flag &= ~TngF_Unkn00400000;
        p_person->State = 0;
        p_person->Flag &= ~(TngF_Unkn00800000|TngF_Unkn0040);
        p_person->U.UPerson.Target2 = 0;
        p_person->Flag |= TngF_Unkn0040;
        return;
    }

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    if (p_person->U.UPerson.Target2 == 0)
    {
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
        {
            p_person->State = 0;
            p_person->Flag &= ~TngF_TriggerUse;
        }
        else if ((p_person->Flag & TngF_Unkn00800000) != 0)
        {
            things[(ushort)p_person->Owner].Flag &= ~TngF_Unkn00400000;
            p_person->Flag &= ~(TngF_Unkn00800000|TngF_Unkn0040);
            p_person->State = 0;
            p_person->Flag |= TngF_Unkn0040;
        }
    }

    p_target = p_person->PTarget;
    if ((p_target == NULL) || ((p_target->Flag & TngF_Destroyed) != 0))
    {
        p_person->U.UPerson.Target2 = 0;
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
        {
            p_person->State = 0;
        }
        else if ((p_person->Flag & TngF_Unkn00800000) != 0)
        {
            things[(ushort)p_person->Owner].Flag &= ~TngF_Unkn00400000;
            p_person->Flag &= ~(TngF_Unkn00800000|TngF_Unkn0040);
            p_person->State = 0;
            p_person->Flag |= TngF_Unkn0040;
        }
    }
    p_person->U.UPerson.Target2 = 0;
}

void person_save_victim(struct Thing *p_person)
{
    asm volatile ("call ASM_person_save_victim\n"
        : : "a" (p_person));
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int search_floor_for_item(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_search_floor_for_item\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}


static DIFFTEST_REF_FUNC void person_get_item_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_get_item\n"
        : : "a" (p_person));
}

void person_get_item(struct Thing *p_person)
{
    struct SimpleThing *p_sthing;
    s32 adx, adz, dist;

    DIFFTEST_SHADOW(person_get_item, person_get_item_asm(p_person));
    if (sthings[p_person->GotoThingIndex].Type != SmTT_DROPPED_ITEM)
    {
        p_person->State = 0;
        p_person->Flag2 &= ~TgF2_Unkn08000000;
        return;
    }
    person_goto_point(p_person);
    if ((p_person->Flag & TngF_Unkn08000000) != 0)
    {
        if (p_person->U.UPerson.StandOnThing != p_person->GotoThingIndex)
        {
            if ((short)search_floor_for_item((int)p_person,
              p_person->GotoThingIndex, (int)p_person, 0) != 1)
                return;
        }
    }
    else
    {
        if ((p_person->Flag & TngF_Persuaded) != 0)
            return;
        if ((ushort)p_person->State != 0)
            return;
        p_sthing = &sthings[p_person->GotoThingIndex];
        adx = abs(p_person->X - p_sthing->X);
        adz = abs(p_person->Z - p_sthing->Z);
        // Approximate distance
        if (adx < adz)
            dist = adz - (adz >> 5) - (adz >> 7) + (adx >> 2) + (adx >> 3) + (adx >> 6) + (adx >> 7);
        else
            dist = adx - (adx >> 5) - (adx >> 7) + (adz >> 2) + (adz >> 3) + (adz >> 6) + (adz >> 7);
        if (dist >= 0x10000)
            return;
    }
    person_init_pickup(p_person, p_person->GotoThingIndex);
}

static DIFFTEST_REF_FUNC void person_run_away_asm(struct Thing *p_person)
{
    asm volatile ("call ASM_person_run_away\n"
        : : "a" (p_person));
}

void person_run_away(struct Thing *p_person)
{
    s32 vel_x, vel_z;
    ubyte angle;

    DIFFTEST_SHADOW(person_run_away, person_run_away_asm(p_person));

    vel_x = p_person->VX;
    vel_z = p_person->VZ;
    p_person->VX = lbSinTable[8 * p_person->U.UPerson.RecoilDir] >> 8;
    p_person->VZ = lbSinTable[8 * p_person->U.UPerson.RecoilDir + LbFPMath_PI/2] >> 8;

    if (person_move(p_person) != 0)
    {
        angle = p_person->U.UPerson.Angle + 1;
        p_person->U.UPerson.Angle = angle;
        p_person->U.UPerson.Angle = angle & 7;
        p_person->U.UPerson.RecoilDir += 0x20;
        p_person->Frame = nstart_ani[p_person->StartFrame + 1 + p_person->U.UPerson.Angle];
    }

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    p_person->VX = vel_x;
    p_person->VZ = vel_z;

    if (((p_person->U.UPerson.RecoilTimer & 0x20) == 0) && ((gameturn & 0x7F) == 0))
    {
        ushort rndval;

        rndval = LbRandomAnyShort();
        if (p_person->SubType == SubTT_PERS_SCIENTIST)
        {
            play_dist_speech(p_person, 62 + (rndval % 2), FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 3);
        }
        else if (((1 << p_person->SubType) & female_peep) != 0)
        {
            play_dist_speech(p_person, 41 + (((gameturn >> 7) + p_person->ThingOffset) & 1),
              FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
        }
        else
        {
            play_dist_speech(p_person, 40 + (((gameturn >> 7) + p_person->ThingOffset) & 1),
              FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
        }
    }

    p_person->U.UPerson.RecoilTimer--;
    if (p_person->U.UPerson.RecoilTimer == 0)
    {
        p_person->Flag &= ~TngF_Unkn00040000;
        if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
            set_person_animmode_walk(p_person);
        p_person->U.UPerson.Mood = 0;
        search_for_danger(p_person);
        remove_path(p_person);
        p_person->U.UPerson.ComTimer = -1;
    }

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        p_person->Frame = frame[p_person->Frame].Next;
    }
}

void person_becomes_persuaded(struct Thing *p_person, ushort energy)
{
    asm volatile ("call ASM_person_becomes_persuaded\n"
        : : "a" (p_person), "d" (energy));
}

void person_persuade_person(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_persuade_person\n"
        : : "a" (p_person));
#endif
    struct Thing *p_target;
    int dist_sq;
    short wep_range;
    TbBool reached_target;

    if (person_carries_weapon(p_person, WEP_PERSUADER2))
        wep_range = get_hand_weapon_range(p_person, WEP_PERSUADER2);
    else
        wep_range = get_hand_weapon_range(p_person, WEP_PERSUADRTRN);

    if (((p_person->Flag & (TngF_TriggerUse|TngF_WepCharging)) != 0) &&
      (p_person->U.UPerson.WeaponTimer > 5)) {
        p_person->Flag &= ~TngF_TriggerUse;
    }

    p_target = &things[p_person->GotoThingIndex];
    p_person->PTarget = p_target;

    if (p_person->GotoThingIndex == 0) {
        p_person->State = PerSt_NONE;
        return;
    }
    if ((p_target->Flag & TngF_Persuaded) != 0) {
        p_person->State = PerSt_NONE;
        return;
    }

    dist_sq = person_goto_person_nav(p_person);

    reached_target = 0;
    if ((dist_sq != 0) && (dist_sq < wep_range * wep_range))
        reached_target = 1;

    if (reached_target)
    {
        ushort energy_reqd;
        ubyte target_select;
        ThingIdx target;

        if (person_carries_weapon(p_person, WEP_PERSUADER2))
            target_select = PTargSelect_PersuadeAdv;
        else
            target_select = PTargSelect_Persuader;

        target = process_persuadertron(p_person, target_select, &energy_reqd);
        if (target != 0)
        {
            p_person->PTarget = &things[p_person->GotoThingIndex];
            person_becomes_persuaded(p_person, 0);
            p_person->State = PerSt_NONE;
            return;
        }
        if (p_person->U.UPerson.ComRange > 2)
            p_person->U.UPerson.ComRange -= 2;
        else
            p_person->U.UPerson.ComRange = 2;
    }
    if (p_person->State == 0) {
        p_person->State = PerSt_PERSUADE_PERSON;
        p_person->Flag &= ~TngF_TriggerUse;
    }

    p_target = p_person->PTarget;
    if ((p_target != NULL) && ((p_target->Flag & TngF_Destroyed) != 0)) {
        p_person->State = PerSt_NONE;
        p_person->Flag &= ~TngF_TriggerUse;
    }
}

void process_support_person(struct Thing *p_person)
{
    asm volatile ("call ASM_process_support_person\n"
        : : "a" (p_person));
}

void person_use_vehicle(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_use_vehicle\n"
        : : "a" (p_person));
#endif
    struct Thing *p_vehicle;

    if ((p_person->Flag & TngF_InVehicle) != 0)
    {
        if ((debug_log_things & 0x01) != 0) {
            LOGSYNC_F("Person %s %d has finished state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              p_person->State, p_person->SubState);
        }
        p_person->State = PerSt_NONE;
        return;
    }

    person_goto_point(p_person);
    if ((p_person->Flag & TngF_StandOnVehicle) == 0) {
        if ((debug_log_things & 0x01) != 0) {
            LOGSYNC_F("Person %s %d has wrong flags to enter, state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              p_person->State, p_person->SubState);
        }
        return;
    }

    if (p_person->U.UPerson.Vehicle != p_person->GotoThingIndex) {
        if ((debug_log_things & 0x01) != 0) {
            LOGSYNC_F("Person %s %d went to wrong thing, state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              p_person->State, p_person->SubState);
        }
        return;
    }

    p_vehicle = &things[p_person->U.UPerson.Vehicle];

    if (!can_i_enter_vehicle(p_person, p_vehicle)) {
        if ((debug_log_things & 0x01) != 0) {
            LOGSYNC_F("Person %s %d state %d.%d cannot enter %s %d state %d.%d",
              person_type_name(p_person->SubType), (int)p_person->ThingOffset,
              p_person->State, p_person->SubState,
              vehicle_type_name(p_vehicle->SubType), (int)p_vehicle->ThingOffset,
              p_vehicle->State, p_vehicle->SubState);
        }
        return;
    }

    person_enter_vehicle(p_person, p_vehicle);
    p_person->State = PerSt_NONE;
}

void person_wait_vehicle(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_wait_vehicle\n"
        : : "a" (p_person));
#endif
    struct Thing *p_vehicle;
    TbBool next_cmd_is_goto;

    p_vehicle = &things[p_person->U.UPerson.Vehicle];

    next_cmd_is_goto = false;
    if ((p_person->Flag & TngF_PlayerAgent) == 0)
    {
        struct Command *p_cmd;
        short cmd;

        cmd = p_person->U.UPerson.ComCur;
        p_cmd = &game_commands[cmd];
        while (1)
        {
            cmd = p_cmd->Next;
            p_cmd = &game_commands[cmd];
            if ((p_cmd->Flags & PCmdF_IsUntil) == 0)
                break;
        }

        if (p_cmd->Type == PCmd_GO_TO_POINT)
        {
            next_cmd_is_goto = true;
        }
        else if (p_cmd->Type == PCmd_LOOP_COM)
        {
            struct Command *p_nxcmd;
            p_nxcmd = &game_commands[p_cmd->OtherThing];
            if (p_nxcmd->Type == PCmd_GO_TO_POINT) {
                next_cmd_is_goto = true;
            }
        }
    }
    if (next_cmd_is_goto)
    {
        short cmd;

        if ((p_vehicle->State == VehSt_UNKN_33)
          || (p_vehicle->State == VehSt_NONE)
          || (p_vehicle->State == VehSt_UNKN_3C)
          || (p_vehicle->State == VehSt_UNKN_41)
          || (p_vehicle->State == VehSt_FLY_LANDING))
        {
            cmd = p_person->U.UPerson.ComCur;
            cmd = game_commands[cmd].Next;
            p_person->U.UPerson.ComCur = cmd;
            person_init_command(p_person, PCmd_FOLLOW_PERSON);
        }
    }
    else
    {
        short cmd;

        if ((p_vehicle->State == VehSt_PARKED_PARAL)
          || (p_vehicle->State == VehSt_PARKED_PERPN)
          || (p_vehicle->State == VehSt_NONE)
          || (p_vehicle->State == VehSt_UNKN_41))
        {
            cmd = p_person->U.UPerson.ComCur;
            cmd = game_commands[cmd].Next;
            p_person->U.UPerson.ComCur = cmd;
            person_init_command(p_person, PCmd_SUPPORT_PERSON);
        }
    }
}

void person_destroy_building(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_destroy_building\n"
        : : "a" (p_person));
#endif
    struct Thing *p_target;
    TbBool in_range;

    if (((p_person->Flag & (TngF_TriggerUse|TngF_WepCharging)) != 0) &&
      (p_person->U.UPerson.WeaponTimer > 5))
        p_person->Flag &= ~TngF_TriggerUse;

    {
        int dist_x, dist_z, range;

        dist_x = PRCCOORD_TO_MAPCOORD(p_person->X) - p_person->U.UPerson.GotoX;
        dist_z = PRCCOORD_TO_MAPCOORD(p_person->Z) - p_person->U.UPerson.GotoZ;
        range = p_person->U.UPerson.ComRange << 6;

        in_range = 0;
        if (dist_x * dist_x + dist_z * dist_z < range * range)
            in_range = 1;
    }

    if (!in_range)
        person_goto_point(p_person);

    p_target = p_person->PTarget;
    if (p_target == NULL)
    {
        p_person->State = PerSt_NONE;
        p_person->Flag &= ~TngF_TriggerUse;
        return;
    }

    if (in_range && (p_person->U.UPerson.WeaponTurn == 0) && ((p_target->Flag & TngF_Destroyed) == 0))
    {
        struct Thing *p_building;
        int wep_range;
        wep_range = get_weapon_range(p_person);
        p_building = &things[p_person->GotoThingIndex];
        if (can_i_see_building(p_person, p_building, wep_range * wep_range, 1) <= 0)
        {
            in_range = 0;
            p_person->Flag &= ~TngF_TriggerUse;
        }
        else
        {
            p_person->Flag &= ~(TngF_Unkn00200000|TngF_TriggerUse|TngF_PersSupShld);
            p_person->Flag |= TngF_TriggerUse;
            if ((p_person->Flag & TngF_InVehicle) != 0) {
                struct Thing *p_vehicle;
                p_vehicle = &things[p_person->U.UPerson.Vehicle];
                p_vehicle->Flag |= TngF_Unkn01000000;
            }
            p_person->U.UPerson.ComRange += 2;
            if (p_person->U.UPerson.ComRange > 5 * 4)
                p_person->U.UPerson.ComRange = 5 * 4;
        }
        if (!in_range)
        {
            if (p_person->U.UPerson.ComRange < 4)
                p_person->U.UPerson.ComRange = 2;
            else
                p_person->U.UPerson.ComRange -= 2;
        }
    }

    if (p_person->State == PerSt_NONE)
    {
        p_person->State = PerSt_DESTROY_BUILDING;
        p_person->Flag &= ~TngF_TriggerUse;
    }
    if ((p_target->Flag & TngF_Destroyed) != 0)
    {
        p_person->State = PerSt_NONE;
        p_person->Flag &= ~TngF_TriggerUse;
    }
}

void person_catch_train(struct Thing *p_person)
{
    asm volatile ("call ASM_person_catch_train\n"
        : : "a" (p_person));
}

void set_angle_to_avoid_group(struct Thing *p_person)
{
#if 1
    asm volatile ("call ASM_set_angle_to_avoid_group\n"
        : : "a" (p_person));
#endif
}

static DIFFTEST_REF_FUNC ubyte check_person_within_asm(struct Command *p_cmd, int x, int z)
{
    ubyte ret;
    asm volatile (
      "call ASM_check_person_within\n"
        : "=a" (ret) : "a" (p_cmd), "d" (x), "b" (z));
    return ret;
}

ubyte check_person_within(struct Command *p_cmd, int x, int z)
{
    ubyte revert;
    TbBool inside;

    DIFFTEST_SHADOW_RET(check_person_within, ubyte,
      check_person_within_asm(p_cmd, x, z));
    revert = (p_cmd->Flags & PCmdF_RevertFunct) != 0;

    if ((p_cmd->Flags & PCmdF_AreaIsRect) != 0)
    {
        // Rectangle area from (X,Z) to (Arg1,Time), bounds exclusive
        inside = (x > p_cmd->X) && (x < p_cmd->Arg1)
          && (z > p_cmd->Z) && (z < p_cmd->Time);
    }
    else
    {
        // Circle area centered at (X,Z), radius Arg1 scaled by 64
        u32 dx, dz, r;
        dx = (u32)(x - p_cmd->X);
        dz = (u32)(z - p_cmd->Z);
        r = (u32)(s32)p_cmd->Arg1;
        inside = (s32)(dx * dx + dz * dz) <= (s32)((r * r) << 12);
    }

    if (inside)
        DIFFTEST_RETURN((ubyte)(revert == 0));
    DIFFTEST_RETURN((ubyte)(revert != 0));
}

/** Finds a collision blocking move of a thing by given shift.
 *
 * Finds a collision vector within the given map element, which
 * blocks the move of a person by the given speed vector (dx,dz).
 *
 * Also updates globals used for intelligent door handling.
 *
 * @return Returns index in `game_col_vects` of the blocking vector, 0 if
 *     move is not blocked, or -1 if something is out of map range.
 */
short do_move_colide(struct Thing *p_person, int dx, int dz, struct MyMapElement *p_mapel)
{
    int tl_x, tl_z, tl_idx;
    ushort cur_colv;
    int iter;
    int dx_full, dz_full, dx_half, dz_half;
    int per_cor_x, per_cor_y_m8, per_cor_z;
    short ret;

    tl_idx = p_mapel - game_my_big_map;
    tl_z = tl_idx / MAP_TILE_WIDTH;
    tl_x = tl_idx % MAP_TILE_WIDTH;
    if ((tl_z >= MAP_TILE_HEIGHT) || (tl_x >= MAP_TILE_WIDTH))
        return -1;

    cur_colv = p_mapel->ColHead;
    word_1AA394 = 0;
    per_cor_x = PRCCOORD_TO_MAPCOORD(p_person->X);
    per_cor_y_m8 = PRCCOORD_TO_YCOORD(p_person->Y);
    per_cor_z = PRCCOORD_TO_MAPCOORD(p_person->Z);

    if ((cur_colv != 0) && (debug_hud_collision))
    {
        int xc, zc;

        xc = TILE_TO_MAPCOORD(tl_x, 128);
        zc = TILE_TO_MAPCOORD(tl_z, 128);
        draw_mapcoord_line_at_ground(xc, zc, per_cor_x, per_cor_z, 0x4F);
        draw_mark_over_tile_with_number(tl_x, tl_z, -1, cur_colv);
    }

    ret = 0;
    if (cur_colv == 0)
        return ret;

    //TODO half is usually less than full; investigate
    dz_full = dz >> 8;
    dx_full = dx >> 8;
    dx_half = dx >> 7;
    dz_half = dz >> 7;

    for (iter = 0; iter < 30; iter++)
    {
        struct ColVectList *p_cvl;
        struct ColVect *p_cv;

        p_cvl = &game_col_vects_list[cur_colv];
        p_cv = &game_col_vects[p_cvl->Vect];

        if ((p_cvl->NextColList & 0x8000) != 0)
            goto next_vect;

        {
            int dy;

            dy = p_cv->Y1 - per_cor_y_m8;
            if (dy < 0)
                dy = -dy;
            if (dy >= 0xD0)
                goto next_vect;
        }

        if ((per_cor_x == p_cv->X1) && (per_cor_z == p_cv->Z1)) {
            goto next_vect;
        }
        if ((per_cor_x == p_cv->X2) && (per_cor_z == p_cv->Z2)) {
            goto next_vect;
        }

        if (!two4_line_intersection(p_cv->X1, p_cv->Z1, p_cv->X2, p_cv->Z2,
              per_cor_x, per_cor_z, per_cor_x + dx_half, per_cor_z + dz_half)) {
            goto next_vect;
        }

        word_1AA38E = cur_colv;
        ret = p_cvl->Vect;

        if ((p_cvl->Object >= 0) && (p_cv->Face < 0))
        {
            struct SingleObjectFace4 *p_face;

            p_face = &game_object_faces4[-p_cv->Face];
            if ((p_face->GFlags & FGFlg_Unkn10) != 0)
            {
                int pt2_x, pt2_z;
                s32 cross;

                if ((p_person->Flag2 & TgF2_InsideBuilding) != 0)
                {
                    pt2_x = per_cor_x + dx_half;
                    pt2_z = per_cor_z + dz_half;
                    cross = (pt2_z - p_cv->Z2) * (p_cv->X2 - p_cv->X1)
                          - (pt2_x - p_cv->X2) * (p_cv->Z2 - p_cv->Z1);
                    if (cross < 0)
                        word_1AA394 = 1;
                }
                else if (two4_line_intersection(p_cv->X1, p_cv->Z1, p_cv->X2, p_cv->Z2,
                  per_cor_x, per_cor_z, per_cor_x + dx_full, per_cor_z + dz_full))
                {
                    pt2_x = PRCCOORD_TO_MAPCOORD(p_person->X) + dx_half;
                    pt2_z = PRCCOORD_TO_MAPCOORD(p_person->Z) + dz_half;
                    cross = (p_cv->X2 - p_cv->X1) * (pt2_z - p_cv->Z2)
                          - (p_cv->Z2 - p_cv->Z1) * (pt2_x - p_cv->X2);
                    word_1AA394 = (cross < 0) ? 1 : -1;
                }
                word_1AA392 = ret;
                word_1AA390 = cur_colv;
            }
        }

        if ((ret != 0) && (word_1AA392 != 0) && (ret != word_1AA392))
            break;

    next_vect:
        cur_colv = p_cvl->NextColList & 0x7FFF;
        if (cur_colv == 0)
            break;
    }

    return ret;
}

short person_hit_razor_wire(struct Thing *p_person, ThingIdx thing)
{
    short ret;
    asm volatile (
      "call ASM_person_hit_razor_wire\n"
        : "=r" (ret) : "a" (p_person), "d" (thing));
    return ret;
}

static DIFFTEST_REF_FUNC ubyte create_intelligent_door_asm(short col)
{
    ubyte ret;
    asm volatile (
      "call ASM_create_intelligent_door\n"
        : "=a" (ret) : "a" (col));
    return ret;
}

ubyte create_intelligent_door(short col)
{
    struct ColVect *p_colvect;
    struct SingleObjectFace4 *p_face;
    struct SimpleThing *p_sthing;
    ThingIdx thing;
    short sthing;

    DIFFTEST_SHADOW_RET(create_intelligent_door, ubyte,
      create_intelligent_door_asm(col));

    thing = game_col_vects_list[word_1AA390].Object;
    if ((things[thing].Flag & TngF_TriggerUse) != 0)
        DIFFTEST_RETURN((ubyte)0);

    p_colvect = &game_col_vects[col];
    p_face = &game_object_faces4[-p_colvect->Face];
    if (p_face->WalkHeader != 0)
    {
        sthings[(short)p_face->WalkHeader].Flag |= 0x01;
        p_face = &game_object_faces4[-p_colvect->Face];
        if ((sthings[(short)p_face->WalkHeader].Flag & 0x02) == 0)
            DIFFTEST_RETURN((ubyte)0);
        DIFFTEST_RETURN((ubyte)1);
    }

    if (sthings_used > STHINGS_LIMIT - 5)
        DIFFTEST_RETURN((ubyte)0);
    sthing = get_new_sthing();
    if (sthing == 0)
        DIFFTEST_RETURN((ubyte)0);
    if (sthing > -1 || sthing < -STHINGS_LIMIT)
        DIFFTEST_RETURN((ubyte)0);

    p_colvect = &game_col_vects[col];
    game_object_faces4[-p_colvect->Face].WalkHeader = sthing;
    p_sthing = &sthings[sthing];
    p_sthing->X = (p_colvect->X1 + p_colvect->X2) << 7;
    p_sthing->Z = (p_colvect->Z1 + p_colvect->Z2) << 7;
    p_sthing->Frame = 0;
    p_sthing->StartFrame = 0;
    p_sthing->Y = (p_colvect->Y1 + p_colvect->Y2) << 7;
    p_sthing->Next = 0;
    p_sthing->Type = SmTT_INTELLIG_DOOR;
    p_sthing->Parent = -p_colvect->Face;
    p_sthing->State = 1;
    play_dist_ssample(p_sthing, 0x32, 0x7F, 0x40, 100, 0, 1);
    DIFFTEST_RETURN((ubyte)0);
}

/** Get collision vector which would collide with given thing at given position.
 */
short move_colide_on_tile(struct Thing *p_person, short tl_x, short tl_z, int speed_x, int speed_z)
{
    struct MyMapElement *p_mapel;

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tl_z + tl_x];
    return do_move_colide(p_person, speed_x, speed_z, p_mapel);
}

TbBool move_blocked_by_collision_vect(struct Thing *p_person, short colvect)
{
    short face;

    if (colvect == 0)
        return false;
    if ((p_person->Flag & TngF_Persuaded) != 0)
        return false;
    if (game_col_vects_list[word_1AA38E].Object < 0)
        return false;
    face = game_col_vects[colvect].Face;
    if ((face >= 0) || (game_object_faces4[-face].GFlags & (FGFlg_Unkn10|FGFlg_Unkn04)) != 0)
        return false;
    return true;
}

// overflow flag of subtraction (x-y)
ubyte __OFSUB__(int x, int y)
{
    int y2 = y;
    ubyte sx = (x < 0);
    return (sx ^ (y2 < 0)) & (sx ^ (x-y2 < 0));
}

void adjust_speed_for_colvect_collision(int *p_speed_x, int *p_speed_z, struct Thing *p_person, short colvect)
{
    struct ColVect *p_colvect;
    s64 ldt;
    int dvdr;
    int dt_x, dt_z;
    int dist_sum;
    s64 chk_z, chk_x;
    int sgnhi;
    int norm_z, norm_x;
    int speed_x2, speed_z2;
    sbyte sgn;
    int speed_z1, speed_x1;
    TbBool lower_z, v34;

    p_colvect = &game_col_vects[colvect];

    dt_x = (p_colvect->X2 - p_colvect->X1) << 8;
    dt_z = (p_colvect->Z2 - p_colvect->Z1) << 8;
    dvdr = map_distance_deltas_fast(dt_x, 0, dt_z);
    if (dvdr < 10)
      dvdr = 10;
    ldt = (s64)(p_colvect->X2 - p_colvect->X1) << 24;
    norm_x = ldt / dvdr;
    ldt = (s64)(p_colvect->Z2 - p_colvect->Z1) << 24;
    norm_z = ldt / dvdr;

#if 0 // original implementation
    dist_sum = mul_shift16_sign_pad_lo(*p_speed_x, norm_x) + mul_shift16_sign_pad_lo(*p_speed_z, norm_z);
    speed_x2 = mul_shift16_sign_pad_lo(norm_x, dist_sum);
    speed_z2 = mul_shift16_sign_pad_lo(norm_z, dist_sum);
#else // easier to read version, seem to produce the same results as original implementation? verify!
    dist_sum = (((*p_speed_x) * norm_x) >> 16) + (((*p_speed_z) * norm_z) >> 16);
    speed_x2 = (norm_x * dist_sum) >> 16;
    speed_z2 = (norm_z * dist_sum) >> 16;
#endif

    chk_z = (p_person->Z - (p_colvect->Z1 << 8)) * norm_x;
    chk_x = (p_person->X - (p_colvect->X1 << 8)) * norm_z;

    lower_z = chk_z < chk_x;
    sgn = chk_z != chk_x;
    v34 = __OFSUB__(chk_z >> 32, (chk_x >> 32) + lower_z);
    sgnhi = (chk_z >> 32) - ((chk_x >> 32) + lower_z);
    if (sgnhi)
        sgn = !((sgnhi < 0) ^ v34) - ((sgnhi < 0) ^ v34);

    if (sgn > 0) {
        speed_x1 = -norm_z >> 5;
        speed_z1 = norm_x >> 5;
    } else if (sgn < 0) {
        speed_x1 = norm_z >> 5;
        speed_z1 = -norm_x >> 5;
    } else {
        speed_x1 = 0;
        speed_z1 = 0;
    }

    *p_speed_x = speed_x1 + speed_x2;
    *p_speed_z = speed_z1 + speed_z2;
}

void set_thing_height_on_ground_mesh(struct Thing *p_person, int x, int z)
{
    struct MyMapElement *p_mapel;
    int alt_cc, alt_cr, alt_uc, alt_ur;
    short tile_x, tile_z;
    short subcor_x, subcor_z;
    short new_alt;

    tile_x = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(x));
    tile_z = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(z));

    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + 0) + (tile_x + 1)];
    alt_cr = p_mapel->Alt;
    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + 0) + (tile_x + 0)];
    alt_cc = p_mapel->Alt;
    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + 1) + (tile_x + 0)];
    alt_uc = p_mapel->Alt;
    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + 1) + (tile_x + 1)];
    alt_ur = p_mapel->Alt;

    subcor_x = PRCCOORD_TO_MAPCOORD(x) & 0xFF;
    subcor_z = PRCCOORD_TO_MAPCOORD(z) & 0xFF;
    if (subcor_x + subcor_z >= 256)
        new_alt = alt_ur + (((alt_uc - alt_ur) * (256 - subcor_x)) >> 8) + (((256 - subcor_z) * (alt_cr - alt_ur)) >> 8);
    else
        new_alt = alt_cc + (((alt_cr - alt_cc) * subcor_x) >> 8) + (((alt_uc - alt_cc) * subcor_z) >> 8);
    move_mapwho(p_person, x, MAPCOORD_TO_PRCCOORD(new_alt, 0), z);
}

short person_move(struct Thing *p_person)
{
    int speed_x, speed_z;
    int x, y, z;
    short colvect;
    short retry;

    word_1AA390 = 0;
    if (((gameturn + 32 * p_person->ThingOffset) & 0x7F) == 0) {
        process_random_speech(p_person, 0);
    }
    if ((p_person->SubType == SubTT_PERS_MECH_SPIDER) && !IsSamplePlaying(p_person->ThingOffset, 79, 0)) {
        play_dist_sample(p_person, 79, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_4EVER, 3);
    }
    if (p_person->U.UPerson.AnimMode == ANIM_PERS_Unkn21) {
        set_person_anim_mode(p_person, ANIM_PERS_IDLE);
    }
    if ((p_person->Flag & (TngF_InVehicle|TngF_Unkn4000)) != 0) {
        return 1;
    }

    {
        short used_stamina;
        used_stamina = abs(p_person->U.UPerson.Mood >> 5) - cybmod_chest_level(&p_person->U.UPerson.UMod);
        if (used_stamina < 0)
            used_stamina = 0;
        p_person->U.UPerson.Stamina -= used_stamina;
    }

    if ((p_person->Flag2 & TgF2_Unkn00080000) != 0)
    {
        if ((p_person->Flag & TngF_PlayerAgent) != 0) {
            p_person->U.UPerson.Stamina = p_person->U.UPerson.Stamina - (7 - 2 * cybmod_legs_level(&p_person->U.UPerson.UMod));
        }
        if ((p_person->U.UPerson.Stamina < p_person->U.UPerson.MaxStamina >> 2) && (p_person->State != PerSt_PERSON_BURNING)) {
            set_person_anim_mode(p_person, gun_out_anim(p_person, 0));
            p_person->Timer1 = 48;
            p_person->StartTimer1 = 48;
            p_person->Flag2 &= ~TgF2_Unkn00080000;
            p_person->Speed = calc_person_speed(p_person);
        }
    }
    speed_x = (p_person->Speed * p_person->VX) >> 4;
    speed_z = (p_person->Speed * p_person->VZ) >> 4;
    p_person->Flag2 &= ~TgF2_Unkn0100;
    y = p_person->Y;

    retry = 2;
    if (p_person->State == PerSt_WANDER)
        retry = 0;
    if (p_person->U.UPerson.OnFace != 0)
        retry = 0;

    for (; 1; retry--)
    {
        int tile_x, tile_z;
        int tile_dt_x, tile_dt_z;
        ThingIdx thing;
        TbBool bumped_colvect;

        x = p_person->X + speed_x;
        z = p_person->Z + speed_z;

        if ((x < 0) || (PRCCOORD_TO_MAPCOORD(x) >= MAP_COORD_WIDTH))
            return 1;
        if ((z < 0) || (PRCCOORD_TO_MAPCOORD(z) >= MAP_COORD_HEIGHT))
            return 1;

        if ((p_person->Flag & (TngF_Unkn0010|TngF_Unkn0004)) != 0)
        {
            thing = check_for_other_people(x, y, z, p_person);
            if (thing != 0) {
                p_person->U.UPerson.BumpCount++;
                if (((p_person->Flag & TngF_Unkn0010) == 0) && ((p_person->Flag & TngF_StandOnVehicle) != 0))
                    return 1;
            } else {
                p_person->Flag &= ~(TngF_Unkn0004|TngF_Unkn0010);
            }
        }
        else
        {
            thing = check_for_other_people(x, y, z, p_person);
            if (thing != 0) {
                p_person->U.UPerson.BumpCount++;
                return thing;
            }
        }

        if (p_person->U.UPerson.Within != 0)
        {
            struct Command *p_cmd;

            p_cmd = &game_commands[p_person->U.UPerson.Within];
            if (!check_person_within(p_cmd, PRCCOORD_TO_MAPCOORD(speed_x + x),
              PRCCOORD_TO_MAPCOORD(speed_z + z))) {
                p_person->Flag2 |= TgF2_Unkn0080;
                return 1;
            }
            p_person->Flag2 &= ~TgF2_Unkn0080;
        }

        tile_x = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(p_person->X));
        tile_dt_x = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(x)) - tile_x;
        tile_z = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(p_person->Z));
        tile_dt_z = MAPCOORD_TO_TILE(PRCCOORD_TO_MAPCOORD(z)) - tile_z;

        word_1AA394 = 0;
        word_1AA392 = 0;

        bumped_colvect = false;
        {
            colvect = move_colide_on_tile(p_person, tile_x, tile_z, speed_x, speed_z);
            if ((retry != 0) && move_blocked_by_collision_vect(p_person, colvect))
                bumped_colvect = true;
        }
        if (!bumped_colvect)
        {
            if ((tile_dt_z != 0) && (colvect == 0))
            {
                struct MyMapElement *p_mapel;
                p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + tile_dt_z) + (tile_x)];
                if ((p_person->U.UPerson.OnFace == 0) && ((p_mapel->Flags2 & (MEF2_Unkn04|MEF2_Unkn01)) != 0))
                {
                    if (!retry || ((p_person->Flag & TngF_Persuaded) != 0)) {
                        p_person->U.UPerson.BumpCount++;
                        return 1;
                    }
                    speed_z = 0;
                    continue;
                }
                colvect = move_colide_on_tile(p_person, tile_x, tile_z + tile_dt_z, speed_x, speed_z);
                if ((retry != 0) && move_blocked_by_collision_vect(p_person, colvect))
                    bumped_colvect = true;
            }
            if (!bumped_colvect && (tile_dt_x != 0) && (colvect == 0))
            {
                struct MyMapElement *p_mapel;
                p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z) + (tile_x + tile_dt_x)];
                if ((p_person->U.UPerson.OnFace == 0) && ((p_mapel->Flags2 & (MEF2_Unkn04|MEF2_Unkn01)) != 0))
                {
                    if ((retry == 0) || ((p_person->Flag & TngF_Persuaded) != 0))
                    {
                        p_person->U.UPerson.BumpCount++;
                        return 1;
                    }
                    speed_x = 0;
                    continue;
                }
                colvect = move_colide_on_tile(p_person, tile_x + tile_dt_x, tile_z, speed_x, speed_z);
                if ((retry != 0) && move_blocked_by_collision_vect(p_person, colvect))
                    bumped_colvect = true;
            }
            if (!bumped_colvect && (tile_dt_z != 0) && (tile_dt_x != 0) && (colvect == 0))
            {
                struct MyMapElement *p_mapel;
                p_mapel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + tile_dt_z) + (tile_x + tile_dt_x)];
                if ((p_person->U.UPerson.OnFace == 0) && (p_mapel->Flags2 & (MEF2_Unkn04|MEF2_Unkn01)) != 0)
                {
                    p_person->U.UPerson.BumpCount++;
                    return 1;
                }
                colvect = move_colide_on_tile(p_person, tile_x + tile_dt_x, tile_z + tile_dt_z, speed_x, speed_z);
                if ((retry != 0) && move_blocked_by_collision_vect(p_person, colvect))
                    bumped_colvect = true;
            }
            if (!bumped_colvect)
                break;
        }

        adjust_speed_for_colvect_collision(&speed_x, &speed_z, p_person, colvect);
    }

    if (debug_hud_collision || byte_1C844F) {
        draw_mapcoord_line_at_ground(PRCCOORD_TO_MAPCOORD(p_person->X), PRCCOORD_TO_MAPCOORD(p_person->Z),
          PRCCOORD_TO_MAPCOORD(p_person->X) + PRCCOORD_TO_MAPCOORD(2 * speed_x),
          PRCCOORD_TO_MAPCOORD(p_person->Z) + PRCCOORD_TO_MAPCOORD(2 * speed_z),
          0xE6u);
    }

    if (colvect < 0)
        return 1;
    if (colvect != 0)
    {
        struct ColVectList *p_cvlist;

        p_cvlist = &game_col_vects_list[word_1AA38E];
        if (p_cvlist->Object < 0)
        {
            x = p_person->X + (speed_x >> 3);
            z = p_person->Z + (speed_z >> 3);
            if (!person_is_dead_or_dying(p_person->ThingOffset) &&
              ((p_person->Flag & TngF_Destroyed) == 0) &&
              person_hit_razor_wire(p_person, -p_cvlist->Object)) {
                return 0;
            }
            colvect = 0;
        }
    }
    if (word_1AA392)
    {
        p_person->Flag2 |= TgF2_Unkn40000000;
        if (create_intelligent_door(word_1AA392) != 1) {
            p_person->U.UPerson.LastDist = 32000;
            return 1;
        }
        p_person->U.UPerson.LastDist = 32000;
        colvect = 0;
        if (word_1AA394 != 0)
        {
            if (word_1AA394 <= 0)
                p_person->Flag2 |= TgF2_InsideBuilding;
            else
                p_person->Flag2 &= ~TgF2_InsideBuilding;
        }
    }

    if (p_person->U.UPerson.OnFace != 0)
    {
        if (set_thing_height_on_face(p_person, x, z, p_person->U.UPerson.OnFace)) {
            move_mapwho(p_person, x, p_person->Y, z);
            p_person->U.UPerson.BumpMode = 0;
            p_person->Flag2 |= TgF2_Unkn0100;
            return 0;
        }
        if (!find_and_set_connected_face(p_person, x, z, p_person->U.UPerson.OnFace))
        {
            if ((colvect == 0) || (p_person->U.UPerson.OnFace != game_col_vects[colvect].Face))
                return 1;
            p_person->U.UPerson.OnFace = 0;
            p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn80;
        }
        move_mapwho(p_person, x, p_person->Y, z);
        p_person->U.UPerson.BumpMode = 0;
        p_person->Flag2 |= TgF2_Unkn0100;
        return 0;
    }

    if (colvect != 0)
    {
        short face;

        face = game_col_vects[colvect].Face;
        if (face == 0)
        {
            // No action
        }
        else if (!face_is_blocking_walk(face))
        {
            if (set_thing_height_on_face(p_person, x, z, face)) {
                p_person->U.UPerson.BumpMode = 0;
                p_person->U.UPerson.OnFace = face;
                p_person->Flag2 |= TgF2_Unkn0100;
                move_mapwho(p_person, x, p_person->Y, z);
                return 0;
            }
            colvect = 0;
        }
        else if (p_person->U.UPerson.OnFace != 0)
        {
            colvect = 0;
        }
    }

    if ((colvect == 0) && (p_person->U.UPerson.OnFace == 0))
    {
        set_thing_height_on_ground_mesh(p_person, x, z);
    }
    if (colvect != 0)
        return 1;
    p_person->U.UPerson.BumpMode = 0;
    p_person->Flag2 |= TgF2_Unkn0100;
    return 0;
}

void process_avoid_group(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_process_avoid_group\n"
        : : "a" (p_person));
#endif
    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;
    if (p_person->U.UPerson.RecoilTimer != 0) {
        return;
    }

    p_person->VX = angle_direction[p_person->U.UPerson.Angle].DiX;
    p_person->VZ = angle_direction[p_person->U.UPerson.Angle].DiY;

    if (person_move(p_person))
    {
        ubyte angl;
        sbyte change;
        // Select changed, but not to opposite direction - only one of: -2,-1,1,2
        change = (LbRandomAnyShort() & 3) - 2;
        if (change >= 0) change++;
        angl = (p_person->U.UPerson.Angle + change) & 7;
        change_person_angle(p_person, angl);
    }

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        p_person->Frame = frame[p_person->Frame].Next;
    }

    p_person->U.UPerson.Timer2--;
    if (p_person->U.UPerson.Timer2 < 0)
    {
        ushort rnd;
        rnd = LbRandomAnyShort();
        p_person->U.UPerson.Timer2 = p_person->U.UPerson.StartTimer2 + ((rnd & 0xF) >> 1);
        set_angle_to_avoid_group(p_person);
    }
}

void person_burning(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_burning\n"
        : : "a" (p_person));
    return;
#endif
    p_person->U.UPerson.Brightness = PALETTE_FADE_LEVELS / 2;
    apply_super_quick_light(PRCCOORD_TO_MAPCOORD(p_person->X),
      PRCCOORD_TO_MAPCOORD(p_person->Z), 16 + (LbRandomAnyShort() & 0xF));

    if (((gameturn + p_person->ThingOffset) & 0xF) == 0)
    {
        ushort smpl_no;

        switch (person_sex(p_person))
        {
        case PERSON_FEMALE:
            smpl_no = 2;
            break;
        case PERSON_MALE:
        default:
            smpl_no = 1;
            break;
        }
        play_dist_speech(p_person, smpl_no, FULL_VOL, EQUL_PAN, NORM_PTCH, LOOP_NO, 2);
    }

    if (p_person->U.UPerson.RecoilTimer > 95)
        p_person->U.UPerson.RecoilTimer = 94;
    if ((p_person->Flag & TngF_PlayerAgent) != 0)
        p_person->U.UPerson.RecoilTimer--;
    else
        p_person->U.UPerson.RecoilTimer -= burning_timer_delatas[LbRandomAnyShort() & 3];

    if (p_person->U.UPerson.RecoilTimer != 0)
    {
        process_weapon_wind_down(p_person);

        p_person->VX = angle_direction[p_person->U.UPerson.Angle].DiX;
        p_person->VZ = angle_direction[p_person->U.UPerson.Angle].DiY;

        if (person_move(p_person))
            p_person->U.UPerson.Timer2 = -1;

        if ((p_person->Flag & TngF_Destroyed) == 0)
        {
            p_person->Timer1 -= fifties_per_gameturn;
            if (p_person->Timer1 < 0)
            {
                p_person->Timer1 = p_person->StartTimer1;
                p_person->Frame = frame[p_person->Frame].Next;
            }

            p_person->U.UPerson.Timer2--;
            if (p_person->U.UPerson.Timer2 < 0)
            {
                short angle;

                p_person->U.UPerson.Timer2 = (LbRandomAnyShort() & 7) + p_person->U.UPerson.StartTimer2;
                angle = p_person->U.UPerson.Angle;
                if ((p_person->U.UPerson.Timer2 & 1) != 0)
                    angle++;
                else
                    angle--;
                change_person_angle(p_person, (angle + 8) & 7);
            }
        }
    }
    else
    {
        stop_sample_using_heap(p_person->ThingOffset, 29);
        set_person_dead(p_person, ANIM_PERS_DEAD_ASH);
    }
}

void person_being_persuaded(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_person_being_persuaded\n"
        : : "a" (p_person));
    return;
#endif
    ushort nxframe;

    p_person->Timer1 -= fifties_per_gameturn;
    if (p_person->Timer1 < 0)
    {
        p_person->Timer1 = p_person->StartTimer1;
        nxframe = frame[p_person->Frame].Next;

        if ((frame[nxframe].Flags & 0x01) != 0)
        {
            p_person->State = PerSt_WAIT;
            p_person->Flag &= ~(TngF_Persuaded | TngF_Unkn40000000);
            p_person->Flag |= TngF_Persuaded;
            reset_person_frame(p_person);
        }
        else
        {
            p_person->Frame = nxframe;
        }
    }
}

void process_wander_and_fly(struct Thing *p_person, struct Thing *p_vehicle)
{
#if 0
    asm volatile ("call ASM_process_wander_and_fly\n"
        : : "a" (p_person), "d" (p_vehicle));
#endif
    int cor_base_x, cor_base_z;
    int cor_nxt_x, cor_nxt_z;
    int i;
    ushort angl;

    if (p_vehicle->State != VehSt_PARKED_PARAL &&
      p_vehicle->State != VehSt_NONE &&
      p_vehicle->State != VehSt_FLY_LANDING)
    {
        short timer;

        timer = p_person->U.UPerson.Timer2;
        p_person->U.UPerson.Timer2--;
        if ((timer >= 0) && p_vehicle->State != VehSt_UNKN_41)
            return;
    }

    angl = LbRandomAnyShort() & LbFPMath_AngleMask;
    cor_base_z = PRCCOORD_TO_MAPCOORD(p_person->Z);
    cor_base_x = PRCCOORD_TO_MAPCOORD(p_person->X);

    for (i = 0; i < 8; i++)
    {
        cor_nxt_x = cor_base_x + (lbSinTable[angl + LbFPMath_PI/2] >> 4);
        cor_nxt_z = cor_base_z + (lbSinTable[angl] >> 4);
        //TODO avoid running into buildings as well?
        if (cor_nxt_x >= TILE_TO_MAPCOORD(1,0)/2 &&
          cor_nxt_x < MAP_COORD_WIDTH - TILE_TO_MAPCOORD(1,0)/2)
            if (cor_nxt_z >= TILE_TO_MAPCOORD(1,0)/2 &&
              cor_nxt_z < MAP_COORD_HEIGHT - TILE_TO_MAPCOORD(1,0)/2)
                break;
        angl = (angl + LbFPMath_PI/4) & LbFPMath_AngleMask;
    }
    if (i < 8)
    {
        start_goto_vehicle(p_vehicle, cor_nxt_x, cor_nxt_z);
        p_person->U.UPerson.Timer2 = 1500;
    }
}

void unkn_update_thing_to_light(ushort light_id, struct Thing *p_thing, ubyte a3)
{
#if 0
    asm volatile ("call ASM_unkn_update_thing_to_light\n"
        : : "a" (light_id), "d" (p_thing), "b" (a3));
    return;
#endif
    struct FullLight *p_fulight;
    int dx, dz;
    short len1, len2;

    p_fulight = &game_full_lights[light_id];
    dx = p_fulight->X - PRCCOORD_TO_MAPCOORD(p_thing->X);
    dz = p_fulight->Z - PRCCOORD_TO_MAPCOORD(p_thing->Z);
    len1 = arctan(dx, -dz) + 4;
    len2 = LbSqrL(dz * dz + dx * dx);
    p_thing->U.UPerson.Shadows[a3 + 0] = len1 >> 3;
    p_thing->U.UPerson.Shadows[a3 + 1] = len2 >> 3;
}

void process_lighting_unkn1(struct Thing *p_person)
{
#if 0
    asm volatile ("call ASM_process_lighting_unkn1\n"
        : : "a" (p_person));
    return;
#endif
    struct MyMapElement *p_mapel;
    MapCoord cor_x, cor_z;
    ushort qlight;

    if ((p_person->Flag & TngF_Destroyed) != 0)
        return;

    p_person->U.UPerson.Shadows[0] = 0;
    p_person->U.UPerson.Shadows[1] = 0;
    p_person->U.UPerson.Shadows[2] = 0;
    p_person->U.UPerson.Shadows[3] = 0;

    cor_x = PRCCOORD_TO_MAPCOORD(p_person->X);
    cor_z = PRCCOORD_TO_MAPCOORD(p_person->Z);
    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * MAPCOORD_TO_TILE(cor_z) + MAPCOORD_TO_TILE(cor_x)];

    qlight = get_brightest_quick_light(p_mapel->Shade);

    if (qlight != 0) {
        unkn_update_thing_to_light(quick_light_to_full_light(qlight), p_person, 0);
    }
}

void person_find_next_state(struct Thing *p_person)
{
    short cmd;

    if (((p_person->Flag & TngF_PlayerAgent) != 0) && ((p_person->Flag2 & TgF2_Unkn0800) == 0))
    {
        person_init_wait(p_person, 50);
        return;
    }

    if (p_person->U.UPerson.ComCur == 0)
    {
        person_finish_executing_commands(p_person);
        person_init_wait(p_person, 50);
        return;
    }

    if ((p_person->Flag & TngF_Unkn0040) != 0)
    {
        p_person->Flag &= ~TngF_Unkn0040;
        person_init_command(p_person, PCmd_GET_ITEM);
        return;
    }

    if (!is_command_completed(p_person))
        return;

    if (p_person->U.UPerson.ComHead == 0)
    {
        person_init_wait(p_person, 50);
        return;
    }

    cmd = p_person->U.UPerson.ComCur;
    cmd = game_commands[cmd].Next;
    p_person->U.UPerson.ComCur = cmd;

    // Repeat with new ComCur
    if (p_person->U.UPerson.ComCur == 0)
    {
        person_finish_executing_commands(p_person);
        person_init_wait(p_person, 100);
        return;
    }

    person_init_command(p_person, PCmd_USE_WEAPON);
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
void check_unlock_target(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_check_unlock_target\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
}
/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int get_target_priority(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_get_target_priority\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}

/** Routed asm (internal label). Arguments and return value are the
 * raw registers the asm takes (eax, edx, ebx, ecx), to be typed when
 * remade. Registers it does not preserve are declared clobbered.
 */
int find_a_target(int a1, int a2, int a3, int a4)
{
    int ra = a1, rd = a2, rb = a3, rc = a4;
    asm volatile ("call ASM_find_a_target\n"
        : "+a" (ra), "+d" (rd), "+b" (rb), "+c" (rc) : : "cc", "memory");
    return ra;
}



static DIFFTEST_REF_FUNC void people_intel_asm(ubyte flag)
{
    asm volatile ("call ASM_people_intel\n"
        : : "a" (flag));
}

/** Default target search step of people_intel() for a person.
 */
static void people_intel_group_search(struct Thing *p_thing)
{
    if (p_thing->U.UPerson.Target2 != 0)
        return;
    if ((p_thing->Flag & (TngF_ShootAtPos|TngF_PlayerAgent)) != (TngF_ShootAtPos|TngF_PlayerAgent))
        p_thing->Flag &= ~TngF_TriggerUse;
    if (p_thing->U.UPerson.WeaponTurn != 0)
        return;
    find_target_from_group((int)p_thing, 0, 0, 0);
}

void people_intel(ubyte flag)
{
    struct Thing *p_thing;
    ushort plyr, plagent, cnt, limit;
    ThingIdx thing;
    ushort state;
    s32 remain;

    DIFFTEST_SHADOW(people_intel, people_intel_asm(flag));

    if (flag == 1)
    {
        word_1AA678 = 0;
        return;
    }

    // Agents with extreme mood search for targets periodically
    for (plyr = 0; plyr < 8; plyr++)
    {
        if ((ingame.InNetGame_UNSURE & (1 << plyr)) == 0)
            continue;
        for (plagent = 0; plagent < playable_agents; plagent++)
        {
            short mood, period;

            p_thing = players[plyr].MyAgent[plagent];
            if (p_thing == NULL)
                continue;
            if ((p_thing->Flag & (TngF_SelectedAgent|TngF_Destroyed)) != 0)
                continue;
            mood = p_thing->U.UPerson.Mood;
            if ((ushort)p_thing->State == 0x2b)
            {
                if ((mood > -10) && (mood < 20))
                    continue;
            }
            period = (0x66 - abs(mood)) >> 2;
            if (period <= 0)
                continue;
            if (((u32)gameturn % (u32)(s32)period) != 0)
                continue;
            find_target_from_group((int)p_thing, 0, 0, 0);
        }
    }

    // Directly controlled agents
    for (plyr = 0; plyr < 8; plyr++)
    {
        PlayerInfo *p_player;

        if ((ingame.InNetGame_UNSURE & (1 << plyr)) == 0)
            continue;
        p_player = &players[plyr];
        for (plagent = 0; plagent < p_player->DoubleMode + 1; plagent++)
        {
            ThingIdx target;

            if ((p_player->UserInput[plagent].ControlMode & 0x1fff) == UInpCtr_Mouse)
                continue;
            p_thing = &things[p_player->DirectControl[plagent]];
            if ((p_thing->Flag & TngF_SelectedAgent) == 0)
                continue;
            if ((p_thing->Flag2 & (TgF2_IgnoreEnemies|TgF2_Unkn00080000)) != 0)
            {
                check_unlock_target((int)p_thing, 0, 0, 0);
                continue;
            }
            if (p_thing->PTarget == NULL)
                p_thing->Flag2 &= ~TgF2_Unkn00200000;
            if ((p_thing->Flag2 & TgF2_Unkn00200000) != 0)
            {
                check_unlock_target((int)p_thing, 0, 0, 0);
                continue;
            }
            target = find_a_target((int)p_thing, 0, 0, 0);
            if (target == 0)
                p_thing->PTarget = NULL;
            else
                p_thing->PTarget = &things[(ushort)target];
        }
    }

    // A few people from the list get to search for targets each turn
    thing = same_type_head[1];
    if (thing == 0)
        return;

    p_thing = NULL;
    cnt = 0;
    limit = word_1AA678;
    while ((cnt < limit) && (thing != 0))
    {
        p_thing = &things[(ushort)thing];
        cnt++;
        thing = p_thing->LinkSame;
    }
    if (thing == 0)
    {
        word_1AA678 = 0;
        thing = same_type_head[1];
    }

    remain = 15;
    do
    {
        if (thing == 0)
        {
            // TODO The asm keeps the previous person here and re-reads its LinkSame,
            // so the remaining iterations only re-set the counter
            word_1AA678 = 0;
            goto next_person;
        }
        p_thing = &things[(ushort)thing];
        if (p_thing->U.UPerson.CurrentWeapon == 0x1e)
            goto next_person;
        if ((p_thing->Flag2 & (TgF2_IgnoreEnemies|TgF2_ExistsOffMap|TgF2_Unkn00080000|TgF2_KnockedOut)) != 0)
            goto next_person;
        if ((p_thing->Flag & (TngF_Unkn4000|TngF_PlayerAgent|TngF_Destroyed)) != 0)
            goto next_person;

        state = (ushort)p_thing->State & 0x7fff;
        switch (state)
        {
        case 7: case 8: case 9: case 10:
        case 0xc: case 0xd: case 0xe:
        case 0x1c: case 0x1d:
        case 0x24: case 0x33:
            break;
        case 0x1b:
        {
            struct Thing *p_target;

            p_target = p_thing->PTarget;
            people_intel_group_search(p_thing);
            if (p_thing->PTarget == NULL)
                p_thing->PTarget = p_target;
            break;
        }
        case 0x2b:
            if ((p_thing->Flag & TngF_PlayerAgent) != 0)
                break;
            people_intel_group_search(p_thing);
            break;
        default:
            people_intel_group_search(p_thing);
            break;
        }
next_person:
        word_1AA678++;
        thing = p_thing->LinkSame;
        remain--;
    } while ((ushort)remain != 0);
}

void process_person(struct Thing *p_person)
{
    short state;
    ushort ccol;

    if ( (p_person->Flag & TngF_PlayerAgent) != 0 && (p_person->Flag2 & TgF2_Unkn10000000) != 0 )
    {
        state = p_person->State;
        if ((state != PerSt_GET_ITEM) && (state != PerSt_DROP_ITEM) && (state != PerSt_PICKUP_ITEM)
          && ((p_person->Flag & (TngF_Unkn40000000|TngF_Destroyed)) == 0)
          && ((p_person->Flag2 & (TgF2_KnockedOut|TgF2_Unkn0008)) == 0))
        {
            p_person->GotoThingIndex = p_person->Owner;
            p_person->State = PerSt_PROTECT_PERSON;
            person_protect_update_follow_distance(p_person, p_person->GotoThingIndex);
            p_person->Flag2 &= ~TgF2_Unkn10000000;
        }
    }
    p_person->Flag2 &= ~TgF2_Unkn8000;
    ccol = get_mapel_col_column_head(p_person->X >> 16, p_person->Z >> 16);
    if (((p_person->Flag2 & TgF2_InsideBuilding) != 0) && (ccol == 0))
        p_person->Flag2 &= ~TgF2_InsideBuilding;

    if ( ((gameturn + p_person->ThingOffset) & 0x7F) == 0)
    {
        p_person->U.UPerson.Flag3 &= ~PrsF3_Unkn20;
        if ((p_person->Flag2 & TgF2_Unkn00020000) != 0)
        {
            p_person->U.UPerson.ComTimer = -1;
            p_person->Flag2 &= ~TgF2_Unkn00020000;
            remove_path(p_person);
        }
    }
    if (((p_person->Flag2 & TgF2_Unkn08000000) != 0) && ((p_person->Flag & TngF_PlayerAgent) != 0))
    {
        state = p_person->State;
        if ((state != PerSt_GET_ITEM) && (state != PerSt_PICKUP_ITEM))
        {
            short pstng_x, pstng_y, pstng_z;
            make_peep_protect_peep(p_person, &things[p_person->Owner]);
            //TODO separate drawing from state update
            get_thing_position_mapcoords(&pstng_x, &pstng_y, &pstng_z, p_person->ThingOffset);
            enlist_hud_draw_mapcoord_flat_circle(pstng_x, pstng_y, pstng_z,
              200, 0, 1, colour_lookup[ColLU_WHITE]);
        }
    }
    if ((p_person->Flag2 & TgF2_Unkn0020) != 0)
    {
        return;
    }
    if ((p_person->Flag2 & TgF2_Unkn00100000) != 0)
    {
        process_tasered_person(p_person);
        return;
    }
    if ((p_person->Flag2 & TgF2_KnockedOut) != 0)
    {
        process_knocked_out(p_person);
        stop_looped_weapon_sample(p_person, p_person->U.UPerson.CurrentWeapon);
        return;
    }
    if ((p_person->Flag2 & TgF2_Unkn0008) != 0)
    {
        p_person->U.UPerson.BumpCount--;
        if (p_person->U.UPerson.BumpCount != 0)
        {
            if (((gameturn + p_person->ThingOffset) & 7) == 0)
                person_intel(p_person);
        }
        else
        {
            ushort group;

            group = p_person->U.UPerson.EffectiveGroup;
            p_person->Flag2 &= ~TgF2_Unkn0008;
            if (group >= 128)
              p_person->U.UPerson.EffectiveGroup = group + 128;
            p_person->U.UPerson.Target2 = 0;
            p_person->PTarget = NULL;
            p_person->State = PerSt_NONE;
            p_person->Flag |= TngF_Unkn0040;
            p_person->Speed = calc_person_speed(p_person);
        }
    }
    else
    {
        ushort limit;
        limit = 8;
        if ((gameturn & 0x7FF) == 0)
            p_person->U.UPerson.BumpCount = 0;
        if ((p_person->Flag & TngF_PlayerAgent) != 0)
            limit = 2;
        if (p_person->U.UPerson.BumpCount > limit)
        {
            p_person->U.UPerson.BumpCount = 0;
            p_person->Flag &= ~TngF_Unkn00020000;
            if ((p_person->Flag & TngF_Persuaded) == 0)
                p_person->Flag |= TngF_Unkn0004;
            remove_path(p_person);
            p_person->U.UPerson.ComTimer = -1;
        }
    }
    if ((gameturn & 3) == 0)
    {
        state = p_person->State;
        if ((state != PerSt_WAIT) && (state != 0))
        {
            p_person->Flag &= ~TngF_Unkn08000000;
            p_person->Flag &= ~TngF_StandOnVehicle;
        }
    }
    if ((p_person->U.UPerson.BumpMode != 0) && ((p_person->Flag & TngF_Destroyed) == 0))
    {
        if ((p_person->Flag2 & TgF2_Unkn0100) != 0)
            p_person->U.UPerson.BumpMode = 0;
        else
            process_im_shoved(p_person);
    }
    if (((p_person->Flag & TngF_Unkn40000000) == 0) || (((p_person->Flag & TngF_Destroyed) == 0)
        && ((p_person->Flag & (TngF_Persuaded|TngF_DangerFlee|TngF_WepRecoil|TngF_Unkn4000|TngF_StationrSht)) != 0)))
    {
        if ((p_person->Flag & TngF_Unkn4000) != 0)
        {
            return;
        }
        if ((p_person->Flag & TngF_WepRecoil) != 0)
        {
            stop_looped_weapon_sample(p_person, p_person->U.UPerson.CurrentWeapon);
            person_recoil(p_person);
            calc_lighting(p_person);
            process_weapon(p_person);
            return;
        }
        if (((p_person->Flag & TngF_Persuaded) != 0) && ((p_person->Flag & TngF_Destroyed) == 0))
        {
            process_persuaded(p_person);
            calc_lighting(p_person);
            process_weapon(p_person);
            return;
        }
        if (((p_person->Flag & TngF_DangerFlee) != 0) && ((p_person->Flag & (TngF_SelectedAgent|TngF_Destroyed)) == 0))
        {
            person_run_away(p_person);
            calc_lighting(p_person);
            return;
        }

        if ((p_person->Flag & TngF_StationrSht) != 0)
        {
            if ((p_person->U.UPerson.WeaponTurn == 0) && ((p_person->Flag & TngF_TriggerUse) == 0))
            {
                p_person->Flag &= ~TngF_StationrSht;
            }
            process_weapon(p_person);
            return;
        }
    }

    if (((p_person->Flag & (TngF_InVehicle|TngF_Unkn40000000|TngF_Persuaded|TngF_Unkn4000)) == 0)
      && ((p_person->Flag2 & TgF2_Unkn0008) == 0)
      && ((gameturn + p_person->ThingOffset) & 3) == 0
      && (p_person->Flag & TngF_Destroyed) == 0)
    {
        if (person_is_basic_civilian(p_person, false)
          || person_is_scientist(p_person, false))
        {
            process_danger(p_person);
        }
    }

    if (((p_person->Flag & (TngF_Unkn40000000|TngF_PlayerAgent)) == 0 || (p_person->Flag2 & TgF2_Unkn0800) != 0)
      && ((gameturn + p_person->ThingOffset) & 7) == 0)
    {
        if (p_person->U.UPerson.ComHead != 0)
        {
          struct Command *p_cmd;
          ushort cmd;

          cmd = p_person->U.UPerson.ComCur;
          p_cmd = &game_commands[cmd];
          if (((p_cmd->Flags & PCmdF_RunUntil) != 0) && ((p_person->Flag & TngF_Destroyed) == 0))
          {
              cmd = person_command_until_check_condition(p_person, cmd);
              if (cmd != CMD_CONDITION_CONTINUE)
              {
                  p_person->U.UPerson.ComCur = cmd;
                  p_person->State = PerSt_NONE;
                  person_init_command(p_person, PCmd_PROTECT_PERSON);
              }
          }
        }
    }

    if ((p_person->Flag & (TngF_Unkn40000000|TngF_Destroyed)) != 0)
        p_person->Flag &= ~TngF_TriggerUse;
    else
        process_weapon(p_person);

    //TODO separate drawing from state update
    if ((debug_hud_collision) && (p_person->Y != 0))
    {
        char locstr[150];
        short pstng_x, pstng_y, pstng_z;

        sprintf(locstr, "%ld", (long)p_person->Y);
        get_thing_position_mapcoords(&pstng_x, &pstng_y, &pstng_z, p_person->ThingOffset);
        draw_text_transformed(pstng_x, pstng_y, pstng_z, locstr);
    }

    if ((p_person->U.UPerson.Target2 != 0) && ((p_person->Flag & (TngF_Unkn40000000|TngF_Destroyed)) == 0))
    {
        person_kill_target2(p_person);
        if (p_person->State == PerSt_PROTECT_PERSON)
          process_protect_person(p_person);
    }
    else if ((p_person->Flag2 & TgF2_Unkn0008) != 0)
    {
        process_wander(p_person);
        if (p_person->U.UPerson.Timer2 == 1)
            p_person->Speed = 500 + (LbRandomAnyShort() % 700);
    }
    else
    {
        switch (p_person->State & 0x7FFF)
        {
        case PerSt_NONE:
              person_find_next_state(p_person);
              break;
        case PerSt_GOTO_POINT:
              person_goto_point(p_person);
              if ((p_person->State == 0) &&
                ((p_person->U.UPerson.Flag3 & PrsF3_Unkn04) != 0) &&
                ((p_person->Flag & TngF_SelectedAgent) == 0))
              {
                  p_person->State = PerSt_PROTECT_PERSON;
                  person_protect_update_follow_distance(p_person, p_person->GotoThingIndex);
              }
              break;
        case PerSt_WANDER:
              process_wander(p_person);
              break;
        case PerSt_WAIT:
              person_wait(p_person);
              break;
        case PerSt_PICKUP_ITEM:
              person_pickup(p_person);
              break;
        case PerSt_DROP_ITEM:
              person_drop_item(p_person);
              break;
        case PerSt_DIEING:
              person_dieing(p_person);
              break;
        case PerSt_DEAD:
              person_dead(p_person);
              break;
        case PerSt_INIT_SAVE_VICTIM:
        case PerSt_SAVE_VICTIM:
              person_save_victim(p_person);
              break;
        case PerSt_GOTO_PERSON:
              person_goto_person_nav(p_person);
              break;
        case PerSt_KILL_PERSON:
              person_kill_person(p_person);
              break;
        case PerSt_BLOCK_PERSON:
              person_block_person(p_person);
              break;
        case PerSt_SCARE_PERSON:
              person_scare_person(p_person);
              break;
        case PerSt_STATIONARY_SHOT:
              process_stationary_shot(p_person);
              break;
        case PerSt_RECOIL_HIT:
              person_recoil(p_person);
              break;
        case PerSt_GOTO_POINT_REL:
              person_goto_point_rel(p_person);
              break;
        case PerSt_PERSON_BURNING:
              person_burning(p_person);
              break;
        case PerSt_PERSUADE_PERSON:
              person_persuade_person(p_person);
              break;
        case PerSt_FOLLOW_PERSON:
              person_follow_person(p_person);
              break;
        case PerSt_SUPPORT_PERSON:
              process_support_person(p_person);
              break;
        case PerSt_PROTECT_PERSON:
              process_protect_person(p_person);
              break;
        case PerSt_GET_ITEM:
              person_get_item(p_person);
              break;
        case PerSt_USE_VEHICLE:
              person_use_vehicle(p_person);
              break;
        case PerSt_WAIT_VEHICLE:
              person_wait_vehicle(p_person);
              break;
        case PerSt_CATCH_TRAIN:
              person_catch_train(p_person);
              break;
        case PerSt_WAIT_TRAIN:
              process_wait_train(p_person);
              break;
        case PerSt_DESTROY_BUILDING:
              person_destroy_building(p_person);
              break;
        case PerSt_GO_PLANT_MINE:
              person_go_plant_mine(p_person);
              break;
        case PerSt_WAIT_TO_EXIT_VEHICLE:
              person_attempt_to_leave_vehicle(p_person);
              break;
        case PerSt_CATCH_FERRY:
              person_catch_ferry(p_person);
              break;
        case PerSt_EXIT_FERRY:
              person_attempt_to_leave_ferry(p_person);
              break;
        case PerSt_AVOID_GROUP:
              process_avoid_group(p_person);
              break;
        case PerSt_INIT_COMMAND:
              person_init_command(p_person, PCmd_DROP_SPEC_ITEM);
              break;
        case PerSt_BEING_PERSUADED:
              person_being_persuaded(p_person);
              break;
        default:
              break;
        }
        calc_lighting(p_person);
        process_lighting_unkn1(p_person);
    }
}

/******************************************************************************/
