#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct mace_swap
    {
        int preferred_mace_slot = 2;
        int return_delay = 10;
        bool operator==(const mace_swap&) const = default;
        static std::span<const modules::settings::descriptor<mace_swap>> schema()
        {
            using model = mace_swap;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("preferred_mace_slot", {"Preferred Mace Slot", "Tercih Edilen Mace Slotu"}, &model::preferred_mace_slot, 1, 9, "Slot %d"),
                setting::integer("return_delay", {"Return Delay", "Geri Donus Gecikmesi"}, &model::return_delay, 0, 75, "%d ms"),
            };
            return fields;
        }
    };
}
