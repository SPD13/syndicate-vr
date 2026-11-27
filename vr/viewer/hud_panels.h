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
        enum Kind { NONE, BADGE, HEALTH, MOOD, ENERGY, MEDI, WEAPON, LIST, COLUMN, GROUP, THERMAL, SCANNER } kind = NONE;
        int agent = -1;  // slot 0..3
        int weapon = 0;  // LIST: the entry's weapon type
        float frac = 0;  // MOOD: 0..1 across the bar
        glm::vec2 map{0}; // SCANNER: the map point (x, z)
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
        // the scanner's map, coloured with the palette
        if (scan_tex_ == 0) {
            glGenTextures(1, &scan_tex_);
            glBindTexture(GL_TEXTURE_2D, scan_tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, scan_tex_);
        if (scan_dirty_ && have_pal_ && scan_cells_.size() == SWV_SCANNER_DIM * SWV_SCANNER_DIM) {
            std::vector<uint8_t> rgb(3 * scan_cells_.size());
            for (size_t i = 0; i < scan_cells_.size(); i++)
                for (int k = 0; k < 3; k++)
                    rgb[3 * i + k] = scan_built_ ? pal_[scan_cells_[i] * 3 + k] : 18;
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, SWV_SCANNER_DIM, SWV_SCANNER_DIM, 0, GL_RGB, GL_UNSIGNED_BYTE,
                         rgb.data());
            scan_dirty_ = false;
        }
        // the desktop's panels as drawn, coloured with the palette
        if (grab_tex_ == 0) {
            glGenTextures(1, &grab_tex_);
            glBindTexture(GL_TEXTURE_2D, grab_tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, grab_tex_);
        if (grab_dirty_ && grab_ok()) {
            std::vector<uint8_t> rgb(3 * grab_.size());
            for (size_t i = 0; i < grab_.size(); i++)
                for (int k = 0; k < 3; k++)
                    rgb[3 * i + k] = pal_[grab_[i] * 3 + k];
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, grab_w_, grab_h_, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
            grab_dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "u_panel_tex"), 8);
        glUniform1i(glGetUniformLocation(prog, "u_scan_tex"), 9);
        glUniform1i(glGetUniformLocation(prog, "u_grab_tex"), 10);
    }

    // The desktop's panels as it drew them (the game build).
    void set_grab(const WorldSnapshot &snap)
    {
        if (snap.grab_frame == grab_frame_ || snap.grab.empty())
            return;
        grab_ = snap.grab;
        grab_w_ = snap.grab_w;
        grab_h_ = snap.grab_h;
        memcpy(grab_rects_, snap.grab_rects, sizeof(grab_rects_));
        grab_frame_ = snap.grab_frame;
        grab_dirty_ = true;
    }
    bool grab_ok() const { return grab_frame_ != 0 && grab_w_ > 0 && grab_h_ > 0 && have_pal_; }

    // A new reading of the scanner's map.
    void set_scanner(const WorldSnapshot &snap)
    {
        if (snap.scanner_rev == scan_rev_ || snap.scanner.empty())
            return;
        scan_cells_ = snap.scanner;
        scan_built_ = snap.scanner_built;
        scan_rev_ = snap.scanner_rev;
        scan_dirty_ = true;
    }

    // The HUD's size in panel pixels (of the 640x480 layout).
    glm::vec2 size() const
    {
        return glm::vec2(scan_origin().x + scan_w(), std::max(std::max(4 * kStripH, kColH), scan_h()));
    }

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
            if ((pa.flags & SwvPAF_BADGE_BLINK) && (snap.hdr.gameturn & 4))
                sprite(hud, f, o + glm::vec2(badge.dx - badge.x, badge.dy - badge.y), 6 + i, bright, 0.0004f);
            if (grab_ok()) {
                // the bars as the desktop drew them: its own pixels, over
                // the slots of the badge (health, mood, energy), each a
                // little beyond its box for the slant
                const P &hp = panel(8 + i), &mp = panel(4 + i), &ep = panel(12 + i);
                grab_rect(hud, f, o, badge, 8 + i, hp.dx - hp.dh - 1, hp.dy, hp.dx + hp.dw + 2, hp.dy + hp.dh);
                grab_rect(hud, f, o, badge, 4 + i, mp.dx - mp.dh - 1, mp.dy, mp.dx + mp.dw + 2, mp.dy + mp.dh);
                grab_rect(hud, f, o, badge, 12 + i, ep.dx, ep.dy - ep.dh, ep.dx + ep.dw + ep.dh + 2, ep.dy + 1);
            } else {
                // the bars as the desktop draws them (hud_panel.c
                // draw_new_panel_{health,mood,energy}_overlay()), in the
                // panels' dynamic boxes, row by row slanted as AppDrawSlantBox()
                glm::vec3 wv = pi.colour_white ? pal_rgb(pi.colour_white) : glm::vec3(1.0f);
                glm::vec3 rv = pi.colour_red ? pal_rgb(pi.colour_red) : glm::vec3(1.0f, 0.1f, 0.05f);
                const float white[3] = {wv.x, wv.y, wv.z}, red[3] = {rv.x, rv.y, rv.z};
                // health: white up to the maximum, beyond it red from the
                // start; the shield over it (full at max_shield)
                const P &hp = panel(8 + i);
                glm::vec2 h0 = o + dyn_rel(hp, badge);
                int lvmax = ag.max_health;
                if (lvmax > 0) {
                    if (ag.health <= lvmax) {
                        if (ag.health > 0)
                            slant_rows(hud, f, h0, hp.dw * ag.health / lvmax, hp.dh, white, bright);
                    } else {
                        slant_rows(hud, f, h0, hp.dw, hp.dh, white, bright);
                        slant_rows(hud, f, h0, hp.dw * (ag.health - lvmax) / lvmax, hp.dh, red, bright, 0.0003f);
                    }
                }
                int max_sh = pi.max_shield ? pi.max_shield : 1024;
                if (ag.shield > 0) {
                    int hw = 0;
                    if (lvmax > 0 && ag.health > 0)
                        hw = hp.dw * std::min<int>(ag.health, lvmax) / lvmax + 1;
                    shield_bar(hud, f, snap, h0, hp.dw * ag.shield / max_sh, hp.dh, hw, wv, bright);
                }
                // mood: a slanted bar from the middle, (w / 2) * mood / 88
                // wide (one pixel at 0: the red mark), coloured by the mood;
                // the stamina limits: two slanted white lines 3/4 high
                const P &mp = panel(4 + i);
                glm::vec2 m0 = o + dyn_rel(mp, badge);
                {
                    glm::vec3 mv = pi.mood_colour[i] ? pal_rgb(pi.mood_colour[i])
                                                     : (pa.mood >= 0 ? glm::vec3(1.0f, 0.3f, 0.25f)
                                                                     : glm::vec3(0.3f, 0.4f, 1.0f));
                    const float mc[3] = {mv.x, mv.y, mv.z};
                    float cx = m0.x + (float)(mp.dw >> 1);
                    int bw = (mp.dw >> 1) * pa.mood / 88;
                    float l = cx + (float)std::min(0, bw), r = cx + (float)std::max(0, bw) + 1.0f;
                    slant_span(hud, f, l, r, m0.y, (float)mp.dh, mc, bright);
                    if (pa.stamina > 0 && pa.max_stamina > 0) {
                        int lh = mp.dh * 3 / 4;
                        int sc = (mp.dw * pa.stamina / pa.max_stamina) >> 1;
                        for (int side = -1; side <= 1; side += 2) {
                            float lx = cx + (float)(side * sc) + 0.5f;
                            line_px(hud, f, glm::vec2(lx, m0.y), glm::vec2(lx - lh, m0.y + lh), white, bright,
                                    0.0003f);
                        }
                    }
                }
                // energy: a slanted white bar growing up from the top of its
                // box (draw_energy_bar(): a slant box of negative height)
                const P &ep = panel(12 + i);
                glm::vec2 e0 = o + dyn_rel(ep, badge);
                if (ag.max_energy > 0)
                    slant_rows(hud, f, e0, ep.dw, -(ep.dh * ag.energy / ag.max_energy), white, bright);
            }
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
            if (sel >= 0 && grab_ok()) {
                // the desktop's own pixels
                grab_rect(hud, f, c0, cp, 24, cp.dx, cp.dy, cp.dx + cp.dw + 1, cp.dy + cp.dh + cp.dw + 2);
            } else if (sel >= 0) {
                // draw_weapon_energy_bar(): columns slanting down to the
                // right, filled from the bottom up (AppDrawVSlantBox), red
                // every other turn below 50
                const SwvHudAgent &ag = hi.agents[sel];
                glm::vec3 wv = pi.colour_white ? pal_rgb(pi.colour_white) : glm::vec3(1.0f);
                glm::vec3 rv = pi.colour_red ? pal_rgb(pi.colour_red) : glm::vec3(1.0f, 0.1f, 0.05f);
                glm::vec3 cv = (ag.energy < 50 && (snap.hdr.gameturn & 1)) ? rv : wv;
                const float ecol[3] = {cv.x, cv.y, cv.z};
                glm::vec2 d0 = c0 + glm::vec2(cp.dx - cp.x, cp.dy - cp.y);
                if (ag.max_energy > 0 && ag.energy > 0) {
                    float ch = (float)(cp.dh * ag.energy / ag.max_energy), w = (float)cp.dw;
                    float yb = d0.y + cp.dh + 1;
                    hud.quad(f.at(d0.x, yb - 1 - ch, 0.0002f), f.at(d0.x + w, yb - 1 - ch + w, 0.0002f),
                             f.at(d0.x + w, yb + w, 0.0002f), f.at(d0.x, yb, 0.0002f), ecol);
                }
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
        draw_scanner(hud, f, snap, now);
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
        // the scanner: the map point under the pointer
        {
            glm::vec2 q = p - scan_origin();
            if (in_scanner(q)) {
                h.kind = Hit::SCANNER;
                h.map = scan_to_map(snap, q);
                return h;
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
    // the scanner on the right of the stack, at its top, shaped as the
    // desktop's (130 x 123, its top right corner cut by 24)
    glm::vec2 scan_origin() const { return glm::vec2(kLeft + kStripW + 8, 0); }
    float scan_w() const { return 240; }
    float scan_h() const { return scan_w() * 123.0f / 130.0f; }
    float scan_cut() const { return scan_w() * 24.0f / 130.0f; }
    bool in_scanner(glm::vec2 q) const
    {
        float w = scan_w(), hh = scan_h(), c = scan_cut();
        if (q.x < 0 || q.y < 0 || q.x > w || q.y > hh)
            return false;
        return !(q.x > w - c && q.y < q.x - (w - c));
    }
    // Map units per scanner pixel: the desktop's scanner shows zoom / 256
    // cells (128 map units) per pixel of its 130 wide panel.
    float scan_scale(const WorldSnapshot &snap) const
    {
        float zoom = snap.panel_info.scanner_zoom > 0 ? (float)snap.panel_info.scanner_zoom : 128.0f;
        return zoom / 256.0f * 128.0f * 130.0f / scan_w();
    }
    // Centred on the desktop's camera point and turned with it (the
    // engine's screen axes: right (cos a, -sin a), up (sin a, cos a)).
    glm::vec2 scan_to_map(const WorldSnapshot &snap, glm::vec2 q) const
    {
        float a = snap.hdr.cam_angle, k = scan_scale(snap);
        glm::vec2 rt(std::cos(a), -std::sin(a)), up(std::sin(a), std::cos(a));
        glm::vec2 d(q.x - scan_w() * 0.5f, q.y - scan_h() * 0.5f);
        return glm::vec2(snap.hdr.cam_x, snap.hdr.cam_z) + rt * (d.x * k) - up * (d.y * k);
    }
    glm::vec2 map_to_scan(const WorldSnapshot &snap, glm::vec2 m) const
    {
        float a = snap.hdr.cam_angle, k = scan_scale(snap);
        glm::vec2 rt(std::cos(a), -std::sin(a)), up(std::sin(a), std::cos(a));
        glm::vec2 d = m - glm::vec2(snap.hdr.cam_x, snap.hdr.cam_z);
        return glm::vec2(glm::dot(d, rt) / k + scan_w() * 0.5f, -glm::dot(d, up) / k + scan_h() * 0.5f);
    }
    glm::vec3 pal_rgb(int idx) const
    {
        idx &= 255;
        return glm::vec3(pal_[idx * 3], pal_[idx * 3 + 1], pal_[idx * 3 + 2]) / 255.0f;
    }

    void draw_scanner(HudBuilder &hud, const Frame &f, const WorldSnapshot &snap, double now) const
    {
        const SwvPanelInfo &pi = snap.panel_info;
        glm::vec2 o = scan_origin();
        float w = scan_w(), hh = scan_h(), c = scan_cut();
        // the map: two quads, the right one under the cut corner
        glm::vec2 pts[2][4] = {{{0, 0}, {w - c, 0}, {w - c, hh}, {0, hh}}, {{w - c, 0}, {w, c}, {w, hh}, {w - c, hh}}};
        for (auto &quad : pts) {
            glm::vec3 q[4];
            glm::vec2 uv[4];
            for (int k = 0; k < 4; k++) {
                q[k] = f.at(o.x + quad[k].x, o.y + quad[k].y, 0.0001f);
                uv[k] = scan_to_map(snap, quad[k]) / 32768.0f;
            }
            hud.textured_quad_uv(q, uv, 10.0f, 0.85f);
        }
        auto dot = [&](glm::vec2 m, float r, glm::vec3 col, float lift) {
            glm::vec2 s = map_to_scan(snap, m);
            if (!in_scanner(s))
                return;
            float cc[3] = {col.x, col.y, col.z};
            box(hud, f, o + s - glm::vec2(r), 2 * r, 2 * r, cc, 1.0f, lift);
        };
        int turn = (int)snap.hdr.gameturn;
        // vehicles, from their moving geometry
        for (const SwvDynGroup &g : snap.dyn_groups) {
            if ((g.id >> 28) != 0 || g.count == 0 || g.first + g.count > snap.dyn_verts.size())
                continue;
            const SwvVert &v = snap.dyn_verts[g.first];
            dot(glm::vec2(v.x, v.z), 1.5f, pal_rgb(pi.scanner_vehicle_colour), 0.0002f);
        }
        // people: by type, persuaded yellow; the player's agents grow and
        // blink, the others blink small and large (SCANNER_draw_thing())
        for (const SwvPersonRender &pr : snap.persons) {
            if (pr.flags & (SwvPF_DEAD | SwvPF_IN_BUILDING | SwvPF_IN_VEHICLE))
                continue;
            int ci = (pr.flags & SwvPF_PERSUADED) ? pi.scanner_persuaded_colour
                                                  : pi.scanner_people_colour[pr.subtype & 15];
            glm::vec3 col = ci ? pal_rgb(ci) : glm::vec3(0.8f);
            if (pr.flags & SwvPF_PLAYER_AGENT) {
                int ph = turn & 3;
                if (ph != 3)
                    dot(glm::vec2(pr.x, pr.z), 1.0f + ph, col, 0.0003f);
            } else {
                dot(glm::vec2(pr.x, pr.z), (turn & 1) ? 1.5f : 1.0f, col, 0.0002f);
            }
        }
        // the objective's places, as the desktop's scanner draws them
        // (scanner.c SCANNER_process_bbpoints(), scandraw.c
        // SCANNER_draw_blips()): a circle starting at the place every 32
        // turns and growing by 1024 map units a turn (16 points spreading
        // at sin table / 4 << 4 in 1/256 map units), so it sweeps across
        // the whole scanner, darkening as it grows; a blinking mark on the
        // place
        const SwvHudInfo &hi = snap.hud_info;
        float k_px = 1.0f / scan_scale(snap);
        for (uint32_t i = 0; i < hi.n_signals && i < SWV_SIGNALS_MAX; i++) {
            const SwvSignal &sg = hi.signals[i];
            glm::vec2 s = map_to_scan(snap, glm::vec2(sg.x, sg.z));
            // the game's own turn of the circle and its colour then (the
            // signal's colour is a fade table base, too dark by itself);
            // the game core alone (no desktop scanner: zoom 0) neither
            // advances the circles nor has their colours: our own turn
            // and red
            bool own = pi.scanner_zoom <= 0;
            int cnt = own ? turn % 32 : pi.signal_counter[i] & 31;
            glm::vec3 col = own ? glm::vec3(1.0f, 0.15f, 0.1f) : pal_rgb(pi.signal_colour[i][0]);
            glm::vec3 rc = own ? col * (1.0f - cnt / 36.0f) : pal_rgb(pi.signal_colour[i][cnt]);
            float r = 1024.0f * (float)(cnt + 1) * k_px;
            float cc[3] = {rc.x, rc.y, rc.z};
            // segments of about 3 pixels (only those inside are drawn)
            const int n = std::clamp((int)(6.2831853f * r / 3.0f), 32, 2048);
            glm::vec2 prev = s + glm::vec2(r, 0);
            for (int k = 1; k <= n; k++) {
                float a = k * 6.2831853f / n;
                glm::vec2 pt = s + glm::vec2(std::cos(a), std::sin(a)) * r;
                if (in_scanner(prev) && in_scanner(pt)) {
                    glm::vec2 d = pt - prev;
                    float len = glm::length(d);
                    if (len > 1e-3f) {
                        glm::vec2 nn = glm::vec2(-d.y, d.x) / len * 0.6f;
                        glm::vec2 a0 = o + prev, a1 = o + pt;
                        hud.quad(f.at(a0.x - nn.x, a0.y - nn.y, 0.0004f), f.at(a1.x - nn.x, a1.y - nn.y, 0.0004f),
                                 f.at(a1.x + nn.x, a1.y + nn.y, 0.0004f), f.at(a0.x + nn.x, a0.y + nn.y, 0.0004f), cc);
                    }
                }
                prev = pt;
            }
            if (in_scanner(s) && (turn & 4) == 0) {
                float cs[3] = {col.x, col.y, col.z};
                box(hud, f, o + s - glm::vec2(2.0f), 4, 4, cs, 1.0f, 0.0004f);
            }
        }
        // the frame
        glm::vec3 fc = pi.scanner_frame_colour ? pal_rgb(pi.scanner_frame_colour) : glm::vec3(0.2f, 0.11f, 0.47f);
        float fcol[3] = {fc.x, fc.y, fc.z};
        glm::vec2 fr[5] = {{0, 0}, {w - c, 0}, {w, c}, {w, hh}, {0, hh}};
        for (int k = 0; k < 5; k++) {
            glm::vec2 a = o + fr[k], b = o + fr[(k + 1) % 5];
            glm::vec2 d = glm::normalize(b - a), n(-d.y, d.x);
            hud.quad(f.at(a.x - n.x, a.y - n.y, 0.0005f), f.at(b.x - n.x, b.y - n.y, 0.0005f),
                     f.at(b.x + n.x, b.y + n.y, 0.0005f), f.at(a.x + n.x, a.y + n.y, 0.0005f), fcol);
        }
        (void)now;
    }

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
    // The shield over the health bar, as AppDrawTexturedFlowSlantBox()
    // draws it (vec mode 18, texture page 2 at 0, 2 tiles, flowing): the
    // texture stretched over the box (64 texels across at most), slanted
    // by 3 pixels, each pixel half texture, half what is under it (the
    // ghost table): the white health bar up to hw pixels, else the panel.
    void shield_bar(HudBuilder &hud, const Frame &f, const WorldSnapshot &snap, glm::vec2 o, int w, int h, int hw,
                    glm::vec3 under_bar, float bright) const
    {
        static const int waft[33] = {-28, -24, -21, -17, -14, -10, -7, -3, 0,  4,  7,   11,  14,  18,  21,  24, 28,
                                     24,  21,  18,  14,  11,  7,  4,  0, -3, -7, -10, -14, -17, -21, -24, -28};
        if (w <= 0 || h <= 0)
            return;
        const size_t page = (size_t)SWV_TMAP_DIM * SWV_TMAP_DIM;
        bool tex = snap.tmap.size() >= 3 * page && have_pal_;
        int turn = (int)snap.hdr.gameturn;
        int su = (waft[(turn >> 3) & 0x1F] + 30) >> 1, sv = (waft[(turn + 16) & 0x1F] + 30) >> 3;
        int u0 = 32 * 0 + su, v0 = 32 * 2 + sv;
        int lu, lv;
        if (w >= h) {
            lu = 64;
            lv = 64 * h / w;
            if (lv > 24) {
                lv = 24;
                lu = 24 * w / h;
            }
        } else {
            lv = 24;
            lu = 24 * w / h;
            if (lu > 64) {
                lu = 64;
                lv = 64 * h / w;
            }
        }
        const glm::vec3 under_panel(0.10f, 0.08f, 0.30f);
        const float sh = 3.0f;
        for (int r = 0; r < h; r++) {
            float off = sh * (float)r / (float)h;
            int v = v0 + r * lv / h;
            for (int x = 0; x < w; x++) {
                int u = u0 + x * lu / w;
                glm::vec3 tc(0.35f, 0.8f, 1.0f);
                if (tex)
                    tc = pal_rgb(snap.tmap[2 * page + (size_t)(v & 0xFF) * SWV_TMAP_DIM + (u & 0xFF)]);
                glm::vec3 c = 0.5f * tc + 0.5f * (x < hw ? under_bar : under_panel);
                float cc[3] = {c.x * bright, c.y * bright, c.z * bright};
                float px = o.x + x - off, py = o.y + r;
                hud.quad(f.at(px, py, 0.00035f), f.at(px + 1, py, 0.00035f), f.at(px + 1 - sh / h, py + 1, 0.00035f),
                         f.at(px - sh / h, py + 1, 0.00035f), cc);
            }
        }
    }
    // A rectangle of the desktop's drawn panels: x0..x1, y0..y1 in the
    // layout's coordinates (conf/pop1-1.ini) of panel p, put where the
    // layout has it (o: where the stack has the panel's owner, owner: its
    // layout box); sampled where the desktop has the panel now (its dyn
    // box, scaled from the layout's).
    void grab_rect(HudBuilder &hud, const Frame &f, glm::vec2 o, const P &owner, int p, int x0, int y0, int x1,
                   int y1) const
    {
        const P &lp = panel(p);
        const int16_t *r = &grab_rects_[8 * p];
        if (p < 0 || p >= 26 || lp.dw <= 0 || lp.dh <= 0 || r[6] <= 0 || r[7] <= 0)
            return;
        float kx = (float)r[6] / lp.dw, ky = (float)r[7] / lp.dh;
        auto uv = [&](int x, int y) {
            return glm::vec2((r[4] + (x - lp.dx) * kx) / grab_w_, (r[5] + (y - lp.dy) * ky) / grab_h_);
        };
        glm::vec2 a = o + glm::vec2(x0 - owner.x, y0 - owner.y), b = o + glm::vec2(x1 - owner.x, y1 - owner.y);
        glm::vec3 q[4] = {f.at(a.x, a.y, 0.0003f), f.at(b.x, a.y, 0.0003f), f.at(b.x, b.y, 0.0003f),
                          f.at(a.x, b.y, 0.0003f)};
        glm::vec2 t[4] = {uv(x0, y0), uv(x1, y0), uv(x1, y1), uv(x0, y1)};
        hud.textured_quad_uv(q, t, 11.0f, 1.0f);
    }
    static glm::vec2 dyn_rel(const P &p, const P &badge) { return glm::vec2(p.dx - badge.x, p.dy - badge.y); }
    // AppDrawSlantBox(x, y, w, h): rows of w + 1 pixels, each one a pixel
    // left of the one above (h > 0, down from y) or right of the one below
    // (h < 0, up from y)
    void slant_rows(HudBuilder &hud, const Frame &f, glm::vec2 o, int w, int h, const float *rgb, float bright,
                    float lift = 0.0002f) const
    {
        if (h == 0 || w < 0)
            return;
        if (h > 0)
            slant_span(hud, f, o.x, o.x + w + 1, o.y, (float)h, rgb, bright, lift);
        else
            slant_span(hud, f, o.x - h - 1, o.x - h + w, o.y + h + 1, (float)-h, rgb, bright, lift);
    }
    // a parallelogram: top row from l to r at y, h rows, shifting left by
    // a pixel a row
    void slant_span(HudBuilder &hud, const Frame &f, float l, float r, float y, float h, const float *rgb,
                    float bright, float lift = 0.0002f) const
    {
        if (r <= l || h <= 0)
            return;
        float c[3] = {rgb[0] * bright, rgb[1] * bright, rgb[2] * bright};
        hud.quad(f.at(l, y, lift), f.at(r, y, lift), f.at(r - h, y + h, lift), f.at(l - h, y + h, lift), c);
    }
    // a line a pixel thick
    void line_px(HudBuilder &hud, const Frame &f, glm::vec2 a, glm::vec2 b, const float *rgb, float bright,
                 float lift = 0.0002f) const
    {
        glm::vec2 d = b - a;
        float len = glm::length(d);
        if (len < 1e-3f)
            return;
        glm::vec2 n = glm::vec2(-d.y, d.x) / len * 0.5f;
        float c[3] = {rgb[0] * bright, rgb[1] * bright, rgb[2] * bright};
        hud.quad(f.at(a.x - n.x, a.y - n.y, lift), f.at(b.x - n.x, b.y - n.y, lift),
                 f.at(b.x + n.x, b.y + n.y, lift), f.at(a.x + n.x, a.y + n.y, lift), c);
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
    std::vector<uint8_t> grab_;
    int grab_w_ = 0, grab_h_ = 0;
    uint32_t grab_frame_ = 0;
    int16_t grab_rects_[8 * 26] = {};
    bool grab_dirty_ = false;
    GLuint grab_tex_ = 0;
    uint8_t pal_[768] = {};
    bool ini_ok_ = false, sprites_ok_ = false, have_pal_ = false, dirty_ = false;
    GLuint tex_ = 0, scan_tex_ = 0;
    std::vector<uint8_t> scan_cells_;
    uint32_t scan_rev_ = 0;
    bool scan_built_ = false, scan_dirty_ = false;
};

// The panels of the viewer.
static DesktopPanels g_panels;
