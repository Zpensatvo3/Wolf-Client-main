#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct auto_tool
    {
        bool require_left_click = true;
        bool prefer_hotbar_only = true;
        bool operator==(const auto_tool&) const = default;

        static std::span<const modules::settings::descriptor<auto_tool>> schema()
        {
            using model = auto_tool;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::toggle("require_left_click", {"Require Left Click"}, &model::require_left_click),
                setting::toggle("prefer_hotbar_only", {"Prefer Hotbar Only"}, &model::prefer_hotbar_only, false),
            };
            return fields;
        }
    };
}
