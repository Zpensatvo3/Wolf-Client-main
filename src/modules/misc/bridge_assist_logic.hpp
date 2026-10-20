#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>

namespace modules::bridge_assist_logic
{
    // vx,vz,minX,minY,minZ,maxX,maxZ. Native keeps the original geometry.
    inline std::optional<std::array<int, 24>> samples(const std::array<double, 7>& v, float offset)
    {
        for (double value : v) if (!std::isfinite(value)) return std::nullopt;
        if (!std::isfinite(offset) || v[2] > v[5] || v[4] > v[6]) return std::nullopt;
        const double cx = (v[2] + v[5]) * .5, cz = (v[4] + v[6]) * .5;
        const double hx = std::max(.025, (v[5] - v[2]) * .5 - static_cast<double>(std::clamp(offset, 0.f, .27f)));
        const double hz = std::max(.025, (v[6] - v[4]) * .5 - static_cast<double>(std::clamp(offset, 0.f, .27f)));
        std::array<int, 24> out{}; unsigned index = 0;
        for (int predicted = 0; predicted < 2; ++predicted)
            for (int x = 0; x < 2; ++x) for (int z = 0; z < 2; ++z) {
                const double sample_x = cx + (predicted ? v[0] * .35 : 0);
                const double sample_z = cz + (predicted ? v[1] * .35 : 0);
                const std::array<double, 3> point{sample_x + (x ? hx : -hx),
                    v[3] - .05, sample_z + (z ? hz : -hz)};
                for (double value : point) {
                    const double block = std::floor(value);
                    if (!std::isfinite(block) || block < (std::numeric_limits<int>::min)() || block > (std::numeric_limits<int>::max)()) return std::nullopt;
                    out[index++] = static_cast<int>(block);
                }
            }
        return out;
    }
    enum class action { none, force, release };
    struct engine
    {
        using clock = std::chrono::steady_clock;
        int edge = 0, safe = 0;
        clock::time_point release_at{};
        action update(bool raw, bool owned, bool pressed, clock::time_point now, int delay)
        {
            if (raw) { edge = std::min(edge + 1, 4); safe = 0; }
            else { edge = 0; safe = std::min(safe + 1, 5); }
            if (raw && (owned || edge >= 2)) {
                release_at = now + std::chrono::milliseconds(std::clamp(delay, 0, 300));
                return !owned && pressed ? action::none : action::force;
            }
            if (owned && (now < release_at || safe < 3)) return action::force;
            if (raw && !owned) return action::none;
            *this = {}; return action::release;
        }
    };
}
