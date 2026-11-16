#pragma once

#include <string>

namespace modules
{
    std::string export_public_config();
    bool apply_public_config(const std::string& payload, std::string& message, bool restore_keybinds = true);
    // Local keybind file: modules/id/keybind fields, no enabled/settings changes.
    // Local config files strip bindings and load with restore_keybinds=false.
    std::string export_keybind_config();
    bool apply_keybind_config(const std::string& payload, std::string& message);
}
