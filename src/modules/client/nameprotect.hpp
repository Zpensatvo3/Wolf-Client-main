#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/nameprotect_settings.hpp"
#include <memory>

namespace modules
{
    class nameprotect final : public module
    {
    public:
        nameprotect();
        ~nameprotect() override;
        void run(::cache&) override {}
        void on_disable(::cache&) override { services::privacy::publish(); }
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_render_shutdown() override;
        void render_options() override;
        float get_options_height() override { return 170.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v,module_settings::nameprotect::schema()); }
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        modules::settings::store<module_settings::nameprotect> settings_;
    };
}
