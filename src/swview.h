/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swview.h
 *     The world of the game as a front end draws it: interface of
 *     libswarsview.
 * @par Purpose:
 *     Gives a front end that draws the game in its own way (VR, a modern
 *     3D renderer, the web) what it needs, read from the game core and the
 *     engine's world data: the static world (terrain and buildings) and the
 *     moving one as triangle lists, people and effects as sprite animation
 *     state, line effects, glows, shadows, health bars, the panels' data,
 *     the texture pages, palette and sprites.
 * @par Comment:
 *     Plain C with fixed-width types only, so the layout does not depend on
 *     pointer size or compiler; no game headers. The library only reads the
 *     game: drawing the world this way never changes the game.
 *     swview_update() fills a world block, whose layout is the SWV_OFF_*
 *     offsets; a block can be in shared memory, for a viewer in another
 *     process.
 *     All positions are floats in map units: X and Z as map coordinates
 *     (256 per tile), Y as engine height, which the renderer projects on
 *     the same scale. The axes are left-handed, as in the game: looking
 *     along +Z with Y up, +X is to the right.
 *     Add to this interface, do not change it (SWVIEW_API_VERSION).
 * @author   SyndicateVR
 * @date     06 Nov 2026 - 25 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef SWVIEW_H
#define SWVIEW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
/** Version of this interface; grows when something is added. */
#define SWVIEW_API_VERSION 1

#pragma pack(1)

#define SWV_MAGIC 0x31525653 /* "SVR1" */
#define SWV_VERSION 6

#define SWV_STATIC_VERTS_MAX 600000
#define SWV_DYN_VERTS_MAX 120000
#define SWV_THINGS_MAX 2048
#define SWV_TMAP_PAGES 18
#define SWV_TMAP_DIM 256
#define SWV_SPRITES_MAX 4096
#define SWV_SPRITE_PAGES 48
#define SWV_NSTART_MAX 4096
#define SWV_FRAMES_MAX 16384
#define SWV_ELEMENTS_MAX 32768
#define SWV_EFFECTS_MAX 4096
#define SWV_LINES_MAX 8192
#define SWV_DYN_GROUPS_MAX 4096
#define SWV_BARS_MAX 1024
#define SWV_GLOWS_MAX 1024
#define SWV_SHADOWS_MAX 512
#define SWV_AGENTS_MAX 4
#define SWV_WEAPON_TYPES 32

enum SwvVertFlags {
    SwvVF_TEXTURED = 0x01, /**< Use texture page and UV; otherwise flat palette colour */
    SwvVF_TRANSLUCENT = 0x02, /**< Drawn see-through, as the game's ghost modes */
    SwvVF_WOBBLE = 0x04,   /**< Terrain which ripples, like water */
    SwvVF_WAFT = 0x08,     /**< Terrain which bobs up and down */
    SwvVF_REFLECTIVE = 0x10, /**< Shiny, like vehicle windows */
};

/** One vertex of a triangle list; every 3 vertices make a triangle. */
struct SwvVert {
    float x, y, z;
    float u, v;     /**< Texel coordinates within the page, 0..256 */
    uint8_t page;   /**< Texture page index, below SWV_TMAP_PAGES */
    uint8_t colour; /**< Palette index for flat faces */
    uint8_t shade;  /**< Game light level times 4; 128 shows colours unchanged */
    uint8_t flags;  /**< SwvVertFlags */
};

enum SwvThingFlags {
    SwvTF_PLAYER_AGENT = 0x01,
    SwvTF_DEAD = 0x02,
    SwvTF_SELECTED = 0x04,
    SwvTF_HIDDEN = 0x08,   /**< Not drawn by the game, e.g. inside a building */
};

/** How the sprite frame of a thing is chosen. */
enum SwvAnimMode {
    SwvAM_NONE = 0,
    /** frame = nstart[anim_start + 1 + view_dir8] + subframe, where
     * view_dir8 = (angle + 8 - cam_dir8) & 7 */
    SwvAM_DIR8 = 1,
    /** frame = nstart[anim_start + 1 + view_dir12] + subframe, where
     * view_dir12 = ((3 * angle >> 1) + 12 - cam_dir12) % 12 */
    SwvAM_DIR12 = 2,
    /** frame = anim_start, the same from every direction */
    SwvAM_FIXED = 3,
};

/** A person or other non-geometry thing. */
struct SwvThing {
    float x, y, z;
    uint16_t index;
    uint8_t type;
    uint8_t subtype;
    uint8_t flags;  /**< SwvThingFlags */
    uint8_t angle;  /**< Facing, 0..7 for people */
    uint8_t group;
    uint8_t pad;
    int16_t health;
    int16_t max_health;
    uint16_t anim_start; /**< See SwvAnimMode */
    uint8_t anim_mode;   /**< SwvAnimMode */
    uint8_t subframe;
    uint8_t frv[5];      /**< Frame versions; an element is drawn when
                              frv[(Flags >> 4) & 0x1F] == ((Flags >> 9) & 7) */
    uint8_t brightness;  /**< Fade level, 32 is unchanged */
};

enum SwvEffectKind {
    SwvEK_STATIC = 0,
    SwvEK_DROPPED_ITEM,
    SwvEK_GRENADE,       /**< Grenades and mines */
    SwvEK_SCALE_EFFECT,
    SwvEK_SMOKE,         /**< Explosion smoke puffs, "phwoar" */
    SwvEK_FLAME,
    SwvEK_OTHER,
};

/** A sprite frame drawn at a point, for effects and items. All elements
 * of the frame whose Flags have bits 9-15 clear are drawn, facing the
 * viewer, as the game's draw_frame_alpha() does.
 */
struct SwvEffect {
    float x, y, z;
    uint16_t frame;
    uint16_t scale;     /**< Sprite scale, 256 is the size of person sprites */
    uint8_t brightness; /**< Fade level, 32 is unchanged */
    uint8_t kind;       /**< SwvEffectKind */
    uint16_t pad;
    uint32_t id;        /**< Stable while the effect lives, to interpolate */
};

/** Range of dynamic vertices belonging to one moving thing, so the viewer
 * can interpolate them between turns: same id and count means the same
 * vertices in the same order. */
struct SwvDynGroup {
    uint32_t id;
    uint32_t first;
    uint32_t count;
};

/** A health bar over a damaged vehicle or mounted gun. */
struct SwvBar {
    float x, y, z;
    float frac;      /**< Remaining health, 0..1 */
    uint32_t id;
};

/** A soft glow, such as headlight glare or a rocket's flame. */
struct SwvGlow {
    float x, y, z;
    float radius;    /**< Map units */
    uint8_t colour;  /**< Palette index */
    uint8_t pad[3];
    uint32_t id;
};

/** Shadow of a vehicle: a quad on the ground under it. */
struct SwvShadow {
    float x[4], z[4];
    float y;
    uint32_t id;
};

enum SwvLineFlags {
    SwvLF_JAGGED = 0x01, /**< Electricity: the viewer adds a random zigzag */
    SwvLF_GLOW = 0x02,   /**< Laser and beam: brighter core */
};

/** A line effect, such as a laser, beam, spark or wire. */
struct SwvLine {
    float x1, y1, z1;
    float x2, y2, z2;
    uint8_t colour;  /**< Palette index */
    uint8_t width;   /**< Map units */
    uint8_t flags;   /**< SwvLineFlags */
    uint8_t pad;
};

/** One of the player's agents, for the panels in the viewer. */
struct SwvAgent {
    uint16_t thing;          /**< 0 if there is no such agent */
    uint8_t current_weapon;  /**< WeaponType, 0 for none */
    uint8_t flags;           /**< SwvTF_SELECTED, SwvTF_DEAD */
    uint32_t weapons;        /**< Carried weapons, bit (WeaponType - 1) */
    int16_t health, max_health;
};

/** What the game's own panels show. */
struct SwvHud {
    struct SwvAgent agents[SWV_AGENTS_MAX];
    char objective[256];     /**< Current objective, as in the top bar */
    char weapon_names[SWV_WEAPON_TYPES][24];
};

/** Where a decoded sprite sits in the sprite pages. */
struct SwvSpriteRect {
    uint8_t page, x, y, w, h, pad;
};

/** Animation frame, as in the game's nfra-0.ani. */
struct SwvFrame {
    uint16_t first_element;
    uint8_t swidth, sheight, fx, flags;
    uint16_t next;
};

/** Frame element, as in the game's mele-0.ani: one sprite of a frame.
 * Sprite index is to_sprite / sprite_stride. X and Y are in half pixels.
 * Flags: bit 0 flip horizontally, bit 1 flip vertically, bit 2 transparent,
 * bits 4-8 version slot, bits 9-11 version value. */
struct SwvElement {
    uint16_t to_sprite;
    int16_t x, y;
    uint16_t flags;
    uint16_t next;
};

/** Header at the start of a world block.
 *
 * swview_update() leaves `seq` alone: a block shared between processes
 * uses it as a seqlock, the writer incrementing it before and after the
 * update (odd while writing).
 */
struct SwvHeader {
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

#define SWV_OFF_STATIC  4096u
#define SWV_OFF_DYN     (SWV_OFF_STATIC + SWV_STATIC_VERTS_MAX * (uint32_t)sizeof(struct SwvVert))
#define SWV_OFF_THINGS  (SWV_OFF_DYN + SWV_DYN_VERTS_MAX * (uint32_t)sizeof(struct SwvVert))
#define SWV_OFF_PALETTE (SWV_OFF_THINGS + SWV_THINGS_MAX * (uint32_t)sizeof(struct SwvThing))
#define SWV_OFF_TMAP    (SWV_OFF_PALETTE + 1024u)
#define SWV_OFF_SPRITE_RECTS (SWV_OFF_TMAP + SWV_TMAP_PAGES * SWV_TMAP_DIM * SWV_TMAP_DIM)
#define SWV_OFF_NSTART  (SWV_OFF_SPRITE_RECTS + SWV_SPRITES_MAX * (uint32_t)sizeof(struct SwvSpriteRect))
#define SWV_OFF_FRAMES  (SWV_OFF_NSTART + SWV_NSTART_MAX * 2u)
#define SWV_OFF_ELEMENTS (SWV_OFF_FRAMES + SWV_FRAMES_MAX * (uint32_t)sizeof(struct SwvFrame))
/** Sprite pages: 256x256 texels of 2 bytes, palette index then opacity. */
#define SWV_OFF_SPRITE_PAGES (SWV_OFF_ELEMENTS + SWV_ELEMENTS_MAX * (uint32_t)sizeof(struct SwvElement))
#define SWV_SPRITE_PAGE_BYTES (SWV_TMAP_DIM * SWV_TMAP_DIM * 2u)
#define SWV_OFF_EFFECTS (SWV_OFF_SPRITE_PAGES + SWV_SPRITE_PAGES * SWV_SPRITE_PAGE_BYTES)
#define SWV_OFF_LINES   (SWV_OFF_EFFECTS + SWV_EFFECTS_MAX * (uint32_t)sizeof(struct SwvEffect))
#define SWV_OFF_DYN_GROUPS (SWV_OFF_LINES + SWV_LINES_MAX * (uint32_t)sizeof(struct SwvLine))
#define SWV_OFF_BARS    (SWV_OFF_DYN_GROUPS + SWV_DYN_GROUPS_MAX * (uint32_t)sizeof(struct SwvDynGroup))
#define SWV_OFF_GLOWS   (SWV_OFF_BARS + SWV_BARS_MAX * (uint32_t)sizeof(struct SwvBar))
#define SWV_OFF_SHADOWS (SWV_OFF_GLOWS + SWV_GLOWS_MAX * (uint32_t)sizeof(struct SwvGlow))
#define SWV_OFF_HUD     (SWV_OFF_SHADOWS + SWV_SHADOWS_MAX * (uint32_t)sizeof(struct SwvShadow))
#define SWV_TOTAL_SIZE  (SWV_OFF_HUD + (uint32_t)sizeof(struct SwvHud))

/******************************************************************************/

/** Size of a world block, in bytes (SWV_TOTAL_SIZE). */
uint32_t swview_block_size(void);

/** Fills a world block with the world of the current turn. Sections which
 * rarely change (static geometry, texture pages, sprites) are written only
 * when they changed or now and then; their revision in the header tells.
 * Call it once per game turn, after the turn, on the same block.
 * Does nothing outside a mission, except clearing `in_mission`.
 */
void swview_update(void *p_block);

/** Makes the next swview_update() write every section, as for a new
 * block. */
void swview_reset(void);

/******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif
