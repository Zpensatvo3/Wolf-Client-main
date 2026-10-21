#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/anchor_macro_settings.hpp"

namespace modules
{
    class anchor_macro : public module
    {
    public:
        anchor_macro() : module("anchor macro","Combat","anchor_macro") {}
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        void on_enable(::cache&) override { generation_.fetch_add(1,std::memory_order_release); lifecycle_reset_.store(true,std::memory_order_release); }
        void on_disable(::cache&) override { generation_.fetch_add(1,std::memory_order_release); lifecycle_reset_.store(true,std::memory_order_release); }
        void on_render_shutdown() override { cancel_on_client_thread(); }
        void cancel_on_client_thread();
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 500.f; }
        void visit_config(config_visitor& v) override
        {
            if (settings_.visit(v,module_settings::anchor_macro::schema()))
                generation_.fetch_add(1,std::memory_order_release);
        }
    private:
        settings::store<module_settings::anchor_macro> settings_;
        std::atomic<std::uint64_t> generation_{1};
        std::atomic_bool lifecycle_reset_{false};
    };
}
