#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"
#include <vector>

namespace services::game::client_bridge
{
    struct walk_snapshot
    {
        jobjectArray packet = nullptr;
        bool alive = false, typing = false;
        int screen = 3; // gameplay/inventory/container/other, as in native policy.
        std::array<int, 12> descriptors{};
        std::array<bool, 6> physical{};
    };
    struct glow_snapshot { jobjectArray packet = nullptr, players = nullptr; int count = 0; };
    hybrid::result capture_walk(walk_snapshot& out);
    bool apply_walk(const walk_snapshot& observed, const std::array<bool, 6>& desired);
    bool release_walk(bool vanilla = false);
    hybrid::result capture_glow(glow_snapshot& out);
    bool apply_glow(const glow_snapshot& observed, const std::vector<bool>& allowed);
    bool release_glow();
    void shutdown();
}
