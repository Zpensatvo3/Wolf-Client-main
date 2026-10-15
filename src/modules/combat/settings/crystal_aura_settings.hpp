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
        bool operator==(const crystal_aura&) const = default;
        static std::span<const modules::settings::descriptor<crystal_aura>> schema()
        {
            using d = modules::settings::descriptor<crystal_aura>;
            static const modules::settings::label targets[]{
                {"Players", "Oyuncular"}, {"Players + Mobs", "Oyuncular + Moblar"},
                {"Mobs Only (Test)", "Yalnizca Moblar (Test)"}
            };
            static const d fields[]{
                d::choice("target_mode", {"Targets", "Hedefler"}, &crystal_aura::target_mode, targets),
                d::toggle("mob_ground_test", {"Skip Airborne Wait for Mobs", "Moblarda Havalanma Bekleme"}, &crystal_aura::mob_ground_test)
                    .when([](const crystal_aura& s) { return s.target_mode != 0; }),
                d::integer("action_delay_ms", {"Action Delay", "Islem Gecikmesi"}, &crystal_aura::action_delay_ms, 50, 500, "%d ms"),
                d::integer("switch_delay_ms", {"Slot Switch Delay", "Slot Gecis Gecikmesi"}, &crystal_aura::switch_delay_ms, 50, 300, "%d ms"),
                d::integer("launch_timeout_ms", {"Airborne Wait", "Havalanma Beklemesi"}, &crystal_aura::launch_timeout_ms, 300, 2000, "%d ms"),
                d::integer("confirm_timeout_ms", {"Result Timeout", "Sonuc Zaman Asimi"}, &crystal_aura::confirm_timeout_ms, 500, 3000, "%d ms")
            };
            return fields;
        }
    };
}
