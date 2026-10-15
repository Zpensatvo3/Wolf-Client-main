#pragma once

#include "../core/setting_schema.hpp"

namespace modules { enum class vape_kind { aim, left, right }; }

namespace vape_settings
{
    struct config
    {
        float min_cps = 9.f, max_cps = 13.5f;
        int randomization = 2;
        bool jitter = false, weapons_only = true, protect_blocks = true;
        bool hold_to_click = true, blocks_only = true, offhand = true;
        float range = 5.f, fov = 180.f, horizontal = 5.f, vertical = 5.f;
        bool aim_vertical = false, strafe_increase = true, require_mouse = true;
        int sort = 0;
        int aim_mode = 0, target_area = 1;
        bool operator==(const config&) const = default;
    };

    inline config defaults(modules::vape_kind kind)
    {
        config value;
        if (kind == modules::vape_kind::right) { value.min_cps = 12; value.max_cps = 18; }
        return value;
    }

    using setting = modules::settings::descriptor<config>;
    using label = modules::settings::label;
    inline std::span<const setting> schema(modules::vape_kind kind)
    {
        static constexpr label randomization[]{{"Normal", "Normal"}, {"Extra", "Extra"}, {"Extra+", "Extra+"}};
        static constexpr label engines[]{{"Simple", "Simple"}, {"Adaptive", "Adaptive"}};
        static constexpr label areas[]{{"Center", "Merkez"}, {"Closest", "En yakın"}};
        static constexpr label sorting[]{{"Angle", "Açı"}, {"Distance", "Mesafe"}, {"Health", "Can"},
            {"Armor", "Zırh"}, {"Threat", "Tehdit"}};
        static const setting left[]{
            setting::range("min_cps", "max_cps", {"CPS", "CPS"}, &config::min_cps, &config::max_cps, 1.f, 30.f),
            setting::choice("randomization", {"Randomization", "Rastgelelik"}, &config::randomization, randomization),
            setting::toggle("jitter", {"Jitter", "Jitter"}, &config::jitter),
            setting::toggle("weapons_only", {"Weapons Only", "Sadece silah"}, &config::weapons_only),
            setting::toggle("protect_blocks", {"Mining Protection", "Kazma koruması"}, &config::protect_blocks),
        };
        static const setting right[]{
            left[0], left[1], left[2],
            setting::toggle("hold_to_click", {"Hold to Click", "Basılı tutarken"}, &config::hold_to_click),
            setting::toggle("blocks_only", {"Blocks Only", "Sadece blok"}, &config::blocks_only),
            setting::toggle("offhand", {"Allow Offhand", "Sol ele izin ver"}, &config::offhand),
        };
        static const setting aim[]{
            setting::choice("engine", {"Engine", "Motor"}, &config::aim_mode, engines),
            setting::choice("target_area", {"Target Area", "Hedef alanı"}, &config::target_area, areas),
            setting::number("horizontal_speed", {"Horizontal Speed", "Yatay hız"}, &config::horizontal, .1f, 20.f),
            setting::toggle("aim_vertical", {"Vertical Aim", "Dikey nişan"}, &config::aim_vertical),
            setting::number("vertical_speed", {"Vertical Speed", "Dikey hız"}, &config::vertical, .1f, 20.f, "%.1f", &config::aim_vertical),
            setting::number("range", {"Range", "Menzil"}, &config::range, 1.f, 8.f, "%.1f m"),
            setting::number("fov", {"FOV", "FOV"}, &config::fov, 10.f, 360.f, "%.0f"),
            setting::choice("sort", {"Priority", "Öncelik"}, &config::sort, sorting),
            setting::toggle("strafe_increase", {"Strafe Increase", "Strafe takviyesi"}, &config::strafe_increase),
            setting::toggle("require_mouse", {"On Click Only", "Sadece tıklarken"}, &config::require_mouse),
            left[3], left[4],
        };
        switch (kind)
        {
        case modules::vape_kind::aim: return aim;
        case modules::vape_kind::left: return left;
        case modules::vape_kind::right: return right;
        }
        return {};
    }

    inline bool normalize(modules::vape_kind kind, config& candidate)
    {
        return modules::settings::normalize(candidate, schema(kind));
    }
    inline bool visit(modules::config_visitor& visitor, modules::vape_kind kind, config& live)
    {
        return modules::settings::visit(visitor, live, schema(kind), "Vape: invalid or non-finite settings.");
    }
}
