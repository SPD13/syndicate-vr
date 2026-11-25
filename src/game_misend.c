/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_misend.c
 *     The end of a mission: the fade-out and the campaign's outcome.
 * @par Purpose:
 *     The rules of a mission's end, which the desktop (load_packet(),
 *     mission_over() in game.c) and a front end without screens share:
 *     the turns of the fade-out once the mission is decided, the rewards,
 *     the mission state, and the missions the outcome opens.
 * @par Comment:
 *     The front end shows the conclusion and its videos between the steps.
 * @author   SyndicateVR
 * @date     24 Nov 2026 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "game_misend.h"

#include "bfutility.h"

#include "campaign.h"
#include "cybmod.h"
#include "febrief.h"
#include "femail.h"
#include "game.h"
#include "game_data.h"
#include "game_options.h"
#include "game_world.h"
#include "lvobjctv.h"
#include "misstat.h"
#include "player.h"
#include "research.h"
#include "thing.h"
#include "weapon.h"
#include "swlog.h"
/******************************************************************************/

/** The mission that ended and its state, from the outcome to the end. */
static ushort mission_over_missi;
static short mission_over_state;

const char *miss_end_sta_names[] = {
  "undecided state",
  "ending success",
  "ending fail",
  "non-ending part success",
  "non-ending part fail",
  "imm. next after success",
  "",
};

void compound_mission_brief_store_next(void)
{
    ushort old_missi, new_missi;

    old_missi = ingame.CurrentMission;
    new_missi = mission_list[old_missi].SuccessTrigger[0];
    brief_store[open_brief - 1].Mission = new_missi;
    replace_mission_state_slot(old_missi, new_missi);
}

void compound_mission_immediate_start_next(void)
{
    ushort missi;
    missi = brief_store[open_brief - 1].Mission;
    restart_back_into_mission(missi);
}

void mission_over_give_extra_reward(ushort missi)
{
    struct Mission *p_missi;
    int i;

    p_missi = &mission_list[missi];

    if (p_missi->ResearchWeapons != 0) {
        research_weapon_flags_allow(p_missi->ResearchWeapons);
    }

    switch (p_missi->ExtraRewardType)
    {
    case MEReward_ResearchLab:
        research.NumBases++;
        // Fall through
    case MEReward_Scientists:
        research.Scientists += p_missi->ExtraRewardParam;
        break;
    case MEReward_CybModResearched:
        research_cymod_complete(p_missi->ExtraRewardParam);
        break;
    case MEReward_CybModSingle:
        for (i = 0; i < cryo_agents.NumAgents; i++)
        {
            TbBool added;

            added = add_mod_to_flags_no_replace(&cryo_agents.Mods[i], p_missi->ExtraRewardParam);
            if (!added)
                continue;

            if (i < 4) {
                PlayerInfo *p_locplayer;
                p_locplayer = &players[local_player_no];
                add_mod_to_flags(&p_locplayer->Mods[i], p_missi->ExtraRewardParam);
            }
            break;
        }
        break;
    case MEReward_WeaponResearched:
        research_weapon_complete(p_missi->ExtraRewardParam);
        break;
    case MEReward_WeaponSingle:
        for (i = 0; i < cryo_agents.NumAgents; i++)
        {
            TbBool added;

            added = weapons_add_one(&cryo_agents.Weapons[i], &cryo_agents.FourPacks[i], p_missi->ExtraRewardParam);
            if (!added)
                continue;

            /* TODO re-ebable when player->FourPacks is unified and in the same format as cryo_agents.FourPacks
            if (i < 4) {
                PlayerInfo *p_locplayer;
                p_locplayer = &players[local_player_no];
                weapons_add_one(&p_locplayer->Weapons[i], &p_player->FourPacks[i], p_missi->ExtraRewardParam);
            }*/
            break;
        }
        break;
    }
}

void hide_weapons_for_agents_from_player(PlayerInfo *p_locplayer)
{
    struct Thing *p_agent;
    short i;
    for (i = 0; i < playable_agents; i++)
    {
        p_agent = p_locplayer->MyAgent[i];

        if (p_agent->Type != TT_PERSON) continue;
        if ((p_agent->Flag & TngF_Destroyed) != 0) continue;

        p_agent->U.UPerson.CurrentWeapon = WEP_NULL;
        process_clone_disguise(p_agent);
    }
}

void mission_over_prepare_agents(void)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    hide_weapons_for_agents_from_player(p_locplayer);
}

void mission_over_update_players(void)
{
    PlayerInfo *p_locplayer;

    p_locplayer = &players[local_player_no];
    cryo_update_agents_from_player(p_locplayer);
}

u32 calculate_cash_gain_from_persuaded_person(struct Thing *p_person)
{
    u32 credits;

    credits = 0;
    credits += person_type_get_persuasion_credit(p_person->SubType);
    credits += person_carried_weapons_pesuaded_sell_value(p_person);

    return credits;
}

u32 mission_over_calculate_cash_gain_from_persuaded_crowd(ushort tgroup)
{
    u32 credits;
    struct Thing *p_person;
    ThingIdx person;
    short i;

    credits = 0;
    person = get_thing_same_type_head(TT_PERSON, -1);
    for (i = 0; person > 0; person = p_person->LinkSame, i++)
    {
        if (i >= THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
        p_person = &things[person];
        if ((p_person->Flag & TngF_Persuaded) == 0)
            continue;
        if (p_person->U.UPerson.EffectiveGroup != tgroup)
            continue;

        credits += calculate_cash_gain_from_persuaded_person(p_person);
    }
    return credits;
}

void mission_over_gain_personnel_from_persuaded_crowd(void)
{
    struct Thing *p_person;
    ThingIdx person;
    short i;

    person = get_thing_same_type_head(TT_PERSON, -1);
    for (i = 0; person > 0; person = p_person->LinkSame, i++)
    {
        if (i >= THINGS_LIMIT) {
            LOGERR("Infinite loop in same type things list");
            break;
        }
        p_person = &things[person];
        if ((p_person->Flag & TngF_Persuaded) == 0)
            continue;
        if (p_person->U.UPerson.EffectiveGroup != ingame.MyGroup)
            continue;

        if (person_type_is_synd_agent(p_person->SubType))
            add_agent(p_person->U.UPerson.WeaponsCarried, p_person->U.UPerson.UMod.Mods);

        if (person_type_is_scientist(p_person->SubType))
            research.Scientists++;
    }
}

void mission_over_gain_persuaded_crowd_rewards(void)
{
    ingame.Credits += mission_over_calculate_cash_gain_from_persuaded_crowd(ingame.MyGroup);
    mission_over_gain_personnel_from_persuaded_crowd();
}

u32 mission_over_calculate_player_cash_gain_from_items(void)
{
    u32 credits;
    short sthing;
    struct SimpleThing *p_sthing;

    credits = 0;
    for (sthing = sthings_used_head; sthing != 0; sthing = p_sthing->LinkChild)
    {
        p_sthing = &sthings[sthing];
        if (p_sthing->Type == SmTT_CARRIED_ITEM)
        {
            struct Thing *p_owntng;
            p_owntng = &things[p_sthing->U.UWeapon.Owner];
            // Item carried by non-persuaded player agents
            if (((p_owntng->Flag & TngF_PlayerAgent) != 0) &&
              ((p_owntng->Flag & TngF_Persuaded) == 0))
            {
                if (p_sthing->U.UWeapon.WeaponType == 0)
                    credits += 100 * p_sthing->U.UWeapon.Ammo;
                LOGSYNC("Carried item thing %hd picked by the player", sthing);
            }
            // Item carried by people persuaded by the player
            else if (((p_owntng->Flag & TngF_Persuaded) != 0) &&
              (p_owntng->U.UPerson.EffectiveGroup == ingame.MyGroup))
            {
                if (p_sthing->U.UWeapon.WeaponType == 0)
                    credits += 100 * p_sthing->U.UWeapon.Ammo;
                LOGSYNC("Carried item thing %hd picked by persuadee", sthing);
            }
            else
            {
                LOGSYNC("Carried item thing %hd not under player control", sthing);
            }
        }
    }
    return credits;
}

void update_player_cash(void)
{
    u32 credits;

    credits = mission_over_calculate_player_cash_gain_from_items();
    ingame.Credits += credits;
}

void do_start_triggers(short missi)
{
    short nxmissi, sptrig;

    for (nxmissi = missi; 1; nxmissi = sptrig)
    {
        if (true)
            sptrig = mission_list[nxmissi].SpecialTrigger[0];
        if (sptrig == 0)
            sptrig = mission_list[nxmissi].SpecialTrigger[1];
        if (sptrig == 0)
            break;
        set_mission_state_using_state_slot(sptrig, MResol_UNDECIDED);
    }
}

ushort open_new_mission(short missi)
{
    if (mission_has_immediate_previous(missi)) {
        LOGSYNC("No slot needed for mission %d", (int)missi);
        return 0;
    }

    set_mission_state_using_state_slot(missi, MResol_UNDECIDED);
    do_start_triggers(missi);
    queue_up_new_mail(1, missi);

    return 0;
}

TbBool check_mission_conds(ushort missi)
{
    int i;
    ushort cmissi;

    for (i = 0; i < 5; i++)
    {
        cmissi = mission_list[missi].MissionCond[i];
        if ((cmissi > 0) && (mission_list[cmissi].Complete != MResol_COMPLETED))
          return false;
    }
    return true;
}

TbBool mission_has_no_special_triggers(ushort missi)
{
    struct Mission *p_missi;
    int i;

    p_missi = &mission_list[missi];

    for (i = 0; i < 3; i++)
    {
        if (p_missi->SpecialTrigger[i] != 0)
          return false;
    }
    return true;
}

TbBool mission_special_triggers_2_is_self(ushort missi)
{
    struct Mission *p_missi;

    p_missi = &mission_list[missi];

    return (p_missi->SpecialTrigger[2] == missi);
}

void mission_reset_spec_triggers_2_chain(ushort missi)
{
    ushort next_missi;

    next_missi = missi;
    while (next_missi != 0)
    {
        ushort tmp_missi;

        LOGSYNC("Setting mission=%d owner to mission=%d", (int)next_missi, (int)missi);
        mission_list[next_missi].SpecialTrigger[2] = missi;

        // TODO Why only one these? If replacing, shouldn't we replace both?
        tmp_missi = mission_list[next_missi].SpecialTrigger[0];
        if (tmp_missi == 0)
            tmp_missi = mission_list[next_missi].SpecialTrigger[1];
        next_missi = tmp_missi;
    }
}

void mission_special_triggers_0_1_set_fail(ushort missi)
{
    ushort next_missi;

    next_missi = missi;
    while (next_missi > 0)
    {
        ushort tmp_missi;

        mission_list[next_missi].Complete = MResol_FAILED;

        // TODO Why only one these? If failing, shouldn't we fail both?
        tmp_missi = mission_list[next_missi].SpecialTrigger[0];
        if (tmp_missi == 0)
            tmp_missi = mission_list[next_missi].SpecialTrigger[1];
        next_missi = tmp_missi;
    }
}

void mission_copy_conds_and_succ_fail_triggers(ushort dst_missi, ushort src_missi)
{
    int i;

    LOGSYNC("Copying mission=%d props to mission=%d", (int)src_missi, (int)dst_missi);
    for (i = 0; i < 5; i++) {
        mission_list[dst_missi].MissionCond[i] = mission_list[src_missi].MissionCond[i];
    }
    for (i = 0; i < 3; i++) {
        mission_list[dst_missi].SuccessTrigger[i] = mission_list[src_missi].SuccessTrigger[i];
        mission_list[dst_missi].FailTrigger[i] = mission_list[src_missi].FailTrigger[i];
    }
}

ushort mission_fire_success_triggers(ushort missi)
{
    struct Mission *p_missi;
    ushort n;
    int i;

    p_missi = &mission_list[missi];

    n = 0;
    for (i = 0; i < 3; i++)
    {
        ushort new_missi;
        new_missi = p_missi->SuccessTrigger[i];
        if (new_missi != 0) {
            open_new_mission(new_missi);
            n++;
        }
    }
    return n;
}

ushort mission_fire_fail_triggers(ushort missi)
{
    struct Mission *p_missi;
    ushort n;
    int i;

    p_missi = &mission_list[missi];

    n = 0;
    for (i = 0; i < 3; i++)
    {
        ushort new_missi;
        new_missi = p_missi->FailTrigger[i];
        if (new_missi != 0) {
            open_new_mission(new_missi);
            n++;
        }
    }
    return n;
}

void update_mission_list_to_mission_state(ushort missi, sbyte state)
{
    if (state == MResol_COMPLETED) {
        mission_list[missi].Complete = state;
    } else if (mission_remain_until_success(missi)) {
        mission_list[missi].Complete = MResol_UNDECIDED;
        set_mission_state_using_state_slot(missi, MResol_UNDECIDED);
    } else {
        mission_list[missi].Complete = state;
    }
}

/** Return the way forward in regard to opening more missions for the player.
 *
 * If the mission was either completed or failed, the function returns
 * whether more missions, and from which group, should become available to
 * the player. It also updates special triggers to acknowledge given
 * state of given mission.
 */
ubyte check_open_next_mission(ushort missi, sbyte state)
{
    if (mission_has_no_special_triggers(missi))
    {
        LOGSYNC("SpecialTriggers none, mission=%d, state=%d",
          (int)missi, (int)state);
        if (state == MResol_COMPLETED)
        {
            if (mission_has_immediate_next_on_success(missi))
                return OMiSta_ContImmSuccess;
            else
                return OMiSta_EndSuccess;
        }
        else if (state == MResol_FAILED)
        {
            return OMiSta_EndFailed;
        }
    }
    else if (mission_special_triggers_2_is_self(missi))
    {
        ushort trg_missi;

        trg_missi = mission_list[missi].SpecialTrigger[0];
        if (trg_missi == 0)
            trg_missi = mission_list[missi].SpecialTrigger[1];
        if (trg_missi != 0)
        {
            LOGSYNC("SpecialTriggers self-owned and set, mission=%d, state=%d",
              (int)missi, (int)state);
            if (state == MResol_COMPLETED)
            {
                brief_store[open_brief - 1].Mission = trg_missi;

                mission_copy_conds_and_succ_fail_triggers(trg_missi, missi);
                mission_reset_spec_triggers_2_chain(trg_missi);

                ingame.MissionStatus = ObvStatu_UNDECIDED;

                return OMiSta_ContSuccess;
            }
            else if (state == MResol_FAILED)
            {
                if (mission_remain_until_success(missi))
                {
                    ingame.MissionStatus = ObvStatu_UNDECIDED;

                    return OMiSta_ContFailed;
                }
                else
                {
                    mission_special_triggers_0_1_set_fail(trg_missi);

                    return OMiSta_EndFailed;
                }
            }
        }
        else
        {
            LOGSYNC("SpecialTriggers self-owned but unset, mission=%d, state=%d",
              (int)missi, (int)state);
            if (state == MResol_COMPLETED)
            {
                if (mission_has_immediate_next_on_success(missi))
                    return OMiSta_ContImmSuccess;
                else
                    return OMiSta_EndSuccess;
            }
            else if (state == MResol_FAILED)
            {
                if (mission_remain_until_success(missi))
                {
                    ingame.MissionStatus = ObvStatu_UNDECIDED;

                    return OMiSta_ContFailed;
                }
                else
                {
                    return OMiSta_EndFailed;
                }
            }
        }
    }
    else
    {
        ushort trg_missi;

        trg_missi = mission_list[missi].SpecialTrigger[2];

        LOGSYNC("SpecialTriggers owner=%d, mission=%d, state=%d", (int)trg_missi, (int)missi, (int)state);
        if (state == MResol_COMPLETED)
        {
            ushort tmp_missi, tmp2_missi, next_missi;

            next_missi = trg_missi;
            while (1)
            {
                tmp_missi = mission_list[next_missi].SpecialTrigger[0];
                if ((tmp_missi == missi) || (next_missi == 0))
                    break;
                next_missi = tmp_missi;
            }
            if (next_missi > 0)
            {
                tmp_missi = mission_list[next_missi].SpecialTrigger[0];
                tmp2_missi = mission_list[tmp_missi].SpecialTrigger[0];
                if (tmp2_missi > 0)
                    mission_list[next_missi].SpecialTrigger[0] = tmp2_missi;
                else
                    mission_list[next_missi].SpecialTrigger[0] = mission_list[tmp_missi].SpecialTrigger[1];
            }
            ingame.MissionStatus = ObvStatu_UNDECIDED;

            return OMiSta_ContSuccess;
        }
        else if (state == MResol_FAILED)
        {
            if (mission_remain_until_success(missi))
            {
                ingame.MissionStatus = ObvStatu_UNDECIDED;

                return OMiSta_ContFailed;
            }
            else
            {
                mission_special_triggers_0_1_set_fail(trg_missi);

                return OMiSta_EndFailed;
            }
        }
    }
    return OMiSta_NONE;
}

ubyte check_delete_open_mission(ushort missi, sbyte state)
{
    TbBool conds_met;
    ubyte misend;

    update_mission_list_to_mission_state(missi, state);

    conds_met = check_mission_conds(missi);

    if ((state == MResol_COMPLETED) || (state == MResol_FAILED))
        mission_over_give_extra_reward(missi);

    misend = check_open_next_mission(missi, state);

    LOGSYNC("Reached %s, mission=%d, current=%d, conds_met=%d",
      miss_end_sta_names[misend], (int)missi,
      (int)ingame.CurrentMission, (int)conds_met);

    switch (misend)
    {
    case OMiSta_EndSuccess:
        if (conds_met) {
            mission_fire_success_triggers(missi);
        }
        remove_mission_state_slot(missi);
        break;
    case OMiSta_ContImmSuccess:
        if (conds_met) {
            mission_fire_success_triggers(missi);
        }
        compound_mission_brief_store_next();
        break;
    case OMiSta_EndFailed:
        if (conds_met) {
            mission_fire_fail_triggers(missi);
        }
        break;
    case OMiSta_ContSuccess:
        break;
    case OMiSta_ContFailed:
        break;
    default:
        break;
    }
    return misend;
}


void game_core_mission_end_fade_start(void)
{
    struct Mission *p_missi;
    int fade_turns;

    if (ingame.MissionEndFade != 0)
        return;
    fade_turns = 0;
    p_missi = &mission_list[ingame.CurrentMission];
    if ((ingame.MissionStatus == ObvStatu_COMPLETED) &&
      ((p_missi->WaitToFade & WTFade_ON_SUCCESS) != 0))
        fade_turns = (p_missi->WaitToFade & 0x1FFF);
    if ((ingame.MissionStatus == ObvStatu_MissTmp_FAILED) &&
      ((p_missi->WaitToFade & WTFade_ON_FAIL) != 0))
        fade_turns = (p_missi->WaitToFade & 0x1FFF);
    if (fade_turns != 0) {
        LOGSYNC("Mission end fade initiated, status=%d, turns=%d",
          (int)ingame.MissionStatus, fade_turns);
        ingame.MissionEndFade = fade_turns;
    }
}

TbBool game_core_mission_end_fade_turn(void)
{
    if (ingame.MissionEndFade == 0)
        return false;
    ingame.MissionEndFade--;
    if (ingame.MissionEndFade != 0)
        return false;
    init_level_3d(1);
    if (is_single_game)
        exit_game = 1;
    return true;
}

void game_core_mission_over_start(void)
{
    // Hide weapon to cancel any effects
    mission_over_prepare_agents();

    ingame.DisplayMode = DpM_PURPLEMNU;
}

ubyte game_core_mission_over_outcome(void)
{
    ushort missi;
    short mstate;
    s32 cr_award;
    short email;
    ubyte misend;

    // Some updates take place no matter whether the mission
    // was finished with success or fail
    update_player_cash();
    mission_over_update_players();
    mission_over_gain_persuaded_crowd_rewards();
    players_sync_from_cryo();

    missi = ingame.CurrentMission;
    mstate = get_mission_state_using_state_slot(missi);

    if (mstate == MResol_UNDECIDED) {
        //TODO this is a bug - these statuses have different values (for no reason)
        mstate = ingame.MissionStatus;
        set_mission_state_using_state_slot(missi, mstate);
    }

    switch (mstate)
    {
    case MResol_COMPLETED:
        cr_award = 1000 * mission_list[missi].CashReward;
        ingame.fld_unkC57++;
        email = mission_list[missi].SuccessID;
        ingame.Credits += cr_award;
        if (email != 0)
            queue_up_new_mail(0, -email);
        break;
    case MResol_FAILED:
        email = mission_list[missi].FailID;
        ingame.fld_unkC57++;
        if (email != 0)
            queue_up_new_mail(0, -email);
        break;
    default:
        mstate = MResol_UNDECIDED;
        break;
    }
    mission_over_missi = missi;
    mission_over_state = mstate;

    misend = check_delete_open_mission(missi, mstate);

    if (misend == OMiSta_EndSuccess) {
        if (mission_is_final_at_game_end(missi))
            misend = OMiSta_CampaignDone;
    }
    return misend;
}

void game_core_mission_over_end(void)
{
    ushort missi;
    short mstate;

    missi = mission_over_missi;
    mstate = mission_over_state;
    ingame.GameOver = 0;
    if (mstate == MResol_FAILED)
    {
        if (!mission_remain_until_success(ingame.CurrentMission) &&
          !mission_has_fail_email_or_triggers(ingame.CurrentMission))
            ingame.GameOver = 1;
    }
    if (cryo_agents.NumAgents == 0)
        ingame.GameOver = 1;
    LOGSYNC("Mission %d ended with state=%d gameover=%d",
      (int)missi, (int)mstate, (int)ingame.GameOver);
}

ubyte game_core_mission_over(void)
{
    ubyte misend;

    game_core_mission_over_start();
    misend = game_core_mission_over_outcome();
    if (misend == OMiSta_ContImmSuccess)
        compound_mission_immediate_start_next();
    game_core_mission_over_end();
    return misend;
}

/******************************************************************************/
