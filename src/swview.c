/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file swview.c
 *     The world of the game as a front end draws it (swview.h).
 * @par Purpose:
 *     Every game turn, converts the terrain, building and vehicle geometry
 *     into triangle lists, and the people, effects, line effects and the
 *     panels' data into plain records, in a world block (see swview.h for
 *     the layout), for a front end which draws the game its own way.
 * @par Comment:
 *     Only reads the game. Was the VR spectator export (vrspectate.c).
 * @author   SyndicateVR
 * @date     06 Nov 2026 - 25 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "swview.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bffile.h"
#include "bfmath.h"
#include "bfmemory.h"
#include "bfpalette.h"
#include "bfsprite.h"
#include "poly.h"
#include "engincam.h"
#include "engincolour.h"
#include "enginfexpl.h"
#include "enginlights.h"
#include "enginshrapn.h"
#include "engintrns.h"
#include "enginprops.h"
#include "enginsngobjs.h"
#include "enginsngtxtr.h"
#include "engintxtrmap.h"
#include "enginzoom.h"
#include "frame_sprani.h"

#include "bigmap.h"
#include "bmbang.h"
#include "building.h"
#include "engindrwlstm_wrp.h"
#include "game.h"
#include "game_options.h"
#include "game_speed.h"
#include "lvobjctv.h"
#include "packet.h"
#include "matrix.h"
#include "people.h"
#include "player.h"
#include "thing.h"
#include "thing_fire.h"
#include "tngobjdrw.h"
#include "vehicle.h"
#include "weapon.h"
#include "game_camera.h"
#include "scanner.h"
#include "swcore.h"
#include "swlog.h"
/******************************************************************************/

/** How often static geometry (terrain and buildings) is re-sent, in turns. */
#define VR_STATIC_PERIOD 16

static ubyte *swv_block;
static uint swv_static_turn;
static uint swv_static_rev;
static uint swv_tmap_rev;
static ushort swv_last_map;
static uint swv_sprite_rev;
static u32 swv_world_hash;

/** Lists filled each turn besides the triangle meshes. */
struct VrLists {
    struct SwvDynGroup *p_groups;
    uint n_groups;
    struct SwvBar *p_bars;
    uint n_bars;
    struct SwvGlow *p_glows;
    uint n_glows;
    struct SwvShadow *p_shadows;
    uint n_shadows;
};
static struct VrLists swv_lists;



/* Look-up tables of how things look, which the desktop renderer reads too
 * (tngobjdrw.c, engindrwlstm_wrp.c). */

//TODO make some kind of config file for 3D objects, which would include glare locations
struct CarGlare car_glare[] = {
  {   0,   16,  336, 0},
  { -96,  -64,  400, 0},
  { 128,  -64,  400, 0},
  {   0,  -96,  304, 0},
  {-208,  -32, -240, 0},
  { 208,  -32, -240, 0},
  { 128, -144,  240, 0},
  {-112, -144,  240, 0},
  {  96,  -32,  304, 0},
  { -80,  -32,  304, 0},
  {  96,  -96,  528, 0},
  { -80,  -96,  528, 0},
  {-128,  -16,  288, 1},
  { 144,  -16,  288, 2},
  { 112, -112,  -96, 2},
  { -96, -112,  -96, 1},
  { -80, -368,   48, 1},
  {  96, -368,   48, 2},
  { -32,  -64,  160, 0},
  { 112,  -16,  304, 0},
  { -96,  -16,  304, 0},
  { -96, -224,  224, 1},
  {  80, -224,  224, 2},
  { -96, -224, -288, 2},
  {  80, -224, -288, 1},
  {-144,    0,  224, 1},
  { 176,    0,  224, 2},
  {-112,  -32,  448, 0},
  { 144,  -32,  448, 0},
  {-224, -176,  576, 0},
  { 224, -176,  576, 0},
  {   0,   16,  336, 0},
  { -96,  -64,  400, 0},
  { 128,  -64,  400, 0},
  {-160, -672, -688, 0},
  { 160, -672, -688, 0},
  {-112,  -32,  448, 0},
  { 144,  -32,  448, 0},
  {   0,  -96,  304, 0},
  { 112,  -16,  304, 0},
  { -96,  -16,  304, 0},
};

/** For every car model StartFrame (3D object selection), gives a pair of
 * {index of the first glare in `car_glare`, amount of glare dots to draw}.
 */
extern ubyte master_glare[][2];

#if !(defined(ENABLE_DIFFTEST) && ENABLE_DIFFTEST)
/* Variables of the asm data (swars_data.c), for builds without the asm;
 * difftest builds take the byte layout of the asm from that file. */
ubyte master_glare[74][2] = {
    {0, 1}, {1, 2}, {3, 1}, {4, 2}, {6, 2}, {0}, {8, 2}, {0x0a, 0x02},
    {0x0c, 0x04}, {0x10, 0x02}, {0x12, 0x01}, {0x13, 0x02}, {0x15, 0x04},
    {0x19, 0x02}, {0x1b, 0x02}, {0}, {0}, {0}, {0x1d, 0x02}, {0}, {0},
    {0x1f, 0x01}, {0x20, 0x02}, {0x22, 0x02}, {0x24, 0x02}, {0}, {0}, {0},
    {0x26, 0x01}, {0}, {0}, {0x27, 0x02}, {1, 4}, {0x06, 0x0a}, {0x0f, 0x14},
    {0x14, 0x16}, {0x18, 0x19}, {0x1b, 0x1d}, {0x1f}, {0}, {3, 2}, {1, 1},
    {1, 2}, {3}, {0}, {0}, {0}, {0}, {0}, {0x00, 0xff}, {0, 1}, {0x00, 0xff},
    {0xfd, 0xff}, {0, 1}, {3, 4}, {3, 1}, {0x00, 0xfe}, {0xfb, 0xf9},
    {0xf8, 0xf9}, {0xfb, 0xfe}, {0, 2}, {5, 8}, {0x0a, 0x0c}, {0x0d, 0x0d},
    {0x0d, 0x0c}, {0x0a, 0x08}, {5, 2}, {0xfe, 0xfb}, {0xf8, 0xf6},
    {0xf4, 0xf3}, {0xf3, 0xf3}, {0xf4, 0xf6}, {0xf8, 0xfb}, {0xfe, 0x63},
};
#endif /* typed asm data */

/** Brightness added to each person type's sprites. */
ubyte byte_152EF0[] = {
   0, 10,  5, 10,  7,  7,  8, 10,
  10, 10,  5,  7,  5,  7,  7,  0,
};

// animation tables are copied as they are
_Static_assert(sizeof(struct Frame) == sizeof(struct SwvFrame), "SwvFrame layout");
_Static_assert(sizeof(struct Element) == sizeof(struct SwvElement), "SwvElement layout");

/* The renderer projects engine Y on the same scale as map X/Z, see
 * transform_shpoint(); only terrain Alt is 8 times coarser. */
#define ENGINE_Y_TO_MAP(y) ((float)(y))



/******************************************************************************/
/* Assets of the classic look: the game's own when its renderer loaded them,
 * else the library's copies (swview_load_assets()) */

static ubyte *swv_own_tmap[SWV_TMAP_PAGES];
static struct TbSprite *swv_own_sprites;
static struct TbSprite *swv_own_sprites_end;
static ubyte *swv_own_sprites_data;

static const ubyte *swv_tmap_page(int page)
{
    if ((page < VEC_TMAP_LIMIT) && (vec_tmap[page] != NULL))
        return vec_tmap[page];
    return swv_own_tmap[page];
}

static const struct TbSprite *swv_sprites(const struct TbSprite **p_end)
{
    if ((m_sprites != NULL) && (m_sprites_end > m_sprites)) {
        *p_end = m_sprites_end;
        return m_sprites;
    }
    *p_end = swv_own_sprites_end;
    return swv_own_sprites;
}

/******************************************************************************/
/* Triangle list building */

struct VrMesh {
    struct SwvVert *verts;
    uint count;
    uint limit;
};

/** Light level of the map at a tile corner, as the floor drawing computes
 * it from the ambient light and the static lights; 32 is unchanged colours.
 */
static int swv_corner_light(int tile_x, int tile_z)
{
    struct MyMapElement *p_mapel;
    int shade;

    if (tile_x < 0)
        tile_x = 0;
    if (tile_z < 0)
        tile_z = 0;
    if (tile_x >= MAP_TILE_WIDTH)
        tile_x = MAP_TILE_WIDTH - 1;
    if (tile_z >= MAP_TILE_HEIGHT)
        tile_z = MAP_TILE_HEIGHT - 1;
    p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
    shade = (p_mapel->Ambient << 7) + 2 * 128;
    shade += cummulate_shade_from_quick_lights(p_mapel->Shade);
    if (shade > 0x7E00)
        shade = 0x7F00;
    return shade >> 9;
}

/** Vertex shade byte for a point in map units: light level times 4. */
static ubyte swv_point_shade(float x, float z)
{
    int level;

    level = swv_corner_light((int)(x + 128) >> 8, (int)(z + 128) >> 8);
    return (level > 63) ? 252 : level * 4;
}

static void swv_mesh_vert(struct VrMesh *p_mesh, float x, float y, float z,
  int u, int v, ubyte page, ubyte colour, ubyte shade, ubyte flags)
{
    struct SwvVert *p_vert;

    if (p_mesh->count >= p_mesh->limit)
        return;
    p_vert = &p_mesh->verts[p_mesh->count];
    p_vert->x = x;
    p_vert->y = y;
    p_vert->z = z;
    p_vert->u = u;
    p_vert->v = v;
    p_vert->page = page;
    p_vert->colour = colour;
    p_vert->shade = shade;
    p_vert->flags = flags;
    p_mesh->count++;
}

/** Position of an object point in map units, either placed directly on the
 * map, or rotated by a matrix around a given origin.
 */
struct VrPointXform {
    TbBool rotate;
    const struct M33 *p_mat;
    float ox, oy, oz; /**< origin, map units */
};

static void swv_point_pos(const struct VrPointXform *p_xf, const struct SingleObject *p_sobj,
  short point, float *p_x, float *p_y, float *p_z)
{
    struct SinglePoint *p_snpoint;

    p_snpoint = &game_object_points[point];
    if (p_xf->rotate)
    {
        struct M31 vec_inp, vec_rot;
        vec_inp.R[0] = 2 * p_snpoint->X;
        vec_inp.R[1] = 2 * p_snpoint->Y;
        vec_inp.R[2] = 2 * p_snpoint->Z;
        matrix_transform(&vec_rot, (struct M33 *)p_xf->p_mat, &vec_inp);
        *p_x = p_xf->ox + (float)(vec_rot.R[0] >> 15);
        *p_y = p_xf->oy + ENGINE_Y_TO_MAP(vec_rot.R[1] >> 15);
        *p_z = p_xf->oz + (float)(vec_rot.R[2] >> 15);
    }
    else
    {
        *p_x = (float)(p_sobj->MapX + p_snpoint->X);
        *p_y = ENGINE_Y_TO_MAP(p_sobj->OffsetY + p_snpoint->Y);
        *p_z = (float)(p_sobj->MapZ + p_snpoint->Z);
    }
}

static void swv_mesh_object(struct VrMesh *p_mesh, ushort obj, const struct VrPointXform *p_xf)
{
    struct SingleObject *p_sobj;
    int i;

    if (obj == 0 || obj >= next_object)
        return;
    p_sobj = &game_objects[obj];

    for (i = 0; i < p_sobj->NumbFaces; i++)
    {
        struct SingleObjectFace3 *p_face;
        ushort face;
        float x[3], y[3], z[3];
        int uv[3][2];
        ubyte page, flags;
        int k;

        face = p_sobj->StartFace + i;
        if (face >= next_object_face3)
            break;
        p_face = &game_object_faces3[face];
        for (k = 0; k < 3; k++)
            swv_point_pos(p_xf, p_sobj, p_face->PointNo[k], &x[k], &y[k], &z[k]);
        memset(uv, 0, sizeof(uv));
        page = 0;
        flags = 0;
        if ((p_face->Texture != 0) && (p_face->Texture < face_textures_limit))
        {
            struct SingleTexture *p_stex;
            p_stex = &game_face_textures[p_face->Texture];
            uv[0][0] = p_stex->TMapX1; uv[0][1] = p_stex->TMapY1;
            uv[1][0] = p_stex->TMapX2; uv[1][1] = p_stex->TMapY2;
            uv[2][0] = p_stex->TMapX3; uv[2][1] = p_stex->TMapY3;
            page = p_stex->Page;
            flags |= SwvVF_TEXTURED;
        }
        if ((p_face->Flags >= 12) && (p_face->Flags <= 26))
            flags |= SwvVF_TRANSLUCENT;
        if ((p_face->GFlags & FGFlg_Unkn80) != 0)
            flags |= SwvVF_REFLECTIVE;
        for (k = 0; k < 3; k++)
            swv_mesh_vert(p_mesh, x[k], y[k], z[k], uv[k][0], uv[k][1],
              page, p_face->ExCol, swv_point_shade(x[k], z[k]), flags);
    }

    for (i = 0; i < p_sobj->NumbFaces4; i++)
    {
        struct SingleObjectFace4 *p_face4;
        ushort face;
        float x[4], y[4], z[4];
        int uv[4][2];
        ubyte page, flags;
        int k;
        static const int quad_tris[6] = {0, 1, 2, 2, 1, 3};

        face = p_sobj->StartFace4 + i;
        if (face >= next_object_face4)
            break;
        p_face4 = &game_object_faces4[face];
        for (k = 0; k < 4; k++)
            swv_point_pos(p_xf, p_sobj, p_face4->PointNo[k], &x[k], &y[k], &z[k]);
        memset(uv, 0, sizeof(uv));
        page = 0;
        flags = 0;
        if ((p_face4->Texture != 0) && (p_face4->Texture < game_textures_limit))
        {
            struct SingleFloorTexture *p_sftex;
            p_sftex = &game_textures[p_face4->Texture];
            uv[0][0] = p_sftex->TMapX1; uv[0][1] = p_sftex->TMapY1;
            uv[1][0] = p_sftex->TMapX2; uv[1][1] = p_sftex->TMapY2;
            // the engine swaps last two texture corners for some faces
            if ((p_face4->GFlags & FGFlg_Unkn20) != 0) {
                uv[3][0] = p_sftex->TMapX3; uv[3][1] = p_sftex->TMapY3;
                uv[2][0] = p_sftex->TMapX4; uv[2][1] = p_sftex->TMapY4;
            } else {
                uv[2][0] = p_sftex->TMapX3; uv[2][1] = p_sftex->TMapY3;
                uv[3][0] = p_sftex->TMapX4; uv[3][1] = p_sftex->TMapY4;
            }
            page = p_sftex->Page;
            flags |= SwvVF_TEXTURED;
        }
        if ((p_face4->Flags >= 12) && (p_face4->Flags <= 26))
            flags |= SwvVF_TRANSLUCENT;
        if ((p_face4->GFlags & FGFlg_Unkn80) != 0)
            flags |= SwvVF_REFLECTIVE;
        for (k = 0; k < 6; k++) {
            int n = quad_tris[k];
            swv_mesh_vert(p_mesh, x[n], y[n], z[n], uv[n][0], uv[n][1],
              page, p_face4->ExCol, swv_point_shade(x[n], z[n]), flags);
        }
    }
}

/** Colour of flat terrain: the one the desktop's setup picks from the
 * palette (game_setup_stuff()), also when no front end did. */
static ubyte swv_terrain_grey(void)
{
    static ubyte grey = 0;
    static TbBool found = false;

    if (colour_grey2 != 0)
        return colour_grey2;
    if (!found && (display_palette != NULL)) {
        grey = LbPaletteFindColour(display_palette, 32, 32, 32);
        found = true;
    }
    return grey;
}

static void swv_mesh_terrain(struct VrMesh *p_mesh)
{
    int tile_x, tile_z;
    // corners in engine order: 0=(x,z) 1=(x+1,z) 2=(x+1,z+1) 3=(x,z+1)
    static const int crn_dx[4] = {0, 1, 1, 0};
    static const int crn_dz[4] = {0, 0, 1, 1};
    static const int quad_tris[6] = {0, 1, 2, 0, 2, 3};

    for (tile_z = 0; tile_z < MAP_TILE_HEIGHT - 1; tile_z++)
    {
        for (tile_x = 0; tile_x < MAP_TILE_WIDTH - 1; tile_x++)
        {
            struct MyMapElement *p_mapel;
            float y[4];
            int uv[4][2];
            ubyte page, flags, shade;
            int k;

            p_mapel = &game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x];
            for (k = 0; k < 4; k++) {
                struct MyMapElement *p_crnel;
                p_crnel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + crn_dz[k]) + (tile_x + crn_dx[k])];
                y[k] = 8.0f * p_crnel->Alt;
            }
            memset(uv, 0, sizeof(uv));
            page = 0;
            flags = 0;
            if ((p_mapel->Texture & 0x3FFF) != 0)
            {
                struct SingleFloorTexture *p_sftex;
                ushort tex = p_mapel->Texture & 0x3FFF;
                if (tex < game_textures_limit)
                {
                    p_sftex = &game_textures[tex];
                    // engine maps texture corners 1..4 to tile corners 3,2,1,0
                    uv[3][0] = p_sftex->TMapX1; uv[3][1] = p_sftex->TMapY1;
                    uv[2][0] = p_sftex->TMapX2; uv[2][1] = p_sftex->TMapY2;
                    uv[1][0] = p_sftex->TMapX3; uv[1][1] = p_sftex->TMapY3;
                    uv[0][0] = p_sftex->TMapX4; uv[0][1] = p_sftex->TMapY4;
                    page = p_sftex->Page;
                    flags |= SwvVF_TEXTURED;
                }
            }
            for (k = 0; k < 6; k++) {
                int n = quad_tris[k];
                struct MyMapElement *p_crnel;
                ubyte vflags = flags;
                int level;
                p_crnel = &game_my_big_map[MAP_TILE_WIDTH * (tile_z + crn_dz[n]) + (tile_x + crn_dx[n])];
                // as shpoint_compute_coord_y(): per corner ripple or bob
                if ((p_crnel->Flags & MEF1_Unkn10) != 0)
                    vflags |= SwvVF_WOBBLE;
                else if ((p_crnel->Flags & MEF1_Unkn40) != 0)
                    vflags |= SwvVF_WAFT;
                level = swv_corner_light(tile_x + crn_dx[n], tile_z + crn_dz[n]);
                shade = (level > 63) ? 252 : level * 4;
                swv_mesh_vert(p_mesh,
                  (float)TILE_TO_MAPCOORD(tile_x + crn_dx[n], 0), y[n],
                  (float)TILE_TO_MAPCOORD(tile_z + crn_dz[n], 0),
                  uv[n][0], uv[n][1], page, swv_terrain_grey(), shade, vflags);
            }
        }
    }
}

/******************************************************************************/

static TbBool swv_is_player_agent(struct Thing *p_thing)
{
    PlayerInfo *p_locplayer;
    int i;

    p_locplayer = &players[local_player_no];
    for (i = 0; i < AGENTS_SQUAD_MAX_COUNT; i++) {
        if (p_locplayer->MyAgent[i] == p_thing)
            return true;
    }
    return false;
}

/** Called for each drawn part of a mech: the part thing, its rotation
 * matrix (as used for local_mats), and its position in map coordinates
 * with engine height. */
typedef void (*MechPartCallback)(short part, const struct M33 *p_rmat,
  const struct M31 *p_pos, void *ctx);

/** Walks the parts of a mech like mech_parts_update(), but without
 * changing any state: that runs only for mechs in view.
 */
static void mech_parts_walk(struct unkn_mech_struc4 *p_itm4,
  const struct M31 *p_bodypos, const struct M33 *p_mat,
  MechPartCallback cb, void *ctx, int depth)
{
    struct M33 mat, locmat2, rmat;
    struct M31 locvec1, locvec2;
    ubyte n_func12;
    int i;

    if ((p_itm4 == NULL) || (depth > 8))
        return;
    locvec2 = p_itm4->field_61;
    locvec2.R[0] += p_itm4->field_6D.R[0];
    locvec2.R[1] += p_itm4->field_6D.R[1];
    locvec2.R[2] += p_itm4->field_6D.R[2];
    vectors_mul_by_transposed_matrix(1, p_mat, &locvec2, &locvec1);
    locvec2.R[0] = (locvec1.R[0] >> 14) + p_bodypos->R[0];
    locvec2.R[1] = (locvec1.R[1] >> 14) + p_bodypos->R[1];
    locvec2.R[2] = (locvec1.R[2] >> 14) + p_bodypos->R[2];

    mat = *p_mat;
    n_func12 = 0;
    if (p_itm4->angle_49 && (mat.R[0][0] || mat.R[0][1] || mat.R[0][2])
      && (mat.R[1][0] || mat.R[1][1] || mat.R[1][2])
      && (mat.R[2][0] || mat.R[2][1] || mat.R[2][2])) {
        mech_unkn_func_12(&mat, &locmat2, lbSinTable[p_itm4->angle_49], lbSinTable[p_itm4->angle_49 + 512], 0);
        n_func12++;
    }
    if (p_itm4->angle_4D && (mat.R[0][0] || mat.R[0][1] || mat.R[0][2])
      && (mat.R[1][0] || mat.R[1][1] || mat.R[1][2])
      && (mat.R[2][0] || mat.R[2][1] || mat.R[2][2])) {
        mech_unkn_func_12(&mat, &locmat2, lbSinTable[p_itm4->angle_4D], lbSinTable[p_itm4->angle_4D + 512], 1);
        n_func12++;
    }
    if (p_itm4->angle_51 && (mat.R[0][0] || mat.R[0][1] || mat.R[0][2])
      && (mat.R[1][0] || mat.R[1][1] || mat.R[1][2])
      && (mat.R[2][0] || mat.R[2][1] || mat.R[2][2])) {
        mech_unkn_func_12(&mat, &locmat2, lbSinTable[p_itm4->angle_51], lbSinTable[p_itm4->angle_51 + 512], 2);
        n_func12++;
    }
    if (n_func12 > 0)
        mech_unkn_func_13(&mat, &locmat2);

    if (p_itm4->field_79 == 1) {
        matrix_transpose_copy(&mat, &rmat);
        cb(p_itm4->thing, &rmat, &locvec2, ctx);
    }
    for (i = 0; i < p_itm4->field_2 && i < 8; i++)
        mech_parts_walk(p_itm4->field_3[i], &locvec2, &mat, cb, ctx, depth + 1);
}

static void mech_parts_foreach(struct Thing *p_thing, MechPartCallback cb, void *ctx)
{
    struct M31 bodypos;
    int mechno;

    if (unkn_mech_arr3 == NULL)
        return;
    mechno = p_thing->Owner;
    bodypos.R[0] = PRCCOORD_TO_MAPCOORD(p_thing->X);
    bodypos.R[2] = PRCCOORD_TO_MAPCOORD(p_thing->Z);
    bodypos.R[1] = PRCCOORD_TO_YCOORD(p_thing->Y) + 600;
    mech_parts_walk(unkn_mech_arr3[mechno].mech3_arr4_ptr, &bodypos,
      &unkn_mech_arr3[mechno].mech3_unkn_mat_C, cb, ctx, 0);
}

/** Adds one mech part, called through mech_parts_foreach(). */
static void swv_mesh_mech_part(short part, const struct M33 *p_rmat,
  const struct M31 *p_pos, void *ctx)
{
    struct VrPointXform xf;

    if ((part <= 0) || (part >= THINGS_LIMIT))
        return;
    xf.rotate = true;
    xf.p_mat = p_rmat;
    xf.ox = p_pos->R[0];
    xf.oy = p_pos->R[1];
    xf.oz = p_pos->R[2];
    swv_mesh_object((struct VrMesh *)ctx, things[part].U.UObject.Object, &xf);
}

/** Adds a rocket, with the orientation build_rocket() computes from its
 * velocity; the game only sets that up while drawing.
 */
static void swv_mesh_rocket(struct VrMesh *p_mesh, struct Thing *p_thing)
{
    struct VrPointXform xf;
    struct M33 mat;
    struct M31 vec1, vec2, vec3;
    short tng_x, tng_y, tng_z;
    ushort obj;

    obj = ingame.Rocket1[0];
    if (obj == 0)
        return;
    mat.R[0][2] = -64 * p_thing->VX;
    mat.R[1][2] = -512 * p_thing->VY;
    mat.R[2][2] = -64 * p_thing->VZ;
    mat.R[0][1] = 0;
    mat.R[1][1] = 0x4000;
    mat.R[2][1] = 0;
    vec2.R[0] = mat.R[0][2];
    vec2.R[1] = mat.R[1][2];
    vec2.R[2] = mat.R[2][2];
    vec1.R[0] = mat.R[0][1];
    vec1.R[1] = mat.R[1][1];
    vec1.R[2] = mat.R[2][1];
    vec_cross_prod(&vec3, &vec1, &vec2);
    mat.R[0][0] = vec3.R[0] >> 14;
    mat.R[1][0] = vec3.R[1] >> 14;
    mat.R[2][0] = vec3.R[2] >> 14;
    object_vec_normalisation(&mat, 0);

    get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, p_thing->ThingOffset);
    xf.rotate = true;
    xf.p_mat = &mat;
    xf.ox = tng_x + (p_thing->VX >> 1);
    xf.oy = tng_y + 30;
    xf.oz = tng_z + (p_thing->VZ >> 1);
    swv_mesh_object(p_mesh, obj, &xf);
}

/** Sprite frame selection of a person, following build_person(); the
 * viewer applies its own view direction.
 */
static void swv_person_anim(struct SwvThing *p_vthing, struct Thing *p_thing)
{
    int brig;

    memcpy(p_vthing->frv, p_thing->U.UPerson.FrameId.Version, sizeof(p_vthing->frv));
    // as draw_pers_e_graphic() and draw_sorted_sprite1b()
    brig = byte_152EF0[p_thing->SubType] + p_thing->U.UPerson.Brightness;
    if (brig < 10)
        brig = 10;
    if (brig > 60)
        brig = 60;
    p_vthing->brightness = brig;
    p_vthing->subframe = 0;
    p_vthing->anim_start = 0;
    p_vthing->anim_mode = SwvAM_NONE;

    if ((p_thing->Flag2 & TgF2_InsideBuilding) != 0)
        p_vthing->flags |= SwvTF_HIDDEN;

    if (p_thing->State == PerSt_BEING_PERSUADED)
    {
        p_vthing->anim_mode = SwvAM_FIXED;
        p_vthing->anim_start = p_thing->Frame;
        p_vthing->brightness = PALETTE_FADE_LEVELS / 2;
    }
    else if (p_thing->U.UPerson.AnimMode == ANIM_PERS_DEAD_BODY)
    {
        p_vthing->anim_mode = SwvAM_DIR12;
        p_vthing->anim_start = p_thing->StartFrame;
        p_vthing->subframe = get_person_anim_subframe(p_thing);
    }
    else if ((p_thing->Flag & TngF_Unkn02000000) != 0)
    {
        p_vthing->flags |= SwvTF_HIDDEN;
    }
    else if (p_thing->StartFrame == 1066)
    {
        p_vthing->anim_mode = SwvAM_FIXED;
        p_vthing->anim_start = p_thing->Frame;
    }
    else
    {
        p_vthing->anim_mode = SwvAM_DIR8;
        p_vthing->anim_start = p_thing->StartFrame;
        p_vthing->subframe = get_person_anim_subframe(p_thing);
    }
}

static void swv_group_begin(struct VrMesh *p_dyn, uint *p_first)
{
    *p_first = p_dyn->count;
}

static void swv_group_end(struct VrMesh *p_dyn, uint first, u32 id)
{
    struct SwvDynGroup *p_grp;

    if ((p_dyn->count == first) || (swv_lists.n_groups >= SWV_DYN_GROUPS_MAX))
        return;
    p_grp = &swv_lists.p_groups[swv_lists.n_groups++];
    p_grp->id = id;
    p_grp->first = first;
    p_grp->count = p_dyn->count - first;
}

static void swv_bar_add(float x, float y, float z, int health, int max_health, u32 id)
{
    struct SwvBar *p_bar;

    if ((max_health <= 0) || (health >= max_health) || (swv_lists.n_bars >= SWV_BARS_MAX))
        return;
    p_bar = &swv_lists.p_bars[swv_lists.n_bars++];
    p_bar->x = x;
    p_bar->y = y;
    p_bar->z = z;
    p_bar->frac = (health > 0) ? (float)health / max_health : 0.0f;
    p_bar->id = id;
}

static void swv_glow_add(float x, float y, float z, float radius, ubyte colour, u32 id)
{
    struct SwvGlow *p_glow;

    if (swv_lists.n_glows >= SWV_GLOWS_MAX)
        return;
    p_glow = &swv_lists.p_glows[swv_lists.n_glows++];
    p_glow->x = x;
    p_glow->y = y;
    p_glow->z = z;
    p_glow->radius = radius;
    p_glow->colour = colour;
    p_glow->pad[0] = p_glow->pad[1] = p_glow->pad[2] = 0;
    p_glow->id = id;
}

/** Ground height under a point, engine units, from the terrain corners. */
static float swv_ground_y(float x, float z)
{
    int tile_x = (int)x >> 8, tile_z = (int)z >> 8;
    float fx = (x - (tile_x << 8)) / 256.0f, fz = (z - (tile_z << 8)) / 256.0f;
    float a00, a10, a01, a11;

    if ((tile_x < 0) || (tile_z < 0) || (tile_x >= MAP_TILE_WIDTH - 1) || (tile_z >= MAP_TILE_HEIGHT - 1))
        return 0;
    a00 = game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x].Alt;
    a10 = game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x + 1].Alt;
    a01 = game_my_big_map[MAP_TILE_WIDTH * (tile_z + 1) + tile_x].Alt;
    a11 = game_my_big_map[MAP_TILE_WIDTH * (tile_z + 1) + tile_x + 1].Alt;
    return 8.0f * ((a00 * (1 - fx) + a10 * fx) * (1 - fz) + (a01 * (1 - fx) + a11 * fx) * fz);
}

/** Shadow of a vehicle: its object's footprint, turned with the vehicle
 * and laid on the ground below it.
 */
static void swv_vehicle_shadow(struct Thing *p_thing, const struct VrPointXform *p_xf, u32 id)
{
    struct SingleObject *p_sobj;
    struct SwvShadow *p_shd;
    int min_x = 32767, max_x = -32768, min_z = 32767, max_z = -32768;
    int i, k;
    ushort obj;

    obj = p_thing->U.UVehicle.Object;
    if ((obj == 0) || (obj >= next_object) || (swv_lists.n_shadows >= SWV_SHADOWS_MAX))
        return;
    p_sobj = &game_objects[obj];
    for (i = 0; i < p_sobj->NumbFaces4; i++)
    {
        struct SingleObjectFace4 *p_face4 = &game_object_faces4[p_sobj->StartFace4 + i];
        for (k = 0; k < 4; k++) {
            struct SinglePoint *p_pt = &game_object_points[p_face4->PointNo[k]];
            if (p_pt->X < min_x) min_x = p_pt->X;
            if (p_pt->X > max_x) max_x = p_pt->X;
            if (p_pt->Z < min_z) min_z = p_pt->Z;
            if (p_pt->Z > max_z) max_z = p_pt->Z;
        }
    }
    for (i = 0; i < p_sobj->NumbFaces; i++)
    {
        struct SingleObjectFace3 *p_face = &game_object_faces3[p_sobj->StartFace + i];
        for (k = 0; k < 3; k++) {
            struct SinglePoint *p_pt = &game_object_points[p_face->PointNo[k]];
            if (p_pt->X < min_x) min_x = p_pt->X;
            if (p_pt->X > max_x) max_x = p_pt->X;
            if (p_pt->Z < min_z) min_z = p_pt->Z;
            if (p_pt->Z > max_z) max_z = p_pt->Z;
        }
    }
    if (min_x > max_x)
        return;
    p_shd = &swv_lists.p_shadows[swv_lists.n_shadows++];
    for (k = 0; k < 4; k++)
    {
        struct M31 vec_inp, vec_rot;
        vec_inp.R[0] = 2 * ((k == 1 || k == 2) ? max_x : min_x);
        vec_inp.R[1] = 0;
        vec_inp.R[2] = 2 * ((k >= 2) ? max_z : min_z);
        matrix_transform(&vec_rot, (struct M33 *)p_xf->p_mat, &vec_inp);
        p_shd->x[k] = p_xf->ox + (float)(vec_rot.R[0] >> 15);
        p_shd->z[k] = p_xf->oz + (float)(vec_rot.R[2] >> 15);
    }
    p_shd->y = swv_ground_y(p_xf->ox, p_xf->oz);
    if (p_shd->y > p_xf->oy)
        p_shd->y = p_xf->oy;
    p_shd->id = id;
}

/** Headlight glare points of a car, as do_car_glare(). */
static void swv_car_glare(struct Thing *p_car, const struct VrPointXform *p_xf)
{
    ushort glare_beg, glare_num, i;

    if (p_car->StartFrame > 31)
        return;
    glare_beg = master_glare[p_car->StartFrame][0];
    glare_num = master_glare[p_car->StartFrame][1];
    for (i = glare_beg; i < glare_beg + glare_num; i++)
    {
        struct M31 vec_src, vec_dst;
        struct CarGlare *p_glare = &car_glare[i];
        vec_src.R[0] = 2 * p_glare->Dix;
        vec_src.R[1] = -2 * p_glare->Diy;
        vec_src.R[2] = -2 * p_glare->Diz;
        matrix_transform(&vec_dst, (struct M33 *)p_xf->p_mat, &vec_src);
        swv_glow_add(p_xf->ox + (vec_dst.R[0] >> 15), p_xf->oy + (vec_dst.R[1] >> 15),
          p_xf->oz + (vec_dst.R[2] >> 15), 48 * 8, colour_lookup[ColLU_WHITE],
          ((u32)p_car->ThingOffset << 8) | (i & 0xFF));
    }
}

/** Explosion debris: faces flying off destroyed objects and floor. */
static void swv_mesh_explode_faces(struct VrMesh *p_mesh)
{
    ushort exface;

    for (exface = 1; exface < EXPLODE_FACES_COUNT; exface++)
    {
        struct ExplodeFace *p_exface = &ex_faces[exface];
        float px[4], py[4], pz[4];
        int uv[4][2];
        ubyte page = 0, flags = 0;
        int npts, k;
        uint first;
        TbBool abs_cor;
        static const int quad_tris[6] = {0, 1, 2, 2, 1, 3};

        if ((p_exface->Timer == 0) || (p_exface->Type == EXPL_FACE_NONE))
            continue;
        npts = ((p_exface->Type == EXPL_FACE_QUAD_TYP2) || (p_exface->Type == EXPL_FACE_QUAD_ABS)
          || (p_exface->Type == EXPL_FACE_QUAD_REL)) ? 4 : 3;
        abs_cor = (p_exface->Type == EXPL_FACE_TRI_ABS) || (p_exface->Type == EXPL_FACE_QUAD_ABS);
        // as enlist_draw_explode_type1/3/5()
        px[0] = p_exface->X0; py[0] = p_exface->Y0; pz[0] = p_exface->Z0;
        px[1] = p_exface->X1; py[1] = p_exface->Y1; pz[1] = p_exface->Z1;
        px[2] = p_exface->X2; py[2] = p_exface->Y2; pz[2] = p_exface->Z2;
        px[3] = p_exface->X3; py[3] = p_exface->Y3; pz[3] = p_exface->Z3;
        if (!abs_cor) {
            for (k = 0; k < 4; k++) {
                px[k] += p_exface->X;
                py[k] += p_exface->Y;
                pz[k] += p_exface->Z;
            }
        }
        memset(uv, 0, sizeof(uv));
        if (npts == 4) {
            if ((p_exface->Texture != 0) && (p_exface->Texture < game_textures_limit)) {
                struct SingleFloorTexture *p_sftex = &game_textures[p_exface->Texture];
                uv[0][0] = p_sftex->TMapX1; uv[0][1] = p_sftex->TMapY1;
                uv[1][0] = p_sftex->TMapX2; uv[1][1] = p_sftex->TMapY2;
                uv[2][0] = p_sftex->TMapX3; uv[2][1] = p_sftex->TMapY3;
                uv[3][0] = p_sftex->TMapX4; uv[3][1] = p_sftex->TMapY4;
                page = p_sftex->Page;
                flags |= SwvVF_TEXTURED;
            }
        } else {
            if ((p_exface->Texture != 0) && (p_exface->Texture < face_textures_limit)) {
                struct SingleTexture *p_stex = &game_face_textures[p_exface->Texture];
                uv[0][0] = p_stex->TMapX1; uv[0][1] = p_stex->TMapY1;
                uv[1][0] = p_stex->TMapX2; uv[1][1] = p_stex->TMapY2;
                uv[2][0] = p_stex->TMapX3; uv[2][1] = p_stex->TMapY3;
                page = p_stex->Page;
                flags |= SwvVF_TEXTURED;
            }
        }
        swv_group_begin(p_mesh, &first);
        if (npts == 4) {
            for (k = 0; k < 6; k++) {
                int n = quad_tris[k];
                swv_mesh_vert(p_mesh, px[n], py[n], pz[n], uv[n][0], uv[n][1], page,
                  p_exface->Col, swv_point_shade(px[n], pz[n]), flags);
            }
        } else {
            for (k = 0; k < 3; k++)
                swv_mesh_vert(p_mesh, px[k], py[k], pz[k], uv[k][0], uv[k][1], page,
                  p_exface->Col, swv_point_shade(px[k], pz[k]), flags);
        }
        swv_group_end(p_mesh, first, 0x20000000u | exface);
    }
}

/** Signature of what the static export shows, to re-send it at once when
 * buildings are destroyed or the ground is cratered.
 */
static u32 swv_world_signature(void)
{
    u32 hash = 0;
    ThingIdx thing;
    int i;

    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        if (things[thing].Type != TT_BUILDING)
            continue;
        hash = hash * 31 + thing;
        hash = hash * 31 + things[thing].U.UObject.Object;
    }
    for (i = 0; i < MAP_TILE_WIDTH * MAP_TILE_HEIGHT; i++)
        hash = hash * 31 + (u32)(game_my_big_map[i].Alt * 65536 + game_my_big_map[i].Texture);
    return hash;
}

static void swv_export_things(struct SwvHeader *p_hdr, struct VrMesh *p_dyn)
{
    struct SwvThing *p_vthings;
    uint n;
    ThingIdx thing;

    p_vthings = (struct SwvThing *)(swv_block + SWV_OFF_THINGS);
    n = 0;
    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing;
        short cor_x, cor_y, cor_z;

        p_thing = &things[thing];
        if (p_thing->Type == TT_NONE)
            continue;

        if (p_thing->Type == TT_VEHICLE)
        {
            struct VrPointXform xf;
            uint first;
            if (p_thing->SubType == SubTT_VEH_MECH) {
                swv_group_begin(p_dyn, &first);
                if ((p_thing->Flag & TngF_Destroyed) == 0)
                    mech_parts_foreach(p_thing, swv_mesh_mech_part, p_dyn);
                swv_group_end(p_dyn, first, thing);
                swv_bar_add(PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_YCOORD(p_thing->Y) + 1200,
                  PRCCOORD_TO_MAPCOORD(p_thing->Z), p_thing->Health, p_thing->U.UVehicle.MaxHealth, thing);
                continue;
            }
            if (p_thing->SubType == SubTT_VEH_SHUTTLE_POD)
                continue;
            if (p_thing->U.UVehicle.MatrixIndex >= LOCAL_MATS_COUNT)
                continue;
            xf.rotate = true;
            xf.p_mat = &local_mats[p_thing->U.UVehicle.MatrixIndex];
            xf.ox = (float)PRCCOORD_TO_MAPCOORD(p_thing->X);
            xf.oy = ENGINE_Y_TO_MAP(PRCCOORD_TO_YCOORD(p_thing->Y));
            xf.oz = (float)PRCCOORD_TO_MAPCOORD(p_thing->Z);
            swv_group_begin(p_dyn, &first);
            swv_mesh_object(p_dyn, p_thing->U.UVehicle.Object, &xf);
            swv_group_end(p_dyn, first, thing);
            // tank turret, as process_child_object()
            if ((p_thing->SubType == SubTT_VEH_TANK) && (p_thing->U.UVehicle.SubThing > 0)
              && (p_thing->U.UVehicle.SubThing < THINGS_LIMIT))
            {
                struct Thing *p_mgun = &things[p_thing->U.UVehicle.SubThing];
                struct M31 vec1, vec2;
                struct VrPointXform gxf;
                vec2.R[0] = PRCCOORD_TO_MAPCOORD(p_mgun->X);
                vec2.R[1] = p_mgun->Y >> 4;
                vec2.R[2] = PRCCOORD_TO_MAPCOORD(p_mgun->Z);
                matrix_transform(&vec1, (struct M33 *)xf.p_mat, &vec2);
                if (p_mgun->U.UMGun.MatrixIndex < LOCAL_MATS_COUNT)
                {
                    short tng_x, tng_y, tng_z;
                    get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, thing);
                    gxf.rotate = true;
                    gxf.p_mat = &local_mats[p_mgun->U.UMGun.MatrixIndex];
                    gxf.ox = tng_x + (vec1.R[0] >> 15);
                    gxf.oy = tng_y + (vec1.R[1] >> 15);
                    gxf.oz = tng_z + (vec1.R[2] >> 15);
                    swv_group_begin(p_dyn, &first);
                    swv_mesh_object(p_dyn, p_mgun->U.UMGun.Object, &gxf);
                    swv_group_end(p_dyn, first, 0x10000000u | p_mgun->ThingOffset);
                }
            }
            if (p_thing->SubType != SubTT_VEH_TRAIN)
                swv_vehicle_shadow(p_thing, &xf, thing);
            swv_car_glare(p_thing, &xf);
            swv_bar_add(xf.ox, xf.oy + 300, xf.oz, p_thing->Health, p_thing->U.UVehicle.MaxHealth, thing);
            continue;
        }
        if (p_thing->Type == TT_ROCKET)
        {
            uint first;
            short tng_x, tng_y, tng_z;
            swv_group_begin(p_dyn, &first);
            swv_mesh_rocket(p_dyn, p_thing);
            swv_group_end(p_dyn, first, thing);
            // build_rocket() adds a glare at the rocket
            get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, thing);
            swv_glow_add(tng_x, tng_y, tng_z, 64 * 8, colour_lookup[ColLU_YELLOW], thing);
            continue;
        }
        if ((p_thing->Type == TT_BUILDING) &&
          ((p_thing->SubType == SubTT_BLD_MOVN_ROTOR) || (p_thing->SubType == SubTT_BLD_MGUN)))
        {
            struct VrPointXform xf;
            if (p_thing->U.UObject.MatrixIndex >= LOCAL_MATS_COUNT)
                continue;
            get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
            xf.rotate = true;
            xf.p_mat = &local_mats[p_thing->U.UObject.MatrixIndex];
            xf.ox = cor_x;
            xf.oy = ENGINE_Y_TO_MAP(cor_y);
            xf.oz = cor_z;
            {
                uint first;
                swv_group_begin(p_dyn, &first);
                swv_mesh_object(p_dyn, p_thing->U.UObject.Object, &xf);
                swv_group_end(p_dyn, first, thing);
            }
            if (p_thing->SubType == SubTT_BLD_MGUN)
                swv_bar_add(cor_x, cor_y + 300, cor_z, p_thing->Health, p_thing->U.UMGun.MaxHealth, thing);
            continue;
        }
        if (p_thing->Type != TT_PERSON)
            continue;
        if (n >= SWV_THINGS_MAX)
            break;
        get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
        p_vthings[n].x = cor_x;
        p_vthings[n].y = ENGINE_Y_TO_MAP(cor_y);
        p_vthings[n].z = cor_z;
        p_vthings[n].index = thing;
        p_vthings[n].type = p_thing->Type;
        p_vthings[n].subtype = p_thing->SubType;
        p_vthings[n].flags = 0;
        if (swv_is_player_agent(p_thing))
            p_vthings[n].flags |= SwvTF_PLAYER_AGENT;
        if (thing == players[local_player_no].DirectControl[0])
            p_vthings[n].flags |= SwvTF_SELECTED;
        if ((p_thing->State == PerSt_DEAD) || ((p_thing->Flag & TngF_Destroyed) != 0))
            p_vthings[n].flags |= SwvTF_DEAD;
        p_vthings[n].angle = p_thing->U.UPerson.Angle;
        p_vthings[n].group = p_thing->U.UPerson.EffectiveGroup;
        p_vthings[n].pad = 0;
        p_vthings[n].health = p_thing->Health;
        p_vthings[n].max_health = p_thing->U.UPerson.MaxHealth;
        swv_person_anim(&p_vthings[n], p_thing);
        n++;
    }
    p_hdr->n_things = n;
}

static void swv_export_static(struct SwvHeader *p_hdr)
{
    struct VrMesh mesh;
    ThingIdx thing;

    mesh.verts = (struct SwvVert *)(swv_block + SWV_OFF_STATIC);
    mesh.count = 0;
    mesh.limit = SWV_STATIC_VERTS_MAX;

    swv_mesh_terrain(&mesh);

    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing;
        struct VrPointXform xf;
        ushort obj, beg_obj, end_obj;

        p_thing = &things[thing];
        if (p_thing->Type != TT_BUILDING)
            continue;
        if ((p_thing->SubType == SubTT_BLD_MOVN_ROTOR) || (p_thing->SubType == SubTT_BLD_MGUN))
            continue;
        memset(&xf, 0, sizeof(xf));
        beg_obj = p_thing->U.UObject.Object;
        end_obj = beg_obj + p_thing->U.UObject.NumbObjects;
        for (obj = beg_obj; obj < end_obj; obj++)
            swv_mesh_object(&mesh, obj, &xf);
    }
    p_hdr->n_static = mesh.count;
    p_hdr->static_rev = ++swv_static_rev;
    if (swv_last_map != current_map)
    {
        int n_veh = 0, n_mech = 0;
        for (thing = 1; thing < THINGS_LIMIT; thing++)
        {
            if (things[thing].Type != TT_VEHICLE)
                continue;
            n_veh++;
            if (things[thing].SubType == SubTT_VEH_MECH)
                n_mech++;
        }
        LOGSYNC_F("VR spectator: map %d, %u static vertices, %d vehicles of which %d mechs",
          (int)current_map, (uint)mesh.count, n_veh, n_mech);
    }
}

static void swv_export_tmap(struct SwvHeader *p_hdr)
{
    int page;

    for (page = 0; page < SWV_TMAP_PAGES; page++)
    {
        ubyte *p_dst = swv_block + SWV_OFF_TMAP + page * SWV_TMAP_DIM * SWV_TMAP_DIM;
        const ubyte *p_src = swv_tmap_page(page);
        if (p_src != NULL)
            memcpy(p_dst, p_src, SWV_TMAP_DIM * SWV_TMAP_DIM);
        else
            memset(p_dst, 0, SWV_TMAP_DIM * SWV_TMAP_DIM);
    }
    if (display_palette != NULL)
        memcpy(swv_block + SWV_OFF_PALETTE, display_palette, 768);
    p_hdr->tmap_rev = ++swv_tmap_rev;
}

/** Decodes one RLE sprite into a sprite page with 2 bytes per texel. */
static void swv_decode_sprite(const struct TbSprite *p_spr, ubyte *p_page, int px, int py)
{
    const sbyte *p_dat;
    int x, y;

    p_dat = (const sbyte *)p_spr->Data;
    for (y = 0; y < p_spr->SHeight; y++)
    {
        ubyte *p_row = p_page + 2 * (SWV_TMAP_DIM * (py + y) + px);
        x = 0;
        while (1)
        {
            sbyte c = *p_dat++;
            if (c == 0)
                break;
            if (c < 0) {
                x -= c;
                continue;
            }
            while (c-- > 0) {
                ubyte col = (ubyte)*p_dat++;
                if (x < p_spr->SWidth) {
                    p_row[2 * x + 0] = col;
                    p_row[2 * x + 1] = 255;
                }
                x++;
            }
        }
    }
}

/** Exports the in-mission sprites, shelf-packed into pages, and the
 * animation tables which compose them into frames.
 */
static void swv_export_sprites(struct SwvHeader *p_hdr)
{
    struct SwvSpriteRect *p_rects;
    ubyte *p_pages;
    const struct TbSprite *p_sprites, *p_sprites_end;
    uint n_sprites, i, n;
    int page, cx, cy, shelf_h;

    p_rects = (struct SwvSpriteRect *)(swv_block + SWV_OFF_SPRITE_RECTS);
    p_pages = swv_block + SWV_OFF_SPRITE_PAGES;
    memset(p_pages, 0, SWV_SPRITE_PAGES * SWV_SPRITE_PAGE_BYTES);
    memset(p_rects, 0, SWV_SPRITES_MAX * sizeof(*p_rects));

    n_sprites = 0;
    p_sprites = swv_sprites(&p_sprites_end);
    if ((p_sprites != NULL) && (p_sprites_end > p_sprites))
        n_sprites = p_sprites_end - p_sprites;
    if (n_sprites > SWV_SPRITES_MAX)
        n_sprites = SWV_SPRITES_MAX;

    page = 0;
    cx = cy = shelf_h = 0;
    for (i = 1; i < n_sprites; i++)
    {
        const struct TbSprite *p_spr = &p_sprites[i];
        int w = p_spr->SWidth, h = p_spr->SHeight;

        if ((w == 0) || (h == 0) || (p_spr->Data == NULL))
            continue;
        // one texel of padding keeps neighbours out of filtered edges
        if (cx + w > SWV_TMAP_DIM) {
            cx = 0;
            cy += shelf_h + 1;
            shelf_h = 0;
        }
        if (cy + h > SWV_TMAP_DIM) {
            page++;
            cx = cy = shelf_h = 0;
        }
        if (page >= SWV_SPRITE_PAGES) {
            LOGWARN("sprite pages full at sprite %u of %u", i, n_sprites);
            break;
        }
        swv_decode_sprite(p_spr, p_pages + page * SWV_SPRITE_PAGE_BYTES, cx, cy);
        p_rects[i].page = page;
        p_rects[i].x = cx;
        p_rects[i].y = cy;
        p_rects[i].w = w;
        p_rects[i].h = h;
        cx += w + 1;
        if (h > shelf_h)
            shelf_h = h;
    }
    p_hdr->n_sprites = n_sprites;
    p_hdr->n_sprite_pages = (page < SWV_SPRITE_PAGES) ? page + 1 : SWV_SPRITE_PAGES;
    // frame elements give a sprite as a byte offset in the .tab file, whose
    // records are smaller than TbSprite on 64-bit targets
    p_hdr->sprite_stride = LB_SPRITE_FILE_SIZE;

    n = (nstart_ani != NULL) ? (uint)(nstart_ani_end - nstart_ani) : 0;
    if (n > SWV_NSTART_MAX)
        n = SWV_NSTART_MAX;
    if (n > 0)
        memcpy(swv_block + SWV_OFF_NSTART, nstart_ani, n * sizeof(ushort));
    p_hdr->n_nstart = n;

    n = (frame != NULL) ? (uint)(frame_end - frame) : 0;
    if (n > SWV_FRAMES_MAX)
        n = SWV_FRAMES_MAX;
    if (n > 0)
        memcpy(swv_block + SWV_OFF_FRAMES, frame, n * sizeof(struct Frame));
    p_hdr->n_frames = n;

    n = (melement_ani != NULL) ? (uint)(mele_ani_end - melement_ani) : 0;
    if (n > SWV_ELEMENTS_MAX)
        n = SWV_ELEMENTS_MAX;
    if (n > 0)
        memcpy(swv_block + SWV_OFF_ELEMENTS, melement_ani, n * sizeof(struct Element));
    p_hdr->n_elements = n;

    p_hdr->sprite_rev = ++swv_sprite_rev;
    LOGSYNC_F("VR spectator: %u sprites in %u pages, %u frames, %u elements",
      n_sprites, (uint)p_hdr->n_sprite_pages, (uint)p_hdr->n_frames, (uint)p_hdr->n_elements);
}

/******************************************************************************/
/* Effects and items drawn as sprite frames */

struct VrEffects {
    struct SwvEffect *p_list;
    uint count;
};

static void swv_effect_add(struct VrEffects *p_efx, float x, float y, float z,
  ushort frm, ushort scale, int brig, ubyte kind, u32 id)
{
    struct SwvEffect *p_eff;

    if (p_efx->count >= SWV_EFFECTS_MAX)
        return;
    if ((frame == NULL) || (frm == 0) || (frm >= frame_end - frame))
        return;
    // draw_frame_alpha() limits the fade level
    if (brig < 10)
        brig = 10;
    if (brig > 48)
        brig = 48;
    p_eff = &p_efx->p_list[p_efx->count++];
    p_eff->x = x;
    p_eff->y = y;
    p_eff->z = z;
    p_eff->frame = frm;
    p_eff->scale = scale;
    p_eff->brightness = brig;
    p_eff->kind = kind;
    p_eff->pad = 0;
    p_eff->id = id;
}

/** Map light level under a precise position, as used for item sprites. */
static int swv_ground_shade(s32 prc_x, s32 prc_z)
{
    int tile_x, tile_z;

    tile_x = prc_x >> 16;
    tile_z = prc_z >> 16;
    if ((tile_x < 0) || (tile_x >= MAP_TILE_WIDTH) || (tile_z < 0) || (tile_z >= MAP_TILE_HEIGHT))
        return 32;
    return game_my_big_map[MAP_TILE_WIDTH * tile_z + tile_x].ShadeR;
}

/** Shrapnel shards as two-sided grey triangles, following
 * enlist_draw_bang_shrapnels().
 */
static void swv_mesh_shrapnel(struct VrMesh *p_mesh, short shrap_beg)
{
    struct Shrapnel *p_shrapnel;
    int shrap, guard;

    guard = 0;
    for (shrap = shrap_beg; (shrap > 0) && (shrap < SHRAPNEL_COUNT) && (guard < SHRAPNEL_COUNT);
      shrap = p_shrapnel->child, guard++)
    {
        int cos_yaw, cos_pitch, sin_yaw, sin_pitch;
        int sh_cc, sh_cs, sh_sc, sh_ss, sh_z;
        float cx, cy, cz;
        float px[3], py[3], pz[3];
        int k;

        p_shrapnel = &shrapnel[shrap];
        if ((p_shrapnel->type < 1) || (p_shrapnel->type > 3))
            continue;
        cos_yaw = lbSinTable[8 * p_shrapnel->yaw + LbFPMath_PI/2];
        sin_yaw = lbSinTable[8 * p_shrapnel->yaw];
        cos_pitch = lbSinTable[8 * p_shrapnel->pitch + LbFPMath_PI/2];
        sin_pitch = lbSinTable[8 * p_shrapnel->pitch];
        // products of 16.16 fractions, scaled to shard size
        sh_cc = (int)(((s64)cos_pitch * cos_yaw) >> 16) >> 10;
        sh_cs = (int)(((s64)cos_pitch * sin_yaw) >> 16) >> 10;
        sh_sc = (int)(((s64)sin_pitch * cos_yaw) >> 16) >> 10;
        sh_ss = (int)(((s64)sin_pitch * sin_yaw) >> 16) >> 10;
        sh_z = sin_yaw >> 10;
        cx = p_shrapnel->x >> 8;
        cy = p_shrapnel->y >> 5;
        cz = p_shrapnel->z >> 8;
        px[0] = cx + sh_cc; py[0] = cy - sh_sc; pz[0] = cz - sh_z;
        px[1] = cx + sh_cs; py[1] = cy - sh_ss; pz[1] = cz;
        px[2] = cx - sh_cc - sh_cs; py[2] = cy + sh_sc + sh_ss; pz[2] = cz + sh_z;
        {
            uint first;
            swv_group_begin(p_mesh, &first);
            for (k = 0; k < 3; k++)
                swv_mesh_vert(p_mesh, px[k], py[k], pz[k], 0, 0, 0,
                  colour_lookup[ColLU_GREYLT], 128, 0);
            swv_group_end(p_mesh, first, 0x30000000u | shrap);
        }
    }
}

static void swv_export_effects(struct SwvHeader *p_hdr, struct VrMesh *p_dyn)
{
    struct VrEffects efx;
    ThingIdx thing;
    int remain;

    efx.p_list = (struct SwvEffect *)(swv_block + SWV_OFF_EFFECTS);
    efx.count = 0;

    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing;
        short cor_x, cor_y, cor_z;

        p_thing = &things[thing];
        if ((p_thing->Type == TT_MINE) || (p_thing->Type == TT_GRENADE))
        {
            get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
            swv_effect_add(&efx, cor_x, cor_y, cor_z, p_thing->Frame, 256,
              swv_ground_shade(p_thing->X, p_thing->Z), SwvEK_GRENADE, thing);
        }
        else if (p_thing->Type == TT_UNKN18)
        {
            swv_effect_add(&efx, PRCCOORD_TO_MAPCOORD(p_thing->X),
              PRCCOORD_TO_YCOORD(p_thing->Y), PRCCOORD_TO_MAPCOORD(p_thing->Z),
              nstart_ani[900], 256, 63, SwvEK_OTHER, thing);
        }
    }

    remain = sthings_used;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;
        float x, y, z;

        if (--remain < 0)
            break;
        p_sthing = &sthings[thing];
        x = PRCCOORD_TO_MAPCOORD(p_sthing->X);
        y = PRCCOORD_TO_YCOORD(p_sthing->Y);
        z = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
        switch (p_sthing->Type)
        {
        case SmTT_STATIC:
            if (p_sthing->Frame == nstart_ani[1008])
                break;
            swv_effect_add(&efx, x, y, z, p_sthing->Frame, 256,
              swv_ground_shade(p_sthing->X, p_sthing->Z), SwvEK_STATIC, 0x01000000u | (ushort)thing);
            break;
        case SmTT_DROPPED_ITEM:
            swv_effect_add(&efx, x, y, z, p_sthing->Frame, 256,
              swv_ground_shade(p_sthing->X, p_sthing->Z), SwvEK_DROPPED_ITEM, 0x01000000u | (ushort)thing);
            break;
        case SmTT_SCALE_EFFECT:
            swv_effect_add(&efx, x, y, z, p_sthing->Frame, p_sthing->Object,
              32, SwvEK_SCALE_EFFECT, 0x01000000u | (ushort)thing);
            break;
        case SmTT_BANG:
        {
            struct Phwoar *p_phwoar;
            int phw, guard = 0;
            for (phw = p_sthing->U.UBang.phwoar; (phw > 0) && (phw < PHWOAR_COUNT) && (guard < PHWOAR_COUNT);
              phw = p_phwoar->child, guard++)
            {
                p_phwoar = &phwoar[phw];
                swv_effect_add(&efx, p_phwoar->x >> 8, p_phwoar->y >> 5, p_phwoar->z >> 8,
                  p_phwoar->f, 256, 32, SwvEK_SMOKE, 0x02000000u | phw);
            }
            swv_mesh_shrapnel(p_dyn, p_sthing->U.UBang.shrapnel);
            break;
        }
        case SmTT_FIRE:
        {
            struct FireFlame *p_flame;
            int flm, guard = 0;
            for (flm = p_sthing->U.UFire.flame; (flm > 0) && (flm < FLAME_COUNT) && (guard < FLAME_COUNT);
              flm = p_flame->next, guard++)
            {
                ushort scale;
                p_flame = &FIRE_flame[flm];
                // draw_fire_flame() enlarges big flames by (big + 128) / 128
                scale = (p_flame->big != 0) ? 2 * (p_flame->big + 128) : 256;
                swv_effect_add(&efx, p_flame->x, p_flame->y, p_flame->z,
                  p_flame->frame, scale, 32, SwvEK_FLAME, 0x03000000u | flm);
            }
            break;
        }
        default:
            break;
        }
    }
    p_hdr->n_effects = efx.count;
}


/******************************************************************************/
/* Line effects: lasers, beams, electricity, sparks and wires */

struct VrLines {
    struct SwvLine *p_list;
    uint count;
};

/** Adds a line; positions in map units with engine height. */
static void swv_line_add(struct VrLines *p_lns, float x1, float y1, float z1,
  float x2, float y2, float z2, ubyte colour, ubyte width, ubyte flags)
{
    struct SwvLine *p_ln;

    if (p_lns->count >= SWV_LINES_MAX)
        return;
    p_ln = &p_lns->p_list[p_lns->count++];
    p_ln->x1 = x1; p_ln->y1 = y1; p_ln->z1 = z1;
    p_ln->x2 = x2; p_ln->y2 = y2; p_ln->z2 = z2;
    p_ln->colour = colour;
    p_ln->width = width;
    p_ln->flags = flags;
    p_ln->pad = 0;
}

/** Ring of line segments around a vertical axis. */
static void swv_line_ring(struct VrLines *p_lns, float cx, float cy, float cz,
  float radius, ubyte colour, ubyte width)
{
    int i;
    const int segs = 32;

    for (i = 0; i < segs; i++)
    {
        int a1 = (i * 2048 / segs) & LbFPMath_AngleMask;
        int a2 = ((i + 1) * 2048 / segs) & LbFPMath_AngleMask;
        swv_line_add(p_lns,
          cx + radius * lbSinTable[a1] / 65536.0f, cy, cz + radius * lbSinTable[a1 + LbFPMath_PI/2] / 65536.0f,
          cx + radius * lbSinTable[a2] / 65536.0f, cy, cz + radius * lbSinTable[a2 + LbFPMath_PI/2] / 65536.0f,
          colour, width, SwvLF_GLOW);
    }
}

/** Start of a shot: the muzzle of its owner, where the game itself starts
 * shots (thing_fire_shot_start_position()), following the owner as it
 * moves; the drawing code starts them there too, plus a screen offset.
 */
static void swv_shot_start(struct Thing *p_owner, WeaponType wtype,
  float *p_x, float *p_y, float *p_z)
{
    struct M31 prc_beg;

    if (((p_owner->Type == TT_PERSON) || (p_owner->Type == TT_VEHICLE)
      || (p_owner->Type == TT_BUILDING))
      && thing_fire_shot_start_position(&prc_beg, p_owner, wtype, 0))
    {
        *p_x = PRCCOORD_TO_MAPCOORD(prc_beg.R[0]);
        *p_y = PRCCOORD_TO_YCOORD(prc_beg.R[1]);
        *p_z = PRCCOORD_TO_MAPCOORD(prc_beg.R[2]);
        return;
    }
    *p_x = PRCCOORD_TO_MAPCOORD(p_owner->X);
    *p_y = PRCCOORD_TO_YCOORD(p_owner->Y);
    *p_z = PRCCOORD_TO_MAPCOORD(p_owner->Z);
}

static void swv_lines_guided_laser(struct VrLines *p_lns, struct Thing *p_laser)
{
    struct Thing *p_target;
    int cur_x, cur_y, cur_z;
    int target_x, target_z;
    short cur_angle, full_angle, glow_size, rate;
    int count;

    p_target = p_laser->PTarget;
    if (p_target == NULL)
        return;
    cur_x = PRCCOORD_TO_MAPCOORD(p_laser->X);
    cur_y = PRCCOORD_TO_MAPCOORD(p_laser->Y);
    cur_z = PRCCOORD_TO_MAPCOORD(p_laser->Z);
    target_x = PRCCOORD_TO_MAPCOORD(p_target->X);
    target_z = PRCCOORD_TO_MAPCOORD(p_target->Z);
    cur_angle = p_laser->U.UEffect.AngleY;
    // as build_laser_guided_piece()
    for (count = 5; count > 0; count--)
    {
        int new_x, new_z;

        full_angle = angle_between_points(cur_x, cur_z, target_x, target_z);
        rate = calc_angle_rate(full_angle, cur_angle, &glow_size);
        new_x = cur_x + ((lbSinTable[cur_angle] * glow_size) >> 17);
        new_z = cur_z + ((lbSinTable[cur_angle + LbFPMath_PI/2] * glow_size) >> 17);
        swv_line_add(p_lns, cur_x, 8 * cur_y, cur_z, new_x, 8 * cur_y, new_z,
          (count == 1) ? colour_lookup[ColLU_WHITE] : colour_lookup[2], 14, SwvLF_GLOW);
        cur_angle = (cur_angle + rate) & LbFPMath_AngleMask;
        cur_x = new_x;
        cur_z = new_z;
    }
}

static void swv_lines_stasis_pod(struct VrLines *p_lns, struct SimpleThing *p_sthing)
{
    int radius, seg, pod_angl;
    float cx, cy, cz;

    radius = p_sthing->Radius;
    if (radius == 0)
        return;
    cx = p_sthing->X >> 8;
    cy = p_sthing->Y >> 8;
    cz = p_sthing->Z >> 8;
    // as build_stasis_pod(): rings of a sphere, spinning with gameturn
    pod_angl = 16 * gameturn - 16 * p_sthing->ThingOffset;
    for (seg = 0; seg < 8; seg++)
    {
        int pod_radius, cor_dy;

        pod_angl &= 0x01FF;
        pod_radius = (radius * lbSinTable[pod_angl + LbFPMath_PI/2]) >> 16;
        cor_dy = (radius * lbSinTable[pod_angl]) >> 19;
        swv_line_ring(p_lns, cx, 8 * (cy + cor_dy), cz, pod_radius,
          colour_lookup[ColLU_RED], 6);
        pod_angl += 100;
    }
}

static void swv_export_lines(struct SwvHeader *p_hdr)
{
    struct VrLines lns;
    struct Thing *p_dcthing;
    ThingIdx thing;
    int remain;

    lns.p_list = (struct SwvLine *)(swv_block + SWV_OFF_LINES);
    lns.count = 0;
    p_dcthing = &things[players[local_player_no].DirectControl[0]];

    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing, *p_owner;
        float x1, y1, z1, x2, y2, z2;
        int i, n;

        p_thing = &things[thing];
        switch (p_thing->Type)
        {
        case TT_LASER11:
            p_owner = &things[p_thing->Owner];
            swv_shot_start(p_owner, WEP_LASER, &x1, &y1, &z1);
            swv_line_add(&lns, x1, y1, z1, p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
              ((p_thing->Flag & TngF_SelectedAgent) != 0) ? colour_lookup[ColLU_BLUE] : colour_lookup[ColLU_RED],
              10, SwvLF_GLOW);
            break;
        case TT_LASER29:
        case TT_LASER38:
            p_owner = &things[p_thing->Owner];
            swv_shot_start(p_owner, WEP_BEAM, &x1, &y1, &z1);
            x2 = p_thing->VX; y2 = 8 * p_thing->VY; z2 = p_thing->VZ;
            // build_laser_beam() and build_laser_beam_q(): core and strands
            swv_line_add(&lns, x1, y1, z1, x2, y2, z2,
              (p_thing->Type == TT_LASER29) ? colour_lookup[ColLU_WHITE] : colour_lookup[ColLU_BLUE],
              (p_thing->Timer1 > 2) ? 40 : 12, SwvLF_GLOW);
            // build_laser_beam_glare(): flash at the muzzle
            swv_glow_add(x1, y1, z1, 13 * 8,
              (p_thing->Type == TT_LASER29) ? colour_lookup[ColLU_WHITE] : colour_lookup[ColLU_BLUE], thing);
            n = (p_thing->Type == TT_LASER29) ? 2 : 4;
            for (i = 0; i < n; i++)
                swv_line_add(&lns, x1, y1 + 160, z1, x2, y2, z2,
                  colour_lookup[ColLU_WHITE], 4, SwvLF_JAGGED);
            break;
        case TT_LASER_ELEC:
            p_owner = &things[p_thing->Owner];
            swv_shot_start(p_owner, WEP_ELLASER, &x1, &y1, &z1);
            n = (p_thing->State != 0) ? 1 : (p_thing->SubType >> 1);
            for (i = 0; i < n; i++)
                swv_line_add(&lns, x1, y1, z1, p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
                  colour_lookup[ColLU_WHITE], 5, SwvLF_JAGGED);
            break;
        case TT_RAZOR_WIRE:
        {
            ushort mask = (p_thing->SubType != 0) ? 0x1F : 0x7F;
            // the game shows other groups' wire only briefly
            if ((p_thing->U.UEffect.Group != p_dcthing->U.UPerson.EffectiveGroup)
              && ((gameturn & mask) != 0))
                break;
            swv_line_add(&lns, PRCCOORD_TO_MAPCOORD(p_thing->X),
              8 * (PRCCOORD_TO_YCOORD(p_thing->Y) >> 3), PRCCOORD_TO_MAPCOORD(p_thing->Z),
              p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
              colour_lookup[ColLU_GREYLT], 4, SwvLF_JAGGED);
            break;
        }
        case TT_LASER_GUIDED:
            swv_lines_guided_laser(&lns, p_thing);
            break;
        default:
            break;
        }
    }

    remain = sthings_used;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;

        if (--remain < 0)
            break;
        p_sthing = &sthings[thing];
        switch (p_sthing->Type)
        {
        case SmTT_SOUL:
            // build_soul() is a glare
            swv_glow_add(PRCCOORD_TO_MAPCOORD(p_sthing->X), PRCCOORD_TO_YCOORD(p_sthing->Y),
              PRCCOORD_TO_MAPCOORD(p_sthing->Z), 32 * 8, colour_lookup[ColLU_WHITE],
              0x04000000u | (ushort)thing);
            break;
        case SmTT_ELECTRIC_STRAND:
            swv_line_add(&lns, p_sthing->X >> 8, p_sthing->Y >> 8, p_sthing->Z >> 8,
              p_sthing->U.UEffect.VX, p_sthing->U.UEffect.VY, p_sthing->U.UEffect.VZ,
              colour_lookup[ColLU_WHITE], 5, SwvLF_JAGGED);
            break;
        case SmTT_SPARK:
        {
            // enlist_draw_mapwho_vect_len(): 8 units towards the old position
            float x1 = PRCCOORD_TO_MAPCOORD(p_sthing->X);
            float y1 = PRCCOORD_TO_YCOORD(p_sthing->Y) >> 3;
            float z1 = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
            float dx = p_sthing->U.UEffect.OX - x1;
            float dy = p_sthing->U.UEffect.OY - y1;
            float dz = p_sthing->U.UEffect.OZ - z1;
            float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist < 1.0f)
                dist = 1.0f;
            swv_line_add(&lns, x1, 8 * y1, z1, x1 + 8 * dx / dist, 8 * (y1 + 8 * dy / dist),
              z1 + 8 * dz / dist, p_sthing->Object & 0xFF, 4, SwvLF_GLOW);
            break;
        }
        case SmTT_BANG:
            if (p_sthing->State != 0)
            {
                float x = p_sthing->X >> 8, y = 8 * (p_sthing->Y >> 8), z = p_sthing->Z >> 8;
                swv_line_add(&lns, x, y, z, x, y + 8 * 400, z,
                  (p_sthing->State < 100) ? colour_lookup[ColLU_GREEN] : colour_lookup[ColLU_PINK],
                  16, SwvLF_GLOW);
            }
            break;
        case SmTT_STASIS_POD:
            swv_lines_stasis_pod(&lns, p_sthing);
            break;
        case SmTT_TIME_POD:
            swv_line_ring(&lns, p_sthing->X >> 8, 8 * (PRCCOORD_TO_MAPCOORD(p_sthing->Y) + 10),
              p_sthing->Z >> 8, (p_sthing->Radius >> 4) * 8.0f, colour_lookup[ColLU_BLUE], 8);
            break;
        case SmTT_NUCLEAR_BOMB:
            if ((p_sthing->Radius > 0) && ((p_sthing->Flag & TngF_InVehicle) != 0))
                swv_line_ring(&lns, PRCCOORD_TO_MAPCOORD(p_sthing->X),
                  8 * (PRCCOORD_TO_YCOORD(p_sthing->Y) >> 3), PRCCOORD_TO_MAPCOORD(p_sthing->Z),
                  p_sthing->Radius * 8.0f, colour_lookup[ColLU_WHITE], 12);
            break;
        default:
            break;
        }
    }
    p_hdr->n_lines = lns.count;
}



/** What the game's panels show: agents, weapons, objective. */
static void swv_export_hud(struct SwvHeader *p_hdr)
{
    struct SwvHud *p_hud;
    PlayerInfo *p_locplayer;
    int i;

    p_hud = (struct SwvHud *)(swv_block + SWV_OFF_HUD);
    p_locplayer = &players[local_player_no];
    p_hdr->selected = p_locplayer->DirectControl[0];
    p_hdr->view_scale = overall_scale;
    // as process_view_inputs(): the user can zoom out only as far as the
    // weapon's range allows, so the furthest is that of the longest range
    p_hdr->view_scale_min = get_scaled_zoom(get_zoom_from_range_bloks(ZOOM_RANGE_BLOCKS_LIMIT));
    // the desktop's projection, to know what its window shows
    p_hdr->view_tilt = engn_cam_tilt;
    p_hdr->view_perspective = (game_perspective == ProjM_Perspective);
    // the desktop's zoom and its range now, as process_view_inputs()
    {
        int far_zoom = CAMERA_ZOOM_MIN;
        ThingIdx dcthing = players[local_player_no].DirectControl[0];
        if ((dcthing > 0) && (things[dcthing].Type == TT_PERSON)) {
            int wz = get_weapon_zoom_min(things[dcthing].U.UPerson.CurrentWeapon);
            if (wz > far_zoom)
                far_zoom = wz;
        }
        p_hdr->view_zoom = (ingame.UserZoom > far_zoom) ? ingame.UserZoom : far_zoom;
        p_hdr->view_zoom_far = far_zoom;
        p_hdr->view_zoom_near = CAMERA_ZOOM_MAX;
        p_hdr->view_scale_256 = get_scaled_zoom(256);
    }
    if (vec_window_width != 0)
        p_hdr->view_size = ((u32)vec_window_width << 16) | ((u32)vec_window_height & 0xFFFF);
    else // the core alone: its view area is that of a 640x480 screen
        p_hdr->view_size = (640u << 16) | 480u;
    for (i = 0; i < SWV_AGENTS_MAX; i++)
    {
        struct SwvAgent *p_va = &p_hud->agents[i];
        struct Thing *p_agent = (i < AGENTS_SQUAD_MAX_COUNT) ? p_locplayer->MyAgent[i] : NULL;
        memset(p_va, 0, sizeof(*p_va));
        if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
            continue;
        p_va->thing = p_agent->ThingOffset;
        p_va->current_weapon = p_agent->U.UPerson.CurrentWeapon;
        p_va->weapons = p_agent->U.UPerson.WeaponsCarried;
        p_va->health = p_agent->Health;
        p_va->max_health = p_agent->U.UPerson.MaxHealth;
        if (p_agent->ThingOffset == p_locplayer->DirectControl[0])
            p_va->flags |= SwvTF_SELECTED;
        if ((p_agent->Flag & TngF_Destroyed) != 0 || (p_agent->State == PerSt_DEAD))
            p_va->flags |= SwvTF_DEAD;
    }
    memset(p_hud->objective, 0, sizeof(p_hud->objective));
    if (scrollinfo_text != NULL)
        strncpy(p_hud->objective, scrollinfo_text, sizeof(p_hud->objective) - 1);
    for (i = 0; i < SWV_WEAPON_TYPES; i++)
    {
        const char *name = (i < WEP_TYPES_COUNT) ? weapon_codename(i) : "";
        memset(p_hud->weapon_names[i], 0, sizeof(p_hud->weapon_names[i]));
        if (name != NULL)
            strncpy(p_hud->weapon_names[i], name, sizeof(p_hud->weapon_names[i]) - 1);
    }
}

/** Header fields of a new block. */
static void swv_block_init(struct SwvHeader *p_hdr)
{
    uint32_t seq;

    seq = p_hdr->seq;
    memset(p_hdr, 0, sizeof(*p_hdr));
    p_hdr->seq = seq;
    p_hdr->magic = SWV_MAGIC;
    p_hdr->version = SWV_VERSION;
    p_hdr->total_size = SWV_TOTAL_SIZE;
    p_hdr->off_static = SWV_OFF_STATIC;
    p_hdr->off_dyn = SWV_OFF_DYN;
    p_hdr->off_things = SWV_OFF_THINGS;
    p_hdr->off_tmap = SWV_OFF_TMAP;
    p_hdr->off_palette = SWV_OFF_PALETTE;
}

static void swv_fx_scan(void);

void swview_update(void *p_block)
{
    struct SwvHeader *p_hdr;
    struct VrMesh dyn;
    TbBool in_mission;

    swv_block = p_block;
    p_hdr = (struct SwvHeader *)swv_block;
    in_mission = (ingame.DisplayMode == DpM_ENGINEPLY) && (game_my_big_map != NULL);

    if (p_hdr->magic != SWV_MAGIC)
        swv_block_init(p_hdr);
    p_hdr->gameturn = gameturn;
    p_hdr->in_mission = in_mission;
    if (in_mission)
    {
        if ((swv_last_map != current_map) || (swv_tmap_rev == 0)
          || ((gameturn - swv_static_turn) >= 8 * VR_STATIC_PERIOD))
            swv_export_tmap(p_hdr);
        if ((swv_last_map != current_map) || (swv_sprite_rev == 0))
            swv_export_sprites(p_hdr);
        {
            u32 sig = swv_world_signature();
            if ((swv_last_map != current_map) || (swv_static_rev == 0) || (sig != swv_world_hash)
              || ((gameturn - swv_static_turn) >= VR_STATIC_PERIOD))
            {
                swv_export_static(p_hdr);
                swv_static_turn = gameturn;
                swv_world_hash = sig;
            }
        }
        swv_last_map = current_map;
        p_hdr->map_no = current_map;

        dyn.verts = (struct SwvVert *)(swv_block + SWV_OFF_DYN);
        dyn.count = 0;
        dyn.limit = SWV_DYN_VERTS_MAX;
        swv_lists.p_groups = (struct SwvDynGroup *)(swv_block + SWV_OFF_DYN_GROUPS);
        swv_lists.p_bars = (struct SwvBar *)(swv_block + SWV_OFF_BARS);
        swv_lists.p_glows = (struct SwvGlow *)(swv_block + SWV_OFF_GLOWS);
        swv_lists.p_shadows = (struct SwvShadow *)(swv_block + SWV_OFF_SHADOWS);
        swv_lists.n_groups = swv_lists.n_bars = swv_lists.n_glows = swv_lists.n_shadows = 0;
        swv_export_things(p_hdr, &dyn);
        swv_mesh_explode_faces(&dyn);
        swv_export_effects(p_hdr, &dyn);
        swv_export_lines(p_hdr);
        p_hdr->n_dyn_groups = swv_lists.n_groups;
        p_hdr->n_bars = swv_lists.n_bars;
        p_hdr->n_glows = swv_lists.n_glows;
        p_hdr->n_shadows = swv_lists.n_shadows;
        swv_export_hud(p_hdr);
        p_hdr->n_dyn = dyn.count;
        swv_fx_scan();

        p_hdr->cam_x = engn_xc;
        p_hdr->cam_y = ENGINE_Y_TO_MAP(8 * engn_yc);
        p_hdr->cam_z = engn_zc;
        // bflib angle units x 32, 2048 units per full turn
        p_hdr->cam_angle = (float)engn_cam_yaw / 32.0f * (6.2831853f / 2048.0f);
    }
}

/******************************************************************************/
uint32_t swview_block_size(void)
{
    return SWV_TOTAL_SIZE;
}

void swview_reset(void)
{
    swv_static_rev = 0;
    swv_tmap_rev = 0;
    swv_sprite_rev = 0;
}

/******************************************************************************/
/* Assets */

int32_t swview_load_assets(void)
{
    char locstr[64];
    int page;
    s32 len;
    int32_t missing;

    swview_free_assets();
    missing = 0;
    for (page = 0; page < SWV_TMAP_PAGES; page++)
    {
        sprintf(locstr, "data/tex%02d.dat", page);
        len = LbFileLength(locstr);
        if (len <= 0)
            continue;
        swv_own_tmap[page] = LbMemoryAlloc(SWV_TMAP_DIM * SWV_TMAP_DIM);
        if (swv_own_tmap[page] == NULL)
            return -1;
        memset(swv_own_tmap[page], 0, SWV_TMAP_DIM * SWV_TMAP_DIM);
        if (len > SWV_TMAP_DIM * SWV_TMAP_DIM)
            len = SWV_TMAP_DIM * SWV_TMAP_DIM;
        if (LbFileLoadAt(locstr, swv_own_tmap[page]) == -1)
            missing++;
    }

    // the multicolour sprites of the mission (people, effects, items)
    sprintf(locstr, "data/mspr-%d.dat", (int)ingame.TrenchcoatPreference);
    len = LbFileLength(locstr);
    if (len > 0) {
        swv_own_sprites_data = LbMemoryAlloc(len + 512);
        if ((swv_own_sprites_data == NULL) || (LbFileLoadAt(locstr, swv_own_sprites_data) == -1))
            missing++;
    } else {
        missing++;
    }
    sprintf(locstr, "data/mspr-%d.tab", (int)ingame.TrenchcoatPreference);
    len = LbFileLength(locstr);
    if ((len > 0) && (swv_own_sprites_data != NULL)) {
        swv_own_sprites = LbMemoryAlloc(LB_SPRITE_TAB_MEM_SIZE(len));
        if ((swv_own_sprites != NULL) && (LbFileLoadAt(locstr, swv_own_sprites) != -1)) {
            len = LbSpriteTabExpand(swv_own_sprites, len);
            swv_own_sprites_end = (struct TbSprite *)((ubyte *)swv_own_sprites + len);
            LbSpriteSetup(swv_own_sprites, swv_own_sprites_end, swv_own_sprites_data);
        } else {
            missing++;
        }
    } else {
        missing++;
    }
    if ((swv_own_sprites_end == NULL) && (swv_own_sprites != NULL)) {
        LbMemoryFree(swv_own_sprites);
        swv_own_sprites = NULL;
    }
    swview_reset();
    if (missing != 0)
        LOGWARN("%d asset files could not be loaded", (int)missing);
    return missing;
}

int32_t swview_load_font(int32_t style, int32_t detail, struct SwvGlyph *p_glyphs, uint32_t max,
    uint8_t *p_page)
{
    char locstr[64];
    struct TbSprite *p_tab, *p_tab_end;
    ubyte *p_dat;
    s32 len_tab, len_dat;
    int32_t n, i, px, py, shelf_h;

    sprintf(locstr, "data/fontc%d-%d.tab", (int)style, (int)detail);
    len_tab = LbFileLength(locstr);
    sprintf(locstr, "data/fontc%d-%d.dat", (int)style, (int)detail);
    len_dat = LbFileLength(locstr);
    if ((len_tab <= 0) || (len_dat <= 0))
        return -1;
    p_dat = LbMemoryAlloc(len_dat + 512);
    p_tab = LbMemoryAlloc(LB_SPRITE_TAB_MEM_SIZE(len_tab));
    if ((p_dat == NULL) || (p_tab == NULL) || (LbFileLoadAt(locstr, p_dat) == -1)) {
        n = -1;
        goto done;
    }
    sprintf(locstr, "data/fontc%d-%d.tab", (int)style, (int)detail);
    if (LbFileLoadAt(locstr, p_tab) == -1) {
        n = -1;
        goto done;
    }
    len_tab = LbSpriteTabExpand(p_tab, len_tab);
    p_tab_end = (struct TbSprite *)((ubyte *)p_tab + len_tab);
    LbSpriteSetup(p_tab, p_tab_end, p_dat);

    memset(p_page, 0, 2 * SWV_FONT_PAGE_DIM * SWV_FONT_PAGE_DIM);
    n = (int32_t)(p_tab_end - p_tab);
    if (n > (int32_t)max)
        n = (int32_t)max;
    // shelf packing, with a texel between characters
    px = py = shelf_h = 0;
    for (i = 0; i < n; i++)
    {
        const struct TbSprite *p_spr = &p_tab[i];
        struct SwvGlyph *p_gl = &p_glyphs[i];
        const sbyte *p_src;
        int x, y;

        memset(p_gl, 0, sizeof(*p_gl));
        p_gl->w = p_spr->SWidth;
        p_gl->h = p_spr->SHeight;
        if ((p_spr->SWidth == 0) || (p_spr->SHeight == 0) || (p_spr->Data == NULL))
            continue;
        if (px + p_spr->SWidth + 1 > SWV_FONT_PAGE_DIM) {
            px = 0;
            py += shelf_h + 1;
            shelf_h = 0;
        }
        if (py + p_spr->SHeight > SWV_FONT_PAGE_DIM)
            break;
        p_gl->x = px;
        p_gl->y = py;
        // the RLE sprite format, as swv_decode_sprite(), one byte texels
        p_src = (const sbyte *)p_spr->Data;
        for (y = 0; y < p_spr->SHeight; y++)
        {
            ubyte *p_row = p_page + 2 * (SWV_FONT_PAGE_DIM * (py + y) + px);
            x = 0;
            while (1)
            {
                sbyte c = *p_src++;
                if (c == 0)
                    break;
                if (c < 0) {
                    x -= c;
                    continue;
                }
                while (c-- > 0) {
                    ubyte col = (ubyte)*p_src++;
                    if (x < p_spr->SWidth) {
                        p_row[2 * x + 0] = col;
                        p_row[2 * x + 1] = 255;
                    }
                    x++;
                }
            }
        }
        px += p_spr->SWidth + 1;
        if (p_spr->SHeight > shelf_h)
            shelf_h = p_spr->SHeight;
    }
done:
    if (p_dat != NULL)
        LbMemoryFree(p_dat);
    if (p_tab != NULL)
        LbMemoryFree(p_tab);
    return n;
}

void swview_free_assets(void)
{
    int page;

    for (page = 0; page < SWV_TMAP_PAGES; page++) {
        if (swv_own_tmap[page] != NULL)
            LbMemoryFree(swv_own_tmap[page]);
        swv_own_tmap[page] = NULL;
    }
    if (swv_own_sprites != NULL)
        LbMemoryFree(swv_own_sprites);
    if (swv_own_sprites_data != NULL)
        LbMemoryFree(swv_own_sprites_data);
    swv_own_sprites = swv_own_sprites_end = NULL;
    swv_own_sprites_data = NULL;
}

/******************************************************************************/
/* What a renderer with its own models needs: the state of every person */

/** Steps from the first frame of the person's current animation (in its
 * own direction) to its current frame. */
static uint16_t swv_frame_in_anim(const struct Thing *p_person)
{
    uint idx;
    ushort frm;
    uint16_t n;

    if ((nstart_ani == NULL) || (frame == NULL))
        return 0;
    idx = p_person->StartFrame + 1 + (p_person->U.UPerson.Angle & 7);
    if (idx >= (uint)(nstart_ani_end - nstart_ani))
        return 0;
    frm = nstart_ani[idx];
    for (n = 0; n < 256; n++)
    {
        if (frm == (ushort)p_person->Frame)
            return n;
        if ((frm == 0) || (frm >= (uint)(frame_end - frame)))
            break;
        frm = frame[frm].Next;
        if (frm == nstart_ani[idx])
            break;
    }
    return 0;
}

uint32_t swview_persons_render(struct SwvPersonRender *p_list, uint32_t max)
{
    PlayerInfo *p_locplayer;
    ThingIdx thing;
    uint32_t n;

    n = 0;
    if (things == NULL)
        return 0;
    p_locplayer = &players[local_player_no];
    for (thing = 1; (thing < THINGS_LIMIT) && (n < max); thing++)
    {
        struct Thing *p_thing;
        struct SwvPersonRender *p_pr;
        short cor_x, cor_y, cor_z;

        p_thing = &things[thing];
        if (p_thing->Type != TT_PERSON)
            continue;
        p_pr = &p_list[n++];
        memset(p_pr, 0, sizeof(*p_pr));
        get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
        p_pr->x = cor_x;
        p_pr->y = ENGINE_Y_TO_MAP(cor_y);
        p_pr->z = cor_z;
        p_pr->thing = thing;
        p_pr->subtype = p_thing->SubType;
        p_pr->anim_mode = p_thing->U.UPerson.AnimMode;
        p_pr->weapon = p_thing->U.UPerson.CurrentWeapon;
        p_pr->angle = (p_thing->U.UPerson.Angle & 7) * 256;
        p_pr->shield_glow = (uint8_t)person_shield_glow_brightness(p_thing);
        p_pr->frame_in_anim = swv_frame_in_anim(p_thing);
        p_pr->health = p_thing->Health;
        p_pr->max_health = p_thing->U.UPerson.MaxHealth;
        if ((p_thing->Flag & TngF_Unkn00200000) != 0)
            p_pr->flags |= SwvPF_SHIELD;
        if ((p_thing->Flag & TngF_Persuaded) != 0) {
            p_pr->flags |= SwvPF_PERSUADED;
            if ((p_thing->Owner > 0) && (p_thing->Owner < THINGS_LIMIT))
                p_pr->persuader = p_thing->Owner;
        }
        if ((p_pr->anim_mode == 10) || (p_pr->anim_mode == 17))
            p_pr->flags |= SwvPF_BURNING;
        if ((p_thing->Flag2 & TgF2_InsideBuilding) != 0)
            p_pr->flags |= SwvPF_IN_BUILDING;
        if ((p_thing->Flag & TngF_InVehicle) != 0)
            p_pr->flags |= SwvPF_IN_VEHICLE;
        if ((p_thing->State == PerSt_DEAD) || ((p_thing->Flag & TngF_Destroyed) != 0))
            p_pr->flags |= SwvPF_DEAD;
        if (swv_is_player_agent(p_thing))
            p_pr->flags |= SwvPF_PLAYER_AGENT;
        if (thing == p_locplayer->DirectControl[0])
            p_pr->flags |= SwvPF_SELECTED;
    }
    return n;
}

/******************************************************************************/
/* Effects which started or ended in the last turn */

#define SWV_FX_TRACK_MAX 1024

struct SwvFxTrack {
    uint16_t uid;
    uint8_t type;
    uint8_t pad;
};

static struct SwvFxTrack swv_fx_prev[SWV_FX_TRACK_MAX];
static uint swv_fx_prev_count;
static struct SwvFxEvent swv_fx_events[SWV_FX_EVENTS_MAX];
static uint swv_fx_events_count;

static TbBool swv_fx_tracked(ubyte type)
{
    return (type == SmTT_BANG) || (type == SmTT_FIRE) || (type == SmTT_SCALE_EFFECT)
      || (type == SmTT_SMOKE_GENERATOR);
}

static void swv_fx_event(ubyte kind, const struct SimpleThing *p_sthing, uint8_t size, uint16_t anim)
{
    struct SwvFxEvent *p_ev;

    if (swv_fx_events_count >= SWV_FX_EVENTS_MAX)
        return;
    p_ev = &swv_fx_events[swv_fx_events_count++];
    p_ev->kind = kind;
    p_ev->size = size;
    p_ev->anim = anim;
    p_ev->id = p_sthing->UniqueID;
    p_ev->x = PRCCOORD_TO_MAPCOORD(p_sthing->X);
    p_ev->y = ENGINE_Y_TO_MAP(PRCCOORD_TO_YCOORD(p_sthing->Y));
    p_ev->z = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
}

static TbBool swv_fx_was(const struct SwvFxTrack *p_list, uint count, uint16_t uid, ubyte type)
{
    uint i;

    for (i = 0; i < count; i++) {
        if ((p_list[i].uid == uid) && (p_list[i].type == type))
            return true;
    }
    return false;
}

/** Bang size: the smoke puffs and shards it made, 1 small .. 3 large. */
static uint8_t swv_bang_size(const struct SimpleThing *p_sthing)
{
    int phw, n, guard;

    n = 0;
    guard = 0;
    for (phw = p_sthing->U.UBang.phwoar; (phw > 0) && (phw < PHWOAR_COUNT) && (guard < PHWOAR_COUNT);
      phw = phwoar[phw].child, guard++)
        n++;
    return (n > 24) ? 3 : (n > 8) ? 2 : 1;
}

static void swv_fx_scan(void)
{
    static struct SwvFxTrack cur[SWV_FX_TRACK_MAX];
    static struct SimpleThing prev_sthings[SWV_FX_TRACK_MAX];
    uint cur_count, i;
    ThingIdx thing;
    int remain;

    swv_fx_events_count = 0;
    cur_count = 0;
    remain = sthings_used;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;

        if (--remain < 0)
            break;
        p_sthing = &sthings[thing];
        if (!swv_fx_tracked(p_sthing->Type))
            continue;
        if (cur_count < SWV_FX_TRACK_MAX) {
            cur[cur_count].uid = p_sthing->UniqueID;
            cur[cur_count].type = p_sthing->Type;
            cur[cur_count].pad = 0;
            prev_sthings[cur_count] = *p_sthing;
            cur_count++;
        }
        if (swv_fx_was(swv_fx_prev, swv_fx_prev_count, p_sthing->UniqueID, p_sthing->Type))
            continue;
        switch (p_sthing->Type)
        {
        case SmTT_BANG:
            swv_fx_event(SwvFX_BANG, p_sthing, swv_bang_size(p_sthing), 0);
            break;
        case SmTT_FIRE:
            swv_fx_event(SwvFX_FIRE_START, p_sthing, 0, 0);
            break;
        case SmTT_SCALE_EFFECT:
            swv_fx_event(SwvFX_SCALE_EFFECT, p_sthing, 0, p_sthing->StartFrame);
            break;
        case SmTT_SMOKE_GENERATOR:
            swv_fx_event(SwvFX_SMOKE, p_sthing, 0, 0);
            break;
        default:
            break;
        }
    }
    // fires which went out: in the last list, not in this one
    {
        static struct SimpleThing last_sthings[SWV_FX_TRACK_MAX];
        static uint last_count;
        for (i = 0; i < swv_fx_prev_count && i < last_count; i++)
        {
            if (swv_fx_prev[i].type != SmTT_FIRE)
                continue;
            if (!swv_fx_was(cur, cur_count, swv_fx_prev[i].uid, SmTT_FIRE))
                swv_fx_event(SwvFX_FIRE_END, &last_sthings[i], 0, 0);
        }
        memcpy(last_sthings, prev_sthings, cur_count * sizeof(struct SimpleThing));
        last_count = cur_count;
    }
    memcpy(swv_fx_prev, cur, cur_count * sizeof(struct SwvFxTrack));
    swv_fx_prev_count = cur_count;
}

uint32_t swview_fx_events(struct SwvFxEvent *p_list, uint32_t max)
{
    uint32_t n;

    n = (swv_fx_events_count < max) ? swv_fx_events_count : max;
    memcpy(p_list, swv_fx_events, n * sizeof(struct SwvFxEvent));
    return n;
}

/******************************************************************************/
/* The panels' data, for a front end's own layout */

extern ushort signal_count;

void swview_hud_info(struct SwvHudInfo *p_info)
{
    struct SwcWorldInfo winfo;
    PlayerInfo *p_locplayer;
    int i;

    memset(p_info, 0, sizeof(*p_info));
    swcore_world_info(&winfo);
    p_info->mission_status = winfo.mission_status;
    p_info->in_mission = winfo.in_mission;
    if (things == NULL)
        return;
    p_locplayer = &players[local_player_no];
    p_info->selected = p_locplayer->DirectControl[0];
    for (i = 0; (i < SWV_AGENTS_MAX) && (i < AGENTS_SQUAD_MAX_COUNT); i++)
    {
        struct SwvHudAgent *p_ha = &p_info->agents[i];
        struct Thing *p_agent = p_locplayer->MyAgent[i];

        if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
            continue;
        p_ha->thing = p_agent->ThingOffset;
        p_ha->current_weapon = p_agent->U.UPerson.CurrentWeapon;
        p_ha->weapons = p_agent->U.UPerson.WeaponsCarried;
        p_ha->health = p_agent->Health;
        p_ha->max_health = p_agent->U.UPerson.MaxHealth;
        p_ha->energy = p_agent->U.UPerson.Energy;
        p_ha->max_energy = p_agent->U.UPerson.MaxEnergy;
        p_ha->shield = p_agent->U.UPerson.ShieldEnergy;
        p_ha->max_shield = p_agent->U.UPerson.MaxShieldEnergy;
        p_ha->weapon_timer = p_agent->U.UPerson.WeaponTimer;
        p_ha->mods = p_agent->U.UPerson.UMod.Mods;
        if (p_agent->PTarget != NULL)
            p_ha->target = p_agent->PTarget->ThingOffset;
        if (p_agent->ThingOffset == p_locplayer->DirectControl[0])
            p_ha->flags |= SwvTF_SELECTED;
        if (((p_agent->Flag & TngF_Destroyed) != 0) || (p_agent->State == PerSt_DEAD))
            p_ha->flags |= SwvTF_DEAD;
    }
    // where the scanner points: the places of the current objective
    for (i = 0; (i < signal_count) && (i < SCANNER_BIG_BLIP_COUNT) && (i < SWV_SIGNALS_MAX); i++)
    {
        p_info->signals[i].x = PRCCOORD_TO_MAPCOORD(ingame.Scanner.BigBlip[i].X);
        p_info->signals[i].z = PRCCOORD_TO_MAPCOORD(ingame.Scanner.BigBlip[i].Z);
        p_info->signals[i].colour = ingame.Scanner.BigBlip[i].Colour;
    }
    p_info->n_signals = i;
    if (scrollinfo_text != NULL)
        strncpy(p_info->objective, scrollinfo_text, sizeof(p_info->objective) - 1);
}

static void swv_prop_add(struct SwvProp *p_list, uint32_t max, uint32_t *p_n, float x, float y, float z,
  uint32_t id, ubyte kind, ubyte weapon, ushort anim)
{
    struct SwvProp *p_prop;

    if (*p_n >= max)
        return;
    p_prop = &p_list[(*p_n)++];
    p_prop->x = x;
    p_prop->y = y;
    p_prop->z = z;
    p_prop->id = id;
    p_prop->kind = kind;
    p_prop->weapon = weapon;
    p_prop->anim = anim;
}

uint32_t swview_props(struct SwvProp *p_list, uint32_t max)
{
    ThingIdx thing;
    uint32_t n;
    int remain;

    n = 0;
    if ((things == NULL) || (sthings == NULL))
        return 0;
    // the same things, places and ids as swv_export_effects()
    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing;
        short cor_x, cor_y, cor_z;
        ubyte wtype;

        p_thing = &things[thing];
        if ((p_thing->Type != TT_MINE) && (p_thing->Type != TT_GRENADE))
            continue;
        get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
        wtype = (p_thing->SubType < WEP_TYPES_COUNT) ? p_thing->SubType : 0;
        swv_prop_add(p_list, max, &n, cor_x, cor_y, cor_z, thing, SwvEK_GRENADE, wtype, 0);
    }
    remain = sthings_used;
    for (thing = sthings_used_head; thing < 0; thing = sthings[thing].LinkChild)
    {
        struct SimpleThing *p_sthing;
        float x, y, z;

        if (--remain < 0)
            break;
        p_sthing = &sthings[thing];
        x = PRCCOORD_TO_MAPCOORD(p_sthing->X);
        y = PRCCOORD_TO_YCOORD(p_sthing->Y);
        z = PRCCOORD_TO_MAPCOORD(p_sthing->Z);
        if (p_sthing->Type == SmTT_STATIC)
        {
            if (p_sthing->Frame == nstart_ani[1008])
                continue;
            swv_prop_add(p_list, max, &n, x, y, z, 0x01000000u | (ushort)thing, SwvEK_STATIC, 0,
              (ushort)(p_sthing->StartFrame + 1));
        }
        else if (p_sthing->Type == SmTT_DROPPED_ITEM)
        {
            ushort wtype = (ushort)p_sthing->U.UWeapon.WeaponType;
            swv_prop_add(p_list, max, &n, x, y, z, 0x01000000u | (ushort)thing, SwvEK_DROPPED_ITEM,
              (wtype < WEP_TYPES_COUNT) ? (ubyte)wtype : 0, 0);
        }
    }
    return n;
}

uint32_t swview_charges(struct SwvCharge *p_list, uint32_t max)
{
    PlayerInfo *p_locplayer;
    uint32_t n;
    int i;

    n = 0;
    if (things == NULL)
        return 0;
    p_locplayer = &players[local_player_no];
    for (i = 0; i < AGENTS_SQUAD_MAX_COUNT; i++)
    {
        struct Thing *p_agent = p_locplayer->MyAgent[i];
        WeaponType wtype;

        if ((p_agent == NULL) || (p_agent->Type != TT_PERSON) || (n >= max))
            continue;
        wtype = p_agent->U.UPerson.CurrentWeapon;
        if (((p_agent->Flag & TngF_WepCharging) == 0) || !weapon_can_be_charged(wtype))
            continue;
        // the charge stops growing at 15 (weapon.c, while TngF_WepCharging)
        p_list[n].thing = p_agent->ThingOffset;
        p_list[n].level = (ubyte)((p_agent->U.UPerson.WeaponTimer < 15) ? p_agent->U.UPerson.WeaponTimer : 15);
        p_list[n].max = 15;
        n++;
    }
    return n;
}

/******************************************************************************/
