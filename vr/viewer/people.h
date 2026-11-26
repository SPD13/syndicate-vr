// Syndicate Wars VR viewer: people, drawn with the game's sprites.
#pragma once

#include "render_classic.h"
#include "hud.h"

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
            for (const SwvDynGroup &g : snap.dyn_groups)
                cur_groups_[g.id] = g;
            prev_pos_.swap(cur_pos_);
            cur_pos_.clear();
            for (const SwvEffect &e : snap.effects)
                cur_pos_[e.id] = glm::vec3(e.x, e.y, e.z);
            for (const SwvBar &b : snap.bars)
                cur_pos_[0x80000000u ^ b.id] = glm::vec3(b.x, b.y, b.z);
            for (const SwvGlow &g : snap.glows)
                cur_pos_[0x40000000u ^ g.id] = glm::vec3(g.x, g.y, g.z);
            prev_shadows_.swap(cur_shadows_);
            cur_shadows_.clear();
            for (const SwvShadow &sh : snap.shadows)
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
    void moving_geometry(double now, std::vector<SwvVert> &out) const
    {
        float a = turn_fraction(now);
        out = cur_dyn_;
        for (const auto &kv : cur_groups_) {
            auto it = prev_groups_.find(kv.first);
            if (it == prev_groups_.end() || it->second.count != kv.second.count)
                continue;
            const SwvDynGroup &g = kv.second, &pg = it->second;
            if (g.first + g.count > out.size() || pg.first + pg.count > prev_dyn_.size())
                continue;
            // skip teleports, such as things appearing elsewhere
            const SwvVert &v0 = out[g.first], &p0 = prev_dyn_[pg.first];
            float dx = v0.x - p0.x, dz = v0.z - p0.z;
            if (dx * dx + dz * dz > 2048.0f * 2048.0f)
                continue;
            for (uint32_t i = 0; i < g.count; i++) {
                SwvVert &v = out[g.first + i];
                const SwvVert &pv = prev_dyn_[pg.first + i];
                v.x = pv.x + (v.x - pv.x) * a;
                v.y = pv.y + (v.y - pv.y) * a;
                v.z = pv.z + (v.z - pv.z) * a;
            }
        }
    }

    // A person drawn as a 3D model: where, and its facing (radians).
    struct ModelPlace {
        uint16_t thing;
        glm::vec3 p;
        float phi;
    };

    // eye: viewer position in map units, to face the sprites towards it.
    // People for which as_model is true get no sprite: their place goes to
    // places, for the upgraded look.
    void build(std::vector<GpuVert> &out, std::vector<GpuVert> &glow_out, double now,
               const WorldSnapshot &snap, glm::vec3 eye, Renderer &rend,
               const std::function<bool(uint16_t)> &as_model = nullptr,
               std::vector<ModelPlace> *places = nullptr) const
    {
        out.clear();
        glow_out.clear();
        rend.instances.clear();
        if (places)
            places->clear();
        float dt = (float)std::clamp(now - last_build_, 0.0, 0.1);
        last_build_ = now;
        float a = (float)std::clamp((now - turn_time_) / std::max(turn_dt_, 0.01), 0.0, 1.0);
        // shadows under vehicles and people, before anything they could cover
        for (const auto &kv : cur_shadows_) {
            SwvShadow sh = kv.second;
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
            const SwvThing &t = kv.second;
            if ((t.flags & SwvTF_HIDDEN) || (t.flags & SwvTF_DEAD))
                continue;
            add_shadow(out, position(kv.first, t, a));
        }
        for (const auto &kv : cur_) {
            const SwvThing &t = kv.second;
            if (t.flags & SwvTF_HIDDEN)
                continue;
            glm::vec3 p = position(kv.first, t, a);
            if (places && as_model && as_model(kv.first)) {
                places->push_back({kv.first, p, facing(kv.first, t, t.angle * 0.78539816f, dt)});
                continue;
            }
            float top = 0;
            if (!add_sprite(out, snap, t, p, eye, top))
                add_figure(out, t, p);
        }
        for (const SwvEffect &e : snap.effects) {
            glm::vec3 p = glide(e.id, glm::vec3(e.x, e.y, e.z), a);
            glm::vec2 fwd;
            view_angle(eye, p, fwd);
            add_frame(out, snap, e.frame, nullptr, p, fwd, e.scale / 256.0f,
                      std::clamp(e.brightness / 32.0f, 0.3f, 1.5f));
        }
        uint32_t seed = snap.hdr.gameturn * 2654435761u;
        for (const SwvLine &l : snap.lines)
            add_line(out, l, eye, rend, seed);
        // health bars over damaged vehicles, guns and people
        for (const SwvBar &b : snap.bars)
            add_bar(out, glide(0x80000000u ^ b.id, glm::vec3(b.x, b.y, b.z), a), b.frac, 220, eye);
        for (const auto &kv : cur_) {
            const SwvThing &t = kv.second;
            if ((t.flags & (SwvTF_HIDDEN | SwvTF_DEAD)) || t.max_health <= 0 ||
                t.health >= t.max_health || t.health <= 0)
                continue;
            glm::vec3 p = position(kv.first, t, a);
            add_bar(out, p + glm::vec3(0, 250, 0), (float)t.health / t.max_health, 110, eye);
        }
        // glows, drawn by the renderer after everything else
        for (const SwvGlow &g : snap.glows) {
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

    // Smoothly turning facing of a 3D figure, in radians. Eases towards
    // the direction of walking, when close to the game's 8-way facing
    // phi_game, else towards phi_game itself, instead of snapping. Facing
    // A of the game is walking towards atan2(vx, vz) = A * 45 degrees, the
    // same rotation as the figure's.
    float facing(uint16_t index, const SwvThing &t, float phi_game, float dt) const
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
    glm::vec3 position(uint16_t index, const SwvThing &t, float a) const
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

    void add_line(std::vector<GpuVert> &out, const SwvLine &l, glm::vec3 eye,
                  const Renderer &rend, uint32_t &seed) const
    {
        glm::vec3 a(l.x1, l.y1, l.z1), b(l.x2, l.y2, l.z2);
        float len = glm::distance(a, b);
        if (len < 1.0f || len > 40000.0f)
            return;
        float rgb[3];
        rend.palette_rgb(rend.palette(), l.colour, rgb);
        float w = std::max<float>(l.width, 2.0f);
        if (l.flags & SwvLF_JAGGED) {
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
        if (l.flags & SwvLF_GLOW) {
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

    static int frame_for(const WorldSnapshot &snap, const SwvThing &t, int cam_angle)
    {
        int idx;
        switch (t.anim_mode) {
        case SwvAM_DIR8: {
            int cam8 = ((cam_angle + 128) >> 8) & 7;
            idx = t.anim_start + 1 + ((t.angle + 8 - cam8) & 7);
            break;
        }
        case SwvAM_DIR12: {
            int cam12 = ((cam_angle + 85) / 170) % 12;
            idx = t.anim_start + 1 + (((3 * t.angle >> 1) + 12 - cam12) % 12);
            break;
        }
        case SwvAM_FIXED:
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
                           const SwvThing &t, glm::vec3 p, glm::vec3 eye, float &top)
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
            const SwvElement &e = snap.elements[el];
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
            const SwvSpriteRect &r = snap.sprite_rects[si];
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
    static float add_figure(std::vector<GpuVert> &out, const SwvThing &t, glm::vec3 p)
    {
        float rgb[3];
        colour_for(t, rgb);
        if (t.flags & SwvTF_DEAD) {
            Renderer::add_box(out, p + glm::vec3(-70, 0, -24), p + glm::vec3(70, 14, 24), rgb);
            return 14;
        }
        float w = 26, h = 160, hw = 20;
        Renderer::add_box(out, p + glm::vec3(-w, 0, -w), p + glm::vec3(w, h, w), rgb);
        float skin[3] = {0.85f, 0.7f, 0.55f};
        Renderer::add_box(out, p + glm::vec3(-hw, h, -hw), p + glm::vec3(hw, h + 2 * hw, hw), skin);
        return h + 2 * hw;
    }

    static void colour_for(const SwvThing &t, float *rgb)
    {
        float c[3] = {0.75f, 0.75f, 0.7f};
        if (t.flags & SwvTF_PLAYER_AGENT) {
            c[0] = 0.1f; c[1] = 0.9f; c[2] = 1.0f;
        } else {
            // vary by subtype so different kinds of people are distinguishable
            static const float pal[8][3] = {
                {0.8f, 0.8f, 0.75f}, {0.95f, 0.25f, 0.2f}, {0.25f, 0.4f, 0.95f}, {0.95f, 0.8f, 0.2f},
                {0.6f, 0.35f, 0.8f}, {0.3f, 0.75f, 0.35f}, {0.9f, 0.55f, 0.25f}, {0.5f, 0.5f, 0.5f}};
            const float *s = pal[t.subtype & 7];
            c[0] = s[0]; c[1] = s[1]; c[2] = s[2];
        }
        if (t.flags & SwvTF_DEAD) {
            c[0] *= 0.4f; c[1] *= 0.2f; c[2] *= 0.2f;
        }
        memcpy(rgb, c, sizeof(c));
    }

    std::unordered_map<uint16_t, SwvThing> cur_, prev_;
    mutable std::unordered_map<uint16_t, float> facing_;
    mutable double last_build_ = 0;
    std::vector<SwvVert> cur_dyn_, prev_dyn_;
    std::unordered_map<uint32_t, SwvDynGroup> cur_groups_, prev_groups_;
    std::unordered_map<uint32_t, glm::vec3> cur_pos_, prev_pos_;
    std::unordered_map<uint32_t, SwvShadow> cur_shadows_, prev_shadows_;
    uint32_t last_turn_ = 0;
    double turn_time_ = 0, turn_dt_ = 1.0 / 16;
};
