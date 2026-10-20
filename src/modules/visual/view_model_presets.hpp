#pragma once
#include "settings/view_model_settings.hpp"
#include <array>
#include <cmath>

namespace view_model_presets
{
    // Presets are actions on the existing transform, not a second persisted
    // source of truth. Hand preference and imported custom values stay intact.
    struct preset { const char* name; float x, y, z, scale, pitch, yaw, roll; };
    inline constexpr std::array values{
        preset{"Vanilla", 0, 0, 0, 1, 0, 0, 0},
        preset{"Compact", .10f, -.12f, -.20f, .78f, 0, 0, 0},
        preset{"PvP", .18f, -.10f, -.12f, .90f, -5, 8, -6},
        preset{"Low", .08f, -.28f, -.10f, .90f, 0, 0, 0},
        preset{"Cinematic", .22f, -.08f, -.18f, 1.05f, -8, 18, -12}
    };
    inline bool apply(module_settings::view_model& config, int index)
    {
        if (index < 0 || index >= static_cast<int>(values.size())) return false;
        const auto& p = values[index];
        config.x=p.x; config.y=p.y; config.z=p.z; config.scale=p.scale;
        config.pitch=p.pitch; config.yaw=p.yaw; config.roll=p.roll;
        return true;
    }
    inline int selected(const module_settings::view_model& config)
    {
        const auto matches_component=[](float a,float b) { return std::abs(a-b)<.0001f; };
        for (int i=0;i<static_cast<int>(values.size());++i) {
            const auto& p=values[i];
            if (matches_component(config.x,p.x) && matches_component(config.y,p.y) &&
                matches_component(config.z,p.z) && matches_component(config.scale,p.scale) &&
                matches_component(config.pitch,p.pitch) && matches_component(config.yaw,p.yaw) &&
                matches_component(config.roll,p.roll)) return i;
        }
        return -1;
    }
}
