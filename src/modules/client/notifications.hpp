#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/notifications_settings.hpp"

namespace modules
{
    class notifications final : public module
    {
    public:
        notifications();
        ~notifications() override;
        void run(::cache&) override;
        void on_disable(::cache&) override;
        void render_options() override;
        float get_options_height() override { return 290.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::notifications::schema()); }
    private:
        modules::settings::store<module_settings::notifications> settings_;
    };
}
