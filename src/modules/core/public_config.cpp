#include "public_config.hpp"
#include "module_registry.hpp"
#include <charconv>
#include <cctype>
#include <optional>
#include <string_view>
#include <vector>

namespace
{
    std::string escape_json(std::string_view value)
    {
        std::string output;
        for (const char character : value)
        {
            switch (character)
            {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default: output.push_back(character); break;
            }
        }
        return output;
    }

    void skip_spaces(std::string_view value, std::size_t& position)
    {
        while (position < value.size() && std::isspace(static_cast<unsigned char>(value[position]))) ++position;
    }

    std::optional<std::string> string_field(std::string_view object, std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        std::size_t position = object.find(needle);
        if (position == std::string_view::npos) return std::nullopt;
        position += needle.size(); skip_spaces(object, position);
        if (position >= object.size() || object[position++] != ':') return std::nullopt;
        skip_spaces(object, position);
        if (position >= object.size() || object[position++] != '"') return std::nullopt;
        std::string output;
        while (position < object.size())
        {
            const char character = object[position++];
            if (character == '"') return output;
            if (character != '\\') { output.push_back(character); continue; }
            if (position >= object.size()) return std::nullopt;
            switch (object[position++])
            {
            case '\\': output.push_back('\\'); break;
            case '"': output.push_back('"'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            default: return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<int> int_field(std::string_view object, std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        std::size_t position = object.find(needle);
        if (position == std::string_view::npos) return std::nullopt;
        position += needle.size(); skip_spaces(object, position);
        if (position >= object.size() || object[position++] != ':') return std::nullopt;
        skip_spaces(object, position);
        int output = 0;
        const auto result = std::from_chars(object.data() + position, object.data() + object.size(), output);
        return result.ec == std::errc{} ? std::optional<int>(output) : std::nullopt;
    }

    std::optional<bool> bool_field(std::string_view object, std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        std::size_t position = object.find(needle);
        if (position == std::string_view::npos) return std::nullopt;
        position += needle.size(); skip_spaces(object, position);
        if (position >= object.size() || object[position++] != ':') return std::nullopt;
        skip_spaces(object, position);
        if (object.substr(position, 4) == "true") return true;
        if (object.substr(position, 5) == "false") return false;
        return std::nullopt;
    }

    std::optional<std::string_view> object_field(std::string_view object, std::string_view key)
    {
        const std::string needle = "\"" + std::string(key) + "\"";
        std::size_t position = object.find(needle);
        if (position == std::string_view::npos) return std::nullopt;
        position += needle.size(); skip_spaces(object, position);
        if (position >= object.size() || object[position++] != ':') return std::nullopt;
        skip_spaces(object, position);
        if (position >= object.size() || object[position] != '{') return std::nullopt;
        const std::size_t start = position++;
        int depth = 1;
        while (position < object.size() && depth > 0)
        {
            if (object[position] == '{') ++depth;
            else if (object[position] == '}') --depth;
            ++position;
        }
        return depth == 0 ? std::optional<std::string_view>(object.substr(start, position - start)) : std::nullopt;
    }
}

std::string modules::export_public_config()
{
    std::string output = "{\"version\":" + std::to_string(public_config_version) + ",\"modules\":[";
    bool first = true;
    for (const module* current : get_modules())
    {
        if (!current) continue;
        if (!first) output += ',';
        first = false;
        output += "{\"id\":\"" + escape_json(current->get_id()) + "\",\"enabled\":" +
            (current->enabled ? "true" : "false") + ",\"keybind\":" + std::to_string(current->get_keybind()) +
            ",\"settings\":" + current->export_config_settings() + "}";
    }
    return output + "]}";
}

bool modules::apply_public_config(const std::string& payload, std::string& message)
{
    const auto version = int_field(payload, "version");
    if (!version || *version != public_config_version)
    {
        message = "Legacy config detected. Republish this config with the latest client.";
        return false;
    }
    const std::size_t modules_key = payload.find("\"modules\"");
    const std::size_t array_start = modules_key == std::string::npos ? std::string::npos : payload.find('[', modules_key);
    if (array_start == std::string::npos) { message = "Config module list is invalid."; return false; }

    struct pending_module
    {
        module* target = nullptr;
        bool enabled = false;
        int keybind = 0;
        std::string settings;
    };
    std::vector<pending_module> pending;
    std::size_t position = array_start + 1;
    while (position < payload.size())
    {
        skip_spaces(payload, position);
        if (position >= payload.size() || payload[position] == ']') break;
        if (payload[position] == ',') { ++position; continue; }
        if (payload[position] != '{') { message = "Config module entry is invalid."; return false; }
        const std::size_t start = position++;
        int depth = 1; bool quoted = false; bool escaped = false;
        while (position < payload.size() && depth > 0)
        {
            const char character = payload[position++];
            if (quoted) { if (escaped) escaped = false; else if (character == '\\') escaped = true; else if (character == '"') quoted = false; continue; }
            if (character == '"') quoted = true;
            else if (character == '{') ++depth;
            else if (character == '}') --depth;
        }
        if (depth != 0) { message = "Config module entry is incomplete."; return false; }
        const std::string_view entry(payload.data() + start, position - start);
        const auto id = string_field(entry, "id");
        const auto enabled = bool_field(entry, "enabled");
        const auto keybind = int_field(entry, "keybind");
        if (!id || !enabled || !keybind) continue;
        if (module* current = find_module_by_id(*id))
        {
            const auto settings = object_field(entry, "settings");
            if (!settings || !current->validate_config_settings(std::string(*settings)))
            {
                message = std::string(current->get_id()) + ": " + (settings
                    ? config_validation_error(*current, std::string(*settings)) : "Missing module settings.");
                return false;
            }
            pending.push_back({ current, *enabled, *keybind, std::string(*settings) });
        }
    }
    if (pending.empty()) { message = "This config has no compatible modules."; return false; }
    for (pending_module& item : pending)
    {
        if (!item.target->apply_config_settings(item.settings))
        {
            message = "Config settings could not be applied.";
            return false;
        }
        item.target->enabled = item.enabled;
        item.target->set_keybind(item.keybind);
    }
    message = std::to_string(pending.size()) + " module settings loaded.";
    return true;
}
