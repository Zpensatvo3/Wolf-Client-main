#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct target_hud
    {
        float position_x = 360.f, position_y = 260.f, scale = 1.f;
        float background_opacity = .94f, keep_seconds = 2.f;
        bool glow = true;
        bool operator==(const target_hud&) const = default;

        static std::span<const modules::settings::descriptor<target_hud>> schema()
        {
            using d = modules::settings::descriptor<target_hud>;
            static const d fields[]{
                d::number("position_x", {"Position X"}, &target_hud::position_x,
                    0.f, 1000000.f, "%.0f px").hidden(),
                d::number("position_y", {"Position Y"}, &target_hud::position_y,
                    0.f, 1000000.f, "%.0f px").hidden(),
                d::number("scale", {"Scale"}, &target_hud::scale, .5f, 2.f, "%.2fx"),
                d::number("keep_seconds", {"Keep Target"},
                    &target_hud::keep_seconds, 0.f, 5.f, "%.1f s"),
                d::number("background_opacity", {"Background"},
                    &target_hud::background_opacity, .3f, 1.f, "%.2f"),
                d::toggle("glow", {"Glow"}, &target_hud::glow),
            };
            return fields;
        }
    };
}
