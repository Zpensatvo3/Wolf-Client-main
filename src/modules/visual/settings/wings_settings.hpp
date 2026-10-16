#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct wings
    {
        int style = 0;
        float size = 1.f, opacity = .48f, speed = 1.f;
        bool animate = true, theme_color = true, glow = true;
        float color[4]{.65f, .40f, 1.f, 1.f};
        bool operator==(const wings&) const = default;
        static std::span<const modules::settings::descriptor<wings>> schema()
        {
            using d = modules::settings::descriptor<wings>;
            static constexpr modules::settings::label styles[]{
                {"Dragon", "Ejderha"}, {"Angel", "Melek"}, {"Butterfly", "Kelebek"}
            };
            static const d fields[]{
                d::choice("style", {"Style", "Tasarım"}, &wings::style, styles),
                d::number("size", {"Size", "Boyut"}, &wings::size, .5f, 1.6f, "%.2fx"),
                d::number("opacity", {"Opacity", "Opaklık"}, &wings::opacity, .1f, 1.f, "%.2f"),
                d::toggle("animate", {"Flapping", "Kanat Çırpma"}, &wings::animate),
                d::number("speed", {"Flap Speed", "Çırpma Hızı"}, &wings::speed, .25f, 2.f, "%.2fx", &wings::animate),
                d::toggle("theme_color", {"Theme Color", "Tema Rengi"}, &wings::theme_color),
                d::color("color", {"Color", "Renk"}, &wings::color).when([](const wings& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow", "Parlama"}, &wings::glow)
            };
            return fields;
        }
    };
}
