#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/crystal_aura_settings.hpp"
#include <array>

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
        void on_enable(::cache&) override { invalidate(); }
        void on_disable(::cache&) override { invalidate(); }
        void on_render_shutdown() override;
        void cancel_on_client_thread();
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 680.f; }
        void visit_config(config_visitor& visitor) override
        {
            if (settings_.visit(visitor, module_settings::crystal_aura::schema()))
                generation_.fetch_add(1,std::memory_order_release);
        }
    private:
        void invalidate() { generation_.fetch_add(1,std::memory_order_release); reset_requested_.store(true,std::memory_order_release); }
        std::array<int,3> status_{-1,0,0};
        std::atomic<std::uint64_t> generation_{1};
        settings::store<module_settings::crystal_aura> settings_;
        std::atomic_bool reset_requested_{true};
    };
}
