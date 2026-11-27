// Syndicate Wars VR viewer: the world source: the game core of this process (CoreWorld:
// swcore.h, swview.h) and the world of a turn read from its block.
#pragma once

#include "common.h"

/******************************************************************************/
// The world of a turn, read from the world block of libswarsview

struct WorldSnapshot {
    SwvHeader hdr{};
    std::vector<SwvVert> static_verts;
    std::vector<SwvVert> dyn_verts;
    std::vector<SwvThing> things;
    std::vector<SwvEffect> effects;
    std::vector<SwvLine> lines;
    std::vector<SwvDynGroup> dyn_groups;
    std::vector<SwvBar> bars;
    std::vector<SwvGlow> glows;
    std::vector<SwvShadow> shadows;
    SwvHud hud{};
    std::vector<uint8_t> tmap;
    uint8_t palette[768]{};
    // sprites and the animation tables composing them into frames
    std::vector<SwvSpriteRect> sprite_rects;
    std::vector<uint16_t> nstart;
    std::vector<SwvFrame> frames;
    std::vector<SwvElement> elements;
    std::vector<uint8_t> sprite_pages; // 2 bytes per texel: index, opacity
    uint32_t sprite_stride = 6;
    uint32_t static_rev = 0, tmap_rev = 0, sprite_rev = 0;
    bool static_changed = false, tmap_changed = false, sprite_changed = false;
    bool valid = false;
    // read from the core in this process, not from the block: the panels'
    // data and every person's state, for the VR HUD
    SwvHudInfo hud_info{};
    std::vector<SwvPersonRender> persons;
    // for the upgraded look: items, grenades and statics; weapons charging;
    // the effects started or ended in the turns played since the last poll
    std::vector<SwvProp> props;
    std::vector<SwvCharge> charges;
    std::vector<SwvFxEvent> fx_events;
};

class BlockReader {
public:
    /** Reads the block which CoreWorld fills. */
    void attach(const uint8_t *block) { block_ = block; }

    bool poll(WorldSnapshot &snap)
    {
        if (!block_)
            return false;
        auto *hdr = reinterpret_cast<const volatile SwvHeader *>(block_);
        for (int attempt = 0; attempt < 20; attempt++) {
            uint32_t seq1 = hdr->seq;
            if (seq1 & 1) {
                Sleep(0);
                continue;
            }
            MemoryBarrier();
            SwvHeader h;
            memcpy(&h, (const void *)block_, sizeof(h));
            if (h.magic != SWV_MAGIC || h.version != SWV_VERSION)
                return false;
            bool need_static = h.static_rev != snap.static_rev;
            bool need_tmap = h.tmap_rev != snap.tmap_rev;
            bool need_sprites = h.sprite_rev != snap.sprite_rev;
            h.n_static = std::min<uint32_t>(h.n_static, SWV_STATIC_VERTS_MAX);
            h.n_dyn = std::min<uint32_t>(h.n_dyn, SWV_DYN_VERTS_MAX);
            h.n_things = std::min<uint32_t>(h.n_things, SWV_THINGS_MAX);
            if (need_static) {
                tmp_static_.resize(h.n_static);
                memcpy(tmp_static_.data(), block_ + SWV_OFF_STATIC, h.n_static * sizeof(SwvVert));
            }
            if (need_tmap) {
                tmp_tmap_.resize(SWV_TMAP_PAGES * SWV_TMAP_DIM * SWV_TMAP_DIM);
                memcpy(tmp_tmap_.data(), block_ + SWV_OFF_TMAP, tmp_tmap_.size());
                memcpy(tmp_pal_, block_ + SWV_OFF_PALETTE, 768);
            }
            if (need_sprites) {
                h.n_sprites = std::min<uint32_t>(h.n_sprites, SWV_SPRITES_MAX);
                h.n_sprite_pages = std::min<uint32_t>(h.n_sprite_pages, SWV_SPRITE_PAGES);
                h.n_nstart = std::min<uint32_t>(h.n_nstart, SWV_NSTART_MAX);
                h.n_frames = std::min<uint32_t>(h.n_frames, SWV_FRAMES_MAX);
                h.n_elements = std::min<uint32_t>(h.n_elements, SWV_ELEMENTS_MAX);
                copy_array(tmp_rects_, SWV_OFF_SPRITE_RECTS, h.n_sprites);
                copy_array(tmp_nstart_, SWV_OFF_NSTART, h.n_nstart);
                copy_array(tmp_frames_, SWV_OFF_FRAMES, h.n_frames);
                copy_array(tmp_elements_, SWV_OFF_ELEMENTS, h.n_elements);
                copy_array(tmp_pages_, SWV_OFF_SPRITE_PAGES, h.n_sprite_pages * SWV_SPRITE_PAGE_BYTES);
            }
            tmp_dyn_.resize(h.n_dyn);
            memcpy(tmp_dyn_.data(), block_ + SWV_OFF_DYN, h.n_dyn * sizeof(SwvVert));
            tmp_things_.resize(h.n_things);
            memcpy(tmp_things_.data(), block_ + SWV_OFF_THINGS, h.n_things * sizeof(SwvThing));
            h.n_effects = std::min<uint32_t>(h.n_effects, SWV_EFFECTS_MAX);
            copy_array(tmp_effects_, SWV_OFF_EFFECTS, h.n_effects);
            h.n_lines = std::min<uint32_t>(h.n_lines, SWV_LINES_MAX);
            copy_array(tmp_lines_, SWV_OFF_LINES, h.n_lines);
            copy_array(tmp_groups_, SWV_OFF_DYN_GROUPS, std::min<uint32_t>(h.n_dyn_groups, SWV_DYN_GROUPS_MAX));
            copy_array(tmp_bars_, SWV_OFF_BARS, std::min<uint32_t>(h.n_bars, SWV_BARS_MAX));
            copy_array(tmp_glows_, SWV_OFF_GLOWS, std::min<uint32_t>(h.n_glows, SWV_GLOWS_MAX));
            copy_array(tmp_shadows_, SWV_OFF_SHADOWS, std::min<uint32_t>(h.n_shadows, SWV_SHADOWS_MAX));
            memcpy(&tmp_hud_, block_ + SWV_OFF_HUD, sizeof(SwvHud));
            MemoryBarrier();
            if (hdr->seq != seq1)
                continue;
            snap.hdr = h;
            snap.static_changed = need_static;
            snap.tmap_changed = need_tmap;
            if (need_static) {
                snap.static_verts.swap(tmp_static_);
                snap.static_rev = h.static_rev;
            }
            if (need_tmap) {
                snap.tmap.swap(tmp_tmap_);
                memcpy(snap.palette, tmp_pal_, 768);
                snap.tmap_rev = h.tmap_rev;
            }
            snap.sprite_changed = need_sprites;
            if (need_sprites) {
                snap.sprite_rects.swap(tmp_rects_);
                snap.nstart.swap(tmp_nstart_);
                snap.frames.swap(tmp_frames_);
                snap.elements.swap(tmp_elements_);
                snap.sprite_pages.swap(tmp_pages_);
                snap.sprite_stride = std::max<uint32_t>(h.sprite_stride, 1);
                snap.sprite_rev = h.sprite_rev;
            }
            snap.dyn_verts.swap(tmp_dyn_);
            snap.things.swap(tmp_things_);
            snap.effects.swap(tmp_effects_);
            snap.lines.swap(tmp_lines_);
            snap.dyn_groups.swap(tmp_groups_);
            snap.bars.swap(tmp_bars_);
            snap.glows.swap(tmp_glows_);
            snap.shadows.swap(tmp_shadows_);
            snap.hud = tmp_hud_;
            snap.valid = true;
            return true;
        }
        return false;
    }

private:
    template <typename T>
    void copy_array(std::vector<T> &dst, uint32_t off, uint32_t n)
    {
        dst.resize(n);
        memcpy(dst.data(), block_ + off, n * sizeof(T));
    }

    const uint8_t *block_ = nullptr;
    std::vector<SwvVert> tmp_static_, tmp_dyn_;
    std::vector<SwvThing> tmp_things_;
    std::vector<SwvEffect> tmp_effects_;
    std::vector<SwvLine> tmp_lines_;
    std::vector<SwvDynGroup> tmp_groups_;
    std::vector<SwvBar> tmp_bars_;
    std::vector<SwvGlow> tmp_glows_;
    std::vector<SwvShadow> tmp_shadows_;
    SwvHud tmp_hud_{};
    std::vector<uint8_t> tmp_tmap_;
    uint8_t tmp_pal_[768];
    std::vector<SwvSpriteRect> tmp_rects_;
    std::vector<uint16_t> tmp_nstart_;
    std::vector<SwvFrame> tmp_frames_;
    std::vector<SwvElement> tmp_elements_;
    std::vector<uint8_t> tmp_pages_;
};

/******************************************************************************/
// The game in this process: the core library steps the mission, the world
// export (libswarsview) fills a block, read by BlockReader

class CoreWorld {
public:
    /** Starts the mission with the game data in data_dir (the core reads
     * its files relative to the current directory). */
    bool start(const char *data_dir, unsigned campaign, unsigned mission, double turn_seconds, double now,
               const std::function<void()> &on_ready = nullptr)
    {
        if (data_dir && !SetCurrentDirectoryA(data_dir)) {
            logf("cannot use game data folder %s", data_dir);
            return false;
        }
        if (swcore_core_init(campaign, mission) != 0) {
            logf("cannot start mission %u,%u", campaign, mission);
            return false;
        }
        if (on_ready)
            on_ready();
        int missing = swview_load_assets();
        if (missing != 0)
            logf("%d asset files missing", missing);
        block_.assign(swview_block_size(), 0);
        swview_update(block_.data());
        reader_.attach(block_.data());
        turn_seconds_ = turn_seconds;
        last_turn_ = now;
        running_ = true;
        fresh_ = true;
        logf("game core: mission %u,%u, API %u, view API %u", campaign, mission,
             (unsigned)SWCORE_API_VERSION, (unsigned)SWVIEW_API_VERSION);
        return true;
    }

    /** Plays the turns due by now, on a fixed clock, and reads the world
     * when it changed. */
    bool poll(WorldSnapshot &snap, double now)
    {
        int n = 0;
        while (running_ && (now - last_turn_ >= turn_seconds_) && (n < 4)) {
            last_turn_ += turn_seconds_;
            n++;
            // a held trigger: the shot again every turn, as the desktop's
            // held mouse button; the first turn without it releases it
            if (held_on_) {
                swcore_command(&held_);
                held_turns_++;
                if (release_after_) {
                    held_on_ = false;
                    release_after_ = false;
                    logf("trigger released after %d turns", held_turns_);
                }
            }
            if (!swcore_core_step()) {
                running_ = false;
                struct SwcWorldInfo info;
                swcore_world_info(&info);
                logf("mission over at turn %u, status %u", info.gameturn, info.mission_status);
            }
            swview_update(block_.data());
            {
                // events are those of the last update: keep every turn's
                SwvFxEvent ev[SWV_FX_EVENTS_MAX];
                uint32_t ne = swview_fx_events(ev, SWV_FX_EVENTS_MAX);
                fx_pending_.insert(fx_pending_.end(), ev, ev + ne);
            }
            if (dump_file_ != nullptr)
                dump_block();
        }
        if (n == 4)
            last_turn_ = now; // the game cannot keep up: no catching up
        if ((n == 0) && !fresh_)
            return false;
        fresh_ = false;
        if (!reader_.poll(snap))
            return false;
        swview_hud_info(&snap.hud_info);
        snap.persons.resize(kPersonsMax);
        snap.persons.resize(swview_persons_render(snap.persons.data(), kPersonsMax));
        snap.props.resize(kPropsMax);
        snap.props.resize(swview_props(snap.props.data(), kPropsMax));
        snap.charges.resize(SWV_AGENTS_MAX);
        snap.charges.resize(swview_charges(snap.charges.data(), SWV_AGENTS_MAX));
        snap.fx_events.swap(fx_pending_);
        fx_pending_.clear();
        return true;
    }

    /** Holds the trigger with this shot (re-aimed by calling again), or
     * releases it (nullptr). A press lasts at least one turn, so a quick
     * click still shoots; a charging weapon fires on release. */
    void hold(const SwcCommand *c)
    {
        if (c != nullptr) {
            if (!held_on_) {
                held_turns_ = 0;
                logf("trigger held: command %u thing %d at %d,%d", c->type, c->thing, c->x, c->z);
            }
            held_ = *c;
            held_on_ = true;
            release_after_ = false;
        } else if (held_on_) {
            if (held_turns_ > 0) {
                held_on_ = false;
                logf("trigger released after %d turns", held_turns_);
            } else {
                release_after_ = true;
            }
        }
    }

    /** A command of the player (swcore_command()). */
    void command(const SwcCommand &c) { swcore_command(&c); }

    /** Moves the game camera to look at a map point, turned to angle
     * (0..2047), or not turned for a negative angle. */
    void look_at(int32_t x, int32_t z, int32_t angle = -1)
    {
        swcore_camera_look_at(x, z);
        if (angle >= 0)
            swcore_camera_set_angle(angle);
    }

    /** Sets the player's zoom of the game camera (120..256). */
    void set_zoom(int32_t zoom) { swcore_camera_set_zoom(zoom); }

    /** The player confirms the end of a decided mission. */
    void conclude()
    {
        struct SwcWorldInfo info;
        swcore_world_info(&info);
        if (running_ && ((info.mission_status == SwcMS_WON) || (info.mission_status == SwcMS_LOST)))
            swcore_mission_conclude();
    }

    /** Writes the world block of that turn to fname, for comparisons. */
    void dump_at(uint32_t turn, const char *fname)
    {
        dump_turn_ = turn;
        dump_file_ = fname;
    }

    bool running() const { return running_; }
    void stop()
    {
        swview_free_assets();
        swcore_core_shutdown();
    }

private:
    void dump_block()
    {
        struct SwcWorldInfo info;
        swcore_world_info(&info);
        if (info.gameturn != dump_turn_)
            return;
        FILE *f = fopen(dump_file_, "wb");
        if (f != nullptr) {
            fwrite(block_.data(), 1, block_.size(), f);
            fclose(f);
            logf("world block of turn %u written to %s", info.gameturn, dump_file_);
        }
        dump_file_ = nullptr;
    }

    static constexpr uint32_t kPersonsMax = 2048;
    static constexpr uint32_t kPropsMax = 8192;
    SwcCommand held_{};
    bool held_on_ = false, release_after_ = false;
    int held_turns_ = 0;
    std::vector<SwvFxEvent> fx_pending_;
    std::vector<uint8_t> block_;
    BlockReader reader_;
    uint32_t dump_turn_ = 0;
    const char *dump_file_ = nullptr;
    double turn_seconds_ = 0.05, last_turn_ = 0;
    bool running_ = false, fresh_ = false;
};

#if VR_DESKTOP_GAME
/******************************************************************************/
// The desktop game in this process: the whole game (libswarsfe: its window,
// panels, sound and mouse and keyboard play, as syndwarsfx) runs on a thread
// of its own, at its own pace; after each of its turns the world export
// fills the block, which the headset's thread reads. Commands of the
// headset are queued and given to the game on its thread, before its next
// turn. Only the game's thread uses the game and SDL's events.

class DesktopGameWorld {
public:
    /** Starts the desktop game on the mission, in the game data folder;
     * on_ready runs on the game's thread once the game is set up. Returns
     * when the game has played its first turn, or failed. */
    bool start(const char *data_dir, unsigned campaign, unsigned mission, double, double,
               const std::function<void()> &on_ready = nullptr)
    {
        if (data_dir && !SetCurrentDirectoryA(data_dir)) {
            logf("cannot use game data folder %s", data_dir);
            return false;
        }
        block_.assign(swview_block_size(), 0);
        reader_.attach(block_.data());
        snprintf(mission_arg_, sizeof(mission_arg_), "%u,%u", campaign, mission);
        on_ready_ = on_ready;
        thread_ = std::thread([this] { game_thread(); });
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait(lk, [this] { return started_ || ended_.load(); });
        if (!started_) {
            logf("the desktop game did not start");
            return false;
        }
        logf("desktop game: mission %s, API %u, view API %u", mission_arg_, (unsigned)SWCORE_API_VERSION,
             (unsigned)SWVIEW_API_VERSION);
        return true;
    }

    /** Reads the world when the game played a turn since the last call. */
    bool poll(WorldSnapshot &snap, double)
    {
        uint32_t turns = turns_.load();
        if (turns == seen_turns_)
            return false;
        if (!reader_.poll(snap))
            return false; // the game is writing it: next frame
        seen_turns_ = turns;
        if (snap.hdr.gameturn / 200 != logged_turn_ / 200) {
            logf("desktop game turn %u, %s, %u things", snap.hdr.gameturn,
                 snap.hdr.in_mission ? "in mission" : "not in a mission", snap.hdr.n_things);
            logged_turn_ = snap.hdr.gameturn;
        }
        {
            std::lock_guard<std::mutex> lk(mu_);
            snap.hud_info = extra_.hud_info;
            snap.persons = extra_.persons;
            snap.props = extra_.props;
            snap.charges = extra_.charges;
            snap.fx_events.swap(extra_.fx_events);
            extra_.fx_events.clear();
        }
        if (snap.hdr.in_mission) {
            seen_mission_ = true;
        } else if (seen_mission_ && !mission_over_) {
            mission_over_ = true;
            logf("the mission is over: the desktop game left it");
        }
        return true;
    }

    void command(const SwcCommand &c)
    {
        std::lock_guard<std::mutex> lk(mu_);
        queue_.push_back(c);
    }

    /** Moves the desktop's camera to look at a map point (on the game's
     * thread, before its next turn). */
    void look_at(int32_t x, int32_t z, int32_t angle = -1)
    {
        std::lock_guard<std::mutex> lk(mu_);
        look_x_ = x;
        look_z_ = z;
        look_angle_ = angle;
        look_ = true;
    }

    void set_zoom(int32_t zoom)
    {
        std::lock_guard<std::mutex> lk(mu_);
        zoom_ = zoom;
    }

    // As CoreWorld::hold(), applied on the game's thread every turn.
    void hold(const SwcCommand *c)
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (c != nullptr) {
            if (!held_on_)
                held_turns_ = 0;
            held_ = *c;
            held_on_ = true;
            release_after_ = false;
        } else if (held_on_) {
            if (held_turns_ > 0)
                held_on_ = false;
            else
                release_after_ = true;
        }
    }

    void conclude()
    {
        std::lock_guard<std::mutex> lk(mu_);
        conclude_ = true;
    }

    void dump_at(uint32_t, const char *) { logf("--dump-block: only with the game core alone"); }

    bool running() const { return !ended_.load() && !mission_over_; }

    void stop()
    {
        quit_ = true;
        if (thread_.joinable())
            thread_.join();
    }

private:
    struct Extra {
        SwvHudInfo hud_info{};
        std::vector<SwvPersonRender> persons;
        std::vector<SwvProp> props;
        std::vector<SwvCharge> charges;
        std::vector<SwvFxEvent> fx_events;
    };

    void game_thread()
    {
        char arg0[] = "syndwarsfx", q[] = "-q", w[] = "-W", m[] = "-m";
        char *argv[] = {arg0, q, w, m, mission_arg_, nullptr};
        if (swcore_init(5, argv) != 0) {
            logf("the desktop game could not be set up");
            finish();
            return;
        }
        if (on_ready_)
            on_ready_();
        update_world();
        {
            std::lock_guard<std::mutex> lk(mu_);
            started_ = true;
        }
        cv_.notify_all();
        while (!quit_) {
            apply_commands();
            if (!swcore_step()) {
                logf("the desktop game ended");
                break;
            }
            update_world();
        }
        swcore_shutdown();
        finish();
    }

    void finish()
    {
        {
            std::lock_guard<std::mutex> lk(mu_);
            ended_ = true;
        }
        cv_.notify_all();
    }

    // The headset's commands, before the game's turn.
    void apply_commands()
    {
        std::vector<SwcCommand> q;
        bool conclude = false, look = false;
        int32_t lx = 0, lz = 0, la = -1, zoom = -1;
        {
            std::lock_guard<std::mutex> lk(mu_);
            q.swap(queue_);
            look = look_;
            lx = look_x_;
            lz = look_z_;
            la = look_angle_;
            look_ = false;
            zoom = zoom_;
            zoom_ = -1;
            conclude = conclude_;
            conclude_ = false;
            if (held_on_) {
                // a held trigger: the shot again every turn
                q.push_back(held_);
                held_turns_++;
                if (release_after_) {
                    held_on_ = false;
                    release_after_ = false;
                }
            }
        }
        for (const SwcCommand &c : q)
            swcore_command(&c);
        if (look) {
            swcore_camera_look_at(lx, lz);
            if (la >= 0)
                swcore_camera_set_angle(la);
        }
        if (zoom >= 0)
            swcore_camera_set_zoom(zoom);
        if (conclude) {
            struct SwcWorldInfo info;
            swcore_world_info(&info);
            if ((info.mission_status == SwcMS_WON) || (info.mission_status == SwcMS_LOST))
                swcore_mission_conclude();
        }
    }

    // The world export of the turn: the block, marked as being written
    // (odd seq) for the reader on the other thread, and the rest.
    void update_world()
    {
        volatile SwvHeader *h = reinterpret_cast<volatile SwvHeader *>(block_.data());
        uint32_t seq = h->seq;
        h->seq = seq + 1;
        MemoryBarrier();
        swview_update(block_.data());
        MemoryBarrier();
        h->seq = seq + 2;
        Extra ex;
        bool in_mission = h->in_mission != 0;
        if (in_mission) {
            swview_hud_info(&ex.hud_info);
            ex.persons.resize(2048);
            ex.persons.resize(swview_persons_render(ex.persons.data(), 2048));
            ex.props.resize(8192);
            ex.props.resize(swview_props(ex.props.data(), 8192));
            ex.charges.resize(SWV_AGENTS_MAX);
            ex.charges.resize(swview_charges(ex.charges.data(), SWV_AGENTS_MAX));
        }
        static SwvFxEvent ev[SWV_FX_EVENTS_MAX];
        uint32_t ne = in_mission ? swview_fx_events(ev, SWV_FX_EVENTS_MAX) : 0;
        {
            std::lock_guard<std::mutex> lk(mu_);
            ex.fx_events.swap(extra_.fx_events);
            ex.fx_events.insert(ex.fx_events.end(), ev, ev + ne);
            extra_ = std::move(ex);
        }
        turns_++;
    }

    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool started_ = false;
    std::atomic<bool> ended_{false}, quit_{false};
    std::function<void()> on_ready_;
    char mission_arg_[32] = "0,1";
    std::vector<uint8_t> block_;
    BlockReader reader_;
    std::atomic<uint32_t> turns_{0};
    uint32_t seen_turns_ = 0, logged_turn_ = 0;
    bool seen_mission_ = false, mission_over_ = false;
    Extra extra_;
    std::vector<SwcCommand> queue_;
    SwcCommand held_{};
    bool held_on_ = false, release_after_ = false, conclude_ = false;
    int held_turns_ = 0;
    bool look_ = false;
    int32_t look_x_ = 0, look_z_ = 0, look_angle_ = -1, zoom_ = -1;
};
#endif

