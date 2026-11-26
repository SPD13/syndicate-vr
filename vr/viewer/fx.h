// Syndicate Wars VR viewer: particle effects of the upgraded look.
//
// The effects of the assets repository (effects/effects.json; HANDOVER.md
// 7.1 and 10.4 there): explosions, fires, smoke, gas, impacts, sparkles.
// The simulation follows the reference simulator particles_sim.py (branch
// assets/tripo, tools/asset3d): emit bursts then rates (with fade-out),
// then per particle gravity and buoyancy, drag exp(-drag dt), the
// simulator's smooth turbulence, ground bounce or death, on_death
// sub-effects and trails. Positions are in metres (map units / kMetre) and
// drawn as quads facing the viewer: alpha ones back to front, then the
// additive ones; each effect's light is a soft pool on the ground and its
// shockwave a ring. Render-only, with its own random numbers: it never
// touches the game.
#pragma once

#include "common.h"
#include "json_min.h"
#include "render_classic.h"

class FxSystem {
public:
    static constexpr float kMetre = 110.0f; // map units per metre, as hud_vr.h
    static constexpr size_t kMaxParticles = 6000;

    bool load(const std::string &path)
    {
        Json root;
        if (!JsonParser::parse_file(path, root) || root.type != Json::OBJ) {
            logf("effects: cannot read %s", path.c_str());
            return false;
        }
        for (const auto &kv : root.obj)
            if (kv.first[0] != '_')
                index_[kv.first] = (int)defs_.size(), defs_.emplace_back();
        for (const auto &kv : root.obj)
            if (kv.first[0] != '_')
                read_def(kv.first, kv.second, defs_[index_[kv.first]]);
        logf("effects: %zu loaded", defs_.size());
        return !defs_.empty();
    }

    bool loaded() const { return !defs_.empty(); }

    /** Starts an effect at a map position, on ground at map height gy;
     * returns its handle (0 if unknown). scale enlarges it (impacts). */
    int start(const std::string &name, glm::vec3 map_pos, float gy, float scale = 1.0f)
    {
        auto it = index_.find(name);
        if (it == index_.end())
            return 0;
        Inst in;
        in.def = it->second;
        in.origin = glm::vec3(map_pos.x, gy, map_pos.z) / kMetre;
        in.scale = scale;
        in.t0 = time_;
        in.acc.assign(defs_[in.def].em.size(), 0.0f);
        in.burst_done.assign(defs_[in.def].em.size(), false);
        int h = ++next_handle_;
        insts_[h] = in;
        return h;
    }

    /** Stops emitting (the particles live on). */
    void stop(int h)
    {
        auto it = insts_.find(h);
        if (it != insts_.end() && !it->second.stopped) {
            it->second.stopped = true;
            it->second.stop_t = time_;
        }
    }

    /** Moves an effect that follows something (map position, ground). */
    void move(int h, glm::vec3 map_pos, float gy)
    {
        auto it = insts_.find(h);
        if (it != insts_.end())
            it->second.origin = glm::vec3(map_pos.x, gy, map_pos.z) / kMetre;
    }

    bool alive(int h) const { return insts_.count(h) != 0; }

    void clear()
    {
        insts_.clear();
        parts_.clear();
    }

    void update(float dt)
    {
        dt = std::min(dt, 1.0f / 30.0f);
        if (dt <= 0)
            return;
        time_ += dt;
        float t = time_;
        std::vector<std::pair<int, glm::vec3>> deaths; // (def, origin metres)
        for (auto it = insts_.begin(); it != insts_.end();) {
            Inst &in = it->second;
            const FxDef &d = defs_[in.def];
            float lt = t - in.t0;
            for (size_t i = 0; i < d.em.size(); i++) {
                const FxEmitter &em = d.em[i];
                if (lt < em.delay || in.stopped)
                    continue;
                if (!in.burst_done[i] && em.burst > 0) {
                    for (int k = 0; k < em.burst; k++)
                        make(in, (int)i, in.origin);
                    in.burst_done[i] = true;
                }
                float edur = em.duration >= 0 ? em.duration : (d.loop ? 1e9f : d.duration);
                if (em.rate > 0 && lt < em.delay + edur) {
                    float fade = 1.0f;
                    if (em.fade_out > 0 && lt > edur - em.fade_out)
                        fade = std::max(0.0f, (edur - lt) / em.fade_out);
                    in.acc[i] += em.rate * dt * fade;
                    while (in.acc[i] >= 1.0f) {
                        in.acc[i] -= 1.0f;
                        make(in, (int)i, in.origin);
                    }
                }
            }
            bool over = in.stopped ? (t - in.stop_t > 8.0f) : (!d.loop && lt > d.duration + 8.0f);
            it = over ? insts_.erase(it) : std::next(it);
        }
        for (size_t i = 0; i < parts_.size();) {
            Part &p = parts_[i];
            const FxEmitter &em = defs_[p.def].em[p.em];
            p.prev = p.pos;
            p.age += dt;
            bool dead = p.age >= p.life;
            if (dead && em.on_death >= 0)
                deaths.push_back({em.on_death, glm::vec3(p.pos.x, p.ground, p.pos.z)});
            if (!dead) {
                p.vel.y += (-em.gravity + em.buoyancy) * dt * p.scale;
                p.vel *= std::exp(-em.drag * dt);
                glm::vec3 move = p.vel * dt;
                if (em.turbulence > 0) {
                    float s = p.seed;
                    glm::vec3 tb(std::sin(1.7f * t + s) + 0.5f * std::sin(3.1f * t + 2 * s),
                                 0.4f * std::sin(2.3f * t + 3 * s),
                                 std::cos(1.9f * t + 4 * s) + 0.5f * std::cos(2.9f * t + s));
                    move += tb * em.turbulence * dt * p.scale;
                }
                p.pos += move;
                if (p.pos.y < p.ground && em.ground != 0) {
                    if (em.ground == 2) {
                        if (em.on_death >= 0)
                            deaths.push_back({em.on_death, glm::vec3(p.pos.x, p.ground, p.pos.z)});
                        dead = true;
                    } else {
                        p.pos.y = p.ground;
                        p.vel = glm::vec3(p.vel.x * 0.6f, -p.vel.y * 0.35f, p.vel.z * 0.6f);
                    }
                }
                if (!dead && em.trail >= 0) {
                    const FxDef &tr = defs_[em.trail];
                    for (size_t k = 0; k < tr.em.size(); k++) {
                        p.trail_acc += tr.em[k].rate * dt;
                        while (p.trail_acc >= 1.0f) {
                            p.trail_acc -= 1.0f;
                            Inst tmp;
                            tmp.def = em.trail;
                            tmp.origin = glm::vec3(p.pos.x, p.ground, p.pos.z);
                            tmp.scale = p.scale;
                            spawn_at(tmp, (int)k, p.pos);
                        }
                    }
                }
            }
            if (dead) {
                parts_[i] = parts_.back();
                parts_.pop_back();
            } else {
                i++;
            }
        }
        for (const auto &d : deaths) {
            Inst in;
            in.def = d.first;
            in.origin = d.second;
            in.t0 = time_;
            in.acc.assign(defs_[in.def].em.size(), 0.0f);
            in.burst_done.assign(defs_[in.def].em.size(), false);
            insts_[++next_handle_] = in;
        }
    }

    /** Draws the particles, lights and rings (inside Renderer::draw, map
     * space, after the opaque world). */
    void draw(const glm::mat4 &mvp, glm::vec3 eye_map, glm::vec3 clip_center, float clip_half, bool linear_out)
    {
        if (parts_.empty() && insts_.empty())
            return;
        if (prog_ == 0 && !init_gl())
            return;
        glm::vec3 eye = eye_map / kMetre;
        alpha_.clear();
        add_.clear();
        // lights: a soft pool on the ground; shockwave rings
        for (const auto &kv : insts_) {
            const Inst &in = kv.second;
            const FxDef &d = defs_[in.def];
            float lt = time_ - in.t0;
            if (d.has_ring && lt >= 0 && lt < d.ring_life) {
                float rr = d.ring_radius.at(lt / d.ring_life) * in.scale;
                glm::vec4 c = d.ring_rgba;
                c.a *= 1.0f - lt / d.ring_life;
                ground_quad(add_, in.origin, rr * 1.1f, 5, c);
            }
            if (d.has_light && !in.stopped) {
                float inten = d.light_int.at(lt);
                if (d.flicker > 0)
                    inten *= 1 + d.flicker * (std::sin(lt * 23.0f) * 0.6f + std::sin(lt * 37.0f + 1.3f) * 0.4f);
                if (inten > 0) {
                    glm::vec4 c(d.light_rgb, std::min(0.45f, inten * 0.03f));
                    ground_quad(add_, in.origin, std::min(d.light_radius, 6.0f) * in.scale, 0, c);
                }
            }
        }
        // particles
        sorted_.clear();
        for (const Part &p : parts_)
            if (defs_[p.def].em[p.em].blend == 1)
                sorted_.push_back({glm::distance(p.pos, eye), &p});
        std::sort(sorted_.begin(), sorted_.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
        for (const auto &sp : sorted_)
            particle(alpha_, *sp.second, eye);
        for (const Part &p : parts_)
            if (defs_[p.def].em[p.em].blend == 0)
                particle(add_, p, eye);

        glUseProgram(prog_);
        glm::mat4 m = mvp * glm::scale(glm::mat4(1.0f), glm::vec3(kMetre));
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(m));
        glUniform3f(u_clip_center_, clip_center.x / kMetre, 0, clip_center.z / kMetre);
        glUniform1f(u_clip_half_, clip_half / kMetre);
        glUniform1i(u_linear_, linear_out ? 1 : 0);
        // particles positions are in metres
        g_fog.bind(prog_, kMetre);
        g_fog.kind(prog_, FogK_MOVING);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
        glUniform1i(u_tex_, 4);
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        auto pass = [&](const std::vector<Vert> &v) {
            if (v.empty())
                return;
            glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vert), v.data(), GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)v.size());
        };
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        pass(alpha_);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        pass(add_);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

    size_t particle_count() const { return parts_.size(); }

private:
    struct Curve1 {
        std::vector<std::pair<float, float>> k;
        float at(float t) const
        {
            if (k.empty())
                return 0;
            if (t <= k[0].first)
                return k[0].second;
            for (size_t i = 1; i < k.size(); i++)
                if (t <= k[i].first) {
                    float a = (t - k[i - 1].first) / std::max(1e-9f, k[i].first - k[i - 1].first);
                    return k[i - 1].second * (1 - a) + k[i].second * a;
                }
            return k.back().second;
        }
    };
    struct Curve4 {
        std::vector<std::pair<float, glm::vec4>> k;
        glm::vec4 at(float t) const
        {
            if (k.empty())
                return glm::vec4(1);
            if (t <= k[0].first)
                return k[0].second;
            for (size_t i = 1; i < k.size(); i++)
                if (t <= k[i].first) {
                    float a = (t - k[i - 1].first) / std::max(1e-9f, k[i].first - k[i - 1].first);
                    return glm::mix(k[i - 1].second, k[i].second, a);
                }
            return k.back().second;
        }
    };
    struct FxEmitter {
        int blend = 0;  // 0 add, 1 alpha
        int shape = 0;  // 0 soft, 1 puff, 2 streak, 3 chunk
        int spawn = 0;  // 0 point, 1 sphere, 2 disc, 3 box
        float spawn_r = 0;
        glm::vec3 spawn_box{0}, offset{0}, dir{0, 1, 0};
        int burst = 0;
        float rate = 0, duration = -1, delay = 0, fade_out = 0;
        float life_min = 1, life_max = 1, spread = 0, speed_min = 0, speed_max = 0;
        float gravity = 0, buoyancy = 0, drag = 0, turbulence = 0, intensity = 1, size_jitter = 0;
        Curve1 size;
        Curve4 color;
        int ground = 0; // 0 none, 1 bounce, 2 die
        int on_death = -1, trail = -1;
    };
    struct FxDef {
        std::vector<FxEmitter> em;
        float duration = 3.0f;
        bool loop = false;
        bool has_light = false, has_ring = false;
        glm::vec3 light_rgb{1};
        Curve1 light_int, ring_radius;
        float light_radius = 1, flicker = 0, ring_life = 1;
        glm::vec4 ring_rgba{1};
    };
    struct Inst {
        int def = 0;
        glm::vec3 origin{0}; // metres; y is the ground
        float scale = 1, t0 = 0, stop_t = 0;
        bool stopped = false;
        std::vector<float> acc;
        std::vector<bool> burst_done;
    };
    struct Part {
        glm::vec3 pos, vel, prev;
        float age, life, jit, seed, trail_acc, ground, scale;
        int def, em;
    };
    struct Vert {
        float pos[3];
        float uv[2];
        float layer;
        float rgba[4];
    };

    void read_def(const std::string &name, const Json &j, FxDef &d)
    {
        d.duration = (float)j.n("duration", 3.0);
        const Json *lp = j.get("loop");
        d.loop = lp && lp->type == Json::BOOL && lp->b;
        if (const Json *l = j.get("light")) {
            d.has_light = true;
            d.light_rgb = vec3_of(l->get("color"), glm::vec3(1));
            d.light_int = curve1(l->get("intensity"));
            d.light_radius = (float)l->n("radius", 1);
            d.flicker = (float)l->n("flicker", 0);
        }
        if (const Json *r = j.get("ring")) {
            d.has_ring = true;
            d.ring_radius = curve1(r->get("radius"));
            d.ring_life = (float)r->n("life", 1);
            const Json *c = r->get("color");
            if (c && c->size() >= 4)
                d.ring_rgba = glm::vec4(c->arr[0].num, c->arr[1].num, c->arr[2].num, c->arr[3].num);
        }
        const Json *ems = j.get("emitters");
        for (size_t i = 0; ems && i < ems->size(); i++) {
            const Json &e = ems->arr[i];
            FxEmitter em;
            em.blend = e.s("blend") == "alpha" ? 1 : 0;
            std::string sh = e.s("shape", "soft");
            em.shape = sh == "puff" ? 1 : sh == "streak" ? 2 : sh == "chunk" ? 3 : 0;
            if (const Json *sp = e.get("spawn")) {
                if (sp->has("sphere"))
                    em.spawn = 1, em.spawn_r = (float)sp->n("sphere");
                else if (sp->has("disc"))
                    em.spawn = 2, em.spawn_r = (float)sp->n("disc");
                else if (sp->has("box"))
                    em.spawn = 3, em.spawn_box = vec3_of(sp->get("box"), glm::vec3(0));
            }
            em.offset = vec3_of(e.get("offset"), glm::vec3(0));
            em.burst = (int)e.n("burst", 0);
            em.rate = (float)e.n("rate", 0);
            em.duration = (float)e.n("duration", -1);
            em.delay = (float)e.n("delay", 0);
            em.fade_out = (float)e.n("fade_out", 0);
            if (const Json *l = e.get("life"); l && l->size() >= 2)
                em.life_min = (float)l->arr[0].num, em.life_max = (float)l->arr[1].num;
            if (const Json *v = e.get("vel")) {
                bool has_dir = v->has("dir");
                em.dir = vec3_of(v->get("dir"), glm::vec3(0, 1, 0));
                em.spread = (float)v->n("spread", has_dir ? 0 : 180);
                if (const Json *s = v->get("speed"); s && s->size() >= 2)
                    em.speed_min = (float)s->arr[0].num, em.speed_max = (float)s->arr[1].num;
            } else {
                em.spread = 180;
            }
            em.gravity = (float)e.n("gravity", 0);
            em.buoyancy = (float)e.n("buoyancy", 0);
            em.drag = (float)e.n("drag", 0);
            em.turbulence = (float)e.n("turbulence", 0);
            em.intensity = (float)e.n("intensity", 1);
            em.size_jitter = (float)e.n("size_jitter", 0);
            em.size = curve1(e.get("size"));
            if (const Json *c = e.get("color"))
                for (const Json &k : c->arr)
                    if (k.size() >= 2 && k.arr[1].size() >= 4)
                        em.color.k.push_back({(float)k.arr[0].num,
                                              glm::vec4(k.arr[1].arr[0].num, k.arr[1].arr[1].num,
                                                        k.arr[1].arr[2].num, k.arr[1].arr[3].num)});
            std::string g = e.s("ground");
            em.ground = g == "bounce" ? 1 : g == "die" ? 2 : 0;
            auto ref = [&](const char *k) {
                std::string n = e.s(k);
                auto it = index_.find(n);
                return it == index_.end() ? -1 : it->second;
            };
            em.on_death = ref("on_death");
            em.trail = ref("trail");
            d.em.push_back(em);
        }
        (void)name;
    }

    static glm::vec3 vec3_of(const Json *j, glm::vec3 def)
    {
        if (j == nullptr || j->type != Json::ARR || j->size() < 3)
            return def;
        return glm::vec3(j->arr[0].num, j->arr[1].num, j->arr[2].num);
    }

    static Curve1 curve1(const Json *j)
    {
        Curve1 c;
        if (j)
            for (const Json &k : j->arr)
                if (k.size() >= 2)
                    c.k.push_back({(float)k.arr[0].num, (float)k.arr[1].num});
        return c;
    }

    // xorshift: the viewer's own random numbers, never the game's
    float rnd()
    {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return (rng_ & 0xFFFFFF) / 16777216.0f;
    }
    float rnd(float a, float b) { return a + (b - a) * rnd(); }

    glm::vec3 rand_dir(glm::vec3 d, float spread_deg)
    {
        d = glm::normalize(d);
        if (spread_deg <= 0)
            return d;
        float cos_max = std::cos(glm::radians(std::min(180.0f, spread_deg)));
        float z = rnd(cos_max, 1.0f), phi = rnd(0, 6.2831853f);
        float r = std::sqrt(std::max(0.0f, 1 - z * z));
        glm::vec3 local(r * std::cos(phi), z, r * std::sin(phi));
        glm::vec3 up(0, 1, 0);
        if (std::fabs(glm::dot(d, up)) > 0.999f)
            return local * (d.y > 0 ? 1.0f : -1.0f);
        glm::vec3 k = glm::normalize(glm::cross(up, d));
        float ang = std::acos(std::clamp(glm::dot(up, d), -1.0f, 1.0f));
        return local * std::cos(ang) + glm::cross(k, local) * std::sin(ang) + k * glm::dot(k, local) * (1 - std::cos(ang));
    }

    glm::vec3 spawn_pos(const FxEmitter &em)
    {
        switch (em.spawn) {
        case 1: {
            glm::vec3 v;
            do
                v = glm::vec3(rnd(-1, 1), rnd(-1, 1), rnd(-1, 1));
            while (glm::dot(v, v) > 1 || glm::dot(v, v) < 1e-6f);
            return glm::normalize(v) * em.spawn_r * std::cbrt(rnd());
        }
        case 2: {
            float a = rnd(0, 6.2831853f), r = em.spawn_r * std::sqrt(rnd());
            return glm::vec3(r * std::cos(a), 0, r * std::sin(a));
        }
        case 3:
            return glm::vec3(rnd(-0.5f, 0.5f), rnd(-0.5f, 0.5f), rnd(-0.5f, 0.5f)) * em.spawn_box;
        default:
            return glm::vec3(0);
        }
    }

    void make(const Inst &in, int ei, glm::vec3 origin)
    {
        const FxEmitter &em = defs_[in.def].em[ei];
        spawn_at(in, ei, origin + (em.offset + spawn_pos(em)) * in.scale);
    }

    void spawn_at(const Inst &in, int ei, glm::vec3 pos)
    {
        const FxEmitter &em = defs_[in.def].em[ei];
        if (parts_.size() >= kMaxParticles) {
            // full: drop the oldest smoke rather than a flash
            size_t victim = parts_.size();
            float oldest = -1;
            for (size_t i = 0; i < parts_.size(); i++)
                if (defs_[parts_[i].def].em[parts_[i].em].blend == 1 && parts_[i].age > oldest)
                    oldest = parts_[i].age, victim = i;
            if (victim == parts_.size())
                return;
            parts_[victim] = parts_.back();
            parts_.pop_back();
        }
        Part p;
        p.pos = p.prev = pos;
        p.vel = rand_dir(em.dir, em.spread) * rnd(em.speed_min, em.speed_max) * in.scale;
        p.age = 0;
        p.life = rnd(em.life_min, em.life_max);
        p.jit = 1 + rnd(-1, 1) * em.size_jitter;
        p.seed = rnd(0, 100);
        p.trail_acc = 0;
        p.ground = in.origin.y;
        p.scale = in.scale;
        p.def = in.def;
        p.em = ei;
        parts_.push_back(p);
    }

    static void push(std::vector<Vert> &out, glm::vec3 p, float u, float v, float layer, glm::vec4 c)
    {
        Vert q;
        q.pos[0] = p.x;
        q.pos[1] = p.y;
        q.pos[2] = p.z;
        q.uv[0] = u;
        q.uv[1] = v;
        q.layer = layer;
        q.rgba[0] = c.r;
        q.rgba[1] = c.g;
        q.rgba[2] = c.b;
        q.rgba[3] = c.a;
        out.push_back(q);
    }

    static void quad(std::vector<Vert> &out, const glm::vec3 *q, float layer, glm::vec4 c)
    {
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        static const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (int k : tri)
            push(out, q[k], uv[k][0], uv[k][1], layer, c);
    }

    // A flat quad on the ground, 2 cm up (lights and rings).
    static void ground_quad(std::vector<Vert> &out, glm::vec3 c, float r, int layer, glm::vec4 col)
    {
        float y = c.y + 0.02f;
        glm::vec3 q[4] = {glm::vec3(c.x - r, y, c.z - r), glm::vec3(c.x + r, y, c.z - r),
                          glm::vec3(c.x + r, y, c.z + r), glm::vec3(c.x - r, y, c.z + r)};
        quad(out, q, (float)layer, col);
    }

    void particle(std::vector<Vert> &out, const Part &p, glm::vec3 eye)
    {
        const FxEmitter &em = defs_[p.def].em[p.em];
        float u = p.age / std::max(p.life, 1e-4f);
        float size = em.size.at(u) * p.jit * p.scale;
        glm::vec4 c = em.color.at(u);
        if (em.blend == 0)
            c = glm::vec4(glm::vec3(c) * em.intensity, c.a);
        glm::vec3 to_eye = eye - p.pos;
        if (glm::length(to_eye) < 1e-4f)
            return;
        to_eye = glm::normalize(to_eye);
        if (em.shape == 2) {
            // streak: along the movement, as wide as the size
            glm::vec3 v = p.pos - p.prev;
            float len = std::max(size * 3.0f, glm::length(v) * 2.5f);
            glm::vec3 dir = glm::length(v) > 1e-6f ? glm::normalize(v) : glm::vec3(0, 1, 0);
            glm::vec3 side = glm::cross(dir, to_eye);
            if (glm::length(side) < 1e-5f)
                return;
            side = glm::normalize(side) * (size * 0.5f);
            glm::vec3 a = p.pos - dir * len, b = p.pos;
            glm::vec3 q[4] = {a - side, b - side, b + side, a + side};
            quad(out, q, 0, c);
            return;
        }
        glm::vec3 right = glm::cross(glm::vec3(0, 1, 0), to_eye);
        if (glm::length(right) < 1e-5f)
            right = glm::vec3(1, 0, 0);
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(to_eye, right);
        // a turn per particle, so puffs do not all look alike
        float a = p.seed;
        glm::vec3 r2 = (right * std::cos(a) + up * std::sin(a)) * (size * 0.5f);
        glm::vec3 u2 = (up * std::cos(a) - right * std::sin(a)) * (size * 0.5f);
        glm::vec3 q[4] = {p.pos - r2 - u2, p.pos + r2 - u2, p.pos + r2 + u2, p.pos - r2 + u2};
        float layer = em.shape == 1 ? 1.0f + (float)((int)p.seed % 3) : em.shape == 3 ? 4.0f : 0.0f;
        quad(out, q, layer, c);
    }

    bool init_gl()
    {
        static const char *vs = R"(#version 330 core
layout(location=0) in vec3 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in float a_layer;
layout(location=3) in vec4 a_col;
uniform mat4 u_mvp;
out vec3 v_pos;
out vec2 v_uv;
flat out float v_layer;
out vec4 v_col;
void main() {
    v_pos = a_pos;
    v_uv = a_uv;
    v_layer = a_layer;
    v_col = a_col;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
)";
        static const char *fs = R"(#version 330 core
in vec3 v_pos;
in vec2 v_uv;
flat in float v_layer;
in vec4 v_col;
uniform sampler2DArray u_tex;
uniform vec3 u_clip_center;
uniform float u_clip_half;
uniform int u_linear_out;
out vec4 o_col;
void main() {
    vec2 d = abs(v_pos.xz - u_clip_center.xz);
    if (d.x > u_clip_half || d.y > u_clip_half)
        discard;
    bool keep;
    fog_apply(vec3(0.0), v_pos, false, keep);
    if (!keep)
        discard;
    float a = texture(u_tex, vec3(v_uv, v_layer)).r * v_col.a;
    if (a < 0.003)
        discard;
    vec3 c = v_col.rgb; // linear, as effects.json
    if (u_linear_out == 0)
        c = pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
    o_col = vec4(c, a);
}
)";
        GLuint v = compile_shader(GL_VERTEX_SHADER, vs), f = compile_shader(GL_FRAGMENT_SHADER, with_fog(fs).c_str());
        prog_ = glCreateProgram();
        glAttachShader(prog_, v);
        glAttachShader(prog_, f);
        glLinkProgram(prog_);
        GLint ok = 0;
        glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
        if (!ok) {
            logf("effects shader does not link");
            glDeleteProgram(prog_);
            prog_ = 0;
            return false;
        }
        u_mvp_ = glGetUniformLocation(prog_, "u_mvp");
        u_tex_ = glGetUniformLocation(prog_, "u_tex");
        u_clip_center_ = glGetUniformLocation(prog_, "u_clip_center");
        u_clip_half_ = glGetUniformLocation(prog_, "u_clip_half");
        u_linear_ = glGetUniformLocation(prog_, "u_linear_out");
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, uv));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, layer));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, rgba));
        glBindVertexArray(0);
        // shapes (red channel), as the simulator's kernels: 0 soft, 1-3
        // noisy puffs, 4 chunk; ring for shockwaves is layer 5 (unused by
        // particles), lights use the soft one
        const int n = 64, layers = 6;
        std::vector<uint8_t> px((size_t)n * n * 4 * layers, 255);
        uint32_t seed = 12345;
        auto nrnd = [&]() {
            seed = seed * 1664525u + 1013904223u;
            return (seed >> 8) / 16777216.0f;
        };
        for (int l = 0; l < layers; l++) {
            // coarse noise for the puffs, bilinear
            float g[9][9];
            for (auto &row : g)
                for (float &x : row)
                    x = nrnd();
            for (int y = 0; y < n; y++)
                for (int x = 0; x < n; x++) {
                    float fx = (x + 0.5f) / n * 2 - 1, fy = (y + 0.5f) / n * 2 - 1;
                    float d2 = fx * fx + fy * fy;
                    float k = d2 > 1 ? 0.0f : std::exp(-d2 * 3.2f);
                    if (l >= 1 && l <= 3) {
                        float gx = (x + 0.5f) / n * 8, gy = (y + 0.5f) / n * 8;
                        int ix = (int)gx, iy = (int)gy;
                        float ax = gx - ix, ay = gy - iy;
                        float nv = g[iy][ix] * (1 - ax) * (1 - ay) + g[iy][ix + 1] * ax * (1 - ay) +
                                   g[iy + 1][ix] * (1 - ax) * ay + g[iy + 1][ix + 1] * ax * ay;
                        k = std::clamp(k * (0.45f + 0.75f * nv), 0.0f, 1.0f);
                    } else if (l == 4) {
                        k = d2 < 0.55f ? 1.0f : 0.0f;
                    } else if (l == 5) {
                        float r = std::sqrt(d2);
                        k = std::exp(-((r - 0.86f) * (r - 0.86f)) / (2 * 0.04f * 0.04f));
                    }
                    px[(((size_t)l * n + y) * n + x) * 4] = (uint8_t)(k * 255);
                }
        }
        glGenTextures(1, &tex_);
        glBindTexture(GL_TEXTURE_2D_ARRAY, tex_);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, n, n, layers, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    std::vector<FxDef> defs_;
    std::map<std::string, int> index_;
    std::map<int, Inst> insts_;
    std::vector<Part> parts_;
    std::vector<std::pair<float, const Part *>> sorted_;
    std::vector<Vert> alpha_, add_;
    float time_ = 0;
    int next_handle_ = 0;
    uint32_t rng_ = 0x9E3779B9u;
    GLuint prog_ = 0, vao_ = 0, vbo_ = 0, tex_ = 0;
    GLint u_mvp_ = -1, u_tex_ = -1, u_clip_center_ = -1, u_clip_half_ = -1, u_linear_ = -1;
};
