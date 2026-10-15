#pragma once
#include "../module_support.hpp"
#include <memory>

namespace vape_bridge { struct runtime; }
namespace modules
{
    enum class vape_kind { aim, bow, left, right, sprint, trigger };

    // The bridge owns IO/lifecycle only. Decisions stay in the supplied headers.
    class vape_module : public module
    {
    public:
        explicit vape_module(vape_kind kind);
        ~vape_module() override;
        void run(::cache&) override {}
        void on_enable(::cache&) override;
        void on_disable(::cache&) override;
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void render_options() override;
        float get_options_height() override;
        void visit_config(config_visitor&) override;
    private:
        struct options
        {
            float min_cps = 9.f, max_cps = 13.5f;
            int randomization = 2;
            bool jitter = false, weapons_only = true, protect_blocks = true;
            bool hold_to_click = true, blocks_only = true, offhand = true;
            float range = 5.f, fov = 180.f, horizontal = 5.f, vertical = 5.f;
            bool aim_vertical = false, strafe_increase = true, require_mouse = true;
            int sort = 0;
            int aim_mode = 0, target_area = 1;
            float bow_speed = 8.f;
            bool prediction = true, high_arc = false, cancel_invis = true;
            bool air_crits = true, shield_check = true, first_hit = false;
            float miss_chance = 0.f, early_chance = 0.f, delay_min = 0.f, delay_max = 0.f;
            int mouse_delay_min = 0, mouse_delay_max = 80;
            bool operator==(const options&) const = default;
        } settings;
        const vape_kind kind;
        std::mutex settings_mutex;
        std::atomic_bool reset_requested{true};
        std::unique_ptr<vape_bridge::runtime> state;
        std::string runtime_issue;
        const char* conflict() const;
    };

    module* create_vape_aim();
    module* create_vape_bow();
    module* create_vape_left();
    module* create_vape_right();
    module* create_vape_sprint();
    module* create_vape_trigger();
}
