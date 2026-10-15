#pragma once
#include "settings/inv_walk_settings.hpp"
#include <array>

namespace inv_walk_engine
{
    enum class screen_kind { gameplay, inventory, container, other };
    enum class mode { blocked, vanilla, inventory };
    enum key : unsigned { forward, back, left, right, jump, sneak, key_count };
    struct observation
    {
        bool enabled = false, focused = false, client_blocked = false, live = false, typing = false;
        screen_kind screen = screen_kind::other;
    };
    inline mode choose_mode(const module_settings::inv_walk& config, const observation& state)
    {
        if (!state.enabled || !state.focused || state.client_blocked || !state.live) return mode::blocked;
        if (state.screen == screen_kind::gameplay) return mode::vanilla;
        if (state.typing) return mode::blocked;
        if (state.screen == screen_kind::inventory ||
            (state.screen == screen_kind::container && config.allow_containers)) return mode::inventory;
        return mode::blocked;
    }
    inline std::array<bool, key_count> desired_keys(const module_settings::inv_walk& config,
        std::array<bool, key_count> physical)
    {
        if (!config.allow_jump) physical[jump] = false;
        if (!config.allow_sneak) physical[sneak] = false;
        return physical;
    }
    // Release only a press made by InvWalk. Hand a held physical key back to
    // vanilla when closing the inventory, but never keep it down on suspension.
    constexpr bool should_release(bool owned, bool currently_pressed, bool vanilla, bool physical)
    {
        return owned && currently_pressed && !(vanilla && physical);
    }
}
