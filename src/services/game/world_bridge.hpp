#pragma once
#include <atomic>
#include <cstdint>
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace trigger_engine { struct config; }
namespace jump_reset_config { struct config; }
namespace module_settings { struct anchor_macro; struct auto_totem; struct crystal_aura; }

namespace services::game::world_bridge
{
    struct edge_snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr, key = nullptr, binding = nullptr;
        std::array<double, 13> data{};
    };
    struct pearl_snapshot { jobjectArray packet = nullptr; int selected = -1; bool owned = false; std::array<bool, 9> pearls{}; };
    hybrid::result capture_edge(edge_snapshot& out);
    bool support(const edge_snapshot& observed, const std::array<int, 24>& positions, std::array<int, 8>& air);
    bool force_sneak(const edge_snapshot& observed);
    bool release_sneak();
    hybrid::result capture_pearl(pearl_snapshot& out);
    bool begin_pearl(const pearl_snapshot& observed, int slot);
    bool use_pearl();
    bool release_pearl();
    // Hit Crystal's decisions, timers, world reads and writes all execute in Java.
    void tick_crystal(bool active, bool auto_switch, bool restore_slot, int place_delay, int break_delay);
    void cancel_crystal(); // Client-thread ownership cleanup; no new interaction.
    std::array<int,3> tick_aura(const module_settings::crystal_aura& settings, bool active,
        const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation);
    void cancel_aura();
    void tick_trigger(const trigger_engine::config& settings, bool active, const std::atomic<std::uint64_t>& generation);
    void cancel_trigger();
    void tick_jump(const jump_reset_config::config& settings, bool active, const std::atomic<std::uint64_t>& generation);
    void cancel_jump();
    void tick_anchor(const module_settings::anchor_macro& settings, bool active, const std::atomic<std::uint64_t>& generation,
        std::uint64_t expected_generation);
    void cancel_anchor();
    void set_safe_anchor_active(bool active);
    bool is_safe_anchor_ticking();
    void tick_totem(const module_settings::auto_totem& settings, bool active,
        const std::atomic<std::uint64_t>& generation, std::uint64_t expected_generation);
    void cancel_totem();
    void shutdown();
}
