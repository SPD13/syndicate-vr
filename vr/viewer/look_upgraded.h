// Syndicate Wars VR viewer: the upgraded look: people drawn with the 3D
// models of the assets repository (HANDOVER.md there: sections 2, 3, 10).
//
// The look can be switched at any time (L, or B on the right controller);
// whatever has no model (a person type not modelled, or not loaded yet) is
// drawn classic. Models load on a worker thread the first time their type
// is seen, and stay loaded. Every person gets a state machine choosing the
// clip from the game's animation state (HANDOVER.md 2.2), with crossfades,
// a walk rate following the speed, and the weapon in hand parented to the
// right hand (socket of agent_arsenal.glb), the left hand on two-handed
// weapons by IK. Items, grenades and scenery statics become their models
// (HANDOVER.md 4, 5; sizes from models/scale.json); explosions, fires,
// smoke, gas and impacts become particle effects (fx.h); a shielded person
// gets a glowing shell (HANDOVER.md 10.6), a burnt one an ash pile.
// Nothing here writes to the game.
#pragma once

#include "fx.h"
#include "gltf_model.h"
#include "json_min.h"
#include "people.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

// Loads GLB files on a worker thread; results are taken on the GL thread.
class AssetLoader {
public:
    ~AssetLoader()
    {
        {
            std::lock_guard<std::mutex> lk(m_);
            quit_ = true;
        }
        cv_.notify_all();
        if (th_.joinable())
            th_.join();
    }

    void request(const std::string &key, const std::string &path, bool meshes)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (!th_.joinable())
            th_ = std::thread([this] { run(); });
        jobs_.push_back({key, path, meshes});
        cv_.notify_one();
    }

    // A finished file, or false.
    bool take(std::string &key, std::unique_ptr<GltfModelData> &out)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (done_.empty())
            return false;
        key = done_.front().first;
        out = std::move(done_.front().second);
        done_.pop_front();
        return true;
    }

private:
    struct Job {
        std::string key, path;
        bool meshes;
    };

    void run()
    {
        for (;;) {
            Job j;
            {
                std::unique_lock<std::mutex> lk(m_);
                cv_.wait(lk, [this] { return quit_ || !jobs_.empty(); });
                if (quit_)
                    return;
                j = jobs_.front();
                jobs_.pop_front();
            }
            auto d = std::make_unique<GltfModelData>();
            gltf_load(j.path, *d, j.meshes);
            std::lock_guard<std::mutex> lk(m_);
            done_.emplace_back(j.key, std::move(d));
        }
    }

    std::mutex m_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::deque<std::pair<std::string, std::unique_ptr<GltfModelData>>> done_;
    std::thread th_;
    bool quit_ = false;
};

static const char *kModelVertSrc = "#version 330 core\n" DESK_PROJ_GLSL R"(
layout(location=0) in vec3 a_pos;
layout(location=1) in vec3 a_nrm;
layout(location=2) in vec2 a_uv;
layout(location=3) in uvec4 a_jnt;
layout(location=4) in vec4 a_wgt;
uniform mat4 u_mvp;      // map units -> clip
uniform mat4 u_model;    // model -> map units
uniform int u_skinned;
uniform int u_joint_base;
uniform sampler2D u_joints; // 4 texels (a mat4) per joint, rows of 1024
uniform float u_inflate;    // shield shell: metres along the normals
out vec3 v_map;
out vec3 v_nrm;
out vec2 v_uv;
mat4 joint(uint j) {
    int i = (u_joint_base + int(j)) * 4;
    return mat4(texelFetch(u_joints, ivec2(i % 1024, i / 1024), 0),
                texelFetch(u_joints, ivec2((i + 1) % 1024, (i + 1) / 1024), 0),
                texelFetch(u_joints, ivec2((i + 2) % 1024, (i + 2) / 1024), 0),
                texelFetch(u_joints, ivec2((i + 3) % 1024, (i + 3) / 1024), 0));
}
void main() {
    mat4 skin = mat4(1.0);
    if (u_skinned != 0)
        skin = a_wgt.x * joint(a_jnt.x) + a_wgt.y * joint(a_jnt.y) +
               a_wgt.z * joint(a_jnt.z) + a_wgt.w * joint(a_jnt.w);
    vec3 sn = normalize(mat3(skin) * a_nrm);
    vec4 m = u_model * (skin * vec4(a_pos, 1.0) + vec4(sn * u_inflate, 0.0));
    v_map = m.xyz;
    v_nrm = mat3(u_model) * sn;
    v_uv = a_uv;
    gl_Position = u_desk != 0 ? desk_project(m.xyz) : u_mvp * m;
}
)";

static const char *kModelFragSrc = R"(#version 330 core
in vec3 v_map;
in vec3 v_nrm;
in vec2 v_uv;
uniform sampler2D u_tex;
uniform int u_textured;
uniform vec3 u_light_dir;
uniform vec3 u_clip_center;
uniform float u_clip_half;
uniform int u_linear_out;
uniform float u_bright;
uniform int u_shield;
uniform vec3 u_eye;
uniform float u_time;
out vec4 o_col;
void main() {
    vec2 d = abs(v_map.xz - u_clip_center.xz);
    if (d.x > u_clip_half || d.y > u_clip_half)
        discard;
    if (u_shield != 0) {
        // energy shield (HANDOVER.md 10.6): a fresnel rim, thin moving
        // bands, pulsing; additive
        vec3 n = normalize(v_nrm), v = normalize(u_eye - v_map);
        float rim = pow(1.0 - abs(dot(n, v)), 3.0);
        float band = sin(v_map.y * 0.09 + u_time * 5.0) * sin(v_map.x * 0.07 - u_time * 3.0 + v_map.z * 0.05);
        float lines = smoothstep(0.9, 1.0, 1.0 - abs(band - 0.5) * 2.0);
        float pulse = (2.2 + 1.2 * sin(7.0 * u_time)) / 3.4;
        vec3 c = vec3(0.3, 0.9, 1.0) * (rim * 0.9 + lines * 0.35) * pulse;
        if (u_linear_out != 0)
            c = pow(c, vec3(2.2));
        o_col = vec4(c, 1.0);
        return;
    }
    vec3 col = u_textured != 0 ? texture(u_tex, v_uv).rgb : vec3(0.6);
    // textures are sRGB: light in linear space
    col = pow(col, vec3(2.2));
    vec3 n = normalize(v_nrm);
    float diff = max(abs(dot(n, normalize(u_light_dir))), 0.0);
    col *= (0.45 + 0.75 * diff) * u_bright;
    // the fog works on display colours
    col = pow(max(col, vec3(0.0)), vec3(1.0 / 2.2));
    bool keep;
    col = fog_apply(col, v_map, false, keep);
    if (!keep)
        discard;
    if (u_linear_out != 0)
        col = pow(col, vec3(2.2));
    o_col = vec4(col, 1.0);
}
)";

class UpgradedLook {
public:
    bool enabled = false;

    /** Folder of the assets repository (absolute). */
    void set_assets(const std::string &dir) { dir_ = dir; }
    const std::string &assets() const { return dir_; }

    /** Map height of the ground at a point (for effects). */
    std::function<float(float, float)> ground;

    // What becomes of an effect sprite: 0 drawn, 1 hidden (particles
    // replace it), 2 a model (a prop whose model is loaded).
    int effect_mode(const SwvEffect &e) const
    {
        if (!enabled)
            return 0;
        if (e.kind == SwvEK_SMOKE || e.kind == SwvEK_FLAME)
            return fx_.loaded() ? 1 : 0;
        // scale effects: only those of the animations particles replace
        if (e.kind == SwvEK_SCALE_EFFECT)
            return (fx_.loaded() && fx_frames_.count(e.frame)) ? 1 : 0;
        if (e.kind == SwvEK_STATIC || e.kind == SwvEK_DROPPED_ITEM || e.kind == SwvEK_GRENADE) {
            auto it = props_.find(e.id);
            if (it == props_.end())
                return 0;
            PropModel pm;
            return prop_model(it->second, pm) && models_.count(pm.key) ? 2 : 0;
        }
        return 0;
    }

    // Whether the person is drawn as a model now.
    bool draws(uint16_t thing) const
    {
        if (!enabled)
            return false;
        auto it = persons_.find(thing);
        return it != persons_.end() && model_for(it->second.subtype) != nullptr;
    }

    /** Takes loaded files; reads the persons of a new turn. */
    void update(const WorldSnapshot &snap, double now)
    {
        std::string key;
        std::unique_ptr<GltfModelData> d;
        while (loader_.take(key, d)) {
            if (!d->ok) {
                logf("upgraded look: %s", d->error.c_str());
                continue;
            }
            if (key == "sockets") {
                sockets_ = std::move(*d);
                continue;
            }
            auto m = std::make_unique<GltfModel>();
            m->data = std::move(*d);
            m->upload();
            logf("upgraded look: %s loaded (%zu clips)", key.c_str(), m->data.clips.size());
            models_[key] = std::move(m);
        }
        if (!enabled || !snap.valid)
            return;
        if (snap.hdr.gameturn != turn_) {
            double dt = now - turn_time_;
            if (dt > 0.01 && dt < 0.5)
                turn_dt_ = turn_dt_ * 0.8 + dt * 0.2;
            turn_time_ = now;
            turn_ = snap.hdr.gameturn;
            std::unordered_map<uint16_t, Person> next;
            for (const SwvPersonRender &pr : snap.persons) {
                Person p;
                auto it = persons_.find(pr.thing);
                if (it != persons_.end()) {
                    p = it->second;
                    glm::vec2 d(pr.x - p.last.x, pr.z - p.last.z);
                    p.speed = glm::length(d); // map units per turn
                }
                p.last = glm::vec3(pr.x, pr.y, pr.z);
                p.subtype = pr.subtype;
                p.anim_mode = pr.anim_mode;
                p.weapon = pr.weapon;
                p.flags = pr.flags;
                next[pr.thing] = p;
                request_model(pr.subtype);
                if (pr.weapon != 0)
                    request_weapon(pr.weapon);
            }
            persons_.swap(next);
            // props of this turn; their models are loaded when first seen
            std::unordered_map<uint32_t, SwvProp> props;
            for (const SwvProp &pr : snap.props) {
                props[pr.id] = pr;
                PropModel pm;
                if (prop_model(pr, pm))
                    request_file(pm.key, pm.path);
            }
            props_.swap(props);
            if (fx_frames_.empty() || snap.sprite_changed)
                collect_fx_frames(snap);
            start_effects(snap);
        }
    }

    /** Poses the people placed by People::build, places the props, moves
     * the effects; for draw() and draw_effects(). */
    void prepare(double now, const std::vector<People::ModelPlace> &places,
                 const std::vector<People::PropPlace> &prop_places)
    {
        draws_.clear();
        prop_draws_.clear();
        joints_.clear();
        float dt = (float)std::clamp(now - last_prepare_, 0.0, 0.1);
        last_prepare_ = now;
        time_ += dt;
        if (!enabled) {
            if (fx_.loaded())
                fx_.clear(), effect_handles_.clear();
            return;
        }
        place_props(prop_places, now);
        follow_effects(places);
        fx_.update(dt);
        for (const People::ModelPlace &pl : places) {
            auto it = persons_.find(pl.thing);
            if (it == persons_.end())
                continue;
            Person &ps = it->second;
            const CharType *ct = char_type(ps.subtype);
            const GltfModel *m = model_for(ps.subtype);
            if (ct == nullptr || m == nullptr)
                continue;
            animate(ps, *m, dt);
            // burnt: once the burning clip has played, an ash pile instead
            if (ps.anim_mode == 18 && ps.clip >= 0 && ps.time >= m->data.clips[ps.clip].duration) {
                auto ash = models_.find("models/items/ashpile.glb");
                if (ash != models_.end()) {
                    PropDraw pd;
                    pd.model = ash->second.get();
                    pd.xform = prop_xform(*pd.model, pl.p, 0.9f, 2, (uint32_t)pl.thing);
                    prop_draws_.push_back(pd);
                } else {
                    request_file("models/items/ashpile.glb", dir_ + "/models/items/ashpile.glb");
                }
                continue;
            }
            // pose: the current clip, faded in over the previous one
            m->rest_pose(pose_a_);
            m->sample(ps.prev_clip, ps.prev_time, pose_a_);
            if (ps.fade < 1.0f) {
                m->rest_pose(pose_b_);
                m->sample(ps.clip, ps.time, pose_b_);
                gltf_blend(pose_a_, pose_b_, smooth(ps.fade));
            } else {
                m->sample(ps.clip, ps.time, pose_a_);
            }
            m->globals(pose_a_, glob_);
            Draw dr;
            dr.model = m;
            // the weapon in the right hand: its model and socket
            const WeaponDef *wd = weapon_def(ps.weapon);
            const GltfModel *wmodel = nullptr;
            glm::mat4 socket(1.0f);
            int hand = m->node("mixamorig:RightHand");
            if (wd != nullptr && hand >= 0 && socket_for(wd->name, socket)) {
                auto wm = models_.find(std::string("weapon:") + wd->name);
                if (wm != models_.end())
                    wmodel = wm->second.get();
            }
            // two-handed weapons in rifle clips: the left hand on the
            // weapon (lhand_ node), blended in and out over 0.1 s
            int lhand = (wmodel && wd->two_hands) ? wmodel->node(std::string("lhand_") + wd->name) : -1;
            bool want_ik = lhand >= 0 && ps.clip_name.rfind("rifle_", 0) == 0;
            ps.ik = std::clamp(ps.ik + (want_ik ? dt : -dt) / 0.1f, 0.0f, 1.0f);
            if (lhand >= 0 && ps.ik > 0) {
                GltfPose wp;
                wmodel->rest_pose(wp);
                wmodel->globals(wp, wglob_);
                left_hand_ik(*m, glob_[hand] * socket * wglob_[lhand], smooth(ps.ik));
            }
            dr.joint_base = (int)joints_.size();
            for (size_t j = 0; j < m->data.joints.size(); j++)
                joints_.push_back(glob_[m->data.joints[j]] * m->data.inv_bind[j]);
            // map units; map space is mirrored in z. A yaw of phi - pi
            // turns the models to their walking direction (checked in the
            // viewer: their rest facing is -X here, not +X as HANDOVER.md 1)
            float k = mk_metre() * ct->scale;
            dr.xform = glm::translate(glm::mat4(1.0f), pl.p) *
                       glm::rotate(glm::mat4(1.0f), pl.phi - 3.1415927f, glm::vec3(0, 1, 0)) *
                       glm::scale(glm::mat4(1.0f), glm::vec3(k, k, -k));
            dr.bright = (ps.flags & SwvPF_IN_BUILDING) ? 0.5f : 1.0f;
            dr.shield = (ps.flags & SwvPF_SHIELD) != 0;
            dr.nodes = glob_;
            if (wmodel != nullptr) {
                dr.weapon = wmodel;
                dr.weapon_xform = dr.xform * glob_[hand] * socket;
            }
            draws_.push_back(std::move(dr));
        }
    }

    /** Draws the particle effects (after the opaque world). */
    void draw_effects(const glm::mat4 &mvp_map, glm::vec3 eye_map, glm::vec3 clip_center, float clip_half,
                      bool linear_out)
    {
        if (enabled)
            fx_.draw(mvp_map, eye_map, clip_center, clip_half, linear_out);
    }

    /** Where the viewer is, in map units (shield rims face it). */
    glm::vec3 eye_map{0.0f};

    /** Draws the posed people and the props (inside Renderer::draw, map
     * space). */
    void draw(const glm::mat4 &mvp_map, glm::vec3 clip_center, float clip_half, bool linear_out)
    {
        if (draws_.empty() && prop_draws_.empty())
            return;
        if (prog_ == 0 && !init_gl())
            return;
        glUseProgram(prog_);
        // joint matrices: 4 RGBA32F texels each, rows of 1024 texels
        int texels = (int)joints_.size() * 4;
        int rows = std::max(1, (texels + 1023) / 1024);
        joint_tex_data_.assign((size_t)rows * 1024 * 4, 0.0f);
        for (size_t j = 0; j < joints_.size(); j++)
            memcpy(&joint_tex_data_[j * 16], glm::value_ptr(joints_[j]), 64);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, joint_tex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1024, rows, 0, GL_RGBA, GL_FLOAT, joint_tex_data_.data());
        glUniform1i(u_joints_, 3);
        glUniform1i(u_tex_, 2);
        glUniformMatrix4fv(u_mvp_, 1, GL_FALSE, glm::value_ptr(mvp_map));
        glUniform3f(u_light_, 0.4f, 1.0f, 0.3f);
        glUniform3f(u_clip_center_, clip_center.x, clip_center.y, clip_center.z);
        glUniform1f(u_clip_half_, clip_half);
        glUniform1i(u_linear_, linear_out ? 1 : 0);
        glUniform1f(u_inflate_, 0.0f);
        glUniform1i(u_shield_, 0);
        glUniform3f(u_eye_, eye_map.x, eye_map.y, eye_map.z);
        glUniform1f(u_time_, (float)time_);
        // people here are only those the fog lets show; props stay grey
        // where seen before
        g_fog.bind(prog_);
        g_desk.bind(prog_);
        g_fog.kind(prog_, FogK_THINGS);
        glActiveTexture(GL_TEXTURE2);
        glUniform1f(u_bright_, 1.0f);
        glUniform1i(u_joint_base_, 0);
        for (const PropDraw &pd : prop_draws_) {
            const GltfModel &m = *pd.model;
            for (size_t n = 0; n < m.data.nodes.size(); n++)
                if (m.data.nodes[n].mesh >= 0)
                    for (const GltfModel::Prim &p : m.meshes[m.data.nodes[n].mesh])
                        if (!p.skinned)
                            draw_prim(p, pd.xform * m.rest[n]);
        }
        for (const Draw &dr : draws_) {
            glUniform1f(u_bright_, dr.bright);
            glUniform1i(u_joint_base_, dr.joint_base);
            const GltfModel &m = *dr.model;
            for (size_t n = 0; n < m.data.nodes.size(); n++) {
                const GltfNode &nd = m.data.nodes[n];
                if (nd.mesh < 0 || nd.name.rfind("weapon_", 0) == 0)
                    continue;
                for (const GltfModel::Prim &p : m.meshes[nd.mesh]) {
                    glm::mat4 xf = p.skinned ? dr.xform : dr.xform * dr.nodes[n];
                    draw_prim(p, xf);
                }
            }
            if (dr.weapon != nullptr) {
                const GltfModel &w = *dr.weapon;
                std::vector<glm::mat4> wg;
                GltfPose wp;
                w.rest_pose(wp);
                w.globals(wp, wg);
                for (size_t n = 0; n < w.data.nodes.size(); n++)
                    if (w.data.nodes[n].mesh >= 0)
                        for (const GltfModel::Prim &p : w.meshes[w.data.nodes[n].mesh])
                            draw_prim(p, dr.weapon_xform * wg[n]);
            }
        }
        // shield shells: the body again, pushed out 6 cm, additive
        bool any_shield = false;
        for (const Draw &dr : draws_)
            any_shield |= dr.shield;
        if (any_shield) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            glUniform1i(u_shield_, 1);
            glUniform1f(u_inflate_, 0.06f);
            for (const Draw &dr : draws_) {
                if (!dr.shield)
                    continue;
                glUniform1i(u_joint_base_, dr.joint_base);
                const GltfModel &m = *dr.model;
                for (size_t n = 0; n < m.data.nodes.size(); n++) {
                    const GltfNode &nd = m.data.nodes[n];
                    if (nd.mesh < 0 || nd.name.rfind("weapon_", 0) == 0)
                        continue;
                    for (const GltfModel::Prim &p : m.meshes[nd.mesh])
                        if (p.skinned)
                            draw_prim(p, dr.xform);
                }
            }
            glUniform1i(u_shield_, 0);
            glUniform1f(u_inflate_, 0.0f);
            glDepthMask(GL_TRUE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
    // ---- props: items, grenades, scenery (HANDOVER.md 4, 5, 6)
    struct PropModel {
        std::string key, path;
        float size = 0;   // metres; 0: the model is in metres already
        int size_axis = 0; // 0 height, 1 longest side
    };
    struct PropDraw {
        const GltfModel *model = nullptr;
        glm::mat4 xform{1.0f};
    };

    // scenery statics by their animation (statics.json, scale.json)
    static const char *scenery_for(int anim)
    {
        static const std::pair<int, const char *> map[] = {
            {1002, "tree_conifer"},   {1035, "tree_blossom"},   {1048, "tree_willow"},  {1036, "tree_round"},
            {1086, "bush"},           {1050, "tree_planter"},   {1065, "tree_dead"},    {1063, "tree_dead_planter"},
            {1010, "tree_red"},       {1039, "lamp_large"},     {1000, "lamp_post"},    {1051, "lamp_post"},
            {1009, "lamp_post_red"},  {1033, "lamp_post_red"},  {1038, "lamp_post_globe"}, {1005, "neon_pole"},
            {1001, "bin_grey"},       {1027, "bin_white"},      {1028, "barrel_yellow"}, {1026, "barrel_blue"},
            {1055, "barrel_burning"}, {1053, "planter_hazard"}, {1030, "fountain_base"}, {1037, "bowl_chrome"},
        };
        for (const auto &e : map)
            if (e.first == anim)
                return e.second;
        return nullptr;
    }

    // items by weapon type (HANDOVER.md 4)
    static const char *item_for(int wtype)
    {
        static const std::pair<int, const char *> map[] = {
            {0x06, "nuclgren"},  {0x0A, "crazygas"},  {0x0B, "kogas"},    {0x0C, "elemine"},
            {0x0D, "explmine"},  {0x0E, "napalmmine"}, {0x10, "airstrike"}, {0x12, "razorwire"},
            {0x17, "energyshld"}, {0x1A, "cerebusiff"}, {0x1B, "medi1"},   {0x1C, "medi2"},
            {0x1D, "explwire"},  {0x1E, "cloneshld"},
        };
        for (const auto &e : map)
            if (e.first == wtype)
                return e.second;
        return nullptr;
    }

    void load_scales()
    {
        if (scales_loaded_)
            return;
        scales_loaded_ = true;
        if (!JsonParser::parse_file(dir_ + "/models/scale.json", scales_))
            logf("upgraded look: cannot read models/scale.json");
    }

    // size of a raw model from scale.json: height, or its longest side
    void raw_size(const std::string &rel, PropModel &pm) const
    {
        pm.size = 1.0f;
        pm.size_axis = 1;
        const Json *e = scales_.get(rel);
        if (e == nullptr)
            return;
        if (e->has("height_m"))
            pm.size = (float)e->n("height_m"), pm.size_axis = 0;
        else if (e->has("length_m"))
            pm.size = (float)e->n("length_m");
        else if (e->has("width_m"))
            pm.size = (float)e->n("width_m");
    }

    bool prop_model(const SwvProp &pr, PropModel &pm) const
    {
        const_cast<UpgradedLook *>(this)->load_scales();
        std::string rel;
        if (pr.kind == SwvEK_STATIC) {
            const char *n = scenery_for(pr.anim);
            if (n == nullptr)
                return false;
            rel = std::string("scenery/") + n + ".glb";
        } else {
            const WeaponDef *wd = weapon_def(pr.weapon);
            if (wd != nullptr && pr.kind == SwvEK_DROPPED_ITEM) {
                pm.key = std::string("weapon:") + wd->name;
                pm.path = dir_ + "/models/weapons/held/" + wd->name + ".glb";
                pm.size = 0;
                return true;
            }
            const char *n = item_for(pr.weapon);
            if (n == nullptr)
                return false;
            rel = std::string("items/") + n + ".glb";
        }
        pm.key = "models/" + rel;
        pm.path = dir_ + "/" + pm.key;
        raw_size(rel, pm);
        return true;
    }

    // Model -> map units: scaled to its size, standing on p, turned by a
    // yaw from its id (props have no facing in the game).
    glm::mat4 prop_xform(const GltfModel &m, glm::vec3 p, float size, int axis, uint32_t id) const
    {
        glm::vec3 ext = m.bmax - m.bmin;
        float k = 1.0f;
        if (size > 0) {
            float ref = axis == 0 ? ext.y : std::max(ext.x, std::max(ext.y, ext.z));
            k = size / std::max(ref, 1e-4f);
        }
        float yaw = (float)((id * 2654435761u) >> 20) / 4096.0f * 6.2831853f;
        glm::vec3 base((m.bmin.x + m.bmax.x) * 0.5f, m.bmin.y, (m.bmin.z + m.bmax.z) * 0.5f);
        float s = mk_metre() * k;
        return glm::translate(glm::mat4(1.0f), p) * glm::rotate(glm::mat4(1.0f), yaw, glm::vec3(0, 1, 0)) *
               glm::scale(glm::mat4(1.0f), glm::vec3(s, s, -s)) * glm::translate(glm::mat4(1.0f), -base);
    }

    void place_props(const std::vector<People::PropPlace> &places, double now)
    {
        for (const People::PropPlace &pp : places) {
            auto it = props_.find(pp.id);
            if (it == props_.end())
                continue;
            PropModel pm;
            if (!prop_model(it->second, pm))
                continue;
            auto mi = models_.find(pm.key);
            if (mi == models_.end())
                continue;
            PropDraw pd;
            pd.model = mi->second.get();
            glm::vec3 p = pp.p;
            float size = pm.size;
            int axis = pm.size_axis;
            uint32_t id = pp.id;
            if (pm.size == 0) // a held weapon lying on the ground
                size = 0;
            pd.xform = prop_xform(*pd.model, p, size, axis, id);
            if (it->second.kind == SwvEK_GRENADE && pm.size > 0) {
                // grenades in flight tumble
                float spin = (float)now * 9.0f;
                pd.xform = pd.xform * glm::rotate(glm::mat4(1.0f), spin, glm::vec3(1, 0, 0));
            }
            prop_draws_.push_back(pd);
        }
    }

    // ---- effects (HANDOVER.md 7.1, 10.5)
    void start_effects(const WorldSnapshot &snap)
    {
        if (!fx_.loaded()) {
            if (fx_tried_)
                return;
            fx_tried_ = true;
            if (!fx_.load(dir_ + "/effects/effects.json"))
                return;
        }
        auto gy = [&](float x, float z) { return ground ? ground(x, z) : 0.0f; };
        for (const SwvFxEvent &ev : snap.fx_events) {
            glm::vec3 p(ev.x, 0, ev.z);
            float g = gy(ev.x, ev.z);
            if (ev.y > g + 150) // on a roof or in the air
                g = ev.y;
            switch (ev.kind) {
            case SwvFX_BANG:
                fx_.start(ev.size >= 3 ? "explosion_large" : "explosion_small", p, g);
                break;
            case SwvFX_FIRE_START:
                effect_handles_[0x10000000u | ev.id] = fx_.start("fire_ground", p, g);
                break;
            case SwvFX_FIRE_END: {
                auto it = effect_handles_.find(0x10000000u | ev.id);
                if (it != effect_handles_.end()) {
                    fx_.stop(it->second);
                    effect_handles_.erase(it);
                }
                break;
            }
            case SwvFX_SMOKE:
                fx_.start("smoke_plume", p, g);
                break;
            case SwvFX_SCALE_EFFECT: {
                const char *name = nullptr;
                float scale = 1.75f; // impacts are tiny on a table (HANDOVER.md 7.1)
                // the event gives StartFrame, the animation - 1
                int anim = ev.anim + 1;
                switch (anim) {
                case 900: name = "explosion_small", scale = 1.0f; break;
                case 907: name = "smoke_plume", scale = 1.0f; break;
                case 908: name = "smoke_dark", scale = 1.0f; break;
                case 920: name = "gas_cloud", scale = 1.0f; break;
                case 1087: name = "impact_water"; break;
                case 1091: name = "impact_sludge"; break;
                case 1090: name = "impact_dust"; break;
                case 1104: name = "sparkle"; break;
                case 1101: case 1103: name = "smoke_dark", scale = 0.5f; break; // rocket smoke
                default: break;
                }
                if (name == nullptr)
                    break;
                int h = fx_.start(name, p, g, scale);
                // smoke puffs are short in the game: let them die out
                if (anim == 907 || anim == 908 || anim == 1101 || anim == 1103)
                    stop_later_.push_back({h, time_ + (anim > 1000 ? 0.6 : 2.5)});
                break;
            }
            default:
                break;
            }
        }
        for (auto it = stop_later_.begin(); it != stop_later_.end();) {
            if (time_ >= it->second) {
                fx_.stop(it->first);
                it = stop_later_.erase(it);
            } else {
                ++it;
            }
        }
        // barrels on fire (scenery S30) burn while they are there
        std::set<uint32_t> barrels;
        for (const auto &kv : props_)
            if (kv.second.kind == SwvEK_STATIC && kv.second.anim == 1055) {
                uint32_t key = 0x20000000u | (kv.first & 0xFFFFFF);
                barrels.insert(key);
                if (!effect_handles_.count(key))
                    effect_handles_[key] = fx_.start("barrel_fire", glm::vec3(kv.second.x, 0, kv.second.z),
                                                     gy(kv.second.x, kv.second.z));
            }
        // people on fire
        for (const auto &kv : persons_) {
            uint32_t key = 0x30000000u | kv.first;
            bool burning = (kv.second.flags & SwvPF_BURNING) != 0 && kv.second.anim_mode != 18;
            auto it = effect_handles_.find(key);
            if (burning && it == effect_handles_.end())
                effect_handles_[key] = fx_.start("burning_person", kv.second.last, gy(kv.second.last.x, kv.second.last.z));
            else if (!burning && it != effect_handles_.end()) {
                fx_.stop(it->second);
                effect_handles_.erase(it);
            }
        }
        for (auto it = effect_handles_.begin(); it != effect_handles_.end();) {
            bool barrel = (it->first & 0xF0000000u) == 0x20000000u;
            if ((barrel && !barrels.count(it->first)) || !fx_.alive(it->second)) {
                fx_.stop(it->second);
                it = effect_handles_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // the sprite frames of the animations that particles replace
    void collect_fx_frames(const WorldSnapshot &snap)
    {
        static const int anims[] = {900, 907, 908, 920, 1087, 1090, 1091, 1101, 1103, 1104};
        fx_frames_.clear();
        for (int a : anims) {
            if (a >= (int)snap.nstart.size())
                continue;
            int f0 = snap.nstart[a], f = f0;
            for (int guard = 0; f > 0 && f < (int)snap.frames.size() && guard < 128; guard++) {
                fx_frames_.insert((uint16_t)f);
                f = snap.frames[f].next;
                if (f == f0)
                    break;
            }
        }
    }

    // burning people's flames follow them
    void follow_effects(const std::vector<People::ModelPlace> &places)
    {
        for (const People::ModelPlace &pl : places) {
            auto it = effect_handles_.find(0x30000000u | pl.thing);
            if (it != effect_handles_.end())
                fx_.move(it->second, pl.p, ground ? ground(pl.p.x, pl.p.z) : pl.p.y);
        }
    }

    void request_file(const std::string &key, const std::string &path)
    {
        if (requested_.count(key))
            return;
        requested_.insert(key);
        loader_.request(key, path, true);
    }

    struct CharType {
        int subtype;
        const char *file;
        float scale;
    };
    struct WeaponDef {
        int type;
        const char *name;
        bool two_hands;
    };
    struct Person {
        glm::vec3 last{0};
        float speed = 0;
        int subtype = 0, anim_mode = 0, weapon = 0, flags = 0;
        int clip = -1, prev_clip = -1;
        float time = 0, prev_time = 0, fade = 1.0f;
        bool loop = true, hold = false;
        std::string clip_name;
        float ik = 0; // weight of the left hand IK
    };
    struct Draw {
        const GltfModel *model = nullptr;
        const GltfModel *weapon = nullptr;
        glm::mat4 xform{1.0f}, weapon_xform{1.0f};
        int joint_base = 0;
        float bright = 1.0f;
        bool shield = false;
        std::vector<glm::mat4> nodes;
    };

    // HANDOVER.md section 2: person type -> model
    static const CharType *char_type(int subtype)
    {
        static const CharType types[] = {
            {1, "models/agent/agent_armed.glb", 1.0f},
            {2, "models/zealot/zealot3_cape_armed.glb", 1.0f},
            {3, "models/punk_f/punk_f_armed.glb", 1.0f},
            {4, "models/briefcase_m/briefcase_m_armed.glb", 1.0f},
            {5, "models/white_brun_f/white_brun_f_armed.glb", 1.0f},
            {6, "models/mercenary/mercenary_armed.glb", 1.0f},
            {8, "models/police/police_armed.glb", 1.0f},
            {9, "models/punk_m/punk_m_armed.glb", 1.0f},
            {10, "models/scientist/scientist_armed.glb", 1.0f},
            {11, "models/shady_m/shady_m_armed.glb", 1.0f},
            {12, "models/high_priest/high_priest_cape.glb", 1.8f},
            {13, "models/whit_blond_f/whit_blond_f_armed.glb", 1.0f},
            {14, "models/leth_jackt_m/leth_jackt_m_armed.glb", 1.0f},
            {15, "models/whit_blond_f/whit_blond_f_armed.glb", 1.0f},
        };
        for (const CharType &c : types)
            if (c.subtype == subtype)
                return &c;
        return nullptr;
    }

    // HANDOVER.md section 3: weapon type -> held model
    static const WeaponDef *weapon_def(int type)
    {
        static const WeaponDef defs[] = {
            {0x01, "uzi", false},        {0x02, "minigun", true},     {0x03, "laser", true},
            {0x04, "ellaser", true},     {0x05, "rap", true},         {0x07, "persuadrtrn", false},
            {0x08, "flamer", true},      {0x09, "h2htaser", false},   {0x0F, "longrange", true},
            {0x11, "beam", true},        {0x13, "sonicblast", true},  {0x14, "qdevastator", true},
            {0x15, "persuader2", false}, {0x16, "stasisfld", false},  {0x18, "soulgun", true},
            {0x19, "timegun", false},
        };
        for (const WeaponDef &w : defs)
            if (w.type == type)
                return &w;
        return nullptr;
    }

    static float mk_metre() { return 110.0f; } // map units per metre, as hud_vr.h
    static float smooth(float x) { return x * x * (3 - 2 * x); }

    const GltfModel *model_for(int subtype) const
    {
        const CharType *ct = char_type(subtype);
        if (ct == nullptr)
            return nullptr;
        auto it = models_.find(ct->file);
        return it == models_.end() ? nullptr : it->second.get();
    }

    void request_model(int subtype)
    {
        const CharType *ct = char_type(subtype);
        if (ct == nullptr || requested_.count(ct->file))
            return;
        requested_.insert(ct->file);
        loader_.request(ct->file, dir_ + "/" + ct->file, true);
        if (!requested_.count("sockets")) {
            requested_.insert("sockets");
            // the hand sockets of every weapon: the node tree only
            loader_.request("sockets", dir_ + "/models/agent/agent_arsenal.glb", false);
        }
    }

    void request_weapon(int type)
    {
        const WeaponDef *wd = weapon_def(type);
        if (wd == nullptr)
            return;
        std::string key = std::string("weapon:") + wd->name;
        if (requested_.count(key))
            return;
        requested_.insert(key);
        loader_.request(key, dir_ + "/models/weapons/held/" + wd->name + ".glb", true);
    }

    bool socket_for(const char *name, glm::mat4 &out) const
    {
        std::string want = std::string("weapon_") + name;
        for (const GltfNode &n : sockets_.nodes)
            if (n.name == want) {
                out = glm::translate(glm::mat4(1.0f), n.t) * glm::mat4_cast(n.r) * glm::scale(glm::mat4(1.0f), n.s);
                return true;
            }
        return false;
    }

    // The clip for the game's animation state (HANDOVER.md 2.2).
    void choose(const Person &ps, std::string &name, bool &loop, bool &hold) const
    {
        float mps = ps.speed / mk_metre() / (float)std::max(turn_dt_, 0.02);
        bool moving = mps > 0.3f, running = mps > 2.6f;
        const WeaponDef *wd = weapon_def(ps.weapon);
        bool heavy = wd != nullptr && wd->two_hands;
        bool armed = wd != nullptr;
        std::string gait = running ? "run" : "walk";
        loop = true;
        hold = false;
        switch (ps.anim_mode) {
        case 1: case 6: case 8:
            name = armed ? (moving ? "pistol_" + gait : "pistol_idle") : (moving ? gait : "idle");
            break;
        case 2: case 7: case 9:
            name = armed ? (moving ? "rifle_" + gait : "rifle_idle") : (moving ? gait : "idle");
            break;
        case 4: case 5:
            name = heavy ? "rifle_crouch" : "pistol_kneel";
            break;
        case 10: case 17: case 18:
            name = "burning";
            loop = false;
            hold = true;
            break;
        case 11:
            name = "death_front";
            loop = false;
            hold = true;
            break;
        case 12: case 19:
            name = "death_back";
            loop = false;
            hold = true;
            break;
        case 13:
            name = "pickup";
            loop = false;
            break;
        case 14:
            name = "pistol_shoot";
            break;
        case 15:
            name = "rifle_shoot";
            break;
        case 16:
            name = "push_back";
            loop = false;
            break;
        case 20: // lying dead: keep the death that played, else lie down
            if (ps.clip_name.rfind("death", 0) == 0 || ps.clip_name == "burning" || ps.clip_name == "fall") {
                name = ps.clip_name;
            } else {
                name = "death_front";
            }
            loop = false;
            hold = true;
            break;
        default:
            name = moving ? (armed ? (heavy ? "rifle_" : "pistol_") + gait : gait) : (armed ? (heavy ? "rifle_idle" : "pistol_idle") : "idle");
            break;
        }
    }

    // A clip of the model for the wanted name, or the nearest it has.
    static int find_clip(const GltfModel &m, const std::string &name)
    {
        int c = m.clip(name);
        if (c >= 0)
            return c;
        // models with few clips (High Priest: idle, walk, run, hurt, fall)
        std::string base = name;
        for (const char *pre : {"pistol_", "rifle_"})
            if (base.rfind(pre, 0) == 0)
                base = base.substr(strlen(pre));
        if ((c = m.clip(base)) >= 0)
            return c;
        if (name.rfind("death", 0) == 0 || name == "burning")
            if ((c = m.clip("fall")) >= 0)
                return c;
        if (name == "hit" || name == "push_back")
            if ((c = m.clip("hurt")) >= 0)
                return c;
        if (base == "shoot" || base == "aim" || base == "kneel" || base == "crouch")
            if ((c = m.clip("idle")) >= 0)
                return c;
        return m.clip("idle");
    }

    void animate(Person &ps, const GltfModel &m, float dt)
    {
        std::string name;
        bool loop, hold;
        choose(ps, name, loop, hold);
        int c = find_clip(m, name);
        if (c != ps.clip || ps.clip < 0) {
            ps.prev_clip = ps.clip >= 0 ? ps.clip : c;
            ps.prev_time = ps.time;
            ps.clip = c;
            ps.time = 0;
            ps.fade = ps.clip_name.empty() ? 1.0f : 0.0f;
            ps.clip_name = name;
        }
        ps.loop = loop;
        ps.hold = hold;
        // walk and run play at the pace of the movement
        float rate = 1.0f;
        float mps = ps.speed / mk_metre() / (float)std::max(turn_dt_, 0.02);
        if (name.find("walk") != std::string::npos)
            rate = std::clamp(mps / 1.4f, 0.5f, 2.0f);
        else if (name.find("run") != std::string::npos)
            rate = std::clamp(mps / 3.6f, 0.6f, 2.0f);
        if (c >= 0) {
            float dur = std::max(m.data.clips[c].duration, 0.01f);
            ps.time += dt * rate;
            ps.time = ps.loop ? std::fmod(ps.time, dur) : std::min(ps.time, dur);
        }
        if (ps.prev_clip >= 0) {
            float pd = std::max(m.data.clips[ps.prev_clip].duration, 0.01f);
            ps.prev_time = std::fmod(ps.prev_time + dt, pd);
        }
        ps.fade = std::min(1.0f, ps.fade + dt / 0.2f);
        if (ps.fade >= 1.0f)
            ps.prev_clip = ps.clip, ps.prev_time = ps.time;
    }

    static glm::vec3 pos_of(const glm::mat4 &m) { return glm::vec3(m[3]); }

    static glm::quat rot_of(const glm::mat4 &m)
    {
        glm::mat3 r(glm::normalize(glm::vec3(m[0])), glm::normalize(glm::vec3(m[1])),
                    glm::normalize(glm::vec3(m[2])));
        return glm::normalize(glm::quat_cast(r));
    }

    // Shortest rotation turning direction u into v.
    static glm::quat arc(glm::vec3 u, glm::vec3 v)
    {
        u = glm::normalize(u);
        v = glm::normalize(v);
        float d = glm::dot(u, v);
        if (d < -0.9999f) {
            glm::vec3 ax = glm::cross(glm::vec3(1, 0, 0), u);
            if (glm::length(ax) < 1e-4f)
                ax = glm::cross(glm::vec3(0, 1, 0), u);
            return glm::angleAxis(3.1415927f, glm::normalize(ax));
        }
        glm::vec3 c = glm::cross(u, v);
        return glm::normalize(glm::quat(1.0f + d, c.x, c.y, c.z));
    }

    // Turns node n (in pose_a_, globals in glob_) so that its world
    // direction `from` becomes `to`.
    void turn_node(const GltfModel &m, int n, glm::vec3 from, glm::vec3 to)
    {
        if (glm::length(from) < 1e-6f || glm::length(to) < 1e-6f)
            return;
        int par = m.data.nodes[n].parent;
        glm::quat pw = par >= 0 ? rot_of(glob_[par]) : glm::quat(1, 0, 0, 0);
        pose_a_.r[n] = glm::normalize(glm::inverse(pw) * arc(from, to) * rot_of(glob_[n]));
    }

    // Two-bone IK of the left arm (HANDOVER.md 3, 10.3): the wrist goes to
    // the target's position, the elbow bends in the plane of its animated
    // place pushed a little down, then the hand takes the target's
    // orientation. w blends it with the clip. Model space, pose_a_/glob_.
    void left_hand_ik(const GltfModel &m, const glm::mat4 &target, float w)
    {
        int a = m.node("mixamorig:LeftArm"), b = m.node("mixamorig:LeftForeArm"), c = m.node("mixamorig:LeftHand");
        if (a < 0 || b < 0 || c < 0 || w <= 0)
            return;
        glm::quat ra = pose_a_.r[a], rb = pose_a_.r[b], rc = pose_a_.r[c];
        glm::vec3 pa = pos_of(glob_[a]), pb = pos_of(glob_[b]), pc = pos_of(glob_[c]);
        glm::vec3 t = pos_of(target);
        float l1 = glm::distance(pa, pb), l2 = glm::distance(pb, pc);
        float d = glm::distance(pa, t);
        if (d < 1e-4f || l1 < 1e-4f || l2 < 1e-4f)
            return;
        glm::vec3 x = (t - pa) / d;
        d = std::clamp(d, std::fabs(l1 - l2) + 1e-3f, l1 + l2 - 1e-3f);
        glm::vec3 pole = pb - glm::vec3(0, 0.1f, 0) - pa;
        glm::vec3 y = pole - x * glm::dot(pole, x);
        if (glm::length(y) < 1e-5f)
            y = glm::vec3(0, -1, 0) - x * x.y;
        y = glm::normalize(y);
        float cos_a = std::clamp((l1 * l1 + d * d - l2 * l2) / (2 * l1 * d), -1.0f, 1.0f);
        glm::vec3 elbow = pa + x * (l1 * cos_a) + y * (l1 * std::sqrt(1 - cos_a * cos_a));
        turn_node(m, a, pb - pa, elbow - pa);
        m.globals(pose_a_, glob_);
        turn_node(m, b, pos_of(glob_[c]) - pos_of(glob_[b]), pa + x * d - pos_of(glob_[b]));
        m.globals(pose_a_, glob_);
        pose_a_.r[c] = glm::normalize(glm::inverse(rot_of(glob_[b])) * rot_of(target));
        if (w < 1.0f) {
            pose_a_.r[a] = glm::slerp(ra, pose_a_.r[a], w);
            pose_a_.r[b] = glm::slerp(rb, pose_a_.r[b], w);
            pose_a_.r[c] = glm::slerp(rc, pose_a_.r[c], w);
        }
        m.globals(pose_a_, glob_);
    }

    bool init_gl()
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, kModelVertSrc);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, with_fog(kModelFragSrc).c_str());
        prog_ = glCreateProgram();
        glAttachShader(prog_, vs);
        glAttachShader(prog_, fs);
        glLinkProgram(prog_);
        GLint ok = 0;
        glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
            logf("model shader: %s", log);
            glDeleteProgram(prog_);
            prog_ = 0;
            return false;
        }
        u_mvp_ = glGetUniformLocation(prog_, "u_mvp");
        u_model_ = glGetUniformLocation(prog_, "u_model");
        u_skinned_ = glGetUniformLocation(prog_, "u_skinned");
        u_joint_base_ = glGetUniformLocation(prog_, "u_joint_base");
        u_joints_ = glGetUniformLocation(prog_, "u_joints");
        u_tex_ = glGetUniformLocation(prog_, "u_tex");
        u_textured_ = glGetUniformLocation(prog_, "u_textured");
        u_light_ = glGetUniformLocation(prog_, "u_light_dir");
        u_clip_center_ = glGetUniformLocation(prog_, "u_clip_center");
        u_clip_half_ = glGetUniformLocation(prog_, "u_clip_half");
        u_linear_ = glGetUniformLocation(prog_, "u_linear_out");
        u_bright_ = glGetUniformLocation(prog_, "u_bright");
        u_inflate_ = glGetUniformLocation(prog_, "u_inflate");
        u_shield_ = glGetUniformLocation(prog_, "u_shield");
        u_eye_ = glGetUniformLocation(prog_, "u_eye");
        u_time_ = glGetUniformLocation(prog_, "u_time");
        glGenTextures(1, &joint_tex_);
        glBindTexture(GL_TEXTURE_2D, joint_tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        return true;
    }

    void draw_prim(const GltfModel::Prim &p, const glm::mat4 &xf)
    {
        glUniformMatrix4fv(u_model_, 1, GL_FALSE, glm::value_ptr(xf));
        glUniform1i(u_skinned_, p.skinned ? 1 : 0);
        glUniform1i(u_textured_, p.tex != 0 ? 1 : 0);
        glBindTexture(GL_TEXTURE_2D, p.tex);
        glBindVertexArray(p.vao);
        glDrawElements(GL_TRIANGLES, p.count, GL_UNSIGNED_INT, nullptr);
    }

    std::string dir_;
    AssetLoader loader_;
    std::set<std::string> requested_;
    std::map<std::string, std::unique_ptr<GltfModel>> models_;
    GltfModelData sockets_;
    std::unordered_map<uint16_t, Person> persons_;
    uint32_t turn_ = UINT32_MAX;
    double turn_time_ = 0, turn_dt_ = 0.05, last_prepare_ = 0;
    std::vector<Draw> draws_;
    std::vector<PropDraw> prop_draws_;
    std::unordered_map<uint32_t, SwvProp> props_;
    Json scales_;
    bool scales_loaded_ = false, fx_tried_ = false;
    FxSystem fx_;
    std::map<uint32_t, int> effect_handles_;
    std::set<uint16_t> fx_frames_;
    std::vector<std::pair<int, double>> stop_later_;
    double time_ = 0;
    std::vector<glm::mat4> joints_, glob_, wglob_;
    GltfPose pose_a_, pose_b_;
    std::vector<float> joint_tex_data_;
    GLuint prog_ = 0, joint_tex_ = 0;
    GLint u_mvp_ = -1, u_model_ = -1, u_skinned_ = -1, u_joint_base_ = -1, u_joints_ = -1, u_tex_ = -1,
          u_textured_ = -1, u_light_ = -1, u_clip_center_ = -1, u_clip_half_ = -1, u_linear_ = -1,
          u_bright_ = -1, u_inflate_ = -1, u_shield_ = -1, u_eye_ = -1, u_time_ = -1;
};
