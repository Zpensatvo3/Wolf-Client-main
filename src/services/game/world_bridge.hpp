#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace services::game::world_bridge
{
    struct edge_snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr, key = nullptr, binding = nullptr;
        std::array<double, 13> data{};
    };
    struct pearl_snapshot { jobjectArray packet = nullptr; int selected = -1; bool owned = false; std::array<bool, 9> pearls{}; };
    hybrid::result capture_edge(edge_snapshot& out);
    bool support(const edge_snapshot& observed, const std::array<int, 24>& positions, std::array<int, 8>& air);
    bool force_sneak(const edge_snapshot& observed);
    bool release_sneak();
    hybrid::result capture_pearl(pearl_snapshot& out);
    bool begin_pearl(const pearl_snapshot& observed, int slot);
    bool use_pearl();
    bool release_pearl();
    void shutdown();
}
