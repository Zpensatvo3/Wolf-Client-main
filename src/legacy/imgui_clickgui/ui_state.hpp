#pragma once

#include <imgui/imgui.h>

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

    struct preferences
    {
        int version = 1;
        layout_mode layout = layout_mode::vertical_columns;
        energy_quality energy = energy_quality::medium;
        bool reduce_motion = false;
        bool has_geometry = false;
        ImVec2 window_position = ImVec2(0.0f, 0.0f);
        ImVec2 window_size = ImVec2(1080.0f, 640.0f);
    };

    preferences& get_preferences();
    void load_preferences();
    void mark_preferences_dirty();
    void flush_preferences(bool force = false);
    void reset_preferences();
    bool consume_geometry_reset();
    bool geometry_reset_pending();
}
