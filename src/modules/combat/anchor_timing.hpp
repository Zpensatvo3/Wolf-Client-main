#pragma once
#include "anchor_config.hpp"
#include <random>

namespace anchor_engine
{
    enum class timing_stage { place, charge, detonate, restore };
    template<class Rng>
    int random_delay(int base, int variance, Rng& random)
    {
        base = std::clamp(base,0,500);
        variance = std::clamp(variance,0,150);
        const int low = std::max(0,base-variance), high = std::min(500,base+variance);
        return std::uniform_int_distribution<int>(low,high)(random);
    }
    template<class Rng>
    int sequence_delay(const config& c, timing_stage stage, Rng& random)
    {
        const auto sample = [&](int value) { return random_delay(value,c.timing_variance,random); };
        if (stage == timing_stage::restore) return sample(c.return_delay);
        const int action = stage == timing_stage::place ? c.place_delay :
            stage == timing_stage::charge ? c.charge_delay : c.detonate_delay;
        return sample(c.switch_delay) + sample(action);
    }
}
