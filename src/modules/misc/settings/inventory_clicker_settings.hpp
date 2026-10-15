#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct inventory_clicker
    {
        int min_cps = 4;
        int max_cps = 7;
        int click_delay = 150;
        bool operator==(const inventory_clicker&) const = default;
        static std::span<const modules::settings::descriptor<inventory_clicker>> schema()
        {
            using model = inventory_clicker;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("min_cps", {"Min CPS", "Minimum CPS"}, &model::min_cps, 1, 20, "%d"),
                setting::integer("max_cps", {"Max CPS", "Maksimum CPS"}, &model::max_cps, 1, 20, "%d"),
                setting::integer("click_delay", {"click_delay", "click_delay"}, &model::click_delay, -1000000, 1000000, "%d").hidden(),
            };
            return fields;
        }
    };
}
