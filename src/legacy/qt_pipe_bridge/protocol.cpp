#include "protocol.hpp"

#include <cstring>
#include <cctype>

std::string ui_bridge::protocol::escape_json(const std::string& value)
{
    std::string result;
    result.reserve(value.size() + 8);
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: if (character >= 0x20) result += static_cast<char>(character); break;
        }
    }
    return result;
}

namespace
{
    std::size_t value_start(const std::string& json, const char* key)
    {
        const std::string marker = std::string("\"") + key + "\"";
        std::size_t position = json.find(marker);
        if (position == std::string::npos) return position;
        position = json.find(':', position + marker.size());
        if (position == std::string::npos) return position;
        do { ++position; } while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position])));
        return position;
    }
}

std::string ui_bridge::protocol::string_value(const std::string& json, const char* key)
{
    std::size_t position = value_start(json, key);
    if (position == std::string::npos || position >= json.size() || json[position] != '"') return {};
    std::string value;
    bool escaped = false;
    for (++position; position < json.size(); ++position)
    {
        const char character = json[position];
        if (escaped) { value += character == 'n' ? '\n' : character; escaped = false; continue; }
        if (character == '\\') { escaped = true; continue; }
        if (character == '"') break;
        value += character;
    }
    return value;
}

std::string ui_bridge::protocol::raw_value(const std::string& json, const char* key)
{
    std::size_t start = value_start(json, key);
    if (start == std::string::npos || start >= json.size()) return {};
    if (json[start] == '"')
    {
        std::size_t end = start + 1;
        bool escaped = false;
        for (; end < json.size(); ++end)
        {
            if (!escaped && json[end] == '"') { ++end; break; }
            if (!escaped && json[end] == '\\') escaped = true;
            else escaped = false;
        }
        return json.substr(start, end - start);
    }
    std::size_t end = start;
    int depth = 0;
    for (; end < json.size(); ++end)
    {
        const char c = json[end];
        if (c == '[' || c == '{') ++depth;
        else if (c == ']' || c == '}') { if (depth == 0) break; --depth; }
        if (depth == 0 && (c == ',' || c == '}')) break;
    }
    while (end > start && std::isspace(static_cast<unsigned char>(json[end - 1]))) --end;
    return json.substr(start, end - start);
}

bool ui_bridge::protocol::bool_value(const std::string& json, const char* key, bool fallback)
{
    const std::string value = raw_value(json, key);
    return value == "true" ? true : value == "false" ? false : fallback;
}

std::string ui_bridge::protocol::frame(const std::string& json)
{
    const auto length = static_cast<std::uint32_t>(json.size());
    std::string output(sizeof(length), '\0');
    std::memcpy(output.data(), &length, sizeof(length));
    output += json;
    return output;
}
