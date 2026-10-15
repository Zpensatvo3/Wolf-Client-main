#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/crystal_aura_settings.hpp"
#include <memory>

namespace modules
{
    class crystal_aura final : public module
    {
    public:
        crystal_aura();
        ~crystal_aura() override;
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        void on_disable(::cache&) override;
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 420.f; }
        void visit_config(config_visitor& visitor) override
        { settings_.visit(visitor, module_settings::crystal_aura::schema()); }
    private:
        struct runtime;
        std::unique_ptr<runtime> runtime_;
        settings::store<module_settings::crystal_aura> settings_;
        std::atomic_bool reset_requested_{true};
    };
}
