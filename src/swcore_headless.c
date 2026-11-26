/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swcore_headless.c
 *     A minimal front end of the game library, for tests.
 * @par Purpose:
 *     Runs the game through swcore.h only: takes the syndwarsfx options
 *     (e.g. "-q -W -m 1,2"), steps the game, reads the world every turn
 *     through the library interface and prints "<turn> <state hash>" per
 *     turn, which must equal the first two columns of the state trace of
 *     the same run of syndwarsfx.
 * @par Comment:
 *     Options of its own, before the game's: "--turns N" stops after N
 *     turns; "--no-draw" leaves out drawing the world in missions;
 *     "--fixes N" sets which fixes of original bugs are on (SwcFixes bits;
 *         by default SWCORE_FIXES_DEFAULT, or as the game options set);
 *     "--core-turn" steps with swcore_core_step() (the game core only,
 *         no input and no drawing) instead of swcore_step();
 *     "--status" also prints "# turn N status S" when the mission status
 *         changes;
 *     "--world" also prints a summary of the world every 100 turns;
 *     "--view" also exports the world every turn as a front end that draws
 *         it its own way would (swview.h; with the core alone it loads the
 *         assets for it too); the export only reads the game, so the
 *         hashes must not change;
 *     "--view-dump TURN DIR" (with --view) writes the export of that turn
 *         to DIR: block.bin (the world block), persons.bin, fx.bin, hud.bin,
 *         props.bin.
 *     Built with SWCORE_CORE_ONLY (swcore_coreonly), it runs the game core
 *     alone: swcore_core_init() with the mission of "-m campaign,mission",
 *     and always the core turn; of the game's options it reads only that
 *     and "--original-bugs"/"--fix-bugs"; "--conclude N" confirms the end
 *     of a decided mission N turns after the decision (as Enter on the end
 *     message), and the last line "# end turn N mission S" gives the
 *     mission's state at the end (SwcMissionStatus, 0 once left). It
 *     plays with the test hooks of the core (libswarstest: the scripted
 *     player and the scenario hooks of statetrace.sh), without those of
 *     front-end code (m_cryo, m_typing, m_pause).
 *     Use SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy for no window.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swcore.h"
#include "swview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if SWCORE_CORE_ONLY
/* The test hooks of the game turn (game_turn.h, difftest.h); a test front
 * end sets them, as swapp.c does for the desktop. */
extern void (*game_turn_test_hook)(void);
void difftest_bot_turn(void);
#endif

#define THINGS_BUF 2000

static struct SwcThing things_buf[THINGS_BUF];

static void print_world(void)
{
    struct SwcWorldInfo info;
    struct SwcAgent agents[SWCORE_AGENTS_MAX];
    uint32_t n, i, kinds[6] = {0};

    swcore_world_info(&info);
    n = swcore_things(things_buf, THINGS_BUF);
    for (i = 0; i < n && i < THINGS_BUF; i++)
        if (things_buf[i].kind < 6)
            kinds[things_buf[i].kind]++;
    swcore_agents(agents);
    printf("# turn %u mission %u status %u map %u things %u (people %u vehicles %u"
      " buildings %u effects %u) selected %u\n", info.gameturn, info.in_mission,
      info.mission_status, info.map_no, n, kinds[SwcTT_PERSON], kinds[SwcTT_VEHICLE],
      kinds[SwcTT_BUILDING], kinds[SwcTT_EFFECT], info.selected);
    for (i = 0; i < SWCORE_AGENTS_MAX; i++)
    {
        if (agents[i].thing == 0)
            continue;
        printf("#   agent %u health %d/%d weapon %s%s\n", agents[i].thing,
          agents[i].health, agents[i].max_health,
          swcore_weapon_name(agents[i].current_weapon),
          (agents[i].flags & SwcTF_DEAD) ? " dead" : "");
    }
}

static void dump_file(const char *dir, const char *name, const void *p_data, size_t len)
{
    char path[512];
    FILE *fh;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    fh = fopen(path, "wb");
    if (fh == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fwrite(p_data, 1, len, fh);
    fclose(fh);
}

/** Effect events seen, per SwvFxKind. */
static uint32_t view_fx_count[8];
static uint32_t view_charge_turns, view_charge_max;

static void print_view_fx(void)
{
    printf("# view fx events: %u bangs, %u fires started, %u ended, %u scale effects, %u smoke\n",
      view_fx_count[1], view_fx_count[2], view_fx_count[3], view_fx_count[4], view_fx_count[5]);
    printf("# view charges: %u turns, highest level %u\n", view_charge_turns, view_charge_max);
}

/** Exports the world as a front end drawing it would, every turn. */
static void view_turn(uint32_t turn, uint32_t dump_turn, const char *dump_dir)
{
    static void *p_block = NULL;
    static struct SwvPersonRender persons[2048];
    static struct SwvFxEvent fx[SWV_FX_EVENTS_MAX];
    static struct SwvHudInfo hud;
    static struct SwvProp props[4096];
    static struct SwvCharge charges[8];
    uint32_t n_persons, n_fx, n_props, n_charges;

    if (p_block == NULL) {
        p_block = calloc(1, swview_block_size());
        if (p_block == NULL)
            return;
    }
    swview_update(p_block);
    n_persons = swview_persons_render(persons, 2048);
    n_fx = swview_fx_events(fx, SWV_FX_EVENTS_MAX);
    swview_hud_info(&hud);
    n_props = swview_props(props, 4096);
    n_charges = swview_charges(charges, 8);
    if (n_charges > 0) {
        uint32_t i;
        view_charge_turns++;
        for (i = 0; i < n_charges; i++)
            if (charges[i].level > view_charge_max)
                view_charge_max = charges[i].level;
    }
    {
        uint32_t i;
        for (i = 0; i < n_fx; i++) {
            if (fx[i].kind < 8)
                view_fx_count[fx[i].kind]++;
        }
    }
    if ((dump_dir != NULL) && (turn == dump_turn)) {
        dump_file(dump_dir, "block.bin", p_block, swview_block_size());
        dump_file(dump_dir, "persons.bin", persons, n_persons * sizeof(persons[0]));
        dump_file(dump_dir, "fx.bin", fx, n_fx * sizeof(fx[0]));
        dump_file(dump_dir, "hud.bin", &hud, sizeof(hud));
        dump_file(dump_dir, "props.bin", props, n_props * sizeof(props[0]));
        printf("# view dump turn %u: %u persons, %u fx events, %u props, %u charging\n", turn, n_persons,
          n_fx, n_props, n_charges);
    }
}

int main(int argc, char **argv)
{
    uint32_t turns = 0, done = 0, fixes = 0;
    int world = 0, no_draw = 0, set_fixes = 0, core_turn = 0, status = 0;
    int view = 0;
    uint32_t view_dump_turn = 0;
    const char *view_dump_dir = NULL;
    uint32_t last_status = 0;
#if SWCORE_CORE_ONLY
    int conclude_after = -1, decided_turns = 0;
#endif

    while (argc > 1)
    {
        if ((strcmp(argv[1], "--turns") == 0) && (argc > 2)) {
            turns = (uint32_t)strtoul(argv[2], NULL, 0);
            argv[2] = argv[0];
            argv += 2;
            argc -= 2;
        } else if (strcmp(argv[1], "--no-draw") == 0) {
            no_draw = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else if ((strcmp(argv[1], "--fixes") == 0) && (argc > 2)) {
            fixes = (uint32_t)strtoul(argv[2], NULL, 0);
            set_fixes = 1;
            argv[2] = argv[0];
            argv += 2;
            argc -= 2;
        } else if (strcmp(argv[1], "--core-turn") == 0) {
            core_turn = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else if (strcmp(argv[1], "--status") == 0) {
            status = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else if (strcmp(argv[1], "--view") == 0) {
            view = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else if ((strcmp(argv[1], "--view-dump") == 0) && (argc > 3)) {
            view_dump_turn = (uint32_t)strtoul(argv[2], NULL, 0);
            view_dump_dir = argv[3];
            argv[3] = argv[0];
            argv += 3;
            argc -= 3;
        } else if (strcmp(argv[1], "--world") == 0) {
            world = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else {
            break;
        }
    }
#if SWCORE_CORE_ONLY
    // the game core alone: the game's options that matter here are
    // "-m campaign,mission" and "--original-bugs"/"--fix-bugs"
    {
        unsigned campaign = 0, mission = 0;
        int i, have_mission = 0;

        for (i = 1; i < argc; i++)
        {
            if ((strcmp(argv[i], "-m") == 0) && (i + 1 < argc)) {
                have_mission = (sscanf(argv[i + 1], "%u,%u", &campaign, &mission) == 2);
                i++;
            } else if (strcmp(argv[i], "--original-bugs") == 0) {
                if (!set_fixes)
                    swcore_set_fixes(0);
            } else if (strcmp(argv[i], "--fix-bugs") == 0) {
                if (!set_fixes)
                    swcore_set_fixes(SWCORE_FIXES_DEFAULT);
            } else if ((strcmp(argv[i], "--conclude") == 0) && (i + 1 < argc)) {
                conclude_after = atoi(argv[i + 1]);
                i++;
            }
        }
        if (!have_mission) {
            fprintf(stderr, "%s: needs \"-m campaign,mission\"\n", argv[0]);
            return 2;
        }
        if (set_fixes)
            swcore_set_fixes(fixes);
        game_turn_test_hook = difftest_bot_turn;
        if (swcore_core_init(campaign, mission) != 0)
            return 1;
        if (view)
            swview_load_assets();
        core_turn = 1;
        (void)no_draw;
    }
#else
    if (swcore_init(argc, argv) != 0)
        return 1;
    if (no_draw)
        swcore_set_world_drawing(0);
    if (set_fixes)
        swcore_set_fixes(fixes);
#endif
#if SWCORE_CORE_ONLY
    while (swcore_core_step())
#else
    while (core_turn ? swcore_core_step() : swcore_step())
#endif
    {
        struct SwcWorldInfo info;

        // read through the interface every turn, as a front end would
        swcore_world_info(&info);
        swcore_things(things_buf, THINGS_BUF);
        printf("%u %08x\n", swcore_game_turn(), swcore_state_hash());
        if (status && (info.mission_status != last_status))
            printf("# turn %u status %u\n", info.gameturn, info.mission_status);
        last_status = info.mission_status;
        if (world && (info.gameturn % 100 == 0))
            print_world();
        if (view)
            view_turn(info.gameturn, view_dump_turn, view_dump_dir);
        fflush(stdout);
        done++;
        if ((turns != 0) && (done >= turns))
            break;
#if SWCORE_CORE_ONLY
        // the player confirms the end message some turns after the decision
        if (info.mission_status != SwcMS_WON && info.mission_status != SwcMS_LOST)
            decided_turns = 0;
        else if ((conclude_after >= 0) && (decided_turns++ == conclude_after))
            swcore_mission_conclude();
#endif
    }
#if SWCORE_CORE_ONLY
    {
        struct SwcWorldInfo info;

        swcore_world_info(&info);
        printf("# end turn %u mission %u\n", info.gameturn, info.mission_status);
        if (view)
            print_view_fx();
        fflush(stdout);
    }
    swcore_core_shutdown();
#else
    swcore_shutdown();
#endif
    return 0;
}
/******************************************************************************/
