#include "../src/modules/misc/auto_tool_scoring.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using modules::auto_tool_logic::is_better_score;
    assert(is_better_score({1, true}, {100, false}));
    assert(!is_better_score({100, false}, {1, true}));
    assert(!is_better_score({8, true}, {8, true}));
    assert(!is_better_score({8.005f, true}, {8, true}));
    assert(is_better_score({8.02f, true}, {8, true}));
    assert(!is_better_score({1, false}, {1, false}));
    std::cout << "Auto Tool original suitability / speed / epsilon policy passed\n";
}
