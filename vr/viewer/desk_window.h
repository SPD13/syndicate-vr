// Syndicate Wars viewer: the desktop game shown in a window of the viewer
// (syndwars-hd.exe, and the desktop window of syndwars-vr.exe): the game's
// frames as they are (the old look), or in missions the world drawn by the
// GL renderers with the game's own camera (desk_proj.h) under the game's
// 2D layer (DesktopGameWorld::set_world_layer()). F7 switches; the game's
// own window is hidden, and this window's mouse is mapped to the game's
// screen.
#pragma once

#include "common.h"
#include "desk_proj.h"
#include "look_upgraded.h"
#include "people.h"
#include "render_classic.h"
#include "table_view.h"
#include "world_source.h"

extern "C" {
// bflib (bfscreen.h, bfwindows.h): the game's window hidden, its events
// seen first here
extern unsigned char lbScreenWindowHidden;
typedef unsigned char (*HdEventFilter)(const void *sdl_event);
extern HdEventFilter lbEventFilterHook;
}

// Shared with the game's thread, which reads the events.
static struct {
    std::atomic<uint32_t> window_id{0};
    // where the game's screen is in the window (window points)
    std::atomic<int> vx{0}, vy{0}, vw{1}, vh{1};
    std::atomic<bool> upgraded{false};
    std::atomic<int> toggles{0};
} g_hd;

// The game's events, on its thread: this window's mouse becomes the
// game's (its screen's coordinates), F7 switches the look, closing the
// window quits the game.
static unsigned char hd_event_filter(const void *p)
{
    const SDL_Event *ev = static_cast<const SDL_Event *>(p);
    auto uv = [](int x, int y, float &u, float &v) {
        u = (float)(x - g_hd.vx.load()) / (float)std::max(1, g_hd.vw.load());
        v = (float)(y - g_hd.vy.load()) / (float)std::max(1, g_hd.vh.load());
    };
    float u = 0, v = 0;
    switch (ev->type) {
    case SDL_MOUSEMOTION:
        uv(ev->motion.x, ev->motion.y, u, v);
        LbMouseVirtualEvent(1, u, v);
        return 1;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        bool down = ev->type == SDL_MOUSEBUTTONDOWN;
        int act = 0;
        if (ev->button.button == SDL_BUTTON_LEFT)
            act = down ? 2 : 3;
        else if (ev->button.button == SDL_BUTTON_RIGHT)
            act = down ? 4 : 5;
        else if (ev->button.button == SDL_BUTTON_MIDDLE)
            act = down ? 6 : 7;
        uv(ev->button.x, ev->button.y, u, v);
        if (act != 0)
            LbMouseVirtualEvent(act, u, v);
        return 1;
    }
    case SDL_MOUSEWHEEL: {
        int x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        uv(x, y, u, v);
        if (ev->wheel.y != 0)
            LbMouseVirtualEvent(ev->wheel.y > 0 ? 8 : 9, u, v);
        return 1;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (ev->key.keysym.sym == SDLK_F7) {
            if (ev->type == SDL_KEYDOWN && !ev->key.repeat)
                g_hd.toggles++;
            return 1;
        }
        return 0;
    case SDL_WINDOWEVENT:
        if (ev->window.windowID == g_hd.window_id.load() && ev->window.event == SDL_WINDOWEVENT_CLOSE) {
            SDL_Event q{};
            q.type = SDL_QUIT;
            SDL_PushEvent(&q);
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

/******************************************************************************/
// The game's screen as a texture over the window: opaque, or its 2D layer
// alone (the world's pixels see-through).

class ScreenLayer {
public:
    bool init()
    {
        static const char *vs = R"(#version 330 core
out vec2 v_uv;
void main() {
    vec2 p = vec2((gl_VertexID & 1) != 0 ? 1.0 : 0.0, (gl_VertexID & 2) != 0 ? 1.0 : 0.0);
    v_uv = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
        static const char *fs = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_tex;
out vec4 o_col;
void main() {
    vec4 c = texture(u_tex, v_uv);
    if (c.a < 0.5)
        discard;
    o_col = vec4(c.rgb, 1.0);
}
)";
        prog_ = link(vs, fs);
        if (prog_ == 0)
            return false;
        glGenVertexArrays(1, &vao_);
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D, tex_);
        // the game's pixels, scaled up sharp
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    // The frame: palette indices, the mask (1: the 2D layer) or null for
    // all of it.
    void set(const std::vector<uint8_t> &px, const uint8_t *mask, int w, int h, const uint8_t *pal)
    {
        rgba_.resize((size_t)w * h * 4);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                size_t i = (size_t)y * w + x;
                uint8_t c = px[i];
                uint8_t *o = &rgba_[i * 4];
                o[0] = pal[3 * c];
                o[1] = pal[3 * c + 1];
                o[2] = pal[3 * c + 2];
                bool on = mask == nullptr || mask[i];
                if (!on && x > 0 && y > 0 && x + 1 < w && y + 1 < h) {
                    // a pixel of a panel that happens to equal the world
                    // under it: kept when the pixels around are the layer's
                    int n = mask[i - 1] + mask[i + 1] + mask[i - w] + mask[i + w];
                    on = n >= 3;
                }
                o[3] = on ? 255 : 0;
            }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba_.data());
        w_ = w;
        h_ = h;
    }

    int width() const { return w_; }
    int height() const { return h_; }

    void draw()
    {
        if (w_ <= 0)
            return;
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(prog_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_);
        glUniform1i(glGetUniformLocation(prog_, "u_tex"), 0);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
    }

private:
    static GLuint link(const char *vs, const char *fs)
    {
        auto comp = [](GLenum type, const char *src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            GLint ok = 0;
            glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
            if (!ok) {
                char log[1024];
                glGetShaderInfoLog(s, sizeof(log), nullptr, log);
                logf("screen layer shader: %s", log);
            }
            return s;
        };
        GLuint p = glCreateProgram();
        glAttachShader(p, comp(GL_VERTEX_SHADER, vs));
        glAttachShader(p, comp(GL_FRAGMENT_SHADER, fs));
        glLinkProgram(p);
        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        return ok ? p : 0;
    }

    GLuint prog_ = 0, vao_ = 0, tex_ = 0;
    std::vector<uint8_t> rgba_;
    int w_ = 0, h_ = 0;
};

/******************************************************************************/
// The window: the game's frames paired with the world of their turn, drawn
// in either look.

class DeskWindow {
public:
    // The frame shown: kept until the world of its turn is read.
    struct Frame {
        std::vector<uint8_t> px, mask;
        int w = 0, h = 0;
        uint8_t pal[768];
        bool has_world = false;
        uint32_t turn = 0;
        bool ready = false;
    };

    /** Before the game starts: its window hidden, its events seen here,
     * its 2D layer found; win is shown instead. */
    bool init(SDL_Window *win, DesktopGameWorld &core)
    {
        win_ = win;
        if (!layer_.init())
            return false;
        g_hd.window_id = SDL_GetWindowID(win);
        lbScreenWindowHidden = 1;
        lbEventFilterHook = &hd_event_filter;
        core.set_world_layer(true);
        return true;
    }

    /** Takes the game's new frames (every frame of the viewer). */
    void take_frames(DesktopGameWorld &core)
    {
        Frame f;
        if (core.screen_layer(f.px, f.mask, f.w, f.h, f.pal, frame_no_, f.has_world, f.turn)) {
            f.ready = true;
            next_ = std::move(f);
        }
        // a mission frame waits for the world of its turn (the game's world
        // and the export of the same turn, one under the other)
        if (next_.ready && (!next_.has_world || core.turns_seen() >= next_.turn)) {
            shown_ = std::move(next_);
            next_ = Frame();
            dirty_ = true;
        }
    }

    /** Whether the world is drawn in GL now (the upgraded look, in a
     * mission frame). */
    bool world(const WorldSnapshot &snap) const
    {
        return g_hd.upgraded && shown_.ready && shown_.has_world && snap.valid && snap.hdr.in_mission;
    }

    /** Draws the window (not swapped). build_world draws what the world
     * needs before rend.draw() (the people's sprites facing eye_map, the
     * models); rend draws with the desktop's camera, without the viewer's
     * own overlays and without fog. */
    void draw(Renderer &rend, const WorldSnapshot &snap, const std::function<void(glm::vec3 eye_map)> &build_world)
    {
        bool w = world(snap);
        if (w != world_was_)
            dirty_ = true;
        world_was_ = w;
        if (dirty_ && shown_.ready) {
            layer_.set(shown_.px, w ? shown_.mask.data() : nullptr, shown_.w, shown_.h, shown_.pal);
            dirty_ = false;
        }

        // the game's screen in the window, its proportions kept
        int dw = 0, dh = 0, ww = 0, wh = 0;
        SDL_GL_GetDrawableSize(win_, &dw, &dh);
        SDL_GetWindowSize(win_, &ww, &wh);
        int gw = layer_.width() > 0 ? layer_.width() : 640, gh = layer_.height() > 0 ? layer_.height() : 480;
        float s = std::min((float)dw / gw, (float)dh / gh);
        int vw = std::max(1, (int)(gw * s)), vh = std::max(1, (int)(gh * s));
        int vx = (dw - vw) / 2, vy = (dh - vh) / 2;
        {
            float k = dw > 0 ? (float)ww / dw : 1.0f; // drawable pixels to window points
            g_hd.vx = (int)(vx * k);
            g_hd.vy = (int)(vy * k);
            g_hd.vw = std::max(1, (int)(vw * k));
            g_hd.vh = std::max(1, (int)(vh * k));
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, dw, dh);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(vx, vy, vw, vh);
        if (w) {
            g_desk.set(snap.hdr);
            g_desk.on = true;
            bool fog_was = g_fog.enabled, overlays_was = rend.draw_overlays, visible_was = rend.world_visible;
            g_fog.enabled = false; // the desktop has no fog of war
            rend.draw_overlays = false;
            rend.world_visible = true;
            build_world(g_desk.eye_map());
            // the desktop's window is the whole map's clip: nothing cut
            TableView tv;
            glm::vec3 clip_c(snap.hdr.cam_x, 0, snap.hdr.cam_z);
            rend.draw(glm::mat4(1.0f), tv.table_model(), tv.map_model(), clip_c, 1.0e7f, false);
            g_fog.enabled = fog_was;
            rend.draw_overlays = overlays_was;
            rend.world_visible = visible_was;
            g_desk.on = false;
        }
        layer_.draw();
        glViewport(0, 0, dw, dh);
    }

    const Frame &shown() const { return shown_; }

private:
    SDL_Window *win_ = nullptr;
    ScreenLayer layer_;
    Frame next_, shown_;
    uint32_t frame_no_ = 0;
    bool dirty_ = false, world_was_ = false;
};

// The look chosen last time (SDL's folder for the program's settings).
static std::string desk_look_file()
{
    char *dir = SDL_GetPrefPath("SyndWarsFX", "hd");
    std::string f = std::string(dir ? dir : "./") + "look.txt";
    SDL_free(dir);
    return f;
}

static int desk_read_saved_look()
{
    FILE *f = fopen(desk_look_file().c_str(), "r");
    if (!f)
        return -1;
    char buf[32] = {0};
    int r = -1;
    if (fgets(buf, sizeof(buf), f))
        r = strncmp(buf, "upgraded", 8) == 0 ? 1 : 0;
    fclose(f);
    return r;
}

static void desk_save_look(bool upgraded)
{
    FILE *f = fopen(desk_look_file().c_str(), "w");
    if (!f)
        return;
    fputs(upgraded ? "upgraded\n" : "classic\n", f);
    fclose(f);
}

// F7 pressed since the last call (on the game's thread): the other look.
static bool desk_take_toggle()
{
    static int seen = 0;
    int tg = g_hd.toggles.load();
    if (tg == seen)
        return false;
    bool odd = ((tg - seen) & 1) != 0;
    seen = tg;
    if (odd) {
        g_hd.upgraded = !g_hd.upgraded;
        desk_save_look(g_hd.upgraded);
    }
    return odd;
}
