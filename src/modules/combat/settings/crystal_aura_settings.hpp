#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct crystal_aura
    {
        int target_mode = 0;
        bool mob_ground_test = true;
        int action_delay_ms = 80;
        int switch_delay_ms = 60;
        int launch_timeout_ms = 900;
        int confirm_timeout_ms = 1200;
        bool place_enabled = true, break_enabled = true, auto_switch = true, restore_slot = true;
        bool support = true, damage_check = true;
        int place_delay_ms = 100, break_delay_ms = 100;
        float target_range = 6.f, place_range = 4.5f, break_range = 3.f;
        float min_damage = 2.f, max_self_damage = 12.f;
        bool operator==(const crystal_aura&) const = default;
        static std::span<const modules::settings::descriptor<crystal_aura>> schema()
        {
            using d = modules::settings::descriptor<crystal_aura>;
            static const modules::settings::label targets[]{
                {"Players"}, {"Players + Mobs"},
                {"Mobs Only (Test)"}
            };
            static const d fields[]{
                d::choice("target_mode", {"Targets"}, &crystal_aura::target_mode, targets),
                d::toggle("place_enabled", {"Place Crystals"}, &crystal_aura::place_enabled),
                d::toggle("break_enabled", {"Break Crystals"}, &crystal_aura::break_enabled),
                d::toggle("auto_switch", {"Auto Switch"}, &crystal_aura::auto_switch),
                d::toggle("restore_slot", {"Restore Slot"}, &crystal_aura::restore_slot),
                d::toggle("support", {"Place Obsidian Support"}, &crystal_aura::support),
                d::number("target_range", {"Target Range"}, &crystal_aura::target_range, 1.f, 12.f),
                d::number("place_range", {"Place Range"}, &crystal_aura::place_range, 1.f, 6.f),
                d::number("break_range", {"Break Range"}, &crystal_aura::break_range, 1.f, 6.f),
                d::integer("place_delay_ms", {"Place Delay"}, &crystal_aura::place_delay_ms, 0, 1000, "%d ms"),
                d::integer("break_delay_ms", {"Break Delay"}, &crystal_aura::break_delay_ms, 0, 1000, "%d ms"),
                d::toggle("damage_check", {"Self / Friend Protection"}, &crystal_aura::damage_check),
                d::number("min_damage", {"Minimum Target Damage"}, &crystal_aura::min_damage, 0.f, 36.f),
                d::number("max_self_damage", {"Maximum Self Damage"}, &crystal_aura::max_self_damage, 0.f, 36.f)
                    .when([](const crystal_aura& s) { return s.damage_check; }),
                d::integer("switch_delay_ms", {"Slot Switch Delay"}, &crystal_aura::switch_delay_ms, 50, 300, "%d ms"),
                d::integer("confirm_timeout_ms", {"Result Timeout"}, &crystal_aura::confirm_timeout_ms, 500, 3000, "%d ms"),
                // Read old profiles without retaining the removed sword/airborne sequence.
                d::toggle("mob_ground_test", {"Legacy Mob Wait"}, &crystal_aura::mob_ground_test).hidden(),
                d::integer("launch_timeout_ms", {"Legacy Airborne Wait"}, &crystal_aura::launch_timeout_ms, 300, 2000).hidden(),
                d::integer("action_delay_ms", {"Legacy Action Delay"}, &crystal_aura::action_delay_ms, 50, 500).hidden()
            };
            return fields;
        }
    };
}
