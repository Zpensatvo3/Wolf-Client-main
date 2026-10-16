#pragma once
#include "settings/china_hat_settings.hpp"
#include "settings/wings_settings.hpp"
#include "settings/cape_settings.hpp"
#include "settings/trails_settings.hpp"
#include <memory>

namespace cosmetics
{
    // Native values only; owned and updated exclusively by the render thread.
    struct cape_motion
    {
        bool initialized = false;
        int entity = -1, age = 0;
        double time = 0, phase = 0, yaw = 0, x = 0, y = 0, z = 0;
        double speed = 0, sideways = 0, vertical = 0, turn = 0;
    };
    void draw_hat(const module_settings::china_hat& config);
    void draw_wings(const module_settings::wings& config);
    void draw_cape(const module_settings::cape& config, cape_motion& motion);
    class trail_state
    {
    public:
        trail_state();
        ~trail_state();
        void clear();
    private:
        struct runtime;
        std::unique_ptr<runtime> state_;
        friend void draw_trails(const module_settings::trails&,trail_state&);
    };
    void draw_trails(const module_settings::trails& config,trail_state& state);
}
