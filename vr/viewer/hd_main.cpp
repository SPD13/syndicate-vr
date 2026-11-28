// Syndicate Wars HD: the desktop game, with its world drawn by the viewer's
// OpenGL renderers (the original look's meshes and sprites, or the
// upgraded look's 3D models) under the game's own 2D layer.
//
// The whole desktop game runs on a thread, as in syndwars-vr.exe
// (DesktopGameWorld), and plays exactly as syndwarsfx: it still draws its
// world in software every frame (drawing keeps game state the rules and
// the pointer read). Its window is hidden; this program's window shows:
// - the old look: the game's frames as they are;
// - the upgraded look, in missions: the world from the world export,
//   projected with the game's own camera (desk_proj.h), and over it the
//   game's 2D layer, the pixels that changed after the world was drawn
//   (target circles, pointer, panels, messages). Outside missions (intro,
//   menus) the frames as they are.
// F7 switches the look at any moment (the game uses no F7); the choice is
// kept for the next run. Keys go to the game as they are; the mouse is
// mapped from this window to the game's screen.
//
// Options: --mission C,M (else the whole game, intro and menus first),
//          --data DIR (game data, default %SWARS_GAMEDATA%),
//          --look classic|upgraded, --assets DIR (the assets repository),
//          --window WxH, --screenshot FILE SECONDS (the window, then quit).

#ifndef VR_DESKTOP_GAME
#define VR_DESKTOP_GAME 1
#endif

#include "common.h"
#include "world_source.h"
#include "render_classic.h"
#include "play.h"
#include "people.h"
#include "table_view.h"
#include "look_upgraded.h"
#include "desk_window.h"

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
    std::vector<uint8_t> px((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE *f = fopen(fname, "wb");
    if (!f)
        return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; y--)
        fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
    fclose(f);
    logf("screenshot saved to %s", fname);
}

/******************************************************************************/

int main(int argc, char **argv)
{
    unsigned campaign = 0, mission = 1;
    bool mission_given = false;
    const char *data_dir = getenv("SWARS_GAMEDATA");
    int look_arg = -1;
    std::string assets_dir = getenv("SWARS_ASSETS3D") ? getenv("SWARS_ASSETS3D") : "";
    int win_w = 1280, win_h = 960;
    const char *screenshot = nullptr;
    double screenshot_after = 0;
    // tests: --click X Y SECONDS BUTTON (window points; 1 left, 3 right)
    struct SimClick {
        int x, y, button;
        double at;
        int step;
    };
    std::vector<SimClick> clicks;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mission") && i + 1 < argc) {
            if (sscanf(argv[++i], "%u,%u", &campaign, &mission) != 2) {
                logf("--mission takes campaign,mission, e.g. 0,1");
                return 1;
            }
            mission_given = true;
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--look") && i + 1 < argc) {
            look_arg = !strcmp(argv[++i], "upgraded") ? 1 : 0;
        } else if (!strcmp(argv[i], "--assets") && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (!strcmp(argv[i], "--window") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &win_w, &win_h) != 2) {
                logf("--window takes WxH, e.g. 1280x960");
                return 1;
            }
        } else if (!strcmp(argv[i], "--click") && i + 4 < argc) {
            SimClick c;
            c.x = atoi(argv[++i]);
            c.y = atoi(argv[++i]);
            c.at = atof(argv[++i]);
            c.button = atoi(argv[++i]);
            c.step = 0;
            clicks.push_back(c);
        } else if (!strcmp(argv[i], "--screenshot") && i + 2 < argc) {
            screenshot = argv[++i];
            screenshot_after = atof(argv[++i]);
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
    SDL_Window *win = SDL_CreateWindow("Syndicate Wars HD", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w,
                                       win_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_MakeCurrent(win, ctx);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        logf("glewInit failed");
        return 1;
    }
    logf("OpenGL %s on %s", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    SDL_GL_SetSwapInterval(1);
    g_hd.window_id = SDL_GetWindowID(win);

    Renderer rend;
    if (!rend.init())
        return 1;
    WorldSnapshot snap;
    People people;
    // things where the turn put them, as the desktop draws them under the
    // target circles
    people.gliding = false;
    std::vector<GpuVert> people_verts, glow_verts;
    Picker picker;
    std::vector<Picker::Person> people_now;
    g_fog.enabled = false; // the desktop has no fog of war

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
        int saved = desk_read_saved_look();
        bool up = look_arg >= 0 ? look_arg == 1 : saved == 1;
        g_hd.upgraded = up;
        look.enabled = up;
        logf("look: %s (assets %s)", up ? "upgraded" : "classic", assets_dir.c_str());
    }
    rend.draw_models = [&](const glm::mat4 &mvp, glm::vec3 cc, float ch, bool lin) { look.draw(mvp, cc, ch, lin); };
    rend.draw_effects = [&](const glm::mat4 &mvp, glm::vec3 cc, float ch, bool lin) {
        look.draw_effects(mvp, look.eye_map, cc, ch, lin);
    };
    look.ground = [&](float x, float z) { return picker.ground_at(x, z); };
    // the VR panels' and screen's samplers, unused here: on units of their
    // own (left on unit 0, a 2D sampler beside the world's texture array
    // makes the draws invalid)
    rend.bind_extra = [](GLuint prog) {
        glUniform1i(glGetUniformLocation(prog, "u_panel_tex"), 11);
        glUniform1i(glGetUniformLocation(prog, "u_scan_tex"), 12);
        glUniform1i(glGetUniformLocation(prog, "u_screen_tex"), 13);
    };
    std::vector<People::ModelPlace> model_places;
    std::vector<People::PropPlace> prop_places;
    {
        char *base = SDL_GetBasePath();
        g_text.load_vector(std::string(base ? base : "./") + "fonts/ShareTechMono-Regular.ttf");
        SDL_free(base);
    }

    // the game: its window hidden, its events seen here first
    DesktopGameWorld core;
    DeskWindow desk;
    if (!desk.init(win, core))
        return 1;
    core.set_full_game(!mission_given);
    if (!core.start(data_dir, campaign, mission, 0.05, now_seconds(), [] { g_text.load_game(); }))
        return 1;
    double t_start = now_seconds();

    while (core.running()) {
        double t = now_seconds();
        // this window's messages come to the thread that made it: into
        // SDL's queue, where the game's thread reads them
        SDL_PumpEvents();
        for (SimClick &c : clicks) {
            // the pointer there, then the button down, then up: as SDL's
            // events of this window, which the game's thread reads
            double due = t_start + c.at + c.step * 0.15;
            if (c.step > 2 || t < due)
                continue;
            SDL_Event e{};
            if (c.step == 0) {
                e.type = SDL_MOUSEMOTION;
                e.motion.windowID = g_hd.window_id;
                e.motion.x = c.x;
                e.motion.y = c.y;
            } else {
                e.type = c.step == 1 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
                e.button.windowID = g_hd.window_id;
                e.button.button = (Uint8)c.button;
                e.button.x = c.x;
                e.button.y = c.y;
            }
            SDL_PushEvent(&e);
            if (c.step == 2)
                logf("simulated click %d at %d,%d", c.button, c.x, c.y);
            c.step++;
        }
        // F7 (on the game's thread): the other look
        if (desk_take_toggle())
            logf("look: %s", g_hd.upgraded ? "upgraded" : "classic");

        if (core.poll(snap, t)) {
            g_text.set_palette(snap.palette);
            rend.upload_world(snap);
            people.update(snap, t);
            if (snap.static_changed)
                picker.update_ground(snap);
            picker.update_vehicles(snap);
        }
        desk.take_frames(core);
        desk.draw(rend, snap, [&](glm::vec3 eye_map) {
            static std::vector<SwvVert> moving;
            people.moving_geometry(t, moving);
            rend.upload_dyn(moving);
            rend.turn = snap.hdr.gameturn + people.turn_fraction(t);
            // test: SWARS_HD_GL_CLASSIC=1 draws the world in GL without the
            // models (the original meshes and sprites), to compare with the
            // game's own frame
            static const bool gl_classic = getenv("SWARS_HD_GL_CLASSIC") != nullptr;
            look.enabled = !gl_classic;
            look.update(snap, t);
            people.list(t, people_now);
            people.fog_state = nullptr;
            picker.visible = nullptr;
            people.build(people_verts, glow_verts, t, snap, eye_map, rend,
                         [&](uint16_t thing) { return look.draws(thing); }, &model_places,
                         [&](const SwvEffect &e) { return look.effect_mode(e); }, &prop_places);
            look.eye_map = eye_map;
            look.prepare(t, model_places, prop_places);
            rend.upload_people(people_verts);
            rend.upload_glows(glow_verts);
        });

        if (screenshot && t - t_start > screenshot_after) {
            int dw = 0, dh = 0;
            SDL_GL_GetDrawableSize(win, &dw, &dh);
            save_screenshot(screenshot, dw, dh);
            const DeskWindow::Frame &sh = desk.shown();
            if (sh.ready) {
                // and the game's own frame of that turn, next to it
                std::string gf = std::string(screenshot) + ".game.ppm";
                FILE *fp = fopen(gf.c_str(), "wb");
                if (fp) {
                    fprintf(fp, "P6\n%d %d\n255\n", sh.w, sh.h);
                    for (uint8_t c : sh.px)
                        fwrite(&sh.pal[3 * c], 1, 3, fp);
                    fclose(fp);
                }
            }
            screenshot = nullptr;
            SDL_Event q{};
            q.type = SDL_QUIT;
            SDL_PushEvent(&q);
        }
        SDL_GL_SwapWindow(win);
    }
    core.stop();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
