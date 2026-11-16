#pragma once
#include "legit_aim_config.hpp"
#include "blatant_aim_config.hpp"

namespace click_aim
{
    struct config
    {
        int mode = 0; // Legit / Blatant
        float legit_strength = 42, legit_fov = 60, legit_max_distance = 4.2f;
        legit_aim_engine::aim_point legit_point = legit_aim_engine::aim_point::adaptive;
        bool legit_weapon_only = true;
        float blatant_strength = .30f, blatant_fov = 60, blatant_max_distance = 4.5f;
        blatant_aim_engine::aim_point blatant_point = blatant_aim_engine::aim_point::body;
        bool blatant_only_weapon = false, blatant_last_target = false;
        bool operator==(const config&) const = default;
        legit_aim_engine::config legit() const
        { return {legit_strength, legit_fov, legit_max_distance, legit_point, legit_weapon_only}; }
        blatant_aim_engine::config blatant() const
        { return {blatant_fov, blatant_max_distance, blatant_strength, blatant_point, blatant_only_weapon, blatant_last_target}; }
        static std::span<const modules::settings::descriptor<config>> schema()
        {
            using d = modules::settings::descriptor<config>;
            using label = modules::settings::label;
            static constexpr label modes[]{{"Legit"}, {"Blatant"}};
            static constexpr label legit_points[]{{"Adaptive"}, {"Upper Body"}, {"Head"}};
            static constexpr int legit_values[]{4, 1, 0};
            static constexpr label blatant_points[]{{"Body"}, {"Random"}, {"Adaptive"}};
            static constexpr int blatant_values[]{1, 3, 4};
            static const d fields[]{
                d::choice("mode", {"Mode"}, &config::mode, modes),
                d::number("legit_strength", {"Assist Strength"}, &config::legit_strength, 0, 100, "%.0f%%").when([](const config& c) {return c.mode == 0;}),
                d::number("legit_fov", {"FOV"}, &config::legit_fov, 20, 120, "%.0f deg").when([](const config& c) {return c.mode == 0;}),
                d::number("legit_max_distance", {"Range"}, &config::legit_max_distance, 2.5f, 5, "%.1f blocks").when([](const config& c) {return c.mode == 0;}),
                d::enumeration<&config::legit_point>("legit_aim_point", {"Aim Point"}, legit_points, legit_values).when([](const config& c) {return c.mode == 0;}),
                d::toggle("legit_weapon_only", {"Weapon Only"}, &config::legit_weapon_only).when([](const config& c) {return c.mode == 0;}),
                d::number("blatant_strength", {"Strength"}, &config::blatant_strength, .05f, 1, "%.2f").when([](const config& c) {return c.mode == 1;}),
                d::number("blatant_fov", {"FOV"}, &config::blatant_fov, 10, 180, "%.0f deg").when([](const config& c) {return c.mode == 1;}),
                d::number("blatant_max_distance", {"Max Distance"}, &config::blatant_max_distance, 1, 6, "%.1f blocks").when([](const config& c) {return c.mode == 1;}),
                d::enumeration<&config::blatant_point>("blatant_aim_point", {"Aim Point"}, blatant_points, blatant_values).when([](const config& c) {return c.mode == 1;}),
                d::toggle("blatant_only_weapon", {"Only Weapon"}, &config::blatant_only_weapon).when([](const config& c) {return c.mode == 1;}),
                d::toggle("blatant_last_target", {"Last Target"}, &config::blatant_last_target).when([](const config& c) {return c.mode == 1;})
            };
            return fields;
        }
    };
}
