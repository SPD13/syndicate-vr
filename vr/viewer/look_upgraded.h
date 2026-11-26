// Syndicate Wars VR viewer: the upgraded look: people drawn with the 3D
// models of the assets repository (HANDOVER.md there: sections 2, 3, 10).
//
// The look can be switched at any time (L, or B on the right controller);
// whatever has no model (a person type not modelled, or not loaded yet) is
// drawn classic. Models load on a worker thread the first time their type
// is seen, and stay loaded. Every person gets a state machine choosing the
// clip from the game's animation state (HANDOVER.md 2.2), with crossfades,
// a walk rate following the speed, and the weapon in hand parented to the
// right hand (socket of agent_arsenal.glb). Nothing here writes to the game.
#pragma once

#include "gltf_model.h"
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

static const char *kModelVertSrc = R"(#version 330 core
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
    vec4 m = u_model * skin * vec4(a_pos, 1.0);
    v_map = m.xyz;
    v_nrm = mat3(u_model) * mat3(skin) * a_nrm;
    v_uv = a_uv;
    gl_Position = u_mvp * m;
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
out vec4 o_col;
void main() {
    vec2 d = abs(v_map.xz - u_clip_center.xz);
    if (d.x > u_clip_half || d.y > u_clip_half)
        discard;
    vec3 col = u_textured != 0 ? texture(u_tex, v_uv).rgb : vec3(0.6);
    // textures are sRGB: light in linear space
    col = pow(col, vec3(2.2));
    vec3 n = normalize(v_nrm);
    float diff = max(abs(dot(n, normalize(u_light_dir))), 0.0);
    col *= (0.45 + 0.75 * diff) * u_bright;
    if (u_linear_out == 0)
        col = pow(col, vec3(1.0 / 2.2));
    o_col = vec4(col, 1.0);
}
)";

class UpgradedLook {
public:
    bool enabled = false;

    /** Folder of the assets repository (absolute). */
    void set_assets(const std::string &dir) { dir_ = dir; }
    const std::string &assets() const { return dir_; }

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
        }
    }

    /** Poses the people placed by People::build, for draw(). */
    void prepare(double now, const std::vector<People::ModelPlace> &places)
    {
        draws_.clear();
        joints_.clear();
        float dt = (float)std::clamp(now - last_prepare_, 0.0, 0.1);
        last_prepare_ = now;
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
            dr.nodes = glob_;
            // weapon in the right hand
            if (ps.weapon != 0) {
                const WeaponDef *wd = weapon_def(ps.weapon);
                auto wm = wd ? models_.find(std::string("weapon:") + wd->name) : models_.end();
                int hand = m->node("mixamorig:RightHand");
                glm::mat4 socket;
                if (wm != models_.end() && hand >= 0 && socket_for(wd->name, socket)) {
                    dr.weapon = wm->second.get();
                    dr.weapon_xform = dr.xform * glob_[hand] * socket;
                }
            }
            draws_.push_back(std::move(dr));
        }
    }

    /** Draws the posed people (inside Renderer::draw, map space). */
    void draw(const glm::mat4 &mvp_map, glm::vec3 clip_center, float clip_half, bool linear_out)
    {
        if (draws_.empty())
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
        glActiveTexture(GL_TEXTURE2);
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
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE0);
    }

private:
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
    };
    struct Draw {
        const GltfModel *model = nullptr;
        const GltfModel *weapon = nullptr;
        glm::mat4 xform{1.0f}, weapon_xform{1.0f};
        int joint_base = 0;
        float bright = 1.0f;
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

    bool init_gl()
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, kModelVertSrc);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, kModelFragSrc);
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
    std::vector<glm::mat4> joints_, glob_;
    GltfPose pose_a_, pose_b_;
    std::vector<float> joint_tex_data_;
    GLuint prog_ = 0, joint_tex_ = 0;
    GLint u_mvp_ = -1, u_model_ = -1, u_skinned_ = -1, u_joint_base_ = -1, u_joints_ = -1, u_tex_ = -1,
          u_textured_ = -1, u_light_ = -1, u_clip_center_ = -1, u_clip_half_ = -1, u_linear_ = -1,
          u_bright_ = -1;
};
