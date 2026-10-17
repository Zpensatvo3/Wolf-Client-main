#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct inv_walk
    {
        bool allow_containers = true;
        bool allow_jump = true;
        bool allow_sneak = false;
        bool operator==(const inv_walk&) const = default;
        static std::span<const modules::settings::descriptor<inv_walk>> schema()
        {
            using d = modules::settings::descriptor<inv_walk>;
            static const d fields[]{
                d::toggle("allow_containers", {"Container Screens"}, &inv_walk::allow_containers),
                d::toggle("allow_jump", {"Allow Jump"}, &inv_walk::allow_jump),
                d::toggle("allow_sneak", {"Allow Sneak"}, &inv_walk::allow_sneak)
            };
            return fields;
        }
    };
}
