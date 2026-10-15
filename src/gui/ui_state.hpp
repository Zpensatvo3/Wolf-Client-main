#pragma once

#include <imgui/imgui.h>
#include <array>

namespace gui::aegis
{
    enum class layout_mode : int
    {
        vertical_columns = 0,
        horizontal_workspace = 1
    };

    enum class energy_quality : int
    {
        low = 0,
        medium = 1,
        high = 2
    };

    struct hud_placement
    {
        float x = 10.f, y = 10.f, scale = 1.f;
        bool visible = true;
    };

    struct preferences
    {
        int version = 1;
        layout_mode layout = layout_mode::horizontal_workspace;
        energy_quality energy = energy_quality::medium;
        bool reduce_motion = false;
        bool has_geometry = false;
        ImVec2 window_position = ImVec2(0.0f, 0.0f);
        ImVec2 window_size = ImVec2(1280.0f, 620.0f);
        bool sound_effects = true;
        float sound_volume = 0.70f;
        bool has_hud_layout = false;
        std::array<hud_placement, 3> hud_layout{{ {10, 10, 1}, {10, 10, 1.12f}, {22, 60, 1.2f} }};
    };

    preferences& get_preferences();
    void load_preferences();
    void mark_preferences_dirty();
    void flush_preferences(bool force = false);
    void reset_preferences();
    bool consume_geometry_reset();
    bool geometry_reset_pending();
}
