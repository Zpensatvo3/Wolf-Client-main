#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct hud
    {
        float scale = 1.f, position_x = 10.f, position_y = 10.f;
        bool operator==(const hud&) const = default;
        static std::span<const modules::settings::descriptor<hud>> schema()
        {
            using d = modules::settings::descriptor<hud>;
            static const d fields[]{
                d::number("scale", {"Scale","Olcek"}, &hud::scale, .5f, 2.f, "%.2f"),
                d::number("position_x", {"Position X","Konum X"}, &hud::position_x, -1000000.f, 1000000.f).hidden(),
                d::number("position_y", {"Position Y","Konum Y"}, &hud::position_y, -1000000.f, 1000000.f).hidden()
            };
            return fields;
        }
    };
}
