/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_world.h
 *     Header file for game_world.c.
 * @par Purpose:
 *     Setting up and running the game world of a mission.
 * @par Comment:
 *     Just a header file - #defines, typedefs, function prototypes etc.
 * @author   Tomasz Lis
 * @date     22 Apr 2023 - 24 Nov 2026
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include "bftypes.h"
#include "game_bstype.h"

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
struct Thing;
struct SimpleThing;

extern u32 stored_l3d_next_object;
extern u32 stored_l3d_next_object_face3;
extern u32 stored_l3d_next_object_face4;
extern u32 stored_l3d_next_object_point;
extern u32 stored_l3d_next_normal;
extern u32 stored_l3d_next_face_texture;
extern u32 stored_l3d_next_floor_texture;
extern u32 stored_l3d_next_local_mat;
extern u32 stored_level3d_inuse;
extern s32 gamep_unknval_10;
extern s32 gamep_unknval_11;
extern s32 gamep_unknval_12;
extern u32 nav_stats__ThisTurn;
extern s32 gamep_unknval_14;
extern s32 gamep_unknval_15;
extern s32 gamep_unknval_16;
extern ushort word_1AABD0;
extern const char *primvehobj_fname;

void debug_trace_setup(int place);
void player_debug(const char *text);
void fill_floor_textures(void);
void fill_netgame_agent_pos(int plyr, int group, int num_agents);
void unkn_f_pressed_func(void);
void load_level_wrap(short level, short missi, ubyte reload);
int can_the_tank_run_over(struct SimpleThing *p_sthing);
void net_player_colors_reassign(void);
void unkn_truce_groups(void);
void blind_progress_game(u32 nturns);
void unkn_obj_faces4_lights_func_10(int face_idx, int point, int intensity);
void map_lights_update(void);
int train_related_func_01(struct Thing *p_train);
void func_74934(void);
void rotate_texture(int texture);
void init_crater_textures(void);
int calc_object_height(int object);
void find_the_tall_buildings(void);
void nullsub_7(void);
void create_unkn_building_subtp27(int cor_x, int cor_z, int object, int cor_y);
int func_4225c(int object);
int create_train_carriage(short cor_dx, short cor_dy, short cor_dz, short otype);
struct Thing *find_unused_train_track(void);
void create_train_for_each_track(void);
void init_my_paths(void);
void init_level_unknsub01_person(struct Thing *p_person);
void init_level_unknsub01_building(struct Thing *p_buildng);
void init_level_unknsub01_vehicle(struct Thing *p_vehicle);
void init_level_unknsub01(void);
void setup_ingame_scene_effect(void);
void start_ingame_ambient_sound(void);
void init_level(void);
void place_multiplayer_agent_groups(void);
void init_game_controls(void);
struct Thing *unkn_create_random_person(int cor_x, int cor_y, int cor_z, int a4, int a5, int a6);
void simulated_level(void);
void init_local_player_group(void);
void validate_player_double_mode(void);
void game_set_cam_starting_location_xz(void);
void game_set_cam_track_player_agent_xz(PlayerIdx plyr, ushort plagent);
void init_random_seed_default(void);
void preprogress_game_turns(void);
void init_player(void);
void randomize_playable_groups_order(void);
void init_game(ubyte reload);
short test_single_mission(short missi);
void post_process_blips(void);

void game_process_orbital_station_explode(void);
void process_map_craters(void);
void mapwho_unkn01(int cent_tile_x, int cent_tile_z);

/******************************************************************************/
#ifdef __cplusplus
};
#endif

#endif
