#pragma once

#include <string>

namespace modules
{
    std::string export_public_config();
    bool apply_public_config(const std::string& payload, std::string& message);
}
