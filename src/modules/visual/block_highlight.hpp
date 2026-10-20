#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "cosmetic_renderer.hpp"
namespace modules
{
    class block_highlight final : public module
    {
    public:
        block_highlight() : module("block highlight", "Visual", "block_highlight") {}
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override { reset_.fetch_add(1, std::memory_order_release); }
        void on_disable(::cache&) override { reset_.fetch_add(1, std::memory_order_release); }
        void on_render_shutdown() override { state_.clear(); }
        void render_options() override;
        float get_options_height() override { return 270.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::block_highlight::schema()); }
    private:
        modules::settings::store<module_settings::block_highlight> settings_;
        cosmetics::visual_effect_state state_;
        std::atomic<unsigned> reset_{0};
        unsigned applied_ = 0;
    };
}
