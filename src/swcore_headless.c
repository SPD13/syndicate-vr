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
 *     "--world" also prints a summary of the world every 100 turns.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv)
{
    uint32_t turns = 0, done = 0, fixes = 0;
    int world = 0, no_draw = 0, set_fixes = 0, core_turn = 0, status = 0;
    uint32_t last_status = 0;

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
        } else if (strcmp(argv[1], "--world") == 0) {
            world = 1;
            argv[1] = argv[0];
            argv++;
            argc--;
        } else {
            break;
        }
    }
    if (swcore_init(argc, argv) != 0)
        return 1;
    if (no_draw)
        swcore_set_world_drawing(0);
    if (set_fixes)
        swcore_set_fixes(fixes);
    while (core_turn ? swcore_core_step() : swcore_step())
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
        fflush(stdout);
        done++;
        if ((turns != 0) && (done >= turns))
            break;
    }
    swcore_shutdown();
    return 0;
}
/******************************************************************************/
