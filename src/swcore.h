/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swcore.h
 *     The game as a library (libswarscore), driven by a host program.
 * @par Purpose:
 *     The interface front ends (the SDL executable, a VR viewer, a mobile
 *     app) use to run the game: initialise it, advance it one turn at a
 *     time, read its state, and shut it down. Depends on nothing but the
 *     C standard headers, so that C++ and other languages can include it.
 * @par Comment:
 *     The game keeps its state in globals: one game per process, and
 *     swcore_init() may be called only once.
 * @author   SyndicateVR decomp agent
 * @date     23 Nov 2026 - 23 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SWCORE_H
#define SWCORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/

/** Version of this interface; grows when functions are added. */
#define SWCORE_API_VERSION 2

/** Sets the game up as the executable would: reads the command line options
 * (the same as syndwarsfx, e.g. "-m 1,2" to start a mission), the config
 * and the game data, opens the display and sound, plays the intro.
 * Returns 0 on success, nonzero on failure.
 */
int swcore_init(int argc, char **argv);

/** Runs one game turn: input, world update, drawing, sound, and the wait
 * for the turn's time. Returns 1 while the game runs, 0 once it asked to
 * exit (then call swcore_shutdown()).
 */
int swcore_step(void);

/** Runs one game turn of a mission with the game core only: the world,
 * the camera and the commands of swcore_command(), without reading input,
 * drawing, sound or the wait for the turn's time. Plays as swcore_step()
 * with the world not drawn and no input (with the default fixes) until the
 * mission is decided; the mission's end (end screen, fade, leaving the
 * mission) is not played yet. Returns 1 while a mission is played, 0 once
 * the game asked to exit or no mission is played. Since version 2.
 */
int swcore_core_step(void);

/** Frees what the game set up. The process may continue afterwards, but
 * the game cannot be initialised again.
 */
void swcore_shutdown(void);

/** Switches drawing the world in missions on (the default) or off. Off,
 * the game still lists what is in view (which in the original also
 * changes game state, e.g. line of sight through vehicles near the map
 * edge, and finds the thing under the pointer), but draws neither the
 * world nor explosions; menus and the panels are still drawn. The game
 * plays the same either way; drawing changes only its own counters, which
 * swcore_state_hash() leaves out. A front end that draws the world from
 * swcore_things() switches it off.
 */
void swcore_set_world_drawing(int enabled);

/** Fixes of original bugs that change how the game plays. They are on by
 * default (SWCORE_FIXES_DEFAULT); swcore_set_fixes(0), or the command line
 * option --original-bugs, plays exactly as the original (needed to compare
 * with it); --fix-bugs switches them back on.
 */
enum SwcFixes {
    /** Line of sight reads the flags of the right triangle face; the
     * original reads memory drawing writes, so that what was drawn changes
     * what people see. Needed to play the same with any renderer. */
    SWCORE_FIX_LOS_FACE_READ = 0x01,
};

/** The fixes a game starts with. */
#define SWCORE_FIXES_DEFAULT SWCORE_FIX_LOS_FACE_READ

/** Sets which fixes of original bugs are on, SwcFixes bits; bits not set
 * are off. Can be called before swcore_init() or between turns. */
void swcore_set_fixes(uint32_t fixes);

/** Fixes of original bugs that are on, SwcFixes bits. */
uint32_t swcore_get_fixes(void);

/** Number of the current game turn. */
uint32_t swcore_game_turn(void);

/** Hash of the world state (all things), equal on every target for the
 * same game; the state traces compare builds on it.
 */
uint32_t swcore_state_hash(void);

/******************************************************************************/
/* Reading the world. Positions are the game's fine coordinates: X and Z
 * across the map, x >> 8 and z >> 8 being map coordinates (256 per tile),
 * and Y the height. Angles are 0..2047 for a full turn. */

#define SWCORE_AGENTS_MAX 4

enum SwcMissionStatus {
    SwcMS_NONE = 0,     /**< Not in a mission (menus) */
    SwcMS_PLAYING,
    SwcMS_WON,
    SwcMS_LOST,
};

struct SwcWorldInfo {
    uint32_t gameturn;
    uint32_t in_mission;     /**< Nonzero while a mission is played */
    uint32_t mission_status; /**< SwcMissionStatus */
    uint32_t map_no;
    int32_t level_no;
    uint32_t selected;       /**< Thing the player controls directly, 0 if none */
    int32_t cam_x, cam_z;    /**< Point the game camera looks at */
    int32_t cam_angle;       /**< Camera yaw, 0..2047 */
    const char *objective;   /**< Current objective as in the top bar, or NULL;
                                  valid until the next swcore_step() */
};

enum SwcThingType {
    SwcTT_OTHER = 0,
    SwcTT_PERSON,
    SwcTT_VEHICLE,
    SwcTT_BUILDING,
    SwcTT_ITEM,     /**< Weapon or other item lying on the ground */
    SwcTT_EFFECT,   /**< Shot, rocket, grenade, explosion part */
};

enum SwcThingFlags {
    SwcTF_PLAYER_AGENT = 0x01,
    SwcTF_DEAD = 0x02,
    SwcTF_SELECTED = 0x04,
};

struct SwcThing {
    uint16_t index;      /**< Thing number, used in commands */
    uint8_t kind;        /**< SwcThingType */
    uint8_t flags;       /**< SwcThingFlags */
    uint8_t game_type;   /**< The game's own Type and SubType */
    uint8_t game_subtype;
    uint8_t group;       /**< Group of people and vehicles, 0 otherwise */
    uint8_t pad;
    int32_t x, y, z;
    int16_t angle;       /**< Facing of people and vehicles, 0..2047 */
    int16_t health;
    int16_t max_health;
    int16_t state;       /**< The game's State */
};

struct SwcAgent {
    uint16_t thing;          /**< 0 if there is no such agent */
    uint8_t current_weapon;  /**< Weapon type, 0 for none */
    uint8_t flags;           /**< SwcTF_SELECTED, SwcTF_DEAD */
    uint32_t weapons;        /**< Carried weapons, bit (weapon type - 1) */
    int16_t health, max_health;
};

/** Fills the general state of the game. */
void swcore_world_info(struct SwcWorldInfo *p_info);

/** Copies up to `max` of the things that exist (people, vehicles,
 * buildings, items, effects) to `p_things`; returns how many exist, which
 * may be more than `max`.
 */
uint32_t swcore_things(struct SwcThing *p_things, uint32_t max);

/** Fills the player's agents, SWCORE_AGENTS_MAX entries. */
void swcore_agents(struct SwcAgent *p_agents);

/** Short name of a weapon type, "" if there is none. */
const char *swcore_weapon_name(uint32_t weapon_type);

/******************************************************************************/
/* Player commands, as mouse clicks would give them to the agent the player
 * controls. They are queued and executed one per turn, in the turns the
 * player gives no input of their own, while a mission is played. */

enum SwcCommandType {
    SwcCmd_NONE = 0,
    SwcCmd_SELECT_AGENT,  /**< thing: one of the player's agents */
    SwcCmd_GOTO,          /**< x, z: map coordinates to walk to */
    SwcCmd_SHOOT_THING,   /**< thing: person or vehicle to shoot at */
    SwcCmd_SHOOT_GROUND,  /**< x, z: map coordinates to shoot at */
    SwcCmd_WEAPON_NEXT,
    SwcCmd_WEAPON_PREV,
    SwcCmd_WEAPON_SELECT, /**< arg: weapon type */
    SwcCmd_ENTER_VEHICLE, /**< thing: vehicle */
};

struct SwcCommand {
    uint16_t type;   /**< SwcCommandType */
    int16_t thing;
    int32_t x, z;    /**< Map coordinates (not the 256 times finer positions) */
    uint32_t arg;
};

#define SWCORE_COMMANDS_MAX 64

/** Queues a command; returns 0, or nonzero if the queue is full. */
int swcore_command(const struct SwcCommand *p_cmd);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
/******************************************************************************/
