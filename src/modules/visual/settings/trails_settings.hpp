#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct trails
    {
        int mode = 2;
        float lifetime = 2.5f, density = 1.f, size = 1.f, opacity = .8f;
        bool theme_color = true, glow = true;
        float color[4]{.65f,.40f,1.f,1.f};
        bool operator==(const trails&) const = default;
        static std::span<const modules::settings::descriptor<trails>> schema()
        {
            using d = modules::settings::descriptor<trails>;
            static constexpr modules::settings::label modes[]{
                {"Particles", "Parçacık"}, {"Footprints", "Ayak İzi"}, {"Both", "İkisi Birlikte"}
            };
            static const d fields[]{
                d::choice("mode", {"Trail", "İz Türü"}, &trails::mode, modes),
                d::number("lifetime", {"Lifetime", "İz Süresi"}, &trails::lifetime, .5f, 5.f, "%.1f s"),
                d::number("density", {"Particle Density", "Parçacık Sıklığı"}, &trails::density, .5f, 2.f, "%.1fx").when([](const trails& c) { return c.mode!=1; }),
                d::number("size", {"Size", "Boyut"}, &trails::size, .5f, 1.5f, "%.2fx"),
                d::number("opacity", {"Opacity", "Opaklık"}, &trails::opacity, .1f, 1.f, "%.2f"),
                d::toggle("theme_color", {"Theme Color", "Tema Rengi"}, &trails::theme_color),
                d::color("color", {"Color", "Renk"}, &trails::color).when([](const trails& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow", "Parlama"}, &trails::glow)
            };
            return fields;
        }
    };
}
