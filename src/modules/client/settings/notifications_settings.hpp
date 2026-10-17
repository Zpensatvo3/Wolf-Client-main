#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct notifications
    {
        int position = 0; // Bottom right, top right, bottom left, top left.
        float duration = 3.4f;
        int max_visible = 3;
        bool module_changes = true, operation_results = true;
        bool operator==(const notifications&) const = default;
        static std::span<const modules::settings::descriptor<notifications>> schema()
        {
            using d = modules::settings::descriptor<notifications>;
            static constexpr modules::settings::label positions[]{
                {"Bottom Right"}, {"Top Right"},
                {"Bottom Left"}, {"Top Left"}
            };
            static const d fields[]{
                d::choice("position", {"Position"}, &notifications::position, positions),
                d::number("duration", {"Duration"}, &notifications::duration, 2.f, 8.f, "%.1f s"),
                d::integer("max_visible", {"Visible Cards"}, &notifications::max_visible, 1, 5),
                d::toggle("module_changes", {"Module Changes"}, &notifications::module_changes),
                d::toggle("operation_results", {"Operation Results"}, &notifications::operation_results)
            };
            return fields;
        }
    };
}
