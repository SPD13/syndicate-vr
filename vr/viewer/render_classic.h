// Syndicate Wars VR viewer: the classic look: the world drawn with the game's textures.
#pragma once

#include "world_source.h"

/******************************************************************************/
// Rendering

struct GpuVert {
    float pos[3];
    float uv[2];
    float layer;
    float rgb[3];
    // 0 flat colour, 1 texture page (tinted by rgb), 2 sprite page (tinted
    // by rgb), 3 unlit colour, 4 shadow (translucent black), 5 soft glow,
    // 6 marker (unlit colour, alpha in uv[0])
    float textured;
    float fx; // SwvVertFlags bits, for water, glass and shine
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
uniform float u_marker_alpha;
out vec4 o_col;
void main() {
    if (u_clip != 0) {
        vec2 d = abs(v_map.xz - u_clip_center.xz);
        if (d.x > u_clip_half || d.y > u_clip_half)
            discard;
    }
    vec3 col = v_rgb;
    if (v_textured > 5.5) {
        // VR markers: emissive colour, partly see-through
        if (u_linear_out != 0)
            col = pow(col, vec3(2.2));
        o_col = vec4(col, clamp(v_uv.x, 0.0, 1.0) * u_marker_alpha);
        return;
    }
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
        u_marker_alpha_ = glGetUniformLocation(prog_, "u_marker_alpha");
        for (auto *m : {&static_mesh_, &static_trans_, &dyn_mesh_, &dyn_trans_, &people_mesh_,
                        &table_mesh_, &glow_mesh_, &hud_mesh_, &marker_mesh_})
            make_mesh(*m);
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, SWV_TMAP_DIM, SWV_TMAP_DIM, SWV_TMAP_PAGES,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glGenTextures(1, &spr_tex_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, spr_tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, SWV_TMAP_DIM, SWV_TMAP_DIM, 1,
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
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, SWV_TMAP_DIM, SWV_TMAP_DIM,
                            SWV_TMAP_PAGES, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
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
    void upload_dyn(const std::vector<SwvVert> &verts) { upload_verts(dyn_mesh_, dyn_trans_, verts); }

    float turn = 0; // game turn with fraction, for animated terrain

    void upload_sprites(const WorldSnapshot &snap)
    {
        size_t texels = snap.sprite_pages.size() / 2;
        int pages = (int)(texels / (SWV_TMAP_DIM * SWV_TMAP_DIM));
        std::vector<uint8_t> rgba(texels * 4);
        for (size_t i = 0; i < texels; i++) {
            uint8_t c = snap.sprite_pages[i * 2];
            for (int k = 0; k < 3; k++)
                rgba[i * 4 + k] = (uint8_t)std::min(255.0f, palette_[c * 3 + k] * pal_scale_);
            rgba[i * 4 + 3] = snap.sprite_pages[i * 2 + 1];
        }
        glBindTexture(GL_TEXTURE_2D_ARRAY, spr_tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, SWV_TMAP_DIM, SWV_TMAP_DIM, pages,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        logf("sprites: %zu in %d pages, %zu frames", snap.sprite_rects.size(), pages,
             snap.frames.size());
    }

    void upload_people(const std::vector<GpuVert> &verts) { upload_gpu(people_mesh_, verts); }
    void upload_glows(const std::vector<GpuVert> &verts) { upload_gpu(glow_mesh_, verts); }
    // Pointers and panels, in world space (meters).
    void upload_hud(const std::vector<GpuVert> &verts) { upload_gpu(hud_mesh_, verts); }
    // VR world markers, in map units (hud_vr.h)
    void upload_markers(const std::vector<GpuVert> &verts) { upload_gpu(marker_mesh_, verts); }

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
        // 3D models of the upgraded look, with their own program
        if (draw_models) {
            draw_models(mvp, clip_center, clip_half, linear_out);
            glUseProgram(prog_);
            glActiveTexture(GL_TEXTURE0);
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
        // markers: hidden by what is in front, then a faint copy over
        // everything, so agents behind buildings stay in sight
        glUniform1f(u_marker_alpha_, 1.0f);
        draw_mesh(marker_mesh_);
        glDisable(GL_DEPTH_TEST);
        glUniform1f(u_marker_alpha_, 0.25f);
        draw_mesh(marker_mesh_);
        glEnable(GL_DEPTH_TEST);
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
    // draws other models in map space (mvp: map units -> clip)
    std::function<void(const glm::mat4 &mvp, glm::vec3 clip_center, float clip_half, bool linear_out)>
        draw_models;

private:

    // Splits see-through faces into their own mesh, drawn after the rest.
    void upload_verts(Mesh &opaque, Mesh &trans, const std::vector<SwvVert> &src)
    {
        std::vector<GpuVert> vo, vt;
        vo.reserve(src.size());
        for (size_t i = 0; i + 2 < src.size(); i += 3) {
            bool see_through = (src[i].flags & SwvVF_TRANSLUCENT) != 0;
            for (size_t k = i; k < i + 3; k++) {
                const SwvVert &s = src[k];
                GpuVert g{};
                g.pos[0] = s.x; g.pos[1] = s.y; g.pos[2] = s.z;
                g.uv[0] = (s.u + 0.5f) / SWV_TMAP_DIM;
                g.uv[1] = (s.v + 0.5f) / SWV_TMAP_DIM;
                g.layer = s.page;
                g.textured = (s.flags & SwvVF_TEXTURED) ? 1.0f : 0.0f;
                g.fx = (float)s.flags;
                // the game's light level, 128 is unchanged
                float light = std::clamp(s.shade / 128.0f, 0.15f, 1.9f);
                if (s.flags & SwvVF_TEXTURED) {
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
          u_light_ = -1, u_inst_ = -1, u_tint_ = -1, u_turn_ = -1, u_marker_alpha_ = -1;
    Mesh static_mesh_, static_trans_, dyn_mesh_, dyn_trans_, people_mesh_, table_mesh_, glow_mesh_,
        hud_mesh_, marker_mesh_;
    uint8_t palette_[768]{};
    float pal_scale_ = 4.0f;
    bool have_palette_ = false;
};
