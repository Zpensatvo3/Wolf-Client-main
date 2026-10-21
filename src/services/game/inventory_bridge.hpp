#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"
#include <string>
#include <vector>

namespace services::game::inventory_bridge
{
    enum class feature : jint { chest = 1, armor = 2, elytra = 3, refill = 4, mouse = 5 };
    struct snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr, screen = nullptr, handler = nullptr;
        bool cursor_empty = false;
        int stage = 0;
        std::array<std::string, 40> keys{};
        std::vector<int> candidates;
    };
    hybrid::result capture(feature operation, bool allow_closed, snapshot& out);
    bool act(const snapshot& observed, int source, int target, int stage, bool allow_closed);
    struct container_slot
    {
        int id = -1, index = -1, count = 0, limit = 0;
        bool empty = true, healing = false, enabled = false, can_insert = false, compatible = false;
    };
    struct container_snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr, screen = nullptr, handler = nullptr;
        feature operation = feature::refill;
        int sync_id = -1, hover = -1, cursor_count = 0, cursor_limit = 0;
        bool cursor_empty = true, confirmed = false;
        std::vector<container_slot> slots;
    };
    hybrid::result capture_container(feature operation, container_snapshot& out);
    bool container_act(const container_snapshot& observed, int action, int source, int target = -1);
    void tick_mouse(bool active, bool right_tweak, bool left_with_item, bool left_without_item);
    void cancel(feature operation); // Drops ownership only; never manufactures cleanup clicks.
    void shutdown();
}
