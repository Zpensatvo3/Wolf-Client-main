#pragma once
#include "../core/config_fields.hpp"
#include <algorithm>
#include <cmath>

namespace legit_aim_engine
{
    // Public config v3 values are intentionally sparse and remain unchanged.
    enum class aim_point { head = 0, upper_body = 1, adaptive = 4 };
    struct config
    {
        float strength = 42.f, fov = 60.f, max_distance = 4.2f;
        aim_point point = aim_point::adaptive;
        bool weapon_only = true;
    };
    inline bool normalize(config& value)
    {
        if (!std::isfinite(value.strength) || !std::isfinite(value.fov) ||
            !std::isfinite(value.max_distance) || (value.point != aim_point::head &&
                value.point != aim_point::upper_body && value.point != aim_point::adaptive)) return false;
        value.strength = std::clamp(value.strength, 0.f, 100.f);
        value.fov = std::clamp(value.fov, 20.f, 120.f);
        value.max_distance = std::clamp(value.max_distance, 2.5f, 5.f);
        return true;
    }
    inline bool same_config(const config& a, const config& b)
    {
        return a.strength == b.strength && a.fov == b.fov && a.max_distance == b.max_distance &&
            a.point == b.point && a.weapon_only == b.weapon_only;
    }
    inline bool visit_config(modules::config_visitor& visitor, config& live)
    {
        config candidate = live;
        const bool importing = visitor.importing();
        if (importing) visitor.stage_values(true);
        visitor.field("fov", candidate.fov); visitor.field("max_distance", candidate.max_distance);
        visitor.field("strength", candidate.strength); visitor.field("weapon_only", candidate.weapon_only);
        int point = static_cast<int>(candidate.point);
        visitor.enum_value("aim_point", point);
        if (!importing) return false;
        candidate.point = static_cast<aim_point>(point);
        if (!normalize(candidate)) visitor.reject("Aim Assist: invalid aim point or non-finite settings.");
        visitor.stage_values(false);
        if (visitor.reading() && visitor.valid()) { live = candidate; return true; }
        return false;
    }
}
