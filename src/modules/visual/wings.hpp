#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/wings_settings.hpp"

namespace modules
{
    class wings final : public module
    {
    public:
        wings() : module("wings", "Visual", "wings") {}
        void run(::cache&) override {}
        void render_overlay() override;
        void render_options() override;
        float get_options_height() override { return 366.f; }
        void visit_config(config_visitor& visitor) override { settings_.visit(visitor, module_settings::wings::schema()); }
    private:
        modules::settings::store<module_settings::wings> settings_;
    };
}
