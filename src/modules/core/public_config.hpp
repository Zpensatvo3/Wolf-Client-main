#pragma once

#include <string>

namespace modules
{
    inline constexpr int public_config_version = 3;
    std::string export_public_config();
    bool apply_public_config(const std::string& payload, std::string& message);
}
