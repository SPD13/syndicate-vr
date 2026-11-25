// Syndicate Wars VR viewer: voxel figures carved from the game's directional sprites.
#pragma once

#include "render_classic.h"

/******************************************************************************/
// Voxel figures carved from the directional sprites
//
// A person's frame exists in 8 (dead bodies: 12) views around them, all from
// the game camera's 30.3 degree tilt. Carving a block of voxels down to the
// silhouette of every view gives a solid figure (its visual hull). The hull
// fills hollows which no silhouette shows, so it is then carved further by
// photo-consistency: a surface voxel which the views showing it disagree on
// the colour of is not on the real surface, and goes, as long as every
// silhouette stays covered. Surface voxels finally take the colour from the
// view which sees them best. Models are built on a worker thread.

// Game camera tilt, as used for sprites: cos and sin of 30.3 degrees.
static constexpr float kTiltCos = 0.8634f;
static constexpr float kTiltSin = 0.5045f;

// Projection of a point, relative to the figure's feet, into the sprite of a
// game camera with yaw a (radians), as transform_shpoint() at scale 256.
// Returns sprite pixel offsets from the anchor and the depth along the view.
static void game_project(float a, float x, float y, float z, float &sx, float &sy, float &depth)
{
    float ca = std::cos(a), sa = std::sin(a);
    float fa = ca * x - sa * z;
    float fb = sa * x + ca * z;
    float fc = kTiltCos * y + kTiltSin * fb;
    sx = fa / 8.0f;
    sy = -fc / 8.0f;
    depth = kTiltCos * fb - kTiltSin * y;
}

// What the builder needs from the snapshot, shared read-only with the worker.
struct SpriteData {
    std::vector<SwvSpriteRect> rects;
    std::vector<uint16_t> nstart;
    std::vector<SwvFrame> frames;
    std::vector<SwvElement> elements;
    std::vector<uint8_t> pages;
    uint32_t stride = 6;
    float rgb[256][3]{};
};

struct VoxelParams {
    int tolerance = 0;     // views a voxel may fall outside of and still stay
    bool dilate = false;   // treat pixels next to the silhouette as inside
    float photo = 0.0f;    // colour disagreement which carves, 0 disables
    int max_depth = 3;     // voxels into the silhouette hull it may carve
};

class VoxelModels {
public:
    struct Model {
        GLuint vao = 0, vbo = 0;
        GLsizei count = 0;
        float top = 0;   // height of the highest voxel, map units
        bool ok = false; // false while building, or if nothing usable was left
    };

    VoxelParams params;

    VoxelModels() { worker_ = std::thread([this] { work(); }); }

    ~VoxelModels()
    {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            quit_ = true;
        }
        cv_.notify_all();
        worker_.join();
    }

    // New sprites or palette: drop all models, build from the new data.
    void reset(const WorldSnapshot &snap, const Renderer &rend)
    {
        auto d = std::make_shared<SpriteData>();
        d->rects = snap.sprite_rects;
        d->nstart = snap.nstart;
        d->frames = snap.frames;
        d->elements = snap.elements;
        d->pages = snap.sprite_pages;
        d->stride = snap.sprite_stride;
        for (int c = 0; c < 256; c++)
            rend.palette_rgb(rend.palette(), (uint8_t)c, d->rgb[c]);
        std::lock_guard<std::mutex> lk(mtx_);
        data_ = d;
        generation_++;
        jobs_.clear();
        done_.clear();
        for (auto &kv : models_)
            if (kv.second.vbo) {
                glDeleteBuffers(1, &kv.second.vbo);
                glDeleteVertexArrays(1, &kv.second.vao);
            }
        models_.clear();
    }

    // Drop models after a parameter change.
    void rebuild()
    {
        std::shared_ptr<const SpriteData> d;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            d = data_;
        }
        if (!d)
            return;
        std::lock_guard<std::mutex> lk(mtx_);
        generation_++;
        jobs_.clear();
        done_.clear();
        for (auto &kv : models_)
            if (kv.second.vbo) {
                glDeleteBuffers(1, &kv.second.vbo);
                glDeleteVertexArrays(1, &kv.second.vao);
            }
        models_.clear();
    }

    // Uploads models the worker has finished; call once per frame.
    void collect()
    {
        std::vector<Done> done;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            done.swap(done_);
        }
        for (Done &d : done) {
            auto it = models_.find(d.key);
            if (it == models_.end() || d.verts.empty())
                continue;
            Model &m = it->second;
            glGenVertexArrays(1, &m.vao);
            glGenBuffers(1, &m.vbo);
            Renderer::setup_vao(m.vao, m.vbo);
            glBufferData(GL_ARRAY_BUFFER, d.verts.size() * sizeof(GpuVert), d.verts.data(),
                         GL_STATIC_DRAW);
            glBindVertexArray(0);
            m.count = (GLsizei)d.verts.size();
            m.top = d.top;
            m.ok = true;
        }
    }

    // Model for a person in its current animation state, or nullptr when it
    // is not available (yet). rot_steps receives the facing of the figure in
    // views, and n_views the number of views around it.
    const Model *get(const SwvThing &t, int &rot_steps, int &n_views)
    {
        if (t.anim_mode == SwvAM_DIR8) {
            n_views = 8;
            rot_steps = t.angle & 7;
        } else if (t.anim_mode == SwvAM_DIR12) {
            n_views = 12;
            rot_steps = (3 * t.angle >> 1) % 12;
        } else {
            return nullptr;
        }
        uint64_t key = (uint64_t)t.anim_start | ((uint64_t)t.subframe << 16) |
                       ((uint64_t)n_views << 24);
        for (int i = 0; i < 5; i++)
            key |= (uint64_t)(t.frv[i] & 7) << (32 + 3 * i);
        auto it = models_.find(key);
        if (it != models_.end())
            return it->second.ok ? &it->second : nullptr;
        models_[key] = Model();
        Job j;
        j.key = key;
        j.anim_start = t.anim_start;
        j.subframe = t.subframe;
        j.n_views = n_views;
        memcpy(j.frv, t.frv, 5);
        {
            std::lock_guard<std::mutex> lk(mtx_);
            j.generation = generation_;
            j.params = params;
            jobs_.push_back(j);
        }
        cv_.notify_one();
        return nullptr;
    }

private:
    struct Job {
        uint64_t key;
        uint32_t generation;
        uint16_t anim_start;
        uint8_t subframe;
        int n_views;
        uint8_t frv[5];
        VoxelParams params;
    };
    struct Done {
        uint64_t key;
        std::vector<GpuVert> verts;
        float top;
    };

    void work()
    {
        for (;;) {
            Job j;
            std::shared_ptr<const SpriteData> d;
            {
                std::unique_lock<std::mutex> lk(mtx_);
                cv_.wait(lk, [this] { return quit_ || !jobs_.empty(); });
                if (quit_)
                    return;
                j = jobs_.front();
                jobs_.pop_front();
                d = data_;
            }
            Done r{j.key, {}, 0};
            if (d)
                build(*d, j, r);
            std::lock_guard<std::mutex> lk(mtx_);
            if (j.generation == generation_)
                done_.push_back(std::move(r));
        }
    }

    // Frame composed into an image of palette indices, -1 where empty.
    struct Image {
        static constexpr int W = 160, H = 160, OX = 80, OY = 120;
        std::vector<int16_t> px = std::vector<int16_t>(W * H, -1);
        int at(int x, int y) const
        {
            x += OX;
            y += OY;
            if (x < 0 || y < 0 || x >= W || y >= H)
                return -1;
            return px[y * W + x];
        }
    };

    static bool compose(const SpriteData &sd, int frm, const uint8_t *frv, Image &img)
    {
        if (frm <= 0 || frm >= (int)sd.frames.size())
            return false;
        bool any = false;
        int guard = 0;
        for (int el = sd.frames[frm].first_element;
             el > 0 && el < (int)sd.elements.size() && guard < 64;
             el = sd.elements[el].next, guard++) {
            const SwvElement &e = sd.elements[el];
            if (e.to_sprite == 0 || e.to_sprite % sd.stride != 0)
                continue;
            uint32_t si = e.to_sprite / sd.stride;
            if (si >= sd.rects.size())
                continue;
            int slot = (e.flags >> 4) & 0x1F;
            int ver = slot < 5 ? frv[slot] : 0;
            if (ver != ((e.flags >> 9) & 7))
                continue;
            const SwvSpriteRect &r = sd.rects[si];
            size_t page_off = (size_t)r.page * SWV_TMAP_DIM * SWV_TMAP_DIM;
            if ((page_off + SWV_TMAP_DIM * SWV_TMAP_DIM) * 2 > sd.pages.size())
                continue;
            for (int v = 0; v < r.h; v++)
                for (int u = 0; u < r.w; u++) {
                    size_t ti = page_off + (size_t)(r.y + v) * SWV_TMAP_DIM + (r.x + u);
                    if (sd.pages[ti * 2 + 1] == 0)
                        continue;
                    int su = (e.flags & 0x01) ? (r.w - 1 - u) : u;
                    int sv = (e.flags & 0x02) ? (r.h - 1 - v) : v;
                    int x = (e.x >> 1) + su + Image::OX, y = (e.y >> 1) + sv + Image::OY;
                    if (x < 0 || y < 0 || x >= Image::W || y >= Image::H)
                        continue;
                    // later elements are drawn over earlier ones
                    img.px[y * Image::W + x] = sd.pages[ti * 2];
                    any = true;
                }
        }
        return any;
    }

    static void build(const SpriteData &sd, const Job &j, Done &r)
    {
        const int nv = j.n_views;
        const VoxelParams &prm = j.params;
        // views of the figure facing angle 0: view d is what a camera with
        // yaw step (n - d) % n sees, as the game picks the frame
        std::vector<Image> imgs(nv);
        std::vector<float> yaw(nv);
        for (int d = 0; d < nv; d++) {
            int idx = j.anim_start + 1 + d;
            if (idx >= (int)sd.nstart.size())
                return;
            if (!compose(sd, sd.nstart[idx] + j.subframe, j.frv, imgs[d]))
                return;
            yaw[d] = ((nv - d) % nv) * 6.2831853f / nv;
        }

        // voxel grid around the feet, one sprite pixel per voxel
        const float vs = 8.0f;
        const int NX = 36, NY = 52, NZ = 36, N = NX * NY * NZ;
        const float x0 = -NX / 2 * vs, y0 = -6 * vs, z0 = -NZ / 2 * vs;
        auto vi = [&](int i, int jj, int k) { return (jj * NZ + k) * NX + i; };
        auto center = [&](int v) {
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            return glm::vec3(x0 + (i + 0.5f) * vs, y0 + (jj + 0.5f) * vs, z0 + (k + 0.5f) * vs);
        };

        // projection of every voxel into every view
        std::vector<int32_t> pix((size_t)N * nv);
        std::vector<float> dep((size_t)N * nv);
        for (int v = 0; v < N; v++) {
            glm::vec3 c = center(v);
            for (int d = 0; d < nv; d++) {
                float sx, sy, z;
                game_project(yaw[d], c.x, c.y, c.z, sx, sy, z);
                int px = (int)std::floor(sx) + Image::OX, py = (int)std::floor(sy) + Image::OY;
                pix[(size_t)v * nv + d] = (px < 0 || py < 0 || px >= Image::W || py >= Image::H)
                                              ? -1 : py * Image::W + px;
                dep[(size_t)v * nv + d] = z;
            }
        }
        auto colour_at = [&](int d, int p) { return p < 0 ? -1 : (int)imgs[d].px[p]; };
        auto inside = [&](int d, int p) {
            if (colour_at(d, p) >= 0)
                return true;
            if (!prm.dilate || p < 0)
                return false;
            int px = p % Image::W, py = p / Image::W;
            return imgs[d].at(px + 1 - Image::OX, py - Image::OY) >= 0 ||
                   imgs[d].at(px - 1 - Image::OX, py - Image::OY) >= 0 ||
                   imgs[d].at(px - Image::OX, py + 1 - Image::OY) >= 0 ||
                   imgs[d].at(px - Image::OX, py - 1 - Image::OY) >= 0;
        };

        // silhouette carving
        std::vector<uint8_t> solid(N, 0);
        int n_solid = 0;
        for (int v = 0; v < N; v++) {
            int misses = 0;
            for (int d = 0; d < nv && misses <= prm.tolerance; d++)
                if (!inside(d, pix[(size_t)v * nv + d]))
                    misses++;
            if (misses <= prm.tolerance) {
                solid[v] = 1;
                n_solid++;
            }
        }
        if (n_solid == 0)
            return;

        static const int nb[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        auto is_solid = [&](int i, int jj, int k) {
            if (i < 0 || jj < 0 || k < 0 || i >= NX || jj >= NY || k >= NZ)
                return false;
            return solid[vi(i, jj, k)] != 0;
        };
        auto is_surface = [&](int v) {
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2]))
                    return true;
            return false;
        };

        // nearest voxel depth per pixel in each view
        std::vector<float> zbuf((size_t)nv * Image::W * Image::H);
        auto build_zbuf = [&]() {
            std::fill(zbuf.begin(), zbuf.end(), 1e9f);
            for (int v = 0; v < N; v++) {
                if (!solid[v])
                    continue;
                for (int d = 0; d < nv; d++) {
                    int p = pix[(size_t)v * nv + d];
                    if (p < 0)
                        continue;
                    float &z = zbuf[(size_t)d * Image::W * Image::H + p];
                    z = std::min(z, dep[(size_t)v * nv + d]);
                }
            }
        };
        auto visible = [&](int v, int d) {
            int p = pix[(size_t)v * nv + d];
            return p >= 0 && dep[(size_t)v * nv + d] <= zbuf[(size_t)d * Image::W * Image::H + p] + 0.5f * vs;
        };

        // photo-consistency carving, keeping every silhouette pixel covered;
        // colours are averaged over 3x3 pixels, as views differ in detail
        if (prm.photo > 0) {
            // how deep each voxel lies inside the silhouette hull, in voxels
            std::vector<uint8_t> depth_in(N, 255);
            {
                std::deque<int> q;
                for (int v = 0; v < N; v++)
                    if (solid[v] && is_surface(v)) {
                        depth_in[v] = 1;
                        q.push_back(v);
                    }
                while (!q.empty()) {
                    int v = q.front();
                    q.pop_front();
                    int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
                    for (auto &o : nb) {
                        int a = i + o[0], b = jj + o[1], c = k + o[2];
                        if (!is_solid(a, b, c))
                            continue;
                        int w = vi(a, b, c);
                        if (depth_in[w] > depth_in[v] + 1) {
                            depth_in[w] = depth_in[v] + 1;
                            q.push_back(w);
                        }
                    }
                }
            }
            // removing v keeps its solid neighbours connected among themselves
            // (26-connectivity within the 3x3x3 block), so carving never
            // splits the figure into floating pieces
            auto removal_keeps_connected = [&](int v) {
                int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
                uint8_t cell[27];
                int first = -1, count = 0;
                for (int n = 0; n < 27; n++) {
                    int dx = n % 3 - 1, dy = n / 9 - 1, dz = (n / 3) % 3 - 1;
                    cell[n] = (n != 13) && is_solid(i + dx, jj + dy, k + dz);
                    if (cell[n]) {
                        count++;
                        if (first < 0)
                            first = n;
                    }
                }
                if (count == 0)
                    return true;
                uint8_t seen[27] = {0};
                int stack[27], sp = 0, reached = 0;
                stack[sp++] = first;
                seen[first] = 1;
                while (sp > 0) {
                    int n = stack[--sp];
                    reached++;
                    int x = n % 3, y = n / 9, z = (n / 3) % 3;
                    for (int m = 0; m < 27; m++) {
                        if (!cell[m] || seen[m])
                            continue;
                        int mx = m % 3, my = m / 9, mz = (m / 3) % 3;
                        if (std::abs(mx - x) <= 1 && std::abs(my - y) <= 1 && std::abs(mz - z) <= 1) {
                            seen[m] = 1;
                            stack[sp++] = m;
                        }
                    }
                }
                return reached == count;
            };
            std::vector<glm::vec3> blur((size_t)nv * Image::W * Image::H, glm::vec3(0));
            for (int d = 0; d < nv; d++)
                for (int py = 1; py < Image::H - 1; py++)
                    for (int px = 1; px < Image::W - 1; px++) {
                        glm::vec3 sum(0);
                        int n = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++) {
                                int col = imgs[d].px[(py + dy) * Image::W + px + dx];
                                if (col < 0)
                                    continue;
                                sum += glm::vec3(sd.rgb[col][0], sd.rgb[col][1], sd.rgb[col][2]);
                                n++;
                            }
                        if (n > 0)
                            blur[(size_t)d * Image::W * Image::H + py * Image::W + px] = sum / (float)n;
                    }
            std::vector<int> cover((size_t)nv * Image::W * Image::H, 0);
            for (int v = 0; v < N; v++)
                if (solid[v])
                    for (int d = 0; d < nv; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0)
                            cover[(size_t)d * Image::W * Image::H + p]++;
                    }
            for (int iter = 0; iter < 24; iter++) {
                build_zbuf();
                std::vector<int> cand;
                for (int v = 0; v < N; v++) {
                    if (!solid[v] || !is_surface(v))
                        continue;
                    float lo[3] = {9, 9, 9}, hi[3] = {-9, -9, -9};
                    int seen = 0;
                    for (int d = 0; d < nv; d++) {
                        if (!visible(v, d))
                            continue;
                        int p = pix[(size_t)v * nv + d];
                        if (colour_at(d, p) < 0)
                            continue;
                        const glm::vec3 &c = blur[(size_t)d * Image::W * Image::H + p];
                        for (int k = 0; k < 3; k++) {
                            lo[k] = std::min(lo[k], c[k]);
                            hi[k] = std::max(hi[k], c[k]);
                        }
                        seen++;
                    }
                    if (seen < 2)
                        continue;
                    float spread = std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) +
                                             (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                                             (hi[2] - lo[2]) * (hi[2] - lo[2]));
                    if (spread > prm.photo)
                        cand.push_back(v);
                }
                int removed = 0;
                for (int v : cand) {
                    bool keep = depth_in[v] > prm.max_depth || !removal_keeps_connected(v);
                    for (int d = 0; d < nv && !keep; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0 && colour_at(d, p) >= 0 &&
                            cover[(size_t)d * Image::W * Image::H + p] <= 1)
                            keep = true;
                    }
                    if (keep)
                        continue;
                    solid[v] = 0;
                    removed++;
                    for (int d = 0; d < nv; d++) {
                        int p = pix[(size_t)v * nv + d];
                        if (p >= 0)
                            cover[(size_t)d * Image::W * Image::H + p]--;
                    }
                }
                if (removed == 0)
                    break;
            }
            // loose voxels with at most one face neighbour look like noise
            for (int v = 0; v < N; v++) {
                if (!solid[v])
                    continue;
                int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ), nbs = 0;
                for (auto &o : nb)
                    nbs += is_solid(i + o[0], jj + o[1], k + o[2]) ? 1 : 0;
                if (nbs <= 1)
                    solid[v] = 0;
            }
            // whatever still came loose: keep only the largest piece
            std::vector<int> comp(N, -1);
            int best_comp = -1, best_size = 0, n_comp = 0;
            for (int v = 0; v < N; v++) {
                if (!solid[v] || comp[v] >= 0)
                    continue;
                int size = 0;
                std::vector<int> stack{v};
                comp[v] = n_comp;
                while (!stack.empty()) {
                    int w = stack.back();
                    stack.pop_back();
                    size++;
                    int i = w % NX, k = (w / NX) % NZ, jj = w / (NX * NZ);
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dz = -1; dz <= 1; dz++)
                            for (int dx = -1; dx <= 1; dx++) {
                                if (!is_solid(i + dx, jj + dy, k + dz))
                                    continue;
                                int u = vi(i + dx, jj + dy, k + dz);
                                if (comp[u] < 0) {
                                    comp[u] = n_comp;
                                    stack.push_back(u);
                                }
                            }
                }
                if (size > best_size) {
                    best_size = size;
                    best_comp = n_comp;
                }
                n_comp++;
            }
            for (int v = 0; v < N; v++)
                if (solid[v] && comp[v] != best_comp)
                    solid[v] = 0;
        }

        // surface voxels as cubes, faces only towards empty neighbours
        build_zbuf();
        float top = 0;
        for (int v = 0; v < N; v++) {
            if (!solid[v])
                continue;
            int i = v % NX, k = (v / NX) % NZ, jj = v / (NX * NZ);
            glm::vec3 n(0);
            int open = 0;
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2])) {
                    n += glm::vec3(o[0], o[1], o[2]);
                    open++;
                }
            if (open == 0)
                continue;
            if (glm::length(n) > 0)
                n = glm::normalize(n);
            // colour from the view which sees it most head-on, among those
            // where nothing covers it; any view as a fallback
            int best = -1, fallback = -1;
            float best_score = -2, fb_score = -2;
            for (int d = 0; d < nv; d++) {
                int col = colour_at(d, pix[(size_t)v * nv + d]);
                if (col < 0)
                    continue;
                glm::vec3 to_cam(-kTiltCos * std::sin(yaw[d]), kTiltSin, -kTiltCos * std::cos(yaw[d]));
                float score = glm::dot(n, to_cam);
                if (visible(v, d) && score > best_score) {
                    best_score = score;
                    best = col;
                }
                if (score > fb_score) {
                    fb_score = score;
                    fallback = col;
                }
            }
            int col = best >= 0 ? best : fallback;
            if (col < 0)
                continue;
            glm::vec3 c = center(v);
            for (auto &o : nb)
                if (!is_solid(i + o[0], jj + o[1], k + o[2]))
                    add_face(r.verts, c, glm::vec3(o[0], o[1], o[2]), vs, sd.rgb[col]);
            top = std::max(top, c.y + vs * 0.5f);
        }
        r.top = top;
    }

    static void add_face(std::vector<GpuVert> &v, glm::vec3 c, glm::vec3 n, float vs, const float *rgb)
    {
        glm::vec3 u = (std::fabs(n.x) > 0.5f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 w = glm::cross(n, u);
        float h = vs * 0.5f;
        glm::vec3 fc = c + n * h;
        glm::vec3 q[4] = {fc - u * h - w * h, fc + u * h - w * h, fc + u * h + w * h, fc - u * h + w * h};
        static const int tri[6] = {0, 1, 2, 0, 2, 3};
        for (int k : tri) {
            GpuVert g{};
            g.pos[0] = q[k].x; g.pos[1] = q[k].y; g.pos[2] = q[k].z;
            g.rgb[0] = rgb[0]; g.rgb[1] = rgb[1]; g.rgb[2] = rgb[2];
            g.textured = 0.0f;
            v.push_back(g);
        }
    }

    std::unordered_map<uint64_t, Model> models_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<Job> jobs_;
    std::vector<Done> done_;
    std::shared_ptr<const SpriteData> data_;
    uint32_t generation_ = 0;
    bool quit_ = false;
};
