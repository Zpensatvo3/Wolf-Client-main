#pragma once
#include "trigger_engine.hpp"
#include "../../meta_jni.hpp"
#include <atomic>
#include <random>
#include <mutex>

namespace trigger_engine
{
    struct live_sampler final : sampler
    {
        std::mt19937 generator{std::random_device{}()};
        float cooldown(float a, float b) override { return a == b ? a : std::uniform_real_distribution<float>(a, b)(generator); }
        int reaction(int a, int b) override { return a == b ? a : std::uniform_int_distribution<int>(a, b)(generator); }
    };
    enum class preparation { none, shield, mace };
    struct adapter_runtime
    {
        runtime engine;
        live_sampler random;
        std::atomic<std::uint64_t> reset_generation{1};
        std::mutex submission_mutex; // Serializes reset with the last generation check + SendInput.
        std::uint64_t seen_reset = 0, session = 0, settings_revision = 0, weapon_revision = 0;
        jni::object_wrapper world{nullptr, true}, player{nullptr, true};
        int previous_slot = -1, prepared_slot = -1, original_slot = -1;
        int return_slot = -1, return_from_slot = -1;
        weapon_type previous_weapon = weapon_type::empty;
        preparation prepared = preparation::none;
    };
    void request_reset(adapter_runtime& state);
    void run_overlay(const config& settings, adapter_runtime& state, bool enabled);
    void render_options(config& settings, adapter_runtime& state, const char* id_suffix);
}
