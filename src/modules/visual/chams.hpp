#pragma once
#include "../module_support.hpp"
#include <memory>

namespace chams_detail { struct runtime; }
namespace modules
{
    class chams : public module
    {
    public:
        chams();
        ~chams() override;
        void run(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        void render_options() override;
        float get_options_height() override { return 440.f; }
        void visit_config(config_visitor&) override;
    private:
        struct settings_type
        {
            float range = 64.f, opacity = .65f;
            bool glow = true;
            float glow_strength = .85f, glow_radius = 8.f;
            bool through_walls = true, theme_color = true, ignore_friends = true;
            float visible_color[4]{.60f, .32f, 1.f, 1.f};
            float hidden_color[4]{.94f, .25f, .36f, 1.f};
        } settings;
        std::mutex settings_mutex;
        std::atomic_bool reset_requested{true};
        std::unique_ptr<chams_detail::runtime> state;
    };
}
