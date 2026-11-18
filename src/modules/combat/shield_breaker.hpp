#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/shield_breaker_settings.hpp"
namespace modules {
class shield_breaker : public module {
public:
    shield_breaker() : module("shield breaker","Combat") { active_instance.store(this); }
    ~shield_breaker() override { auto* expected=this; active_instance.compare_exchange_strong(expected,nullptr); }
    void run(::cache&) override {} // No worker-side Minecraft reads or writes.
    void render_overlay() override;
    bool render_when_disabled() const override { return true; }
    void on_render_shutdown() override;
    void cancel_on_client_thread();
    void on_enable(::cache&) override { generation_.fetch_add(1,std::memory_order_acq_rel); }
    void on_disable(::cache&) override { generation_.fetch_add(1,std::memory_order_acq_rel); }
    void render_options() override;
    float get_options_height() override { return 360.f; }
    void visit_config(config_visitor& v) override {
        if (settings_.visit(v,module_settings::shield_breaker::schema())) generation_.fetch_add(1,std::memory_order_acq_rel);
    }
    static bool gate_left_click(); // Input routing only; Java owns all preparation.
private:
    inline static std::atomic<shield_breaker*> active_instance{nullptr};
    std::atomic<std::uint64_t> generation_{0};
    modules::settings::store<module_settings::shield_breaker> settings_;
};
}
