#include "../src/modules/misc/bridge_assist_logic.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace modules::bridge_assist_logic;
    const auto points = samples({.5, 0, .8, 64, .2, 1.4, .8}, .08f);
    assert(points && (*points)[1] == 63);
    assert((*points)[0] == 0 && (*points)[12] == 1); // Predicted support crosses block boundary.
    assert(!samples({0, 0, 2, 64, 0, 1, 1}, .08f));
    assert(!samples({0, 0, 1e20, 64, 0, 1e20, 1}, .08f));
    engine state;
    auto now = engine::clock::time_point{} + std::chrono::seconds(1);
    assert(state.update(true, false, false, now, 55) == action::none);
    assert(state.update(true, false, false, now, 55) == action::force);
    assert(state.update(false, true, true, now + std::chrono::milliseconds(10), 55) == action::force);
    assert(state.update(false, true, true, now + std::chrono::milliseconds(60), 55) == action::force);
    assert(state.update(false, true, true, now + std::chrono::milliseconds(61), 55) == action::release);
    state = {};
    state.update(true, false, true, now, 0);
    assert(state.update(true, false, true, now, 0) == action::none); // Don't own manual sneak.
    std::cout << "PASS original bridge geometry, confirmation and release rules\n";
}
