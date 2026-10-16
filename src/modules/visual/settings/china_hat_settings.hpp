#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct china_hat
    {
        int style = 0;
        float radius = .62f, height = .30f, offset = .025f, opacity = .45f;
        bool theme_color = true, glow = true;
        float color[4]{.65f, .40f, 1.f, 1.f};
        bool operator==(const china_hat&) const = default;
        static std::span<const modules::settings::descriptor<china_hat>> schema()
        {
            using d = modules::settings::descriptor<china_hat>;
            static constexpr modules::settings::label styles[]{
                {"Classic", "Klasik"}, {"Wide Brim", "Geniş Kenar"}, {"Pagoda", "Katmanlı"}
            };
            static const d fields[]{
                d::choice("style", {"Style", "Tasarım"}, &china_hat::style, styles),
                d::number("radius", {"Width", "Genişlik"}, &china_hat::radius, .35f, 1.f, "%.2f"),
                d::number("height", {"Height", "Yükseklik"}, &china_hat::height, .15f, .65f, "%.2f"),
                d::number("offset", {"Head Offset", "Baş Mesafesi"}, &china_hat::offset, 0.f, .25f, "%.2f"),
                d::number("opacity", {"Opacity", "Opaklık"}, &china_hat::opacity, .1f, 1.f, "%.2f"),
                d::toggle("theme_color", {"Theme Color", "Tema Rengi"}, &china_hat::theme_color),
                d::color("color", {"Color", "Renk"}, &china_hat::color).when([](const china_hat& c) { return !c.theme_color; }),
                d::toggle("glow", {"Glow", "Parlama"}, &china_hat::glow)
            };
            return fields;
        }
    };
}
