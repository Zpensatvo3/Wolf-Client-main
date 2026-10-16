#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/target_hud_settings.hpp"
#include <imgui/imgui.h>
#include <memory>

namespace modules
{
    class target_hud : public module
    {
    public:
        target_hud();
        ~target_hud() override;
        void run(::cache&) override {}
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        void render_options() override;
        float get_options_height() override { return 190.f; }
        void visit_config(config_visitor& visitor) override
        { settings_.visit(visitor, module_settings::target_hud::schema()); }

        struct bounds { float x, y, w, h; };
        bounds get_bounds(float display_width, float display_height) const;
        void render_hud_preview(ImDrawList* draw, float x, float y);
        module_settings::target_hud get_settings() const { return settings_.read(); }
        void set_placement(float x, float y, float scale);

    private:
        struct runtime;
        std::unique_ptr<runtime> runtime_;
        modules::settings::store<module_settings::target_hud> settings_;
        std::atomic<unsigned> reset_generation_{0};
        void refresh(bool preview);
        void draw_card(ImDrawList* draw, float x, float y, bool preview);
    };
}
