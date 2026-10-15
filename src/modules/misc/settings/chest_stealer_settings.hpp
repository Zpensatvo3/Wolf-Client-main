#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct chest_stealer
    {
        int min_delay_ms = 85;
        int max_delay_ms = 145;
        int open_reaction_ms = 95;
        bool random_slot_order = true;
        bool operator==(const chest_stealer&) const = default;

        static std::span<const modules::settings::descriptor<chest_stealer>> schema()
        {
            using model = chest_stealer;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("open_reaction_ms", {"Open Reaction", "Acilis Tepkisi"}, &model::open_reaction_ms, 25, 800, "%d ms").display_maximum(500),
                setting::integer("min_delay_ms", {"Minimum Delay", "Minimum Gecikme"}, &model::min_delay_ms, 25, 800, "%d ms").display_maximum(500),
                setting::integer("max_delay_ms", {"Maximum Delay", "Maksimum Gecikme"}, &model::max_delay_ms, 25, 1000, "%d ms", &model::min_delay_ms).display_maximum(800),
                setting::toggle("random_slot_order", {"Random Slot Order", "Rastgele Slot Sirasi"}, &model::random_slot_order),
            };
            return fields;
        }
    };
}
