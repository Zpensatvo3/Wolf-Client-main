#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_totem
    {
        bool fill_offhand = true;
        bool fill_hotbar = true;
        bool auto_open_inventory = true;
        bool offhand_first = true;
        bool pause_with_cursor_item = true;
        int hotbar_slot = 9;
        int delay_ms = 75;
        int timing_randomness_ms = 30;
        int initial_reaction_ms = 110;
        int confirmation_timeout_ms = 260;
        bool operator==(const auto_totem&) const = default;
        static std::span<const modules::settings::descriptor<auto_totem>> schema()
        {
            using model = auto_totem;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("confirmation_timeout_ms", {"Server Confirmation", "Server Confirmation"}, &model::confirmation_timeout_ms, 80, 1200, "%d ms"),
                setting::toggle("fill_offhand", {"Fill Offhand", "Yan Eli Doldur"}, &model::fill_offhand),
                setting::toggle("fill_hotbar", {"Fill Hotbar", "Hotbarı Doldur"}, &model::fill_hotbar),
                setting::toggle("auto_open_inventory", {"Auto Open Inventory ('E')", "Otomatik Envanter Aç ('E')"}, &model::auto_open_inventory),
                setting::toggle("offhand_first", {"Offhand First", "Önce Yan El"}, &model::offhand_first),
                setting::toggle("pause_with_cursor_item", {"Pause While Carrying Item", "Elde Item Varken Bekle"}, &model::pause_with_cursor_item),
                setting::integer("hotbar_slot", {"Hotbar Slot", "Hotbar Slotu"}, &model::hotbar_slot, 1, 9, "Slot %d"),
                setting::integer("delay_ms", {"Action Delay", "İşlem Gecikmesi"}, &model::delay_ms, 0, 1000, "%d ms").display_minimum(20).display_maximum(500),
                setting::integer("timing_randomness_ms", {"Timing Randomness", "Zamanlama Rastgeleliği"}, &model::timing_randomness_ms, 0, 250, "%d ms").display_maximum(150),
                setting::integer("initial_reaction_ms", {"Initial Reaction", "İlk Tepki Süresi"}, &model::initial_reaction_ms, 0, 1000, "%d ms").display_minimum(30).display_maximum(500),
            };
            return fields;
        }
    };
}
