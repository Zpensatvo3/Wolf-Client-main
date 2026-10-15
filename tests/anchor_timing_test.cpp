#include "../src/modules/combat/settings/anchor_macro_settings.hpp"
#include "../src/modules/combat/anchor_timing.hpp"
#include <cassert>
#include <iostream>
#include <set>

int main()
{
    using namespace anchor_engine;
    module_settings::anchor_macro settings;
    assert(settings.selected_profile() == profile::legit);
    settings.legit_randomize = false;
    settings.legit_switch_delay = 30;
    settings.legit_place_delay = 40;
    settings.legit_charge_delay = 50;
    settings.legit_detonate_delay = 60;
    settings.legit_return_delay = 25;
    auto c = settings.selected_config();
    std::mt19937_64 random{42};
    assert(c.separate_delays && c.timing_variance == 0);
    assert(sequence_delay(c,timing_stage::place,random) == 70);
    assert(sequence_delay(c,timing_stage::charge,random) == 80);
    assert(sequence_delay(c,timing_stage::detonate,random) == 90);
    assert(sequence_delay(c,timing_stage::restore,random) == 25);
    settings.mode = 1;
    assert(settings.selected_profile() == profile::blatant);
    c = settings.selected_config();
    assert(sequence_delay(c,timing_stage::place,random) == 0);
    // Blatant now honors every configured speed instead of forcing zero.
    settings.blatant_switch_delay = 15; settings.blatant_place_delay = 35;
    settings.blatant_charge_delay = 45; settings.blatant_detonate_delay = 55; settings.blatant_return_delay = 65;
    c = settings.selected_config();
    assert(sequence_delay(c,timing_stage::place,random) == 50);
    assert(sequence_delay(c,timing_stage::charge,random) == 60);
    assert(sequence_delay(c,timing_stage::detonate,random) == 70);
    assert(sequence_delay(c,timing_stage::restore,random) == 65);
    settings.blatant_randomize = true; settings.blatant_variance = 10;
    c = settings.selected_config();
    std::set<int> observed;
    for (int i = 0; i < 1000; ++i)
    {
        const int delay = sequence_delay(c,timing_stage::place,random);
        assert(delay >= 30 && delay <= 70); observed.insert(delay);
        const int restore = sequence_delay(c,timing_stage::restore,random);
        assert(restore >= 55 && restore <= 75);
    }
    assert(observed.size() > 10);
    for (int base : {-1000,0,1,150,499,500,1000})
        for (int variance : {-999,0,14,150,999})
            for (int i = 0; i < 100; ++i)
            {
                const int result = random_delay(base,variance,random);
                assert(result >= 0 && result <= 500);
                assert(result >= std::max(0,std::clamp(base,0,500)-std::clamp(variance,0,150)));
                assert(result <= std::min(500,std::clamp(base,0,500)+std::clamp(variance,0,150)));
            }
    settings.mode = 0;
    c = settings.selected_config();
    assert(c.switch_delay == 30 && c.place_delay == 40 && c.charge_delay == 50);
    assert(c.timing_variance == 0); // Switching modes never copies the other profile's values.
    std::cout << "Anchor timing: independent profiles/stages, Blatant delays and bounded random variation passed\n";
}
