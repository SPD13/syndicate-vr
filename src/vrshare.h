/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vrshare.h
 *     Shared memory protocol between the game and the VR spectator viewer.
 * @par Purpose:
 *     Defines the layout of the world snapshot which the game publishes
 *     every turn, and the external viewer reads.
 * @par Comment:
 *     Plain C with fixed-width types only; included both by the 32-bit game
 *     and the 64-bit viewer, so the layout must not depend on pointer size.
 *     All positions are floats in map units: X and Z as map coordinates
 *     (256 per tile), Y as engine height, which the renderer projects on
 *     the same scale. The axes are left-handed, as in the game: looking
 *     along +Z with Y up, +X is to the right.
 * @author   SyndicateVR
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef VRSHARE_H
#define VRSHARE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
#pragma pack(1)

#define VRS_MAPPING_NAME "Local\\SyndWarsFX_VRSpectator"
#define VRS_MAGIC 0x31525653 /* "SVR1" */
#define VRS_VERSION 6

#define VRS_STATIC_VERTS_MAX 600000
#define VRS_DYN_VERTS_MAX 120000
#define VRS_THINGS_MAX 2048
#define VRS_TMAP_PAGES 18
#define VRS_TMAP_DIM 256
#define VRS_SPRITES_MAX 4096
#define VRS_SPRITE_PAGES 48
#define VRS_NSTART_MAX 4096
#define VRS_FRAMES_MAX 16384
#define VRS_ELEMENTS_MAX 32768
#define VRS_EFFECTS_MAX 4096
#define VRS_LINES_MAX 8192
#define VRS_DYN_GROUPS_MAX 4096
#define VRS_BARS_MAX 1024
#define VRS_GLOWS_MAX 1024
#define VRS_SHADOWS_MAX 512
#define VRS_AGENTS_MAX 4
#define VRS_WEAPON_TYPES 32

enum VrsVertFlags {
    VrsVF_TEXTURED = 0x01, /**< Use texture page and UV; otherwise flat palette colour */
    VrsVF_TRANSLUCENT = 0x02, /**< Drawn see-through, as the game's ghost modes */
    VrsVF_WOBBLE = 0x04,   /**< Terrain which ripples, like water */
    VrsVF_WAFT = 0x08,     /**< Terrain which bobs up and down */
    VrsVF_REFLECTIVE = 0x10, /**< Shiny, like vehicle windows */
};

/** One vertex of a triangle list; every 3 vertices make a triangle. */
struct VrsVert {
    float x, y, z;
    float u, v;     /**< Texel coordinates within the page, 0..256 */
    uint8_t page;   /**< Texture page index, below VRS_TMAP_PAGES */
    uint8_t colour; /**< Palette index for flat faces */
    uint8_t shade;  /**< Game light level times 4; 128 shows colours unchanged */
    uint8_t flags;  /**< VrsVertFlags */
};

enum VrsThingFlags {
    VrsTF_PLAYER_AGENT = 0x01,
    VrsTF_DEAD = 0x02,
    VrsTF_SELECTED = 0x04,
    VrsTF_HIDDEN = 0x08,   /**< Not drawn by the game, e.g. inside a building */
};

/** How the sprite frame of a thing is chosen. */
enum VrsAnimMode {
    VrsAM_NONE = 0,
    /** frame = nstart[anim_start + 1 + view_dir8] + subframe, where
     * view_dir8 = (angle + 8 - cam_dir8) & 7 */
    VrsAM_DIR8 = 1,
    /** frame = nstart[anim_start + 1 + view_dir12] + subframe, where
     * view_dir12 = ((3 * angle >> 1) + 12 - cam_dir12) % 12 */
    VrsAM_DIR12 = 2,
    /** frame = anim_start, the same from every direction */
    VrsAM_FIXED = 3,
};

/** A person or other non-geometry thing. */
struct VrsThing {
    float x, y, z;
    uint16_t index;
    uint8_t type;
    uint8_t subtype;
    uint8_t flags;  /**< VrsThingFlags */
    uint8_t angle;  /**< Facing, 0..7 for people */
    uint8_t group;
    uint8_t pad;
    int16_t health;
    int16_t max_health;
    uint16_t anim_start; /**< See VrsAnimMode */
    uint8_t anim_mode;   /**< VrsAnimMode */
    uint8_t subframe;
    uint8_t frv[5];      /**< Frame versions; an element is drawn when
                              frv[(Flags >> 4) & 0x1F] == ((Flags >> 9) & 7) */
    uint8_t brightness;  /**< Fade level, 32 is unchanged */
};

enum VrsEffectKind {
    VrsEK_STATIC = 0,
    VrsEK_DROPPED_ITEM,
    VrsEK_GRENADE,       /**< Grenades and mines */
    VrsEK_SCALE_EFFECT,
    VrsEK_SMOKE,         /**< Explosion smoke puffs, "phwoar" */
    VrsEK_FLAME,
    VrsEK_OTHER,
};

/** A sprite frame drawn at a point, for effects and items. All elements
 * of the frame whose Flags have bits 9-15 clear are drawn, facing the
 * viewer, as the game's draw_frame_alpha() does.
 */
struct VrsEffect {
    float x, y, z;
    uint16_t frame;
    uint16_t scale;     /**< Sprite scale, 256 is the size of person sprites */
    uint8_t brightness; /**< Fade level, 32 is unchanged */
    uint8_t kind;       /**< VrsEffectKind */
    uint16_t pad;
    uint32_t id;        /**< Stable while the effect lives, to interpolate */
};

/** Range of dynamic vertices belonging to one moving thing, so the viewer
 * can interpolate them between turns: same id and count means the same
 * vertices in the same order. */
struct VrsDynGroup {
    uint32_t id;
    uint32_t first;
    uint32_t count;
};

/** A health bar over a damaged vehicle or mounted gun. */
struct VrsBar {
    float x, y, z;
    float frac;      /**< Remaining health, 0..1 */
    uint32_t id;
};

/** A soft glow, such as headlight glare or a rocket's flame. */
struct VrsGlow {
    float x, y, z;
    float radius;    /**< Map units */
    uint8_t colour;  /**< Palette index */
    uint8_t pad[3];
    uint32_t id;
};

/** Shadow of a vehicle: a quad on the ground under it. */
struct VrsShadow {
    float x[4], z[4];
    float y;
    uint32_t id;
};

enum VrsLineFlags {
    VrsLF_JAGGED = 0x01, /**< Electricity: the viewer adds a random zigzag */
    VrsLF_GLOW = 0x02,   /**< Laser and beam: brighter core */
};

/** A line effect, such as a laser, beam, spark or wire. */
struct VrsLine {
    float x1, y1, z1;
    float x2, y2, z2;
    uint8_t colour;  /**< Palette index */
    uint8_t width;   /**< Map units */
    uint8_t flags;   /**< VrsLineFlags */
    uint8_t pad;
};

/** One of the player's agents, for the panels in the viewer. */
struct VrsAgent {
    uint16_t thing;          /**< 0 if there is no such agent */
    uint8_t current_weapon;  /**< WeaponType, 0 for none */
    uint8_t flags;           /**< VrsTF_SELECTED, VrsTF_DEAD */
    uint32_t weapons;        /**< Carried weapons, bit (WeaponType - 1) */
    int16_t health, max_health;
};

/** What the game's own panels show. */
struct VrsHud {
    struct VrsAgent agents[VRS_AGENTS_MAX];
    char objective[256];     /**< Current objective, as in the top bar */
    char weapon_names[VRS_WEAPON_TYPES][24];
};

/** Where a decoded sprite sits in the sprite pages. */
struct VrsSpriteRect {
    uint8_t page, x, y, w, h, pad;
};

/** Animation frame, as in the game's nfra-0.ani. */
struct VrsFrame {
    uint16_t first_element;
    uint8_t swidth, sheight, fx, flags;
    uint16_t next;
};

/** Frame element, as in the game's mele-0.ani: one sprite of a frame.
 * Sprite index is to_sprite / sprite_stride. X and Y are in half pixels.
 * Flags: bit 0 flip horizontally, bit 1 flip vertically, bit 2 transparent,
 * bits 4-8 version slot, bits 9-11 version value. */
struct VrsElement {
    uint16_t to_sprite;
    int16_t x, y;
    uint16_t flags;
    uint16_t next;
};

/** Header at the start of the shared memory block.
 *
 * The writer increments `seq` before and after updating the block, so it is
 * odd while writing; a reader copies what it needs and retries if `seq`
 * was odd or changed meanwhile.
 */
struct VrsHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t total_size;
    volatile uint32_t seq;
    uint32_t gameturn;
    uint32_t map_no;
    uint32_t static_rev;  /**< Changes when static_verts content changes */
    uint32_t tmap_rev;    /**< Changes when texture pages or palette change */
    uint32_t n_static;
    uint32_t n_dyn;
    uint32_t n_things;
    uint32_t in_mission;  /**< Non-zero while a level is being played */
    float cam_x, cam_y, cam_z; /**< Point the game camera looks at */
    float cam_angle;      /**< Game camera yaw in radians */
    uint32_t off_static;  /**< Byte offsets of the arrays from block start */
    uint32_t off_dyn;
    uint32_t off_things;
    uint32_t off_tmap;
    uint32_t off_palette;
    uint32_t sprite_rev;  /**< Changes when sprites or animation tables change */
    uint32_t n_sprites;
    uint32_t n_sprite_pages;
    uint32_t sprite_stride; /**< sizeof(TbSprite) in the game */
    uint32_t n_nstart, n_frames, n_elements;
    uint32_t n_effects;
    uint32_t n_lines;
    uint32_t n_dyn_groups, n_bars, n_glows, n_shadows;
    uint32_t selected;    /**< Thing the player controls directly */
    uint32_t view_scale;  /**< Game zoom, overall_scale; 256 is one pixel per 8 units */
    uint32_t view_size;   /**< Game view size in pixels, width << 16 | height */
};

#define VRS_OFF_STATIC  4096u
#define VRS_OFF_DYN     (VRS_OFF_STATIC + VRS_STATIC_VERTS_MAX * (uint32_t)sizeof(struct VrsVert))
#define VRS_OFF_THINGS  (VRS_OFF_DYN + VRS_DYN_VERTS_MAX * (uint32_t)sizeof(struct VrsVert))
#define VRS_OFF_PALETTE (VRS_OFF_THINGS + VRS_THINGS_MAX * (uint32_t)sizeof(struct VrsThing))
#define VRS_OFF_TMAP    (VRS_OFF_PALETTE + 1024u)
#define VRS_OFF_SPRITE_RECTS (VRS_OFF_TMAP + VRS_TMAP_PAGES * VRS_TMAP_DIM * VRS_TMAP_DIM)
#define VRS_OFF_NSTART  (VRS_OFF_SPRITE_RECTS + VRS_SPRITES_MAX * (uint32_t)sizeof(struct VrsSpriteRect))
#define VRS_OFF_FRAMES  (VRS_OFF_NSTART + VRS_NSTART_MAX * 2u)
#define VRS_OFF_ELEMENTS (VRS_OFF_FRAMES + VRS_FRAMES_MAX * (uint32_t)sizeof(struct VrsFrame))
/** Sprite pages: 256x256 texels of 2 bytes, palette index then opacity. */
#define VRS_OFF_SPRITE_PAGES (VRS_OFF_ELEMENTS + VRS_ELEMENTS_MAX * (uint32_t)sizeof(struct VrsElement))
#define VRS_SPRITE_PAGE_BYTES (VRS_TMAP_DIM * VRS_TMAP_DIM * 2u)
#define VRS_OFF_EFFECTS (VRS_OFF_SPRITE_PAGES + VRS_SPRITE_PAGES * VRS_SPRITE_PAGE_BYTES)
#define VRS_OFF_LINES   (VRS_OFF_EFFECTS + VRS_EFFECTS_MAX * (uint32_t)sizeof(struct VrsEffect))
#define VRS_OFF_DYN_GROUPS (VRS_OFF_LINES + VRS_LINES_MAX * (uint32_t)sizeof(struct VrsLine))
#define VRS_OFF_BARS    (VRS_OFF_DYN_GROUPS + VRS_DYN_GROUPS_MAX * (uint32_t)sizeof(struct VrsDynGroup))
#define VRS_OFF_GLOWS   (VRS_OFF_BARS + VRS_BARS_MAX * (uint32_t)sizeof(struct VrsBar))
#define VRS_OFF_SHADOWS (VRS_OFF_GLOWS + VRS_GLOWS_MAX * (uint32_t)sizeof(struct VrsGlow))
#define VRS_OFF_HUD     (VRS_OFF_SHADOWS + VRS_SHADOWS_MAX * (uint32_t)sizeof(struct VrsShadow))
#define VRS_TOTAL_SIZE  (VRS_OFF_HUD + (uint32_t)sizeof(struct VrsHud))

/******************************************************************************/
/* Commands from the viewer to the game, in a second, writable block. */

#define VRS_CMD_MAPPING_NAME "Local\\SyndWarsFX_VRCommands"
#define VRS_CMD_MAGIC 0x31435653 /* "SVC1" */
#define VRS_CMDS_MAX 64

enum VrsCmdType {
    VrsCmd_NONE = 0,
    VrsCmd_SELECT_AGENT,  /**< thing: one of the player's agents */
    VrsCmd_GOTO,          /**< x, z: map point to walk to */
    VrsCmd_SHOOT_THING,   /**< thing: person or vehicle to shoot at */
    VrsCmd_SHOOT_GROUND,  /**< x, z: map point to shoot at */
    VrsCmd_WEAPON_NEXT,
    VrsCmd_WEAPON_PREV,
    VrsCmd_WEAPON_SELECT, /**< arg: WeaponType */
    VrsCmd_ENTER_VEHICLE, /**< thing: vehicle */
};

struct VrsCmd {
    uint16_t type;   /**< VrsCmdType */
    int16_t thing;
    int32_t x, z;
    uint32_t arg;
};

/** Ring of commands: the viewer writes at `write` and then increments it;
 * the game executes the entry at `read`, one per turn, and increments it. */
struct VrsCmdQueue {
    uint32_t magic;
    volatile uint32_t write;
    volatile uint32_t read;
    uint32_t pad;
    struct VrsCmd cmds[VRS_CMDS_MAX];
};

#pragma pack()
/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
