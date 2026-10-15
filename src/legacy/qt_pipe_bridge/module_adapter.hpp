#pragma once

#include <string>

namespace modules { class module; }

namespace ui_bridge
{
    std::string module_snapshot_json();
    bool apply_setting(modules::module& owner, const std::string& field, const std::string& raw_value);
}
