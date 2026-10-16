#pragma once
#include "config_migration_fields.hpp"

namespace modules::config_migration
{
    inline bool unified_anchor(const config_json::value& settings)
    {
        for (const auto& [key,value] : settings.members)
        {
            (void)value;
            if (key == "mode" || key.starts_with("legit_") || key.starts_with("blatant_")) return true;
        }
        return false;
    }
    inline fields anchor_fields(const config_json::value& settings, bool blatant, bool unified)
    {
        fields result;
        if (unified)
        {
            for (const auto& [key,value] : settings.members) result[key] = std::string(value.raw);
            return result;
        }
        const std::string prefix = blatant ? "blatant_" : "legit_";
        const auto copy = [&](const char* old, const char* next) {
            if (const auto* value = settings.find(old)) result[prefix+next] = std::string(value->raw);
        };
        copy("hybrid_slot_input","hybrid_slot_input");
        copy("camera_jitter","camera_movement");
        copy("camera_jitter_strength","camera_strength");
        copy("switch_delay","switch_delay");
        copy("return_delay","return_delay");
        copy("action_delay","place_delay");
        copy("action_delay","charge_delay");
        copy("action_delay","detonate_delay");
        copy("jitter_ms","variance");
        if (const auto* variance = settings.find("jitter_ms"))
            result[prefix+"randomize"] = variance->integer().value_or(1) > 0 ? "true" : "false";
        return result;
    }
    struct anchor_entry { bool enabled; int keybind; std::string settings; };
    // Entry envelopes have already been checked by apply_public_config. New
    // unified entries win over aliases; otherwise enabled Legit wins ties.
    inline anchor_entry merge_anchor(const config_json::value* anchor, const config_json::value* blatant)
    {
        fields merged;
        if (blatant) merged = anchor_fields(*blatant->find("settings"),true,false);
        const bool modern = anchor && unified_anchor(*anchor->find("settings"));
        if (anchor)
            for (const auto& [key,value] : anchor_fields(*anchor->find("settings"),false,modern)) merged[key] = value;
        const auto* selected = anchor;
        if (!modern && (!anchor || (!anchor->find("enabled")->boolean && blatant && blatant->find("enabled")->boolean))) selected = blatant;
        if (!selected) selected = blatant;
        if (!modern) merged["mode"] = selected == blatant ? "1" : "0";
        return {selected->find("enabled")->boolean,*selected->find("keybind")->integer(),encode(merged)};
    }
}
