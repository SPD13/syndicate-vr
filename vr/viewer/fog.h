// Fog of war of the VR viewer (on by default, --no-fog switches it off; G
// on the desktop, the right grip in VR): the player sees around their
// agents only.
//
// - Seen now: a circle around each living agent, as wide as the most the
//   desktop game's window can show (half its width at the furthest zoom);
//   where agents are near each other, their circles join.
// - Seen before: ground and buildings that have been inside a circle stay,
//   grey and under a slow smoke, as they were last seen may be out of date;
//   people, vehicles and effects there are not shown.
// - Never seen: dark ground, no buildings.
//
// The viewer always records what agents have seen, so switching the fog on
// mid-mission shows the real history. Shaders include kFogGlsl and call
// fog_apply(); draws set u_fog to the kind of what they draw (FogKind).
#pragma once

#include "common.h"
#include "world_source.h"

#include <string>
#include <vector>

enum FogKind {
    FogK_NONE = 0,   // drawn as it is (table, markers, HUD)
    FogK_WORLD = 1,  // ground and buildings: grey where seen before, gone where never
    FogK_MOVING = 2, // vehicles, effects: only inside the circles
    FogK_THINGS = 3, // what stays where seen before (statics, models): grey there
};

static const char *kFogGlsl = R"(
uniform int u_fog;
uniform sampler2D u_fog_tex;
uniform vec2 u_fog_pos[8];
uniform int u_fog_n;
uniform float u_fog_r;
uniform float u_fog_time;
uniform float u_fog_unit;
float fog_vis(vec2 m) {
    float d = 1e9;
    for (int i = 0; i < u_fog_n; i++)
        d = min(d, distance(m, u_fog_pos[i]));
    return 1.0 - smoothstep(u_fog_r - 96.0, u_fog_r, d);
}
float fog_hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float fog_noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(fog_hash(i), fog_hash(i + vec2(1, 0)), f.x),
               mix(fog_hash(i + vec2(0, 1)), fog_hash(i + vec2(1, 1)), f.x), f.y);
}
float fog_smoke(vec2 m) {
    vec2 p = m / 700.0;
    float t = u_fog_time;
    return 0.6 * fog_noise(p + vec2(t * 0.11, t * 0.05)) + 0.4 * fog_noise(p * 2.3 - vec2(t * 0.07, -t * 0.13));
}
// col: the shaded colour (display space); pos: the point (map units times
// u_fog_unit); terrain: ground, which shows (dark) where never seen.
// keep turns false where nothing may be drawn.
vec3 fog_apply(vec3 col, vec3 pos, bool terrain, out bool keep) {
    keep = true;
    if (u_fog == 0)
        return col;
    vec2 m = pos.xz * u_fog_unit;
    float vis = fog_vis(m);
    if (vis >= 1.0)
        return col;
    if (u_fog == 2) {
        keep = vis > 0.5;
        return col;
    }
    float seen = texture(u_fog_tex, m / 32768.0).r;
    if (seen < 0.5 && !terrain) {
        keep = vis > 0.5;
        return col;
    }
    float s = fog_smoke(m);
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 grey = mix(vec3(lum * 0.5 + 0.02), vec3(0.46, 0.48, 0.52), 0.1 + 0.3 * s);
    vec3 dark = vec3(0.03, 0.035, 0.045) + vec3(0.025) * s;
    vec3 c = mix(dark, grey, smoothstep(0.3, 0.7, seen));
    return mix(c, col, vis);
}
)";

// Inserts the fog functions into a shader, after its #version line.
static std::string with_fog(const char *src)
{
    std::string s(src);
    size_t nl = s.find('\n');
    s.insert(nl + 1, kFogGlsl);
    return s;
}

class Fog {
public:
    bool enabled = true;
    float radius_override = 0; // --fog-radius, map units; 0: from the game

    float radius() const { return radius_override > 0 ? radius_override : radius_; }

    // Called every frame with where the living agents are (map x, z).
    void update(const WorldSnapshot &snap, const std::vector<glm::vec2> &agents, double now)
    {
        time_ = now;
        // half the width of the desktop game's window at its furthest zoom
        float scale = (float)snap.hdr.view_scale_min;
        float vw = (float)(snap.hdr.view_size >> 16);
        if (scale > 0 && vw > 0)
            radius_ = vw * 0.5f * 8.0f * 256.0f / scale;
        agents_.assign(agents.begin(), agents.begin() + std::min<size_t>(agents.size(), 8));
        if (!snap.valid || !snap.hdr.in_mission)
            return;
        float r = radius();
        for (const glm::vec2 &a : agents_) {
            int x0 = std::max(0, (int)((a.x - r) / kCell)), x1 = std::min(kN - 1, (int)((a.x + r) / kCell));
            int z0 = std::max(0, (int)((a.y - r) / kCell)), z1 = std::min(kN - 1, (int)((a.y + r) / kCell));
            for (int z = z0; z <= z1; z++)
                for (int x = x0; x <= x1; x++) {
                    uint8_t &c = seen_[z * kN + x];
                    if (c == 0 && glm::distance(glm::vec2((x + 0.5f) * kCell, (z + 0.5f) * kCell), a) < r) {
                        c = 255;
                        dirty_ = true;
                    }
                }
        }
    }

    // 2 seen now, 1 seen before, 0 never; everything is seen without fog.
    int state(glm::vec3 p) const
    {
        if (!enabled)
            return 2;
        if (visible(p))
            return 2;
        int x = (int)(p.x / kCell), z = (int)(p.z / kCell);
        if (x < 0 || z < 0 || x >= kN || z >= kN)
            return 0;
        return seen_[z * kN + x] ? 1 : 0;
    }
    bool visible(glm::vec3 p) const
    {
        float r = radius();
        for (const glm::vec2 &a : agents_)
            if (glm::distance(glm::vec2(p.x, p.z), a) < r)
                return true;
        return false;
    }

    // Sets the fog's uniforms of a program in use; unit: map units per unit
    // of the positions the shader passes to fog_apply().
    void bind(GLuint prog, float unit = 1.0f)
    {
        if (tex_ == 0) {
            glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            dirty_ = true;
        }
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, tex_);
        if (dirty_) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kN, kN, 0, GL_RED, GL_UNSIGNED_BYTE, seen_.data());
            dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "u_fog_tex"), 6);
        float pos[16] = {0};
        for (size_t i = 0; i < agents_.size(); i++) {
            pos[i * 2] = agents_[i].x;
            pos[i * 2 + 1] = agents_[i].y;
        }
        glUniform2fv(glGetUniformLocation(prog, "u_fog_pos"), 8, pos);
        glUniform1i(glGetUniformLocation(prog, "u_fog_n"), (int)agents_.size());
        glUniform1f(glGetUniformLocation(prog, "u_fog_r"), radius());
        glUniform1f(glGetUniformLocation(prog, "u_fog_time"), (float)time_);
        glUniform1f(glGetUniformLocation(prog, "u_fog_unit"), unit);
        kind(prog, FogK_NONE);
    }
    // What the next draws of the program are (FogKind).
    void kind(GLuint prog, int k) const
    {
        glUniform1i(glGetUniformLocation(prog, "u_fog"), enabled ? k : 0);
    }

    // The edge of what the agents see: dashes along each circle, where no
    // other circle covers it. Calls dash(a, b) for each, on the ground.
    template <class F> void outline(F dash) const
    {
        float r = radius();
        int n = std::max(16, (int)(6.2831853f * r / 140.0f)) & ~1;
        for (size_t i = 0; i < agents_.size(); i++)
            for (int k = 0; k < n; k += 2) {
                float a1 = 6.2831853f * k / n, a2 = 6.2831853f * (k + 1) / n;
                glm::vec2 p1 = agents_[i] + r * glm::vec2(std::cos(a1), std::sin(a1));
                glm::vec2 p2 = agents_[i] + r * glm::vec2(std::cos(a2), std::sin(a2));
                glm::vec2 mid = (p1 + p2) * 0.5f;
                bool covered = false;
                for (size_t j = 0; j < agents_.size() && !covered; j++)
                    covered = j != i && glm::distance(mid, agents_[j]) < r - 1.0f;
                if (!covered)
                    dash(p1, p2);
            }
    }

private:
    static const int kN = 256;
    static constexpr float kCell = 32768.0f / kN;
    std::vector<uint8_t> seen_ = std::vector<uint8_t>(kN * kN, 0);
    std::vector<glm::vec2> agents_;
    float radius_ = 2156.0f;
    double time_ = 0;
    bool dirty_ = true;
    GLuint tex_ = 0;
};

// The one fog of the viewer, used by the renderers.
static Fog g_fog;
