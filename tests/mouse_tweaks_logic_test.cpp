#include "../src/modules/misc/mouse_tweaks_logic.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace modules::mouse_tweaks_logic;
    module_settings::mouse_tweaks settings;
    gestures state;
    auto out = state.update(settings, {2, false, true, true});
    assert(out.right[0] == -1 && out.right[1] == -1); // Vanilla half-stack pickup.
    out = state.update(settings, {3, false, true, false});
    assert(out.right[1] == -1);
    state = {};
    state.update(settings, {2, false, true, false});
    out = state.update(settings, {3, false, true, false});
    assert(out.right[0] == 2 && out.right[1] == 3);
    out = state.update(settings, {2, false, true, false});
    assert(out.right[0] == -1 && out.right[1] == 2); // Revisit deposits again.
    state.update(settings, {2, false, false, false});
    assert(state.update(settings, {3, false, false, false}).right[1] == -1);
    state = {};
    assert(state.update(settings, {2, true, false, true}).left == -1);
    assert(state.update(settings, {3, true, false, false}).left == 3);
    assert(state.update(settings, {3, true, false, false}).left == -1); // No repeated click in same slot.
    state = {}; settings.left_click_with_item = settings.left_click_without_item = false;
    state.update(settings, {2, true, false, true});
    assert(state.update(settings, {3, true, false, false}).left == -1);
    std::cout << "PASS original mouse drag transitions\n";
}
