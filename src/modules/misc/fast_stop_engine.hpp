#pragma once
#include <cmath>
#include <optional>
#include <utility>

namespace modules::fast_stop_logic
{
    struct velocity
    {
        double x = 0, y = 0, z = 0;
        bool operator==(const velocity&) const = default;
    };

    struct observation
    {
        bool active = false, focused = false, screen_open = false;
        bool alive = false, grounded = false;
        bool in_water = false, in_lava = false, climbing = false;
        bool gliding = false, riding = false, flying = false, jumping = false;
        int hurt_time = 0;
        float forward = 0, sideways = 0;
        bool directional_pressed = false;
        velocity motion{};
    };

    class engine
    {
    public:
        void reset() { moving_ = false; }
        std::optional<velocity> update(const observation& state)
        {
            if (!state.active || !state.focused || state.screen_open || !state.alive || !state.grounded ||
                state.in_water || state.in_lava || state.climbing || state.gliding || state.riding ||
                state.flying || state.jumping || state.hurt_time != 0 || state.motion.y > 0 ||
                !std::isfinite(state.forward) || !std::isfinite(state.sideways) ||
                !std::isfinite(state.motion.x) || !std::isfinite(state.motion.y) || !std::isfinite(state.motion.z))
            {
                reset();
                return std::nullopt;
            }
            if (state.forward != 0 || state.sideways != 0)
            {
                moving_ = true;
                return std::nullopt;
            }
            // Check actual bindings too: opposite keys can cancel both axes.
            if (state.directional_pressed) return std::nullopt;
            const bool released = std::exchange(moving_, false);
            if (!released || (state.motion.x == 0 && state.motion.z == 0)) return std::nullopt;
            // One write per release, never a continuous velocity suppressor.
            return velocity{0, state.motion.y, 0};
        }
    private:
        bool moving_ = false;
    };
}
