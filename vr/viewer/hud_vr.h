// Syndicate Wars VR viewer: the VR HUD: wrist panel, objective board and
// world markers.
//
// The 2D panels of the game do not suit VR: the agents and their weapons
// are on a panel at the left wrist, the objective on a board at the
// table's edge, and the rest is in the scene as markers (HANDOVER.md 7.2
// of the assets repository, markers/markers.json): M1 selection, M4 health,
// M7 waypoint, M9 objective. Markers animate on real time; sizes are in
// metres of a person's scale (kMetre map units) and keep a minimum angular
// size on the table.
#pragma once

#include "hud.h"
#include "play.h"
#include "table_view.h"

#include <unordered_map>

namespace mk {
// map units per metre: an agent's sprite stands about 200 units, 1.8 m
static constexpr float kMetre = 110.0f;
static constexpr float kTau = 6.2831853f;
// colours of markers.json
static const float kFriend[3] = {0.15f, 0.85f, 1.0f};
static const float kNeutral[3] = {0.92f, 0.95f, 1.0f};
static const float kHostile[3] = {1.0f, 0.18f, 0.15f};
static const float kHealthOk[3] = {0.3f, 1.0f, 0.35f};
static const float kHealthMid[3] = {1.0f, 0.85f, 0.2f};
static const float kHealthLow[3] = {1.0f, 0.2f, 0.15f};
static const float kTrail[3] = {1.0f, 0.95f, 0.9f};
static const float kObjective[3] = {1.0f, 0.8f, 0.25f};
static const float kBack[3] = {0.02f, 0.03f, 0.05f};
} // namespace mk

// Builds marker triangles in map units (Renderer::upload_markers).
class MarkerBuilder {
public:
    std::vector<GpuVert> verts;
    glm::vec3 eye{0}; // the viewer, in map units

    // A frame facing the eye, turning about the vertical.
    struct Bill {
        glm::vec3 c, right, up, front; // front: towards the eye
    };

    Bill bill(glm::vec3 c) const
    {
        glm::vec3 to_eye = eye - c;
        to_eye.y = 0;
        if (glm::length(to_eye) < 1e-3f)
            to_eye = glm::vec3(0, 0, 1);
        Bill b;
        b.c = c;
        b.up = glm::vec3(0, 1, 0);
        b.front = glm::normalize(to_eye);
        // map space is mirrored (z), so right is front x up there
        b.right = glm::normalize(glm::cross(b.front, b.up));
        return b;
    }

    // Factor that keeps a size of `size` map units at least `min_rad`
    // radians wide from the eye.
    float keep_visible(glm::vec3 p, float size, float min_rad) const
    {
        float d = glm::distance(p, eye);
        return std::max(1.0f, min_rad * d / std::max(size, 1e-3f));
    }

    void quad(const glm::vec3 *q, const float *rgb, const float *alpha)
    {
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.uv[0] = alpha[k];
            g.textured = 6.0f;
            verts.push_back(g);
        }
    }

    void quad(const glm::vec3 *q, const float *rgb, float a)
    {
        float al[4] = {a, a, a, a};
        quad(q, rgb, al);
    }

    // Flat arc on the ground (y of c), from angle a0 to a1.
    void arc(glm::vec3 c, float r, float w, float a0, float a1, const float *rgb, float a, int segs = 24)
    {
        for (int i = 0; i < segs; i++) {
            float b0 = a0 + (a1 - a0) * i / segs, b1 = a0 + (a1 - a0) * (i + 1) / segs;
            glm::vec3 d0(std::cos(b0), 0, std::sin(b0)), d1(std::cos(b1), 0, std::sin(b1));
            glm::vec3 q[4] = {c + d0 * (r - w * 0.5f), c + d0 * (r + w * 0.5f), c + d1 * (r + w * 0.5f),
                              c + d1 * (r - w * 0.5f)};
            quad(q, rgb, a);
        }
    }

    void ring(glm::vec3 c, float r, float w, const float *rgb, float a)
    {
        arc(c, r, w, 0, mk::kTau, rgb, a, 32);
    }

    // Rectangle in a billboard, from its bottom left corner (x, y) relative
    // to the frame's centre.
    void rect(const Bill &b, float x, float y, float w, float h, const float *rgb, float a,
              float depth = 0)
    {
        glm::vec3 o = b.c + b.right * x + b.up * y + b.front * depth;
        glm::vec3 q[4] = {o, o + b.right * w, o + b.right * w + b.up * h, o + b.up * h};
        quad(q, rgb, a);
    }

    // Text centred on x = 0, its top at y; px is the size of a font pixel.
    void text(const Bill &b, float y, float px, const std::string &s, const float *rgb, float a)
    {
        float x0 = -(float)s.size() * 6 * px * 0.5f;
        for (size_t i = 0; i < s.size(); i++) {
            int ch = (unsigned char)s[i];
            if (ch < 32 || ch > 126)
                ch = '?';
            const uint8_t *g = kFont6x11[ch - 32];
            for (int gy = 0; gy < 11; gy++)
                for (int gx = 0; gx < 6; gx++)
                    if ((g[gy] >> gx) & 1)
                        rect(b, x0 + px * (6 * i + gx), y - px * (gy + 1), px, px, rgb, a, 0.5f);
        }
    }

    // Diamond standing on its tip, spinning about the vertical.
    void diamond(glm::vec3 c, float size, float spin, const float *rgb, float a)
    {
        for (int k = 0; k < 2; k++) {
            float s = spin + k * mk::kTau / 4;
            glm::vec3 side(std::cos(s) * size * 0.6f, 0, std::sin(s) * size * 0.6f);
            glm::vec3 up(0, size, 0);
            glm::vec3 q[4] = {c - up, c + side, c + up, c - side};
            quad(q, rgb, a);
        }
    }
};

// The HUD of the VR viewer: panels in the scene, markers in the world.
class VrHud {
public:
    // What the panels give the commands of the main loop.
    struct Panels {
        Panel wrist;
        std::vector<int> weapon_rows;
        int selected_agent = -1;
        int hover_row = -1;
        bool on_wrist = false;
    };

    // The player's walk order, for the waypoint marker (M7).
    void on_goto(float x, float z, uint16_t agent, double now)
    {
        wp_.active = true;
        wp_.dest = glm::vec3(x, 0, z);
        wp_.agent = agent;
        wp_.since = now;
    }

    // Wrist panel (agents with health, energy and shield; the weapons of the
    // selected one) and the objective board.
    Panels panels(HudBuilder &hud, const WorldSnapshot &snap, bool use_xr, const Ray &left_hand,
                  const Ray &right_hand, const TableView &tv, glm::vec3 eye_world) const
    {
        Panels out;
        const SwvHud &hd = snap.hud;
        const SwvHudInfo &hi = snap.hud_info;
        auto weapon_name = [&](int w) {
            return std::string(hd.weapon_names[w], strnlen(hd.weapon_names[w], sizeof(hd.weapon_names[w])));
        };
        for (int i = 0; i < SWV_AGENTS_MAX; i++)
            if (hd.agents[i].thing != 0 && hd.agents[i].thing == snap.hdr.selected)
                out.selected_agent = i;
        if (out.selected_agent >= 0)
            for (int w = 1; w < SWV_WEAPON_TYPES; w++)
                if (hd.agents[out.selected_agent].weapons & (1u << (w - 1)))
                    out.weapon_rows.push_back(w);
        const float row_h = 0.024f, px = 0.0016f;
        int n_rows = 6 + (int)out.weapon_rows.size();
        glm::vec3 panel_c;
        if (use_xr && left_hand.valid)
            panel_c = left_hand.o + glm::vec3(0, 0.07f + n_rows * row_h * 0.5f, 0);
        else
            panel_c = glm::vec3(tv.table_pose * glm::vec4(-0.95f, 0.25f, 0.1f, 1.0f));
        Panel &panel = out.wrist;
        panel = make_panel(panel_c, eye_world, 0.27f, n_rows * row_h, row_h);
        out.hover_row = panel.row_at(right_hand);
        bool playing = snap.valid && snap.hdr.in_mission;
        out.on_wrist = out.hover_row >= 0 && playing;
        if (!playing)
            return out;
        static const float bg[3] = {0.04f, 0.06f, 0.09f}, sel[3] = {0.08f, 0.32f, 0.36f},
                           hov[3] = {0.35f, 0.32f, 0.08f}, white[3] = {0.9f, 0.92f, 0.95f},
                           dim[3] = {0.55f, 0.6f, 0.65f}, cyan[3] = {0.3f, 0.95f, 1.0f},
                           energy[3] = {0.35f, 1.0f, 0.85f}, gold[3] = {1.0f, 0.8f, 0.25f},
                           red[3] = {1.0f, 0.3f, 0.25f}, bar_bg[3] = {0.1f, 0.12f, 0.15f};
        glm::vec3 back = -glm::cross(panel.right, panel.up) * 0.002f;
        hud.rect(panel.tl + back, panel.right, panel.up, panel.w, panel.h, bg);
        auto row_tl = [&](int r) { return panel.tl - panel.up * (r * row_h); };
        auto row_text = [&](int r, float x, const std::string &s, const float *rgb, size_t n = 26) {
            hud.text(row_tl(r) + panel.right * (0.008f + x) - panel.up * 0.004f, panel.right, panel.up, px,
                     s, rgb, n);
        };
        auto bar = [&](int r, float y, float h, float f, const float *rgb) {
            glm::vec3 o = row_tl(r) + panel.right * 0.17f - panel.up * y;
            hud.rect(o - back * 0.5f, panel.right, panel.up, 0.09f, h, bar_bg);
            if (f > 0)
                hud.rect(o - back, panel.right, panel.up, 0.09f * std::clamp(f, 0.0f, 1.0f), h, rgb);
        };
        row_text(0, 0, "AGENTS", cyan);
        if (hi.mission_status == SwcMS_WON)
            row_text(0, 0.13f, "MISSION WON", gold);
        else if (hi.mission_status == SwcMS_LOST)
            row_text(0, 0.13f, "MISSION LOST", red);
        for (int i = 0; i < SWV_AGENTS_MAX; i++) {
            const SwvAgent &ag = hd.agents[i];
            if (ag.thing == 0)
                continue;
            int r = 1 + i;
            if (i == out.selected_agent)
                hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, sel);
            if (out.on_wrist && out.hover_row == r)
                hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, hov);
            bool dead = (ag.flags & SwvTF_DEAD) != 0;
            std::string wname = ag.current_weapon < SWV_WEAPON_TYPES ? weapon_name(ag.current_weapon) : "";
            char buf[64];
            snprintf(buf, sizeof(buf), "%d %s", i + 1, dead ? "DEAD" : (wname.empty() ? "-" : wname.c_str()));
            row_text(r, 0, buf, dead ? dim : white, 17);
            if (dead)
                continue;
            // health over energy; a shield adds an S
            if (ag.max_health > 0) {
                float f = (float)ag.health / ag.max_health;
                bar(r, 0.006f, 0.007f, f, f > 0.6f ? mk::kHealthOk : (f > 0.3f ? mk::kHealthMid : mk::kHealthLow));
            }
            const SwvHudAgent &ha = hi.agents[i];
            if (ha.thing == ag.thing && ha.max_energy > 0)
                bar(r, 0.016f, 0.004f, (float)ha.energy / ha.max_energy, energy);
            if (ha.thing == ag.thing && ha.shield > 0)
                row_text(r, 0.25f, "S", cyan, 1);
        }
        row_text(5, 0, "WEAPONS", cyan);
        for (size_t k = 0; k < out.weapon_rows.size(); k++) {
            int r = 6 + (int)k;
            int w = out.weapon_rows[k];
            if (out.selected_agent >= 0 && hd.agents[out.selected_agent].current_weapon == w)
                hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, sel);
            if (out.on_wrist && out.hover_row == r)
                hud.rect(row_tl(r), panel.right, panel.up, panel.w, row_h, hov);
            row_text(r, 0, weapon_name(w), white);
        }

        // objective board, leaning back at the far edge of the table
        std::vector<std::string> lines = wrap_text(hd.objective, 34);
        if (!lines.empty()) {
            const float bpx = 0.003f, brow = 0.04f;
            glm::vec3 bc = glm::vec3(tv.table_pose * glm::vec4(0.0f, 0.16f, -0.8f, 1.0f));
            Panel board = make_panel(bc, eye_world, 34 * 6 * bpx + 0.03f, (lines.size() + 1) * brow, brow);
            glm::vec3 to_eye = glm::cross(board.right, board.up);
            const float lean = 0.45f; // radians back from upright
            board.up = glm::normalize(board.up * std::cos(lean) - to_eye * std::sin(lean));
            board.tl = bc - board.right * (board.w * 0.5f) + board.up * (board.h * 0.5f);
            glm::vec3 bback = -glm::cross(board.right, board.up) * 0.002f;
            hud.rect(board.tl + bback, board.right, board.up, board.w, board.h, bg);
            hud.text(board.tl + board.right * 0.015f - board.up * 0.006f, board.right, board.up, bpx * 0.8f,
                     "OBJECTIVE", gold);
            for (size_t l = 0; l < lines.size(); l++)
                hud.text(board.tl + board.right * 0.015f - board.up * (brow * (l + 1) + 0.006f), board.right,
                         board.up, bpx, lines[l], white);
        }
        return out;
    }

    // Markers M1 (selection), M4 (health), M7 (waypoint), M9 (objective).
    void markers(MarkerBuilder &mb, double t, const WorldSnapshot &snap,
                 const std::vector<Picker::Person> &people, const Picker &picker, uint16_t hovered)
    {
        using namespace mk;
        if (!snap.valid || !snap.hdr.in_mission)
            return;
        track_health(t, snap);
        const SwvHudInfo &hi = snap.hud_info;
        std::unordered_map<uint16_t, glm::vec3> pos;
        for (const Picker::Person &pp : people)
            if (!(pp.flags & (SwvTF_HIDDEN | SwvTF_DEAD)))
                pos[pp.thing] = pp.p;

        // M1: under each agent a ring, with turning ticks and its number
        // over the selected one
        for (int i = 0; i < SWV_AGENTS_MAX; i++) {
            const SwvHudAgent &ag = hi.agents[i];
            auto it = pos.find(ag.thing);
            if (ag.thing == 0 || (ag.flags & SwvTF_DEAD) || it == pos.end())
                continue;
            bool sel = ag.thing == hi.selected;
            glm::vec3 c = it->second + glm::vec3(0, 6, 0);
            float k = mb.keep_visible(c, 0.45f * kMetre, 0.0175f);
            float r = 0.45f * kMetre * k;
            if (!sel) {
                mb.ring(c, r, 0.04f * kMetre * k, kFriend, 0.3f);
                continue;
            }
            float pulse = 0.75f + 0.25f * (0.5f + 0.5f * std::sin((float)t * 5.0f));
            mb.ring(c, r, 0.04f * kMetre * k, kFriend, pulse);
            float turn = (float)std::fmod(t, 8.0) * (kTau / 4) / 2.0f; // 90 degrees per 2 s
            for (int q = 0; q < 4; q++) {
                float a0 = turn + q * kTau / 4;
                mb.arc(c, r + 0.07f * kMetre * k, 0.05f * kMetre * k, a0, a0 + 0.35f, kFriend, pulse, 6);
            }
            // number plate with corner brackets, over the health bar
            glm::vec3 pc = it->second + glm::vec3(0, 2.6f * kMetre, 0);
            float kp = mb.keep_visible(pc, 0.2f * kMetre, 0.0105f);
            MarkerBuilder::Bill b = mb.bill(pc);
            float h = 0.3f * kMetre * kp, br = 0.02f * kMetre * kp, bl = 0.08f * kMetre * kp;
            mb.rect(b, -h / 2, -h / 2, h, h, kBack, 0.55f);
            for (int cx = -1; cx <= 1; cx += 2)
                for (int cy = -1; cy <= 1; cy += 2) {
                    float x = cx * h / 2, y = cy * h / 2;
                    mb.rect(b, cx < 0 ? x : x - bl, cy < 0 ? y : y - br, bl, br, kFriend, pulse, 0.3f);
                    mb.rect(b, cx < 0 ? x : x - br, cy < 0 ? y : y - bl, br, bl, kFriend, pulse, 0.3f);
                }
            char num[4];
            snprintf(num, sizeof(num), "%d", i + 1);
            mb.text(b, h * 0.38f, h * 0.07f, num, kFriend, 1.0f);
        }

        // M4: health bars over the agents always, over others when hit or
        // pointed at
        for (const SwvPersonRender &pr : snap.persons) {
            auto it = pos.find(pr.thing);
            if (it == pos.end() || pr.max_health <= 0 || (pr.flags & (SwvPF_DEAD | SwvPF_IN_VEHICLE)))
                continue;
            const Health &hs = health_[pr.thing];
            bool agent = (pr.flags & SwvPF_PLAYER_AGENT) != 0;
            if (!agent && pr.thing != hovered && t - hs.hit_at > 3.0)
                continue;
            float f = std::clamp((float)pr.health / pr.max_health, 0.0f, 1.0f);
            // the white trail follows the drop after 0.25 s, over 0.75 s
            float trail = hs.trail_from + (f - hs.trail_from) *
                          (float)std::clamp((t - hs.hit_at - 0.25) / 0.75, 0.0, 1.0);
            trail = std::max(trail, f);
            glm::vec3 bc = it->second + glm::vec3(0, 2.25f * kMetre, 0);
            float k = mb.keep_visible(bc, 0.6f * kMetre, 0.02f);
            float w = 0.6f * kMetre * k, h = 0.07f * kMetre * k;
            MarkerBuilder::Bill b = mb.bill(bc);
            mb.rect(b, -w / 2, -h / 2, w, h, kBack, 0.6f);
            const float *col = f > 0.6f ? kHealthOk : (f > 0.3f ? kHealthMid : kHealthLow);
            if (trail > f)
                mb.rect(b, -w / 2 + w * f, -h / 2, w * (trail - f), h, kTrail, 0.9f, 0.2f);
            if (f > 0)
                mb.rect(b, -w / 2, -h / 2, w * f, h, col, 0.95f, 0.3f);
            for (int s = 1; s < 5; s++) // 5 segments
                mb.rect(b, -w / 2 + w * s / 5 - h * 0.08f, -h / 2, h * 0.16f, h, kBack, 0.9f, 0.4f);
        }

        // M7: where the selected agent walks, until it is there
        if (wp_.active) {
            auto it = pos.find(wp_.agent);
            glm::vec3 dest(wp_.dest.x, picker.ground_at(wp_.dest.x, wp_.dest.z) + 6, wp_.dest.z);
            if (it == pos.end() || t - wp_.since > 60.0 ||
                glm::length(glm::vec2(it->second.x - dest.x, it->second.z - dest.z)) < 0.6f * kMetre) {
                wp_.active = false;
            } else {
                float k = mb.keep_visible(dest, 0.3f * kMetre, 0.0175f);
                mb.arc(dest, 0.04f * kMetre * k, 0.08f * kMetre * k, 0, kTau, kFriend, 0.95f, 12);
                for (int ri = 0; ri < 3; ri++) {
                    float ph = (float)std::fmod(t / 1.5 + ri / 3.0, 1.0);
                    mb.ring(dest, (0.1f + 0.5f * ph) * kMetre * k, 0.03f * kMetre * k, kFriend, 1.0f - ph);
                }
                glm::vec3 from = it->second;
                glm::vec2 d(dest.x - from.x, dest.z - from.z);
                float len = glm::length(d);
                glm::vec2 dir = d / std::max(len, 1.0f), side(-dir.y, dir.x);
                float step = 0.5f * kMetre * k, cs = 0.12f * kMetre * k;
                int n = std::min(60, (int)(len / step));
                for (int ci = 1; ci < n; ci++) {
                    glm::vec2 p = glm::vec2(from.x, from.z) + dir * (step * ci);
                    float y = picker.ground_at(p.x, p.y) + 6;
                    // lit in sequence towards the destination
                    float ph = (float)std::fmod(t * 1.2 - ci * 0.12, 1.0);
                    if (ph < 0)
                        ph += 1;
                    float a = 0.25f + 0.75f * std::max(0.0f, 1.0f - ph / 0.3f);
                    glm::vec2 tip = p + dir * cs, l = p - side * cs, r = p + side * cs;
                    glm::vec2 lw = dir * (cs * 0.4f);
                    glm::vec3 q1[4] = {glm::vec3(l.x, y, l.y), glm::vec3(tip.x, y, tip.y),
                                       glm::vec3(tip.x - lw.x, y, tip.y - lw.y),
                                       glm::vec3(l.x - lw.x, y, l.y - lw.y)};
                    glm::vec3 q2[4] = {glm::vec3(r.x, y, r.y), glm::vec3(tip.x, y, tip.y),
                                       glm::vec3(tip.x - lw.x, y, tip.y - lw.y),
                                       glm::vec3(r.x - lw.x, y, r.y - lw.y)};
                    mb.quad(q1, kFriend, a);
                    mb.quad(q2, kFriend, a);
                }
            }
        }

        // M9: a light column over each place of the objective
        for (uint32_t si = 0; si < hi.n_signals && si < 4; si++) {
            glm::vec3 g(hi.signals[si].x, 0, hi.signals[si].z);
            g.y = picker.ground_at(g.x, g.z) + 6;
            float k = mb.keep_visible(g, 0.8f * kMetre, 0.0175f);
            MarkerBuilder::Bill b = mb.bill(g);
            float w = 0.3f * kMetre * k, h = 6.0f * kMetre;
            glm::vec3 q[4] = {g - b.right * (w / 2), g + b.right * (w / 2), g + b.right * (w / 2) + b.up * h,
                              g - b.right * (w / 2) + b.up * h};
            const float al[4] = {0.55f, 0.55f, 0.0f, 0.0f};
            mb.quad(q, kObjective, al);
            float pulse = 0.7f + 0.3f * std::sin((float)t * 3.0f);
            mb.ring(g, 0.8f * kMetre * k, 0.05f * kMetre * k, kObjective, pulse);
            mb.diamond(g + glm::vec3(0, h, 0), 0.3f * kMetre * k, (float)std::fmod(t * 1.5, (double)kTau),
                       kObjective, 0.9f);
        }
    }

private:
    struct Health {
        int16_t last = -1;
        float trail_from = 1.0f; // fraction shown before the last hit
        double hit_at = -100.0;
    };

    // Notices health dropping between turns.
    void track_health(double t, const WorldSnapshot &snap)
    {
        if (snap.hdr.gameturn == health_turn_)
            return;
        health_turn_ = snap.hdr.gameturn;
        for (const SwvPersonRender &pr : snap.persons) {
            Health &hs = health_[pr.thing];
            if (hs.last >= 0 && pr.health < hs.last && pr.max_health > 0) {
                float before = std::clamp((float)hs.last / pr.max_health, 0.0f, 1.0f);
                // a hit while the trail still runs starts from where it is
                if (t - hs.hit_at < 1.0)
                    before = std::max(before, hs.trail_from);
                hs.trail_from = before;
                hs.hit_at = t;
            }
            hs.last = pr.health;
        }
    }

    struct Waypoint {
        bool active = false;
        glm::vec3 dest{0};
        uint16_t agent = 0;
        double since = 0;
    } wp_;
    std::unordered_map<uint16_t, Health> health_;
    uint32_t health_turn_ = UINT32_MAX;
};
