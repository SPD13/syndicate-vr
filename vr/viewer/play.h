// Syndicate Wars VR viewer: playing from the table: commands to the game core, picking.
#pragma once

#include "render_classic.h"

/******************************************************************************/
// Playing from the table: pointers, commands to the game, rings and panels


// Sends commands to the game of this process: through sink (the world
// source's command()), or swcore_command() without one.
class CommandSender {
public:
    std::function<void(const SwcCommand &)> sink;

    void send(uint16_t type, int16_t thing = 0, int32_t x = 0, int32_t z = 0, uint32_t arg = 0)
    {
        struct SwcCommand c{};
        c.type = type;
        c.thing = thing;
        c.x = x;
        c.z = z;
        c.arg = arg;
        if (sink)
            sink(c);
        else
            swcore_command(&c);
        logf("command %u thing %d at %d,%d arg %u", type, thing, x, z, arg);
    }
};

struct Ray {
    glm::vec3 o{0}, d{0, 0, -1};
    bool valid = false;
};

struct Hit {
    enum Kind { NONE, GROUND, PERSON, VEHICLE } kind = NONE;
    uint16_t thing = 0;
    glm::vec3 p{0}; // map units
    float t = 1e30f; // distance along the ray, map units
};

// What can be pointed at on the map.
class Picker {
public:
    // Ground heights at tile corners, from the terrain at the start of the
    // static vertices (6 per tile, corner order 0,1,2,0,2,3).
    void update_ground(const WorldSnapshot &snap)
    {
        const int n = 127;
        if (snap.static_verts.size() < (size_t)n * n * 6)
            return;
        alt_.assign(128 * 128, 0.0f);
        for (int tz = 0; tz < n; tz++)
            for (int tx = 0; tx < n; tx++) {
                const SwvVert *v = &snap.static_verts[(size_t)(tz * n + tx) * 6];
                alt_[tz * 128 + tx] = v[0].y;
                alt_[tz * 128 + tx + 1] = v[1].y;
                alt_[(tz + 1) * 128 + tx + 1] = v[2].y;
                alt_[(tz + 1) * 128 + tx] = v[5].y;
            }
    }

    // Vehicle centres and sizes from their moving geometry.
    void update_vehicles(const WorldSnapshot &snap)
    {
        vehicles_.clear();
        for (const SwvDynGroup &g : snap.dyn_groups) {
            if ((g.id >> 28) != 0 || g.first + g.count > snap.dyn_verts.size() || g.count == 0)
                continue;
            glm::vec3 lo(1e9f), hi(-1e9f);
            for (uint32_t i = 0; i < g.count; i++) {
                const SwvVert &v = snap.dyn_verts[g.first + i];
                lo = glm::min(lo, glm::vec3(v.x, v.y, v.z));
                hi = glm::max(hi, glm::vec3(v.x, v.y, v.z));
            }
            vehicles_.push_back({(uint16_t)g.id, (lo + hi) * 0.5f,
                                 std::max(80.0f, glm::length(hi - lo) * 0.4f)});
        }
    }

    float ground_at(float x, float z) const
    {
        if (alt_.empty())
            return 0;
        float fx = std::clamp(x / 256.0f, 0.0f, 126.99f), fz = std::clamp(z / 256.0f, 0.0f, 126.99f);
        int tx = (int)fx, tz = (int)fz;
        fx -= tx;
        fz -= tz;
        float a = alt_[tz * 128 + tx], b = alt_[tz * 128 + tx + 1];
        float c = alt_[(tz + 1) * 128 + tx], d = alt_[(tz + 1) * 128 + tx + 1];
        return (a * (1 - fx) + b * fx) * (1 - fz) + (c * (1 - fx) + d * fx) * fz;
    }

    struct Person {
        uint16_t thing;
        uint8_t flags;
        glm::vec3 p;
    };

    // With the fog of war: whether a vehicle can be seen, to point at it.
    std::function<bool(glm::vec3)> visible;

    // Nearest of: people (upright cylinders), vehicles (spheres), ground.
    Hit pick(const Ray &r, const std::vector<Person> &people, glm::vec3 clip_center,
             float clip_half) const
    {
        Hit best;
        if (!r.valid)
            return best;
        for (const Person &pp : people) {
            if (pp.flags & SwvTF_HIDDEN)
                continue;
            // closest approach to the vertical axis through the person
            glm::vec2 o2(r.o.x - pp.p.x, r.o.z - pp.p.z), d2(r.d.x, r.d.z);
            float dd = glm::dot(d2, d2);
            if (dd < 1e-6f)
                continue;
            float t = -glm::dot(o2, d2) / dd;
            if (t < 0)
                continue;
            glm::vec3 q = r.o + r.d * t;
            float dist = glm::length(glm::vec2(q.x - pp.p.x, q.z - pp.p.z));
            float h = q.y - pp.p.y;
            if (dist < 70.0f && h > -20.0f && h < 240.0f && t < best.t) {
                best.kind = Hit::PERSON;
                best.thing = pp.thing;
                best.p = pp.p;
                best.t = t;
            }
        }
        for (const Vehicle &v : vehicles_) {
            if (visible && !visible(v.c))
                continue;
            glm::vec3 oc = r.o - v.c;
            float b = glm::dot(oc, r.d), c = glm::dot(oc, oc) - v.r * v.r;
            float disc = b * b - c;
            if (disc < 0)
                continue;
            float t = -b - std::sqrt(disc);
            if (t > 0 && t < best.t) {
                best.kind = Hit::VEHICLE;
                best.thing = v.thing;
                best.p = v.c;
                best.t = t;
            }
        }
        // march the ray down to the ground, inside the table
        float step = 24.0f;
        for (float t = 0; t < 80000.0f && t < best.t; t += step) {
            glm::vec3 q = r.o + r.d * t;
            if (std::fabs(q.x - clip_center.x) > clip_half || std::fabs(q.z - clip_center.z) > clip_half) {
                if (q.y < -2000.0f)
                    break;
                continue;
            }
            if (q.y <= ground_at(q.x, q.z)) {
                best.kind = Hit::GROUND;
                best.p = glm::vec3(q.x, ground_at(q.x, q.z), q.z);
                best.t = t;
                break;
            }
        }
        return best;
    }

private:
    struct Vehicle {
        uint16_t thing;
        glm::vec3 c;
        float r;
    };
    std::vector<float> alt_;
    std::vector<Vehicle> vehicles_;
};

// Unlit coloured geometry in world space (meters): text, panels, pointers.
