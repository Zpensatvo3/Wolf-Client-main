#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/view_model_settings.hpp"
#include <memory>
namespace modules
{
    class view_model final : public module
    {
    public:
        view_model();
        ~view_model() override;
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_render_shutdown() override;
        void render_options() override;
        float get_options_height() override { return 405.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v,module_settings::view_model::schema()); }
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        modules::settings::store<module_settings::view_model> settings_;
    };
}
