#pragma once
#include <cstdint>

// Pure model: no JNI, Windows, ImGui, random_device or wall-clock dependencies.
namespace trigger_engine
{
    enum class target_mode { players, players_and_mobs, all_living };
    enum class weapon_mode { any, sword, axe };
    enum class critical_mode { off, smart, only };
    struct config
    {
        float cooldown[2]{ .92f, .98f };
        int reaction[2]{ 25, 65 };
        target_mode targets = target_mode::players;
        weapon_mode weapon = weapon_mode::any;
        critical_mode criticals = critical_mode::smart;
        bool on_click = false;
        bool swing_hand = true;
        bool click_simulation = false;
    };
    bool normalize(config& value);
    bool same_config(const config& a, const config& b);
}
