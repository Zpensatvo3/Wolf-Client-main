#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct trajectories
    {
        bool show_bows = true;
        bool show_throwables = true;
        bool show_potions = true;
        bool show_landing_marker = true;
        bool show_flight_time = true;
        float line_thickness = 2.0f;
        int max_ticks = 160;
        bool operator==(const trajectories&) const = default;
        static std::span<const modules::settings::descriptor<trajectories>> schema()
        {
            using model = trajectories;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("show_bows", {"Bows & Trident"}, &model::show_bows),
                setting::toggle("show_throwables", {"Throwables"}, &model::show_throwables),
                setting::toggle("show_potions", {"Potions & XP"}, &model::show_potions),
                setting::toggle("show_landing_marker", {"Impact Marker"}, &model::show_landing_marker),
                setting::toggle("show_flight_time", {"Flight Info"}, &model::show_flight_time),
                setting::number("line_thickness", {"Line Thickness"}, &model::line_thickness, 1.0f, 5.0f, "%.1f px"),
                setting::integer("max_ticks", {"Prediction Time"}, &model::max_ticks, 40, 240, "%d ticks"),
            };
            return fields;
        }
    };
}
