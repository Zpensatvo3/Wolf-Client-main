#include "../src/modules/misc/fast_stop_engine.hpp"
#include <cassert>
#include <iostream>
#include <limits>
#include <random>

namespace
{
    using namespace modules::fast_stop_logic;
    observation walking()
    {
        observation state;
        state.active = state.focused = state.alive = state.grounded = true;
        state.forward = 1;
        state.directional_pressed = true;
        state.motion = {.13, -.0784, -.17};
        return state;
    }
    observation released(observation state)
    {
        state.forward = state.sideways = 0;
        state.directional_pressed = false;
        return state;
    }
}

int main()
{
    using namespace modules::fast_stop_logic;
    engine engine;
    auto moving = walking();
    auto idle = released(moving);
    assert(!engine.update(idle)); // Enabling while idle never removes outside momentum.
    assert(!engine.update(moving));
    const auto result = engine.update(idle);
    assert(result && result->x == 0 && result->z == 0 && result->y == idle.motion.y);
    assert(!engine.update(idle)); // At most one write for the same release.
    idle.motion.x = .4;
    assert(!engine.update(idle)); // An impulse while standing is left alone.

    for (const auto field : {&observation::active, &observation::focused, &observation::alive, &observation::grounded})
    {
        engine.reset();
        assert(!engine.update(moving));
        auto blocked = released(moving);
        blocked.*field = false;
        assert(!engine.update(blocked));
        assert(!engine.update(released(moving))); // Blocked release cannot replay later.
    }
    for (const auto field : {&observation::screen_open, &observation::in_water, &observation::in_lava,
        &observation::climbing, &observation::gliding, &observation::riding, &observation::flying, &observation::jumping})
    {
        engine.reset();
        assert(!engine.update(moving));
        auto blocked = released(moving);
        blocked.*field = true;
        assert(!engine.update(blocked));
        assert(!engine.update(released(moving)));
    }
    for (int hurt : {-1, 1, 5, 10})
    {
        engine.reset();
        assert(!engine.update(moving));
        auto blocked = released(moving);
        blocked.hurt_time = hurt;
        assert(!engine.update(blocked));
        blocked.hurt_time = 0;
        assert(!engine.update(blocked)); // Does not cancel delayed knockback after hurtTime ends.
    }
    engine.reset();
    assert(!engine.update(moving));
    auto opposite = released(moving);
    opposite.directional_pressed = true;
    for (int frame = 0; frame < 120; ++frame) assert(!engine.update(opposite));
    assert(engine.update(released(moving))); // Only after every directional binding is up.

    for (float axis : {-1.f, -.5f, .5f, 1.f})
    {
        engine.reset();
        auto strafe = walking();
        strafe.forward = 0;
        strafe.sideways = axis;
        assert(!engine.update(strafe));
        assert(engine.update(released(strafe)));
    }
    const double invalids[]{std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    for (double invalid : invalids)
    {
        for (const auto field : {&velocity::x, &velocity::y, &velocity::z})
        {
            engine.reset();
            assert(!engine.update(moving));
            auto broken = released(moving);
            broken.motion.*field = invalid;
            assert(!engine.update(broken));
            assert(!engine.update(released(moving)));
        }
        for (const auto field : {&observation::forward, &observation::sideways})
        {
            engine.reset();
            assert(!engine.update(moving));
            auto broken = released(moving);
            broken.*field = static_cast<float>(invalid);
            assert(!engine.update(broken));
            assert(!engine.update(released(moving)));
        }
    }
    engine.reset();
    assert(!engine.update(moving));
    auto jumping = released(moving);
    jumping.motion.y = .42; // onGround can still be stale at the beginning of a jump.
    assert(!engine.update(jumping));
    assert(!engine.update(released(moving)));

    // Vertical motion is bit-for-bit preserved across arbitrary horizontal speeds.
    std::mt19937_64 random{19};
    std::uniform_real_distribution<double> speed(-5, 5);
    for (int trial = 0; trial < 1000; ++trial)
    {
        engine.reset();
        auto state = moving;
        state.motion = {speed(random), -std::abs(speed(random)), speed(random)};
        assert(!engine.update(state));
        const auto stopped = engine.update(released(state));
        assert((stopped && *stopped == velocity{0, state.motion.y, 0}));
    }
    engine.reset();
    assert(!engine.update(moving));
    engine.reset(); // World/player replacement, disconnect, disable or failed JNI read.
    assert(!engine.update(released(moving)));
    assert(!engine.update(moving));
    auto still = released(moving);
    still.motion.x = still.motion.z = 0;
    assert(!engine.update(still)); // No redundant setVelocity.
    std::cout << "FastStop: release edges, vertical preservation, environment/damage guards and resets passed\n";
}
