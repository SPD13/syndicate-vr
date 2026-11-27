// A virtual keyboard in the headset, for the game's text boxes (the login
// name, a save slot's name): shown while one has the focus (the game's
// edit_flag, outside missions). It lies over the near half of the table,
// tilted towards the viewer, fixed to the table like the screen panel; the
// right hand points at a key, its trigger types it. The keys are the
// game's key codes (bfkeybd.h, PC scan codes), sent into bflib's keyboard
// as the real keyboard's are.
#pragma once

#include "hud.h"

class VirtualKeyboard {
public:
    struct Key {
        std::string label;
        uint8_t code;
        float x, y, w; // in key units from the top left; one unit wide by default
    };

    VirtualKeyboard()
    {
        const char *r0 = "1234567890";
        for (int i = 0; i < 10; i++)
            add(std::string(1, r0[i]), (uint8_t)(0x02 + i), (float)i, 0);
        add("<-", 0x0E, 10.2f, 0, 1.6f); // backspace
        const char *r1 = "QWERTYUIOP";
        for (int i = 0; i < 10; i++)
            add(std::string(1, r1[i]), (uint8_t)(0x10 + i), 0.3f + i, 1);
        const char *r2 = "ASDFGHJKL";
        for (int i = 0; i < 9; i++)
            add(std::string(1, r2[i]), (uint8_t)(0x1E + i), 0.6f + i, 2);
        add("ENTER", 0x1C, 9.8f, 2, 2.0f);
        const char *r3 = "ZXCVBNM";
        for (int i = 0; i < 7; i++)
            add(std::string(1, r3[i]), (uint8_t)(0x2C + i), 0.9f + i, 3);
        add("-", 0x0C, 7.9f, 3);
        add(".", 0x34, 8.9f, 3);
        add("SPACE", 0x39, 2.9f, 4, 5.0f);
    }

    // Over the near half of the table, tilted back 55 degrees from upright.
    Panel place(const glm::mat4 &table_pose) const
    {
        float w = kUnit * kCols, h = kUnit * kRows;
        glm::vec3 right = glm::normalize(glm::vec3(table_pose * glm::vec4(1, 0, 0, 0)));
        float a = glm::radians(35.0f); // from lying flat
        glm::vec3 up = glm::normalize(glm::vec3(table_pose * glm::vec4(0, std::sin(a), -std::cos(a), 0)));
        glm::vec3 c = glm::vec3(table_pose * glm::vec4(0.0f, 0.10f, 0.30f, 1.0f));
        Panel p;
        p.tl = c - right * (w * 0.5f) + up * (h * 0.5f);
        p.right = right;
        p.up = up;
        p.w = w;
        p.h = h;
        p.valid = true;
        return p;
    }

    // The key a ray points at, or -1.
    int key_at(const Panel &p, const Ray &r) const
    {
        float u, v;
        if (!r.valid || !p.local(r, u, v))
            return -1;
        u /= kUnit;
        v /= kUnit;
        for (size_t i = 0; i < keys_.size(); i++) {
            const Key &k = keys_[i];
            if (u >= k.x && u <= k.x + k.w && v >= k.y && v <= k.y + 1)
                return (int)i;
        }
        return -1;
    }
    bool on(const Panel &p, const Ray &r) const
    {
        float u, v;
        return r.valid && p.local(r, u, v) && u >= 0 && v >= 0 && u <= p.w && v <= p.h;
    }

    const Key &key(int i) const { return keys_[i]; }

    void draw(HudBuilder &hud, const Panel &p, int hover, int flash) const
    {
        glm::vec3 n = glm::cross(p.right, p.up);
        static const float board[3] = {0.08f, 0.07f, 0.16f}, cap[3] = {0.22f, 0.16f, 0.42f},
                           lit[3] = {0.45f, 0.35f, 0.85f}, hit[3] = {0.9f, 0.8f, 0.3f},
                           label[3] = {0.85f, 1.0f, 1.0f};
        float m = 0.012f;
        hud.rect(p.tl - n * 0.003f - p.right * m + p.up * m, p.right, p.up, p.w + 2 * m, p.h + 2 * m, board);
        const float gap = 0.06f, px = kUnit * 0.045f;
        for (size_t i = 0; i < keys_.size(); i++) {
            const Key &k = keys_[i];
            glm::vec3 tl = p.tl + p.right * ((k.x + gap) * kUnit) - p.up * ((k.y + gap) * kUnit);
            float kw = (k.w - 2 * gap) * kUnit, kh = (1 - 2 * gap) * kUnit;
            hud.rect(tl, p.right, p.up, kw, kh, (int)i == flash ? hit : (int)i == hover ? lit : cap);
            // the label in the middle (about 6 font pixels a letter, 7 high)
            float tw = 6.0f * px * (float)k.label.size();
            glm::vec3 ttl = tl + p.right * ((kw - tw) * 0.5f) - p.up * ((kh - 7.0f * px) * 0.5f) + n * 0.002f;
            hud.text(ttl, p.right, p.up, px, k.label, label);
        }
    }

private:
    static constexpr float kUnit = 0.055f, kCols = 11.8f, kRows = 5.0f;
    void add(const std::string &label, uint8_t code, float x, float y, float w = 1.0f)
    {
        keys_.push_back(Key{label, code, x, y, w});
    }
    std::vector<Key> keys_;
};

static VirtualKeyboard g_keyboard;
