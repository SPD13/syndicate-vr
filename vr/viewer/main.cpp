// Syndicate Wars VR viewer.
//
// With --mission C,M it plays the mission itself: the game core library
// (swcore.h) steps the turns on a fixed clock (--turn-ms, default 50), the
// world export (swview.h) gives the world; game data from --data DIR or
// $SWARS_GAMEDATA; Enter confirms the end of a decided mission. Without it,
// it reads the world snapshot which syndwarsfx publishes in shared memory
// (see src/vrshare.h, enabled with SWARS_VR_SPECTATOR=1).
// It shows the world as a miniature on a table, in a SteamVR headset
// through OpenXR, or in a desktop window with an orbit camera (--desktop, or
// when no headset is available).
//
// Controls (VR):  right trigger: point at an agent to select it, at the ground to
//                 walk there, at a vehicle to get in, at a row of the wrist panel
//                 to pick an agent or weapon; left trigger: shoot at what the
//                 left hand points at; left A/B (X/Y): previous/next weapon;
//                 left stick pan, right stick zoom (Y) / rotate (X),
//                 A toggles following the game camera, B toggles voxel figures,
//                 left stick click recentres the table in front of you.
// Options: --desktop, --voxel, --click X Y SECONDS BUTTON (simulated click), --voxel-tol N, --voxel-dilate, --voxel-photo T,
//          --zoom HALF_MAP, --center X Z, --orbit YAW PITCH DIST,
//          --screenshot FILE SECONDS, --dump-block TURN FILE (the world block of that turn, with --mission).
// Controls (desktop): click as the right trigger, right click as the left
//                 trigger, [ ] weapons, drag mouse to orbit, wheel to change distance,
//                 arrows pan, +/- zoom, F follow, Q/E rotate the table,
//                 V voxel figures, T photo-consistency threshold, D dilation.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h> // openxr_platform.h needs IUnknown
#include <GL/glew.h>
#include <SDL2/SDL.h>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "vrshare.h"
#include "swcore.h"
#include "swview.h"
}

// the world block of libswarsview has the layout of the shared memory
static_assert(SWV_TOTAL_SIZE == VRS_TOTAL_SIZE, "swview block layout");
static_assert(SWV_OFF_HUD == VRS_OFF_HUD, "swview block layout");
// and the core takes the viewer's commands as they are
static_assert((int)SwcCmd_GOTO == (int)VrsCmd_GOTO, "command types");
static_assert((int)SwcCmd_ENTER_VEHICLE == (int)VrsCmd_ENTER_VEHICLE, "command types");

static void logf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

/******************************************************************************/
// Shared memory snapshot

struct WorldSnapshot {
    VrsHeader hdr{};
    std::vector<VrsVert> static_verts;
    std::vector<VrsVert> dyn_verts;
    std::vector<VrsThing> things;
    std::vector<VrsEffect> effects;
    std::vector<VrsLine> lines;
    std::vector<VrsDynGroup> dyn_groups;
    std::vector<VrsBar> bars;
    std::vector<VrsGlow> glows;
    std::vector<VrsShadow> shadows;
    VrsHud hud{};
    std::vector<uint8_t> tmap;
    uint8_t palette[768]{};
    // sprites and the animation tables composing them into frames
    std::vector<VrsSpriteRect> sprite_rects;
    std::vector<uint16_t> nstart;
    std::vector<VrsFrame> frames;
    std::vector<VrsElement> elements;
    std::vector<uint8_t> sprite_pages; // 2 bytes per texel: index, opacity
    uint32_t sprite_stride = 6;
    uint32_t static_rev = 0, tmap_rev = 0, sprite_rev = 0;
    bool static_changed = false, tmap_changed = false, sprite_changed = false;
    bool valid = false;
};

class SharedWorld {
public:
    /** Reads a block of this process (CoreWorld) instead of the game's
     * shared memory. */
    void attach(const uint8_t *block) { block_ = block; }

    bool poll(WorldSnapshot &snap)
    {
        if (!block_ && !open())
            return false;
        auto *hdr = reinterpret_cast<const volatile VrsHeader *>(block_);
        for (int attempt = 0; attempt < 20; attempt++) {
            uint32_t seq1 = hdr->seq;
            if (seq1 & 1) {
                Sleep(0);
                continue;
            }
            MemoryBarrier();
            VrsHeader h;
            memcpy(&h, (const void *)block_, sizeof(h));
            if (h.magic != VRS_MAGIC || h.version != VRS_VERSION)
                return false;
            bool need_static = h.static_rev != snap.static_rev;
            bool need_tmap = h.tmap_rev != snap.tmap_rev;
            bool need_sprites = h.sprite_rev != snap.sprite_rev;
            h.n_static = std::min<uint32_t>(h.n_static, VRS_STATIC_VERTS_MAX);
            h.n_dyn = std::min<uint32_t>(h.n_dyn, VRS_DYN_VERTS_MAX);
            h.n_things = std::min<uint32_t>(h.n_things, VRS_THINGS_MAX);
            if (need_static) {
                tmp_static_.resize(h.n_static);
                memcpy(tmp_static_.data(), block_ + VRS_OFF_STATIC, h.n_static * sizeof(VrsVert));
            }
            if (need_tmap) {
                tmp_tmap_.resize(VRS_TMAP_PAGES * VRS_TMAP_DIM * VRS_TMAP_DIM);
                memcpy(tmp_tmap_.data(), block_ + VRS_OFF_TMAP, tmp_tmap_.size());
                memcpy(tmp_pal_, block_ + VRS_OFF_PALETTE, 768);
            }
            if (need_sprites) {
                h.n_sprites = std::min<uint32_t>(h.n_sprites, VRS_SPRITES_MAX);
                h.n_sprite_pages = std::min<uint32_t>(h.n_sprite_pages, VRS_SPRITE_PAGES);
                h.n_nstart = std::min<uint32_t>(h.n_nstart, VRS_NSTART_MAX);
                h.n_frames = std::min<uint32_t>(h.n_frames, VRS_FRAMES_MAX);
                h.n_elements = std::min<uint32_t>(h.n_elements, VRS_ELEMENTS_MAX);
                copy_array(tmp_rects_, VRS_OFF_SPRITE_RECTS, h.n_sprites);
                copy_array(tmp_nstart_, VRS_OFF_NSTART, h.n_nstart);
                copy_array(tmp_frames_, VRS_OFF_FRAMES, h.n_frames);
                copy_array(tmp_elements_, VRS_OFF_ELEMENTS, h.n_elements);
                copy_array(tmp_pages_, VRS_OFF_SPRITE_PAGES, h.n_sprite_pages * VRS_SPRITE_PAGE_BYTES);
            }
            tmp_dyn_.resize(h.n_dyn);
            memcpy(tmp_dyn_.data(), block_ + VRS_OFF_DYN, h.n_dyn * sizeof(VrsVert));
            tmp_things_.resize(h.n_things);
            memcpy(tmp_things_.data(), block_ + VRS_OFF_THINGS, h.n_things * sizeof(VrsThing));
            h.n_effects = std::min<uint32_t>(h.n_effects, VRS_EFFECTS_MAX);
            copy_array(tmp_effects_, VRS_OFF_EFFECTS, h.n_effects);
            h.n_lines = std::min<uint32_t>(h.n_lines, VRS_LINES_MAX);
            copy_array(tmp_lines_, VRS_OFF_LINES, h.n_lines);
            copy_array(tmp_groups_, VRS_OFF_DYN_GROUPS, std::min<uint32_t>(h.n_dyn_groups, VRS_DYN_GROUPS_MAX));
            copy_array(tmp_bars_, VRS_OFF_BARS, std::min<uint32_t>(h.n_bars, VRS_BARS_MAX));
            copy_array(tmp_glows_, VRS_OFF_GLOWS, std::min<uint32_t>(h.n_glows, VRS_GLOWS_MAX));
            copy_array(tmp_shadows_, VRS_OFF_SHADOWS, std::min<uint32_t>(h.n_shadows, VRS_SHADOWS_MAX));
            memcpy(&tmp_hud_, block_ + VRS_OFF_HUD, sizeof(VrsHud));
            MemoryBarrier();
            if (hdr->seq != seq1)
                continue;
            snap.hdr = h;
            snap.static_changed = need_static;
            snap.tmap_changed = need_tmap;
            if (need_static) {
                snap.static_verts.swap(tmp_static_);
                snap.static_rev = h.static_rev;
            }
            if (need_tmap) {
                snap.tmap.swap(tmp_tmap_);
                memcpy(snap.palette, tmp_pal_, 768);
                snap.tmap_rev = h.tmap_rev;
            }
            snap.sprite_changed = need_sprites;
            if (need_sprites) {
                snap.sprite_rects.swap(tmp_rects_);
                snap.nstart.swap(tmp_nstart_);
                snap.frames.swap(tmp_frames_);
                snap.elements.swap(tmp_elements_);
                snap.sprite_pages.swap(tmp_pages_);
                snap.sprite_stride = std::max<uint32_t>(h.sprite_stride, 1);
                snap.sprite_rev = h.sprite_rev;
            }
            snap.dyn_verts.swap(tmp_dyn_);
            snap.things.swap(tmp_things_);
            snap.effects.swap(tmp_effects_);
            snap.lines.swap(tmp_lines_);
            snap.dyn_groups.swap(tmp_groups_);
            snap.bars.swap(tmp_bars_);
            snap.glows.swap(tmp_glows_);
            snap.shadows.swap(tmp_shadows_);
            snap.hud = tmp_hud_;
            snap.valid = true;
            return true;
        }
        return false;
    }

private:
    template <typename T>
    void copy_array(std::vector<T> &dst, uint32_t off, uint32_t n)
    {
        dst.resize(n);
        memcpy(dst.data(), block_ + off, n * sizeof(T));
    }

    bool open()
    {
        DWORD now = GetTickCount();
        if (now - last_try_ < 1000)
            return false;
        last_try_ = now;
        HANDLE h = OpenFileMappingA(FILE_MAP_READ, FALSE, VRS_MAPPING_NAME);
        if (!h)
            return false;
        block_ = static_cast<const uint8_t *>(MapViewOfFile(h, FILE_MAP_READ, 0, 0, VRS_TOTAL_SIZE));
        if (block_)
            logf("connected to game shared memory");
        return block_ != nullptr;
    }

    const uint8_t *block_ = nullptr;
    DWORD last_try_ = 0;
    std::vector<VrsVert> tmp_static_, tmp_dyn_;
    std::vector<VrsThing> tmp_things_;
    std::vector<VrsEffect> tmp_effects_;
    std::vector<VrsLine> tmp_lines_;
    std::vector<VrsDynGroup> tmp_groups_;
    std::vector<VrsBar> tmp_bars_;
    std::vector<VrsGlow> tmp_glows_;
    std::vector<VrsShadow> tmp_shadows_;
    VrsHud tmp_hud_{};
    std::vector<uint8_t> tmp_tmap_;
    uint8_t tmp_pal_[768];
    std::vector<VrsSpriteRect> tmp_rects_;
    std::vector<uint16_t> tmp_nstart_;
    std::vector<VrsFrame> tmp_frames_;
    std::vector<VrsElement> tmp_elements_;
    std::vector<uint8_t> tmp_pages_;
};

/******************************************************************************/
// The game in this process: the core library steps the mission, the world
// export (libswarsview) fills a block read as the shared memory is

class CoreWorld {
public:
    /** Starts the mission with the game data in data_dir (the core reads
     * its files relative to the current directory). */
    bool start(const char *data_dir, unsigned campaign, unsigned mission, double turn_seconds, double now)
    {
        if (data_dir && !SetCurrentDirectoryA(data_dir)) {
            logf("cannot use game data folder %s", data_dir);
            return false;
        }
        if (swcore_core_init(campaign, mission) != 0) {
            logf("cannot start mission %u,%u", campaign, mission);
            return false;
        }
        int missing = swview_load_assets();
        if (missing != 0)
            logf("%d asset files missing", missing);
        block_.assign(swview_block_size(), 0);
        swview_update(block_.data());
        reader_.attach(block_.data());
        turn_seconds_ = turn_seconds;
        last_turn_ = now;
        running_ = true;
        fresh_ = true;
        logf("game core: mission %u,%u, API %u, view API %u", campaign, mission,
             (unsigned)SWCORE_API_VERSION, (unsigned)SWVIEW_API_VERSION);
        return true;
    }

    /** Plays the turns due by now, on a fixed clock, and reads the world
     * when it changed. */
    bool poll(WorldSnapshot &snap, double now)
    {
        int n = 0;
        while (running_ && (now - last_turn_ >= turn_seconds_) && (n < 4)) {
            last_turn_ += turn_seconds_;
            n++;
            if (!swcore_core_step()) {
                running_ = false;
                struct SwcWorldInfo info;
                swcore_world_info(&info);
                logf("mission over at turn %u, status %u", info.gameturn, info.mission_status);
            }
            swview_update(block_.data());
            if (dump_file_ != nullptr)
                dump_block();
        }
        if (n == 4)
            last_turn_ = now; // the game cannot keep up: no catching up
        if ((n == 0) && !fresh_)
            return false;
        fresh_ = false;
        return reader_.poll(snap);
    }

    /** The player confirms the end of a decided mission. */
    void conclude()
    {
        struct SwcWorldInfo info;
        swcore_world_info(&info);
        if (running_ && ((info.mission_status == SwcMS_WON) || (info.mission_status == SwcMS_LOST)))
            swcore_mission_conclude();
    }

    /** Writes the world block of that turn to fname, for comparisons. */
    void dump_at(uint32_t turn, const char *fname)
    {
        dump_turn_ = turn;
        dump_file_ = fname;
    }

    bool running() const { return running_; }
    void stop()
    {
        swview_free_assets();
        swcore_core_shutdown();
    }

private:
    void dump_block()
    {
        struct SwcWorldInfo info;
        swcore_world_info(&info);
        if (info.gameturn != dump_turn_)
            return;
        FILE *f = fopen(dump_file_, "wb");
        if (f != nullptr) {
            fwrite(block_.data(), 1, block_.size(), f);
            fclose(f);
            logf("world block of turn %u written to %s", info.gameturn, dump_file_);
        }
        dump_file_ = nullptr;
    }

    std::vector<uint8_t> block_;
    SharedWorld reader_;
    uint32_t dump_turn_ = 0;
    const char *dump_file_ = nullptr;
    double turn_seconds_ = 0.05, last_turn_ = 0;
    bool running_ = false, fresh_ = false;
};

/******************************************************************************/
// Rendering

struct GpuVert {
    float pos[3];
    float uv[2];
    float layer;
    float rgb[3];
    // 0 flat colour, 1 texture page (tinted by rgb), 2 sprite page (tinted
    // by rgb), 3 unlit colour, 4 shadow (translucent black), 5 soft glow
    float textured;
    float fx; // VrsVertFlags bits, for water, glass and shine
};

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(sh, sizeof(buf), nullptr, buf);
        logf("shader error: %s", buf);
    }
    return sh;
}

static const char *kVertSrc = R"(#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in float a_layer;
layout(location=3) in vec3 a_rgb;
layout(location=4) in float a_textured;
layout(location=5) in float a_fx;
uniform mat4 u_mvp;
uniform mat4 u_inst;
uniform float u_turn;
out vec3 v_map;
flat out int v_fx;
out float v_shimmer;
out vec2 v_uv;
flat out float v_layer;
out vec3 v_rgb;
flat out float v_textured;
void main() {
    vec3 p = a_pos;
    int fx = int(a_fx + 0.5);
    v_fx = fx;
    v_shimmer = 0.0;
    if ((fx & 4) != 0) {
        // ripple as floor_wobble_at_turn(): three waves over 32 turns, the
        // ground moving by 4 units per step and brightening with it
        float w = 28.0 * (sin(6.2831853 * (u_turn + p.x / 128.0) / 32.0)
                        + sin(6.2831853 * (u_turn + p.z / 128.0) / 32.0)
                        + sin(6.2831853 * u_turn / 200.0)) / 8.0;
        p.y += 4.0 * w;
        v_shimmer = (w + 32.0) / 32.0;
    } else if ((fx & 8) != 0) {
        p.y += 28.0 * sin(6.2831853 * u_turn / 32.0);
    }
    vec4 m = u_inst * vec4(p, 1.0);
    v_map = m.xyz;
    v_uv = a_uv;
    v_layer = a_layer;
    v_rgb = a_rgb;
    v_textured = a_textured;
    gl_Position = u_mvp * m;
}
)";

static const char *kFragSrc = R"(#version 330 core
in vec3 v_map;
in vec2 v_uv;
flat in float v_layer;
in vec3 v_rgb;
flat in float v_textured;
flat in int v_fx;
in float v_shimmer;
uniform sampler2DArray u_tex;
uniform sampler2DArray u_spr;
uniform vec3 u_clip_center;
uniform float u_clip_half;
uniform int u_clip;
uniform int u_linear_out;
uniform vec3 u_light_dir;
uniform mat3 u_normal_mat;
uniform float u_tint;
out vec4 o_col;
void main() {
    if (u_clip != 0) {
        vec2 d = abs(v_map.xz - u_clip_center.xz);
        if (d.x > u_clip_half || d.y > u_clip_half)
            discard;
    }
    vec3 col = v_rgb;
    if (v_textured > 4.5) {
        // soft round glow on a quad facing the viewer
        float r = length(v_uv * 2.0 - 1.0);
        if (r > 1.0)
            discard;
        if (u_linear_out != 0)
            col = pow(col, vec3(2.2));
        o_col = vec4(col, 0.6 * (1.0 - r) * (1.0 - r));
        return;
    }
    if (v_textured > 3.5) {
        o_col = vec4(0.0, 0.0, 0.0, 0.45);
        return;
    }
    if (v_textured > 2.5) {
        // lines glow regardless of lighting
        if (u_linear_out != 0)
            col = pow(col, vec3(2.2));
        o_col = vec4(col, 1.0);
        return;
    }
    if (v_textured > 1.5) {
        // sprites face the viewer and carry their own shading
        vec4 t = texture(u_spr, vec3(v_uv, v_layer));
        if (t.a < 0.5)
            discard;
        col = t.rgb * v_rgb;
        if (u_linear_out != 0)
            col = pow(col, vec3(2.2));
        o_col = vec4(col, 1.0);
        return;
    }
    if (v_textured > 0.5) {
        vec4 t = texture(u_tex, vec3(v_uv, v_layer));
        // rgb carries the game's light level for textured faces
        col = t.rgb * (v_rgb + vec3(v_shimmer));
    }
    vec3 n = normalize(cross(dFdx(v_map), dFdy(v_map)));
    if (n.y < 0.0) n = -n;
    float diff = abs(dot(n, normalize(u_light_dir)));
    col *= (0.55 + 0.6 * diff) * u_tint;
    float alpha = 1.0;
    if ((v_fx & 16) != 0)
        col = mix(col, vec3(0.55, 0.65, 0.8), 0.35) + vec3(0.06);
    if ((v_fx & 2) != 0)
        alpha = 0.5;
    if (u_linear_out != 0)
        col = pow(col, vec3(2.2));
    o_col = vec4(col, alpha);
}
)";

class Renderer {
public:
    bool init()
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, kVertSrc);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kFragSrc);
        prog_ = glCreateProgram();
        glAttachShader(prog_, vs);
        glAttachShader(prog_, fs);
        glLinkProgram(prog_);
        GLint ok = 0;
        glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
        if (!ok) {
            logf("program link failed");
            return false;
        }
        u_mvp_ = glGetUniformLocation(prog_, "u_mvp");
        u_clip_center_ = glGetUniformLocation(prog_, "u_clip_center");
        u_clip_half_ = glGetUniformLocation(prog_, "u_clip_half");
        u_clip_ = glGetUniformLocation(prog_, "u_clip");
        u_linear_ = glGetUniformLocation(prog_, "u_linear_out");
        u_light_ = glGetUniformLocation(prog_, "u_light_dir");
        u_inst_ = glGetUniformLocation(prog_, "u_inst");
        u_tint_ = glGetUniformLocation(prog_, "u_tint");
        u_turn_ = glGetUniformLocation(prog_, "u_turn");
        for (auto *m : {&static_mesh_, &static_trans_, &dyn_mesh_, &dyn_trans_, &people_mesh_,
                        &table_mesh_, &glow_mesh_, &hud_mesh_})
            make_mesh(*m);
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, VRS_TMAP_DIM, VRS_TMAP_DIM, VRS_TMAP_PAGES,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenTextures(1, &spr_tex_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, spr_tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, VRS_TMAP_DIM, VRS_TMAP_DIM, 1,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    void palette_rgb(const uint8_t *pal, uint8_t idx, float *rgb) const
    {
        for (int k = 0; k < 3; k++)
            rgb[k] = std::min(1.0f, pal[idx * 3 + k] * (pal_scale_ / 255.0f));
    }

    void upload_world(const WorldSnapshot &snap)
    {
        if (snap.tmap_changed) {
            memcpy(palette_, snap.palette, 768);
            uint8_t maxc = 0;
            for (int i = 0; i < 768; i++)
                maxc = std::max(maxc, palette_[i]);
            pal_scale_ = (maxc <= 63) ? 4.0f : 1.0f; // 6-bit VGA palette
            std::vector<uint8_t> rgba(snap.tmap.size() * 4);
            for (size_t i = 0; i < snap.tmap.size(); i++) {
                uint8_t c = snap.tmap[i];
                for (int k = 0; k < 3; k++)
                    rgba[i * 4 + k] = (uint8_t)std::min(255.0f, palette_[c * 3 + k] * pal_scale_);
                rgba[i * 4 + 3] = 255;
            }
            glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, VRS_TMAP_DIM, VRS_TMAP_DIM,
                            VRS_TMAP_PAGES, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            have_palette_ = true;
        }
        if ((snap.sprite_changed || snap.tmap_changed) && !snap.sprite_pages.empty())
            upload_sprites(snap);
        if (snap.static_changed || snap.tmap_changed) {
            upload_verts(static_mesh_, static_trans_, snap.static_verts);
            // the terrain is the first part of the static vertices; take a
            // low percentile, so roads are above the table but small pits
            // and the map edge do not lift everything
            size_t n_ground = std::min<size_t>(snap.static_verts.size(), 127 * 127 * 6);
            if (n_ground > 0) {
                std::vector<float> ys(n_ground);
                for (size_t i = 0; i < n_ground; i++)
                    ys[i] = snap.static_verts[i].y;
                std::nth_element(ys.begin(), ys.begin() + n_ground / 50, ys.end());
                ground_low = ys[n_ground / 50];
            }
        }
    }

    float ground_low = -64.0f; // lowest terrain height, map units

    // Moving geometry, interpolated by the caller every frame.
    void upload_dyn(const std::vector<VrsVert> &verts) { upload_verts(dyn_mesh_, dyn_trans_, verts); }

    float turn = 0; // game turn with fraction, for animated terrain

    void upload_sprites(const WorldSnapshot &snap)
    {
        size_t texels = snap.sprite_pages.size() / 2;
        int pages = (int)(texels / (VRS_TMAP_DIM * VRS_TMAP_DIM));
        std::vector<uint8_t> rgba(texels * 4);
        for (size_t i = 0; i < texels; i++) {
            uint8_t c = snap.sprite_pages[i * 2];
            for (int k = 0; k < 3; k++)
                rgba[i * 4 + k] = (uint8_t)std::min(255.0f, palette_[c * 3 + k] * pal_scale_);
            rgba[i * 4 + 3] = snap.sprite_pages[i * 2 + 1];
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, spr_tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, VRS_TMAP_DIM, VRS_TMAP_DIM, pages,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        logf("sprites: %zu in %d pages, %zu frames", snap.sprite_rects.size(), pages,
             snap.frames.size());
    }

    void upload_people(const std::vector<GpuVert> &verts) { upload_gpu(people_mesh_, verts); }
    void upload_glows(const std::vector<GpuVert> &verts) { upload_gpu(glow_mesh_, verts); }
    // Pointers and panels, in world space (meters).
    void upload_hud(const std::vector<GpuVert> &verts) { upload_gpu(hud_mesh_, verts); }

    void build_table(float half_map, float thickness_map)
    {
        // table slab in map units around origin, top at y = 0
        std::vector<GpuVert> v;
        float wood[3] = {0.36f, 0.24f, 0.14f};
        float rim = half_map * 1.06f;
        add_box(v, glm::vec3(-rim, -thickness_map, -rim), glm::vec3(rim, -1.0f, rim), wood);
        float leg[3] = {0.25f, 0.17f, 0.10f};
        float legw = rim * 0.05f, legh = thickness_map * 25.0f;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sz = -1; sz <= 1; sz += 2) {
                glm::vec3 c(sx * (rim - legw * 2), 0, sz * (rim - legw * 2));
                add_box(v, c + glm::vec3(-legw, -thickness_map - legh, -legw),
                        c + glm::vec3(legw, -thickness_map, legw), leg);
            }
        upload_gpu(table_mesh_, v);
    }

    static void add_box(std::vector<GpuVert> &v, glm::vec3 a, glm::vec3 b, const float *rgb)
    {
        glm::vec3 c[8] = {{a.x, a.y, a.z}, {b.x, a.y, a.z}, {b.x, b.y, a.z}, {a.x, b.y, a.z},
                          {a.x, a.y, b.z}, {b.x, a.y, b.z}, {b.x, b.y, b.z}, {a.x, b.y, b.z}};
        static const int idx[36] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
                                    3, 2, 6, 3, 6, 7, 0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2};
        for (int i : idx) {
            GpuVert g{};
            g.pos[0] = c[i].x; g.pos[1] = c[i].y; g.pos[2] = c[i].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            v.push_back(g);
        }
    }

    // model: map units -> world (meters); vp: projection*view
    void draw(const glm::mat4 &vp, const glm::mat4 &table_model, const glm::mat4 &map_model,
              glm::vec3 clip_center, float clip_half, bool linear_out)
    {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glUseProgram(prog_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
        glUniform1i(glGetUniformLocation(prog_, "u_tex"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D_ARRAY, spr_tex_);
        glUniform1i(glGetUniformLocation(prog_, "u_spr"), 1);
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(u_linear_, linear_out ? 1 : 0);
        glUniform3f(u_light_, 0.4f, 1.0f, 0.3f);
        glUniform3f(u_clip_center_, clip_center.x, clip_center.y, clip_center.z);
        glUniform1f(u_clip_half_, clip_half);
        glm::mat4 ident(1.0f);
        glUniformMatrix4fv(u_inst_, 1, GL_FALSE, glm::value_ptr(ident));
        glUniform1f(u_tint_, 1.0f);
        glUniform1f(u_turn_, turn);

        glm::mat4 mvp = vp * table_model;
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(u_clip_, 0);
        draw_mesh(table_mesh_);

        mvp = vp * map_model;
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp));
        glUniform1i(u_clip_, 1);
        draw_mesh(static_mesh_);
        draw_mesh(dyn_mesh_);
        for (const Instance &in : instances) {
            glUniformMatrix4fv(u_inst_, 1, GL_FALSE, glm::value_ptr(in.model));
            glUniform1f(u_tint_, in.tint);
            glBindVertexArray(in.vao);
            glDrawArrays(GL_TRIANGLES, 0, in.count);
        }
        glUniformMatrix4fv(u_inst_, 1, GL_FALSE, glm::value_ptr(ident));
        glUniform1f(u_tint_, 1.0f);
        // people mesh holds translucent shadows, drawn first within it
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_mesh(people_mesh_);
        // see-through faces last, without hiding what is behind them
        glDepthMask(GL_FALSE);
        draw_mesh(static_trans_);
        draw_mesh(dyn_trans_);
        // glows add light, hidden by solid things in front but hiding nothing
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        draw_mesh(glow_mesh_);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(vp));
        glUniform1i(u_clip_, 0);
        draw_mesh(hud_mesh_);
        glDisable(GL_BLEND);
    }

    const uint8_t *palette() const { return palette_; }
    float pal_scale() const { return pal_scale_; }

private:
    struct Mesh {
        GLuint vao = 0, vbo = 0;
        GLsizei count = 0;
    };

    void make_mesh(Mesh &m)
    {
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        setup_vao(m.vao, m.vbo);
        glBindVertexArray(0);
    }

public:
    // Sets up the GpuVert layout; leaves the VAO and VBO bound.
    static void setup_vao(GLuint vao, GLuint vbo)
    {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        GLsizei st = sizeof(GpuVert);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, layer));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, rgb));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, textured));
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, st, (void *)offsetof(GpuVert, fx));
    }

    // A prebuilt mesh placed on the map, such as a voxel figure.
    struct Instance {
        GLuint vao;
        GLsizei count;
        glm::mat4 model;
        float tint;
    };
    std::vector<Instance> instances;

private:

    // Splits see-through faces into their own mesh, drawn after the rest.
    void upload_verts(Mesh &opaque, Mesh &trans, const std::vector<VrsVert> &src)
    {
        std::vector<GpuVert> vo, vt;
        vo.reserve(src.size());
        for (size_t i = 0; i + 2 < src.size(); i += 3) {
            bool see_through = (src[i].flags & VrsVF_TRANSLUCENT) != 0;
            for (size_t k = i; k < i + 3; k++) {
                const VrsVert &s = src[k];
                GpuVert g{};
                g.pos[0] = s.x; g.pos[1] = s.y; g.pos[2] = s.z;
                g.uv[0] = (s.u + 0.5f) / VRS_TMAP_DIM;
                g.uv[1] = (s.v + 0.5f) / VRS_TMAP_DIM;
                g.layer = s.page;
                g.textured = (s.flags & VrsVF_TEXTURED) ? 1.0f : 0.0f;
                g.fx = (float)s.flags;
                // the game's light level, 128 is unchanged
                float light = std::clamp(s.shade / 128.0f, 0.15f, 1.9f);
                if (s.flags & VrsVF_TEXTURED) {
                    g.rgb[0] = g.rgb[1] = g.rgb[2] = light;
                } else {
                    palette_rgb(palette_, s.colour, g.rgb);
                    for (float &c : g.rgb)
                        c *= light;
                }
                (see_through ? vt : vo).push_back(g);
            }
        }
        upload_gpu(opaque, vo);
        upload_gpu(trans, vt);
    }

    void upload_gpu(Mesh &m, const std::vector<GpuVert> &v)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(GpuVert), v.data(), GL_DYNAMIC_DRAW);
        m.count = (GLsizei)v.size();
    }

    void draw_mesh(const Mesh &m)
    {
        if (m.count == 0)
            return;
        glBindVertexArray(m.vao);
        glDrawArrays(GL_TRIANGLES, 0, m.count);
    }

    GLuint prog_ = 0, tex_ = 0, spr_tex_ = 0;
    GLint u_mvp_ = -1, u_clip_center_ = -1, u_clip_half_ = -1, u_clip_ = -1, u_linear_ = -1,
          u_light_ = -1, u_inst_ = -1, u_tint_ = -1, u_turn_ = -1;
    Mesh static_mesh_, static_trans_, dyn_mesh_, dyn_trans_, people_mesh_, table_mesh_, glow_mesh_,
        hud_mesh_;
    uint8_t palette_[768]{};
    float pal_scale_ = 4.0f;
    bool have_palette_ = false;
};

/******************************************************************************/
// Voxel figures carved from the directional sprites
//
// A person's frame exists in 8 (dead bodies: 12) views around them, all from
// the game camera's 30.3 degree tilt. Carving a block of voxels down to the
// silhouette of every view gives a solid figure (its visual hull). The hull
// fills hollows which no silhouette shows, so it is then carved further by
// photo-consistency: a surface voxel which the views showing it disagree on
// the colour of is not on the real surface, and goes, as long as every
// silhouette stays covered. Surface voxels finally take the colour from the
// view which sees them best. Models are built on a worker thread.

// Game camera tilt, as used for sprites: cos and sin of 30.3 degrees.
static constexpr float kTiltCos = 0.8634f;
static constexpr float kTiltSin = 0.5045f;

// Projection of a point, relative to the figure's feet, into the sprite of a
// game camera with yaw a (radians), as transform_shpoint() at scale 256.
// Returns sprite pixel offsets from the anchor and the depth along the view.
static void game_project(float a, float x, float y, float z, float &sx, float &sy, float &depth)
{
    float ca = std::cos(a), sa = std::sin(a);
    float fa = ca * x - sa * z;
    float fb = sa * x + ca * z;
    float fc = kTiltCos * y + kTiltSin * fb;
    sx = fa / 8.0f;
    sy = -fc / 8.0f;
    depth = kTiltCos * fb - kTiltSin * y;
}

// What the builder needs from the snapshot, shared read-only with the worker.
struct SpriteData {
    std::vector<VrsSpriteRect> rects;
    std::vector<uint16_t> nstart;
    std::vector<VrsFrame> frames;
    std::vector<VrsElement> elements;
    std::vector<uint8_t> pages;
    uint32_t stride = 6;
    float rgb[256][3]{};
};

struct VoxelParams {
    int tolerance = 0;     // views a voxel may fall outside of and still stay
    bool dilate = false;   // treat pixels next to the silhouette as inside
    float photo = 0.0f;    // colour disagreement which carves, 0 disables
    int max_depth = 3;     // voxels into the silhouette hull it may carve
};

class VoxelModels {
public:
    struct Model {
        GLuint vao = 0, vbo = 0;
        GLsizei count = 0;
        float top = 0;   // height of the highest voxel, map units
        bool ok = false; // false while building, or if nothing usable was left
    };

    VoxelParams params;

    VoxelModels() { worker_ = std::thread([this] { work(); }); }

    ~VoxelModels()
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            quit_ = true;
        }
        cv_.notify_all();
        worker_.join();
    }

    // New sprites or palette: drop all models, build from the new data.
    void reset(const WorldSnapshot &snap, const Renderer &rend)
    {
        auto d = std::make_shared<SpriteData>();
        d->rects = snap.sprite_rects;
        d->nstart = snap.nstart;
        d->frames = snap.frames;
        d->elements = snap.elements;
        d->pages = snap.sprite_pages;
        d->stride = snap.sprite_stride;
        for (int c = 0; c < 256; c++)
            rend.palette_rgb(rend.palette(), (uint8_t)c, d->rgb[c]);
        std::lock_guard<std::mutex> lk(mtx_);
        data_ = d;
        generation_++;
        jobs_.clear();
        done_.clear();
        for (auto &kv : models_)
            if (kv.second.vbo) {
                glDeleteBuffers(1, &kv.second.vbo);
                glDeleteVertexArrays(1, &kv.second.vao);
            }
        models_.clear();
    }

    // Drop models after a parameter change.
    void rebuild()
    {
        std::shared_ptr<const SpriteData> d;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            d = data_;
        }
        if (!d)
            return;
        std::lock_guard<std::mutex> lk(mtx_);
        generation_++;
        jobs_.clear();
        done_.clear();
        for (auto &kv : models_)
            if (kv.second.vbo) {
                glDeleteBuffers(1, &kv.second.vbo);
                glDeleteVertexArrays(1, &kv.second.vao);
            }
        models_.clear();
    }

    // Uploads models the worker has finished; call once per frame.
    void collect()
    {
        std::vector<Done> done;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            done.swap(done_);
        }
        for (Done &d : done) {
            auto it = models_.find(d.key);
            if (it == models_.end() || d.verts.empty())
                continue;
            Model &m = it->second;
            glGenVertexArrays(1, &m.vao);
            glGenBuffers(1, &m.vbo);
            Renderer::setup_vao(m.vao, m.vbo);
            glBufferData(GL_ARRAY_BUFFER, d.verts.size() * sizeof(GpuVert), d.verts.data(),
                         GL_STATIC_DRAW);
            glBindVertexArray(0);
            m.count = (GLsizei)d.verts.size();
            m.top = d.top;
            m.ok = true;
        }
    }

    // Model for a person in its current animation state, or nullptr when it
    // is not available (yet). rot_steps receives the facing of the figure in
    // views, and n_views the number of views around it.
    const Model *get(const VrsThing &t, int &rot_steps, int &n_views)
    {
        if (t.anim_mode == VrsAM_DIR8) {
            n_views = 8;
            rot_steps = t.angle & 7;
        } else if (t.anim_mode == VrsAM_DIR12) {
            n_views = 12;
            rot_steps = (3 * t.angle >> 1) % 12;
        } else {
            return nullptr;
        }
        uint64_t key = (uint64_t)t.anim_start | ((uint64_t)t.subframe << 16) |
                       ((uint64_t)n_views << 24);
        for (int i = 0; i < 5; i++)
            key |= (uint64_t)(t.frv[i] & 7) << (32 + 3 * i);
        auto it = models_.find(key);
        if (it != models_.end())
            return it->second.ok ? &it->second : nullptr;
        models_[key] = Model();
        Job j;
        j.key = key;
        j.anim_start = t.anim_start;
        j.subframe = t.subframe;
        j.n_views = n_views;
        memcpy(j.frv, t.frv, 5);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            j.generation = generation_;
            j.params = params;
            jobs_.push_back(j);
        }
        cv_.notify_one();
        return nullptr;
    }

private:
    struct Job {
        uint64_t key;
        uint32_t generation;
        uint16_t anim_start;
        uint8_t subframe;
        int n_views;
        uint8_t frv[5];
        VoxelParams params;
    };
    struct Done {
        uint64_t key;
        std::vector<GpuVert> verts;
        float top;
    };

    void work()
    {
        for (;;) {
            Job j;
            std::shared_ptr<const SpriteData> d;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return quit_ || !jobs_.empty(); });
                if (quit_)
                    return;
                j = jobs_.front();
                jobs_.pop_front();
                d = data_;
            }
            Done r{j.key, {}, 0};
            if (d)
                build(*d, j, r);
            std::lock_guard<std::mutex> lk(mtx_);
            if (j.generation == generation_)
                done_.push_back(std::move(r));
        }
    }

    // Frame composed into an image of palette indices, -1 where empty.
    struct Image {
        static constexpr int W = 160, H = 160, OX = 80, OY = 120;
        std::vector<int16_t> px = std::vector<int16_t>(W * H, -1);
        int at(int x, int y) const
        {
            x += OX;
            y += OY;
            if (x < 0 || y < 0 || x >= W || y >= H)
                return -1;
            return px[y * W + x];
        }
    };

    static bool compose(const SpriteData &sd, int frm, const uint8_t *frv, Image &img)
    {
        if (frm <= 0 || frm >= (int)sd.frames.size())
            return false;
        bool any = false;
        int guard = 0;
        for (int el = sd.frames[frm].first_element;
             el > 0 && el < (int)sd.elements.size() && guard < 64;
             el = sd.elements[el].next, guard++) {
            const VrsElement &e = sd.elements[el];
            if (e.to_sprite == 0 || e.to_sprite % sd.stride != 0)
                continue;
            uint32_t si = e.to_sprite / sd.stride;
            if (si >= sd.rects.size())
                continue;
            int slot = (e.flags >> 4) & 0x1F;
            int ver = slot < 5 ? frv[slot] : 0;
            if (ver != ((e.flags >> 9) & 7))
                continue;
            const VrsSpriteRect &r = sd.rects[si];
            size_t page_off = (size_t)r.page * VRS_TMAP_DIM * VRS_TMAP_DIM;
            if ((page_off + VRS_TMAP_DIM * VRS_TMAP_DIM) * 2 > sd.pages.size())
                continue;
            for (int v = 0; v < r.h; v++)
                for (int u = 0; u < r.w; u++) {
                    size_t ti = page_off + (size_t)(r.y + v) * VRS_TMAP_DIM + (r.x + u);
                    if (sd.pages[ti * 2 + 1] == 0)
                        continue;
                    int su = (e.flags & 0x01) ? (r.w - 1 - u) : u;
                    int sv = (e.flags & 0x02) ? (r.h - 1 - v) : v;
                    int x = (e.x >> 1) + su + Image::OX, y = (e.y >> 1) + sv + Image::OY;
                    if (x < 0 || y < 0 || x >= Image::W || y >= Image::H)
                        continue;
                    // later elements are drawn over earlier ones
                    img.px[y * Image::W + x] = sd.pages[ti * 2];
                    any = true;
                }
        }
        return any;
    }

    static void build(const SpriteData &sd, const Job &j, Done &r)
    {
        const int nv = j.n_views;
        const VoxelParams &prm = j.params;
        // views of the figure facing angle 0: view d is what a camera with
        // yaw step (n - d) % n sees, as the game picks the frame
        std::vector<Image> imgs(nv);
        std::vector<float> yaw(nv);
        for (int d = 0; d < nv; d++) {
            int idx = j.anim_start + 1 + d;
            if (idx >= (int)sd.nstart.size())
                return;
            if (!compose(sd, sd.nstart[idx] + j.subframe, j.frv, imgs[d]))
                return;
            yaw[d] = ((nv - d) % nv) * 6.2831853f / nv;
        }

        // voxel grid around the feet, one sprite pixel per voxel
        const float vs = 8.0f;
        const int NX = 36, NY = 52, NZ = 36, N = NX * NY * NZ;
        const float x0 = -NX / 2 * vs, y0 = -6 * vs, z0 = -NZ / 2 * vs;
        auto vi = [&](int i, int jj, int k) { return (jj * NZ + k) * NX + i; };
        auto center = [&](int v) {
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            return glm::vec3(x0 + (i + 0.5f) * vs, y0 + (jj + 0.5f) * vs, z0 + (k + 0.5f) * vs);
        };

        // projection of every voxel into every view
        std::vector<int32_t> pix((size_t)N * nv);
        std::vector<float> dep((size_t)N * nv);
        for (int v = 0; v < N; v++) {
            glm::vec3 c = center(v);
            for (int d = 0; d < nv; d++) {
                float sx, sy, z;
                game_project(yaw[d], c.x, c.y, c.z, sx, sy, z);
                int px = (int)std::floor(sx) + Image::OX, py = (int)std::floor(sy) + Image::OY;
                pix[(size_t)v * nv + d] = (px < 0 || py < 0 || px >= Image::W || py >= Image::H)
                                              ? -1 : py * Image::W + px;
                dep[(size_t)v * nv + d] = z;
            }
        }
        auto colour_at = [&](int d, int p) { return p < 0 ? -1 : (int)imgs[d].px[p]; };
        auto inside = [&](int d, int p) {
            if (colour_at(d, p) >= 0)
                return true;
            if (!prm.dilate || p < 0)
                return false;
            int px = p % Image::W, py = p / Image::W;
            return imgs[d].at(px + 1 - Image::OX, py - Image::OY) >= 0 ||
                   imgs[d].at(px - 1 - Image::OX, py - Image::OY) >= 0 ||
                   imgs[d].at(px - Image::OX, py + 1 - Image::OY) >= 0 ||
                   imgs[d].at(px - Image::OX, py - 1 - Image::OY) >= 0;
        };

        // silhouette carving
        std::vector<uint8_t> solid(N, 0);
        int n_solid = 0;
        for (int v = 0; v < N; v++) {
            int misses = 0;
            for (int d = 0; d < nv && misses <= prm.tolerance; d++)
                if (!inside(d, pix[(size_t)v * nv + d]))
                    misses++;
            if (misses <= prm.tolerance) {
                solid[v] = 1;
                n_solid++;
            }
        }
        if (n_solid == 0)
            return;

        static const int nb[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        auto is_solid = [&](int i, int jj, int k) {
            if (i < 0 || jj < 0 || k < 0 || i >= NX || jj >= NY || k >= NZ)
                return false;
            return solid[vi(i, jj, k)] != 0;
        };
        auto is_surface = [&](int v) {
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2]))
                    return true;
            return false;
        };

        // nearest voxel depth per pixel in each view
        std::vector<float> zbuf((size_t)nv * Image::W * Image::H);
        auto build_zbuf = [&]() {
            std::fill(zbuf.begin(), zbuf.end(), 1e9f);
            for (int v = 0; v < N; v++) {
                if (!solid[v])
                    continue;
                for (int d = 0; d < nv; d++) {
                    int p = pix[(size_t)v * nv + d];
                    if (p < 0)
                        continue;
                    float &z = zbuf[(size_t)d * Image::W * Image::H + p];
                    z = std::min(z, dep[(size_t)v * nv + d]);
                }
            }
        };
        auto visible = [&](int v, int d) {
            int p = pix[(size_t)v * nv + d];
            return p >= 0 && dep[(size_t)v * nv + d] <= zbuf[(size_t)d * Image::W * Image::H + p] + 0.5f * vs;
        };

        // photo-consistency carving, keeping every silhouette pixel covered;
        // colours are averaged over 3x3 pixels, as views differ in detail
        if (prm.photo > 0) {
            // how deep each voxel lies inside the silhouette hull, in voxels
            std::vector<uint8_t> depth_in(N, 255);
            {
                std::deque<int> q;
                for (int v = 0; v < N; v++)
                    if (solid[v] && is_surface(v)) {
                        depth_in[v] = 1;
                        q.push_back(v);
                    }
                while (!q.empty()) {
                    int v = q.front();
                    q.pop_front();
                    int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
                    for (auto &o : nb) {
                        int a = i + o[0], b = jj + o[1], c = k + o[2];
                        if (!is_solid(a, b, c))
                            continue;
                        int w = vi(a, b, c);
                        if (depth_in[w] > depth_in[v] + 1) {
                            depth_in[w] = depth_in[v] + 1;
                            q.push_back(w);
                        }
                    }
                }
            }
            // removing v keeps its solid neighbours connected among themselves
            // (26-connectivity within the 3x3x3 block), so carving never
            // splits the figure into floating pieces
            auto removal_keeps_connected = [&](int v) {
                int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
                uint8_t cell[27];
                int first = -1, count = 0;
                for (int n = 0; n < 27; n++) {
                    int dx = n % 3 - 1, dy = n / 9 - 1, dz = (n / 3) % 3 - 1;
                    cell[n] = (n != 13) && is_solid(i + dx, jj + dy, k + dz);
                    if (cell[n]) {
                        count++;
                        if (first < 0)
                            first = n;
                    }
                }
                if (count == 0)
                    return true;
                uint8_t seen[27] = {0};
                int stack[27], sp = 0, reached = 0;
                stack[sp++] = first;
                seen[first] = 1;
                while (sp > 0) {
                    int n = stack[--sp];
                    reached++;
                    int x = n % 3, y = n / 9, z = (n / 3) % 3;
                    for (int m = 0; m < 27; m++) {
                        if (!cell[m] || seen[m])
                            continue;
                        int mx = m % 3, my = m / 9, mz = (m / 3) % 3;
                        if (std::abs(mx - x) <= 1 && std::abs(my - y) <= 1 && std::abs(mz - z) <= 1) {
                            seen[m] = 1;
                            stack[sp++] = m;
                        }
                    }
                }
                return reached == count;
            };
            std::vector<glm::vec3> blur((size_t)nv * Image::W * Image::H, glm::vec3(0));
            for (int d = 0; d < nv; d++)
                for (int py = 1; py < Image::H - 1; py++)
                    for (int px = 1; px < Image::W - 1; px++) {
                        glm::vec3 sum(0);
                        int n = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int col = imgs[d].px[(py + dy) * Image::W + px + dx];
                                if (col < 0)
                                    continue;
                                sum += glm::vec3(sd.rgb[col][0], sd.rgb[col][1], sd.rgb[col][2]);
                                n++;
                            }
                        if (n > 0)
                            blur[(size_t)d * Image::W * Image::H + py * Image::W + px] = sum / (float)n;
                    }
            std::vector<int> cover((size_t)nv * Image::W * Image::H, 0);
            for (int v = 0; v < N; v++)
                if (solid[v])
                    for (int d = 0; d < nv; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0)
                            cover[(size_t)d * Image::W * Image::H + p]++;
                    }
            for (int iter = 0; iter < 24; iter++) {
                build_zbuf();
                std::vector<int> cand;
                for (int v = 0; v < N; v++) {
                    if (!solid[v] || !is_surface(v))
                        continue;
                    float lo[3] = {9, 9, 9}, hi[3] = {-9, -9, -9};
                    int seen = 0;
                    for (int d = 0; d < nv; d++) {
                        if (!visible(v, d))
                            continue;
                        int p = pix[(size_t)v * nv + d];
                        if (colour_at(d, p) < 0)
                            continue;
                        const glm::vec3 &c = blur[(size_t)d * Image::W * Image::H + p];
                        for (int k = 0; k < 3; k++) {
                            lo[k] = std::min(lo[k], c[k]);
                            hi[k] = std::max(hi[k], c[k]);
                        }
                        seen++;
                    }
                    if (seen < 2)
                        continue;
                    float spread = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) +
                                             (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                             (hi[2] - lo[2]) * (hi[2] - lo[2]));
                    if (spread > prm.photo)
                        cand.push_back(v);
                }
                int removed = 0;
                for (int v : cand) {
                    bool keep = depth_in[v] > prm.max_depth || !removal_keeps_connected(v);
                    for (int d = 0; d < nv && !keep; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0 && colour_at(d, p) >= 0 &&
                            cover[(size_t)d * Image::W * Image::H + p] <= 1)
                            keep = true;
                    }
                    if (keep)
                        continue;
                    solid[v] = 0;
                    removed++;
                    for (int d = 0; d < nv; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0)
                            cover[(size_t)d * Image::W * Image::H + p]--;
                    }
                }
                if (removed == 0)
                    break;
            }
            // loose voxels with at most one face neighbour look like noise
            for (int v = 0; v < N; v++) {
                if (!solid[v])
                    continue;
                int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ), nbs = 0;
                for (auto &o : nb)
                    nbs += is_solid(i + o[0], jj + o[1], k + o[2]) ? 1 : 0;
                if (nbs <= 1)
                    solid[v] = 0;
            }
            // whatever still came loose: keep only the largest piece
            std::vector<int> comp(N, -1);
            int best_comp = -1, best_size = 0, n_comp = 0;
            for (int v = 0; v < N; v++) {
                if (!solid[v] || comp[v] >= 0)
                    continue;
                int size = 0;
                std::vector<int> stack{v};
                comp[v] = n_comp;
                while (!stack.empty()) {
                    int w = stack.back();
                    stack.pop_back();
                    size++;
                    int i = w % NX, k = (w / NX) % NZ, jj = w / (NX * NZ);
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dz = -1; dz <= 1; dz++)
                            for (int dx = -1; dx <= 1; dx++) {
                                if (!is_solid(i + dx, jj + dy, k + dz))
                                    continue;
                                int u = vi(i + dx, jj + dy, k + dz);
                                if (comp[u] < 0) {
                                    comp[u] = n_comp;
                                    stack.push_back(u);
                                }
                            }
                }
                if (size > best_size) {
                    best_size = size;
                    best_comp = n_comp;
                }
                n_comp++;
            }
            for (int v = 0; v < N; v++)
                if (solid[v] && comp[v] != best_comp)
                    solid[v] = 0;
        }

        // surface voxels as cubes, faces only towards empty neighbours
        build_zbuf();
        float top = 0;
        for (int v = 0; v < N; v++) {
            if (!solid[v])
                continue;
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            glm::vec3 n(0);
            int open = 0;
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2])) {
                    n += glm::vec3(o[0], o[1], o[2]);
                    open++;
                }
            if (open == 0)
                continue;
            if (glm::length(n) > 0)
                n = glm::normalize(n);
            // colour from the view which sees it most head-on, among those
            // where nothing covers it; any view as a fallback
            int best = -1, fallback = -1;
            float best_score = -2, fb_score = -2;
            for (int d = 0; d < nv; d++) {
                int col = colour_at(d, pix[(size_t)v * nv + d]);
                if (col < 0)
                    continue;
                glm::vec3 to_cam(-kTiltCos * std::sin(yaw[d]), kTiltSin, -kTiltCos * std::cos(yaw[d]));
                float score = glm::dot(n, to_cam);
                if (visible(v, d) && score > best_score) {
                    best_score = score;
                    best = col;
                }
                if (score > fb_score) {
                    fb_score = score;
                    fallback = col;
                }
            }
            int col = best >= 0 ? best : fallback;
            if (col < 0)
                continue;
            glm::vec3 c = center(v);
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2]))
                    add_face(r.verts, c, glm::vec3(o[0], o[1], o[2]), vs, sd.rgb[col]);
            top = std::max(top, c.y + vs * 0.5f);
        }
        r.top = top;
    }

    static void add_face(std::vector<GpuVert> &v, glm::vec3 c, glm::vec3 n, float vs, const float *rgb)
    {
        glm::vec3 u = (std::fabs(n.x) > 0.5f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 w = glm::cross(n, u);
        float h = vs * 0.5f;
        glm::vec3 fc = c + n * h;
        glm::vec3 q[4] = {fc - u * h - w * h, fc + u * h - w * h, fc + u * h + w * h, fc - u * h + w * h};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 0.0f;
            v.push_back(g);
        }
    }

    std::unordered_map<uint64_t, Model> models_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::vector<Done> done_;
    std::shared_ptr<const SpriteData> data_;
    uint32_t generation_ = 0;
    bool quit_ = false;
};

/******************************************************************************/
// Playing from the table: pointers, commands to the game, rings and panels

#include "font_table.inc"

// Sends commands to the game through its command block.
class CommandSender {
public:
    /** Commands go to the game core of this process (swcore_command()). */
    bool in_process = false;

    void send(uint16_t type, int16_t thing = 0, int32_t x = 0, int32_t z = 0, uint32_t arg = 0)
    {
        if (in_process) {
            // the command types of swcore.h are those of vrshare.h
            struct SwcCommand c{};
            c.type = type;
            c.thing = thing;
            c.x = x;
            c.z = z;
            c.arg = arg;
            swcore_command(&c);
            logf("command %u thing %d at %d,%d arg %u", type, thing, x, z, arg);
            return;
        }
        if (!q_ && !open())
            return;
        if (q_->magic != VRS_CMD_MAGIC)
            return;
        uint32_t w = q_->write;
        VrsCmd &c = q_->cmds[w % VRS_CMDS_MAX];
        c.type = type;
        c.thing = thing;
        c.x = x;
        c.z = z;
        c.arg = arg;
        MemoryBarrier();
        q_->write = w + 1;
        logf("command %u thing %d at %d,%d arg %u", type, thing, x, z, arg);
    }

private:
    bool open()
    {
        HANDLE h = OpenFileMappingA(FILE_MAP_WRITE | FILE_MAP_READ, FALSE, VRS_CMD_MAPPING_NAME);
        if (!h)
            return false;
        q_ = static_cast<VrsCmdQueue *>(MapViewOfFile(h, FILE_MAP_WRITE | FILE_MAP_READ, 0, 0,
                                                       sizeof(VrsCmdQueue)));
        return q_ != nullptr;
    }
    VrsCmdQueue *q_ = nullptr;
};

struct Ray {
    glm::vec3 o{0}, d{0, 0, -1};
    bool valid = false;
};

struct Hit {
    enum Kind { NONE, GROUND, PERSON, VEHICLE } kind = NONE;
    uint16_t thing = 0;
    glm::vec3 p{0}; // map units
    float t = 1e30f; // distance along the ray, map units
};

// What can be pointed at on the map.
class Picker {
public:
    // Ground heights at tile corners, from the terrain at the start of the
    // static vertices (6 per tile, corner order 0,1,2,0,2,3).
    void update_ground(const WorldSnapshot &snap)
    {
        const int n = 127;
        if (snap.static_verts.size() < (size_t)n * n * 6)
            return;
        alt_.assign(128 * 128, 0.0f);
        for (int tz = 0; tz < n; tz++)
            for (int tx = 0; tx < n; tx++) {
                const VrsVert *v = &snap.static_verts[(size_t)(tz * n + tx) * 6];
                alt_[tz * 128 + tx] = v[0].y;
                alt_[tz * 128 + tx + 1] = v[1].y;
                alt_[(tz + 1) * 128 + tx + 1] = v[2].y;
                alt_[(tz + 1) * 128 + tx] = v[5].y;
            }
    }

    // Vehicle centres and sizes from their moving geometry.
    void update_vehicles(const WorldSnapshot &snap)
    {
        vehicles_.clear();
        for (const VrsDynGroup &g : snap.dyn_groups) {
            if ((g.id >> 28) != 0 || g.first + g.count > snap.dyn_verts.size() || g.count == 0)
                continue;
            glm::vec3 lo(1e9f), hi(-1e9f);
            for (uint32_t i = 0; i < g.count; i++) {
                const VrsVert &v = snap.dyn_verts[g.first + i];
                lo = glm::min(lo, glm::vec3(v.x, v.y, v.z));
                hi = glm::max(hi, glm::vec3(v.x, v.y, v.z));
            }
            vehicles_.push_back({(uint16_t)g.id, (lo + hi) * 0.5f,
                                 std::max(80.0f, glm::length(hi - lo) * 0.4f)});
        }
    }

    float ground_at(float x, float z) const
    {
        if (alt_.empty())
            return 0;
        float fx = std::clamp(x / 256.0f, 0.0f, 126.99f), fz = std::clamp(z / 256.0f, 0.0f, 126.99f);
        int tx = (int)fx, tz = (int)fz;
        fx -= tx;
        fz -= tz;
        float a = alt_[tz * 128 + tx], b = alt_[tz * 128 + tx + 1];
        float c = alt_[(tz + 1) * 128 + tx], d = alt_[(tz + 1) * 128 + tx + 1];
        return (a * (1 - fx) + b * fx) * (1 - fz) + (c * (1 - fx) + d * fx) * fz;
    }

    struct Person {
        uint16_t thing;
        uint8_t flags;
        glm::vec3 p;
    };

    // Nearest of: people (upright cylinders), vehicles (spheres), ground.
    Hit pick(const Ray &r, const std::vector<Person> &people, glm::vec3 clip_center,
             float clip_half) const
    {
        Hit best;
        if (!r.valid)
            return best;
        for (const Person &pp : people) {
            if (pp.flags & VrsTF_HIDDEN)
                continue;
            // closest approach to the vertical axis through the person
            glm::vec2 o2(r.o.x - pp.p.x, r.o.z - pp.p.z), d2(r.d.x, r.d.z);
            float dd = glm::dot(d2, d2);
            if (dd < 1e-6f)
                continue;
            float t = -glm::dot(o2, d2) / dd;
            if (t < 0)
                continue;
            glm::vec3 q = r.o + r.d * t;
            float dist = glm::length(glm::vec2(q.x - pp.p.x, q.z - pp.p.z));
            float h = q.y - pp.p.y;
            if (dist < 70.0f && h > -20.0f && h < 240.0f && t < best.t) {
                best.kind = Hit::PERSON;
                best.thing = pp.thing;
                best.p = pp.p;
                best.t = t;
            }
        }
        for (const Vehicle &v : vehicles_) {
            glm::vec3 oc = r.o - v.c;
            float b = glm::dot(oc, r.d), c = glm::dot(oc, oc) - v.r * v.r;
            float disc = b * b - c;
            if (disc < 0)
                continue;
            float t = -b - std::sqrt(disc);
            if (t > 0 && t < best.t) {
                best.kind = Hit::VEHICLE;
                best.thing = v.thing;
                best.p = v.c;
                best.t = t;
            }
        }
        // march the ray down to the ground, inside the table
        float step = 24.0f;
        for (float t = 0; t < 80000.0f && t < best.t; t += step) {
            glm::vec3 q = r.o + r.d * t;
            if (std::fabs(q.x - clip_center.x) > clip_half || std::fabs(q.z - clip_center.z) > clip_half) {
                if (q.y < -2000.0f)
                    break;
                continue;
            }
            if (q.y <= ground_at(q.x, q.z)) {
                best.kind = Hit::GROUND;
                best.p = glm::vec3(q.x, ground_at(q.x, q.z), q.z);
                best.t = t;
                break;
            }
        }
        return best;
    }

private:
    struct Vehicle {
        uint16_t thing;
        glm::vec3 c;
        float r;
    };
    std::vector<float> alt_;
    std::vector<Vehicle> vehicles_;
};

// Unlit coloured geometry in world space (meters): text, panels, pointers.
class HudBuilder {
public:
    std::vector<GpuVert> verts;

    void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, const float *rgb)
    {
        glm::vec3 q[4] = {a, b, c, d};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 3.0f;
            verts.push_back(g);
        }
    }

    // Rectangle in the plane of (right, up) from its top left corner.
    void rect(glm::vec3 tl, glm::vec3 right, glm::vec3 up, float w, float h, const float *rgb)
    {
        quad(tl, tl + right * w, tl + right * w - up * h, tl - up * h, rgb);
    }

    // Text from its top left corner; px is the size of a font pixel.
    void text(glm::vec3 tl, glm::vec3 right, glm::vec3 up, float px, const std::string &s,
              const float *rgb, size_t max_chars = 1000)
    {
        for (size_t i = 0; i < s.size() && i < max_chars; i++) {
            int ch = (unsigned char)s[i];
            if (ch < 32 || ch > 126)
                ch = '?';
            const uint8_t *g = kFont6x11[ch - 32];
            for (int y = 0; y < 11; y++) {
                int x = 0;
                while (x < 6) {
                    if (!((g[y] >> x) & 1)) {
                        x++;
                        continue;
                    }
                    int x0 = x;
                    while (x < 6 && ((g[y] >> x) & 1))
                        x++;
                    glm::vec3 p = tl + right * (px * (6 * i + x0)) - up * (px * y);
                    rect(p + up * 0.0f, right, up, px * (x - x0), px, rgb);
                }
            }
        }
    }

    // Thin line facing the eye.
    void line(glm::vec3 a, glm::vec3 b, float width, glm::vec3 eye, const float *rgb)
    {
        glm::vec3 side = glm::cross(b - a, eye - a);
        if (glm::length(side) < 1e-6f)
            return;
        side = glm::normalize(side) * (width * 0.5f);
        quad(a - side, a + side, b + side, b - side, rgb);
    }
};

// A flat board facing the viewer, with rows that can be pointed at.
struct Panel {
    glm::vec3 tl, right, up; // top left corner, unit axes
    float w = 0, h = 0, row_h = 0;
    bool valid = false;

    // Row index under the ray, or -1.
    int row_at(const Ray &r) const
    {
        if (!valid || !r.valid)
            return -1;
        glm::vec3 n = glm::cross(right, up);
        float den = glm::dot(n, r.d);
        if (std::fabs(den) < 1e-6f)
            return -1;
        float t = glm::dot(tl - r.o, n) / den;
        if (t < 0)
            return -1;
        glm::vec3 q = r.o + r.d * t - tl;
        float u = glm::dot(q, right), v = -glm::dot(q, up);
        if (u < 0 || u > w || v < 0 || v > h)
            return -1;
        return (int)(v / row_h);
    }

    float hit_distance(const Ray &r) const
    {
        glm::vec3 n = glm::cross(right, up);
        float den = glm::dot(n, r.d);
        return std::fabs(den) < 1e-6f ? -1.0f : glm::dot(tl - r.o, n) / den;
    }
};

// Faces a panel centred at c towards the eye, turning about the vertical.
static Panel make_panel(glm::vec3 c, glm::vec3 eye, float w, float h, float row_h)
{
    Panel p;
    glm::vec3 to_eye = eye - c;
    to_eye.y = 0;
    if (glm::length(to_eye) < 1e-4f)
        to_eye = glm::vec3(0, 0, 1);
    to_eye = glm::normalize(to_eye);
    p.right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), to_eye));
    p.up = glm::vec3(0, 1, 0);
    p.tl = c - p.right * (w * 0.5f) + p.up * (h * 0.5f);
    p.w = w;
    p.h = h;
    p.row_h = row_h;
    p.valid = true;
    return p;
}

static std::vector<std::string> wrap_text(const std::string &s, size_t width)
{
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        std::string word = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back(cur);
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
        if (j == std::string::npos)
            break;
        i = j + 1;
    }
    if (!cur.empty())
        lines.push_back(cur);
    return lines;
}

// Ring of ribbons on the ground around a point, map units.
static void ground_ring(std::vector<GpuVert> &out, glm::vec3 c, float radius, float width,
                        const float *rgb, glm::vec3 eye)
{
    const int n = 20;
    for (int i = 0; i < n; i++) {
        float a1 = i * 6.2831853f / n, a2 = (i + 1) * 6.2831853f / n;
        glm::vec3 p1 = c + glm::vec3(radius * std::cos(a1), 6, radius * std::sin(a1));
        glm::vec3 p2 = c + glm::vec3(radius * std::cos(a2), 6, radius * std::sin(a2));
        // flat on the ground: the ribbon faces up, not the eye
        glm::vec3 side = glm::normalize(glm::cross(p2 - p1, glm::vec3(0, 1, 0))) * (width * 0.5f);
        glm::vec3 q[4] = {p1 - side, p1 + side, p2 + side, p2 - side};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 3.0f;
            out.push_back(g);
        }
    }
    (void)eye;
}

// Reads the game's panel data out of the snapshot block.
struct HudData {
    VrsHud hud{};
    bool valid = false;
};

/******************************************************************************/
// People, drawn with the game's own sprites on quads facing the viewer

// Size of a sprite pixel in map units. The game draws sprites unscaled at
// overall_scale 256, where one screen pixel spans 8 map units across; its
// camera looks down 30.3 degrees, which shortens heights by cos(30.3).
static constexpr float kSprPixelW = 8.0f;
static constexpr float kSprPixelH = 8.0f / 0.8634f;

class People {
public:
    void update(const WorldSnapshot &snap, double now)
    {
        if (snap.hdr.gameturn != last_turn_) {
            double dt = now - turn_time_;
            if (dt > 0.02 && dt < 0.5)
                turn_dt_ = turn_dt_ * 0.8 + dt * 0.2;
            turn_time_ = now;
            last_turn_ = snap.hdr.gameturn;
            prev_ = cur_;
            cur_.clear();
            for (const auto &t : snap.things)
                cur_[t.index] = t;
            // moving geometry and effects, to glide between turns
            prev_dyn_.swap(cur_dyn_);
            cur_dyn_ = snap.dyn_verts;
            prev_groups_.swap(cur_groups_);
            cur_groups_.clear();
            for (const VrsDynGroup &g : snap.dyn_groups)
                cur_groups_[g.id] = g;
            prev_pos_.swap(cur_pos_);
            cur_pos_.clear();
            for (const VrsEffect &e : snap.effects)
                cur_pos_[e.id] = glm::vec3(e.x, e.y, e.z);
            for (const VrsBar &b : snap.bars)
                cur_pos_[0x80000000u ^ b.id] = glm::vec3(b.x, b.y, b.z);
            for (const VrsGlow &g : snap.glows)
                cur_pos_[0x40000000u ^ g.id] = glm::vec3(g.x, g.y, g.z);
            prev_shadows_.swap(cur_shadows_);
            cur_shadows_.clear();
            for (const VrsShadow &sh : snap.shadows)
                cur_shadows_[sh.id] = sh;
        }
    }

    // Where people are now, for pointing at them.
    void list(double now, std::vector<Picker::Person> &out) const
    {
        out.clear();
        float a = turn_fraction(now);
        for (const auto &kv : cur_)
            out.push_back({kv.first, kv.second.flags, position(kv.first, kv.second, a)});
    }

    // Fraction of the current turn passed, 0..1.
    float turn_fraction(double now) const
    {
        return (float)std::clamp((now - turn_time_) / std::max(turn_dt_, 0.01), 0.0, 1.0);
    }

    // Moving geometry between the last two turns: vertices of a thing
    // present in both, with the same count, glide from old to new.
    void moving_geometry(double now, std::vector<VrsVert> &out) const
    {
        float a = turn_fraction(now);
        out = cur_dyn_;
        for (const auto &kv : cur_groups_) {
            auto it = prev_groups_.find(kv.first);
            if (it == prev_groups_.end() || it->second.count != kv.second.count)
                continue;
            const VrsDynGroup &g = kv.second, &pg = it->second;
            if (g.first + g.count > out.size() || pg.first + pg.count > prev_dyn_.size())
                continue;
            // skip teleports, such as things appearing elsewhere
            const VrsVert &v0 = out[g.first], &p0 = prev_dyn_[pg.first];
            float dx = v0.x - p0.x, dz = v0.z - p0.z;
            if (dx * dx + dz * dz > 2048.0f * 2048.0f)
                continue;
            for (uint32_t i = 0; i < g.count; i++) {
                VrsVert &v = out[g.first + i];
                const VrsVert &pv = prev_dyn_[pg.first + i];
                v.x = pv.x + (v.x - pv.x) * a;
                v.y = pv.y + (v.y - pv.y) * a;
                v.z = pv.z + (v.z - pv.z) * a;
            }
        }
    }

    // eye: viewer position in map units, to face the sprites towards it
    // With vox, people with a voxel figure become instances instead of sprites.
    void build(std::vector<GpuVert> &out, std::vector<GpuVert> &glow_out, double now,
               const WorldSnapshot &snap, glm::vec3 eye, Renderer &rend, VoxelModels *vox,
               glm::vec3 clip_center, float clip_half) const
    {
        out.clear();
        glow_out.clear();
        rend.instances.clear();
        float dt = (float)std::clamp(now - last_build_, 0.0, 0.1);
        last_build_ = now;
        float a = (float)std::clamp((now - turn_time_) / std::max(turn_dt_, 0.01), 0.0, 1.0);
        // shadows under vehicles and people, before anything they could cover
        for (const auto &kv : cur_shadows_) {
            VrsShadow sh = kv.second;
            auto it = prev_shadows_.find(kv.first);
            if (it != prev_shadows_.end())
                for (int k = 0; k < 4; k++) {
                    sh.x[k] = it->second.x[k] + (sh.x[k] - it->second.x[k]) * a;
                    sh.z[k] = it->second.z[k] + (sh.z[k] - it->second.z[k]) * a;
                }
            glm::vec3 c[4];
            for (int k = 0; k < 4; k++)
                c[k] = glm::vec3(sh.x[k], sh.y + 4, sh.z[k]);
            static const int tri[6] = {0, 1, 2, 0, 2, 3};
            for (int k : tri)
                push(out, c[k], nullptr, 4.0f);
        }
        for (const auto &kv : cur_) {
            const VrsThing &t = kv.second;
            if ((t.flags & VrsTF_HIDDEN) || (t.flags & VrsTF_DEAD))
                continue;
            add_shadow(out, position(kv.first, t, a));
        }
        for (const auto &kv : cur_) {
            const VrsThing &t = kv.second;
            if (t.flags & VrsTF_HIDDEN)
                continue;
            glm::vec3 p = position(kv.first, t, a);
            float top = 0;
            const VoxelModels::Model *vm = nullptr;
            int rot_steps = 0, n_views = 8;
            bool on_table = std::fabs(p.x - clip_center.x) < clip_half + 300 &&
                            std::fabs(p.z - clip_center.z) < clip_half + 300;
            if (vox && on_table)
                vm = vox->get(t, rot_steps, n_views);
            if (vm) {
                float phi = facing(kv.first, t, rot_steps * 6.2831853f / n_views, dt);
                glm::mat4 m = glm::rotate(glm::translate(glm::mat4(1.0f), p), phi, glm::vec3(0, 1, 0));
                rend.instances.push_back({vm->vao, vm->count, m,
                                          std::clamp(t.brightness / 32.0f, 0.3f, 1.5f)});
                top = vm->top;
            } else if (!add_sprite(out, snap, t, p, eye, top)) {
                top = add_figure(out, t, p);
            }
            if (t.flags & VrsTF_PLAYER_AGENT) {
                // marker above own agents, visible from afar
                float mk[3] = {0.2f, 1.0f, 1.0f};
                Renderer::add_box(out, p + glm::vec3(-14, top + 40, -14),
                                  p + glm::vec3(14, top + 68, 14), mk);
            }
        }
        for (const VrsEffect &e : snap.effects) {
            glm::vec3 p = glide(e.id, glm::vec3(e.x, e.y, e.z), a);
            glm::vec2 fwd;
            view_angle(eye, p, fwd);
            add_frame(out, snap, e.frame, nullptr, p, fwd, e.scale / 256.0f,
                      std::clamp(e.brightness / 32.0f, 0.3f, 1.5f));
        }
        uint32_t seed = snap.hdr.gameturn * 2654435761u;
        for (const VrsLine &l : snap.lines)
            add_line(out, l, eye, rend, seed);
        // health bars over damaged vehicles, guns and people
        for (const VrsBar &b : snap.bars)
            add_bar(out, glide(0x80000000u ^ b.id, glm::vec3(b.x, b.y, b.z), a), b.frac, 220, eye);
        for (const auto &kv : cur_) {
            const VrsThing &t = kv.second;
            if ((t.flags & (VrsTF_HIDDEN | VrsTF_DEAD)) || t.max_health <= 0 ||
                t.health >= t.max_health || t.health <= 0)
                continue;
            glm::vec3 p = position(kv.first, t, a);
            add_bar(out, p + glm::vec3(0, 250, 0), (float)t.health / t.max_health, 110, eye);
        }
        // glows, drawn by the renderer after everything else
        for (const VrsGlow &g : snap.glows) {
            float rgb[3];
            rend.palette_rgb(rend.palette(), g.colour, rgb);
            add_glow(glow_out, glide(0x40000000u ^ g.id, glm::vec3(g.x, g.y, g.z), a),
                     g.radius * 0.6f, rgb, eye);
        }
    }

private:
    static float wrap_angle(float a)
    {
        while (a > 3.1415927f)
            a -= 6.2831853f;
        while (a < -3.1415927f)
            a += 6.2831853f;
        return a;
    }

    // Smoothly turning facing of a voxel figure, in radians. Eases towards
    // the direction of walking, when close to the game's 8-way facing
    // phi_game, else towards phi_game itself, instead of snapping. Facing
    // A of the game is walking towards atan2(vx, vz) = A * 45 degrees, the
    // same rotation as the figure's.
    float facing(uint16_t index, const VrsThing &t, float phi_game, float dt) const
    {
        float target = phi_game;
        auto it = prev_.find(index);
        if (it != prev_.end()) {
            float vx = t.x - it->second.x, vz = t.z - it->second.z;
            if (vx * vx + vz * vz > 16.0f * 16.0f && vx * vx + vz * vz < 512.0f * 512.0f) {
                float heading = std::atan2(vx, vz);
                if (std::fabs(wrap_angle(heading - phi_game)) < 0.87f)
                    target = heading;
            }
        }
        auto f = facing_.find(index);
        if (f == facing_.end()) {
            facing_[index] = target;
            return target;
        }
        float &phi = f->second;
        phi = wrap_angle(phi + wrap_angle(target - phi) * (1.0f - std::exp(-dt * 10.0f)));
        return phi;
    }

    glm::vec3 glide(uint32_t id, glm::vec3 p, float a) const
    {
        auto it = prev_pos_.find(id);
        if (it != prev_pos_.end() && glm::distance(it->second, p) < 1024.0f)
            return glm::mix(it->second, p, a);
        return p;
    }

    // Horizontal bar facing the viewer: dark back, coloured remaining part.
    static void add_bar(std::vector<GpuVert> &out, glm::vec3 c, float frac, float len, glm::vec3 eye)
    {
        glm::vec3 to_eye = eye - c;
        glm::vec3 right = glm::cross(glm::vec3(0, 1, 0), to_eye);
        if (glm::length(right) < 1e-3f)
            return;
        right = glm::normalize(right) * (len * 0.5f);
        float back[3] = {0.08f, 0.08f, 0.08f};
        add_ribbon(out, c - right, c + right, 22, eye, back);
        frac = std::clamp(frac, 0.0f, 1.0f);
        float rgb[3] = {std::min(1.0f, 2.0f * (1.0f - frac)), std::min(1.0f, 2.0f * frac), 0.1f};
        glm::vec3 nudge = glm::normalize(to_eye) * 2.0f;
        add_ribbon(out, c - right + nudge, c - right + 2.0f * right * frac + nudge, 14, eye, rgb);
    }

    // Round soft glow of a given radius, facing the viewer.
    static void add_glow(std::vector<GpuVert> &out, glm::vec3 c, float radius, const float *rgb,
                         glm::vec3 eye)
    {
        glm::vec3 to_eye = eye - c;
        if (glm::length(to_eye) < 1.0f)
            return;
        to_eye = glm::normalize(to_eye);
        // in front of the source, so nearby surfaces do not cut the disc
        c += to_eye * radius;
        glm::vec3 right = glm::cross(glm::vec3(0, 1, 0), to_eye);
        if (glm::length(right) < 1e-3f)
            right = glm::vec3(1, 0, 0);
        right = glm::normalize(right) * radius;
        glm::vec3 up = glm::normalize(glm::cross(to_eye, right)) * radius;
        glm::vec3 q[4] = {c - right - up, c + right - up, c + right + up, c - right + up};
        glm::vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.uv[0] = uv[k].x; g.uv[1] = uv[k].y;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 5.0f;
            out.push_back(g);
        }
    }

    // Interpolated position between the last two game turns.
    glm::vec3 position(uint16_t index, const VrsThing &t, float a) const
    {
        glm::vec3 p(t.x, t.y, t.z);
        auto it = prev_.find(index);
        if (it != prev_.end()) {
            glm::vec3 q(it->second.x, it->second.y, it->second.z);
            if (glm::distance(p, q) < 512.0f)
                p = glm::mix(q, p, a);
        }
        return p;
    }

    static void push(std::vector<GpuVert> &out, glm::vec3 c, const float *rgb, float mode)
    {
        GpuVert g{};
        g.pos[0] = c.x; g.pos[1] = c.y; g.pos[2] = c.z;
        if (rgb) {
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
        }
        g.textured = mode;
        out.push_back(g);
    }

    // Soft dark disc on the ground under a person.
    static void add_shadow(std::vector<GpuVert> &out, glm::vec3 p)
    {
        const int n = 12;
        const float rx = 48, rz = 40;
        glm::vec3 c = p + glm::vec3(0, 3, 0);
        for (int i = 0; i < n; i++) {
            float a1 = i * 6.2831853f / n, a2 = (i + 1) * 6.2831853f / n;
            push(out, c, nullptr, 4.0f);
            push(out, c + glm::vec3(rx * std::cos(a1), 0, rz * std::sin(a1)), nullptr, 4.0f);
            push(out, c + glm::vec3(rx * std::cos(a2), 0, rz * std::sin(a2)), nullptr, 4.0f);
        }
    }

    // Ribbon of a given width from a to b, turned towards the eye.
    static void add_ribbon(std::vector<GpuVert> &out, glm::vec3 a, glm::vec3 b, float width,
                           glm::vec3 eye, const float *rgb)
    {
        glm::vec3 d = b - a;
        glm::vec3 side = glm::cross(d, eye - a);
        float len = glm::length(side);
        if (len < 1e-3f)
            return;
        side *= 0.5f * width / len;
        glm::vec3 c[4] = {a - side, a + side, b + side, b - side};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri)
            push(out, c[k], rgb, 3.0f);
    }

    static float rand_signed(uint32_t &seed)
    {
        seed = seed * 1664525u + 1013904223u;
        return ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    }

    void add_line(std::vector<GpuVert> &out, const VrsLine &l, glm::vec3 eye,
                  const Renderer &rend, uint32_t &seed) const
    {
        glm::vec3 a(l.x1, l.y1, l.z1), b(l.x2, l.y2, l.z2);
        float len = glm::distance(a, b);
        if (len < 1.0f || len > 40000.0f)
            return;
        float rgb[3];
        rend.palette_rgb(rend.palette(), l.colour, rgb);
        float w = std::max<float>(l.width, 2.0f);
        if (l.flags & VrsLF_JAGGED) {
            // lightning: zigzag through jittered points, like do_beam_electric()
            const int steps = 16;
            glm::vec3 dir = (b - a) / len;
            glm::vec3 n1 = glm::normalize(glm::cross(dir, std::fabs(dir.y) < 0.9f ? glm::vec3(0, 1, 0)
                                                                                    : glm::vec3(1, 0, 0)));
            glm::vec3 n2 = glm::cross(dir, n1);
            float amp = std::min(80.0f, len * 0.06f);
            glm::vec3 prev = a;
            for (int i = 1; i <= steps; i++) {
                float t = (float)i / steps;
                glm::vec3 p = glm::mix(a, b, t);
                if (i < steps) {
                    float env = std::sin(t * 3.1415927f);
                    p += (n1 * rand_signed(seed) + n2 * rand_signed(seed)) * amp * env;
                }
                add_ribbon(out, prev, p, w, eye, rgb);
                prev = p;
            }
            return;
        }
        add_ribbon(out, a, b, w, eye, rgb);
        if (l.flags & VrsLF_GLOW) {
            // whiter core, nudged towards the viewer to stay on top
            float core[3];
            for (int k = 0; k < 3; k++)
                core[k] = 0.55f * rgb[k] + 0.45f;
            glm::vec3 ta = glm::normalize(eye - a) * 2.0f, tb = glm::normalize(eye - b) * 2.0f;
            add_ribbon(out, a + ta, b + tb, w * 0.35f, eye, core);
        }
    }

    // Game camera direction for a view from eye towards p, in the units of
    // the game's camera yaw (2048 per turn): forward is (sin a, cos a).
    static int view_angle(glm::vec3 eye, glm::vec3 p, glm::vec2 &fwd)
    {
        glm::vec2 d(p.x - eye.x, p.z - eye.z);
        if (glm::length(d) < 1.0f)
            d = glm::vec2(0, 1);
        fwd = glm::normalize(d);
        float ang = std::atan2(fwd.x, fwd.y) * (2048.0f / 6.2831853f);
        return ((int)std::lround(ang) + 2048) & 2047;
    }

    static int frame_for(const WorldSnapshot &snap, const VrsThing &t, int cam_angle)
    {
        int idx;
        switch (t.anim_mode) {
        case VrsAM_DIR8: {
            int cam8 = ((cam_angle + 128) >> 8) & 7;
            idx = t.anim_start + 1 + ((t.angle + 8 - cam8) & 7);
            break;
        }
        case VrsAM_DIR12: {
            int cam12 = ((cam_angle + 85) / 170) % 12;
            idx = t.anim_start + 1 + (((3 * t.angle >> 1) + 12 - cam12) % 12);
            break;
        }
        case VrsAM_FIXED:
            return t.anim_start;
        default:
            return -1;
        }
        if (idx < 0 || idx >= (int)snap.nstart.size())
            return -1;
        return snap.nstart[idx] + t.subframe;
    }

    // Adds the frame elements as quads; returns false if there is no frame.
    static bool add_sprite(std::vector<GpuVert> &out, const WorldSnapshot &snap,
                           const VrsThing &t, glm::vec3 p, glm::vec3 eye, float &top)
    {
        glm::vec2 fwd;
        int frm = frame_for(snap, t, view_angle(eye, p, fwd));
        // the game's fade level, 32 shows the sprite unchanged
        float b = std::clamp(t.brightness / 32.0f, 0.3f, 1.5f);
        return add_frame(out, snap, frm, t.frv, p, fwd, 1.0f, b, &top);
    }

    // Adds the elements of a frame as quads facing along fwd. With frv, the
    // elements are selected by frame versions as for people; without, as for
    // plain frames. Returns false if nothing was drawn.
    static bool add_frame(std::vector<GpuVert> &out, const WorldSnapshot &snap, int frm,
                          const uint8_t *frv, glm::vec3 p, glm::vec2 fwd, float scale, float b,
                          float *p_top = nullptr)
    {
        if (frm <= 0 || frm >= (int)snap.frames.size())
            return false;
        // screen right of the game camera, and a step towards the viewer
        glm::vec3 right(fwd.y, 0, -fwd.x);
        glm::vec3 toward(-fwd.x, 0, -fwd.y);
        float pw = kSprPixelW * scale, ph = kSprPixelH * scale;
        float top = 0;
        bool any = false;
        int layer_no = 0;
        int guard = 0;
        for (int el = snap.frames[frm].first_element;
             el > 0 && el < (int)snap.elements.size() && guard < 64;
             el = snap.elements[el].next, guard++) {
            const VrsElement &e = snap.elements[el];
            if (e.to_sprite == 0 || e.to_sprite % snap.sprite_stride != 0)
                continue;
            uint32_t si = e.to_sprite / snap.sprite_stride;
            if (si >= snap.sprite_rects.size())
                continue;
            if (frv) {
                int slot = (e.flags >> 4) & 0x1F;
                int ver = slot < 5 ? frv[slot] : 0;
                if (ver != ((e.flags >> 9) & 7))
                    continue;
            } else if (e.flags & 0xFE00) {
                continue;
            }
            const VrsSpriteRect &r = snap.sprite_rects[si];
            if (r.w == 0 || r.h == 0)
                continue;
            float x0 = (e.x >> 1) * pw, x1 = x0 + r.w * pw;
            float y0 = -(e.y >> 1) * ph, y1 = y0 - r.h * ph;
            float u0 = r.x / 256.0f, u1 = (r.x + r.w) / 256.0f;
            float v0 = r.y / 256.0f, v1 = (r.y + r.h) / 256.0f;
            if (e.flags & 0x01)
                std::swap(u0, u1);
            if (e.flags & 0x02)
                std::swap(v0, v1);
            // later elements are painted over earlier ones in the game
            glm::vec3 o = p + toward * (1.5f * layer_no++);
            glm::vec3 c[4] = {o + right * x0 + glm::vec3(0, y0, 0), o + right * x1 + glm::vec3(0, y0, 0),
                              o + right * x1 + glm::vec3(0, y1, 0), o + right * x0 + glm::vec3(0, y1, 0)};
            glm::vec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
            static const int tri[6] = {0, 1, 2, 0, 2, 3};
            for (int k : tri) {
                GpuVert g{};
                g.pos[0] = c[k].x; g.pos[1] = c[k].y; g.pos[2] = c[k].z;
                g.uv[0] = uv[k].x; g.uv[1] = uv[k].y;
                g.layer = r.page;
                g.rgb[0] = b; g.rgb[1] = b; g.rgb[2] = b;
                g.textured = 2.0f;
                out.push_back(g);
            }
            top = std::max(top, y0);
            any = true;
        }
        if (p_top)
            *p_top = top;
        return any;
    }

    // Box figure, for when sprites are not available; returns its height.
    static float add_figure(std::vector<GpuVert> &out, const VrsThing &t, glm::vec3 p)
    {
        float rgb[3];
        colour_for(t, rgb);
        if (t.flags & VrsTF_DEAD) {
            Renderer::add_box(out, p + glm::vec3(-70, 0, -24), p + glm::vec3(70, 14, 24), rgb);
            return 14;
        }
        float w = 26, h = 160, hw = 20;
        Renderer::add_box(out, p + glm::vec3(-w, 0, -w), p + glm::vec3(w, h, w), rgb);
        float skin[3] = {0.85f, 0.7f, 0.55f};
        Renderer::add_box(out, p + glm::vec3(-hw, h, -hw), p + glm::vec3(hw, h + 2 * hw, hw), skin);
        return h + 2 * hw;
    }

    static void colour_for(const VrsThing &t, float *rgb)
    {
        float c[3] = {0.75f, 0.75f, 0.7f};
        if (t.flags & VrsTF_PLAYER_AGENT) {
            c[0] = 0.1f; c[1] = 0.9f; c[2] = 1.0f;
        } else {
            // vary by subtype so different kinds of people are distinguishable
            static const float pal[8][3] = {
                {0.8f, 0.8f, 0.75f}, {0.95f, 0.25f, 0.2f}, {0.25f, 0.4f, 0.95f}, {0.95f, 0.8f, 0.2f},
                {0.6f, 0.35f, 0.8f}, {0.3f, 0.75f, 0.35f}, {0.9f, 0.55f, 0.25f}, {0.5f, 0.5f, 0.5f}};
            const float *s = pal[t.subtype & 7];
            c[0] = s[0]; c[1] = s[1]; c[2] = s[2];
        }
        if (t.flags & VrsTF_DEAD) {
            c[0] *= 0.4f; c[1] *= 0.2f; c[2] *= 0.2f;
        }
        memcpy(rgb, c, sizeof(c));
    }

    std::unordered_map<uint16_t, VrsThing> cur_, prev_;
    mutable std::unordered_map<uint16_t, float> facing_;
    std::vector<VrsVert> cur_dyn_, prev_dyn_;
    std::unordered_map<uint32_t, VrsDynGroup> cur_groups_, prev_groups_;
    std::unordered_map<uint32_t, glm::vec3> cur_pos_, prev_pos_;
    std::unordered_map<uint32_t, VrsShadow> cur_shadows_, prev_shadows_;
    mutable double last_build_ = 0;
    uint32_t last_turn_ = 0;
    double turn_time_ = 0, turn_dt_ = 1.0 / 16;
};

/******************************************************************************/
// Table view state: which part of the map is shown, and how

struct TableView {
    glm::vec3 center{16384, 0, 16384}; // map units, the point at table centre
    float half_map = 5120;              // map units from centre to table edge
    float yaw = 0;                      // table rotation, radians
    bool follow = true;
    float table_half_m = 0.7f;          // physical half size in meters
    glm::mat4 table_pose{1.0f};         // table top centre in world space

    float scale() const { return table_half_m / half_map; }

    // map units -> table-local meters -> world; the game's axes are
    // left-handed, so Z is flipped to show the map the right way round
    glm::mat4 map_model() const
    {
        glm::mat4 m = table_pose;
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(scale(), scale(), -scale()));
        m = glm::translate(m, glm::vec3(-center.x, -ground_offset, -center.z));
        return m;
    }

    // table mesh is built in map units around origin
    glm::mat4 table_model() const
    {
        glm::mat4 m = table_pose;
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(scale()));
        return m;
    }

    void follow_camera(const VrsHeader &h, float dt)
    {
        if (!follow || !h.in_mission)
            return;
        glm::vec3 target(h.cam_x, 0, h.cam_z);
        float k = 1.0f - std::exp(-dt * 4.0f);
        center = glm::mix(center, target, k);
    }

    void pan(float dx, float dz)
    {
        // input in table space; rotate into map space
        float c = std::cos(-yaw), s = std::sin(-yaw);
        center.x += (c * dx - s * dz) * half_map;
        center.z -= (s * dx + c * dz) * half_map; // map Z is flipped
        center.x = std::clamp(center.x, 0.0f, 32768.0f);
        center.z = std::clamp(center.z, 0.0f, 32768.0f);
    }

    void zoom(float f) { half_map = std::clamp(half_map * f, 1024.0f, 17000.0f); }

    // map height at the table top: just under the lowest ground, as roads
    // and water lie below the level of the streets
    float ground_offset = -64.0f;
};

/******************************************************************************/
// OpenXR

#define XR_CHECK(x)                                                    \
    do {                                                               \
        XrResult r_ = (x);                                             \
        if (XR_FAILED(r_)) {                                           \
            logf("%s failed: %d (line %d)", #x, (int)r_, __LINE__);   \
            return false;                                              \
        }                                                              \
    } while (0)

class XrApp {
public:
    bool init()
    {
        std::vector<const char *> exts = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
        XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy(ci.applicationInfo.applicationName, "SyndWarsVR Spectator");
        ci.applicationInfo.applicationVersion = 1;
        strcpy(ci.applicationInfo.engineName, "syndwarsfx");
        ci.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        ci.enabledExtensionCount = (uint32_t)exts.size();
        ci.enabledExtensionNames = exts.data();
        XR_CHECK(xrCreateInstance(&ci, &instance_));

        XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
        xrGetInstanceProperties(instance_, &ip);
        logf("OpenXR runtime: %s", ip.runtimeName);

        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
        si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XR_CHECK(xrGetSystem(instance_, &si, &system_));

        PFN_xrGetOpenGLGraphicsRequirementsKHR getreq = nullptr;
        XR_CHECK(xrGetInstanceProcAddr(instance_, "xrGetOpenGLGraphicsRequirementsKHR",
                                       (PFN_xrVoidFunction *)&getreq));
        XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        XR_CHECK(getreq(instance_, system_, &req));

        uint32_t n = 0;
        XR_CHECK(xrEnumerateViewConfigurationViews(instance_, system_,
                 XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr));
        view_cfg_.resize(n, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        XR_CHECK(xrEnumerateViewConfigurationViews(instance_, system_,
                 XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, view_cfg_.data()));
        return true;
    }

    bool create_session()
    {
        XrGraphicsBindingOpenGLWin32KHR gb{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        gb.hDC = wglGetCurrentDC();
        gb.hGLRC = wglGetCurrentContext();
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
        sci.next = &gb;
        sci.systemId = system_;
        XR_CHECK(xrCreateSession(instance_, &sci, &session_));

        XrReferenceSpaceCreateInfo rs{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        rs.poseInReferenceSpace.orientation.w = 1;
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        if (XR_FAILED(xrCreateReferenceSpace(session_, &rs, &space_))) {
            rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            XR_CHECK(xrCreateReferenceSpace(session_, &rs, &space_));
        }
        rs.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        XR_CHECK(xrCreateReferenceSpace(session_, &rs, &view_space_));

        uint32_t nf = 0;
        XR_CHECK(xrEnumerateSwapchainFormats(session_, 0, &nf, nullptr));
        std::vector<int64_t> formats(nf);
        XR_CHECK(xrEnumerateSwapchainFormats(session_, nf, &nf, formats.data()));
        int64_t fmt = formats[0];
        for (int64_t f : formats)
            if (f == GL_SRGB8_ALPHA8) { fmt = f; break; }
        srgb_ = (fmt == GL_SRGB8_ALPHA8);

        for (auto &vc : view_cfg_) {
            Eye e;
            e.w = vc.recommendedImageRectWidth;
            e.h = vc.recommendedImageRectHeight;
            XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
            sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
            sc.format = fmt;
            sc.sampleCount = 1;
            sc.width = e.w;
            sc.height = e.h;
            sc.faceCount = 1;
            sc.arraySize = 1;
            sc.mipCount = 1;
            XR_CHECK(xrCreateSwapchain(session_, &sc, &e.swapchain));
            uint32_t ni = 0;
            XR_CHECK(xrEnumerateSwapchainImages(e.swapchain, 0, &ni, nullptr));
            e.images.resize(ni, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
            XR_CHECK(xrEnumerateSwapchainImages(e.swapchain, ni, &ni,
                     (XrSwapchainImageBaseHeader *)e.images.data()));
            glGenFramebuffers(1, &e.fbo);
            glGenRenderbuffers(1, &e.depth);
            glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.w, e.h);
            eyes_.push_back(e);
        }
        logf("XR session created, %zu views %dx%d, %s swapchain", eyes_.size(),
             eyes_.empty() ? 0 : eyes_[0].w, eyes_.empty() ? 0 : eyes_[0].h,
             srgb_ ? "sRGB" : "linear");
        return create_actions();
    }

    bool create_actions()
    {
        XrActionSetCreateInfo asi{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy(asi.actionSetName, "spectator");
        strcpy(asi.localizedActionSetName, "Spectator");
        XR_CHECK(xrCreateActionSet(instance_, &asi, &action_set_));

        auto make = [&](XrAction &a, const char *name, const char *loc, XrActionType t) {
            XrActionCreateInfo ai{XR_TYPE_ACTION_CREATE_INFO};
            ai.actionType = t;
            strcpy(ai.actionName, name);
            strcpy(ai.localizedActionName, loc);
            return xrCreateAction(action_set_, &ai, &a);
        };
        XR_CHECK(make(act_pan_, "pan", "Pan map", XR_ACTION_TYPE_VECTOR2F_INPUT));
        XR_CHECK(make(act_zoom_, "zoom_rotate", "Zoom and rotate", XR_ACTION_TYPE_VECTOR2F_INPUT));
        XR_CHECK(make(act_follow_, "toggle_follow", "Toggle follow camera", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_recenter_, "recenter", "Recenter table", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_voxel_, "toggle_voxel", "Toggle voxel figures", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_aim_l_, "aim_left", "Left pointer", XR_ACTION_TYPE_POSE_INPUT));
        XR_CHECK(make(act_aim_r_, "aim_right", "Right pointer", XR_ACTION_TYPE_POSE_INPUT));
        XR_CHECK(make(act_trig_l_, "command_left", "Shoot", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_trig_r_, "command_right", "Select or move", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_wep_next_, "weapon_next", "Next weapon", XR_ACTION_TYPE_BOOLEAN_INPUT));
        XR_CHECK(make(act_wep_prev_, "weapon_prev", "Previous weapon", XR_ACTION_TYPE_BOOLEAN_INPUT));

        auto path = [&](const char *s) {
            XrPath p;
            xrStringToPath(instance_, s, &p);
            return p;
        };
        struct Profile {
            const char *name;
            const char *follow;
            const char *trigger; // per hand, after /user/hand/<side>
            const char *wep_prev, *wep_next;
        };
        const Profile profiles[] = {
            {"/interaction_profiles/valve/index_controller", "/user/hand/right/input/a/click",
             "/input/trigger/click", "/user/hand/left/input/a/click", "/user/hand/left/input/b/click"},
            {"/interaction_profiles/oculus/touch_controller", "/user/hand/right/input/a/click",
             "/input/trigger/value", "/user/hand/left/input/x/click", "/user/hand/left/input/y/click"},
        };
        for (const auto &pr : profiles) {
            std::vector<XrActionSuggestedBinding> b = {
                {act_pan_, path("/user/hand/left/input/thumbstick")},
                {act_zoom_, path("/user/hand/right/input/thumbstick")},
                {act_follow_, path(pr.follow)},
                {act_recenter_, path("/user/hand/left/input/thumbstick/click")},
                {act_voxel_, path("/user/hand/right/input/b/click")},
                {act_aim_l_, path("/user/hand/left/input/aim/pose")},
                {act_aim_r_, path("/user/hand/right/input/aim/pose")},
                {act_trig_l_, path((std::string("/user/hand/left") + pr.trigger).c_str())},
                {act_trig_r_, path((std::string("/user/hand/right") + pr.trigger).c_str())},
                {act_wep_prev_, path(pr.wep_prev)},
                {act_wep_next_, path(pr.wep_next)},
            };
            XrInteractionProfileSuggestedBinding sb{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
            sb.interactionProfile = path(pr.name);
            sb.suggestedBindings = b.data();
            sb.countSuggestedBindings = (uint32_t)b.size();
            XrResult r = xrSuggestInteractionProfileBindings(instance_, &sb);
            if (XR_FAILED(r))
                logf("bindings for %s rejected: %d", pr.name, (int)r);
        }
        XrSessionActionSetsAttachInfo at{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        at.countActionSets = 1;
        at.actionSets = &action_set_;
        XR_CHECK(xrAttachSessionActionSets(session_, &at));
        XrActionSpaceCreateInfo asci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        asci.poseInActionSpace.orientation.w = 1;
        asci.action = act_aim_l_;
        XR_CHECK(xrCreateActionSpace(session_, &asci, &aim_space_[0]));
        asci.action = act_aim_r_;
        XR_CHECK(xrCreateActionSpace(session_, &asci, &aim_space_[1]));
        return true;
    }

    // Returns false when the app should quit.
    bool poll_events()
    {
        XrEventDataBuffer ev{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(instance_, &ev) == XR_SUCCESS) {
            if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto *sc = (XrEventDataSessionStateChanged *)&ev;
                state_ = sc->state;
                logf("XR session state %d", (int)state_);
                if (state_ == XR_SESSION_STATE_FOCUSED && !was_focused_) {
                    was_focused_ = true;
                    just_focused_ = true;
                }
                if (state_ == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(session_, &bi);
                    running_ = true;
                } else if (state_ == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(session_);
                    running_ = false;
                } else if (state_ == XR_SESSION_STATE_EXITING ||
                           state_ == XR_SESSION_STATE_LOSS_PENDING) {
                    return false;
                }
            } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                return false;
            }
            ev = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        return true;
    }

    struct Input {
        glm::vec2 pan{0}, zoom{0};
        bool follow_pressed = false, recenter_pressed = false, voxel_pressed = false;
        bool trig_pressed[2] = {false, false}; // left, right
        bool wep_next = false, wep_prev = false;
        bool aim_valid[2] = {false, false};
        glm::vec3 aim_pos[2], aim_dir[2];
    };

    Input read_input()
    {
        Input in;
        if (!running_)
            return in;
        XrActiveActionSet as{action_set_, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
        sync.countActiveActionSets = 1;
        sync.activeActionSets = &as;
        if (XR_FAILED(xrSyncActions(session_, &sync)))
            return in;
        XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
        XrActionStateVector2f v{XR_TYPE_ACTION_STATE_VECTOR2F};
        gi.action = act_pan_;
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &gi, &v)) && v.isActive)
            in.pan = {v.currentState.x, v.currentState.y};
        gi.action = act_zoom_;
        v = {XR_TYPE_ACTION_STATE_VECTOR2F};
        if (XR_SUCCEEDED(xrGetActionStateVector2f(session_, &gi, &v)) && v.isActive)
            in.zoom = {v.currentState.x, v.currentState.y};
        XrActionStateBoolean b{XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_follow_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.follow_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_recenter_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.recenter_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        b = {XR_TYPE_ACTION_STATE_BOOLEAN};
        gi.action = act_voxel_;
        if (XR_SUCCEEDED(xrGetActionStateBoolean(session_, &gi, &b)))
            in.voxel_pressed = b.isActive && b.changedSinceLastSync && b.currentState;
        auto pressed = [&](XrAction a) {
            XrActionStateGetInfo g{XR_TYPE_ACTION_STATE_GET_INFO};
            g.action = a;
            XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
            return XR_SUCCEEDED(xrGetActionStateBoolean(session_, &g, &st)) && st.isActive &&
                   st.changedSinceLastSync && st.currentState;
        };
        in.trig_pressed[0] = pressed(act_trig_l_);
        in.trig_pressed[1] = pressed(act_trig_r_);
        in.wep_next = pressed(act_wep_next_);
        in.wep_prev = pressed(act_wep_prev_);
        for (int h = 0; h < 2 && last_time_ != 0; h++) {
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
            if (XR_FAILED(xrLocateSpace(aim_space_[h], space_, last_time_, &loc)))
                continue;
            if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
                !(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
                continue;
            glm::quat q(loc.pose.orientation.w, loc.pose.orientation.x, loc.pose.orientation.y,
                        loc.pose.orientation.z);
            in.aim_pos[h] = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
            in.aim_dir[h] = q * glm::vec3(0, 0, -1);
            in.aim_valid[h] = true;
        }
        return in;
    }

    // Pose of the head in the stage space, if tracked.
    bool head_pose(XrTime t, glm::vec3 &pos, glm::quat &ori)
    {
        XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
        if (XR_FAILED(xrLocateSpace(view_space_, space_, t, &loc)))
            return false;
        if (!(loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
            !(loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT))
            return false;
        pos = {loc.pose.position.x, loc.pose.position.y, loc.pose.position.z};
        ori = glm::quat(loc.pose.orientation.w, loc.pose.orientation.x, loc.pose.orientation.y,
                        loc.pose.orientation.z);
        return true;
    }

    template <typename DrawFn>
    void frame(DrawFn draw, XrTime &out_time)
    {
        out_time = 0;
        if (!running_) {
            Sleep(10);
            return;
        }
        XrFrameWaitInfo wi{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState fs{XR_TYPE_FRAME_STATE};
        if (XR_FAILED(xrWaitFrame(session_, &wi, &fs)))
            return;
        XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
        if (XR_FAILED(xrBeginFrame(session_, &bi)))
            return;
        out_time = fs.predictedDisplayTime;
        last_time_ = fs.predictedDisplayTime;

        std::vector<XrCompositionLayerProjectionView> pviews;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerBaseHeader *> layers;
        if (fs.shouldRender) {
            uint32_t nv = (uint32_t)eyes_.size();
            std::vector<XrView> views(nv, {XR_TYPE_VIEW});
            XrViewLocateInfo li{XR_TYPE_VIEW_LOCATE_INFO};
            li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            li.displayTime = fs.predictedDisplayTime;
            li.space = space_;
            XrViewState vs{XR_TYPE_VIEW_STATE};
            if (XR_SUCCEEDED(xrLocateViews(session_, &li, &vs, nv, &nv, views.data())) &&
                (vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT)) {
                pviews.resize(nv, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
                for (uint32_t i = 0; i < nv; i++) {
                    Eye &e = eyes_[i];
                    uint32_t idx = 0;
                    XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
                    xrAcquireSwapchainImage(e.swapchain, &ai, &idx);
                    XrSwapchainImageWaitInfo wsi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
                    wsi.timeout = XR_INFINITE_DURATION;
                    xrWaitSwapchainImage(e.swapchain, &wsi);

                    glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                           e.images[idx].image, 0);
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                              GL_RENDERBUFFER, e.depth);
                    glViewport(0, 0, e.w, e.h);
                    if (srgb_)
                        glEnable(GL_FRAMEBUFFER_SRGB);
                    glm::mat4 proj = projection(views[i].fov, 0.02f, 100.0f);
                    glm::mat4 view = glm::inverse(pose_matrix(views[i].pose));
                    draw(proj * view, srgb_);
                    glDisable(GL_FRAMEBUFFER_SRGB);
                    if (i == 0)
                        last_left_ = {e.fbo, e.w, e.h};
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);

                    XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
                    xrReleaseSwapchainImage(e.swapchain, &ri);

                    pviews[i].pose = views[i].pose;
                    pviews[i].fov = views[i].fov;
                    pviews[i].subImage.swapchain = e.swapchain;
                    pviews[i].subImage.imageRect.offset = {0, 0};
                    pviews[i].subImage.imageRect.extent = {e.w, e.h};
                }
                layer.space = space_;
                layer.viewCount = nv;
                layer.views = pviews.data();
                layers.push_back((XrCompositionLayerBaseHeader *)&layer);
            }
        }
        XrFrameEndInfo ei{XR_TYPE_FRAME_END_INFO};
        ei.displayTime = fs.predictedDisplayTime;
        ei.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        ei.layerCount = (uint32_t)layers.size();
        ei.layers = layers.data();
        xrEndFrame(session_, &ei);
    }

    struct Mirror {
        GLuint fbo = 0;
        int w = 0, h = 0;
    };
    Mirror last_left() const { return last_left_; }
    bool running() const { return running_; }
    // True once, when the headset is first put on and the app gets focus.
    bool take_just_focused()
    {
        bool r = just_focused_;
        just_focused_ = false;
        return r;
    }

private:
    static glm::mat4 pose_matrix(const XrPosef &p)
    {
        glm::quat q(p.orientation.w, p.orientation.x, p.orientation.y, p.orientation.z);
        glm::mat4 m = glm::mat4_cast(q);
        m[3] = glm::vec4(p.position.x, p.position.y, p.position.z, 1.0f);
        return m;
    }

    static glm::mat4 projection(const XrFovf &fov, float n, float f)
    {
        float l = std::tan(fov.angleLeft), r = std::tan(fov.angleRight);
        float d = std::tan(fov.angleDown), u = std::tan(fov.angleUp);
        glm::mat4 m(0.0f);
        m[0][0] = 2.0f / (r - l);
        m[1][1] = 2.0f / (u - d);
        m[2][0] = (r + l) / (r - l);
        m[2][1] = (u + d) / (u - d);
        m[2][2] = -(f + n) / (f - n);
        m[2][3] = -1.0f;
        m[3][2] = -2.0f * f * n / (f - n);
        return m;
    }

    struct Eye {
        XrSwapchain swapchain = XR_NULL_HANDLE;
        int32_t w = 0, h = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        GLuint fbo = 0, depth = 0;
    };

    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId system_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace space_ = XR_NULL_HANDLE, view_space_ = XR_NULL_HANDLE;
    std::vector<XrViewConfigurationView> view_cfg_;
    std::vector<Eye> eyes_;
    XrSessionState state_ = XR_SESSION_STATE_UNKNOWN;
    bool running_ = false, srgb_ = false;
    bool was_focused_ = false, just_focused_ = false;
    XrActionSet action_set_ = XR_NULL_HANDLE;
    XrAction act_pan_ = XR_NULL_HANDLE, act_zoom_ = XR_NULL_HANDLE, act_follow_ = XR_NULL_HANDLE,
             act_recenter_ = XR_NULL_HANDLE, act_voxel_ = XR_NULL_HANDLE, act_aim_l_ = XR_NULL_HANDLE,
             act_aim_r_ = XR_NULL_HANDLE, act_trig_l_ = XR_NULL_HANDLE, act_trig_r_ = XR_NULL_HANDLE,
             act_wep_next_ = XR_NULL_HANDLE, act_wep_prev_ = XR_NULL_HANDLE;
    XrSpace aim_space_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrTime last_time_ = 0;
    Mirror last_left_;
};

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
    bool use_voxels = false, voxel_dilate = false;
    int voxel_tol = 0;
    float voxel_photo = -1;
    // the game in this process: --mission C,M (else the game's shared memory)
    bool in_process = false;
    unsigned campaign = 0, mission = 0;
    const char *data_dir = getenv("SWARS_GAMEDATA");
    double turn_ms = 50.0;
    uint32_t dump_turn = 0;
    const char *dump_file = nullptr;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--mission") && i + 1 < argc) {
            in_process = (sscanf(argv[++i], "%u,%u", &campaign, &mission) == 2);
        } else if (!strcmp(argv[i], "--data") && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (!strcmp(argv[i], "--dump-block") && i + 2 < argc) {
            dump_turn = (uint32_t)strtoul(argv[++i], nullptr, 0);
            dump_file = argv[++i];
        } else if (!strcmp(argv[i], "--turn-ms") && i + 1 < argc) {
            turn_ms = std::max(1.0, atof(argv[++i]));
        } else if (!strcmp(argv[i], "--desktop"))
            desktop = true;
        else if (!strcmp(argv[i], "--voxel"))
            use_voxels = true;
        else if (!strcmp(argv[i], "--click") && i + 4 < argc) {
            SimClick c;
            c.x = atoi(argv[++i]);
            c.y = atoi(argv[++i]);
            c.at = atof(argv[++i]);
            c.button = atoi(argv[++i]);
            c.done = false;
            sim_clicks.push_back(c);
        }
        else if (!strcmp(argv[i], "--voxel-tol") && i + 1 < argc)
            voxel_tol = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--voxel-dilate"))
            voxel_dilate = true;
        else if (!strcmp(argv[i], "--voxel-photo") && i + 1 < argc)
            voxel_photo = (float)atof(argv[++i]);
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

    SharedWorld shared;
    WorldSnapshot snap;
    People people;
    VoxelModels voxels;
    voxels.params.tolerance = voxel_tol;
    voxels.params.dilate = voxel_dilate;
    if (voxel_photo >= 0)
        voxels.params.photo = voxel_photo;
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
    double over_at = -1;
    if (in_process) {
        if (!core.start(data_dir, campaign, mission, turn_ms / 1000.0, t_prev))
            return 1;
        cmds.in_process = true;
        if (dump_file != nullptr)
            core.dump_at(dump_turn, dump_file);
    }
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
                case SDLK_RETURN: if (in_process) core.conclude(); break;
                case SDLK_f: tv.follow = !tv.follow; break;
                case SDLK_LEFTBRACKET: key_wep_prev = true; break;
                case SDLK_RIGHTBRACKET: key_wep_next = true; break;
                case SDLK_v: use_voxels = !use_voxels; break;
                case SDLK_t: {
                    // cycle the photo-consistency threshold, 0 is off
                    static const float steps[] = {0.0f, 0.35f, 0.25f, 0.50f};
                    static int si = 0;
                    si = (si + 1) % 4;
                    voxels.params.photo = steps[si];
                    voxels.rebuild();
                    logf("voxel photo-consistency threshold %.2f", voxels.params.photo);
                    break;
                }
                case SDLK_d:
                    voxels.params.dilate = !voxels.params.dilate;
                    voxels.rebuild();
                    logf("voxel dilation %s", voxels.params.dilate ? "on" : "off");
                    break;
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
        if (in_process && !core.running()) {
            // the mission is over: show its end for a moment, then leave
            if (over_at < 0)
                over_at = t;
            else if (t - over_at > 3.0)
                quit = true;
        }
        if (in_process ? core.poll(snap, t) : shared.poll(snap)) {
            rend.upload_world(snap);
            // figures are built from the sprites and coloured by the palette
            if (snap.sprite_changed || snap.tmap_changed)
                voxels.reset(snap, rend);
            people.update(snap, t);
            if (snap.static_changed)
                picker.update_ground(snap);
            picker.update_vehicles(snap);
        }
        tv.follow_camera(snap.hdr, dt);
        voxels.collect();
        {
            static std::vector<VrsVert> moving;
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
        people.build(people_verts, glow_verts, t, snap, eye_map, rend, use_voxels ? &voxels : nullptr,
                     glm::vec3(tv.center.x, 0, tv.center.z), tv.half_map);

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
        // table on the desktop), the objective on a board behind the table
        const VrsHud &hd = snap.hud;
        auto weapon_name = [&](int w) {
            return std::string(hd.weapon_names[w], strnlen(hd.weapon_names[w], sizeof(hd.weapon_names[w])));
        };
        int selected_agent = -1;
        std::vector<int> weapon_rows;
        for (int i = 0; i < VRS_AGENTS_MAX; i++)
            if (hd.agents[i].thing != 0 && hd.agents[i].thing == snap.hdr.selected)
                selected_agent = i;
        if (selected_agent >= 0)
            for (int w = 1; w < VRS_WEAPON_TYPES; w++)
                if (hd.agents[selected_agent].weapons & (1u << (w - 1)))
                    weapon_rows.push_back(w);
        const float row_h = 0.024f, px = 0.0016f;
        int n_rows = 6 + (int)weapon_rows.size();
        glm::vec3 panel_c;
        if (use_xr && world_ray[0].valid)
            panel_c = world_ray[0].o + glm::vec3(0, 0.07f + n_rows * row_h * 0.5f, 0);
        else
            panel_c = glm::vec3(tv.table_pose * glm::vec4(-0.95f, 0.25f, 0.1f, 1.0f));
        Panel panel = make_panel(panel_c, eye_world, 0.27f, n_rows * row_h, row_h);
        int hover_row = panel.row_at(world_ray[1]);
        bool on_panel = hover_row >= 0 && snap.valid && snap.hdr.in_mission;
        if (snap.valid && snap.hdr.in_mission) {
            static const float bg[3] = {0.04f, 0.06f, 0.09f}, sel[3] = {0.08f, 0.32f, 0.36f},
                               hov[3] = {0.35f, 0.32f, 0.08f}, white[3] = {0.9f, 0.92f, 0.95f},
                               dim[3] = {0.55f, 0.6f, 0.65f}, cyan[3] = {0.3f, 0.95f, 1.0f};
            glm::vec3 back = -glm::cross(panel.right, panel.up) * 0.002f;
            hud.rect(panel.tl + back, panel.right, panel.up, panel.w, panel.h, bg);
            auto row_tl = [&](int r) { return panel.tl - panel.up * (r * row_h); };
            auto row_text = [&](int r, const std::string &s, const float *rgb) {
                hud.text(row_tl(r) + panel.right * 0.008f - panel.up * 0.004f, panel.right, panel.up, px,
                         s, rgb, 26);
            };
            row_text(0, "AGENTS", cyan);
            for (int i = 0; i < VRS_AGENTS_MAX; i++) {
                const VrsAgent &ag = hd.agents[i];
                if (ag.thing == 0)
                    continue;
                int r = 1 + i;
                if (i == selected_agent)
                    hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, sel);
                if (on_panel && hover_row == r)
                    hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, hov);
                std::string wname = ag.current_weapon < VRS_WEAPON_TYPES
                    ? weapon_name(ag.current_weapon) : std::string();
                char buf[64];
                snprintf(buf, sizeof(buf), "%d %s", i + 1,
                         (ag.flags & VrsTF_DEAD) ? "DEAD" : (wname.empty() ? "-" : wname.c_str()));
                row_text(r, buf, (ag.flags & VrsTF_DEAD) ? dim : white);
                // health bar along the bottom of the row
                if (ag.max_health > 0 && !(ag.flags & VrsTF_DEAD)) {
                    float f = std::clamp((float)ag.health / ag.max_health, 0.0f, 1.0f);
                    float hb[3] = {std::min(1.0f, 2 * (1 - f)), std::min(1.0f, 2 * f), 0.1f};
                    hud.rect(row_tl(r) + panel.right * 0.17f - panel.up * 0.008f - back, panel.right,
                             panel.up, 0.09f * f, 0.008f, hb);
                }
            }
            row_text(5, "WEAPONS", cyan);
            for (size_t k = 0; k < weapon_rows.size(); k++) {
                int r = 6 + (int)k;
                int w = weapon_rows[k];
                if (selected_agent >= 0 && hd.agents[selected_agent].current_weapon == w)
                    hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, sel);
                if (on_panel && hover_row == r)
                    hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, hov);
                row_text(r, weapon_name(w), white);
            }
            // objective board
            std::vector<std::string> lines = wrap_text(hd.objective, 34);
            if (!lines.empty()) {
                const float bpx = 0.003f, brow = 0.04f;
                glm::vec3 bc = glm::vec3(tv.table_pose * glm::vec4(0.0f, 0.42f, -0.85f, 1.0f));
                Panel board = make_panel(bc, eye_world, 34 * 6 * bpx + 0.03f,
                                         (lines.size() + 1) * brow, brow);
                glm::vec3 bback = -glm::cross(board.right, board.up) * 0.002f;
                hud.rect(board.tl + bback, board.right, board.up, board.w, board.h, bg);
                hud.text(board.tl + board.right * 0.015f - board.up * 0.006f, board.right, board.up,
                         bpx * 0.8f, "OBJECTIVE", cyan);
                for (size_t l = 0; l < lines.size(); l++)
                    hud.text(board.tl + board.right * 0.015f - board.up * (brow * (l + 1) + 0.006f),
                             board.right, board.up, bpx, lines[l], white);
            }
        }

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
            for (const VrsAgent &ag : hd.agents)
                if (ag.thing == thing && thing != 0)
                    return true;
            return false;
        };

        // commands
        if (snap.valid && snap.hdr.in_mission) {
            if (act[1]) {
                if (on_panel) {
                    if (hover_row >= 1 && hover_row <= 4 && hd.agents[hover_row - 1].thing != 0)
                        cmds.send(VrsCmd_SELECT_AGENT, (int16_t)hd.agents[hover_row - 1].thing);
                    else if (hover_row >= 6 && hover_row - 6 < (int)weapon_rows.size())
                        cmds.send(VrsCmd_WEAPON_SELECT, 0, 0, 0, weapon_rows[hover_row - 6]);
                } else if (hit[1].kind == Hit::PERSON && own_agent(hit[1].thing)) {
                    cmds.send(VrsCmd_SELECT_AGENT, (int16_t)hit[1].thing);
                } else if (hit[1].kind == Hit::VEHICLE) {
                    cmds.send(VrsCmd_ENTER_VEHICLE, (int16_t)hit[1].thing);
                } else if (hit[1].kind != Hit::NONE) {
                    cmds.send(VrsCmd_GOTO, 0, (int32_t)hit[1].p.x, (int32_t)hit[1].p.z);
                }
            }
            if (act[0]) {
                if ((hit[0].kind == Hit::PERSON && !own_agent(hit[0].thing)) || hit[0].kind == Hit::VEHICLE)
                    cmds.send(VrsCmd_SHOOT_THING, (int16_t)hit[0].thing);
                else if (hit[0].kind == Hit::GROUND || hit[0].kind == Hit::PERSON)
                    cmds.send(VrsCmd_SHOOT_GROUND, 0, (int32_t)hit[0].p.x, (int32_t)hit[0].p.z);
            }
            if (wep_next)
                cmds.send(VrsCmd_WEAPON_NEXT);
            if (wep_prev)
                cmds.send(VrsCmd_WEAPON_PREV);
        }

        // rings on the table: own agents, the selected one, what is pointed at
        if (snap.valid && snap.hdr.in_mission) {
            static const float ring_sel[3] = {0.2f, 1.0f, 0.4f}, ring_own[3] = {0.2f, 0.8f, 0.9f},
                               ring_r[3] = {1.0f, 0.9f, 0.2f}, ring_l[3] = {1.0f, 0.2f, 0.15f};
            float pulse = 0.85f + 0.15f * std::sin((float)t * 6.0f);
            for (const Picker::Person &pp : people_now) {
                if (pp.flags & (VrsTF_HIDDEN | VrsTF_DEAD))
                    continue;
                if (pp.thing == snap.hdr.selected)
                    ground_ring(people_verts, pp.p, 85.0f * pulse, 14.0f, ring_sel, eye_map);
                else if (pp.flags & VrsTF_PLAYER_AGENT)
                    ground_ring(people_verts, pp.p, 70.0f, 8.0f, ring_own, eye_map);
            }
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
            if (in.voxel_pressed)
                use_voxels = !use_voxels;
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
    if (in_process)
        core.stop();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
