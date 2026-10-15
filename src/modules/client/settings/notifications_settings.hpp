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
                {"Bottom Right", "Sag Alt"}, {"Top Right", "Sag Ust"},
                {"Bottom Left", "Sol Alt"}, {"Top Left", "Sol Ust"}
            };
            static const d fields[]{
                d::choice("position", {"Position", "Konum"}, &notifications::position, positions),
                d::number("duration", {"Duration", "Sure"}, &notifications::duration, 2.f, 8.f, "%.1f s"),
                d::integer("max_visible", {"Visible Cards", "Gorunen Bildirim"}, &notifications::max_visible, 1, 5),
                d::toggle("module_changes", {"Module Changes", "Modul Degisiklikleri"}, &notifications::module_changes),
                d::toggle("operation_results", {"Operation Results", "Islem Sonuclari"}, &notifications::operation_results)
            };
            return fields;
        }
    };
}
