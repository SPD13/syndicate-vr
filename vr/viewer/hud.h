// Syndicate Wars VR viewer: the HUD: text and panels drawn in the scene.
#pragma once

#include "render_classic.h"

#include "font_table.inc"

class HudBuilder {
public:
    std::vector<GpuVert> verts;

    void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, const float *rgb)
    {
        glm::vec3 q[4] = {a, b, c, d};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 3.0f;
            verts.push_back(g);
        }
    }

    // Rectangle in the plane of (right, up) from its top left corner.
    void rect(glm::vec3 tl, glm::vec3 right, glm::vec3 up, float w, float h, const float *rgb)
    {
        quad(tl, tl + right * w, tl + right * w - up * h, tl - up * h, rgb);
    }

    // Text from its top left corner; px is the size of a font pixel (of
    // the 6x11 cells of text.h, whichever font draws it).
    void text(glm::vec3 tl, glm::vec3 right, glm::vec3 up, float px, const std::string &s,
              const float *rgb, size_t max_chars = 1000)
    {
        int kind = g_text.kind();
        if (kind != TFK_PIXEL) {
            g_text.layout(s, max_chars, [&](float x, float y, float w, float h, float u0, float v0, float u1,
                                             float v1) {
                glm::vec3 a = tl + right * (px * x) - up * (px * y);
                glm::vec3 q[4] = {a, a + right * (px * w), a + right * (px * w) - up * (px * h), a - up * (px * h)};
                float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
                static const int tri[6] = {0, 1, 2, 0, 2, 3};
                for (int k : tri) {
                    GpuVert g{};
                    g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
                    g.uv[0] = uv[k][0]; g.uv[1] = uv[k][1];
                    g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
                    g.textured = kind + 0.9f;
                    verts.push_back(g);
                }
            });
            return;
        }
        for (size_t i = 0; i < s.size() && i < max_chars; i++) {
            int ch = (unsigned char)s[i];
            if (ch < 32 || ch > 126)
                ch = '?';
            const uint8_t *g = kFont6x11[ch - 32];
            for (int y = 0; y < 11; y++) {
                int x = 0;
                while (x < 6) {
                    if (!((g[y] >> x) & 1)) {
                        x++;
                        continue;
                    }
                    int x0 = x;
                    while (x < 6 && ((g[y] >> x) & 1))
                        x++;
                    glm::vec3 p = tl + right * (px * (6 * i + x0)) - up * (px * y);
                    rect(p + up * 0.0f, right, up, px * (x - x0), px, rgb);
                }
            }
        }
    }

    // Thin line facing the eye.
    void line(glm::vec3 a, glm::vec3 b, float width, glm::vec3 eye, const float *rgb)
    {
        glm::vec3 side = glm::cross(b - a, eye - a);
        if (glm::length(side) < 1e-6f)
            return;
        side = glm::normalize(side) * (width * 0.5f);
        quad(a - side, a + side, b + side, b - side, rgb);
    }
};

// A flat board facing the viewer, with rows that can be pointed at.
struct Panel {
    glm::vec3 tl, right, up; // top left corner, unit axes
    float w = 0, h = 0, row_h = 0;
    bool valid = false;

    // Row index under the ray, or -1.
    int row_at(const Ray &r) const
    {
        if (!valid || !r.valid)
            return -1;
        glm::vec3 n = glm::cross(right, up);
        float den = glm::dot(n, r.d);
        if (std::fabs(den) < 1e-6f)
            return -1;
        float t = glm::dot(tl - r.o, n) / den;
        if (t < 0)
            return -1;
        glm::vec3 q = r.o + r.d * t - tl;
        float u = glm::dot(q, right), v = -glm::dot(q, up);
        if (u < 0 || u > w || v < 0 || v > h)
            return -1;
        return (int)(v / row_h);
    }

    float hit_distance(const Ray &r) const
    {
        glm::vec3 n = glm::cross(right, up);
        float den = glm::dot(n, r.d);
        return std::fabs(den) < 1e-6f ? -1.0f : glm::dot(tl - r.o, n) / den;
    }
};

// Faces a panel centred at c towards the eye, turning about the vertical.
static Panel make_panel(glm::vec3 c, glm::vec3 eye, float w, float h, float row_h)
{
    Panel p;
    glm::vec3 to_eye = eye - c;
    to_eye.y = 0;
    if (glm::length(to_eye) < 1e-4f)
        to_eye = glm::vec3(0, 0, 1);
    to_eye = glm::normalize(to_eye);
    p.right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), to_eye));
    p.up = glm::vec3(0, 1, 0);
    p.tl = c - p.right * (w * 0.5f) + p.up * (h * 0.5f);
    p.w = w;
    p.h = h;
    p.row_h = row_h;
    p.valid = true;
    return p;
}

static std::vector<std::string> wrap_text(const std::string &s, size_t width)
{
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        std::string word = s.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back(cur);
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
        if (j == std::string::npos)
            break;
        i = j + 1;
    }
    if (!cur.empty())
        lines.push_back(cur);
    return lines;
}

// Ring of ribbons on the ground around a point, map units.
static void ground_ring(std::vector<GpuVert> &out, glm::vec3 c, float radius, float width,
                        const float *rgb, glm::vec3 eye)
{
    const int n = 20;
    for (int i = 0; i < n; i++) {
        float a1 = i * 6.2831853f / n, a2 = (i + 1) * 6.2831853f / n;
        glm::vec3 p1 = c + glm::vec3(radius * std::cos(a1), 6, radius * std::sin(a1));
        glm::vec3 p2 = c + glm::vec3(radius * std::cos(a2), 6, radius * std::sin(a2));
        // flat on the ground: the ribbon faces up, not the eye
        glm::vec3 side = glm::normalize(glm::cross(p2 - p1, glm::vec3(0, 1, 0))) * (width * 0.5f);
        glm::vec3 q[4] = {p1 - side, p1 + side, p2 + side, p2 - side};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 3.0f;
            out.push_back(g);
        }
    }
    (void)eye;
}

// Reads the game's panel data out of the snapshot block.
struct HudData {
    SwvHud hud{};
    bool valid = false;
};
