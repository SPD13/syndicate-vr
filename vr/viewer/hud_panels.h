// Syndicate Wars VR viewer: the desktop game's agent panels on the wrist.
//
// The same elements as the desktop's in-mission HUD (hud_panel.c), from
// the same sprites (data/pop1-1, through swview_load_sprites()) and the
// same layout file (conf/pop1-1.ini), with the four agents' panels stacked
// top down instead of side by side, the energy column and grouping pane to
// their left:
// - badge (agent number; blinks on the selected agent), health bar with
//   the shield over it, mood bar with the stamina marks, energy mini bar,
//   medikit button, current weapon button with its four-pack ammo, and
//   the weapon list it opens;
// - the energy column of the selected agent (supershield);
// - the grouping pane (agents protecting the selected one) with the
//   thermal view box.
// Clicks do what the desktop's do (check_panel_button(),
// process_panel_state()), through the same packets (swcore.h panel
// commands): the click is the right hand's main trigger, the right click
// the HUD right click (Steam Frame: R2; other controllers: the left
// trigger while the right hand points at the HUD; desktop: right button).
#pragma once

#include "hud.h"

#include <map>

class DesktopPanels {
public:
    // What a point of the HUD is over.
    struct Hit {
        enum Kind { NONE, BADGE, HEALTH, MOOD, ENERGY, MEDI, WEAPON, LIST, COLUMN, GROUP, THERMAL } kind = NONE;
        int agent = -1;  // slot 0..3
        int weapon = 0;  // LIST: the entry's weapon type
        float frac = 0;  // MOOD: 0..1 across the bar
    };

    bool ok() const { return sprites_ok_ && ini_ok_; }

    // Loads the layout and the sprites; on the game's thread (the game's
    // data folder is the current one).
    void load()
    {
        ini_ok_ = read_ini("conf/pop1-1.ini");
        glyphs_.assign(256, SwvGlyph{});
        page_.assign(2 * kDim * kDim, 0);
        int n = swview_load_sprites("pop", 1, 1, glyphs_.data(), 256, page_.data(), kDim);
        sprites_ok_ = n >= 150;
        glyphs_.resize(std::max(n, 0));
        dirty_ = true;
        logf("panels: %s, %d sprites", ini_ok_ ? "conf/pop1-1.ini" : "no layout", n);
    }

    void set_palette(const uint8_t *pal)
    {
        int maxc = 0;
        for (int i = 0; i < 768; i++)
            maxc = std::max(maxc, (int)pal[i]);
        if (maxc == 0 || have_pal_)
            return;
        float k = maxc <= 63 ? 4.0f : 1.0f;
        for (int i = 0; i < 768; i++)
            pal_[i] = (uint8_t)std::min(255.0f, pal[i] * k);
        have_pal_ = true;
        dirty_ = true;
    }

    // Binds the sprite page (unit 8, u_panel_tex) for a program in use.
    void bind(GLuint prog)
    {
        if (tex_ == 0) {
            glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, tex_);
        if (dirty_ && sprites_ok_ && have_pal_) {
            std::vector<uint8_t> rgba(4 * kDim * kDim);
            for (int i = 0; i < kDim * kDim; i++) {
                uint8_t idx = page_[2 * i];
                rgba[4 * i] = pal_[idx * 3];
                rgba[4 * i + 1] = pal_[idx * 3 + 1];
                rgba[4 * i + 2] = pal_[idx * 3 + 2];
                rgba[4 * i + 3] = page_[2 * i + 1];
            }
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kDim, kDim, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "u_panel_tex"), 8);
    }

    // The HUD's size in panel pixels (of the 640x480 layout).
    glm::vec2 size() const { return glm::vec2(kLeft + kStripW, std::max(4 * kStripH, kColH)); }

    // Where the panels are this frame: top left corner, axes, metres per
    // panel pixel.
    struct Frame {
        glm::vec3 tl, right, up;
        float px = 0.0011f;
        glm::vec3 at(float x, float y, float lift = 0) const
        {
            return tl + right * (x * px) - up * (y * px) + glm::cross(right, up) * lift;
        }
    };

    // The state the desktop keeps in PanelState / PanelItem: an open
    // weapon list (agent, whole group) and a mood being dragged.
    struct State {
        int list_agent = -1;
        bool list_group = false;
        int mood_agent = -1;
        bool mood_group = false;
        int mood_sent = 1000;
        double mood_at = 0;
    };

    // Draws the panels; hover: the panel pixel the right hand points at
    // (or negative).
    void draw(HudBuilder &hud, const Frame &f, const WorldSnapshot &snap, const State &st, glm::vec2 hover,
              double now) const
    {
        const SwvHudInfo &hi = snap.hud_info;
        const SwvPanelInfo &pi = snap.panel_info;
        Hit hh = hit_test(snap, st, hover);
        // a dark board behind, as the desktop's panels lie over the game
        static const float board[3] = {0.03f, 0.04f, 0.06f};
        glm::vec2 sz = size();
        hud.quad(f.at(-4, -4, -0.002f), f.at(sz.x + 4, -4, -0.002f), f.at(sz.x + 4, sz.y + 4, -0.002f),
                 f.at(-4, sz.y + 4, -0.002f), board);
        int sel = -1;
        for (int i = 0; i < 4; i++)
            if (hi.agents[i].thing != 0 && hi.agents[i].thing == hi.selected)
                sel = i;
        for (int i = 0; i < 4; i++) {
            const SwvHudAgent &ag = hi.agents[i];
            const SwvPanelAgent &pa = pi.agents[i];
            if (ag.thing == 0 || (ag.flags & SwvTF_DEAD))
                continue;
            glm::vec2 o = strip_origin(i);
            float bright = (pa.flags & SwvPAF_DISABLED) ? 0.5f : 1.0f;
            // badge: three sprites in a row, the middle stretched
            const P &badge = panel(i);
            sprites_in_line(hud, f, o, badge, bright);
            if (i == sel && ((int)(now * 6.0) & 1))
                sprite(hud, f, o + glm::vec2(badge.dx - badge.x, badge.dy - badge.y), 6 + i, bright, 0.0004f);
            // health, the shield over it
            const P &hp = panel(8 + i);
            glm::vec2 h0 = o + rel(hp, badge);
            float fh = ag.max_health > 0 ? (float)ag.health / ag.max_health : 0;
            static const float white[3] = {1.0f, 1.0f, 1.0f}, red[3] = {1.0f, 0.2f, 0.15f},
                               cyan[3] = {0.3f, 0.85f, 1.0f}, blue[3] = {0.25f, 0.45f, 1.0f};
            slant_box(hud, f, h0, std::min(fh, 1.0f) * hp.w, (float)hp.h, white, bright);
            if (fh > 1.0f)
                slant_box(hud, f, h0 + glm::vec2(hp.w, 0), 0, (float)hp.h, red, bright);
            if (ag.max_shield > 0 && ag.shield > 0) {
                float fs = std::min(1.0f, (float)ag.shield / ag.max_shield);
                float pulse = 0.7f + 0.3f * std::sin((float)now * 6.0f);
                float c[3] = {cyan[0] * pulse, cyan[1] * pulse, cyan[2] * pulse};
                slant_box(hud, f, h0, fs * hp.w, (float)hp.h, c, bright, 0.0003f);
            }
            // mood: from the middle, red faster, blue slower; the stamina
            // marks either side
            const P &mp = panel(4 + i);
            glm::vec2 m0 = o + glm::vec2(mp.dx - badge.x, mp.dy - badge.y);
            float mw = (float)mp.dw, mh = (float)mp.dh, mid = m0.x + mw * 0.5f;
            float fm = std::clamp(pa.mood / 88.0f, -1.0f, 1.0f);
            float mc[3];
            const float *base = fm >= 0 ? red : blue;
            for (int k = 0; k < 3; k++)
                mc[k] = base[k] * (0.4f + 0.6f * std::fabs(fm));
            if (fm >= 0)
                box(hud, f, glm::vec2(mid, m0.y), fm * mw * 0.5f, mh, mc, bright);
            else
                box(hud, f, glm::vec2(mid + fm * mw * 0.5f, m0.y), -fm * mw * 0.5f, mh, mc, bright);
            if (pa.max_stamina > 0) {
                float fsx = std::clamp((float)pa.stamina / pa.max_stamina, 0.0f, 1.0f) * mw * 0.5f;
                box(hud, f, glm::vec2(mid + fsx - 0.5f, m0.y - 1), 1.0f, mh + 2, white, bright, 0.0003f);
                box(hud, f, glm::vec2(mid - fsx - 0.5f, m0.y - 1), 1.0f, mh + 2, white, bright, 0.0003f);
            }
            // energy mini bar, growing up
            const P &ep = panel(12 + i);
            glm::vec2 e0 = o + rel(ep, badge);
            float fe = ag.max_energy > 0 ? std::clamp((float)ag.energy / ag.max_energy, 0.0f, 1.0f) : 0;
            box(hud, f, glm::vec2(e0.x, e0.y + ep.h * (1 - fe)), (float)ep.w, ep.h * fe, white, bright);
            // medikit
            const P &medp = panel(16 + i);
            sprite(hud, f, o + rel(medp, badge), (pa.flags & SwvPAF_MEDIKIT) ? medp.extra[0] : medp.spr[0], bright);
            // current weapon button
            const P &wp = panel(20 + i);
            glm::vec2 w0 = o + rel(wp, badge);
            sprite(hud, f, w0, wp.spr[0], bright);
            int cur = ag.current_weapon, shown = cur != 0 ? cur : pa.prev_weapon;
            if (shown > 0 && shown < SWV_WEAPON_TYPES) {
                bool lit = cur != 0 && (pa.weapon_delay == 0 || ((int)(now * 10.0) & 1));
                sprite(hud, f, w0 + shift("WepCurrBtnToSymbol"), pi.weapon_sprite[shown][lit ? 1 : 0], bright,
                       0.0002f);
                fourpack_boxes(hud, f, w0, shown, pa, pi, bright);
            }
            if (hh.kind == Hit::WEAPON && hh.agent == i)
                sprite(hud, f, w0 + shift("WepCurrBtnToDecor"), 90, 1.0f, 0.0003f);
        }
        // energy column of the selected agent: the supershield's sprites
        // when it is on
        {
            const P &cp = panel(24);
            glm::vec2 c0 = col_origin();
            bool sup = sel >= 0 && (pi.agents[sel].flags & SwvPAF_SUPERSHIELD);
            sprites_in_column(hud, f, c0, cp, sup);
            if (sel >= 0) {
                const SwvHudAgent &ag = hi.agents[sel];
                float fe = ag.max_energy > 0 ? std::clamp((float)ag.energy / ag.max_energy, 0.0f, 1.0f) : 0;
                static const float ecol[3] = {0.95f, 0.95f, 1.0f}, low[3] = {1.0f, 0.15f, 0.1f};
                bool blink = ag.energy < 50 && ((int)(now * 4.0) & 1);
                glm::vec2 d0 = c0 + glm::vec2(cp.dx - cp.x, cp.dy - cp.y);
                box(hud, f, glm::vec2(d0.x, d0.y + cp.dh * (1 - fe)), (float)cp.dw, cp.dh * fe, blink ? low : ecol,
                    1.0f, 0.0002f);
            }
        }
        // grouping pane: an icon per agent protecting the selected one;
        // the thermal view
        {
            const P &gp = panel(25);
            glm::vec2 g0 = group_origin();
            sprite(hud, f, g0, gp.spr[0], 1.0f);
            static const char *slots[4] = {"", "GroupPaneAgents2", "GroupPaneAgents3", "GroupPaneAgents4"};
            int n = 0;
            for (int i = 0; i < 4 && n < 4; i++)
                if (pi.agents[i].flags & SwvPAF_PROTECTING) {
                    sprite(hud, f, g0 + (n ? shift(slots[n]) : glm::vec2(0)), 69, 1.0f, 0.0002f);
                    n++;
                }
            if (pi.thermal)
                sprite(hud, f, g0 + shift("GroupPaneToThermalSpr"), 91, 1.0f, 0.0002f);
        }
        // the open weapon list, over the panels below it
        if (st.list_agent >= 0 && st.list_agent < 4 && hi.agents[st.list_agent].thing != 0) {
            std::vector<int> ws = list_weapons(snap, st.list_agent);
            glm::vec2 l0 = list_origin(st.list_agent);
            for (size_t k = 0; k < ws.size(); k++) {
                glm::vec2 e = l0 + shift("WepNextDistance") * (float)k;
                bool first = k == 0;
                sprite(hud, f, e + shift(first ? "WepFrstBtnToDecor" : "WepNextBtnToDecor"), first ? 13 : 94, 1.0f,
                       0.0005f);
                sprite(hud, f, e + shift(first ? "WepFrstBtnToSymbol" : "WepNextBtnToSymbol"),
                       pi.weapon_sprite[ws[k]][0], 1.0f, 0.0007f);
                if (hh.kind == Hit::LIST && hh.weapon == ws[k])
                    sprite(hud, f, e + shift(first ? "WepFrstBtnToDecor" : "WepNextBtnToDecor"), first ? 92 : 93,
                           1.0f, 0.0006f);
                fourpack_boxes(hud, f, e, ws[k], snap.panel_info.agents[st.list_agent], pi, 1.0f, 0.0008f);
            }
        }
    }

    // What panel pixel p is over.
    Hit hit_test(const WorldSnapshot &snap, const State &st, glm::vec2 p) const
    {
        Hit h;
        if (!ok() || p.x < -4 || p.y < -4)
            return h;
        const SwvHudInfo &hi = snap.hud_info;
        // the open list first: it lies over the rest
        if (st.list_agent >= 0 && st.list_agent < 4) {
            std::vector<int> ws = list_weapons(snap, st.list_agent);
            glm::vec2 l0 = list_origin(st.list_agent), area = shift("WepNextButtonArea");
            for (size_t k = 0; k < ws.size(); k++) {
                glm::vec2 e = l0 + shift("WepNextDistance") * (float)k;
                if (inside(p, e, area)) {
                    h.kind = Hit::LIST;
                    h.agent = st.list_agent;
                    h.weapon = ws[k];
                    return h;
                }
            }
        }
        // the grouping pane, its thermal box; the energy column
        {
            const P &gp = panel(25);
            glm::vec2 g0 = group_origin();
            glm::vec2 gs = sprite_size(gp.spr[0], gp);
            if (inside(p, g0, gs)) {
                glm::vec2 tb = g0 + shift("GroupPaneToThermalBox");
                h.kind = inside(p, tb, glm::vec2(gs.x, gs.y - (tb.y - g0.y))) ? Hit::THERMAL : Hit::GROUP;
                return h;
            }
            const P &cp = panel(24);
            if (inside(p, col_origin(), glm::vec2(cp.w, cp.h))) {
                h.kind = Hit::COLUMN;
                return h;
            }
        }
        for (int i = 0; i < 4; i++) {
            const SwvHudAgent &ag = hi.agents[i];
            if (ag.thing == 0 || (ag.flags & SwvTF_DEAD))
                continue;
            glm::vec2 o = strip_origin(i);
            const P &badge = panel(i);
            h.agent = i;
            // as the desktop, the later panels first: weapon, medikit
            const P &wp = panel(20 + i);
            if (inside(p, o + rel(wp, badge), shift("WepCurrButtonArea"))) {
                h.kind = Hit::WEAPON;
                return h;
            }
            const P &medp = panel(16 + i);
            if (inside(p, o + rel(medp, badge), sprite_size(medp.spr[0], medp))) {
                h.kind = Hit::MEDI;
                return h;
            }
            const P &ep = panel(12 + i);
            if (inside(p, o + rel(ep, badge) - glm::vec2(2, 0), glm::vec2(ep.w + 4, ep.h))) {
                h.kind = Hit::ENERGY;
                return h;
            }
            const P &hp = panel(8 + i);
            if (inside(p, o + rel(hp, badge) - glm::vec2(0, 2), glm::vec2(hp.w, hp.h + 4))) {
                h.kind = Hit::HEALTH;
                return h;
            }
            const P &mp = panel(4 + i);
            glm::vec2 m0 = o + rel(mp, badge);
            if (inside(p, m0, glm::vec2(mp.w, mp.h))) {
                h.kind = Hit::MOOD;
                h.frac = std::clamp((p.x - m0.x) / std::max(1, mp.w), 0.0f, 1.0f);
                return h;
            }
            if (inside(p, o, glm::vec2(badge.w, kStripH - 2))) {
                h.kind = Hit::BADGE;
                return h;
            }
        }
        h.agent = -1;
        return h;
    }

    // The weapons of an agent's list: those carried but the one on its
    // button and the energy shield, in order, at most 12
    // (draw_agent_weapons_selection()).
    std::vector<int> list_weapons(const WorldSnapshot &snap, int agent) const
    {
        std::vector<int> out;
        const SwvHudAgent &ag = snap.hud_info.agents[agent];
        int shown = ag.current_weapon != 0 ? ag.current_weapon : snap.panel_info.agents[agent].prev_weapon;
        for (int w = 1; w < SWV_WEAPON_TYPES && out.size() < 12; w++) {
            if (!(ag.weapons & (1u << (w - 1))) || w == shown || w == kEnergyShield)
                continue;
            out.push_back(w);
        }
        return out;
    }

    // The weapon on an agent's button (drawn, or the one put away).
    int button_weapon(const WorldSnapshot &snap, int agent) const
    {
        const SwvHudAgent &ag = snap.hud_info.agents[agent];
        return ag.current_weapon != 0 ? ag.current_weapon : snap.panel_info.agents[agent].prev_weapon;
    }

private:
    struct P {
        int x = 0, y = 0, w = 0, h = 0, dx = 0, dy = 0, dw = 0, dh = 0;
        int spr[3] = {0, 0, 0}, extra[3] = {0, 0, 0};
    };
    static const int kDim = 1024;
    static const int kEnergyShield = 0x17; // WEP_ENERGYSHLD, not in the list
    // the stack: the left column, then agent strips of kStripH pixels
    static constexpr float kLeft = 40, kStripW = 160, kStripH = 56, kColH = 302;

    const P &panel(int i) const
    {
        static const P none;
        auto it = panels_.find(i);
        return it == panels_.end() ? none : it->second;
    }
    glm::vec2 shift(const char *name) const
    {
        auto it = shifts_.find(name);
        return it == shifts_.end() ? glm::vec2(0) : it->second;
    }
    static glm::vec2 rel(const P &p, const P &badge) { return glm::vec2(p.x - badge.x, p.y - badge.y); }
    glm::vec2 strip_origin(int i) const { return glm::vec2(kLeft, i * kStripH); }
    glm::vec2 col_origin() const { return glm::vec2(0, 0); }
    glm::vec2 group_origin() const
    {
        const P &cp = panel(24), &gp = panel(25);
        return glm::vec2(gp.x - cp.x, gp.y - cp.y);
    }
    glm::vec2 list_origin(int agent) const
    {
        const P &badge = panel(agent), &wp = panel(20 + agent);
        return strip_origin(agent) + rel(wp, badge) + shift("AgentWeaponToList");
    }
    glm::vec2 sprite_size(int spr, const P &p) const
    {
        if (p.w > 0 && p.h > 0)
            return glm::vec2(p.w, p.h);
        if (spr > 0 && spr < (int)glyphs_.size())
            return glm::vec2(glyphs_[spr].w, glyphs_[spr].h);
        return glm::vec2(0);
    }
    static bool inside(glm::vec2 p, glm::vec2 o, glm::vec2 s)
    {
        return p.x >= o.x && p.y >= o.y && p.x < o.x + s.x && p.y < o.y + s.y;
    }

    // a sprite at panel pixel o, w x h (its own size when 0)
    void sprite(HudBuilder &hud, const Frame &f, glm::vec2 o, int spr, float bright, float lift = 0.0001f,
                float w = 0, float h = 0) const
    {
        if (spr <= 0 || spr >= (int)glyphs_.size() || glyphs_[spr].w == 0)
            return;
        const SwvGlyph &g = glyphs_[spr];
        if (w <= 0)
            w = g.w;
        if (h <= 0)
            h = g.h;
        float u0 = (float)g.x / kDim, v0 = (float)g.y / kDim, u1 = (float)(g.x + g.w) / kDim,
              v1 = (float)(g.y + g.h) / kDim;
        hud.textured_quad(f.at(o.x, o.y, lift), f.at(o.x + w, o.y, lift), f.at(o.x + w, o.y + h, lift),
                          f.at(o.x, o.y + h, lift), u0, v0, u1, v1, 9.0f, bright);
    }
    void sprites_in_line(HudBuilder &hud, const Frame &f, glm::vec2 o, const P &p, float bright) const
    {
        glm::vec2 l = sprite_size(p.spr[0], P()), r = sprite_size(p.spr[2], P());
        sprite(hud, f, o, p.spr[0], bright);
        sprite(hud, f, o + glm::vec2(l.x, 0), p.spr[1], bright, 0.0001f, std::max(0.0f, p.w - l.x - r.x), 0);
        sprite(hud, f, o + glm::vec2(p.w - r.x, 0), p.spr[2], bright);
    }
    void sprites_in_column(HudBuilder &hud, const Frame &f, glm::vec2 o, const P &p, bool extra) const
    {
        const int *s = extra && p.extra[0] ? p.extra : p.spr;
        glm::vec2 t = sprite_size(s[0], P()), b = sprite_size(s[2], P());
        sprite(hud, f, o, s[0], 1.0f);
        sprite(hud, f, o + glm::vec2(0, t.y), s[1], 1.0f, 0.0001f, 0, std::max(0.0f, p.h - t.y - b.y));
        sprite(hud, f, o + glm::vec2(0, p.h - b.y), s[2], 1.0f);
    }
    void box(HudBuilder &hud, const Frame &f, glm::vec2 o, float w, float h, const float *rgb, float bright,
             float lift = 0.0002f) const
    {
        if (w <= 0 || h <= 0)
            return;
        float c[3] = {rgb[0] * bright, rgb[1] * bright, rgb[2] * bright};
        hud.quad(f.at(o.x, o.y, lift), f.at(o.x + w, o.y, lift), f.at(o.x + w, o.y + h, lift),
                 f.at(o.x, o.y + h, lift), c);
    }
    // the desktop's bars lean right (draw_health_level())
    void slant_box(HudBuilder &hud, const Frame &f, glm::vec2 o, float w, float h, const float *rgb, float bright,
                   float lift = 0.0002f) const
    {
        if (w <= 0 || h <= 0)
            return;
        float c[3] = {rgb[0] * bright, rgb[1] * bright, rgb[2] * bright};
        hud.quad(f.at(o.x + h, o.y, lift), f.at(o.x + w + h, o.y, lift), f.at(o.x + w, o.y + h, lift),
                 f.at(o.x, o.y + h, lift), c);
    }
    // the uses left of a four-pack weapon: small boxes on its button
    void fourpack_boxes(HudBuilder &hud, const Frame &f, glm::vec2 o, int w, const SwvPanelAgent &pa,
                        const SwvPanelInfo &pi, float bright, float lift = 0.0003f) const
    {
        int fp = pi.weapon_fourpack[w];
        if (fp <= 0 || fp > 5)
            return;
        int n = std::min<int>(4, pa.fourpacks[fp - 1]);
        glm::vec2 base = shift("WepFourpackBaseSh"), sz = shift("WepFourpackSize");
        if (sz.x <= 0)
            sz = glm::vec2(4, 4);
        float col[3] = {(float)pal_[247 * 3] / 255.0f, (float)pal_[247 * 3 + 1] / 255.0f,
                        (float)pal_[247 * 3 + 2] / 255.0f};
        for (int k = 0; k < n; k++)
            box(hud, f, o + base + glm::vec2(2, 4 + k * (sz.y + 2)), sz.x, sz.y, col, bright, lift);
    }

    bool read_ini(const char *path)
    {
        FILE *fp = fopen(path, "r");
        if (!fp)
            return false;
        char line[256];
        std::string section;
        while (fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' '))
                s.pop_back();
            if (s.empty() || s[0] == ';')
                continue;
            if (s[0] == '[') {
                section = s.substr(1, s.find(']') - 1);
                continue;
            }
            size_t eq = s.find('=');
            if (eq == std::string::npos)
                continue;
            std::string key = s.substr(0, eq), val = s.substr(eq + 1);
            while (!key.empty() && key.back() == ' ')
                key.pop_back();
            int a = 0, b = 0, c = 0;
            int n = sscanf(val.c_str(), "%d %d %d", &a, &b, &c);
            if (section == "shifts" && n >= 2) {
                shifts_[key] = glm::vec2(a, b);
            } else if (section.rfind("panel", 0) == 0) {
                P &p = panels_[atoi(section.c_str() + 5)];
                if (key == "ScreenPos" && n >= 2) {
                    p.x = a;
                    p.y = b;
                } else if (key == "ScreenSize" && n >= 2) {
                    p.w = a;
                    p.h = b;
                } else if (key == "DynaPos" && n >= 2) {
                    p.dx = a;
                    p.dy = b;
                } else if (key == "DynaSize" && n >= 2) {
                    p.dw = a;
                    p.dh = b;
                } else if (key == "Sprite" && n >= 1) {
                    p.spr[0] = a;
                    p.spr[1] = b;
                    p.spr[2] = c;
                } else if (key == "ExtraSpr" && n >= 1) {
                    p.extra[0] = a;
                    p.extra[1] = b;
                    p.extra[2] = c;
                }
            }
        }
        fclose(fp);
        return panels_.size() >= 26;
    }

    std::map<int, P> panels_;
    std::map<std::string, glm::vec2> shifts_;
    std::vector<SwvGlyph> glyphs_;
    std::vector<uint8_t> page_;
    uint8_t pal_[768] = {};
    bool ini_ok_ = false, sprites_ok_ = false, have_pal_ = false, dirty_ = false;
    GLuint tex_ = 0;
};

// The panels of the viewer.
static DesktopPanels g_panels;
