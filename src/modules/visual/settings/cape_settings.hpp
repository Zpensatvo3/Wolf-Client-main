#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct cape
    {
        int style = 1;
        float length = 1.05f, width = .66f, motion = 1.f, opacity = .94f;
        bool animate = true, theme_color = true, glow = true;
        float color[4]{.65f, .40f, 1.f, 1.f};
        bool operator==(const cape&) const = default;
        static std::span<const modules::settings::descriptor<cape>> schema()
        {
            using d = modules::settings::descriptor<cape>;
            static constexpr modules::settings::label styles[]{
                {"Plain", "Düz"}, {"Bordered", "Şeritli"}, {"Emblem", "Amblemli"}
            };
            static const d fields[]{
                d::choice("style", {"Style", "Tasarım"}, &cape::style, styles),
                d::number("length", {"Length", "Uzunluk"}, &cape::length, .65f, 1.30f, "%.2f"),
                d::number("width", {"Width", "Genişlik"}, &cape::width, .45f, .95f, "%.2f"),
                d::toggle("animate", {"Animation", "Animasyon"}, &cape::animate),
                d::number("motion", {"Motion", "Hareket Miktarı"}, &cape::motion, .25f, 1.75f, "%.2fx", &cape::animate),
                d::number("opacity", {"Opacity", "Opaklık"}, &cape::opacity, .25f, 1.f, "%.2f"),
                d::toggle("theme_color", {"Theme Color", "Tema Rengi"}, &cape::theme_color),
                d::color("color", {"Color", "Renk"}, &cape::color).when([](const cape& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow", "Parlama"}, &cape::glow)
            };
            return fields;
        }
    };
}
