#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct player_esp
    {
        int esp_mode = 0;
        float box_thickness = 1.5f;
        float fill_opacity = 0.16f;
        bool show_names = true;
        bool show_health = true;
        bool show_distance = true;
        float fov_value = 70.f;
        bool use_game_fov = true;
        bool use_theme_color = false;
        float esp_color[4] = { 0.35f, 0.75f, 1.0f, 1.0f };
        bool operator==(const player_esp&) const = default;
        static std::span<const modules::settings::descriptor<player_esp>> schema()
        {
            using model = player_esp;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label esp_mode_labels[]{{"2D Box", "2D Kutu"},{"Corners", "Köşeler"},{"3D Box", "3D Kutu"}};
            static const setting fields[]{
                setting::choice("esp_mode", {"Style", "Stil"}, &model::esp_mode, esp_mode_labels),
                setting::number("fill_opacity", {"Fill Opacity", "Dolgu Saydamlığı"}, &model::fill_opacity, 0.02f, 0.55f, "%.2f").when([](const model& c) { return c.esp_mode == 2; }),
                setting::number("box_thickness", {"Thickness", "Kalınlık"}, &model::box_thickness, 0.5f, 5.0f, "%.1f"),
                setting::toggle("use_theme_color", {"Use Theme Color", "Tema Rengini Kullan"}, &model::use_theme_color),
                setting::color("esp_color", {"ESP Color", "ESP Rengi"}, &model::esp_color).when([](const model& c) { return !c.use_theme_color; }),
                setting::toggle("show_names", {"Show Names", "İsimleri Göster"}, &model::show_names),
                setting::toggle("show_health", {"Show Health", "Canı Göster"}, &model::show_health),
                setting::toggle("show_distance", {"Show Distance", "Mesafeyi Göster"}, &model::show_distance),
                setting::toggle("use_game_fov", {"Use Game FOV", "Oyun FOV'unu Kullan"}, &model::use_game_fov),
                setting::number("fov", {"FOV", "FOV"}, &model::fov_value, 30.f, 120.f, "%.0f").when([](const model& c) { return !c.use_game_fov; }),
            };
            return fields;
        }
    };
}
