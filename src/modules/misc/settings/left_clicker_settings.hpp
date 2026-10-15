#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct left_clicker
    {
        int min_cps = 10;
        int max_cps = 14;
        int jitter_percent = 8;
        bool jitter_camera = false;
        float jitter_amount = 0.25f;
        bool fatigue_enabled = true;
        float fatigue_factor = 0.35f;
        bool glfw_input = true;
        bool operator==(const left_clicker&) const = default;
        static std::span<const modules::settings::descriptor<left_clicker>> schema()
        {
            using model = left_clicker;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("min_cps", {"Minimum CPS", "Minimum CPS"}, &model::min_cps, 4, 30, "%d CPS"),
                setting::integer("max_cps", {"Maximum CPS", "Maksimum CPS"}, &model::max_cps, 4, 30, "%d CPS"),
                setting::integer("jitter_percent", {"Timing Jitter", "Zamanlama Sapması"}, &model::jitter_percent, 0, 35, "%d%%"),
                setting::toggle("jitter_camera", {"Jitter Camera", "Kamera Jitter (Sarsıntı)"}, &model::jitter_camera),
                setting::number("jitter_amount", {"Jitter Intensity", "Jitter Şiddeti"}, &model::jitter_amount, 0.05f, 1.0f, "%.2f°").when([](const model& c) { return c.jitter_camera; }),
                setting::toggle("fatigue_enabled", {"Human Fatigue", "El Yorulması (Human Fatigue)"}, &model::fatigue_enabled),
                setting::number("fatigue_factor", {"Fatigue Factor", "Yorulma Katsayısı"}, &model::fatigue_factor, 0.1f, 1.0f, "%.2f").when([](const model& c) { return c.fatigue_enabled; }),
                setting::toggle("glfw_input", {"GLFW Input", "GLFW Girişi"}, &model::glfw_input),
            };
            return fields;
        }
    };
}
