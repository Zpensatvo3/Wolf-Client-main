#pragma once
#include "hybrid_bridge.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace services::game::items
{
    struct tool_snapshot
    {
        jobjectArray packet = nullptr; // Caller-owned JNI local frame only.
        int selected = -1;
        std::array<float, 9> speed{};
        std::array<bool, 9> suitable{};
    };
    struct exp_snapshot
    {
        jobjectArray packet = nullptr;
        jobject player = nullptr, world = nullptr;
        bool holding_exp = false;
    };
    hybrid::result capture_tools(bool require_click, tool_snapshot& out);
    bool select_tool(const tool_snapshot& observed, int slot, bool require_click);
    hybrid::result capture_exp(exp_snapshot& out);
    bool use_exp(const exp_snapshot& observed);
    struct click_snapshot { jobjectArray packet = nullptr; bool right = false; };
    hybrid::result capture_click(bool right, bool only_blocks, click_snapshot& out);
    bool click(const click_snapshot& observed, bool game_input);
    bool jitter(const click_snapshot& observed, float yaw_delta, float pitch_delta);
    void shutdown(); // After render callbacks have joined.
}
