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
//                 A toggles following the game camera, right stick click
//                 shows the whole world or only the table's square, the
//                 right grip switches the fog of war,
//                 left stick click recentres the table in front of you.
// Options: --no-desktop-frame (no yellow outline of the desktop's view),
//          --companion game|table (with VR_DESKTOP_GAME: the desktop
//          window is the desktop game, or the table program), --no-fog
//          (the fog of war of fog.h is on by default; switch with G),
//          --fog-scale F (times the default radius), --fog-radius R
//          (map units; default half the desktop window at its furthest
//          zoom), --desktop, --table-only (draw only the map square on the table;
//          the default draws the whole world around it; switch with C),
//          --look classic|upgraded (switch with L, or B on the
//          right controller), --assets DIR (the assets repository, default
//          $SWARS_ASSETS3D or assets3d next to the repository),
//          --click X Y SECONDS BUTTON (simulated click), --press X Y SECONDS
//          BUTTON HOLD (button held HOLD seconds), --key KEY SECONDS,
//          --zoom HALF_MAP, --center X Z, --orbit YAW PITCH DIST,
//          --screenshot FILE SECONDS, --dump-block TURN FILE (the world block of that turn), --mission C,M, --data DIR, --turn-ms MS.
// Controls (desktop): click as the right trigger, right button (held) as the
//                 left trigger, [ ] weapons, drag mouse to orbit, wheel to change distance,
//                 arrows pan, +/- zoom, F follow, Q/E rotate the table, L look,
//                 Esc quits, Enter confirms the end of a decided mission.
//
// Modules (headers, one translation unit): common.h, world_source.h (the
// game core and its world), render_classic.h, play.h (commands,
// picking), hud.h, people.h, table_view.h, hud_vr.h (the VR HUD: wrist
// panel, objective board, world markers), xr.h; main.cpp is the loop.

#ifndef VR_DESKTOP_GAME
#define VR_DESKTOP_GAME 0
#endif

#include "common.h"
#include "world_source.h"
#include "render_classic.h"
#include "play.h"
#include "hud.h"
#include "people.h"
#include "table_view.h"
#include "hud_vr.h"
#include "screen_panel.h"
#include "vkeyboard.h"
#include "look_upgraded.h"
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

#if VR_DESKTOP_GAME
// Runs the table build (syndwars-vr-table.exe, the game core alone, next to
// this program) with the same options and returns its exit code: for the
// 3D table in a window (--desktop, no headset) or with the headset
// (--companion table).
static int run_table_build(int argc, char **argv)
{
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string path(exe, n);
    size_t slash = path.find_last_of("\\/");
    path = path.substr(0, slash + 1) + "syndwars-vr-table.exe";
    std::string cmd = "\"" + path + "\"";
    for (int i = 1; i < argc; i++)
        cmd += std::string(" \"") + argv[i] + "\"";
    logf("running the table build: %s", cmd.c_str());
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(path.c_str(), &cmd[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        logf("cannot run %s", path.c_str());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}
#endif

int main(int argc, char **argv)
{
    bool desktop = false;
    // with the headset, the desktop window shows the desktop game (the
    // program built with VR_DESKTOP_GAME) or the 3D table (--companion table)
    bool companion_table = false;
    const char *screenshot = nullptr;
    double screenshot_after = 0;
    float orbit_yaw = 0.6f, orbit_pitch = 0.75f, orbit_dist = 1.6f;
    float start_zoom = 0;
    float center_x = -1, center_z = -1;
    struct SimClick {
        int x, y, button;
        double at;
        bool done;
        double hold = 0; // --press: seconds the button stays down
        bool released = false;
    };
    std::vector<SimClick> sim_clicks;
    // simulated key presses: --key KEY SECONDS (KEY as SDL names it: l, ], ...)
    struct SimKey {
        SDL_Keycode key;
        double at;
        bool done;
    };
    std::vector<SimKey> sim_keys;
    // the mission the game core plays: --mission C,M
    unsigned campaign = 0, mission = 1;
    // the game build without --mission plays the whole game: intro, menus
    bool mission_given = false;
    const char *data_dir = getenv("SWARS_GAMEDATA");
    bool upgraded = false;
    // the whole map drawn around the table, or only the square on it
    bool whole_world = true;
    // the fog of war (fog.h): on unless --no-fog; G, the right grip
    float fog_radius = 0, fog_scale = 1.0f;
    // the yellow outline of the desktop's view on the table
    bool desktop_frame = true;
    std::string assets_dir = getenv("SWARS_ASSETS3D") ? getenv("SWARS_ASSETS3D") : "";
    double turn_ms = 50.0;
    uint32_t dump_turn = 0;
    const char *dump_file = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mission") && i + 1 < argc) {
            if (sscanf(argv[++i], "%u,%u", &campaign, &mission) != 2) {
                logf("--mission takes campaign,mission, e.g. 0,1");
                return 1;
            }
            mission_given = true;
        } else if (!strcmp(argv[i], "--screen-size") && i + 1 < argc) {
            g_screen.width = std::clamp((float)atof(argv[++i]), 0.5f, 5.0f);
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--look") && i + 1 < argc) {
            upgraded = !strcmp(argv[++i], "upgraded");
        } else if (!strcmp(argv[i], "--assets") && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (!strcmp(argv[i], "--dump-block") && i + 2 < argc) {
            dump_turn = (uint32_t)strtoul(argv[++i], nullptr, 0);
            dump_file = argv[++i];
        } else if (!strcmp(argv[i], "--turn-ms") && i + 1 < argc) {
            turn_ms = std::max(1.0, atof(argv[++i]));
        } else if (!strcmp(argv[i], "--fog-radius") && i + 1 < argc) {
            fog_radius = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--fog-scale") && i + 1 < argc) {
            fog_scale = std::max(0.05f, (float)atof(argv[++i]));
        } else if (!strcmp(argv[i], "--desktop-frame"))
            desktop_frame = true;
        else if (!strcmp(argv[i], "--no-desktop-frame"))
            desktop_frame = false;
        else if (!strcmp(argv[i], "--fog"))
            g_fog.enabled = true;
        else if (!strcmp(argv[i], "--no-fog"))
            g_fog.enabled = false;
        else if (!strcmp(argv[i], "--table-only"))
            whole_world = false;
        else if (!strcmp(argv[i], "--whole-world"))
            whole_world = true;
        else if (!strcmp(argv[i], "--desktop"))
            desktop = true;
        else if (!strcmp(argv[i], "--companion") && i + 1 < argc)
            companion_table = !strcmp(argv[++i], "table");
        else if (!strcmp(argv[i], "--click") && i + 4 < argc) {
            SimClick c;
            c.x = atoi(argv[++i]);
            c.y = atoi(argv[++i]);
            c.at = atof(argv[++i]);
            c.button = atoi(argv[++i]);
            c.done = false;
            sim_clicks.push_back(c);
        } else if (!strcmp(argv[i], "--press") && i + 5 < argc) {
            SimClick c;
            c.x = atoi(argv[++i]);
            c.y = atoi(argv[++i]);
            c.at = atof(argv[++i]);
            c.button = atoi(argv[++i]);
            c.hold = atof(argv[++i]);
            c.done = false;
            sim_clicks.push_back(c);
        } else if (!strcmp(argv[i], "--key") && i + 2 < argc) {
            SDL_Keycode k = SDL_GetKeyFromName(argv[++i]);
            sim_keys.push_back({k, atof(argv[++i]), false});
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
#if VR_DESKTOP_GAME
    if (desktop || companion_table)
        return run_table_build(argc, argv);
#else
    (void)companion_table;
#endif
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
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                           // the desktop game has its own window
                                           (VR_DESKTOP_GAME ? SDL_WINDOW_HIDDEN : 0));
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
#if VR_DESKTOP_GAME
    // SWARS_VR_NO_HEADSET=1: the desktop game without a headset, for tests
    if (!use_xr && getenv("SWARS_VR_NO_HEADSET") == nullptr) {
        // no headset: the 3D table in a window, from the table build
        SDL_GL_DeleteContext(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return run_table_build(argc, argv);
    }
#endif
    SDL_GL_SetSwapInterval(use_xr ? 0 : 1);

    WorldSnapshot snap;
    People people;
    std::vector<GpuVert> people_verts, glow_verts;
    bool placed = !use_xr;
    // where the table goes when placed: along the play area's forward (at
    // the start and after the runtime's recentring), else along the head's
    // (the left stick click)
    bool place_forward = true;
    bool anchor_set = false; // the anchor of the table view, from the head
    XrApp::Input xr_in;
    glm::vec3 eye_world(0, 1.0f, 1.0f); // last known viewer position
    if (!use_xr)
        tv.table_pose = glm::mat4(1.0f);

    bool quit = false, dragging = false;
    int mouse_x = 0, mouse_y = 0, down_x = 0, down_y = 0;
    bool click_l = false, click_r = false, key_wep_next = false, key_wep_prev = false;
    bool right_down = false; // the right button held: the trigger
    CommandSender cmds;
    Picker picker;
    std::vector<Picker::Person> people_now;
    glm::mat4 desk_view(1.0f), desk_proj(1.0f);
    double t_prev = now_seconds(), t_start = t_prev;
#if VR_DESKTOP_GAME
    DesktopGameWorld core;
#else
    CoreWorld core;
#endif
    cmds.sink = [&](const SwcCommand &c) { core.command(c); };
    VrHud vrhud;
    UpgradedLook look;
    {
        // the assets folder, made absolute before the game changes folder
        if (assets_dir.empty()) {
            char *base = SDL_GetBasePath(); // build/vr-viewer/
            assets_dir = std::string(base ? base : "./") + "../../../assets3d";
            SDL_free(base);
        }
        char full[MAX_PATH];
        if (GetFullPathNameA(assets_dir.c_str(), MAX_PATH, full, nullptr) > 0)
            assets_dir = full;
        look.set_assets(assets_dir);
        look.enabled = upgraded;
        logf("look: %s (assets %s)", upgraded ? "upgraded" : "classic", assets_dir.c_str());
    }
    rend.draw_models = [&](const glm::mat4 &mvp, glm::vec3 cc, float ch, bool lin) {
        look.draw(mvp, cc, ch, lin);
    };
    rend.draw_effects = [&](const glm::mat4 &mvp, glm::vec3 cc, float ch, bool lin) {
        look.draw_effects(mvp, look.eye_map, cc, ch, lin);
    };
    look.ground = [&](float x, float z) { return picker.ground_at(x, z); };
    std::vector<People::ModelPlace> model_places;
    std::vector<People::PropPlace> prop_places;
    double over_at = -1;
    double drive_until = -1, look_sent = -1;
    double align_logged = -100;
    // the desktop's panels on the wrist: open weapon list, mood drag; the
    // buttons held the frame before
    DesktopPanels::State pst;
    bool panel_main_was = false, panel_alt_was = false;
    float zoom_ratio = 0, zoom_goal = 0; // table half size per desktop half width
    uint32_t zoom_seen = 0;
    bool heading_set = false;
    // the HUD's fonts (text.h): the vector font next to the program, read
    // before the game changes folder; the game's from its data folder
    {
        char *base = SDL_GetBasePath();
        g_text.load_vector(std::string(base ? base : "./") + "fonts/ShareTechMono-Regular.ttf");
        SDL_free(base);
    }
    // the game font is read on the game's thread, once it is set up
    rend.bind_extra = [](GLuint prog) {
        g_panels.bind(prog);
        g_screen.bind(prog);
    };
    bool full_game = false;
#if VR_DESKTOP_GAME
    core.set_full_game(!mission_given);
    full_game = core.full_game();
#endif
    (void)mission_given;
    // the game's screen on its panel (outside missions): pointer, buttons
    glm::vec2 scr_uv_sent(-1.0f);
    bool scr_l = false, scr_r = false, was_in_mission = false;
    if (!core.start(data_dir, campaign, mission, turn_ms / 1000.0, t_prev, [] {
            g_text.load_game();
            g_panels.load();
        }))
        return 1;
    if (dump_file != nullptr)
        core.dump_at(dump_turn, dump_file);
    while (!quit) {
        double t = now_seconds();
        float dt = (float)std::min(0.1, t - t_prev);
        t_prev = t;

        // whether the viewer moved the table this frame (then the desktop's
        // camera follows it, not the other way round)
        bool view_moved = false;
        SDL_Event ev;
        for (SimKey &k : sim_keys)
            if (!k.done && t - t_start >= k.at) {
                k.done = true;
                SDL_Event e{};
                e.type = SDL_KEYDOWN;
                e.key.keysym.sym = k.key;
                SDL_PushEvent(&e);
                logf("simulated key %s", SDL_GetKeyName(k.key));
            }
        // with the desktop game, SDL's events are its own: it reads them
        while (!VR_DESKTOP_GAME && SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT)
                quit = true;
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                down_x = ev.button.x;
                down_y = ev.button.y;
                if (ev.button.button == SDL_BUTTON_LEFT)
                    dragging = true;
                else if (ev.button.button == SDL_BUTTON_RIGHT)
                    right_down = true;
            } else if (ev.type == SDL_MOUSEBUTTONUP) {
                // a click is a press and release without dragging
                bool still = std::abs(ev.button.x - down_x) + std::abs(ev.button.y - down_y) < 5;
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    dragging = false;
                    click_l = click_l || still;
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    click_r = click_r || still;
                    right_down = false;
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
                case SDLK_g:
                    g_fog.enabled = !g_fog.enabled;
                    logf("fog of war: %s", g_fog.enabled ? "on" : "off");
                    break;
                case SDLK_c:
                    whole_world = !whole_world;
                    logf("world: %s", whole_world ? "whole" : "table only");
                    break;
                case SDLK_l:
                    look.enabled = !look.enabled;
                    logf("look: %s", look.enabled ? "upgraded" : "classic");
                    break;
                case SDLK_LEFTBRACKET: key_wep_prev = true; break;
                case SDLK_RIGHTBRACKET: key_wep_next = true; break;
                case SDLK_EQUALS: case SDLK_KP_PLUS: tv.zoom_at_anchor(0.8f); view_moved = true; break;
                case SDLK_MINUS: case SDLK_KP_MINUS: tv.zoom_at_anchor(1.25f); view_moved = true; break;
                case SDLK_q: tv.turn_at_anchor(0.2f); view_moved = true; break;
                case SDLK_e: tv.turn_at_anchor(-0.2f); view_moved = true; break;
                // the arrows move the view: the map slides the other way
                case SDLK_LEFT: tv.slide(0.1f, 0); view_moved = true; break;
                case SDLK_RIGHT: tv.slide(-0.1f, 0); view_moved = true; break;
                case SDLK_UP: tv.slide(0, -0.1f); view_moved = true; break;
                case SDLK_DOWN: tv.slide(0, 0.1f); view_moved = true; break;
                default: break;
                }
            } else if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                win_w = ev.window.data1;
                win_h = ev.window.data2;
            }
        }

        if (use_xr)
            xr_in = xr.read_input();
#if VR_DESKTOP_GAME
        {
            static std::vector<uint8_t> spx;
            static uint32_t sframe = 0;
            int sw = 0, sh = 0;
            uint8_t spal[768];
            if (core.screen(spx, sw, sh, spal, sframe))
                g_screen.set_frame(spx, sw, sh, spal);
        }
#endif
        // outside a mission (intro, menus, briefing) the game's own screen
        // on a panel, the table empty
        bool menus = full_game && !(snap.valid && snap.hdr.in_mission);
#if VR_DESKTOP_GAME
        if (getenv("SWARS_VR_MOUSE_TEST")) {
            // test: a click (skips the intro), a click on LOGIN, "VR" typed
            // with the headset's keys into its text box, the pointer to a
            // quarter of the screen, the frame then into the file named
            static int step = 0;
            if (step == 0 && t - t_start > 12) { core.mouse(2, 0.5f, 0.5f); step = 10; logf("test: press"); }
            if (step == 10 && t - t_start > 12.5) { core.mouse(3, 0.5f, 0.5f); step = 11; logf("test: release"); }
            if (step == 11 && t - t_start > 17) { core.mouse(1, 0.5f, 0.64f); core.mouse(2, 0.5f, 0.64f); step = 12; }
            if (step == 12 && t - t_start > 17.3) { core.mouse(3, 0.5f, 0.64f); step = 13; logf("test: LOGIN clicked"); }
            if (step == 13 && t - t_start > 19) {
                logf("test: text box focus %d", (int)core.text_focus());
                core.key(0x2F); // V
                core.key(0x13); // R
                step = 1;
            }
            if (step == 1 && t - t_start > 20) { core.mouse(1, 0.25f, 0.25f); step = 2; logf("test: mouse to 0.25"); }
            if (step == 2 && t - t_start > 23) {
                std::vector<uint8_t> px; int w = 0, h = 0; uint8_t pal[768]; uint32_t fr = 0;
                if (core.screen(px, w, h, pal, fr)) {
                    FILE *fp = fopen(getenv("SWARS_VR_MOUSE_TEST"), "wb");
                    fprintf(fp, "P6 %d %d 255\n", w, h);
                    for (uint8_t c : px) fwrite(&pal[3 * c], 1, 3, fp);
                    fclose(fp);
                    logf("test: frame %dx%d saved", w, h);
                }
                step = 3;
            }
        }
#endif
        rend.world_visible = !menus;
        if (!menus && !was_in_mission && snap.valid && snap.hdr.in_mission) {
            // a mission begins: the table lines up with the desktop again
            tv.resync();
            zoom_ratio = 0;
            zoom_goal = 0;
        }
        was_in_mission = snap.valid && snap.hdr.in_mission;
        if (!core.running()) {
            // the mission is over: show its end for a moment, then leave
            if (over_at < 0)
                over_at = t;
            else if (t - over_at > 3.0)
                quit = true;
        }
        if (core.poll(snap, t)) {
            g_text.set_palette(snap.palette);
            g_panels.set_palette(snap.palette);
            g_panels.set_scanner(snap);
            {
                // SWARS_VR_DUMP_PANELS=dir: the wrist panels drawn flat, once
                static bool panels_dumped = false;
                const char *dd = getenv("SWARS_VR_DUMP_PANELS");
                if (dd && !panels_dumped && snap.hdr.in_mission && snap.hdr.gameturn >= 100 && g_panels.ok()) {
                    g_panels.dump_flat(snap, dd);
                    panels_dumped = true;
                }
            }
            rend.upload_world(snap);
            people.update(snap, t);
            if (snap.static_changed)
                picker.update_ground(snap);
            picker.update_vehicles(snap);
        }
        // the two views look at the same place: while the viewer moves the
        // table (and a moment after, as the desktop's camera glides there)
        // the desktop's camera is sent to the map point in front of the
        // viewer; else the table follows the desktop's camera
        if (!use_xr)
            tv.anchor = glm::vec3(tv.table_pose[3]);
        {
            // the viewer's heading: in the headset the table's front, set
            // when it is placed (or recentred), so the head turning changes
            // nothing and the view turns only with the turn controls; in
            // the table program's window from the orbit camera, once
            glm::vec3 hv = tv.anchor - eye_world;
            if (!use_xr && !heading_set && hv.x * hv.x + hv.z * hv.z > 1e-6f) {
                tv.heading = std::atan2(hv.x, -hv.z);
                heading_set = true;
            }
        }
        if (view_moved && tv.follow)
            drive_until = t + 1.2;
        // the zooms: the table's half size and the desktop window's half
        // width on the ground keep the ratio they had when the views were
        // lined up; the desktop zooms only within what the game allows now
        // (the weapon limits how far out), the table goes on further
        float desk_vw = (float)(snap.hdr.view_size >> 16);
        auto desk_half_w = [&](float zoom) {
            float sc = zoom * (float)std::max<uint32_t>(snap.hdr.view_scale_256, 1) / 256.0f;
            return desk_vw * 0.5f * 2048.0f / sc;
        };
        bool zoom_known = snap.valid && snap.hdr.in_mission && snap.hdr.view_zoom > 0 && desk_vw > 0;
        if (zoom_known && zoom_ratio <= 0 && tv.synced()) {
            zoom_ratio = tv.half_map / desk_half_w((float)snap.hdr.view_zoom);
            zoom_seen = snap.hdr.view_zoom;
            logf("zoom: table %.0f map units for the desktop's %.0f (zoom %u)", tv.half_map,
                 desk_half_w((float)snap.hdr.view_zoom), snap.hdr.view_zoom);
        }
        if (tv.follow && t < drive_until && t - look_sent > 0.15 && snap.valid && snap.hdr.in_mission) {
            glm::vec2 m = tv.map_at(tv.anchor);
            core.look_at((int32_t)m.x, (int32_t)m.y, tv.game_angle());
            if (zoom_known && zoom_ratio > 0) {
                float want = desk_vw * 0.5f * 2048.0f / (tv.half_map / zoom_ratio) * 256.0f /
                             (float)std::max<uint32_t>(snap.hdr.view_scale_256, 1);
                float lo = (float)snap.hdr.view_zoom_far, hi = (float)snap.hdr.view_zoom_near;
                core.set_zoom((int32_t)std::lround(std::clamp(want, lo, std::max(lo, hi))));
            }
            look_sent = t;
        }
        if (zoom_known && zoom_ratio > 0) {
            if (t < drive_until) {
                // our own zoom coming back from the game is not the desktop's
                zoom_seen = snap.hdr.view_zoom;
            } else if (snap.hdr.view_zoom != zoom_seen && tv.follow) {
                // the desktop zoomed (its keys, a weapon's limit): the table
                // follows, about the point in front of the viewer
                zoom_seen = snap.hdr.view_zoom;
                zoom_goal = zoom_ratio * desk_half_w((float)snap.hdr.view_zoom);
            }
            if (zoom_goal > 0 && t >= drive_until) {
                float f = std::pow(zoom_goal / tv.half_map, 1.0f - std::exp(-dt * 4.0f));
                tv.zoom_at_anchor(f);
                if (std::fabs(tv.half_map / zoom_goal - 1.0f) < 0.005f)
                    zoom_goal = 0;
            } else if (t < drive_until) {
                zoom_goal = 0;
            }
        }
        if (t < drive_until)
            ;
        else if (placed)
            tv.follow_camera(snap.hdr, dt);
        // check of the alignment: the angle between the viewer's view and
        // the desktop's "up" on the table (0 when lined up)
        if (use_xr && placed && snap.valid && snap.hdr.in_mission && t - align_logged > 5.0) {
            float a = snap.hdr.cam_angle;
            glm::vec3 c(snap.hdr.cam_x, 0, snap.hdr.cam_z);
            glm::mat4 mm = tv.map_model();
            glm::vec3 w0 = glm::vec3(mm * glm::vec4(c, 1.0f));
            glm::vec3 w1 = glm::vec3(mm * glm::vec4(c + glm::vec3(std::sin(a), 0, std::cos(a)) * 1000.0f, 1.0f));
            glm::vec3 r1 = glm::vec3(mm * glm::vec4(c + glm::vec3(std::cos(a), 0, -std::sin(a)) * 1000.0f, 1.0f));
            glm::vec3 up_w = w1 - w0, rt_w = r1 - w0, head = tv.anchor - eye_world;
            float ang_up = glm::degrees(std::atan2(up_w.x, -up_w.z)), ang_head = glm::degrees(std::atan2(head.x, -head.z));
            float ang_rt = glm::degrees(std::atan2(rt_w.x, -rt_w.z));
            // logged when the desktop's "up" is off the viewer's view, as
            // while the views are unlinked or catching up
            if (std::fabs(std::remainder(glm::radians(ang_up - ang_head), 6.2831853f)) > glm::radians(15.0f))
                logf("alignment: desktop up %.0f deg, right %.0f deg, viewer looking %.0f deg, heading used %.0f deg",
                     ang_up, ang_rt, ang_head, glm::degrees(tv.heading));
            align_logged = t;
        }
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
        look.update(snap, t);
        // the HUD's text follows the look: the game's font or the vector one
        g_text.want = look.enabled ? TFK_VECTOR : TFK_GAME;
        // the fog of war: where the agents are now, what they have seen
        people.list(t, people_now);
        {
            g_fog.radius_override = fog_radius;
            g_fog.radius_scale = fog_scale;
            std::vector<glm::vec2> agents;
            for (const Picker::Person &pp : people_now)
                if ((pp.flags & SwvTF_PLAYER_AGENT) && !(pp.flags & SwvTF_DEAD))
                    agents.push_back(glm::vec2(pp.p.x, pp.p.z));
            g_fog.update(snap, agents, t);
            if (g_fog.enabled) {
                people.fog_state = [](glm::vec3 p) { return g_fog.state(p); };
                picker.visible = [](glm::vec3 p) { return g_fog.visible(p); };
            } else {
                people.fog_state = nullptr;
                picker.visible = nullptr;
            }
        }
        people.build(people_verts, glow_verts, t, snap, eye_map, rend,
                     [&](uint16_t thing) { return look.draws(thing); }, &model_places,
                     [&](const SwvEffect &e) { return look.effect_mode(e); }, &prop_places);
        look.eye_map = eye_map;
        look.prepare(t, model_places, prop_places);

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
            for (SimClick &c : sim_clicks) {
                if (!c.done && t - t_start >= c.at) {
                    c.done = true;
                    mx = mouse_x = c.x;
                    my = mouse_y = c.y;
                    (c.button == 2 ? click_r : click_l) = true;
                    if (c.button == 2)
                        right_down = true;
                    logf("simulated %s %d at %d,%d", c.hold > 0 ? "press" : "click", c.button, c.x, c.y);
                }
                if (c.done && !c.released && c.button == 2 && t - t_start >= c.at + c.hold) {
                    c.released = true;
                    right_down = false;
                }
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
        VrHud::Panels pn =
            menus ? VrHud::Panels() : vrhud.panels(hud, snap, use_xr, world_ray[0], world_ray[1], tv, eye_world, pst, t);
        const Panel &panel = pn.wrist;
        const std::vector<int> &weapon_rows = pn.weapon_rows;
        int hover_row = pn.hover_row;
        bool on_panel = pn.on_wrist;

        // what each hand points at on the table; with the fog of war, only
        // the people the agents see (the markers follow this list too)
        people.list(t, people_now);
        if (g_fog.enabled)
            people_now.erase(std::remove_if(people_now.begin(), people_now.end(),
                                            [](const Picker::Person &pp) { return !g_fog.visible(pp.p); }),
                             people_now.end());
        glm::vec3 clip_c(tv.center.x, 0, tv.center.z);
        float clip_h = tv.half_map;
        if (whole_world) {
            // the map's square: nothing lies outside it
            clip_c = glm::vec3(16384.0f, 0, 16384.0f);
            clip_h = 16384.0f + 256.0f;
        }
        Hit hit[2];
        for (int h = 0; h < 2; h++) {
            if (!world_ray[h].valid)
                continue;
            Ray mr;
            mr.o = glm::vec3(world_to_map * glm::vec4(world_ray[h].o, 1.0f));
            mr.d = glm::normalize(glm::vec3(world_to_map * glm::vec4(world_ray[h].o + world_ray[h].d, 1.0f)) - mr.o);
            mr.valid = true;
            if (!(h == 1 && on_panel))
                hit[h] = picker.pick(mr, people_now, clip_c, clip_h);
        }
        auto own_agent = [&](uint16_t thing) {
            for (const SwvAgent &ag : hd.agents)
                if (ag.thing == thing && thing != 0)
                    return true;
            return false;
        };

        // the desktop's panels: clicks as on the desktop (hud_panel.c
        // check_panel_button(), process_panel_state()). The click is the
        // right hand's main trigger (R2 on the Steam Frame); the right click R1,
        // else the left trigger while the right hand points at the panels
        // (the right mouse button on the desktop)
        bool panel_alt_used = false;
        if (pn.desk && snap.valid && snap.hdr.in_mission) {
            bool m_held = use_xr ? xr_in.trig_held[1] : dragging;
            bool a_held = use_xr ? (xr.frame_ext_ ? xr_in.alt_held : (on_panel && xr_in.trig_held[0])) : right_down;
            bool m_press = (m_held && !panel_main_was) || (act[1] && on_panel);
            bool a_press = a_held && !panel_alt_was;
            panel_alt_used = on_panel && !xr.frame_ext_;
            const DesktopPanels::Hit &ph = pn.hit;
            const SwvHudInfo &phi = snap.hud_info;
            auto thing_of = [&](int i) { return (int16_t)(i >= 0 && i < 4 ? phi.agents[i].thing : 0); };
            auto mood_of = [&](float f) { return (int32_t)std::lround(f * 176.0f - 88.0f); };
            if (pst.mood_agent >= 0) {
                // dragging the mood bar: a new mood when it changes
                bool held = pst.mood_group ? a_held : m_held;
                if (!held) {
                    pst.mood_agent = -1;
                } else if (ph.kind == DesktopPanels::Hit::MOOD && ph.agent == pst.mood_agent) {
                    int v = mood_of(ph.frac);
                    if (v != pst.mood_sent && t - pst.mood_at > 0.1) {
                        cmds.send(pst.mood_group ? SwcCmd_GROUP_SET_MOOD : SwcCmd_SET_MOOD, thing_of(pst.mood_agent),
                                  v);
                        pst.mood_sent = v;
                        pst.mood_at = t;
                    }
                }
            } else if (pst.list_agent >= 0) {
                // a weapon list is open: released over an entry, that
                // weapon; over the button, its weapon (draw / put away);
                // the other button pressed meanwhile drops the entry's
                bool group = pst.list_group;
                bool held = group ? a_held : m_held, other = group ? m_press : a_press;
                int16_t ag = thing_of(pst.list_agent);
                int w = 0;
                if (ph.kind == DesktopPanels::Hit::LIST)
                    w = ph.weapon;
                else if (ph.kind == DesktopPanels::Hit::WEAPON && ph.agent == pst.list_agent)
                    w = g_panels.button_weapon(snap, pst.list_agent);
                if (other) {
                    if (w > 0)
                        cmds.send(SwcCmd_DROP_WEAPON, ag, 0, 0, w);
                    pst.list_agent = -1;
                } else if (!held) {
                    if (w > 0)
                        cmds.send(group ? SwcCmd_GROUP_WEAPON : SwcCmd_AGENT_WEAPON, ag, 0, 0, w);
                    pst.list_agent = -1;
                }
            } else if (m_press && on_panel) {
                switch (ph.kind) {
                case DesktopPanels::Hit::BADGE:
                case DesktopPanels::Hit::HEALTH:
                case DesktopPanels::Hit::ENERGY:
                    cmds.send(SwcCmd_SELECT_AGENT, thing_of(ph.agent));
                    break;
                case DesktopPanels::Hit::MOOD:
                    pst.mood_agent = ph.agent;
                    pst.mood_group = false;
                    pst.mood_sent = mood_of(ph.frac);
                    pst.mood_at = t;
                    cmds.send(SwcCmd_SET_MOOD, thing_of(ph.agent), pst.mood_sent);
                    break;
                case DesktopPanels::Hit::WEAPON:
                    pst.list_agent = ph.agent;
                    pst.list_group = false;
                    break;
                case DesktopPanels::Hit::MEDI:
                    cmds.send(SwcCmd_USE_MEDIKIT, thing_of(ph.agent));
                    break;
                case DesktopPanels::Hit::COLUMN:
                    cmds.send(SwcCmd_SHIELD_TOGGLE);
                    break;
                case DesktopPanels::Hit::THERMAL:
                    cmds.send(SwcCmd_THERMAL_TOGGLE);
                    break;
                case DesktopPanels::Hit::GROUP:
                    cmds.send(SwcCmd_PROTECT_INC);
                    break;
                case DesktopPanels::Hit::SCANNER:
                    // the selected agent walks there, as a click on the
                    // desktop's scanner
                    cmds.send(SwcCmd_GOTO, 0, (int32_t)ph.map.x, (int32_t)ph.map.y);
                    vrhud.on_goto(ph.map.x, ph.map.y, (uint16_t)snap.hdr.selected, t);
                    break;
                default:
                    break;
                }
            } else if (a_press && on_panel) {
                switch (ph.kind) {
                case DesktopPanels::Hit::MOOD:
                    pst.mood_agent = ph.agent;
                    pst.mood_group = true;
                    pst.mood_sent = mood_of(ph.frac);
                    pst.mood_at = t;
                    cmds.send(SwcCmd_GROUP_SET_MOOD, thing_of(ph.agent), pst.mood_sent);
                    break;
                case DesktopPanels::Hit::WEAPON:
                    pst.list_agent = ph.agent;
                    pst.list_group = true;
                    break;
                case DesktopPanels::Hit::GROUP:
                case DesktopPanels::Hit::THERMAL:
                    cmds.send(SwcCmd_PROTECT_TOGGLE);
                    break;
                default:
                    break;
                }
            }
            panel_main_was = m_held;
            panel_alt_was = a_held;
        } else {
            pst = DesktopPanels::State();
        }

        // commands
        if (snap.valid && snap.hdr.in_mission) {
            if (act[1]) {
                if (on_panel && pn.desk) {
                    // done above
                } else if (on_panel) {
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
            // shooting: while the left trigger (right button) is held, the
            // shot is given every turn at what it points at, so weapons fire
            // on and charging ones charge, and fire when it is let go
            // a click held for less than a frame counts too (act[0])
            bool trigger = (use_xr ? xr_in.trig_held[0] : right_down) || act[0];
            // the left trigger (right button) is the right click on the
            // panels while the right hand points at them
            if (panel_alt_used || (pn.desk && on_panel && !use_xr))
                trigger = false;
            if (trigger) {
                SwcCommand c{};
                if ((hit[0].kind == Hit::PERSON && !own_agent(hit[0].thing)) || hit[0].kind == Hit::VEHICLE) {
                    c.type = SwcCmd_SHOOT_THING;
                    c.thing = (int16_t)hit[0].thing;
                } else if (hit[0].kind == Hit::GROUND || hit[0].kind == Hit::PERSON) {
                    c.type = SwcCmd_SHOOT_GROUND;
                    c.x = (int32_t)hit[0].p.x;
                    c.z = (int32_t)hit[0].p.z;
                }
                if (c.type != SwcCmd_NONE)
                    core.hold(&c);
            } else {
                core.hold(nullptr);
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
            // white dashes on the ground: with the fog of war, the edge of
            // what the agents see; else the most the desktop game's window
            // can show (its size at the game's furthest zoom)
            static const float frame_col[3] = {0.75f, 0.75f, 0.8f};
            // a line on the ground, half_w map units each side
            auto ground_line = [&](glm::vec2 b1, glm::vec2 b2, float half_w, const float *col) {
                glm::vec3 a1(b1.x, picker.ground_at(b1.x, b1.y) + 10, b1.y);
                glm::vec3 a2(b2.x, picker.ground_at(b2.x, b2.y) + 10, b2.y);
                if (glm::length(a2 - a1) < 1e-3f)
                    return;
                glm::vec3 side = glm::normalize(glm::cross(a2 - a1, glm::vec3(0, 1, 0))) * half_w;
                glm::vec3 q[4] = {a1 - side, a1 + side, a2 + side, a2 - side};
                static const int tri[6] = {0, 1, 2, 0, 2, 3};
                for (int kk : tri) {
                    GpuVert g{};
                    g.pos[0] = q[kk].x; g.pos[1] = q[kk].y; g.pos[2] = q[kk].z;
                    g.rgb[0] = col[0]; g.rgb[1] = col[1]; g.rgb[2] = col[2];
                    g.textured = 3.0f;
                    people_verts.push_back(g);
                }
            };
            auto dash = [&](glm::vec2 b1, glm::vec2 b2) { ground_line(b1, b2, 6.0f, frame_col); };
            float scale = snap.hdr.view_scale_min > 0 ? (float)snap.hdr.view_scale_min
                          : snap.hdr.view_scale > 0   ? (float)snap.hdr.view_scale
                                                      : 256.0f;
            float vw = (float)(snap.hdr.view_size >> 16), vh = (float)(snap.hdr.view_size & 0xFFFF);
            // yellow: what the desktop's window shows now, around its camera
            // point and turned with it (the engine's screen to map:
            // right is (cos a, -sin a), up (sin a, cos a); depth is
            // stretched by the camera's tilt)
            static std::vector<glm::vec2> outline;
            if (desktop_frame && snap.hdr.view_scale > 0) {
                static const float yellow[3] = {1.0f, 0.85f, 0.1f};
                desktop_view_outline(snap.hdr, (float)snap.hdr.view_scale, 12, outline);
                for (size_t i = 0; i < outline.size(); i++)
                    ground_line(outline[i], outline[(i + 1) % outline.size()], 4.0f, yellow);
            }
            if (g_fog.enabled) {
                g_fog.outline(dash);
            } else if (vw > 0 && vh > 0) {
                // the same outline at the furthest zoom, dashed
                desktop_view_outline(snap.hdr, scale, 8, outline);
                for (size_t i = 0; i < outline.size(); i += 2)
                    dash(outline[i], outline[(i + 1) % outline.size()]);
            }
        }

        // the game's screen: the right hand points (the mouse), its trigger
        // is the left button, the right click (R1 on the Steam Frame, else
        // the left trigger) the right one
        // The flat screens (outside missions): one hand points, the active
        // one, the right at first; a trigger of the other hand makes it the
        // active one, without clicking. Its bottom trigger (Steam Frame: R2,
        // L2) is the left button and types the keyboard's keys, its top one
        // (R1, L1) the right button.
        Panel scr_panel, kb_panel;
        bool scr_on = false, kb_on = false;
        static int act_hand = 1;
        if (menus && use_xr) {
#if VR_DESKTOP_GAME
            static bool swallow_l = false, swallow_r = false, eff_l_was = false;
            bool main_held[2] = {xr_in.trig_held[0], xr_in.trig_held[1]};
            bool alt_held[2] = {xr_in.alt_l_held, xr_in.alt_held};
            bool any_press[2] = {xr_in.trig_pressed[0] || xr_in.alt_l_pressed,
                                 xr_in.trig_pressed[1] || xr_in.alt_pressed};
            int other = 1 - act_hand;
            if (any_press[other] && world_ray[other].valid) {
                // the swap: buttons the old hand held go up, the new hand's
                // are not clicks until released
                if (scr_l)
                    core.mouse(3, scr_uv_sent.x, scr_uv_sent.y);
                if (scr_r)
                    core.mouse(5, scr_uv_sent.x, scr_uv_sent.y);
                scr_l = scr_r = false;
                act_hand = other;
                swallow_l = main_held[act_hand];
                swallow_r = alt_held[act_hand];
                eff_l_was = false;
                logf("active controller: %s", act_hand ? "right" : "left");
            }
            if (!main_held[act_hand])
                swallow_l = false;
            if (!alt_held[act_hand])
                swallow_r = false;
            bool l = main_held[act_hand] && !swallow_l, r = alt_held[act_hand] && !swallow_r;
            bool l_press = l && !eff_l_was;
            eff_l_was = l;
#endif
            const Ray &ray = world_ray[act_hand];
            scr_panel = g_screen.place(tv.table_pose);
            glm::vec2 uv(0.0f);
            scr_on = ray.valid && ScreenPanel::hit(scr_panel, ray, uv);
            g_screen.draw(hud, scr_panel, scr_on, uv);
#if VR_DESKTOP_GAME
            // a text box has the focus: the keyboard, its keys typed with
            // the bottom trigger
            static int kb_flash = -1;
            static double kb_flash_until = 0;
            if (core.text_focus()) {
                kb_panel = g_keyboard.place(tv.table_pose);
                kb_on = g_keyboard.on(kb_panel, ray);
                int k = kb_on ? g_keyboard.key_at(kb_panel, ray) : -1;
                if (k >= 0 && l_press) {
                    core.key(g_keyboard.key(k).code);
                    kb_flash = k;
                    kb_flash_until = t + 0.15;
                }
                g_keyboard.draw(hud, kb_panel, k, t < kb_flash_until ? kb_flash : -1);
            }
            if (scr_on && glm::distance(uv, scr_uv_sent) > 1e-4f) {
                core.mouse(1, uv.x, uv.y);
                scr_uv_sent = uv;
            }
            glm::vec2 at = scr_on ? uv : scr_uv_sent;
            if (l != scr_l && (scr_on || !l))
                core.mouse(l ? 2 : 3, at.x, at.y);
            if (r != scr_r && (scr_on || !r))
                core.mouse(r ? 4 : 5, at.x, at.y);
            if (scr_on || !l)
                scr_l = l;
            if (scr_on || !r)
                scr_r = r;
#endif
        }

        // pointer beams from the controllers
        if (use_xr) {
            static const float beam_r[3] = {1.0f, 0.9f, 0.3f}, beam_l[3] = {1.0f, 0.3f, 0.2f},
                               beam_blue[3] = {0.25f, 0.55f, 1.0f};
            for (int h = 0; h < 2; h++) {
                if (!world_ray[h].valid)
                    continue;
                // the flat screens: the active hand's beam alone, blue
                if (menus && h != act_hand)
                    continue;
                float len = 2.0f;
                if (menus) {
                    if (scr_on)
                        len = std::max(0.02f, scr_panel.hit_distance(world_ray[h]));
                    else if (kb_on)
                        len = std::max(0.02f, kb_panel.hit_distance(world_ray[h]));
                    hud.line(world_ray[h].o, world_ray[h].o + world_ray[h].d * len, 0.003f, eye_world, beam_blue);
                    continue;
                } else if (h == 1 && on_panel)
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

        glm::vec3 clip_center = clip_c;
        auto draw_scene = [&](const glm::mat4 &vp, bool linear) {
            if (rend.world_visible)
                glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
            else
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // the menus: a black void
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            rend.draw(vp, tv.table_model(), tv.map_model(), clip_center, clip_h, linear);
        };

        if (use_xr) {
            if (!xr.poll_events())
                break;
            XrApp::Input in = xr_in;
            if (std::fabs(in.pan.x) > 0.15f || std::fabs(in.pan.y) > 0.15f) {
                // the stick moves the view, as the viewer sees it (up: on
                // towards what is away from them), whatever the table's
                // turn: the map slides the other way
                tv.slide(-in.pan.x * dt * 0.8f, -in.pan.y * dt * 0.8f);
                view_moved = true;
            }
            if (std::fabs(in.zoom.y) > 0.2f) {
                tv.zoom_at_anchor(std::pow(0.4f, in.zoom.y * dt));
                view_moved = true;
            }
            if (std::fabs(in.zoom.x) > 0.2f) {
                tv.turn_at_anchor(-in.zoom.x * dt * 1.5f);
                view_moved = true;
            }
            if (view_moved && tv.follow)
                drive_until = t + 1.2;
            if (in.look_pressed) {
                look.enabled = !look.enabled;
                logf("look: %s", look.enabled ? "upgraded" : "classic");
            }
            if (in.fog_pressed) {
                g_fog.enabled = !g_fog.enabled;
                logf("fog of war: %s", g_fog.enabled ? "on" : "off");
            }
            if (in.world_pressed) {
                whole_world = !whole_world;
                logf("world: %s", whole_world ? "whole" : "table only");
            }
            if (in.follow_pressed)
                tv.follow = !tv.follow;
            if (in.recenter_pressed) {
                placed = false;
                place_forward = false;
            }
            if (xr.take_just_focused() || xr.take_space_changed()) {
                placed = false;
                place_forward = true;
            }
            XrTime xt = 0;
            xr.frame(draw_scene, xt);
            {
                glm::vec3 hp;
                glm::quat ho;
                if (xt != 0 && xr.head_pose(xt, hp, ho)) {
                    eye_world = hp;
                    // the point of the table in front of the viewer: along
                    // the table's forward, where a gaze 40 degrees down
                    // meets the table top, kept on the table (taken once
                    // per placement)
                    glm::vec3 fwd = glm::vec3(tv.table_pose * glm::vec4(0, 0, -1, 0));
                    fwd.y = 0;
                    if (placed && glm::length(fwd) > 0.01f) {
                        fwd = glm::normalize(fwd);
                        float top = tv.table_pose[3].y;
                        float d = std::clamp((hp.y - top) / std::tan(glm::radians(40.0f)), 0.25f, 1.2f);
                        glm::vec3 a(hp.x + fwd.x * d, top, hp.z + fwd.z * d);
                        glm::vec3 loc = glm::vec3(glm::inverse(tv.table_pose) * glm::vec4(a, 1.0f));
                        float lim = tv.table_half_m * 0.85f;
                        loc.x = std::clamp(loc.x, -lim, lim);
                        loc.z = std::clamp(loc.z, -lim, lim);
                        loc.y = 0;
                        a = glm::vec3(tv.table_pose * glm::vec4(loc, 1.0f));
                        // taken once, when the table is placed (or
                        // recentred): turning the head moves neither the
                        // desktop's view nor its outline on the table
                        if (!anchor_set) {
                            tv.anchor = a;
                            anchor_set = true;
                            // line the table up with the desktop's view
                            // from this anchor and heading
                            tv.resync();
                        }
                    }
                }
            }
            if (!placed && xt != 0) {
                glm::vec3 hp;
                glm::quat ho;
                if (xr.head_pose(xt, hp, ho)) {
                    glm::vec3 fwd = place_forward ? glm::vec3(0, 0, -1) : ho * glm::vec3(0, 0, -1);
                    fwd.y = 0;
                    if (glm::length(fwd) > 0.01f) {
                        fwd = glm::normalize(fwd);
                        glm::vec3 c = hp + fwd * 0.75f;
                        c.y = std::max(0.6f, hp.y - 0.5f);
                        float yaw = std::atan2(-fwd.x, -fwd.z);
                        tv.table_pose = glm::rotate(glm::translate(glm::mat4(1.0f), c), yaw,
                                                    glm::vec3(0, 1, 0));
                        placed = true;
                        anchor_set = false;
                        // the desktop's "up" goes along the table's front,
                        // away from the viewer (the table is turned to face
                        // them: its pose yaw is minus their heading)
                        tv.heading = -tv.pose_yaw();
                        heading_set = true;
                        tv.resync();
                        logf("table placed at %.2f %.2f %.2f", c.x, c.y, c.z);
                    }
                }
            }
            // mirror left eye to the desktop window (hidden with the desktop
            // game, which has the desktop)
            XrApp::Mirror m = xr.last_left();
            if (m.fbo && !VR_DESKTOP_GAME) {
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
            if (!VR_DESKTOP_GAME)
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
            // the desktop game without a headset (a test): its window is
            // the one shown, ours hidden and unpaced
            if (VR_DESKTOP_GAME)
                SDL_Delay(10);
        }
    }
    core.stop();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
