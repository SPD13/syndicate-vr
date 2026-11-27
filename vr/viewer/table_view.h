// Syndicate Wars VR viewer: the table view: which part of the map is shown, and how.
#pragma once

#include "common.h"

// Game camera tilt: cos and sin of 30.3 degrees.
static constexpr float kTiltCos = 0.8634f;
static constexpr float kTiltSin = 0.5045f;

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
        glm::vec3 target = center_for(glm::vec2(h.cam_x, h.cam_z), anchor);
        float k = 1.0f - std::exp(-dt * 4.0f);
        center = glm::mix(center, target, k);
        clamp_center();
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
