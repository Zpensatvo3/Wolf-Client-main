#pragma once
#include "../services/notifications/notification_model.hpp"
#include <imgui/imgui.h>
#include <unordered_map>

namespace gui::notification_ui
{
    struct appearance
    {
        ImFont* font = nullptr;
        ImVec4 accent{.6f, .32f, 1.f, 1.f};
        ImVec4 secondary{.38f, .62f, 1.f, 1.f};
        bool reduce_motion = false;
    };
    class renderer
    {
        struct state { float offset = 0, alpha = 0; };
        std::unordered_map<std::uint64_t, state> animations_;
        int position_ = -1;
    public:
        void clear() { animations_.clear(); position_ = -1; }
        void render(const services::notifications::snapshot& snapshot, const ImGuiViewport& viewport,
            const appearance& style);
    };
}
