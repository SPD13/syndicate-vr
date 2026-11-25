/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file vrspectate.c
 *     Publishing the game world for the external VR spectator viewer.
 * @par Purpose:
 *     Every game turn, converts the terrain, building and vehicle geometry
 *     and the people into a simple triangle list in shared memory, where
 *     the VR viewer (vr/viewer) reads it. See vrshare.h for the layout.
 * @par Comment:
 *     Read-only with regard to the game state. Windows only for now.
 * @author   SyndicateVR
 * @date     06 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "vrspectate.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "bfmath.h"
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
#include "vrshare.h"
#include "swlog.h"
#if defined(WIN32) || defined(_WIN32)
# include <windows.h>
#endif
/******************************************************************************/

/** How often static geometry (terrain and buildings) is re-sent, in turns. */
#define VR_STATIC_PERIOD 16

static TbBool vr_enabled;
static TbBool vr_env_checked;
static ubyte *vr_block;
static uint vr_static_turn;
static uint vr_static_rev;
static uint vr_tmap_rev;
static ushort vr_last_map;
static uint vr_sprite_rev;
static u32 vr_world_hash;
static struct VrsCmdQueue *vr_cmds;

/** Lists filled each turn besides the triangle meshes. */
struct VrLists {
    struct VrsDynGroup *p_groups;
    uint n_groups;
    struct VrsBar *p_bars;
    uint n_bars;
    struct VrsGlow *p_glows;
    uint n_glows;
    struct VrsShadow *p_shadows;
    uint n_shadows;
};
static struct VrLists vr_lists;

extern struct CarGlare car_glare[];
extern ubyte master_glare[][2];

extern ubyte byte_152EF0[];

// animation tables are copied as they are
_Static_assert(sizeof(struct Frame) == sizeof(struct VrsFrame), "VrsFrame layout");
_Static_assert(sizeof(struct Element) == sizeof(struct VrsElement), "VrsElement layout");

/* The renderer projects engine Y on the same scale as map X/Z, see
 * transform_shpoint(); only terrain Alt is 8 times coarser. */
#define ENGINE_Y_TO_MAP(y) ((float)(y))

void vr_spectator_enable(TbBool enable)
{
    vr_enabled = enable;
    vr_env_checked = true;
}

static TbBool vr_open_block(void)
{
#if defined(WIN32) || defined(_WIN32)
    HANDLE hmap;
    struct VrsHeader *p_hdr;

    hmap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      0, VRS_TOTAL_SIZE, VRS_MAPPING_NAME);
    if (hmap == NULL) {
        LOGERR("cannot create shared memory, error %lu", (ulong)GetLastError());
        return false;
    }
    vr_block = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, VRS_TOTAL_SIZE);
    if (vr_block == NULL) {
        LOGERR("cannot map shared memory, error %lu", (ulong)GetLastError());
        return false;
    }
    p_hdr = (struct VrsHeader *)vr_block;
    memset(p_hdr, 0, sizeof(*p_hdr));
    p_hdr->magic = VRS_MAGIC;
    p_hdr->version = VRS_VERSION;
    p_hdr->total_size = VRS_TOTAL_SIZE;
    p_hdr->off_static = VRS_OFF_STATIC;
    p_hdr->off_dyn = VRS_OFF_DYN;
    p_hdr->off_things = VRS_OFF_THINGS;
    p_hdr->off_tmap = VRS_OFF_TMAP;
    p_hdr->off_palette = VRS_OFF_PALETTE;
    LOGSYNC_F("VR spectator shared memory '%s' ready, %u bytes",
      VRS_MAPPING_NAME, (uint)VRS_TOTAL_SIZE);

    hmap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
      0, sizeof(struct VrsCmdQueue), VRS_CMD_MAPPING_NAME);
    if (hmap != NULL)
        vr_cmds = MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(struct VrsCmdQueue));
    if (vr_cmds != NULL) {
        memset(vr_cmds, 0, sizeof(*vr_cmds));
        vr_cmds->magic = VRS_CMD_MAGIC;
    } else {
        LOGWARN("no command channel, the viewer can only watch");
    }
    return true;
#else
    return false;
#endif
}

/******************************************************************************/
/* Triangle list building */

struct VrMesh {
    struct VrsVert *verts;
    uint count;
    uint limit;
};

/** Light level of the map at a tile corner, as the floor drawing computes
 * it from the ambient light and the static lights; 32 is unchanged colours.
 */
static int vr_corner_light(int tile_x, int tile_z)
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
static ubyte vr_point_shade(float x, float z)
{
    int level;

    level = vr_corner_light((int)(x + 128) >> 8, (int)(z + 128) >> 8);
    return (level > 63) ? 252 : level * 4;
}

static void vr_mesh_vert(struct VrMesh *p_mesh, float x, float y, float z,
  int u, int v, ubyte page, ubyte colour, ubyte shade, ubyte flags)
{
    struct VrsVert *p_vert;

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

static void vr_point_pos(const struct VrPointXform *p_xf, const struct SingleObject *p_sobj,
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

static void vr_mesh_object(struct VrMesh *p_mesh, ushort obj, const struct VrPointXform *p_xf)
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
            vr_point_pos(p_xf, p_sobj, p_face->PointNo[k], &x[k], &y[k], &z[k]);
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
            flags |= VrsVF_TEXTURED;
        }
        if ((p_face->Flags >= 12) && (p_face->Flags <= 26))
            flags |= VrsVF_TRANSLUCENT;
        if ((p_face->GFlags & FGFlg_Unkn80) != 0)
            flags |= VrsVF_REFLECTIVE;
        for (k = 0; k < 3; k++)
            vr_mesh_vert(p_mesh, x[k], y[k], z[k], uv[k][0], uv[k][1],
              page, p_face->ExCol, vr_point_shade(x[k], z[k]), flags);
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
            vr_point_pos(p_xf, p_sobj, p_face4->PointNo[k], &x[k], &y[k], &z[k]);
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
            flags |= VrsVF_TEXTURED;
        }
        if ((p_face4->Flags >= 12) && (p_face4->Flags <= 26))
            flags |= VrsVF_TRANSLUCENT;
        if ((p_face4->GFlags & FGFlg_Unkn80) != 0)
            flags |= VrsVF_REFLECTIVE;
        for (k = 0; k < 6; k++) {
            int n = quad_tris[k];
            vr_mesh_vert(p_mesh, x[n], y[n], z[n], uv[n][0], uv[n][1],
              page, p_face4->ExCol, vr_point_shade(x[n], z[n]), flags);
        }
    }
}

static void vr_mesh_terrain(struct VrMesh *p_mesh)
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
                    flags |= VrsVF_TEXTURED;
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
                    vflags |= VrsVF_WOBBLE;
                else if ((p_crnel->Flags & MEF1_Unkn40) != 0)
                    vflags |= VrsVF_WAFT;
                level = vr_corner_light(tile_x + crn_dx[n], tile_z + crn_dz[n]);
                shade = (level > 63) ? 252 : level * 4;
                vr_mesh_vert(p_mesh,
                  (float)TILE_TO_MAPCOORD(tile_x + crn_dx[n], 0), y[n],
                  (float)TILE_TO_MAPCOORD(tile_z + crn_dz[n], 0),
                  uv[n][0], uv[n][1], page, colour_grey2, shade, vflags);
            }
        }
    }
}

/******************************************************************************/

static TbBool vr_is_player_agent(struct Thing *p_thing)
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
static void vr_mesh_mech_part(short part, const struct M33 *p_rmat,
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
    vr_mesh_object((struct VrMesh *)ctx, things[part].U.UObject.Object, &xf);
}

/** Adds a rocket, with the orientation build_rocket() computes from its
 * velocity; the game only sets that up while drawing.
 */
static void vr_mesh_rocket(struct VrMesh *p_mesh, struct Thing *p_thing)
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
    vr_mesh_object(p_mesh, obj, &xf);
}

/** Sprite frame selection of a person, following build_person(); the
 * viewer applies its own view direction.
 */
static void vr_person_anim(struct VrsThing *p_vthing, struct Thing *p_thing)
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
    p_vthing->anim_mode = VrsAM_NONE;

    if ((p_thing->Flag2 & TgF2_InsideBuilding) != 0)
        p_vthing->flags |= VrsTF_HIDDEN;

    if (p_thing->State == PerSt_BEING_PERSUADED)
    {
        p_vthing->anim_mode = VrsAM_FIXED;
        p_vthing->anim_start = p_thing->Frame;
        p_vthing->brightness = PALETTE_FADE_LEVELS / 2;
    }
    else if (p_thing->U.UPerson.AnimMode == ANIM_PERS_DEAD_BODY)
    {
        p_vthing->anim_mode = VrsAM_DIR12;
        p_vthing->anim_start = p_thing->StartFrame;
        p_vthing->subframe = get_person_anim_subframe(p_thing);
    }
    else if ((p_thing->Flag & TngF_Unkn02000000) != 0)
    {
        p_vthing->flags |= VrsTF_HIDDEN;
    }
    else if (p_thing->StartFrame == 1066)
    {
        p_vthing->anim_mode = VrsAM_FIXED;
        p_vthing->anim_start = p_thing->Frame;
    }
    else
    {
        p_vthing->anim_mode = VrsAM_DIR8;
        p_vthing->anim_start = p_thing->StartFrame;
        p_vthing->subframe = get_person_anim_subframe(p_thing);
    }
}

static void vr_group_begin(struct VrMesh *p_dyn, uint *p_first)
{
    *p_first = p_dyn->count;
}

static void vr_group_end(struct VrMesh *p_dyn, uint first, u32 id)
{
    struct VrsDynGroup *p_grp;

    if ((p_dyn->count == first) || (vr_lists.n_groups >= VRS_DYN_GROUPS_MAX))
        return;
    p_grp = &vr_lists.p_groups[vr_lists.n_groups++];
    p_grp->id = id;
    p_grp->first = first;
    p_grp->count = p_dyn->count - first;
}

static void vr_bar_add(float x, float y, float z, int health, int max_health, u32 id)
{
    struct VrsBar *p_bar;

    if ((max_health <= 0) || (health >= max_health) || (vr_lists.n_bars >= VRS_BARS_MAX))
        return;
    p_bar = &vr_lists.p_bars[vr_lists.n_bars++];
    p_bar->x = x;
    p_bar->y = y;
    p_bar->z = z;
    p_bar->frac = (health > 0) ? (float)health / max_health : 0.0f;
    p_bar->id = id;
}

static void vr_glow_add(float x, float y, float z, float radius, ubyte colour, u32 id)
{
    struct VrsGlow *p_glow;

    if (vr_lists.n_glows >= VRS_GLOWS_MAX)
        return;
    p_glow = &vr_lists.p_glows[vr_lists.n_glows++];
    p_glow->x = x;
    p_glow->y = y;
    p_glow->z = z;
    p_glow->radius = radius;
    p_glow->colour = colour;
    p_glow->pad[0] = p_glow->pad[1] = p_glow->pad[2] = 0;
    p_glow->id = id;
}

/** Ground height under a point, engine units, from the terrain corners. */
static float vr_ground_y(float x, float z)
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
static void vr_vehicle_shadow(struct Thing *p_thing, const struct VrPointXform *p_xf, u32 id)
{
    struct SingleObject *p_sobj;
    struct VrsShadow *p_shd;
    int min_x = 32767, max_x = -32768, min_z = 32767, max_z = -32768;
    int i, k;
    ushort obj;

    obj = p_thing->U.UVehicle.Object;
    if ((obj == 0) || (obj >= next_object) || (vr_lists.n_shadows >= VRS_SHADOWS_MAX))
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
    p_shd = &vr_lists.p_shadows[vr_lists.n_shadows++];
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
    p_shd->y = vr_ground_y(p_xf->ox, p_xf->oz);
    if (p_shd->y > p_xf->oy)
        p_shd->y = p_xf->oy;
    p_shd->id = id;
}

/** Headlight glare points of a car, as do_car_glare(). */
static void vr_car_glare(struct Thing *p_car, const struct VrPointXform *p_xf)
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
        vr_glow_add(p_xf->ox + (vec_dst.R[0] >> 15), p_xf->oy + (vec_dst.R[1] >> 15),
          p_xf->oz + (vec_dst.R[2] >> 15), 48 * 8, colour_lookup[ColLU_WHITE],
          ((u32)p_car->ThingOffset << 8) | (i & 0xFF));
    }
}

/** Explosion debris: faces flying off destroyed objects and floor. */
static void vr_mesh_explode_faces(struct VrMesh *p_mesh)
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
                flags |= VrsVF_TEXTURED;
            }
        } else {
            if ((p_exface->Texture != 0) && (p_exface->Texture < face_textures_limit)) {
                struct SingleTexture *p_stex = &game_face_textures[p_exface->Texture];
                uv[0][0] = p_stex->TMapX1; uv[0][1] = p_stex->TMapY1;
                uv[1][0] = p_stex->TMapX2; uv[1][1] = p_stex->TMapY2;
                uv[2][0] = p_stex->TMapX3; uv[2][1] = p_stex->TMapY3;
                page = p_stex->Page;
                flags |= VrsVF_TEXTURED;
            }
        }
        vr_group_begin(p_mesh, &first);
        if (npts == 4) {
            for (k = 0; k < 6; k++) {
                int n = quad_tris[k];
                vr_mesh_vert(p_mesh, px[n], py[n], pz[n], uv[n][0], uv[n][1], page,
                  p_exface->Col, vr_point_shade(px[n], pz[n]), flags);
            }
        } else {
            for (k = 0; k < 3; k++)
                vr_mesh_vert(p_mesh, px[k], py[k], pz[k], uv[k][0], uv[k][1], page,
                  p_exface->Col, vr_point_shade(px[k], pz[k]), flags);
        }
        vr_group_end(p_mesh, first, 0x20000000u | exface);
    }
}

/** Signature of what the static export shows, to re-send it at once when
 * buildings are destroyed or the ground is cratered.
 */
static u32 vr_world_signature(void)
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

static void vr_export_things(struct VrsHeader *p_hdr, struct VrMesh *p_dyn)
{
    struct VrsThing *p_vthings;
    uint n;
    ThingIdx thing;

    p_vthings = (struct VrsThing *)(vr_block + VRS_OFF_THINGS);
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
                vr_group_begin(p_dyn, &first);
                if ((p_thing->Flag & TngF_Destroyed) == 0)
                    mech_parts_foreach(p_thing, vr_mesh_mech_part, p_dyn);
                vr_group_end(p_dyn, first, thing);
                vr_bar_add(PRCCOORD_TO_MAPCOORD(p_thing->X), PRCCOORD_TO_YCOORD(p_thing->Y) + 1200,
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
            vr_group_begin(p_dyn, &first);
            vr_mesh_object(p_dyn, p_thing->U.UVehicle.Object, &xf);
            vr_group_end(p_dyn, first, thing);
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
                    vr_group_begin(p_dyn, &first);
                    vr_mesh_object(p_dyn, p_mgun->U.UMGun.Object, &gxf);
                    vr_group_end(p_dyn, first, 0x10000000u | p_mgun->ThingOffset);
                }
            }
            if (p_thing->SubType != SubTT_VEH_TRAIN)
                vr_vehicle_shadow(p_thing, &xf, thing);
            vr_car_glare(p_thing, &xf);
            vr_bar_add(xf.ox, xf.oy + 300, xf.oz, p_thing->Health, p_thing->U.UVehicle.MaxHealth, thing);
            continue;
        }
        if (p_thing->Type == TT_ROCKET)
        {
            uint first;
            short tng_x, tng_y, tng_z;
            vr_group_begin(p_dyn, &first);
            vr_mesh_rocket(p_dyn, p_thing);
            vr_group_end(p_dyn, first, thing);
            // build_rocket() adds a glare at the rocket
            get_thing_position_mapcoords(&tng_x, &tng_y, &tng_z, thing);
            vr_glow_add(tng_x, tng_y, tng_z, 64 * 8, colour_lookup[ColLU_YELLOW], thing);
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
                vr_group_begin(p_dyn, &first);
                vr_mesh_object(p_dyn, p_thing->U.UObject.Object, &xf);
                vr_group_end(p_dyn, first, thing);
            }
            if (p_thing->SubType == SubTT_BLD_MGUN)
                vr_bar_add(cor_x, cor_y + 300, cor_z, p_thing->Health, p_thing->U.UMGun.MaxHealth, thing);
            continue;
        }
        if (p_thing->Type != TT_PERSON)
            continue;
        if (n >= VRS_THINGS_MAX)
            break;
        get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
        p_vthings[n].x = cor_x;
        p_vthings[n].y = ENGINE_Y_TO_MAP(cor_y);
        p_vthings[n].z = cor_z;
        p_vthings[n].index = thing;
        p_vthings[n].type = p_thing->Type;
        p_vthings[n].subtype = p_thing->SubType;
        p_vthings[n].flags = 0;
        if (vr_is_player_agent(p_thing))
            p_vthings[n].flags |= VrsTF_PLAYER_AGENT;
        if (thing == players[local_player_no].DirectControl[0])
            p_vthings[n].flags |= VrsTF_SELECTED;
        if ((p_thing->State == PerSt_DEAD) || ((p_thing->Flag & TngF_Destroyed) != 0))
            p_vthings[n].flags |= VrsTF_DEAD;
        p_vthings[n].angle = p_thing->U.UPerson.Angle;
        p_vthings[n].group = p_thing->U.UPerson.EffectiveGroup;
        p_vthings[n].pad = 0;
        p_vthings[n].health = p_thing->Health;
        p_vthings[n].max_health = p_thing->U.UPerson.MaxHealth;
        vr_person_anim(&p_vthings[n], p_thing);
        n++;
    }
    p_hdr->n_things = n;
}

static void vr_export_static(struct VrsHeader *p_hdr)
{
    struct VrMesh mesh;
    ThingIdx thing;

    mesh.verts = (struct VrsVert *)(vr_block + VRS_OFF_STATIC);
    mesh.count = 0;
    mesh.limit = VRS_STATIC_VERTS_MAX;

    vr_mesh_terrain(&mesh);

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
            vr_mesh_object(&mesh, obj, &xf);
    }
    p_hdr->n_static = mesh.count;
    p_hdr->static_rev = ++vr_static_rev;
    if (vr_last_map != current_map)
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

static void vr_export_tmap(struct VrsHeader *p_hdr)
{
    int page;

    for (page = 0; page < VRS_TMAP_PAGES; page++)
    {
        ubyte *p_dst = vr_block + VRS_OFF_TMAP + page * VRS_TMAP_DIM * VRS_TMAP_DIM;
        if ((page < VEC_TMAP_LIMIT) && (vec_tmap[page] != NULL))
            memcpy(p_dst, vec_tmap[page], VRS_TMAP_DIM * VRS_TMAP_DIM);
        else
            memset(p_dst, 0, VRS_TMAP_DIM * VRS_TMAP_DIM);
    }
    if (display_palette != NULL)
        memcpy(vr_block + VRS_OFF_PALETTE, display_palette, 768);
    p_hdr->tmap_rev = ++vr_tmap_rev;
}

/** Decodes one RLE sprite into a sprite page with 2 bytes per texel. */
static void vr_decode_sprite(const struct TbSprite *p_spr, ubyte *p_page, int px, int py)
{
    const sbyte *p_dat;
    int x, y;

    p_dat = (const sbyte *)p_spr->Data;
    for (y = 0; y < p_spr->SHeight; y++)
    {
        ubyte *p_row = p_page + 2 * (VRS_TMAP_DIM * (py + y) + px);
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
static void vr_export_sprites(struct VrsHeader *p_hdr)
{
    struct VrsSpriteRect *p_rects;
    ubyte *p_pages;
    uint n_sprites, i, n;
    int page, cx, cy, shelf_h;

    p_rects = (struct VrsSpriteRect *)(vr_block + VRS_OFF_SPRITE_RECTS);
    p_pages = vr_block + VRS_OFF_SPRITE_PAGES;
    memset(p_pages, 0, VRS_SPRITE_PAGES * VRS_SPRITE_PAGE_BYTES);
    memset(p_rects, 0, VRS_SPRITES_MAX * sizeof(*p_rects));

    n_sprites = 0;
    if ((m_sprites != NULL) && (m_sprites_end > m_sprites))
        n_sprites = m_sprites_end - m_sprites;
    if (n_sprites > VRS_SPRITES_MAX)
        n_sprites = VRS_SPRITES_MAX;

    page = 0;
    cx = cy = shelf_h = 0;
    for (i = 1; i < n_sprites; i++)
    {
        const struct TbSprite *p_spr = &m_sprites[i];
        int w = p_spr->SWidth, h = p_spr->SHeight;

        if ((w == 0) || (h == 0) || (p_spr->Data == NULL))
            continue;
        // one texel of padding keeps neighbours out of filtered edges
        if (cx + w > VRS_TMAP_DIM) {
            cx = 0;
            cy += shelf_h + 1;
            shelf_h = 0;
        }
        if (cy + h > VRS_TMAP_DIM) {
            page++;
            cx = cy = shelf_h = 0;
        }
        if (page >= VRS_SPRITE_PAGES) {
            LOGWARN("sprite pages full at sprite %u of %u", i, n_sprites);
            break;
        }
        vr_decode_sprite(p_spr, p_pages + page * VRS_SPRITE_PAGE_BYTES, cx, cy);
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
    p_hdr->n_sprite_pages = (page < VRS_SPRITE_PAGES) ? page + 1 : VRS_SPRITE_PAGES;
    p_hdr->sprite_stride = sizeof(struct TbSprite);

    n = (nstart_ani != NULL) ? (uint)(nstart_ani_end - nstart_ani) : 0;
    if (n > VRS_NSTART_MAX)
        n = VRS_NSTART_MAX;
    if (n > 0)
        memcpy(vr_block + VRS_OFF_NSTART, nstart_ani, n * sizeof(ushort));
    p_hdr->n_nstart = n;

    n = (frame != NULL) ? (uint)(frame_end - frame) : 0;
    if (n > VRS_FRAMES_MAX)
        n = VRS_FRAMES_MAX;
    if (n > 0)
        memcpy(vr_block + VRS_OFF_FRAMES, frame, n * sizeof(struct Frame));
    p_hdr->n_frames = n;

    n = (melement_ani != NULL) ? (uint)(mele_ani_end - melement_ani) : 0;
    if (n > VRS_ELEMENTS_MAX)
        n = VRS_ELEMENTS_MAX;
    if (n > 0)
        memcpy(vr_block + VRS_OFF_ELEMENTS, melement_ani, n * sizeof(struct Element));
    p_hdr->n_elements = n;

    p_hdr->sprite_rev = ++vr_sprite_rev;
    LOGSYNC_F("VR spectator: %u sprites in %u pages, %u frames, %u elements",
      n_sprites, (uint)p_hdr->n_sprite_pages, (uint)p_hdr->n_frames, (uint)p_hdr->n_elements);
}

/******************************************************************************/
/* Effects and items drawn as sprite frames */

struct VrEffects {
    struct VrsEffect *p_list;
    uint count;
};

static void vr_effect_add(struct VrEffects *p_efx, float x, float y, float z,
  ushort frm, ushort scale, int brig, ubyte kind, u32 id)
{
    struct VrsEffect *p_eff;

    if (p_efx->count >= VRS_EFFECTS_MAX)
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
static int vr_ground_shade(s32 prc_x, s32 prc_z)
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
static void vr_mesh_shrapnel(struct VrMesh *p_mesh, short shrap_beg)
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
            vr_group_begin(p_mesh, &first);
            for (k = 0; k < 3; k++)
                vr_mesh_vert(p_mesh, px[k], py[k], pz[k], 0, 0, 0,
                  colour_lookup[ColLU_GREYLT], 128, 0);
            vr_group_end(p_mesh, first, 0x30000000u | shrap);
        }
    }
}

static void vr_export_effects(struct VrsHeader *p_hdr, struct VrMesh *p_dyn)
{
    struct VrEffects efx;
    ThingIdx thing;
    int remain;

    efx.p_list = (struct VrsEffect *)(vr_block + VRS_OFF_EFFECTS);
    efx.count = 0;

    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing;
        short cor_x, cor_y, cor_z;

        p_thing = &things[thing];
        if ((p_thing->Type == TT_MINE) || (p_thing->Type == TT_GRENADE))
        {
            get_thing_position_mapcoords(&cor_x, &cor_y, &cor_z, thing);
            vr_effect_add(&efx, cor_x, cor_y, cor_z, p_thing->Frame, 256,
              vr_ground_shade(p_thing->X, p_thing->Z), VrsEK_GRENADE, thing);
        }
        else if (p_thing->Type == TT_UNKN18)
        {
            vr_effect_add(&efx, PRCCOORD_TO_MAPCOORD(p_thing->X),
              PRCCOORD_TO_YCOORD(p_thing->Y), PRCCOORD_TO_MAPCOORD(p_thing->Z),
              nstart_ani[900], 256, 63, VrsEK_OTHER, thing);
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
            vr_effect_add(&efx, x, y, z, p_sthing->Frame, 256,
              vr_ground_shade(p_sthing->X, p_sthing->Z), VrsEK_STATIC, 0x01000000u | (ushort)thing);
            break;
        case SmTT_DROPPED_ITEM:
            vr_effect_add(&efx, x, y, z, p_sthing->Frame, 256,
              vr_ground_shade(p_sthing->X, p_sthing->Z), VrsEK_DROPPED_ITEM, 0x01000000u | (ushort)thing);
            break;
        case SmTT_SCALE_EFFECT:
            vr_effect_add(&efx, x, y, z, p_sthing->Frame, p_sthing->Object,
              32, VrsEK_SCALE_EFFECT, 0x01000000u | (ushort)thing);
            break;
        case SmTT_BANG:
        {
            struct Phwoar *p_phwoar;
            int phw, guard = 0;
            for (phw = p_sthing->U.UBang.phwoar; (phw > 0) && (phw < PHWOAR_COUNT) && (guard < PHWOAR_COUNT);
              phw = p_phwoar->child, guard++)
            {
                p_phwoar = &phwoar[phw];
                vr_effect_add(&efx, p_phwoar->x >> 8, p_phwoar->y >> 5, p_phwoar->z >> 8,
                  p_phwoar->f, 256, 32, VrsEK_SMOKE, 0x02000000u | phw);
            }
            vr_mesh_shrapnel(p_dyn, p_sthing->U.UBang.shrapnel);
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
                vr_effect_add(&efx, p_flame->x, p_flame->y, p_flame->z,
                  p_flame->frame, scale, 32, VrsEK_FLAME, 0x03000000u | flm);
            }
            break;
        }
        default:
            break;
        }
    }
    p_hdr->n_effects = efx.count;
}

/** Test aid: with SWARS_VR_TEST_BANG=1, makes an explosion and a fire near
 * the first player agent every 48 turns, and the other agents fire a laser,
 * an electric laser and a laser beam, to check the effects export without
 * playing.
 */
static void vr_test_bangs(void)
{
    static int test_bang = -1;
    struct Thing *p_agent;
    s32 prc_x, prc_y, prc_z;

    if (test_bang < 0) {
        const char *s = getenv("SWARS_VR_TEST_BANG");
        test_bang = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!test_bang)
        return;
    if ((gameturn % 48) == 20)
    {
        int i;
        // without a target, only the selected agent shoots, straight ahead
        for (i = 1; i < 4; i++)
        {
            u32 flag_bkp;
            p_agent = players[local_player_no].MyAgent[i];
            if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
                continue;
            flag_bkp = p_agent->Flag;
            p_agent->Flag |= TngF_SelectedAgent;
            if (i == 1)
                init_laser(p_agent, 0);
            else if (i == 2)
                init_laser_elec(p_agent, 0);
            else
                init_laser_beam(p_agent, 0, 0);
            p_agent->Flag = (p_agent->Flag & ~TngF_SelectedAgent) | (flag_bkp & TngF_SelectedAgent);
        }
    }
    if ((gameturn % 48) != 40)
        return;
    p_agent = players[local_player_no].MyAgent[0];
    if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
        return;
    prc_x = p_agent->X + MAPCOORD_TO_PRCCOORD(700, 0);
    prc_z = p_agent->Z + MAPCOORD_TO_PRCCOORD(300, 0);
    prc_y = p_agent->Y;
    bang_new4(prc_x, prc_y, prc_z, 20);
    FIRE_new(prc_x - MAPCOORD_TO_PRCCOORD(500, 0), PRCCOORD_TO_YCOORD(prc_y), prc_z, 3);
}

/******************************************************************************/
/* Line effects: lasers, beams, electricity, sparks and wires */

struct VrLines {
    struct VrsLine *p_list;
    uint count;
};

/** Adds a line; positions in map units with engine height. */
static void vr_line_add(struct VrLines *p_lns, float x1, float y1, float z1,
  float x2, float y2, float z2, ubyte colour, ubyte width, ubyte flags)
{
    struct VrsLine *p_ln;

    if (p_lns->count >= VRS_LINES_MAX)
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
static void vr_line_ring(struct VrLines *p_lns, float cx, float cy, float cz,
  float radius, ubyte colour, ubyte width)
{
    int i;
    const int segs = 32;

    for (i = 0; i < segs; i++)
    {
        int a1 = (i * 2048 / segs) & LbFPMath_AngleMask;
        int a2 = ((i + 1) * 2048 / segs) & LbFPMath_AngleMask;
        vr_line_add(p_lns,
          cx + radius * lbSinTable[a1] / 65536.0f, cy, cz + radius * lbSinTable[a1 + LbFPMath_PI/2] / 65536.0f,
          cx + radius * lbSinTable[a2] / 65536.0f, cy, cz + radius * lbSinTable[a2 + LbFPMath_PI/2] / 65536.0f,
          colour, width, VrsLF_GLOW);
    }
}

/** Start of a shot: the muzzle of its owner, where the game itself starts
 * shots (thing_fire_shot_start_position()), following the owner as it
 * moves; the drawing code starts them there too, plus a screen offset.
 */
static void vr_shot_start(struct Thing *p_owner, WeaponType wtype,
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

static void vr_lines_guided_laser(struct VrLines *p_lns, struct Thing *p_laser)
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
        vr_line_add(p_lns, cur_x, 8 * cur_y, cur_z, new_x, 8 * cur_y, new_z,
          (count == 1) ? colour_lookup[ColLU_WHITE] : colour_lookup[2], 14, VrsLF_GLOW);
        cur_angle = (cur_angle + rate) & LbFPMath_AngleMask;
        cur_x = new_x;
        cur_z = new_z;
    }
}

static void vr_lines_stasis_pod(struct VrLines *p_lns, struct SimpleThing *p_sthing)
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
        vr_line_ring(p_lns, cx, 8 * (cy + cor_dy), cz, pod_radius,
          colour_lookup[ColLU_RED], 6);
        pod_angl += 100;
    }
}

static void vr_export_lines(struct VrsHeader *p_hdr)
{
    struct VrLines lns;
    struct Thing *p_dcthing;
    ThingIdx thing;
    int remain;

    lns.p_list = (struct VrsLine *)(vr_block + VRS_OFF_LINES);
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
            vr_shot_start(p_owner, WEP_LASER, &x1, &y1, &z1);
            vr_line_add(&lns, x1, y1, z1, p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
              ((p_thing->Flag & TngF_SelectedAgent) != 0) ? colour_lookup[ColLU_BLUE] : colour_lookup[ColLU_RED],
              10, VrsLF_GLOW);
            break;
        case TT_LASER29:
        case TT_LASER38:
            p_owner = &things[p_thing->Owner];
            vr_shot_start(p_owner, WEP_BEAM, &x1, &y1, &z1);
            x2 = p_thing->VX; y2 = 8 * p_thing->VY; z2 = p_thing->VZ;
            // build_laser_beam() and build_laser_beam_q(): core and strands
            vr_line_add(&lns, x1, y1, z1, x2, y2, z2,
              (p_thing->Type == TT_LASER29) ? colour_lookup[ColLU_WHITE] : colour_lookup[ColLU_BLUE],
              (p_thing->Timer1 > 2) ? 40 : 12, VrsLF_GLOW);
            // build_laser_beam_glare(): flash at the muzzle
            vr_glow_add(x1, y1, z1, 13 * 8,
              (p_thing->Type == TT_LASER29) ? colour_lookup[ColLU_WHITE] : colour_lookup[ColLU_BLUE], thing);
            n = (p_thing->Type == TT_LASER29) ? 2 : 4;
            for (i = 0; i < n; i++)
                vr_line_add(&lns, x1, y1 + 160, z1, x2, y2, z2,
                  colour_lookup[ColLU_WHITE], 4, VrsLF_JAGGED);
            break;
        case TT_LASER_ELEC:
            p_owner = &things[p_thing->Owner];
            vr_shot_start(p_owner, WEP_ELLASER, &x1, &y1, &z1);
            n = (p_thing->State != 0) ? 1 : (p_thing->SubType >> 1);
            for (i = 0; i < n; i++)
                vr_line_add(&lns, x1, y1, z1, p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
                  colour_lookup[ColLU_WHITE], 5, VrsLF_JAGGED);
            break;
        case TT_RAZOR_WIRE:
        {
            ushort mask = (p_thing->SubType != 0) ? 0x1F : 0x7F;
            // the game shows other groups' wire only briefly
            if ((p_thing->U.UEffect.Group != p_dcthing->U.UPerson.EffectiveGroup)
              && ((gameturn & mask) != 0))
                break;
            vr_line_add(&lns, PRCCOORD_TO_MAPCOORD(p_thing->X),
              8 * (PRCCOORD_TO_YCOORD(p_thing->Y) >> 3), PRCCOORD_TO_MAPCOORD(p_thing->Z),
              p_thing->VX, 8 * p_thing->VY, p_thing->VZ,
              colour_lookup[ColLU_GREYLT], 4, VrsLF_JAGGED);
            break;
        }
        case TT_LASER_GUIDED:
            vr_lines_guided_laser(&lns, p_thing);
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
            vr_glow_add(PRCCOORD_TO_MAPCOORD(p_sthing->X), PRCCOORD_TO_YCOORD(p_sthing->Y),
              PRCCOORD_TO_MAPCOORD(p_sthing->Z), 32 * 8, colour_lookup[ColLU_WHITE],
              0x04000000u | (ushort)thing);
            break;
        case SmTT_ELECTRIC_STRAND:
            vr_line_add(&lns, p_sthing->X >> 8, p_sthing->Y >> 8, p_sthing->Z >> 8,
              p_sthing->U.UEffect.VX, p_sthing->U.UEffect.VY, p_sthing->U.UEffect.VZ,
              colour_lookup[ColLU_WHITE], 5, VrsLF_JAGGED);
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
            vr_line_add(&lns, x1, 8 * y1, z1, x1 + 8 * dx / dist, 8 * (y1 + 8 * dy / dist),
              z1 + 8 * dz / dist, p_sthing->Object & 0xFF, 4, VrsLF_GLOW);
            break;
        }
        case SmTT_BANG:
            if (p_sthing->State != 0)
            {
                float x = p_sthing->X >> 8, y = 8 * (p_sthing->Y >> 8), z = p_sthing->Z >> 8;
                vr_line_add(&lns, x, y, z, x, y + 8 * 400, z,
                  (p_sthing->State < 100) ? colour_lookup[ColLU_GREEN] : colour_lookup[ColLU_PINK],
                  16, VrsLF_GLOW);
            }
            break;
        case SmTT_STASIS_POD:
            vr_lines_stasis_pod(&lns, p_sthing);
            break;
        case SmTT_TIME_POD:
            vr_line_ring(&lns, p_sthing->X >> 8, 8 * (PRCCOORD_TO_MAPCOORD(p_sthing->Y) + 10),
              p_sthing->Z >> 8, (p_sthing->Radius >> 4) * 8.0f, colour_lookup[ColLU_BLUE], 8);
            break;
        case SmTT_NUCLEAR_BOMB:
            if ((p_sthing->Radius > 0) && ((p_sthing->Flag & TngF_InVehicle) != 0))
                vr_line_ring(&lns, PRCCOORD_TO_MAPCOORD(p_sthing->X),
                  8 * (PRCCOORD_TO_YCOORD(p_sthing->Y) >> 3), PRCCOORD_TO_MAPCOORD(p_sthing->Z),
                  p_sthing->Radius * 8.0f, colour_lookup[ColLU_WHITE], 12);
            break;
        default:
            break;
        }
    }
    p_hdr->n_lines = lns.count;
}

/** Test aid: with SWARS_VR_TEST_COLLAPSE=1, destroys the building nearest
 * to the first player agent once, at turn 100, to check how destruction
 * shows.
 */
static void vr_test_collapse(void)
{
    static int test_collapse = -1;
    struct Thing *p_agent, *p_best;
    ThingIdx thing;
    s32 best_dist;

    if (test_collapse < 0) {
        const char *s = getenv("SWARS_VR_TEST_COLLAPSE");
        test_collapse = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!test_collapse || (gameturn != 100))
        return;
    p_agent = players[local_player_no].MyAgent[0];
    if ((p_agent == NULL) || (p_agent->Type != TT_PERSON))
        return;
    p_best = NULL;
    best_dist = 0;
    for (thing = 1; thing < THINGS_LIMIT; thing++)
    {
        struct Thing *p_thing = &things[thing];
        s32 dx, dz, dist;
        if ((p_thing->Type != TT_BUILDING) || (p_thing->U.UObject.NumbObjects == 0))
            continue;
        if ((p_thing->SubType == SubTT_BLD_MOVN_ROTOR) || (p_thing->SubType == SubTT_BLD_MGUN))
            continue;
        dx = (p_thing->X - p_agent->X) >> 8;
        dz = (p_thing->Z - p_agent->Z) >> 8;
        dist = dx * dx + dz * dz;
        if ((p_best == NULL) || (dist < best_dist)) {
            p_best = p_thing;
            best_dist = dist;
        }
    }
    if (p_best != NULL) {
        LOGSYNC_F("VR test: collapsing building %d", (int)p_best->ThingOffset);
        collapse_building(p_best->X >> 8, p_best->Y >> 8, p_best->Z >> 8, p_best);
    }
}

/** Executes the next command from the viewer, as the packet a mouse click
 * would build (see process_mouse_inputs()); one per turn, as the game
 * takes one packet per turn from the local player. Called from
 * load_packet() when the player gave no input of their own.
 * Returns whether a packet was built.
 */
TbBool vr_spectator_input(void)
{
    struct VrsCmd cmd;
    struct Packet *p_pckt;
    PlayerInfo *p_locplayer;
    ThingIdx dcthing;
    int map_y;

    if ((vr_cmds == NULL) || (vr_cmds->read == vr_cmds->write))
        return false;
    if (vr_cmds->write - vr_cmds->read > VRS_CMDS_MAX) // viewer ran ahead; drop
        vr_cmds->read = vr_cmds->write - VRS_CMDS_MAX;
    p_pckt = &packets[local_player_no];
    cmd = vr_cmds->cmds[vr_cmds->read % VRS_CMDS_MAX];
    __sync_synchronize();
    vr_cmds->read++;

    p_locplayer = &players[local_player_no];
    dcthing = p_locplayer->DirectControl[0];
    if ((dcthing <= 0) || (things[dcthing].Type != TT_PERSON))
        return false;
    p_pckt->Action = PAct_NONE;
    switch (cmd.type)
    {
    case VrsCmd_SELECT_AGENT:
    {
        int i;
        for (i = 0; i < AGENTS_SQUAD_MAX_COUNT; i++) {
            struct Thing *p_agent = p_locplayer->MyAgent[i];
            if ((p_agent != NULL) && (p_agent->ThingOffset == cmd.thing)
              && ((p_agent->Flag & TngF_Destroyed) == 0))
                my_build_packet(p_pckt, PAct_SELECT_AGENT, dcthing, cmd.thing, 0, 0);
        }
        break;
    }
    case VrsCmd_GOTO:
        if ((cmd.x <= 0) || (cmd.x >= MAP_COORD_WIDTH) || (cmd.z <= 0) || (cmd.z >= MAP_COORD_HEIGHT))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_AGENT_GOTO_GND_PT_ABS, dcthing, cmd.x, map_y, cmd.z);
        break;
    case VrsCmd_SHOOT_THING:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type != TT_NONE))
            my_build_packet(p_pckt, PAct_SHOOT_AT_THING, dcthing, cmd.thing, 0, 0);
        break;
    case VrsCmd_SHOOT_GROUND:
        if ((cmd.x <= 0) || (cmd.x >= MAP_COORD_WIDTH) || (cmd.z <= 0) || (cmd.z >= MAP_COORD_HEIGHT))
            break;
        map_y = (alt_at_point(cmd.x, cmd.z) >> 8) + 20;
        my_build_packet(p_pckt, PAct_SHOOT_AT_GND_POINT, dcthing, cmd.x, map_y, cmd.z);
        break;
    case VrsCmd_WEAPON_NEXT:
        my_build_packet(p_pckt, PAct_SELECT_NEXT_WEAPON, dcthing, 0, 0, 0);
        break;
    case VrsCmd_WEAPON_PREV:
        my_build_packet(p_pckt, PAct_SELECT_PREV_WEAPON, dcthing, 0, 0, 0);
        break;
    case VrsCmd_WEAPON_SELECT:
        if ((cmd.arg > 0) && (cmd.arg < WEP_TYPES_COUNT))
            my_build_packet(p_pckt, PAct_SELECT_SPECIFIC_WEAPON, dcthing, cmd.arg, WepSel_TOGGLE, 0);
        break;
    case VrsCmd_ENTER_VEHICLE:
        if ((cmd.thing > 0) && (cmd.thing < THINGS_LIMIT) && (things[cmd.thing].Type == TT_VEHICLE))
            my_build_packet(p_pckt, PAct_GO_ENTER_VEHICLE, dcthing, cmd.thing, 0, 0);
        break;
    default:
        break;
    }
    return (p_pckt->Action != PAct_NONE);
}

/** What the game's panels show: agents, weapons, objective. */
static void vr_export_hud(struct VrsHeader *p_hdr)
{
    struct VrsHud *p_hud;
    PlayerInfo *p_locplayer;
    int i;

    p_hud = (struct VrsHud *)(vr_block + VRS_OFF_HUD);
    p_locplayer = &players[local_player_no];
    p_hdr->selected = p_locplayer->DirectControl[0];
    p_hdr->view_scale = overall_scale;
    p_hdr->view_size = ((u32)vec_window_width << 16) | ((u32)vec_window_height & 0xFFFF);
    for (i = 0; i < VRS_AGENTS_MAX; i++)
    {
        struct VrsAgent *p_va = &p_hud->agents[i];
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
            p_va->flags |= VrsTF_SELECTED;
        if ((p_agent->Flag & TngF_Destroyed) != 0 || (p_agent->State == PerSt_DEAD))
            p_va->flags |= VrsTF_DEAD;
    }
    memset(p_hud->objective, 0, sizeof(p_hud->objective));
    if (scrollinfo_text != NULL)
        strncpy(p_hud->objective, scrollinfo_text, sizeof(p_hud->objective) - 1);
    for (i = 0; i < VRS_WEAPON_TYPES; i++)
    {
        const char *name = (i < WEP_TYPES_COUNT) ? weapon_codename(i) : "";
        memset(p_hud->weapon_names[i], 0, sizeof(p_hud->weapon_names[i]));
        if (name != NULL)
            strncpy(p_hud->weapon_names[i], name, sizeof(p_hud->weapon_names[i]) - 1);
    }
}

void vr_spectator_update(void)
{
    struct VrsHeader *p_hdr;
    struct VrMesh dyn;
    TbBool in_mission;

    if (!vr_env_checked) {
        const char *s = getenv("SWARS_VR_SPECTATOR");
        vr_env_checked = true;
        vr_enabled = (s != NULL) && (*s != '\0') && (*s != '0');
    }
    if (!vr_enabled)
        return;
    if (vr_block == NULL) {
        if (!vr_open_block()) {
            vr_enabled = false;
            return;
        }
    }
    p_hdr = (struct VrsHeader *)vr_block;
    in_mission = (ingame.DisplayMode == DpM_ENGINEPLY) && (game_my_big_map != NULL);

    p_hdr->seq++; // odd - writing
    __sync_synchronize();
    p_hdr->gameturn = gameturn;
    p_hdr->in_mission = in_mission;
    if (in_mission)
    {
        if ((vr_last_map != current_map) || (vr_tmap_rev == 0)
          || ((gameturn - vr_static_turn) >= 8 * VR_STATIC_PERIOD))
            vr_export_tmap(p_hdr);
        if ((vr_last_map != current_map) || (vr_sprite_rev == 0))
            vr_export_sprites(p_hdr);
        {
            u32 sig = vr_world_signature();
            if ((vr_last_map != current_map) || (vr_static_rev == 0) || (sig != vr_world_hash)
              || ((gameturn - vr_static_turn) >= VR_STATIC_PERIOD))
            {
                vr_export_static(p_hdr);
                vr_static_turn = gameturn;
                vr_world_hash = sig;
            }
        }
        vr_last_map = current_map;
        p_hdr->map_no = current_map;

        dyn.verts = (struct VrsVert *)(vr_block + VRS_OFF_DYN);
        dyn.count = 0;
        dyn.limit = VRS_DYN_VERTS_MAX;
        vr_test_bangs();
        vr_test_collapse();
        vr_lists.p_groups = (struct VrsDynGroup *)(vr_block + VRS_OFF_DYN_GROUPS);
        vr_lists.p_bars = (struct VrsBar *)(vr_block + VRS_OFF_BARS);
        vr_lists.p_glows = (struct VrsGlow *)(vr_block + VRS_OFF_GLOWS);
        vr_lists.p_shadows = (struct VrsShadow *)(vr_block + VRS_OFF_SHADOWS);
        vr_lists.n_groups = vr_lists.n_bars = vr_lists.n_glows = vr_lists.n_shadows = 0;
        vr_export_things(p_hdr, &dyn);
        vr_mesh_explode_faces(&dyn);
        vr_export_effects(p_hdr, &dyn);
        vr_export_lines(p_hdr);
        p_hdr->n_dyn_groups = vr_lists.n_groups;
        p_hdr->n_bars = vr_lists.n_bars;
        p_hdr->n_glows = vr_lists.n_glows;
        p_hdr->n_shadows = vr_lists.n_shadows;
        vr_export_hud(p_hdr);
        p_hdr->n_dyn = dyn.count;

        p_hdr->cam_x = engn_xc;
        p_hdr->cam_y = ENGINE_Y_TO_MAP(8 * engn_yc);
        p_hdr->cam_z = engn_zc;
        // bflib angle units x 32, 2048 units per full turn
        p_hdr->cam_angle = (float)engn_cam_yaw / 32.0f * (6.2831853f / 2048.0f);
    }
    __sync_synchronize();
    p_hdr->seq++; // even - consistent
}

/******************************************************************************/
