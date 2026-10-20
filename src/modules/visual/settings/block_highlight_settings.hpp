#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct block_highlight
    {
        float opacity = .8f, fill_opacity = .10f;
        bool theme_color = true, glow = true, fill = true;
        float color[4]{.65f,.4f,1.f,1.f};
        bool operator==(const block_highlight&) const = default;
        static std::span<const modules::settings::descriptor<block_highlight>> schema()
        {
            using d = modules::settings::descriptor<block_highlight>;
            static const d fields[]{
                d::number("opacity",{"Outline Opacity"},&block_highlight::opacity,.1f,1.f,"%.2f"),
                d::toggle("fill",{"Fill"},&block_highlight::fill),
                d::number("fill_opacity",{"Fill Opacity"},&block_highlight::fill_opacity,0.f,.3f,"%.2f").when([](const block_highlight& c){return c.fill;}),
                d::toggle("theme_color",{"Theme Color"},&block_highlight::theme_color),
                d::color("color",{"Color"},&block_highlight::color).when([](const block_highlight& c){return !c.theme_color;}),
                d::toggle("glow",{"Glow"},&block_highlight::glow)
            }; return fields;
        }
    };
}
