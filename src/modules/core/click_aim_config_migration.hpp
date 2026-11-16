#pragma once
#include "config_migration_fields.hpp"

namespace modules::config_migration
{
    inline fields click_aim_fields(const config_json::value& settings, bool blatant)
    {
        fields result;
        for (const auto& [key, value] : settings.members) {
            // Blatant's old Head/Legs aliases used Body/Adaptive behavior.
            auto raw = std::string(value.raw);
            if (blatant && key == "aim_point") {
                if (value.integer() == 0) raw = "1";
                if (value.integer() == 2) raw = "4";
            }
            result[(blatant ? "blatant_" : "legit_") + key] = raw;
        }
        return result;
    }
    struct click_aim_entry { bool enabled; int keybind; std::string settings; };
    // Canonical entry wins. Otherwise preserve both profiles and select the
    // enabled one; Legit wins ties, independent of input ordering.
    inline click_aim_entry merge_click_aim(const config_json::value* modern,
        const config_json::value* legit, const config_json::value* blatant)
    {
        fields merged;
        if (legit) merged = click_aim_fields(*legit->find("settings"), false);
        if (blatant) for (const auto& [k,v] : click_aim_fields(*blatant->find("settings"), true)) merged[k] = v;
        const auto* selected = legit;
        if (!legit || (!legit->find("enabled")->boolean && blatant && blatant->find("enabled")->boolean)) selected = blatant;
        merged["mode"] = selected == blatant ? "1" : "0";
        if (modern) {
            for (const auto& [k,v] : modern->find("settings")->members) merged[k] = std::string(v.raw);
            selected = modern;
            if (!modern->find("settings")->find("mode")) merged["mode"] = "0";
        }
        return {selected->find("enabled")->boolean, *selected->find("keybind")->integer(), encode(merged)};
    }
}
