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
        bool separate_delays = false;
        int place_delay = 0, charge_delay = 0, detonate_delay = 0;
        bool operator==(const config&) const = default;
    };
    inline std::span<const modules::settings::descriptor<config>> schema(profile mode)
    {
        using d = modules::settings::descriptor<config>;
        static const auto build = [](profile p) {
            const bool visible = p != profile::blatant;
            return std::array{
                d::toggle("hybrid_slot_input", {"Hybrid Slot Input"}, &config::hybrid_slot_input),
                d::toggle("camera_jitter", {"Camera Movement"}, &config::camera_movement, visible),
                d::integer("jitter_ms", {"Timing Variance"}, &config::timing_variance, 0, 80),
                d::number("camera_jitter_strength", {"Camera Strength"}, &config::camera_strength, 0.f, 1.2f, "%.2f", &config::camera_movement),
                d::integer("switch_delay", {"Switch Delay"}, &config::switch_delay, 0, 180, "%d ms"),
                d::integer("action_delay", {"Action Delay"}, &config::action_delay, 0, 200, "%d ms"),
                d::integer("return_delay", {"Return Delay"}, &config::return_delay, 0, 160, "%d ms"),
                d::integer("aim_delay", {"Shield Delay"}, &config::shield_delay, 0, 220, "%d ms")
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
