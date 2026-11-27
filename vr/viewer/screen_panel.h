// The game's own screen on a flat panel in the headset: the intro, the
// menus, the briefing and the other screens outside a mission
// (syndwars-vr.exe, the desktop game, only). The frame comes from the
// desktop's screen swap (bflib lbScreenSwapHook, palette indices and the
// palette); the panel stands fixed above the far half of the table, facing
// the viewer, and the right hand's pointer on it is the game's mouse.
#pragma once

#include "hud.h"
#include "render_classic.h"

class ScreenPanel {
public:
    // A new frame of the game's screen (palette indices, rows of w).
    void set_frame(const std::vector<uint8_t> &px, int w, int h, const uint8_t *pal_rgb)
    {
        if (w <= 0 || h <= 0 || px.size() < (size_t)w * h)
            return;
        rgb_.resize((size_t)3 * w * h);
        for (size_t i = 0; i < (size_t)w * h; i++) {
            const uint8_t *c = &pal_rgb[3 * px[i]];
            rgb_[3 * i] = c[0];
            rgb_[3 * i + 1] = c[1];
            rgb_[3 * i + 2] = c[2];
        }
        w_ = w;
        h_ = h;
        dirty_ = true;
    }
    bool has_frame() const { return w_ > 0 && h_ > 0; }

    // Binds the frame (unit 10, u_screen_tex) for a program in use.
    void bind(GLuint prog)
    {
        if (tex_ == 0) {
            glGenTextures(1, &tex_);
            glBindTexture(GL_TEXTURE_2D, tex_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_2D, tex_);
        if (dirty_ && has_frame()) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w_, h_, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_.data());
            dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "u_screen_tex"), 10);
    }

    // Its width in metres (--screen-size).
    float width = 1.7f;

    // Where it stands: in the table's frame (it does not follow the head),
    // above the far half of the table, width wide, the game's aspect.
    Panel place(const glm::mat4 &table_pose) const
    {
        float w = width, h = has_frame() ? w * h_ / w_ : w * 0.75f;
        glm::vec3 c = glm::vec3(table_pose * glm::vec4(0.0f, 0.12f + h * 0.5f, -0.55f, 1.0f));
        glm::vec3 right = glm::normalize(glm::vec3(table_pose * glm::vec4(1, 0, 0, 0)));
        glm::vec3 up(0, 1, 0);
        Panel p;
        p.tl = c - right * (w * 0.5f) + up * (h * 0.5f);
        p.right = right;
        p.up = up;
        p.w = w;
        p.h = h;
        p.valid = true;
        return p;
    }

    // The frame on the panel, a thin frame round it, and the pointer's dot.
    void draw(HudBuilder &hud, const Panel &p, bool hovering, glm::vec2 uv) const
    {
        glm::vec3 back = -glm::cross(p.right, p.up) * 0.004f;
        static const float edge[3] = {0.12f, 0.14f, 0.2f};
        float m = 0.02f;
        hud.rect(p.tl + back - p.right * m + p.up * m, p.right, p.up, p.w + 2 * m, p.h + 2 * m, edge);
        glm::vec3 q[4] = {p.tl, p.tl + p.right * p.w, p.tl + p.right * p.w - p.up * p.h, p.tl - p.up * p.h};
        glm::vec2 t[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        if (has_frame())
            hud.textured_quad_uv(q, t, 11.0f, 1.0f);
        if (hovering) {
            // a round dot where the pointer is
            static const float dot[3] = {0.35f, 0.65f, 1.0f};
            glm::vec3 c = p.tl + p.right * (uv.x * p.w) - p.up * (uv.y * p.h) - back * 0.5f;
            const float r = 0.007f;
            const int n = 20;
            for (int i = 0; i < n; i++) {
                float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
                glm::vec3 e0 = c + (p.right * std::cos(a0) + p.up * std::sin(a0)) * r;
                glm::vec3 e1 = c + (p.right * std::cos(a1) + p.up * std::sin(a1)) * r;
                hud.quad(c, e0, e1, e1, dot);
            }
        }
    }

    // Where a ray meets the panel, 0..1 from its top left corner.
    static bool hit(const Panel &p, const Ray &r, glm::vec2 &uv)
    {
        float u, v;
        if (!p.local(r, u, v))
            return false;
        uv = glm::vec2(u / p.w, v / p.h);
        return uv.x >= 0 && uv.y >= 0 && uv.x <= 1 && uv.y <= 1;
    }

private:
    std::vector<uint8_t> rgb_;
    int w_ = 0, h_ = 0;
    bool dirty_ = false;
    GLuint tex_ = 0;
};

static ScreenPanel g_screen;
