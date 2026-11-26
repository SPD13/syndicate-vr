// Fonts of the VR viewer's HUD text (wrist panel, objective board, world
// markers), which follow the look:
// - classic: the game's own panel font (data/fontc0-1, through
//   swview_load_font()), its pixels shaded as in the game;
// - upgraded: Share Tech Mono (vr/viewer/fonts, SIL OFL), as a signed
//   distance field, sharp at any distance;
// - the 6x11 bitmap font of font_table.inc when neither could be loaded.
// Text is laid out in the cells of the bitmap font (6 x 11 "font pixels"),
// so panels keep their layout whichever font draws them.
#pragma once

#include "common.h"
#include "third_party/stb_truetype.h"

#include <string>
#include <vector>

enum TextFontKind {
    TFK_PIXEL = 0,  // the bitmap font, drawn as small squares
    TFK_GAME = 7,   // GpuVert::textured of the game font (7 + 0.9 * alpha)
    TFK_VECTOR = 8, // of the vector font (8 + 0.9 * alpha)
};

class TextFonts {
public:
    // The font wanted, set every frame from the look; falls back to the
    // others when it is not loaded.
    int want = TFK_GAME;

    int kind() const
    {
        if (want == TFK_VECTOR && vec_ok_)
            return TFK_VECTOR;
        if (want != TFK_PIXEL && game_ok_ && have_palette_)
            return TFK_GAME;
        return TFK_PIXEL;
    }

    // The game's panel font, from the game data folder (current directory).
    bool load_game()
    {
        game_glyphs_.assign(224, SwvGlyph{});
        game_page_.assign(2 * SWV_FONT_PAGE_DIM * SWV_FONT_PAGE_DIM, 0);
        int n = swview_load_font(0, 1, game_glyphs_.data(), 224, game_page_.data());
        if (n <= 0)
            n = swview_load_font(0, 0, game_glyphs_.data(), 224, game_page_.data());
        if (n <= 0) {
            logf("text: game font not loaded");
            return false;
        }
        game_glyphs_.resize(n);
        // a capital's height is the font's: it fills rows 2 to 9 of a cell
        const SwvGlyph &cap = game_glyphs_.size() > 'H' - kGameFirst ? game_glyphs_['H' - kGameFirst] : game_glyphs_[0];
        game_scale_ = cap.h > 0 ? 7.0f / cap.h : 1.0f;
        // narrowed, if needed, so its capitals take the bitmap font's 6
        // pixels on average, as the panels are laid out for
        float sum = 0;
        int cnt = 0;
        for (int c = 'A'; c <= 'Z'; c++)
            if (c - kGameFirst < (int)game_glyphs_.size() && game_glyphs_[c - kGameFirst].w > 0) {
                sum += (game_glyphs_[c - kGameFirst].w + 1) * game_scale_;
                cnt++;
            }
        game_xscale_ = cnt > 0 ? std::min(1.0f, 6.0f * cnt / sum) : 1.0f;
        game_lower_ = game_glyphs_.size() > 'z' - kGameFirst && game_glyphs_['a' - kGameFirst].w > 0;
        game_ok_ = true;
        game_dirty_ = true;
        logf("text: game font, %d characters, capitals %d texels high%s", n, cap.h,
             game_lower_ ? "" : ", capitals only");
        return true;
    }

    // The colours of the game font's pixels: their brightness shades it.
    void set_palette(const uint8_t *pal)
    {
        int maxc = 0;
        for (int i = 0; i < 768; i++)
            maxc = std::max(maxc, (int)pal[i]);
        if (maxc == 0 || have_palette_)
            return;
        float k = maxc <= 63 ? 4.0f / 255.0f : 1.0f / 255.0f;
        for (int i = 0; i < 256; i++)
            lum_[i] = 0.299f * pal[i * 3] * k + 0.587f * pal[i * 3 + 1] * k + 0.114f * pal[i * 3 + 2] * k;
        have_palette_ = true;
        game_dirty_ = true;
    }

    // The vector font: a signed distance field of its printable characters.
    bool load_vector(const std::string &path)
    {
        FILE *f = fopen(path.c_str(), "rb");
        if (!f) {
            logf("text: no font %s", path.c_str());
            return false;
        }
        std::vector<unsigned char> ttf;
        fseek(f, 0, SEEK_END);
        ttf.resize((size_t)ftell(f));
        fseek(f, 0, SEEK_SET);
        size_t got = fread(ttf.data(), 1, ttf.size(), f);
        fclose(f);
        stbtt_fontinfo fi;
        if (got != ttf.size() || !stbtt_InitFont(&fi, ttf.data(), stbtt_GetFontOffsetForIndex(ttf.data(), 0))) {
            logf("text: cannot read font %s", path.c_str());
            return false;
        }
        const float px_h = 64.0f; // texels per em height
        const int pad = 8;
        float sc = stbtt_ScaleForPixelHeight(&fi, px_h);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBox(&fi, 'H', &x0, &y0, &x1, &y1);
        float cap_tx = (y1 - y0) * sc; // capital height in texels
        float fpx = 7.0f / cap_tx;     // font pixels per texel: capitals fill 7 rows
        vec_page_.assign(kVecDim * kVecDim, 0);
        vec_glyphs_.assign(95, VecGlyph{});
        int px = 0, py = 0, shelf = 0;
        for (int c = 32; c < 127; c++) {
            VecGlyph &g = vec_glyphs_[c - 32];
            int adv, lsb;
            stbtt_GetCodepointHMetrics(&fi, c, &adv, &lsb);
            g.adv = adv * sc * fpx;
            int w = 0, h = 0, xo = 0, yo = 0;
            unsigned char *bm = stbtt_GetCodepointSDF(&fi, sc, c, pad, 128, 128.0f / pad, &w, &h, &xo, &yo);
            if (!bm)
                continue;
            if (px + w + 1 > kVecDim) {
                px = 0;
                py += shelf + 1;
                shelf = 0;
            }
            if (py + h <= kVecDim) {
                for (int y = 0; y < h; y++)
                    memcpy(&vec_page_[(py + y) * kVecDim + px], bm + y * w, w);
                g.u0 = (float)px / kVecDim;
                g.v0 = (float)py / kVecDim;
                g.u1 = (float)(px + w) / kVecDim;
                g.v1 = (float)(py + h) / kVecDim;
                // the baseline lies under the capitals, at row 9
                g.x = xo * fpx;
                g.y = 9.0f + yo * fpx;
                g.w = w * fpx;
                g.h = h * fpx;
                g.ok = true;
                px += w + 1;
                shelf = std::max(shelf, h);
            }
            stbtt_FreeSDF(bm, nullptr);
        }
        vec_ok_ = true;
        vec_dirty_ = true;
        logf("text: vector font %s", path.c_str());
        return true;
    }

    // Lays out s (at most max_chars characters) in font pixels, x to the
    // right and y down from the line's top: calls quad(x, y, w, h, u0, v0,
    // u1, v1) for each character of the game or vector font; returns the
    // width. For the bitmap font only the width is computed.
    template <class Q> float layout(const std::string &s, size_t max_chars, Q quad) const
    {
        int k = kind();
        float pen = 0;
        for (size_t i = 0; i < s.size() && i < max_chars; i++) {
            int ch = (unsigned char)s[i];
            if (k == TFK_PIXEL) {
                pen += 6;
            } else if (k == TFK_VECTOR) {
                if (ch < 32 || ch > 126)
                    ch = '?';
                const VecGlyph &g = vec_glyphs_[ch - 32];
                if (g.ok && ch != ' ')
                    quad(pen + g.x, g.y, g.w, g.h, g.u0, g.v0, g.u1, g.v1);
                pen += g.adv;
            } else {
                if (ch >= 'a' && ch <= 'z' && !game_lower_)
                    ch -= 32;
                if (ch < 32 || ch - kGameFirst >= (int)game_glyphs_.size())
                    ch = '?';
                const SwvGlyph &g = game_glyphs_[ch - kGameFirst];
                float w = g.w * game_scale_ * game_xscale_;
                if (g.w > 0 && g.h > 0 && ch != ' ')
                    quad(pen, 2.0f, w, g.h * game_scale_, (float)g.x / SWV_FONT_PAGE_DIM,
                         (float)g.y / SWV_FONT_PAGE_DIM, (float)(g.x + g.w) / SWV_FONT_PAGE_DIM,
                         (float)(g.y + g.h) / SWV_FONT_PAGE_DIM);
                pen += (g.w > 0 ? w : 4.0f) + game_scale_ * game_xscale_;
            }
        }
        return pen;
    }
    float width(const std::string &s, size_t max_chars = 1000) const
    {
        return layout(s, max_chars, [](float, float, float, float, float, float, float, float) {});
    }

    // Binds the font textures for a program in use (u_font_game on unit 5,
    // u_font_vec on unit 7).
    void bind(GLuint prog)
    {
        if (game_tex_ == 0) {
            game_tex_ = make_tex(GL_NEAREST);
            vec_tex_ = make_tex(GL_LINEAR);
        }
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, game_tex_);
        if (game_dirty_ && game_ok_ && have_palette_) {
            // red: the pixel's brightness, green: whether there is one
            std::vector<uint8_t> rg(2 * SWV_FONT_PAGE_DIM * SWV_FONT_PAGE_DIM);
            float top = 0.01f;
            for (size_t i = 0; i < rg.size() / 2; i++)
                if (game_page_[2 * i + 1])
                    top = std::max(top, lum_[game_page_[2 * i]]);
            for (size_t i = 0; i < rg.size() / 2; i++) {
                rg[2 * i] = (uint8_t)std::clamp(lum_[game_page_[2 * i]] / top * 255.0f, 0.0f, 255.0f);
                rg[2 * i + 1] = game_page_[2 * i + 1];
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, SWV_FONT_PAGE_DIM, SWV_FONT_PAGE_DIM, 0, GL_RG,
                         GL_UNSIGNED_BYTE, rg.data());
            game_dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, vec_tex_);
        if (vec_dirty_ && vec_ok_) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kVecDim, kVecDim, 0, GL_RED, GL_UNSIGNED_BYTE,
                         vec_page_.data());
            vec_dirty_ = false;
        }
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(glGetUniformLocation(prog, "u_font_game"), 5);
        glUniform1i(glGetUniformLocation(prog, "u_font_vec"), 7);
    }

private:
    struct VecGlyph {
        float x = 0, y = 0, w = 0, h = 0, adv = 6;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0;
        bool ok = false;
    };
    static const int kVecDim = 512;
    // the game's font sprites start at character 31 (as its text drawing
    // takes them: sprite = code - 31)
    static const int kGameFirst = 31;

    static GLuint make_tex(GLint filter)
    {
        GLuint t;
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return t;
    }

    std::vector<SwvGlyph> game_glyphs_;
    std::vector<uint8_t> game_page_;
    float game_scale_ = 1.0f, game_xscale_ = 1.0f;
    bool game_lower_ = false; // whether the font has lowercase letters
    float lum_[256] = {};
    bool game_ok_ = false, have_palette_ = false, game_dirty_ = false;
    std::vector<VecGlyph> vec_glyphs_;
    std::vector<uint8_t> vec_page_;
    bool vec_ok_ = false, vec_dirty_ = false;
    GLuint game_tex_ = 0, vec_tex_ = 0;
};

// The fonts of the viewer.
static TextFonts g_text;
