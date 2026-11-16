#pragma once
#include "../module_support.hpp"
#include "click_aim_config.hpp"
#include "legit_aim_engine.hpp"
#include "blatant_aim_engine.hpp"
#include <mutex>

namespace modules
{
    class click_aim_assist final : public module
    {
    public:
        click_aim_assist() : module("Click Aim Assist", "Combat", "click_aim_assist") {}
        void run(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override { request_reset(); }
        void on_disable(::cache&) override { request_reset(); }
        void on_render_shutdown() override;
        void render_options() override;
        float get_options_height() override { return 370; }
        void visit_config(config_visitor&) override;
    private:
        void request_reset() { generation.fetch_add(1, std::memory_order_release); }
        std::mutex mutex;
        click_aim::config settings;
        legit_aim_engine::runtime legit_state;
        blatant_aim_engine::runtime blatant_state;
        std::atomic_uint64_t generation{1};
        std::uint64_t worker_generation = 0, render_generation = 0;
    };
}
