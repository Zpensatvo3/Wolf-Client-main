#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct shield_breaker
    {
        int switch_delay = 0; // Legacy serialized field; the previous native implementation never used it.
        int switch_ticks = 0, hit_delay = 0;
        bool switch_back = true, stun = false, click_simulation = false, hold_axe = false;
        int return_delay = 110;
        int config_axe_slot = 1;
        bool operator==(const shield_breaker&) const = default;
        static std::span<const modules::settings::descriptor<shield_breaker>> schema()
        {
            using model = shield_breaker;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::integer("switch_ticks", {"Switch Delay"}, &model::switch_ticks, 0, 20, "%d ticks"),
                setting::integer("hit_delay", {"Hit Delay"}, &model::hit_delay, 0, 20, "%d ticks"),
                setting::toggle("switch_back", {"Switch Back"}, &model::switch_back),
                setting::toggle("stun", {"Stun (Standalone)"}, &model::stun),
                setting::toggle("click_simulation", {"Click Simulation"}, &model::click_simulation),
                setting::toggle("hold_axe", {"Hold Axe"}, &model::hold_axe),
                setting::integer("axe_slot", {"Preferred Axe Slot"}, &model::config_axe_slot, 1, 9, "Slot %d"),
                setting::integer("return_delay", {"Return Delay"}, &model::return_delay, 75, 500, "%d ms"),
                setting::integer("switch_delay", {"switch_delay"}, &model::switch_delay, -1000000, 1000000, "%d").hidden(),
            };
            return fields;
        }
    };
}
