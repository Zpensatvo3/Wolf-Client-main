#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    inline constexpr bool anti_bot_default_enabled = true;
    struct anti_bot
    {
        bool check_player_list = true;
        int spawn_grace_ticks = 40;
        int missing_list_ticks = 40;
        bool operator==(const anti_bot&) const = default;
        static std::span<const modules::settings::descriptor<anti_bot>> schema()
        {
            using d = modules::settings::descriptor<anti_bot>;
            static const d fields[]{
                d::toggle("check_player_list", {"Player List Check", "Oyuncu Listesi Kontrolu"}, &anti_bot::check_player_list),
                d::integer("spawn_grace_ticks", {"Spawn Grace", "Dogma Beklemesi"}, &anti_bot::spawn_grace_ticks, 0, 200, "%d ticks"),
                d::integer("missing_list_ticks", {"Missing List Delay", "Listede Olmama Suresi"}, &anti_bot::missing_list_ticks, 5, 200, "%d ticks")
            };
            return fields;
        }
    };
}
