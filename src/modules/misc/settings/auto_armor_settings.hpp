#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_armor
    {
        int action_delay_ms = 120;
        bool protect_elytra = true;
        bool operator==(const auto_armor&) const = default;

        static std::span<const modules::settings::descriptor<auto_armor>> schema()
        {
            using model = auto_armor;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("protect_elytra", {"Protect Elytra", "Elytra'yi Koru"}, &model::protect_elytra),
                setting::integer("action_delay_ms", {"Click Delay", "Tik Gecikmesi"}, &model::action_delay_ms, 40, 500, "%d ms"),
            };
            return fields;
        }
    };
}
