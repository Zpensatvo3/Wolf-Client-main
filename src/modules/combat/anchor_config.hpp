#pragma once
#include "../core/setting_schema.hpp"
#include <array>

namespace anchor_engine
{
    enum class profile { legit, blatant, safe };
    struct config
    {
        bool hybrid_slot_input = false, camera_movement = true;
        int switch_delay = 28, action_delay = 38, shield_delay = 45;
        int return_delay = 24, timing_variance = 14;
        float camera_strength = .28f;
        bool operator==(const config&) const = default;
    };
    inline std::span<const modules::settings::descriptor<config>> schema(profile mode)
    {
        using d = modules::settings::descriptor<config>;
        static const auto build = [](profile p) {
            const bool visible = p != profile::blatant;
            return std::array{
                d::toggle("hybrid_slot_input", {"Hybrid Slot Input","Hibrit Slot Girisi"}, &config::hybrid_slot_input),
                d::toggle("camera_jitter", {"Camera Movement","Kamera Hareketi"}, &config::camera_movement, visible),
                d::integer("jitter_ms", {"Timing Variance","Zamanlama Sapmasi"}, &config::timing_variance, 0, 80),
                d::number("camera_jitter_strength", {"Camera Strength","Kamera Siddeti"}, &config::camera_strength, 0.f, 1.2f, "%.2f", &config::camera_movement),
                d::integer("switch_delay", {"Switch Delay","Gecis Gecikmesi"}, &config::switch_delay, 0, 180, "%d ms"),
                d::integer("action_delay", {"Action Delay","Islem Gecikmesi"}, &config::action_delay, 0, 200, "%d ms"),
                d::integer("return_delay", {"Return Delay","Donus Gecikmesi"}, &config::return_delay, 0, 160, "%d ms"),
                d::integer("aim_delay", {"Shield Delay","Koruma Gecikmesi"}, &config::shield_delay, 0, 220, "%d ms")
            };
        };
        static const auto legit = build(profile::legit);
        static const auto safe = build(profile::safe);
        static const auto blatant = [] {
            auto fields = build(profile::blatant);
            for (std::size_t i = 1; i < fields.size(); ++i) fields[i].visible = false;
            return fields;
        }();
        const auto& fields = mode == profile::safe ? safe : mode == profile::blatant ? blatant : legit;
        // Only Safe Anchor serialized shield delay in older configs.
        return {fields.data(), mode == profile::safe ? fields.size() : fields.size() - 1};
    }
}
