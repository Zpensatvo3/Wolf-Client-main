#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct legit_auto_totem
    {
        int reaction_delay_ms = 135;
        int action_delay_ms = 165;
        int timing_variation_ms = 45;
        int confirmation_timeout_ms = 360;
        int hotbar_slot = 9;
        float health_threshold = 12.0f;
        bool fill_offhand = true;
        bool fill_hotbar = false;
        bool offhand_first = true;
        bool low_health_only = false;
        bool cursor_guided_swap = true;
        int cursor_travel_ms = 46;
        bool pause_with_cursor_item = true;
        bool preserve_hotbar = true;
        bool operator==(const legit_auto_totem&) const = default;
        static std::span<const modules::settings::descriptor<legit_auto_totem>> schema()
        {
            using model = legit_auto_totem;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("fill_offhand", {"Fill Offhand", "Fill Offhand"}, &model::fill_offhand),
                setting::toggle("fill_hotbar", {"Fill Hotbar", "Fill Hotbar"}, &model::fill_hotbar),
                setting::toggle("offhand_first", {"Offhand First", "Offhand First"}, &model::offhand_first),
                setting::integer("hotbar_slot", {"Hotbar Slot", "Hotbar Slot"}, &model::hotbar_slot, 1, 9, "Slot %d"),
                setting::toggle("low_health_only", {"Low Health Offhand Only", "Low Health Offhand Only"}, &model::low_health_only),
                setting::number("health_threshold", {"Health Threshold", "Health Threshold"}, &model::health_threshold, 1.0f, 20.0f, "%.1f HP", &model::low_health_only),
                setting::toggle("preserve_hotbar", {"Preserve Hotbar", "Hotbar'i Koru"}, &model::preserve_hotbar),
                setting::toggle("pause_with_cursor_item", {"Pause While Carrying Item", "Elde Item Varken Bekle"}, &model::pause_with_cursor_item),
                setting::toggle("cursor_guided_swap", {"Cursor Guided Swap", "Cursor Guided Swap"}, &model::cursor_guided_swap),
                setting::integer("cursor_travel_ms", {"Cursor Travel", "Cursor Travel"}, &model::cursor_travel_ms, 16, 180, "%d ms").when([](const model& c) { return c.cursor_guided_swap; }),
                setting::integer("confirmation_timeout_ms", {"Server Confirmation", "Server Confirmation"}, &model::confirmation_timeout_ms, 120, 1800, "%d ms"),
                setting::integer("reaction_delay_ms", {"Inventory Reaction", "Envanter Tepkisi"}, &model::reaction_delay_ms, 40, 1200, "%d ms").display_maximum(600),
                setting::integer("action_delay_ms", {"Action Delay", "Islem Gecikmesi"}, &model::action_delay_ms, 40, 1200, "%d ms").display_minimum(50).display_maximum(800),
                setting::integer("timing_variation_ms", {"Timing Variation", "Zamanlama Degiskenligi"}, &model::timing_variation_ms, 0, 250, "%d ms").display_maximum(180),
            };
            return fields;
        }
    };
}
