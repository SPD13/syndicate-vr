// Syndicate Wars VR viewer: the table view: which part of the map is shown, and how.
#pragma once

#include "common.h"

/******************************************************************************/
// Table view state: which part of the map is shown, and how

struct TableView {
    glm::vec3 center{16384, 0, 16384}; // map units, the point at table centre
    float half_map = 5120;              // map units from centre to table edge
    float yaw = 0;                      // table rotation, radians
    bool follow = true;
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

    void follow_camera(const SwvHeader &h, float dt)
    {
        if (!follow || !h.in_mission)
            return;
        glm::vec3 target(h.cam_x, 0, h.cam_z);
        float k = 1.0f - std::exp(-dt * 4.0f);
        center = glm::mix(center, target, k);
    }

    void pan(float dx, float dz)
    {
        // input in table space; rotate into map space
        float c = std::cos(-yaw), s = std::sin(-yaw);
        center.x += (c * dx - s * dz) * half_map;
        center.z -= (s * dx + c * dz) * half_map; // map Z is flipped
        center.x = std::clamp(center.x, 0.0f, 32768.0f);
        center.z = std::clamp(center.z, 0.0f, 32768.0f);
    }

    void zoom(float f) { half_map = std::clamp(half_map * f, 1024.0f, 17000.0f); }

    // map height at the table top: just under the lowest ground, as roads
    // and water lie below the level of the streets
    float ground_offset = -64.0f;
};
