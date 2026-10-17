#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct glow_esp
    {
        float box_thickness = 1.5f;
        bool show_names = true;
        bool show_health = true;
        bool show_distance = true;
        float fov_value = 70.f;
        bool use_game_fov = true;
        bool operator==(const glow_esp&) const = default;
        static std::span<const modules::settings::descriptor<glow_esp>> schema()
        {
            using model = glow_esp;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("box_thickness", {"box_thickness"}, &model::box_thickness, -1000000, 1000000, "%.2f").hidden(),
                setting::toggle("show_names", {"show_names"}, &model::show_names).hidden(),
                setting::toggle("show_health", {"show_health"}, &model::show_health).hidden(),
                setting::toggle("show_distance", {"show_distance"}, &model::show_distance).hidden(),
                setting::number("fov", {"fov"}, &model::fov_value, -1000000, 1000000, "%.2f").hidden(),
                setting::toggle("use_game_fov", {"use_game_fov"}, &model::use_game_fov).hidden(),
            };
            return fields;
        }
    };
}
