// Syndicate Wars viewer: the desktop game's own camera, for drawing the
// world in a window exactly where the desktop draws it (syndwars-hd.exe:
// the world in the GL renderers under the game's 2D layer).
//
// The projection is the engine's transform_shpoint() (swrendersoft
// engintrns.c), done in the vertex shaders: map point (x, y, z) relative to
// the camera point; a = cos*dx - sin*dz across, b = sin*dx + cos*dz into the
// screen (the yaw); the tilt gives the height yc = tc*dy - ts*b and the
// depth d = ts*dy + tc*b; screen x = cx + a*S*f, y = cy - yc*S*f with
// S = overall_scale / 2048 and, in perspective, f = (16384 - d') / 16384
// (d' = d, or 16384*d/(d + 16384) past 1024). Points come out with w = 1:
// triangles are filled straight across the screen, as the desktop does.
#pragma once

#include "common.h"

// GLSL: the uniforms and desk_project(); the programs that draw the world
// pick it with u_desk != 0 instead of their u_mvp.
#define DESK_PROJ_GLSL                                                                                   \
    "uniform int u_desk;\n"                                                                              \
    "uniform vec4 u_desk_cam;\n" /* camera point (map units), overall_scale / 2048 */                    \
    "uniform vec4 u_desk_rot;\n" /* sin, cos of the yaw; sin, cos of the tilt */                         \
    "uniform vec4 u_desk_scr;\n" /* screen centre x, y; view width, height (pixels) */                   \
    "uniform vec2 u_desk_opt;\n" /* perspective (0, 1); map units per unit of the position */            \
    "vec4 desk_project(vec3 q) {\n"                                                                      \
    "    vec3 p = q * u_desk_opt.y - u_desk_cam.xyz;\n"                                                  \
    "    float a = u_desk_rot.y * p.x - u_desk_rot.x * p.z;\n"                                           \
    "    float b = u_desk_rot.x * p.x + u_desk_rot.y * p.z;\n"                                           \
    "    float yc = u_desk_rot.w * p.y - u_desk_rot.z * b;\n"                                            \
    "    float d = u_desk_rot.z * p.y + u_desk_rot.w * b;\n"                                             \
    "    float f = 1.0;\n"                                                                               \
    "    if (u_desk_opt.x > 0.5) {\n"                                                                    \
    "        float dd = d > 1024.0 ? 16384.0 * d / (d + 16384.0) : d;\n"                                 \
    "        f = (16384.0 - dd) / 16384.0;\n"                                                            \
    "    }\n"                                                                                            \
    "    float sx = u_desk_scr.x + a * u_desk_cam.w * f;\n"                                              \
    "    float sy = u_desk_scr.y - yc * u_desk_cam.w * f;\n"                                             \
    "    return vec4(2.0 * sx / u_desk_scr.z - 1.0, 1.0 - 2.0 * sy / u_desk_scr.w,\n"                    \
    "                clamp(d / 65536.0, -1.0, 1.0), 1.0);\n"                                             \
    "}\n"

struct DeskProjection {
    bool on = false;
    float cam[4] = {0, 0, 0, 0}, rot[4] = {0, 1, 0, 1}, scr[4] = {320, 240, 640, 480};
    bool persp = true;

    // From the world export's header (the camera of that turn).
    void set(const SwvHeader &h)
    {
        float w = (float)(h.view_size >> 16), hh = (float)(h.view_size & 0xFFFF);
        if (w <= 0 || hh <= 0) {
            w = 640;
            hh = 480;
        }
        // the engine: vec_window_width / 2, in whole pixels
        scr[0] = (float)((int)w / 2);
        scr[1] = (float)((int)hh / 2);
        scr[2] = w;
        scr[3] = hh;
        cam[0] = h.cam_x;
        cam[1] = h.cam_y;
        cam[2] = h.cam_z;
        cam[3] = (float)h.view_scale / 2048.0f;
        rot[0] = std::sin(h.cam_angle);
        rot[1] = std::cos(h.cam_angle);
        float tilt = (h.view_tilt != 0 ? (float)h.view_tilt : -(303.0f * 1024.0f / 1800.0f)) * (6.2831853f / 2048.0f);
        rot[2] = std::sin(tilt);
        rot[3] = std::cos(tilt);
        persp = h.view_perspective != 0;
    }

    // Into the screen, in map space (unit length).
    glm::vec3 forward() const { return glm::vec3(rot[0] * rot[3], rot[2], rot[1] * rot[3]); }

    // Where the eye is, for what faces it (sprites, particles): the centre
    // of the projection, 16384 units before the camera point (far away
    // without perspective).
    glm::vec3 eye_map() const
    {
        return glm::vec3(cam[0], cam[1], cam[2]) - forward() * (persp ? 16384.0f : 1.0e6f);
    }

    // The uniforms of a program in use; unit: map units per unit of the
    // positions it projects.
    void bind(GLuint prog, float unit = 1.0f) const
    {
        glUniform1i(glGetUniformLocation(prog, "u_desk"), on ? 1 : 0);
        if (!on)
            return;
        glUniform4fv(glGetUniformLocation(prog, "u_desk_cam"), 1, cam);
        glUniform4fv(glGetUniformLocation(prog, "u_desk_rot"), 1, rot);
        glUniform4fv(glGetUniformLocation(prog, "u_desk_scr"), 1, scr);
        glUniform2f(glGetUniformLocation(prog, "u_desk_opt"), persp ? 1.0f : 0.0f, unit);
    }
};

static DeskProjection g_desk;
