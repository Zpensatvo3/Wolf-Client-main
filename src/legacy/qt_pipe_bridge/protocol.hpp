#pragma once

#include <cstdint>
#include <string>

namespace ui_bridge::protocol
{
    inline constexpr std::uint32_t version = 1;
    std::string escape_json(const std::string& value);
    std::string string_value(const std::string& json, const char* key);
    std::string raw_value(const std::string& json, const char* key);
    bool bool_value(const std::string& json, const char* key, bool fallback = false);
    std::string frame(const std::string& json);
}
