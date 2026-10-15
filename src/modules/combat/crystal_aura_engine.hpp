#pragma once
#include "settings/crystal_aura_settings.hpp"
#include <chrono>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

namespace modules::crystal_aura_logic
{
    using clock = std::chrono::steady_clock;
    struct point { double x = 0, y = 0, z = 0; };
    struct bounds { point min, max; };
    struct cell
    {
        int x = 0, y = 0, z = 0;
        bool operator==(const cell&) const = default;
    };
    inline bool finite(point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
    inline bool valid(bounds b)
    {
        return finite(b.min) && finite(b.max) && b.min.x < b.max.x && b.min.y < b.max.y && b.min.z < b.max.z;
    }
    inline double distance_squared(point a, point b)
    { return (a.x-b.x)*(a.x-b.x) + (a.y-b.y)*(a.y-b.y) + (a.z-b.z)*(a.z-b.z); }
    inline point closest(point p, bounds b)
    { return {std::clamp(p.x,b.min.x,b.max.x),std::clamp(p.y,b.min.y,b.max.y),std::clamp(p.z,b.min.z,b.max.z)}; }
    inline bool intersects(bounds a, bounds b)
    {
        return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y &&
            a.min.z < b.max.z && a.max.z > b.min.z;
    }
    inline bounds base_volume(cell c)
    { return {{double(c.x),double(c.y),double(c.z)},{c.x+1.,c.y+1.,c.z+1.}}; }
    // EndCrystalItem's vanilla entity-clearance query is a 1 x 2 x 1 box.
    inline bounds crystal_volume(cell c)
    { return {{double(c.x),c.y+1.,double(c.z)},{c.x+1.,c.y+3.,c.z+1.}}; }
    struct candidate { int id = -1; double distance_sq = 0; bool eligible = false; };
    enum class target_kind { other, player, mob };
    struct target_state
    {
        target_kind kind = target_kind::other;
        bool self = false, alive = false, spectator = false, friendly = false, bot = false;
    };
    inline bool supports_target(int mode, target_kind kind)
    {
        if (kind == target_kind::player) return mode == 0 || mode == 1;
        if (kind == target_kind::mob) return mode == 1 || mode == 2;
        return false;
    }
    inline bool target_allowed(int mode, target_state target)
    {
        if (!supports_target(mode,target.kind) || target.self || !target.alive || target.spectator) return false;
        return target.kind != target_kind::player || (!target.friendly && !target.bot);
    }
    inline int choose_target(std::span<const candidate> candidates, int locked_id)
    {
        int best = -1;
        double nearest = INFINITY;
        for (const auto& c : candidates)
        {
            if (!c.eligible || c.id < 0 || !std::isfinite(c.distance_sq) || c.distance_sq < 0) continue;
            if (c.id == locked_id) return c.id;
            if (c.distance_sq < nearest || (c.distance_sq == nearest && c.id < best))
            { nearest = c.distance_sq; best = c.id; }
        }
        return best;
    }
    enum class phase { melee, launch, base, wait_base, crystal, wait_crystal, break_crystal, wait_break };
    enum class action_kind { sword, place_obsidian, place_crystal, break_crystal };
    struct action
    {
        action_kind kind;
        int target_id, crystal_id;
        std::uint64_t serial;
        bool operator==(const action&) const = default;
    };
    struct observation
    {
        std::uint64_t session = 0;
        int target_id = -1, crystal_id = -1;
        bool valid = false, materials = false, airborne = false, can_melee = false;
        bool site_available = false, base_ready = false, space_clear = false, can_break = false;
        bool target_is_mob = false;
    };
    class engine
    {
    public:
        void reset()
        {
            current_ = phase::melee; target_ = -1; crystal_ = -1; session_ = 0;
            pending_.reset(); ready_at_ = deadline_ = {}; ++serial_;
        }
        phase state() const { return current_; }
        int target() const { return target_; }
        int crystal() const { return crystal_; }
        bool finishing_crystal() const
        { return current_ == phase::wait_crystal || current_ == phase::break_crystal || current_ == phase::wait_break; }
        std::optional<action> next(const observation& seen, clock::time_point now,
            const module_settings::crystal_aura& config)
        {
            if (!seen.valid || seen.target_id < 0 || seen.session == 0 || !seen.materials)
            { reset(); return std::nullopt; }
            if (seen.session != session_ || seen.target_id != target_)
            { reset(); session_ = seen.session; target_ = seen.target_id; }
            if (pending_) return std::nullopt;
            switch (current_)
            {
            case phase::launch:
                if (seen.airborne || (seen.target_is_mob && config.mob_ground_test))
                    enter(phase::base, now, config.confirm_timeout_ms);
                else if (now >= deadline_) restart(now, config);
                return std::nullopt;
            case phase::base:
                if (seen.base_ready) { enter(phase::crystal, now, config.confirm_timeout_ms); return std::nullopt; }
                if (now >= deadline_) { restart(now, config); return std::nullopt; }
                break;
            case phase::wait_base:
                if (seen.base_ready) enter(phase::crystal, now, config.confirm_timeout_ms);
                else if (now >= deadline_) restart(now, config);
                return std::nullopt;
            case phase::crystal:
            case phase::wait_crystal:
                if (seen.crystal_id >= 0)
                { crystal_ = seen.crystal_id; enter(phase::break_crystal, now, config.confirm_timeout_ms); return std::nullopt; }
                if (now >= deadline_) { restart(now, config); return std::nullopt; }
                if (current_ == phase::wait_crystal) return std::nullopt;
                break;
            case phase::break_crystal:
                if (seen.crystal_id != crystal_ || now >= deadline_)
                { restart(now, config); return std::nullopt; }
                break;
            case phase::wait_break:
                if (seen.crystal_id != crystal_ || now >= deadline_) restart(now, config);
                return std::nullopt;
            case phase::melee: break;
            }
            if (now < ready_at_) return std::nullopt;
            std::optional<action_kind> kind;
            if (current_ == phase::melee && seen.can_melee) kind = action_kind::sword;
            if (current_ == phase::base && seen.site_available && seen.space_clear) kind = action_kind::place_obsidian;
            if (current_ == phase::crystal && seen.base_ready && seen.space_clear) kind = action_kind::place_crystal;
            if (current_ == phase::break_crystal && seen.can_break) kind = action_kind::break_crystal;
            if (!kind) return std::nullopt;
            pending_ = action{*kind, target_, crystal_, ++serial_};
            return pending_;
        }
        bool commit(action request, bool submitted, clock::time_point now, const module_settings::crystal_aura& config)
        {
            if (!pending_ || *pending_ != request) return false;
            pending_.reset();
            ready_at_ = now + std::chrono::milliseconds(std::clamp(config.action_delay_ms,50,500));
            if (!submitted) return true;
            switch (request.kind)
            {
            case action_kind::sword: enter(phase::launch, now, config.launch_timeout_ms); break;
            case action_kind::place_obsidian: enter(phase::wait_base, now, config.confirm_timeout_ms); break;
            case action_kind::place_crystal: enter(phase::wait_crystal, now, config.confirm_timeout_ms); break;
            case action_kind::break_crystal: enter(phase::wait_break, now, config.confirm_timeout_ms); break;
            }
            return true;
        }
    private:
        void enter(phase state, clock::time_point now, int timeout)
        { current_ = state; deadline_ = now + std::chrono::milliseconds(std::clamp(timeout,300,3000)); }
        void restart(clock::time_point now, const module_settings::crystal_aura& config)
        { current_ = phase::melee; crystal_ = -1; ready_at_ = now + std::chrono::milliseconds(std::clamp(config.action_delay_ms,50,500)); }
        phase current_ = phase::melee;
        int target_ = -1, crystal_ = -1;
        std::uint64_t session_ = 0, serial_ = 0;
        std::optional<action> pending_;
        clock::time_point ready_at_{}, deadline_{};
    };
}
