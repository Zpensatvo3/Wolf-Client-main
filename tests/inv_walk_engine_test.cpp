#include "../src/modules/misc/inv_walk_engine.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace inv_walk_engine;
    module_settings::inv_walk config;
    for (bool enabled : {false, true}) for (bool focused : {false, true})
        for (bool blocked : {false, true}) for (bool live : {false, true})
            for (bool typing : {false, true}) for (bool containers : {false, true})
                for (auto screen : {screen_kind::gameplay, screen_kind::inventory, screen_kind::container, screen_kind::other})
                {
                    config.allow_containers = containers;
                    observation state{enabled, focused, blocked, live, typing, screen};
                    const auto mode = choose_mode(config, state);
                    if (!enabled || !focused || blocked || !live) assert(mode == mode::blocked);
                    else if (screen == screen_kind::gameplay) assert(mode == mode::vanilla);
                    else if (!typing && (screen == screen_kind::inventory || (screen == screen_kind::container && containers)))
                        assert(mode == mode::inventory);
                    else assert(mode == mode::blocked); // Chat, sign, book, pause, search/rename.
                }

    for (unsigned mask = 0; mask < (1u << key_count); ++mask)
        for (bool jumping : {false, true}) for (bool sneaking : {false, true})
        {
            std::array<bool, key_count> physical{};
            for (unsigned index = 0; index < key_count; ++index) physical[index] = (mask & (1u << index)) != 0;
            config.allow_jump = jumping; config.allow_sneak = sneaking;
            const auto desired = desired_keys(config, physical);
            for (unsigned index = forward; index <= right; ++index) assert(desired[index] == physical[index]);
            assert(desired[jump] == (jumping && physical[jump]));
            assert(desired[sneak] == (sneaking && physical[sneak]));
        }
    // Losing focus, disabling, disconnecting or starting to type releases owned
    // presses even while hardware is held. Closing to gameplay preserves a hold.
    for (bool owned : {false, true}) for (bool current : {false, true})
        for (bool vanilla : {false, true}) for (bool physical : {false, true})
            assert(should_release(owned, current, vanilla, physical) == (owned && current && !(vanilla && physical)));
    assert(!should_release(false, true, false, false)); // Never release another owner's key.
    assert(!should_release(true, true, true, true)); // Hand real input back to vanilla.
    assert(should_release(true, true, false, true)); // Typing/focus-loss cancels movement.
    assert(should_release(true, true, true, false)); // Released/rebound hardware cannot stick.
    std::cout << "InvWalk: screen/focus/typing guards, movement masks and owned-key cleanup passed\n";
}
