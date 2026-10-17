#pragma once

#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct bridge_assist
    {
        float edge_offset = 0.08f;
        int unsneak_delay = 55;
        bool require_block_item = true;
        bool only_while_moving = true;
        bool operator==(const bridge_assist&) const = default;

        static std::span<const modules::settings::descriptor<bridge_assist>> schema()
        {
            using model = bridge_assist;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("edge_offset", {"Edge Offset"}, &model::edge_offset, 0.f, .27f, "%.2f blocks"),
                setting::integer("unsneak_delay", {"Unsneak Delay"}, &model::unsneak_delay, 0, 300, "%d ms"),
                setting::toggle("require_block_item", {"Require Block Item"}, &model::require_block_item),
                setting::toggle("only_while_moving", {"Only While Moving"}, &model::only_while_moving),
            };
            return fields;
        }
    };
}
