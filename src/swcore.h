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
#define SWCORE_API_VERSION 6

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
 * with the world not drawn and no input (with the default fixes). Once the
 * mission is decided (mission_status of swcore_world_info()), it ends after its fade-out
 * or when the front end calls swcore_mission_conclude(), with the
 * campaign's outcome (rewards, missions opened). Returns 1 while a mission
 * is played, 0 once it is over, the game asked to exit or no mission is
 * played. Since version 2; the mission's end since version 4.
 */
int swcore_core_step(void);

/** Frees what the game set up. The process may continue afterwards, but
 * the game cannot be initialised again.
 */
void swcore_shutdown(void);

/** Sets up the game core alone, without display, input, sound or menus, and
 * starts a mission, as the option "-m campaign,mission" of syndwarsfx
 * does: `campaign` is the campaign's index, `mission` the mission's index.
 * The game data must be in the current directory; the default user
 * settings are used. Then play the mission with swcore_core_step(), and
 * end with swcore_core_shutdown(). Instead of swcore_init(), for front ends
 * that draw the world themselves; the program links the game core and a
 * front end's services (swfe.h), not the desktop front end. Returns 0 on
 * success, nonzero on failure. Since version 3.
 */
int swcore_core_init(uint32_t campaign, uint32_t mission);

/** Frees what swcore_core_init() set up. Since version 3. */
void swcore_core_shutdown(void);

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

/** The player confirmed the conclusion of a decided mission (the desktop's
 * Enter or Esc on the end message): the next swcore_core_step() ends it.
 * Does nothing while the mission is not decided. Since version 4.
 */
void swcore_mission_conclude(void);

/** Moves the game camera to look at map point x, z, as the desktop's
 * scrolling does: it glides there over the next turns (while it follows
 * no thing). For a second view of the game, such as a VR table, to show
 * the desktop the same place. Call between turns. Since version 5.
 */
void swcore_camera_look_at(int32_t x, int32_t z);

/** Turns the game camera to an angle, 0 to 2047 for a full turn (as
 * SwcWorldInfo.cam_angle), at once and without spin. Call between turns.
 * Since version 5.
 */
void swcore_camera_set_angle(int32_t angle);

/** Sets the zoom the player chose (the desktop's zoom keys), kept within
 * the game's range (120 far to 256 close); the game still zooms no further
 * out than the selected agent's weapon allows (swview.h view_zoom_far).
 * Since version 5.
 */
void swcore_camera_set_zoom(int32_t zoom);

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
 * player gives no input of their own, while a mission is played.
 * A shot holds the trigger for its turn only: to keep it held (continuous
 * fire, or charging a laser, electron mace, graviton gun or devastator),
 * give the shot again before every turn, as the desktop does while its
 * mouse button is down; the first turn without one releases it, and a
 * charged weapon fires then. */

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
    /* since version 6: the desktop's agent panels; thing: one of the
     * player's agents (the selected one for the last four) */
    SwcCmd_AGENT_WEAPON,  /**< arg: weapon type; that agent draws or puts
                               away the weapon (its panel's weapon list) */
    SwcCmd_GROUP_WEAPON,  /**< arg: weapon type; the whole group too */
    SwcCmd_DROP_WEAPON,   /**< arg: weapon type; that agent drops it */
    SwcCmd_SET_MOOD,      /**< x: mood, -88..88 */
    SwcCmd_GROUP_SET_MOOD,/**< x: mood, -88..88, for the whole group */
    SwcCmd_USE_MEDIKIT,
    SwcCmd_SHIELD_TOGGLE, /**< supershield on/off */
    SwcCmd_PROTECT_INC,   /**< one more agent protects the selected one */
    SwcCmd_PROTECT_TOGGLE,/**< all agents protect it, or none */
    SwcCmd_THERMAL_TOGGLE,
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
