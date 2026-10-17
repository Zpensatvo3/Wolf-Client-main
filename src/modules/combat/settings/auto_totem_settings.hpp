#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_totem
    {
        int mode = 0; // Normal / Legit; each profile keeps its own settings.
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
        int legit_reaction_delay_ms = 135;
        int legit_action_delay_ms = 165;
        int legit_timing_variation_ms = 45;
        int legit_confirmation_timeout_ms = 360;
        int legit_hotbar_slot = 9;
        float legit_health_threshold = 12.0f;
        bool legit_fill_offhand = true;
        bool legit_fill_hotbar = false;
        bool legit_offhand_first = true;
        bool legit_low_health_only = false;
        bool legit_cursor_guided_swap = true;
        int legit_cursor_travel_ms = 46;
        bool legit_pause_with_cursor_item = true;
        bool legit_preserve_hotbar = true;
        bool operator==(const auto_totem&) const = default;
        static std::span<const modules::settings::descriptor<auto_totem>> schema()
        {
            using model = auto_totem;
            using setting = modules::settings::descriptor<model>;
            static constexpr modules::settings::label modes[]{{"Normal"}, {"Legit"}};
            static const auto normal = +[](const model& c) { return c.mode == 0; };
            static const auto legit = +[](const model& c) { return c.mode == 1; };
            static const setting fields[]{
                setting::choice("mode", {"Mode"}, &model::mode, modes),
                setting::toggle("fill_offhand", {"Fill Offhand"}, &model::fill_offhand).when(normal),
                setting::toggle("fill_hotbar", {"Fill Hotbar"}, &model::fill_hotbar).when(normal),
                setting::toggle("auto_open_inventory", {"Auto Open Inventory ('E')"}, &model::auto_open_inventory).when(normal),
                setting::toggle("offhand_first", {"Offhand First"}, &model::offhand_first).when(normal),
                setting::toggle("pause_with_cursor_item", {"Pause While Carrying Item"}, &model::pause_with_cursor_item).when(normal),
                setting::integer("hotbar_slot", {"Hotbar Slot"}, &model::hotbar_slot, 1, 9, "Slot %d").when(normal),
                setting::integer("delay_ms", {"Action Delay"}, &model::delay_ms, 0, 1000, "%d ms").display_minimum(20).display_maximum(500).when(normal),
                setting::integer("timing_randomness_ms", {"Timing Randomness"}, &model::timing_randomness_ms, 0, 250, "%d ms").display_maximum(150).when(normal),
                setting::integer("initial_reaction_ms", {"Initial Reaction"}, &model::initial_reaction_ms, 0, 1000, "%d ms").display_minimum(30).display_maximum(500).when(normal),
                setting::integer("confirmation_timeout_ms", {"Confirmation Timeout"}, &model::confirmation_timeout_ms, 80, 1200, "%d ms").when(normal),
                setting::toggle("legit_fill_offhand", {"Fill Offhand"}, &model::legit_fill_offhand).when(legit),
                setting::toggle("legit_fill_hotbar", {"Fill Hotbar"}, &model::legit_fill_hotbar).when(legit),
                setting::toggle("legit_offhand_first", {"Offhand First"}, &model::legit_offhand_first).when(legit),
                setting::integer("legit_hotbar_slot", {"Hotbar Slot"}, &model::legit_hotbar_slot, 1, 9, "Slot %d").when(legit),
                setting::toggle("legit_low_health_only", {"Low Health Offhand Only"}, &model::legit_low_health_only).when(legit),
                setting::number("legit_health_threshold", {"Health Threshold"}, &model::legit_health_threshold, 1.0f, 20.0f, "%.1f HP")
                    .when([](const model& c) { return c.mode == 1 && c.legit_low_health_only; }),
                setting::toggle("legit_preserve_hotbar", {"Preserve Hotbar"}, &model::legit_preserve_hotbar).when(legit),
                setting::toggle("legit_pause_with_cursor_item", {"Pause While Carrying Item"}, &model::legit_pause_with_cursor_item).when(legit),
                setting::toggle("legit_cursor_guided_swap", {"Cursor Guided Swap"}, &model::legit_cursor_guided_swap).when(legit),
                setting::integer("legit_cursor_travel_ms", {"Cursor Travel"}, &model::legit_cursor_travel_ms, 16, 180, "%d ms")
                    .when([](const model& c) { return c.mode == 1 && c.legit_cursor_guided_swap; }),
                setting::integer("legit_confirmation_timeout_ms", {"Confirmation Timeout"}, &model::legit_confirmation_timeout_ms, 120, 1800, "%d ms").when(legit),
                setting::integer("legit_reaction_delay_ms", {"Inventory Reaction"}, &model::legit_reaction_delay_ms, 40, 1200, "%d ms").display_maximum(600).when(legit),
                setting::integer("legit_action_delay_ms", {"Action Delay"}, &model::legit_action_delay_ms, 40, 1200, "%d ms").display_minimum(50).display_maximum(800).when(legit),
                setting::integer("legit_timing_variation_ms", {"Timing Variation"}, &model::legit_timing_variation_ms, 0, 250, "%d ms").display_maximum(180).when(legit),
            };
            return fields;
        }
    };
}
