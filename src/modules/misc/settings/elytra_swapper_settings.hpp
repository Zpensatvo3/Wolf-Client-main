#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct elytra_swapper
    {
        int target_mode = 0;
        int swap_delay_ms = 70;
        bool use_hotbar = true;
        bool work_without_inventory = true;
        bool operator==(const elytra_swapper&) const = default;

        static std::span<const modules::settings::descriptor<elytra_swapper>> schema()
        {
            using model = elytra_swapper;
            using setting = modules::settings::descriptor<model>;
        static constexpr modules::settings::label modes[]{{"Equip Elytra"}, {"Equip Chestplate"}};
            static const setting fields[]{
                setting::choice("target_mode", {"Swap Target"}, &model::target_mode, modes),
                setting::toggle("use_hotbar", {"Use Hotbar"}, &model::use_hotbar),
                setting::toggle("work_without_inventory", {"Work Without Inventory"}, &model::work_without_inventory),
                setting::integer("swap_delay_ms", {"Click Delay"}, &model::swap_delay_ms, 20, 300, "%d ms"),
            };
            return fields;
        }
    };
}
