#pragma once
#include "settings/mouse_tweaks_settings.hpp"
#include <array>

namespace modules::mouse_tweaks_logic
{
    struct observation { int slot = -1; bool left = false, right = false, cursor_empty = true; };
    struct transitions { std::array<int, 2> right{-1, -1}; int left = -1; };
    struct gestures
    {
        int right_slot = -1, left_slot = -1;
        bool right_active = false, left_active = false, left_original = false;
        bool last_right = false, last_left = false, cursor_was_empty = true, cursor_known = false;
        transitions update(const module_settings::mouse_tweaks& settings, observation in)
        {
            transitions out;
            if (!cursor_known) { cursor_was_empty = in.cursor_empty; cursor_known = true; }
            if (!settings.right_click_tweak || !in.right) {
                right_active = false; right_slot = -1; left_original = false;
            } else if (in.right && !last_right && !cursor_was_empty) {
                right_active = true; right_slot = in.slot; left_original = false;
            }
            if ((!settings.left_click_with_item && !settings.left_click_without_item) || !in.left) {
                left_active = false; left_slot = -1;
            } else if (in.left && !last_left && cursor_was_empty) {
                left_active = true; left_slot = in.slot;
            }
            if (right_active && in.slot != right_slot) {
                if (!left_original) { left_original = true; out.right[0] = right_slot; }
                right_slot = in.slot; out.right[1] = in.slot;
            }
            if (left_active && in.slot != left_slot) { left_slot = in.slot; out.left = in.slot; }
            last_right = in.right; last_left = in.left; cursor_was_empty = in.cursor_empty;
            return out;
        }
    };
}
