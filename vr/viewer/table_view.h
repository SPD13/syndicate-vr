// Syndicate Wars VR viewer: the table view: which part of the map is shown, and how.
#pragma once

#include "common.h"

// Game camera tilt: cos and sin of 30.3 degrees.
static constexpr float kTiltCos = 0.8634f;
static constexpr float kTiltSin = 0.5045f;

/******************************************************************************/
// What the desktop's window shows

// The map point (x, z) of the ground seen at a point of the desktop's
// window, (sx, sy) pixels from its centre, x to the right, y up; at view
// scale scale (the header's view_scale, or view_scale_min for the furthest
// zoom). The inverse of the engine's transform_shpoint() for ground at the
// camera's height: a = cos*dx - sin*dz across the screen, b = sin*dx +
// cos*dz into it; the tilt lifts b by -sin(tilt), and in perspective both
// shrink by (16384 - d) / 16384 with d = cos(tilt) * b.
static glm::vec2 desktop_screen_to_ground(const SwvHeader &h, float scale, float sx, float sy)
{
    float tilt = (h.view_tilt != 0 ? (float)h.view_tilt : -(303.0f * 1024.0f / 1800.0f)) * (6.2831853f / 2048.0f);
    float ty = std::sin(tilt), txz = std::cos(tilt);
    bool persp = h.view_perspective != 0;
    auto shrink = [&](float b) {
        if (!persp)
            return 1.0f;
        float d = txz * b;
        if (d > 1024.0f)
            d = 16384.0f * d / (d + 16384.0f);
        return (16384.0f - d) / 16384.0f;
    };
    // the screen's height of depth b: -ty * b * scale / 2048, shrunk
    float k = scale / 2048.0f;
    float lo = -9000.0f, hi = 9000.0f;
    for (int i = 0; i < 40; i++) {
        float mid = 0.5f * (lo + hi);
        float y = -ty * mid * k * shrink(mid);
        if ((y < sy) == (-ty > 0))
            lo = mid;
        else
            hi = mid;
    }
    float b = 0.5f * (lo + hi);
    float a = sx / (k * shrink(b));
    float s = std::sin(h.cam_angle), c = std::cos(h.cam_angle);
    return glm::vec2(h.cam_x + c * a + s * b, h.cam_z - s * a + c * b);
}

// The border of the desktop's window on the ground, n points along each
// edge, clockwise from the top left corner.
static void desktop_view_outline(const SwvHeader &h, float scale, int n, std::vector<glm::vec2> &out)
{
    out.clear();
    float w = (float)(h.view_size >> 16), hh = (float)(h.view_size & 0xFFFF);
    if (w <= 0 || hh <= 0 || scale <= 0)
        return;
    glm::vec2 k[4] = {{-w / 2, hh / 2}, {w / 2, hh / 2}, {w / 2, -hh / 2}, {-w / 2, -hh / 2}};
    for (int e = 0; e < 4; e++)
        for (int i = 0; i < n; i++) {
            glm::vec2 p = glm::mix(k[e], k[(e + 1) % 4], (float)i / n);
            out.push_back(desktop_screen_to_ground(h, scale, p.x, p.y));
        }
}

/******************************************************************************/
// Table view state: which part of the map is shown, and how

struct TableView {
    glm::vec3 center{16384, 0, 16384}; // map units, the point at table centre
    float half_map = 5120;              // map units from centre to table edge
    float yaw = 0;                      // table rotation, radians
    // the views linked: the table follows the desktop's camera, and moving
    // the table moves the desktop's camera (A on the right controller, F)
    bool follow = true;
    // The point of the table in front of the viewer (world space, on the
    // table top): the headset's "centre of the view", as the desktop's
    // camera point is the centre of its window. The table's centre without
    // a headset.
    glm::vec3 anchor{0};
    float table_half_m = 0.7f;          // physical half size in meters
    glm::mat4 table_pose{1.0f};         // table top centre in world space

    float scale() const { return table_half_m / half_map; }

    // map units -> table-local meters -> world; the game's axes are
    // left-handed, so Z is flipped to show the map the right way round
    glm::mat4 map_model() const
    {
        glm::mat4 m = table_pose;
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(scale(), scale(), -scale()));
        m = glm::translate(m, glm::vec3(-center.x, -ground_offset, -center.z));
        return m;
    }

    // table mesh is built in map units around origin
    glm::mat4 table_model() const
    {
        glm::mat4 m = table_pose;
        m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
        m = glm::scale(m, glm::vec3(scale()));
        return m;
    }

    // The map point (x, z) under a world point of the table top.
    glm::vec2 map_at(glm::vec3 world) const
    {
        glm::vec4 m = glm::inverse(map_model()) * glm::vec4(world, 1.0f);
        return glm::vec2(m.x, m.z);
    }

    // The table centre's map point that puts map point m under world
    // point w of the table top.
    glm::vec3 center_for(glm::vec2 m, glm::vec3 w) const
    {
        glm::vec2 now = map_at(w);
        return glm::vec3(center.x + m.x - now.x, 0, center.z + m.y - now.y);
    }

    // The desktop's camera point goes under the anchor, easing there; the
    // first time at once, with the table turned as the desktop's view
    // (its top away from the viewer).
    void follow_camera(const SwvHeader &h, float dt)
    {
        if (!follow || !h.in_mission)
            return;
        if (!synced_) {
            yaw = h.cam_angle;
            center = center_for(glm::vec2(h.cam_x, h.cam_z), anchor);
            synced_ = true;
            return;
        }
        // the table turns with the desktop's camera, about the point in
        // front of the viewer, the shorter way
        float k = 1.0f - std::exp(-dt * 4.0f);
        float da = std::remainder(h.cam_angle - yaw, 6.2831853f);
        if (std::fabs(da) > 1e-4f)
            turn_at_anchor(da * k);
        glm::vec3 target = center_for(glm::vec2(h.cam_x, h.cam_z), anchor);
        center = glm::mix(center, target, k);
        clamp_center();
    }

    // The table's turn as a game camera angle, 0..2047.
    int32_t game_angle() const
    {
        float a = std::remainder(yaw, 6.2831853f);
        if (a < 0)
            a += 6.2831853f;
        return (int32_t)std::lround(a / 6.2831853f * 2048.0f) & 2047;
    }
    // Lines the table up with the desktop's view again at the next
    // follow_camera(), as at the start (a new table placement).
    void resync() { synced_ = false; }

    void pan(float dx, float dz)
    {
        // input in table space; rotate into map space
        float c = std::cos(-yaw), s = std::sin(-yaw);
        center.x += (c * dx - s * dz) * half_map;
        center.z -= (s * dx + c * dz) * half_map; // map Z is flipped
        clamp_center();
    }

    void zoom(float f) { half_map = std::clamp(half_map * f, 1024.0f, 17000.0f); }

    // Zooming and turning about the anchor: the point in front of the
    // viewer stays where it is.
    void zoom_at_anchor(float f)
    {
        glm::vec2 m = map_at(anchor);
        zoom(f);
        center = center_for(m, anchor);
        clamp_center();
    }
    void turn_at_anchor(float da)
    {
        glm::vec2 m = map_at(anchor);
        yaw += da;
        center = center_for(m, anchor);
        clamp_center();
    }

    void clamp_center()
    {
        center.x = std::clamp(center.x, 0.0f, 32768.0f);
        center.z = std::clamp(center.z, 0.0f, 32768.0f);
    }

    // map height at the table top: just under the lowest ground, as roads
    // and water lie below the level of the streets
    float ground_offset = -64.0f;

private:
    bool synced_ = false;
};
