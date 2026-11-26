// Syndicate Wars VR viewer.
//
// With --mission C,M it plays the mission itself: the game core library
// (swcore.h) steps the turns on a fixed clock (--turn-ms, default 50), the
// world export (swview.h) gives the world; game data from --data DIR or
// $SWARS_GAMEDATA; Enter confirms the end of a decided mission (default
// mission 0,1).
// It shows the world as a miniature on a table, in a SteamVR headset
// through OpenXR, or in a desktop window with an orbit camera (--desktop, or
// when no headset is available).
//
// Controls (VR):  right trigger: point at an agent to select it, at the ground to
//                 walk there, at a vehicle to get in, at a row of the wrist panel
//                 to pick an agent or weapon; left trigger: shoot at what the
//                 left hand points at; left A/B (X/Y): previous/next weapon;
//                 left stick pan, right stick zoom (Y) / rotate (X),
//                 A toggles following the game camera,
//                 left stick click recentres the table in front of you.
// Options: --desktop, --click X Y SECONDS BUTTON (simulated click),
//          --zoom HALF_MAP, --center X Z, --orbit YAW PITCH DIST,
//          --screenshot FILE SECONDS, --dump-block TURN FILE (the world block of that turn), --mission C,M, --data DIR, --turn-ms MS.
// Controls (desktop): click as the right trigger, right click as the left
//                 trigger, [ ] weapons, drag mouse to orbit, wheel to change distance,
//                 arrows pan, +/- zoom, F follow, Q/E rotate the table,
//                 Esc quits, Enter confirms the end of a decided mission.
//
// Modules (headers, one translation unit): common.h, world_source.h (the
// game core and its world), render_classic.h, play.h (commands,
// picking), hud.h, people.h, table_view.h, hud_vr.h (the VR HUD: wrist
// panel, objective board, world markers), xr.h; main.cpp is the loop.

#include "common.h"
#include "world_source.h"
#include "render_classic.h"
#include "play.h"
#include "hud.h"
#include "people.h"
#include "table_view.h"
#include "hud_vr.h"
#include "xr.h"

/******************************************************************************/

static double now_seconds()
{
    static LARGE_INTEGER freq{}, start{};
    LARGE_INTEGER t;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&start);
    }
    QueryPerformanceCounter(&t);
    return double(t.QuadPart - start.QuadPart) / double(freq.QuadPart);
}

static void save_screenshot(const char *fname, int w, int h)
{
    std::vector<uint8_t> px(w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE *f = fopen(fname, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        fwrite(&px[y * w * 3], 1, w * 3, f);
    fclose(f);
    logf("screenshot saved to %s", fname);
}

int main(int argc, char **argv)
{
    bool desktop = false;
    const char *screenshot = nullptr;
    double screenshot_after = 0;
    float orbit_yaw = 0.6f, orbit_pitch = 0.75f, orbit_dist = 1.6f;
    float start_zoom = 0;
    float center_x = -1, center_z = -1;
    struct SimClick {
        int x, y, button;
        double at;
        bool done;
    };
    std::vector<SimClick> sim_clicks;
    // the mission the game core plays: --mission C,M
    unsigned campaign = 0, mission = 1;
    const char *data_dir = getenv("SWARS_GAMEDATA");
    double turn_ms = 50.0;
    uint32_t dump_turn = 0;
    const char *dump_file = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mission") && i + 1 < argc) {
            if (sscanf(argv[++i], "%u,%u", &campaign, &mission) != 2) {
                logf("--mission takes campaign,mission, e.g. 0,1");
                return 1;
            }
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--dump-block") && i + 2 < argc) {
            dump_turn = (uint32_t)strtoul(argv[++i], nullptr, 0);
            dump_file = argv[++i];
        } else if (!strcmp(argv[i], "--turn-ms") && i + 1 < argc) {
            turn_ms = std::max(1.0, atof(argv[++i]));
        } else if (!strcmp(argv[i], "--desktop"))
            desktop = true;
        else if (!strcmp(argv[i], "--click") && i + 4 < argc) {
            SimClick c;
            c.x = atoi(argv[++i]);
            c.y = atoi(argv[++i]);
            c.at = atof(argv[++i]);
            c.button = atoi(argv[++i]);
            c.done = false;
            sim_clicks.push_back(c);
        }
        else if (!strcmp(argv[i], "--screenshot") && i + 2 < argc) {
            screenshot = argv[++i];
            screenshot_after = atof(argv[++i]);
        } else if (!strcmp(argv[i], "--orbit") && i + 3 < argc) {
            orbit_yaw = (float)atof(argv[++i]);
            orbit_pitch = (float)atof(argv[++i]);
            orbit_dist = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--center") && i + 2 < argc) {
            center_x = (float)atof(argv[++i]);
            center_z = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) {
            start_zoom = (float)atof(argv[++i]);
        }
    }

    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logf("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    int win_w = 1280, win_h = 800;
    SDL_Window *win = SDL_CreateWindow("Syndicate Wars VR Spectator", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, win_w, win_h,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, ctx);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        logf("glewInit failed");
        return 1;
    }
    logf("OpenGL %s on %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));

    Renderer rend;
    if (!rend.init())
        return 1;
    TableView tv;
    if (start_zoom > 0)
        tv.half_map = start_zoom;
    if (center_x >= 0) {
        tv.center = glm::vec3(center_x, 0, center_z);
        tv.follow = false;
    }
    rend.build_table(tv.half_map, 40.0f);
    float built_half = tv.half_map;

    XrApp xr;
    bool use_xr = false;
    if (!desktop) {
        use_xr = xr.init() && xr.create_session();
        if (!use_xr)
            logf("no VR headset available, using desktop mode");
    }
    SDL_GL_SetSwapInterval(use_xr ? 0 : 1);

    WorldSnapshot snap;
    People people;
    std::vector<GpuVert> people_verts, glow_verts;
    bool placed = !use_xr;
    XrApp::Input xr_in;
    glm::vec3 eye_world(0, 1.0f, 1.0f); // last known viewer position
    if (!use_xr)
        tv.table_pose = glm::mat4(1.0f);

    bool quit = false, dragging = false;
    int mouse_x = 0, mouse_y = 0, down_x = 0, down_y = 0;
    bool click_l = false, click_r = false, key_wep_next = false, key_wep_prev = false;
    CommandSender cmds;
    Picker picker;
    std::vector<Picker::Person> people_now;
    glm::mat4 desk_view(1.0f), desk_proj(1.0f);
    double t_prev = now_seconds(), t_start = t_prev;
    CoreWorld core;
    VrHud vrhud;
    double over_at = -1;
    if (!core.start(data_dir, campaign, mission, turn_ms / 1000.0, t_prev))
        return 1;
    if (dump_file != nullptr)
        core.dump_at(dump_turn, dump_file);
    while (!quit) {
        double t = now_seconds();
        float dt = (float)std::min(0.1, t - t_prev);
        t_prev = t;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                quit = true;
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                down_x = ev.button.x;
                down_y = ev.button.y;
                if (ev.button.button == SDL_BUTTON_LEFT)
                    dragging = true;
            } else if (ev.type == SDL_MOUSEBUTTONUP) {
                // a click is a press and release without dragging
                bool still = std::abs(ev.button.x - down_x) + std::abs(ev.button.y - down_y) < 5;
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    dragging = false;
                    click_l = click_l || still;
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    click_r = click_r || still;
                }
            } else if (ev.type == SDL_MOUSEMOTION) {
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
                if (dragging && (std::abs(mouse_x - down_x) + std::abs(mouse_y - down_y) >= 5)) {
                    orbit_yaw -= ev.motion.xrel * 0.005f;
                    orbit_pitch = std::clamp(orbit_pitch + ev.motion.yrel * 0.005f, 0.05f, 1.5f);
                }
            } else if (ev.type == SDL_MOUSEWHEEL)
                orbit_dist = std::clamp(orbit_dist * (ev.wheel.y > 0 ? 0.9f : 1.1f), 0.3f, 6.0f);
            else if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                case SDLK_ESCAPE: quit = true; break;
                case SDLK_RETURN: core.conclude(); break;
                case SDLK_f: tv.follow = !tv.follow; break;
                case SDLK_LEFTBRACKET: key_wep_prev = true; break;
                case SDLK_RIGHTBRACKET: key_wep_next = true; break;
                case SDLK_EQUALS: case SDLK_KP_PLUS: tv.zoom(0.8f); break;
                case SDLK_MINUS: case SDLK_KP_MINUS: tv.zoom(1.25f); break;
                case SDLK_q: tv.yaw += 0.2f; break;
                case SDLK_e: tv.yaw -= 0.2f; break;
                case SDLK_LEFT: tv.follow = false; tv.pan(-0.1f, 0); break;
                case SDLK_RIGHT: tv.follow = false; tv.pan(0.1f, 0); break;
                case SDLK_UP: tv.follow = false; tv.pan(0, -0.1f); break;
                case SDLK_DOWN: tv.follow = false; tv.pan(0, 0.1f); break;
                default: break;
                }
            } else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                win_w = ev.window.data1;
                win_h = ev.window.data2;
            }
        }

        if (use_xr)
            xr_in = xr.read_input();
        if (!core.running()) {
            // the mission is over: show its end for a moment, then leave
            if (over_at < 0)
                over_at = t;
            else if (t - over_at > 3.0)
                quit = true;
        }
        if (core.poll(snap, t)) {
            rend.upload_world(snap);
            people.update(snap, t);
            if (snap.static_changed)
                picker.update_ground(snap);
            picker.update_vehicles(snap);
        }
        tv.follow_camera(snap.hdr, dt);
        {
            static std::vector<SwvVert> moving;
            people.moving_geometry(t, moving);
            rend.upload_dyn(moving);
            rend.turn = snap.hdr.gameturn + people.turn_fraction(t);
        }
        if (!use_xr) {
            glm::vec3 eye(std::sin(orbit_yaw) * std::cos(orbit_pitch) * orbit_dist,
                          std::sin(orbit_pitch) * orbit_dist,
                          std::cos(orbit_yaw) * std::cos(orbit_pitch) * orbit_dist);
            eye_world = eye;
            desk_view = glm::lookAt(eye, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            desk_proj = glm::perspective(glm::radians(55.0f), (float)win_w / win_h, 0.02f, 50.0f);
        }
        glm::mat4 map_to_world = tv.map_model(), world_to_map = glm::inverse(map_to_world);
        glm::vec3 eye_map = glm::vec3(world_to_map * glm::vec4(eye_world, 1.0f));
        people.build(people_verts, glow_verts, t, snap, eye_map, rend);

        // ---- playing from the table: pointers, panels, commands
        HudBuilder hud;
        Ray world_ray[2]; // left hand: shoot; right hand: select and move
        bool act[2] = {false, false};
        bool wep_next = false, wep_prev = false;
        if (use_xr) {
            const XrApp::Input &in = xr_in;
            for (int h = 0; h < 2; h++) {
                world_ray[h].valid = in.aim_valid[h];
                world_ray[h].o = in.aim_pos[h];
                world_ray[h].d = glm::normalize(in.aim_dir[h]);
                act[h] = in.trig_pressed[h];
            }
            wep_next = in.wep_next;
            wep_prev = in.wep_prev;
        } else {
            int mx = mouse_x, my = mouse_y;
            for (SimClick &c : sim_clicks)
                if (!c.done && t - t_start >= c.at) {
                    c.done = true;
                    mx = mouse_x = c.x;
                    my = mouse_y = c.y;
                    (c.button == 2 ? click_r : click_l) = true;
                    logf("simulated click %d at %d,%d", c.button, c.x, c.y);
                }
            glm::vec4 vpv(0, 0, win_w, win_h);
            glm::vec3 a = glm::unProject(glm::vec3(mx, win_h - my, 0.0f), desk_view, desk_proj, vpv);
            glm::vec3 b = glm::unProject(glm::vec3(mx, win_h - my, 1.0f), desk_view, desk_proj, vpv);
            for (int h = 0; h < 2; h++) {
                world_ray[h].o = a;
                world_ray[h].d = glm::normalize(b - a);
                world_ray[h].valid = true;
            }
            act[0] = click_r;
            act[1] = click_l;
            wep_next = key_wep_next;
            wep_prev = key_wep_prev;
            click_l = click_r = key_wep_next = key_wep_prev = false;
        }

        // panels: agents and weapons on the left wrist (or beside the
        // table on the desktop), the objective on a board at the table's edge
        const SwvHud &hd = snap.hud;
        VrHud::Panels pn = vrhud.panels(hud, snap, use_xr, world_ray[0], world_ray[1], tv, eye_world);
        const Panel &panel = pn.wrist;
        const std::vector<int> &weapon_rows = pn.weapon_rows;
        int hover_row = pn.hover_row;
        bool on_panel = pn.on_wrist;

        // what each hand points at on the table
        people.list(t, people_now);
        glm::vec3 clip_c(tv.center.x, 0, tv.center.z);
        Hit hit[2];
        for (int h = 0; h < 2; h++) {
            if (!world_ray[h].valid)
                continue;
            Ray mr;
            mr.o = glm::vec3(world_to_map * glm::vec4(world_ray[h].o, 1.0f));
            mr.d = glm::normalize(glm::vec3(world_to_map * glm::vec4(world_ray[h].o + world_ray[h].d, 1.0f)) - mr.o);
            mr.valid = true;
            if (!(h == 1 && on_panel))
                hit[h] = picker.pick(mr, people_now, clip_c, tv.half_map);
        }
        auto own_agent = [&](uint16_t thing) {
            for (const SwvAgent &ag : hd.agents)
                if (ag.thing == thing && thing != 0)
                    return true;
            return false;
        };

        // commands
        if (snap.valid && snap.hdr.in_mission) {
            if (act[1]) {
                if (on_panel) {
                    if (hover_row >= 1 && hover_row <= 4 && hd.agents[hover_row - 1].thing != 0)
                        cmds.send(SwcCmd_SELECT_AGENT, (int16_t)hd.agents[hover_row - 1].thing);
                    else if (hover_row >= 6 && hover_row - 6 < (int)weapon_rows.size())
                        cmds.send(SwcCmd_WEAPON_SELECT, 0, 0, 0, weapon_rows[hover_row - 6]);
                } else if (hit[1].kind == Hit::PERSON && own_agent(hit[1].thing)) {
                    cmds.send(SwcCmd_SELECT_AGENT, (int16_t)hit[1].thing);
                } else if (hit[1].kind == Hit::VEHICLE) {
                    cmds.send(SwcCmd_ENTER_VEHICLE, (int16_t)hit[1].thing);
                } else if (hit[1].kind != Hit::NONE) {
                    cmds.send(SwcCmd_GOTO, 0, (int32_t)hit[1].p.x, (int32_t)hit[1].p.z);
                    vrhud.on_goto(hit[1].p.x, hit[1].p.z, (uint16_t)snap.hdr.selected, t);
                }
            }
            if (act[0]) {
                if ((hit[0].kind == Hit::PERSON && !own_agent(hit[0].thing)) || hit[0].kind == Hit::VEHICLE)
                    cmds.send(SwcCmd_SHOOT_THING, (int16_t)hit[0].thing);
                else if (hit[0].kind == Hit::GROUND || hit[0].kind == Hit::PERSON)
                    cmds.send(SwcCmd_SHOOT_GROUND, 0, (int32_t)hit[0].p.x, (int32_t)hit[0].p.z);
            }
            if (wep_next)
                cmds.send(SwcCmd_WEAPON_NEXT);
            if (wep_prev)
                cmds.send(SwcCmd_WEAPON_PREV);
        }

        // world markers: selection, health, waypoint, objective
        {
            MarkerBuilder mb;
            mb.eye = eye_map;
            VrHud::Pointed pointed;
            for (int h = 1; h >= 0; h--)
                if (pointed.thing == 0 && (hit[h].kind == Hit::PERSON || hit[h].kind == Hit::VEHICLE)) {
                    pointed.thing = hit[h].thing;
                    pointed.vehicle = hit[h].kind == Hit::VEHICLE;
                    pointed.p = hit[h].p;
                }
            vrhud.markers(mb, t, snap, people_now, picker, pointed);
            rend.upload_markers(mb.verts);
        }

        // rings on the table: what each hand points at
        if (snap.valid && snap.hdr.in_mission) {
            static const float ring_r[3] = {1.0f, 0.9f, 0.2f}, ring_l[3] = {1.0f, 0.2f, 0.15f};
            for (int h = 0; h < 2; h++) {
                const float *rc = h ? ring_r : ring_l;
                if (hit[h].kind == Hit::PERSON)
                    ground_ring(people_verts, hit[h].p, 100.0f, 10.0f, rc, eye_map);
                else if (hit[h].kind == Hit::VEHICLE)
                    ground_ring(people_verts, glm::vec3(hit[h].p.x, picker.ground_at(hit[h].p.x, hit[h].p.z),
                                                        hit[h].p.z), 260.0f, 14.0f, rc, eye_map);
                else if (hit[h].kind == Hit::GROUND)
                    ground_ring(people_verts, hit[h].p, 40.0f, 10.0f, rc, eye_map);
            }
            // the area the game window shows, from its zoom and size
            float scale = snap.hdr.view_scale > 0 ? (float)snap.hdr.view_scale : 256.0f;
            float vw = (float)(snap.hdr.view_size >> 16), vh = (float)(snap.hdr.view_size & 0xFFFF);
            if (vw > 0 && vh > 0) {
                float half_w = vw * 0.5f * 8.0f * 256.0f / scale;
                float half_d = vh * 0.5f * 8.0f * 256.0f / scale / kTiltSin;
                float a = snap.hdr.cam_angle;
                glm::vec3 right(std::cos(a), 0, -std::sin(a)), fwd(std::sin(a), 0, std::cos(a));
                glm::vec3 c(snap.hdr.cam_x, 0, snap.hdr.cam_z);
                glm::vec3 k[4] = {c - right * half_w - fwd * half_d, c + right * half_w - fwd * half_d,
                                  c + right * half_w + fwd * half_d, c - right * half_w + fwd * half_d};
                static const float frame_col[3] = {0.75f, 0.75f, 0.8f};
                for (int e = 0; e < 4; e++) {
                    glm::vec3 p1 = k[e], p2 = k[(e + 1) % 4];
                    const int seg = 16;
                    for (int s2 = 0; s2 < seg; s2 += 2) {
                        glm::vec3 a1 = glm::mix(p1, p2, (float)s2 / seg), a2 = glm::mix(p1, p2, (float)(s2 + 1) / seg);
                        a1.y = picker.ground_at(a1.x, a1.z) + 10;
                        a2.y = picker.ground_at(a2.x, a2.z) + 10;
                        glm::vec3 side = glm::normalize(glm::cross(a2 - a1, glm::vec3(0, 1, 0))) * 6.0f;
                        glm::vec3 q[4] = {a1 - side, a1 + side, a2 + side, a2 - side};
                        static const int tri[6] = {0, 1, 2, 0, 2, 3};
                        for (int kk : tri) {
                            GpuVert g{};
                            g.pos[0] = q[kk].x; g.pos[1] = q[kk].y; g.pos[2] = q[kk].z;
                            g.rgb[0] = frame_col[0]; g.rgb[1] = frame_col[1]; g.rgb[2] = frame_col[2];
                            g.textured = 3.0f;
                            people_verts.push_back(g);
                        }
                    }
                }
            }
        }

        // pointer beams from the controllers
        if (use_xr) {
            static const float beam_r[3] = {1.0f, 0.9f, 0.3f}, beam_l[3] = {1.0f, 0.3f, 0.2f};
            for (int h = 0; h < 2; h++) {
                if (!world_ray[h].valid)
                    continue;
                float len = 2.0f;
                if (h == 1 && on_panel)
                    len = std::max(0.02f, panel.hit_distance(world_ray[h]));
                else if (hit[h].kind != Hit::NONE)
                    len = glm::distance(world_ray[h].o, glm::vec3(map_to_world * glm::vec4(hit[h].p, 1.0f)));
                hud.line(world_ray[h].o, world_ray[h].o + world_ray[h].d * len, 0.003f, eye_world,
                         h ? beam_r : beam_l);
            }
        }
        rend.upload_hud(hud.verts);
        // people colours come from the figures, not the palette
        rend.upload_people(people_verts);
        rend.upload_glows(glow_verts);
        tv.ground_offset = rend.ground_low - 4.0f;
        if (std::fabs(built_half - tv.half_map) > 1.0f) {
            rend.build_table(tv.half_map, 40.0f);
            built_half = tv.half_map;
        }

        glm::vec3 clip_center(tv.center.x, 0, tv.center.z);
        auto draw_scene = [&](const glm::mat4 &vp, bool linear) {
            glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            rend.draw(vp, tv.table_model(), tv.map_model(), clip_center, tv.half_map, linear);
        };

        if (use_xr) {
            if (!xr.poll_events())
                break;
            XrApp::Input in = xr_in;
            if (std::fabs(in.pan.x) > 0.15f || std::fabs(in.pan.y) > 0.15f) {
                tv.follow = false;
                tv.pan(in.pan.x * dt * 0.8f, -in.pan.y * dt * 0.8f);
            }
            if (std::fabs(in.zoom.y) > 0.2f)
                tv.zoom(std::pow(0.4f, in.zoom.y * dt));
            if (std::fabs(in.zoom.x) > 0.2f)
                tv.yaw -= in.zoom.x * dt * 1.5f;
            if (in.follow_pressed)
                tv.follow = !tv.follow;
            if (in.recenter_pressed || xr.take_just_focused())
                placed = false;
            XrTime xt = 0;
            xr.frame(draw_scene, xt);
            {
                glm::vec3 hp;
                glm::quat ho;
                if (xt != 0 && xr.head_pose(xt, hp, ho))
                    eye_world = hp;
            }
            if (!placed && xt != 0) {
                glm::vec3 hp;
                glm::quat ho;
                if (xr.head_pose(xt, hp, ho)) {
                    glm::vec3 fwd = ho * glm::vec3(0, 0, -1);
                    fwd.y = 0;
                    if (glm::length(fwd) > 0.01f) {
                        fwd = glm::normalize(fwd);
                        glm::vec3 c = hp + fwd * 0.75f;
                        c.y = std::max(0.6f, hp.y - 0.5f);
                        float yaw = std::atan2(-fwd.x, -fwd.z);
                        tv.table_pose = glm::rotate(glm::translate(glm::mat4(1.0f), c), yaw,
                                                    glm::vec3(0, 1, 0));
                        placed = true;
                        logf("table placed at %.2f %.2f %.2f", c.x, c.y, c.z);
                    }
                }
            }
            // mirror left eye to the desktop window
            XrApp::Mirror m = xr.last_left();
            if (m.fbo) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m.fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
                int sw = win_w, sh = (int)((float)win_w * m.h / m.w);
                if (sh > win_h) {
                    sh = win_h;
                    sw = (int)((float)win_h * m.w / m.h);
                }
                glClear(GL_COLOR_BUFFER_BIT);
                glBlitFramebuffer(0, 0, m.w, m.h, (win_w - sw) / 2, (win_h - sh) / 2,
                                  (win_w + sw) / 2, (win_h + sh) / 2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }
            SDL_GL_SwapWindow(win);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, win_w, win_h);
            draw_scene(desk_proj * desk_view, false);
            if (screenshot && t - t_start > screenshot_after) {
                save_screenshot(screenshot, win_w, win_h);
                screenshot = nullptr;
                quit = true;
            }
            SDL_GL_SwapWindow(win);
        }
    }
    core.stop();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
