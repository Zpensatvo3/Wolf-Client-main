#include "config_fields.hpp"
#include "module.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>

namespace
{
    std::string escape_json(std::string_view value)
    {
        std::string output;
        output.reserve(value.size() + 8);
        for (const unsigned char character : value)
        {
            switch (character)
            {
            case '\\': output += "\\\\"; break;
            case '"': output += "\\\""; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20)
                {
                    static constexpr char digits[] = "0123456789abcdef";
                    output += "\\u00";
                    output.push_back(digits[(character >> 4) & 0x0f]);
                    output.push_back(digits[character & 0x0f]);
                }
                else output.push_back(static_cast<char>(character));
                break;
            }
        }
        return output;
    }

    void skip_spaces(std::string_view text, std::size_t& position)
    {
        while (position < text.size() && static_cast<unsigned char>(text[position]) <= 0x20) ++position;
    }

    std::optional<std::string_view> raw_field(std::string_view object, std::string_view key)
    {
        std::size_t position = 0;
        skip_spaces(object, position);
        if (position >= object.size() || object[position++] != '{') return std::nullopt;

        while (position < object.size())
        {
            skip_spaces(object, position);
            if (position < object.size() && object[position] == '}') return std::nullopt;
            if (position >= object.size() || object[position++] != '"') return std::nullopt;

            std::string current_key;
            bool escaped = false;
            while (position < object.size())
            {
                const char character = object[position++];
                if (escaped) { current_key.push_back(character); escaped = false; continue; }
                if (character == '\\') { escaped = true; continue; }
                if (character == '"') break;
                current_key.push_back(character);
            }
            skip_spaces(object, position);
            if (position >= object.size() || object[position++] != ':') return std::nullopt;
            skip_spaces(object, position);
            const std::size_t start = position;
            bool quoted = false;
            bool value_escaped = false;
            int object_depth = 0;
            int array_depth = 0;
            while (position < object.size())
            {
                const char character = object[position];
                if (quoted)
                {
                    ++position;
                    if (value_escaped) value_escaped = false;
                    else if (character == '\\') value_escaped = true;
                    else if (character == '"') quoted = false;
                    continue;
                }
                if (character == '"') { quoted = true; ++position; continue; }
                if (character == '{') ++object_depth;
                else if (character == '}')
                {
                    if (object_depth == 0 && array_depth == 0) break;
                    --object_depth;
                }
                else if (character == '[') ++array_depth;
                else if (character == ']') --array_depth;
                else if (character == ',' && object_depth == 0 && array_depth == 0) break;
                ++position;
            }
            std::size_t end = position;
            while (end > start && static_cast<unsigned char>(object[end - 1]) <= 0x20) --end;
            if (current_key == key) return object.substr(start, end - start);
            if (position < object.size() && object[position] == ',') ++position;
        }
        return std::nullopt;
    }

    bool parse_json_string(std::string_view raw, std::string& output)
    {
        if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') return false;
        output.clear();
        for (std::size_t position = 1; position + 1 < raw.size(); ++position)
        {
            char character = raw[position];
            if (character != '\\') { output.push_back(character); continue; }
            if (++position + 1 > raw.size()) return false;
            switch (raw[position])
            {
            case '\\': output.push_back('\\'); break;
            case '"': output.push_back('"'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            default: return false;
            }
        }
        return true;
    }

    template <typename Number>
    bool parse_integer(std::string_view raw, Number& output)
    {
        Number parsed{};
        const auto result = std::from_chars(raw.data(), raw.data() + raw.size(), parsed);
        if (result.ec != std::errc{} || result.ptr != raw.data() + raw.size()) return false;
        if (parsed < static_cast<Number>(-1000000) || parsed > static_cast<Number>(1000000)) return false;
        output = parsed;
        return true;
    }

    template <typename Number>
    bool parse_decimal(std::string_view raw, Number& output)
    {
        std::string copy(raw);
        char* end = nullptr;
        const double parsed = std::strtod(copy.c_str(), &end);
        if (!end || end != copy.c_str() + copy.size() || !std::isfinite(parsed)) return false;
        if (parsed < -1000000.0 || parsed > 1000000.0) return false;
        output = static_cast<Number>(parsed);
        return true;
    }

    template <typename Value, typename Parser>
    bool parse_number_array(std::string_view raw, Value* output, std::size_t count, Parser parser, bool assign)
    {
        if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') return false;
        std::size_t position = 1;
        for (std::size_t index = 0; index < count; ++index)
        {
            skip_spaces(raw, position);
            const std::size_t start = position;
            while (position + 1 < raw.size() && raw[position] != ',' && raw[position] != ']') ++position;
            std::size_t end = position;
            while (end > start && static_cast<unsigned char>(raw[end - 1]) <= 0x20) --end;
            Value parsed{};
            if (start == end || !parser(raw.substr(start, end - start), parsed)) return false;
            if (assign) output[index] = parsed;
            skip_spaces(raw, position);
            if (index + 1 < count)
            {
                if (position >= raw.size() || raw[position++] != ',') return false;
            }
        }
        skip_spaces(raw, position);
        return position + 1 == raw.size() && raw[position] == ']';
    }

    class writer final : public modules::config_visitor
    {
    public:
        bool reading() const override { return false; }
        void field(const char* name, bool& value) override { prefix(name); output += value ? "true" : "false"; }
        void field(const char* name, int& value) override { prefix(name); output += std::to_string(value); }
        void field(const char* name, float& value) override { number(name, value); }
        void field(const char* name, double& value) override { number(name, value); }
        void field(const char* name, std::string& value) override { prefix(name); output += '"' + escape_json(value) + '"'; }
        void field(const char* name, int* values, std::size_t count) override { array(name, values, count); }
        void field(const char* name, float* values, std::size_t count) override { array(name, values, count); }
        void field(const char* name, std::vector<std::string>& values) override
        {
            prefix(name); output.push_back('[');
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index) output.push_back(',');
                output += '"' + escape_json(values[index]) + '"';
            }
            output.push_back(']');
        }

        std::string finish() { return '{' + output + '}'; }

    private:
        std::string output;
        void prefix(const char* name)
        {
            if (!output.empty()) output.push_back(',');
            output += '"' + escape_json(name ? name : "") + "\":";
        }
        template <typename Number> void number(const char* name, Number value)
        {
            prefix(name);
            std::ostringstream stream;
            stream << std::setprecision(std::numeric_limits<Number>::max_digits10) << value;
            output += stream.str();
        }
        template <typename Number> void array(const char* name, Number* values, std::size_t count)
        {
            prefix(name); output.push_back('[');
            for (std::size_t index = 0; index < count; ++index)
            {
                if (index) output.push_back(',');
                if constexpr (std::is_integral_v<Number>) output += std::to_string(values[index]);
                else
                {
                    std::ostringstream stream;
                    stream << std::setprecision(std::numeric_limits<Number>::max_digits10) << values[index];
                    output += stream.str();
                }
            }
            output.push_back(']');
        }
    };

    class reader final : public modules::config_visitor
    {
    public:
        reader(std::string_view object, bool apply) : object(object), apply(apply) {}
        bool reading() const override { return apply; }
        bool valid() const override { return valid_state; }
        bool importing() const override { return true; }
        bool has_field(const char* name) const override { return raw_field(object, name).has_value(); }
        void stage_values(bool enabled) override { staged = enabled; }
        void reject(const char* message) override { valid_state = false; if (error.empty()) error = message ? message : "Invalid settings."; }
        const std::string& message() const { return error; }

        void field(const char* name, bool& value) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            bool parsed{};
            if (*raw == "true") parsed = true;
            else if (*raw == "false") parsed = false;
            else { valid_state = false; return; }
            if (apply || staged) value = parsed;
        }
        void field(const char* name, int& value) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            int parsed{}; if (!parse_integer(*raw, parsed)) { valid_state = false; return; }
            if (apply || staged) value = parsed;
        }
        void field(const char* name, float& value) override { decimal(name, value); }
        void field(const char* name, double& value) override { decimal(name, value); }
        void field(const char* name, std::string& value) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            std::string parsed; if (!parse_json_string(*raw, parsed)) { valid_state = false; return; }
            if (apply || staged) value = std::move(parsed);
        }
        void field(const char* name, int* values, std::size_t count) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            if (!parse_number_array(*raw, values, count, parse_integer<int>, apply || staged)) valid_state = false;
        }
        void field(const char* name, float* values, std::size_t count) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            if (!parse_number_array(*raw, values, count, parse_decimal<float>, apply || staged)) valid_state = false;
        }
        void field(const char* name, std::vector<std::string>& values) override
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            if (raw->size() < 2 || raw->front() != '[' || raw->back() != ']') { valid_state = false; return; }
            std::vector<std::string> parsed;
            std::size_t position = 1;
            while (position + 1 < raw->size())
            {
                skip_spaces(*raw, position);
                if ((*raw)[position] == ']') break;
                if ((*raw)[position] != '"') { valid_state = false; return; }
                const std::size_t start = position++;
                bool escaped = false;
                while (position < raw->size())
                {
                    const char character = (*raw)[position++];
                    if (escaped) escaped = false;
                    else if (character == '\\') escaped = true;
                    else if (character == '"') break;
                }
                std::string item;
                if (!parse_json_string(raw->substr(start, position - start), item)) { valid_state = false; return; }
                if (item.size() > 128 || parsed.size() >= 256) { valid_state = false; return; }
                parsed.push_back(std::move(item));
                skip_spaces(*raw, position);
                if (position < raw->size() && (*raw)[position] == ',') ++position;
                else if (position < raw->size() && (*raw)[position] != ']') { valid_state = false; return; }
            }
            if (apply || staged) values = std::move(parsed);
        }

    private:
        std::string_view object;
        bool apply = false;
        bool valid_state = true;
        bool staged = false;
        std::string error;
        template <typename Number> void decimal(const char* name, Number& value)
        {
            const auto raw = raw_field(object, name); if (!raw) return;
            Number parsed{}; if (!parse_decimal(*raw, parsed)) { valid_state = false; return; }
            if (apply || staged) value = parsed;
        }
    };
}

std::string modules::serialize_config_fields(module& owner)
{
    writer output;
    owner.visit_config(output);
    return output.finish();
}

bool modules::deserialize_config_fields(module& owner, const std::string& object)
{
    if (!validate_config_fields(owner, object)) return false;
    reader application(object, true);
    owner.visit_config(application);
    return application.valid();
}

bool modules::validate_config_fields(module& owner, const std::string& object)
{
    reader validation(object, false);
    owner.visit_config(validation);
    return validation.valid();
}

std::string modules::config_validation_error(module& owner, const std::string& object)
{
    reader validation(object, false);
    owner.visit_config(validation);
    return validation.message().empty() ? "Invalid module settings." : validation.message();
}
