#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct pot_refill
    {
        int delay_ms = 100;
        bool operator==(const pot_refill&) const = default;

        static std::span<const modules::settings::descriptor<pot_refill>> schema()
        {
            using model = pot_refill;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("delay_ms", {"Refill Delay", "Doldurma Gecikmesi"}, &model::delay_ms, 50, 1000, "%d ms"),
            };
            return fields;
        }
    };
}
