#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct hit_crystal
    {
        bool auto_switch = true;
        bool restore_slot = true;
        int place_delay = 28;
        int break_delay = 24;
        bool operator==(const hit_crystal&) const = default;
        static std::span<const modules::settings::descriptor<hit_crystal>> schema()
        {
            using model = hit_crystal;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("auto_switch", {"Auto Switch", "Otomatik Gecis"}, &model::auto_switch),
                setting::toggle("restore_slot", {"Restore Slot", "Slotu Geri Yukle"}, &model::restore_slot),
                setting::integer("place_delay", {"Place Delay", "Yerlestirme Gecikmesi"}, &model::place_delay, 0, 250, "%d ms"),
                setting::integer("break_delay", {"Break Delay", "Kirma Gecikmesi"}, &model::break_delay, 0, 200, "%d ms"),
            };
            return fields;
        }
    };
}
