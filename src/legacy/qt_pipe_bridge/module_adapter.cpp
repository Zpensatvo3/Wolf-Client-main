#include "module_adapter.hpp"
#include "protocol.hpp"
#include "../modules/core/module.hpp"
#include "../modules/core/module_registry.hpp"
#include "../gui/gui.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace
{
    std::string json_quote(const std::string& value) { return std::string("\"") + ui_bridge::protocol::escape_json(value) + "\""; }
    std::string label_for(const char* field)
    {
        std::string result = field ? field : "Setting";
        bool upper = true;
        for (char& character : result)
        {
            if (character == '_') { character = ' '; upper = true; }
            else if (upper) { character = static_cast<char>(std::toupper(static_cast<unsigned char>(character))); upper = false; }
        }
        return result;
    }
    std::string presentation_category(const modules::module& owner)
    {
        const char* category = owner.get_category();
        const std::string value = category ? category : "Player";
        if (value == "Misc")
        {
            const std::string_view id = owner.get_id();
            if (id == "sprint" || id == "bridge_assist" || id == "scaffold" || id == "trajectories") return "Movement";
            return "Player";
        }
        if (value == "Visual") return "Visuals";
        if (value == "Render") return "Hud";
        return value;
    }
    std::string description_for(const char* name)
    {
        struct entry { const char* name; const char* description; };
        static constexpr entry descriptions[] = {
            { "click gui", "Controls the client language and menu key" }, { "panic", "Safely closes the client" },
            { "triggerbot", "Attacks valid targets under the crosshair" }, { "mace swap", "Swaps to a mace for the impact, then returns" },
            { "anchor macro", "Places, charges and detonates a respawn anchor" }, { "blatant anchor macro", "Executes the anchor sequence at maximum speed" },
            { "safe anchor", "Runs anchor actions with additional safety checks" }, { "hit crystal", "Places and breaks crystals after a manual hit" },
            { "legit click aim assist", "Smooth aim assistance while clicking" }, { "blatant click aim assist", "Strong target tracking while clicking" },
            { "aim assist", "Smoothly guides the crosshair toward targets" }, { "jump reset", "Times jumps to reduce incoming knockback" },
            { "velocity", "Controls horizontal and vertical knockback" }, { "shield breaker", "Switches to an axe against blocking players" },
            { "auto totem", "Moves available totems to protected slots" }, { "legit auto totem", "Refills totems with human-like inventory input" },
            { "watermark", "Shows the Thedorius status overlay" }, { "arraylist", "Shows enabled modules on the game overlay" },
            { "glow esp", "Highlights visible players with a glow effect" }, { "player esp", "Displays configurable player highlights" },
            { "storage esp", "Highlights nearby storage blocks" }, { "block esp", "Highlights only selected block types" },
            { "radar", "Shows nearby players on a compact radar" }, { "full bright", "Brightens dark areas without changing the world" },
            { "nametag", "Replaces player labels with detailed nametags" }, { "pearl macro", "Throws a pearl and returns to the previous slot" },
            { "chest stealer", "Moves chest contents into the inventory" }, { "elytra swapper", "Switches between an elytra and chestplate" },
            { "auto tool", "Selects the best tool for the targeted block" }, { "auto armor", "Equips the strongest available armor" },
            { "bridge assist", "Sneaks automatically near block edges" }, { "sprint", "Keeps sprint active while moving" },
            { "trajectories", "Previews projectile flight paths" }, { "scaffold", "Automatically places bridge blocks beneath you" },
            { "right clicker", "Generates configurable right clicks" }, { "left clicker", "Generates configurable left clicks" },
            { "inventory clicker", "Accelerates repeated inventory clicks" }, { "fast place", "Reduces delay between block placements" },
            { "fast exp", "Throws experience bottles at adjustable speed" }, { "mouse tweaks", "Improves inventory dragging and item movement" },
            { "pot refill", "Refills the hotbar with Instant Health potions" }
        };
        for (const entry& current : descriptions) if (name && std::strcmp(current.name, name) == 0) return current.description;
        return "Client module";
    }

    class snapshot_visitor final : public modules::config_visitor
    {
    public:
        bool reading() const override { return false; }
        void field(const char* name, bool& value) override { add(name, "bool", value ? "true" : "false"); }
        void field(const char* name, int& value) override { add(name, "int", std::to_string(value), -1000, 1000); }
        void field(const char* name, float& value) override { add(name, "float", std::to_string(value), 0, 100); }
        void field(const char* name, double& value) override { add(name, "float", std::to_string(value), 0, 100); }
        void field(const char* name, std::string& value) override { add(name, "action", json_quote(value)); }
        void field(const char* name, int* values, std::size_t count) override { add_array(name, values, count, true); }
        void field(const char* name, float* values, std::size_t count) override { add_array(name, values, count, false); }
        void field(const char* name, std::vector<std::string>& values) override
        {
            std::string raw = "[";
            for (std::size_t i = 0; i < values.size(); ++i) { if (i) raw += ','; raw += json_quote(values[i]); }
            raw += ']'; add(name, "multiselect", raw);
        }
        void enum_value(const char* name, int& value) override
        {
            add(name, "enum", std::to_string(value), 0, 5,
                "[\"Option 0\",\"Option 1\",\"Option 2\",\"Option 3\",\"Option 4\",\"Option 5\"]");
        }
        std::string json() const { return '[' + content_ + ']'; }
    private:
        template <typename Value> void add_array(const char* name, Value* values, std::size_t count, bool integral)
        {
            std::string raw = "[";
            for (std::size_t i = 0; i < count; ++i) { if (i) raw += ','; raw += std::to_string(values[i]); }
            raw += ']'; add(name, count == 2 ? "range" : "multiselect", raw, integral ? -1000 : 0, integral ? 1000 : 100);
        }
        void add(const char* name, const char* type, const std::string& value, double minimum = 0, double maximum = 1,
            const std::string& options = "[]")
        {
            if (!content_.empty()) content_ += ',';
            content_ += "{\"id\":" + json_quote(name ? name : "setting") + ",\"label\":" + json_quote(label_for(name)) +
                ",\"type\":" + json_quote(type) + ",\"value\":" + value + ",\"minimum\":" + std::to_string(minimum) +
                ",\"maximum\":" + std::to_string(maximum) + ",\"unit\":\"\",\"options\":" + options + '}';
        }
        std::string content_;
    };

    class update_visitor final : public modules::config_visitor
    {
    public:
        update_visitor(std::string field, std::string raw) : field_(std::move(field)), raw_(std::move(raw)) {}
        bool reading() const override { return true; }
        bool updating() const override { return true; }
        void field(const char* name, bool& value) override { if (match(name)) value = raw_ == "true" || raw_ == "1"; }
        void field(const char* name, int& value) override { if (match(name)) value = std::atoi(raw_.c_str()); }
        void field(const char* name, float& value) override { if (match(name)) value = std::strtof(raw_.c_str(), nullptr); }
        void field(const char* name, double& value) override { if (match(name)) value = std::strtod(raw_.c_str(), nullptr); }
        void field(const char* name, std::string& value) override { if (match(name)) value = unquote(raw_); }
        void field(const char* name, int* values, std::size_t count) override { if (match(name)) parse_array(values, count); }
        void field(const char* name, float* values, std::size_t count) override { if (match(name)) parse_array(values, count); }
        void field(const char*, std::vector<std::string>&) override {}
        bool applied() const { return applied_; }
    private:
        bool match(const char* name) { if (!name || field_ != name) return false; applied_ = true; return true; }
        static std::string unquote(const std::string& value) { return value.size() >= 2 && value.front() == '"' && value.back() == '"' ? value.substr(1, value.size() - 2) : value; }
        template <typename Value> void parse_array(Value* values, std::size_t count)
        {
            const char* cursor = raw_.c_str();
            for (std::size_t i = 0; i < count; ++i)
            {
                while (*cursor && (*cursor == '[' || *cursor == ',' || std::isspace(static_cast<unsigned char>(*cursor)))) ++cursor;
                char* end = nullptr; const double parsed = std::strtod(cursor, &end); if (end == cursor) break;
                values[i] = static_cast<Value>(parsed); cursor = end;
            }
        }
        std::string field_; std::string raw_; bool applied_ = false;
    };
}

std::string ui_bridge::module_snapshot_json()
{
    std::string result = "[";
    bool first = true;
    for (modules::module* current : modules::get_modules())
    {
        if (!current) continue;
        snapshot_visitor visitor; current->visit_config(visitor);
        if (!first) result += ','; first = false;
        std::string settings = visitor.json();
        if (std::string_view(current->get_id()) == "click_gui")
        {
            const std::string special =
                "{\"id\":\"language\",\"label\":\"Language\",\"type\":\"enum\",\"value\":" + std::to_string(gui::active_language) +
                ",\"minimum\":0,\"maximum\":1,\"unit\":\"\",\"options\":[\"English\",\"Turkce\"]},"
                "{\"id\":\"menu_key\",\"label\":\"Menu key\",\"type\":\"action\",\"value\":\"" + std::to_string(gui::menu_key) +
                "\",\"minimum\":0,\"maximum\":255,\"unit\":\"\",\"options\":[]}";
            settings = settings == "[]" ? '[' + special + ']' : settings.substr(0, settings.size() - 1) + ',' + special + ']';
        }
        result += "{\"id\":" + json_quote(current->get_id()) + ",\"name\":" + json_quote(current->get_name()) +
            ",\"description\":" + json_quote(description_for(current->get_name())) + ",\"category\":" + json_quote(presentation_category(*current)) +
            ",\"enabled\":" + (current->enabled ? "true" : "false") + ",\"keybind\":" + json_quote(std::to_string(current->get_keybind())) +
            ",\"settings\":" + settings + '}';
    }
    return result + ']';
}

bool ui_bridge::apply_setting(modules::module& owner, const std::string& field, const std::string& raw_value)
{
    if (std::string_view(owner.get_id()) == "click_gui")
    {
        if (field == "language") { gui::active_language = std::clamp(std::atoi(raw_value.c_str()), 0, 1); return true; }
        if (field == "menu_key") { gui::menu_key = std::clamp(std::atoi(raw_value.c_str()), 0, 255); return true; }
    }
    update_visitor visitor(field, raw_value); owner.visit_config(visitor); return visitor.applied();
}
