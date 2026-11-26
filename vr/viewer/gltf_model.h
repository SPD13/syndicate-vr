// Syndicate Wars VR viewer: glTF models (cgltf): loading, GPU buffers and
// skeletal animation, for the upgraded look.
//
// Loading is split: gltf_load() parses the file and decodes the base colour
// textures into CPU arrays (it can run on a worker thread), and
// GltfModel::upload() makes the GL objects on the GL thread. Only the base
// colour texture of each material is used, scaled down to at most
// kGltfMaxTexture texels a side.
#pragma once

#include "common.h"
#include "third_party/cgltf.h"
#include "third_party/stb_image.h"

#include <glm/gtc/quaternion.hpp>

static constexpr int kGltfMaxTexture = 1024;

struct GltfNode {
    std::string name;
    int parent = -1;
    glm::vec3 t{0.0f}, s{1.0f};
    glm::quat r{1.0f, 0.0f, 0.0f, 0.0f};
    int mesh = -1;
    int skin = -1;
};

struct GltfClip {
    std::string name;
    float duration = 0;
    struct Channel {
        int node = -1;
        int path = 0; // 0 translation, 1 rotation, 2 scale
        bool step = false;
        std::vector<float> times, values;
    };
    std::vector<Channel> channels;
};

struct GltfPrimCpu {
    std::vector<float> pos, nrm, uv, wgt;
    std::vector<uint16_t> jnt;
    std::vector<uint32_t> idx;
    int image = -1;
};

struct GltfImageCpu {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
};

// What gltf_load() reads; vertex and image arrays are dropped by upload().
struct GltfModelData {
    std::vector<GltfNode> nodes;
    std::vector<std::vector<GltfPrimCpu>> meshes;
    std::vector<GltfImageCpu> images;
    std::vector<int> joints; // skin 0: node of each joint
    std::vector<glm::mat4> inv_bind;
    std::vector<GltfClip> clips;
    bool ok = false;
    std::string error;
};

// Halves an image until it fits max texels a side.
static void gltf_shrink(GltfImageCpu &im, int max)
{
    while (im.w > max || im.h > max) {
        int w = std::max(1, im.w / 2), h = std::max(1, im.h / 2);
        std::vector<uint8_t> out((size_t)w * h * 4);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int c = 0; c < 4; c++) {
                    int sx = std::min(2 * x, im.w - 1), sy = std::min(2 * y, im.h - 1);
                    int sx1 = std::min(sx + 1, im.w - 1), sy1 = std::min(sy + 1, im.h - 1);
                    int v = im.rgba[((size_t)sy * im.w + sx) * 4 + c] + im.rgba[((size_t)sy * im.w + sx1) * 4 + c] +
                            im.rgba[((size_t)sy1 * im.w + sx) * 4 + c] + im.rgba[((size_t)sy1 * im.w + sx1) * 4 + c];
                    out[((size_t)y * w + x) * 4 + c] = (uint8_t)(v / 4);
                }
        im.w = w;
        im.h = h;
        im.rgba.swap(out);
    }
}

static std::vector<float> gltf_floats(const cgltf_accessor *a)
{
    std::vector<float> v;
    if (a == nullptr)
        return v;
    v.resize(cgltf_accessor_unpack_floats(a, nullptr, 0));
    cgltf_accessor_unpack_floats(a, v.data(), v.size());
    return v;
}

/** Reads a GLB. With meshes false, only the node tree (no buffers are
 * read: cheap even for a large file). */
static bool gltf_load(const std::string &path, GltfModelData &out, bool meshes = true)
{
    cgltf_options opt{};
    cgltf_data *d = nullptr;
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success) {
        out.error = "cannot parse " + path;
        return false;
    }
    if (meshes && cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        cgltf_free(d);
        out.error = "cannot read the buffers of " + path;
        return false;
    }
    out.nodes.resize(d->nodes_count);
    for (size_t i = 0; i < d->nodes_count; i++) {
        const cgltf_node *n = &d->nodes[i];
        GltfNode &g = out.nodes[i];
        g.name = n->name ? n->name : "";
        g.parent = n->parent ? (int)(n->parent - d->nodes) : -1;
        if (n->has_matrix) {
            glm::mat4 m = glm::make_mat4(n->matrix);
            g.t = glm::vec3(m[3]);
            g.s = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
            glm::mat3 rm(glm::vec3(m[0]) / g.s.x, glm::vec3(m[1]) / g.s.y, glm::vec3(m[2]) / g.s.z);
            g.r = glm::quat_cast(rm);
        }
        if (n->has_translation)
            g.t = glm::make_vec3(n->translation);
        if (n->has_rotation)
            g.r = glm::quat(n->rotation[3], n->rotation[0], n->rotation[1], n->rotation[2]);
        if (n->has_scale)
            g.s = glm::make_vec3(n->scale);
        g.mesh = n->mesh ? (int)(n->mesh - d->meshes) : -1;
        g.skin = n->skin ? (int)(n->skin - d->skins) : -1;
    }
    if (!meshes) {
        cgltf_free(d);
        out.ok = true;
        return true;
    }

    // base colour textures
    std::vector<int> image_slot(d->images_count, -1);
    auto image_of = [&](const cgltf_material *m) -> int {
        if (m == nullptr || !m->has_pbr_metallic_roughness)
            return -1;
        const cgltf_texture *tx = m->pbr_metallic_roughness.base_color_texture.texture;
        if (tx == nullptr || tx->image == nullptr)
            return -1;
        size_t ii = tx->image - d->images;
        if (image_slot[ii] >= 0)
            return image_slot[ii];
        const cgltf_buffer_view *bv = tx->image->buffer_view;
        if (bv == nullptr || bv->buffer->data == nullptr)
            return -1;
        int w, h, ch;
        const stbi_uc *src = (const stbi_uc *)bv->buffer->data + bv->offset;
        stbi_uc *px = stbi_load_from_memory(src, (int)bv->size, &w, &h, &ch, 4);
        if (px == nullptr)
            return -1;
        GltfImageCpu im;
        im.w = w;
        im.h = h;
        im.rgba.assign(px, px + (size_t)w * h * 4);
        stbi_image_free(px);
        gltf_shrink(im, kGltfMaxTexture);
        out.images.push_back(std::move(im));
        image_slot[ii] = (int)out.images.size() - 1;
        return image_slot[ii];
    };

    out.meshes.resize(d->meshes_count);
    for (size_t mi = 0; mi < d->meshes_count; mi++) {
        const cgltf_mesh *m = &d->meshes[mi];
        for (size_t pi = 0; pi < m->primitives_count; pi++) {
            const cgltf_primitive *p = &m->primitives[pi];
            if (p->type != cgltf_primitive_type_triangles)
                continue;
            GltfPrimCpu c;
            const cgltf_accessor *joints = nullptr;
            for (size_t ai = 0; ai < p->attributes_count; ai++) {
                const cgltf_attribute &at = p->attributes[ai];
                if (at.type == cgltf_attribute_type_position)
                    c.pos = gltf_floats(at.data);
                else if (at.type == cgltf_attribute_type_normal)
                    c.nrm = gltf_floats(at.data);
                else if (at.type == cgltf_attribute_type_texcoord && at.index == 0)
                    c.uv = gltf_floats(at.data);
                else if (at.type == cgltf_attribute_type_weights && at.index == 0)
                    c.wgt = gltf_floats(at.data);
                else if (at.type == cgltf_attribute_type_joints && at.index == 0)
                    joints = at.data;
            }
            size_t nv = c.pos.size() / 3;
            if (nv == 0)
                continue;
            if (joints != nullptr && c.wgt.size() == nv * 4) {
                c.jnt.resize(nv * 4);
                for (size_t k = 0; k < nv; k++) {
                    cgltf_uint u[4] = {0, 0, 0, 0};
                    cgltf_accessor_read_uint(joints, k, u, 4);
                    for (int q = 0; q < 4; q++)
                        c.jnt[k * 4 + q] = (uint16_t)u[q];
                }
            } else {
                c.wgt.clear();
            }
            if (c.nrm.size() != nv * 3)
                c.nrm.assign(nv * 3, 0.0f);
            if (c.uv.size() != nv * 2)
                c.uv.assign(nv * 2, 0.0f);
            if (p->indices) {
                c.idx.resize(p->indices->count);
                for (size_t k = 0; k < p->indices->count; k++)
                    c.idx[k] = (uint32_t)cgltf_accessor_read_index(p->indices, k);
            } else {
                c.idx.resize(nv);
                for (size_t k = 0; k < nv; k++)
                    c.idx[k] = (uint32_t)k;
            }
            c.image = image_of(p->material);
            out.meshes[mi].push_back(std::move(c));
        }
    }

    if (d->skins_count > 0) {
        const cgltf_skin *s = &d->skins[0];
        for (size_t j = 0; j < s->joints_count; j++)
            out.joints.push_back((int)(s->joints[j] - d->nodes));
        std::vector<float> ib = gltf_floats(s->inverse_bind_matrices);
        out.inv_bind.assign(s->joints_count, glm::mat4(1.0f));
        for (size_t j = 0; j < s->joints_count && (j + 1) * 16 <= ib.size(); j++)
            out.inv_bind[j] = glm::make_mat4(&ib[j * 16]);
    }

    for (size_t ai = 0; ai < d->animations_count; ai++) {
        const cgltf_animation *a = &d->animations[ai];
        GltfClip clip;
        clip.name = a->name ? a->name : "";
        for (size_t ci = 0; ci < a->channels_count; ci++) {
            const cgltf_animation_channel &ch = a->channels[ci];
            if (ch.target_node == nullptr)
                continue;
            GltfClip::Channel c;
            c.node = (int)(ch.target_node - d->nodes);
            if (ch.target_path == cgltf_animation_path_type_translation)
                c.path = 0;
            else if (ch.target_path == cgltf_animation_path_type_rotation)
                c.path = 1;
            else if (ch.target_path == cgltf_animation_path_type_scale)
                c.path = 2;
            else
                continue;
            c.step = ch.sampler->interpolation == cgltf_interpolation_type_step;
            c.times = gltf_floats(ch.sampler->input);
            std::vector<float> v = gltf_floats(ch.sampler->output);
            int width = c.path == 1 ? 4 : 3;
            if (ch.sampler->interpolation == cgltf_interpolation_type_cubic_spline) {
                // in-tangent, value, out-tangent: keep the values
                for (size_t k = 0; k < c.times.size(); k++)
                    c.values.insert(c.values.end(), v.begin() + (k * 3 + 1) * width,
                                    v.begin() + (k * 3 + 2) * width);
            } else {
                c.values = std::move(v);
            }
            if (c.times.empty() || c.values.size() < c.times.size() * width)
                continue;
            clip.duration = std::max(clip.duration, c.times.back());
            clip.channels.push_back(std::move(c));
        }
        out.clips.push_back(std::move(clip));
    }
    cgltf_free(d);
    out.ok = true;
    return true;
}

// The pose of a model: local translation, rotation and scale per node.
struct GltfPose {
    std::vector<glm::vec3> t, s;
    std::vector<glm::quat> r;
};

class GltfModel {
public:
    struct Prim {
        GLuint vao = 0, vbo[5] = {0, 0, 0, 0, 0}, ebo = 0;
        GLsizei count = 0;
        GLuint tex = 0;
        bool skinned = false;
    };

    GltfModelData data;
    std::vector<std::vector<Prim>> meshes;
    std::vector<GLuint> textures;
    std::vector<int> order; // nodes, parents first
    std::vector<glm::mat4> rest; // world matrix of every node at rest
    glm::vec3 bmin{0.0f}, bmax{0.0f}; // bounds of the meshes at rest

    /** Makes the GL objects (GL thread) and frees the CPU arrays. */
    void upload()
    {
        // parents before children; the rest pose and the bounds
        std::vector<int> depth(data.nodes.size(), 0);
        for (size_t i = 0; i < data.nodes.size(); i++)
            for (int p = data.nodes[i].parent; p >= 0; p = data.nodes[p].parent)
                depth[i]++;
        order.resize(data.nodes.size());
        for (size_t i = 0; i < order.size(); i++)
            order[i] = (int)i;
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return depth[a] < depth[b]; });
        {
            GltfPose p;
            rest_pose(p);
            globals(p, rest);
        }
        bool any = false;
        for (size_t n = 0; n < data.nodes.size(); n++) {
            int mi = data.nodes[n].mesh;
            if (mi < 0 || mi >= (int)data.meshes.size() || data.nodes[n].name.rfind("weapon_", 0) == 0)
                continue;
            for (const GltfPrimCpu &c : data.meshes[mi]) {
                glm::mat4 m = c.jnt.empty() ? rest[n] : glm::mat4(1.0f);
                for (size_t k = 0; k + 2 < c.pos.size(); k += 3) {
                    glm::vec3 v = glm::vec3(m * glm::vec4(c.pos[k], c.pos[k + 1], c.pos[k + 2], 1.0f));
                    bmin = any ? glm::min(bmin, v) : v;
                    bmax = any ? glm::max(bmax, v) : v;
                    any = true;
                }
            }
        }
        for (GltfImageCpu &im : data.images) {
            GLuint tx;
            glGenTextures(1, &tx);
            glBindTexture(GL_TEXTURE_2D, tx);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, im.w, im.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, im.rgba.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            textures.push_back(tx);
            std::vector<uint8_t>().swap(im.rgba);
        }
        meshes.resize(data.meshes.size());
        for (size_t mi = 0; mi < data.meshes.size(); mi++)
            for (GltfPrimCpu &c : data.meshes[mi]) {
                Prim p;
                p.skinned = !c.jnt.empty();
                glGenVertexArrays(1, &p.vao);
                glBindVertexArray(p.vao);
                glGenBuffers(5, p.vbo);
                auto attrib = [&](int slot, const void *src, size_t bytes, int n) {
                    glBindBuffer(GL_ARRAY_BUFFER, p.vbo[slot]);
                    glBufferData(GL_ARRAY_BUFFER, bytes, src, GL_STATIC_DRAW);
                    glEnableVertexAttribArray(slot);
                    glVertexAttribPointer(slot, n, GL_FLOAT, GL_FALSE, 0, nullptr);
                };
                attrib(0, c.pos.data(), c.pos.size() * 4, 3);
                attrib(1, c.nrm.data(), c.nrm.size() * 4, 3);
                attrib(2, c.uv.data(), c.uv.size() * 4, 2);
                if (p.skinned) {
                    glBindBuffer(GL_ARRAY_BUFFER, p.vbo[3]);
                    glBufferData(GL_ARRAY_BUFFER, c.jnt.size() * 2, c.jnt.data(), GL_STATIC_DRAW);
                    glEnableVertexAttribArray(3);
                    glVertexAttribIPointer(3, 4, GL_UNSIGNED_SHORT, 0, nullptr);
                    attrib(4, c.wgt.data(), c.wgt.size() * 4, 4);
                } else {
                    glDisableVertexAttribArray(3);
                    glVertexAttribI4ui(3, 0, 0, 0, 0);
                    glDisableVertexAttribArray(4);
                    glVertexAttrib4f(4, 0, 0, 0, 0);
                }
                glGenBuffers(1, &p.ebo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, p.ebo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, c.idx.size() * 4, c.idx.data(), GL_STATIC_DRAW);
                p.count = (GLsizei)c.idx.size();
                p.tex = c.image >= 0 ? textures[c.image] : 0;
                glBindVertexArray(0);
                meshes[mi].push_back(p);
            }
        std::vector<std::vector<GltfPrimCpu>>().swap(data.meshes);
        std::vector<GltfImageCpu>().swap(data.images);
        for (size_t i = 0; i < data.nodes.size(); i++)
            node_index_[data.nodes[i].name] = (int)i;
        for (size_t i = 0; i < data.clips.size(); i++)
            clip_index_[data.clips[i].name] = (int)i;
    }

    int node(const std::string &name) const
    {
        auto it = node_index_.find(name);
        return it == node_index_.end() ? -1 : it->second;
    }
    int clip(const std::string &name) const
    {
        auto it = clip_index_.find(name);
        return it == clip_index_.end() ? -1 : it->second;
    }

    void rest_pose(GltfPose &p) const
    {
        size_t n = data.nodes.size();
        p.t.resize(n);
        p.r.resize(n);
        p.s.resize(n);
        for (size_t i = 0; i < n; i++) {
            p.t[i] = data.nodes[i].t;
            p.r[i] = data.nodes[i].r;
            p.s[i] = data.nodes[i].s;
        }
    }

    /** Sets the animated nodes of the pose to the clip at time `time`. */
    void sample(int ci, float time, GltfPose &p) const
    {
        if (ci < 0)
            return;
        for (const GltfClip::Channel &c : data.clips[ci].channels) {
            const std::vector<float> &tm = c.times;
            size_t k = std::upper_bound(tm.begin(), tm.end(), time) - tm.begin();
            size_t k0 = k == 0 ? 0 : k - 1, k1 = std::min(k, tm.size() - 1);
            float f = 0;
            if (k1 != k0 && !c.step)
                f = std::clamp((time - tm[k0]) / std::max(tm[k1] - tm[k0], 1e-6f), 0.0f, 1.0f);
            if (c.path == 1) {
                const float *a = &c.values[k0 * 4], *b = &c.values[k1 * 4];
                glm::quat qa(a[3], a[0], a[1], a[2]), qb(b[3], b[0], b[1], b[2]);
                p.r[c.node] = glm::normalize(glm::slerp(qa, qb, f));
            } else {
                glm::vec3 va = glm::make_vec3(&c.values[k0 * 3]), vb = glm::make_vec3(&c.values[k1 * 3]);
                (c.path == 0 ? p.t[c.node] : p.s[c.node]) = glm::mix(va, vb, f);
            }
        }
    }

    /** World matrix of every node (model space). */
    void globals(const GltfPose &p, std::vector<glm::mat4> &g) const
    {
        g.resize(data.nodes.size());
        for (int i : order) {
            glm::mat4 l = glm::translate(glm::mat4(1.0f), p.t[i]) * glm::mat4_cast(p.r[i]) *
                          glm::scale(glm::mat4(1.0f), p.s[i]);
            int par = data.nodes[i].parent;
            g[i] = par >= 0 ? g[par] * l : l;
        }
    }

private:
    std::unordered_map<std::string, int> node_index_, clip_index_;
};

// Blends pose b into a by weight w (0 keeps a).
static void gltf_blend(GltfPose &a, const GltfPose &b, float w)
{
    for (size_t i = 0; i < a.t.size(); i++) {
        a.t[i] = glm::mix(a.t[i], b.t[i], w);
        a.s[i] = glm::mix(a.s[i], b.s[i], w);
        a.r[i] = glm::normalize(glm::slerp(a.r[i], b.r[i], w));
    }
}
